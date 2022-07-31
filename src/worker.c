#include "worker.h"
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <time.h>
#include "time_config.h"
#define BURST_SIZE 32

extern rte_atomic16_t global_exit_flag ;
extern int global_work_type;
extern struct hm_config *global_hm_config;
extern bool global_log_hook ;
extern struct rte_hash *global_domain_hash_sockets[HM_MAX_CPU_SOCKET];

static inline bool
get_http_host(char *content, size_t content_length, char **host, size_t *host_length, char **url, size_t *url_length, char** refer, size_t *refer_length);

static struct rte_hash * hm_hash_create(int socketid){

    if ( socketid >= HM_MAX_CPU_SOCKET)
        rte_exit(-1, "max cpu limit exceeded\n");


    char *name = malloc(32);
    snprintf(name,32 , "hmhash%d", socketid);
	struct rte_hash_parameters params_pseudo_hash = {
		.name = name,
		.entries = 1024*512,
		.key_len = HM_MAX_DOMAIN_LEN,
		.hash_func = rte_jhash,
		.hash_func_init_val = 0,
		.socket_id = socketid,
	};
    return rte_hash_create(&params_pseudo_hash);
}

static void hm_hash_add_all_domain()
{
    int nb_sockets = rte_socket_count();
    int socket;
    const domain_conf_t* conf = global_hm_config->domain_config;
    for ( socket = 0 ; socket < nb_sockets; socket++) {
        int i;
        for(i=0; i<conf->domains_count; i++) {
            char *key = rte_calloc_socket("hmhash", HM_MAX_DOMAIN_LEN, 1, 8 , socket);
            char *value = rte_calloc_socket("hmhash", HM_MAX_DOMAIN_LEN, 1, 8 , socket);
            strncpy(key, conf->domains[i]->domain, HM_MAX_DOMAIN_LEN);
            if ( conf->domains[i]->use_time_config ) {
                strncat(value, "T:", 2);
                strncat(value, conf->domains[i]->target, HM_MAX_DOMAIN_LEN - 3);
            } else {
                strncpy(value,conf->domains[i]->target, HM_MAX_DOMAIN_LEN);
            }
            int ret = rte_hash_add_key_data(global_domain_hash_sockets[socket], key, value);
            if ( ret < 0 )
                HM_INFO("add hash key failed\n");
        }
    }
}

static void hm_hash_search(int socketid, char *key, void**data) {
    if ( socketid >= HM_MAX_CPU_SOCKET ) {
        return;
    }

    struct rte_hash * h = global_domain_hash_sockets[socketid];
    int ret = rte_hash_lookup_data(global_domain_hash_sockets[socketid], key, data);

    return ;
}

static void hash_test(char *pad_key, char*host) {
    int socket = rte_socket_id();
    strncpy(pad_key, host, HM_MAX_DOMAIN_LEN);
    char *data = NULL;

    hm_hash_search(socket, pad_key, (void**)&data );
    if (data == NULL ) {
        HM_INFO("hash search failed\n");
    } else {
        HM_INFO("hash search=%s\n",data);
    }

}

void hm_hash_init(){
    int socket = 0;
    for ( ; socket < HM_MAX_CPU_SOCKET; socket++) {
        global_domain_hash_sockets[socket] = NULL;
    }

    int nb_sockets = rte_socket_count();
    for ( socket = 0 ; socket < nb_sockets; socket++) {
        global_domain_hash_sockets[socket] = hm_hash_create(socket);
        if (global_domain_hash_sockets[socket] == NULL)
		    rte_panic("Failed to create cdev_map hash table, errno = %d\n", rte_errno);
    }

    hm_hash_add_all_domain();
}



FILE * hplog_init(int core_id)
{
    char fn[32];
    snprintf(fn,32,"httplog_%d.log", core_id);
    FILE *fp = fopen(fn, "w+");

    return fp;
}

void hplog_append_line(FILE* fp,char *data, size_t len)
{
    fwrite(data, len, 1, fp);
}


static inline size_t
get_vlan_offset(struct rte_ether_hdr *eth_hdr, uint16_t *proto)
{
    size_t vlan_offset = 0;

    if (rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN) == *proto ||
        rte_cpu_to_be_16(RTE_ETHER_TYPE_QINQ) == *proto) {
        struct rte_vlan_hdr *vlan_hdr =
            (struct rte_vlan_hdr *)(eth_hdr + 1);

        vlan_offset = sizeof(struct rte_vlan_hdr);
        *proto = vlan_hdr->eth_proto;

        if (rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN) == *proto) {
            vlan_hdr = vlan_hdr + 1;
            *proto = vlan_hdr->eth_proto;
            vlan_offset += sizeof(struct rte_vlan_hdr);
        }
    }
    return vlan_offset;
}


static inline bool __attribute__((always_inline))
ModifyAndSendPacket(struct rte_mbuf* originalMbuf, struct rte_ether_hdr *eth_hdr, struct rte_ipv4_hdr *ipv4_hdr, struct rte_tcp_hdr *tcphdr,
                    uint16_t pid, uint16_t qid, char *data, size_t data_len, size_t request_data_len)
{
	//struct ether_addr tmpEthAddr;
	uint16_t ether_type,offset;
	struct rte_ipv4_hdr *ip_hdr;
	int ipv4_hdr_len, tcp_hdr_len;

    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(originalMbuf->pool);
    mbuf->packet_type = originalMbuf->packet_type;
    mbuf->hash.rss = originalMbuf->hash.rss;

    struct rte_ether_hdr *new_ether_hdr = (struct rte_ether_hdr *) rte_pktmbuf_append(mbuf, sizeof(struct rte_ether_hdr));
	//fill_ethernet_header
    {
        rte_memcpy(new_ether_hdr, eth_hdr, sizeof(struct rte_ether_hdr));
	    new_ether_hdr->s_addr = eth_hdr->d_addr;
	    new_ether_hdr->d_addr = eth_hdr->s_addr;
    }

	struct rte_ipv4_hdr *new_ipv4_hdr = (struct rte_ipv4_hdr *) rte_pktmbuf_append(mbuf, sizeof(struct rte_ipv4_hdr));
	//fill_ipv4_header(new_ipv4_hdr);
    {
        rte_memcpy(new_ipv4_hdr, ipv4_hdr, sizeof(struct rte_ipv4_hdr));
        new_ipv4_hdr->src_addr = ipv4_hdr->dst_addr;
        new_ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
        new_ipv4_hdr->version_ihl = IPVERSION << 4 | sizeof(struct rte_ipv4_hdr) / RTE_IPV4_IHL_MULTIPLIER,
        //printf("old ipv4 hdr=%d %d\n", ipv4_hdr->version_ihl, new_ipv4_hdr->version_ihl);
        //printf("old ipv4 total len=%d\n", rte_be_to_cpu_16(ipv4_hdr->total_length));

        new_ipv4_hdr->type_of_service = 0; // No Diffserv
        new_ipv4_hdr->total_length = rte_cpu_to_be_16( sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) + data_len);
        new_ipv4_hdr->packet_id = rte_cpu_to_be_16(5462); // set random
        new_ipv4_hdr->fragment_offset = rte_cpu_to_be_16(0);
        new_ipv4_hdr->time_to_live = 58;
        new_ipv4_hdr->next_proto_id = IPPROTO_TCP; // tcp
        //new_ipv4_hdr->hdr_checksum = rte_cpu_to_be_16(25295);
    }

    struct rte_tcp_hdr *new_tcp_hdr = (struct rte_tcp_hdr *) rte_pktmbuf_append(mbuf, sizeof(struct rte_tcp_hdr));
	//fill_tcp_header
    {
    	uint32_t seq_tmp;
	    seq_tmp = tcphdr->recv_ack;
	    new_tcp_hdr->recv_ack = rte_cpu_to_be_32(rte_be_to_cpu_32(tcphdr->sent_seq) + request_data_len);
	    new_tcp_hdr->sent_seq = seq_tmp;
	    new_tcp_hdr->tcp_flags = RTE_TCP_ACK_FLAG | RTE_TCP_PSH_FLAG;
        new_tcp_hdr->src_port = tcphdr->dst_port;
        new_tcp_hdr->dst_port = tcphdr->src_port;
        new_tcp_hdr->rx_win = rte_cpu_to_be_16(256);
        new_tcp_hdr->data_off =  sizeof(struct rte_tcp_hdr) / 4 << 4;
        new_tcp_hdr->tcp_urp = 0;

        // do cksum later
    }
    char *content = (char *) rte_pktmbuf_append(mbuf, data_len);
    rte_memcpy(content, data, data_len);

    new_ipv4_hdr->hdr_checksum = 0;
    new_tcp_hdr->cksum = 0;
    new_ipv4_hdr->hdr_checksum = rte_ipv4_cksum(new_ipv4_hdr);
    new_tcp_hdr->cksum = rte_ipv4_udptcp_cksum(new_ipv4_hdr, new_tcp_hdr);

    uint16_t ret = rte_eth_tx_burst(pid, qid, &mbuf, 1);
	if (ret <= 0)
		HM_LOG(ERR, "Send failed..\n");
    else
    {
        //printf("send cnt=%d\n",ret);
    }

    return true;
}


static inline bool is_rx_port(struct port_params *port_param)
{
    return port_param->tx_port >= 0;
}


void _hm_worker_run(void *dummy)
{
	uint32_t lcore_id;
    uint16_t queue_id;
	lcore_id = rte_lcore_id();
    char *pad_key = NULL;


    if ( lcore_id == rte_get_main_lcore() )
        return;

    struct port_params *port_param = hm_config_get_core_rx_param(lcore_id, &queue_id);
    if ( port_param == NULL ) {
        HM_INFO("lcore can't get port param\n");
        return;
    }

	uint16_t port = port_param->port_id;
    if ( !is_rx_port(port_param) ) {
        HM_INFO("port(%d) is tx port , return\n", port);
        return;
    } else {
        HM_INFO("lcore=%d lcore index=%d, port=%d, queue id=%d\n", lcore_id, rte_lcore_index(-1), port, queue_id);
    }

    if (rte_eth_dev_socket_id(port) != port_param->physical_socket ) {
        rte_exit(-1, "port %d socket wrong: %d %d \n", port, rte_eth_dev_socket_id(port), port_param->physical_socket );
    }

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */

    if (rte_eth_dev_socket_id(port) >= 0 &&
        rte_eth_dev_socket_id(port) != (int)rte_socket_id()
    )
        rte_exit(-1, "port %u is on remote NUMA node to "
                "polling thread.\n\tPerformance will "
                "not be optimal.\n", port);

    int self_socket = rte_socket_id();    
    struct hijack_time_params *time_config = NULL;
    if ( global_hm_config->time_config != NULL ) {
        time_config = rte_calloc_socket(NULL, 1, sizeof(struct hijack_time_params), 8, self_socket);        
        memcpy(time_config, global_hm_config->time_config, sizeof(struct hijack_time_params));    
        HM_INFO("default hcount=%02d\n", time_config->default_hcount);
        int n1 = 0;
        for ( ; n1 < time_config->items_count; n1++ ) {
            struct hijack_time_config *pt = &time_config->items[n1];
            HM_INFO("core(%d), time config = %02d:%02d to %02d:%02d, hcount=%d%%\n", lcore_id , pt->start_hour,pt->start_minute, pt->end_hour, pt->end_minute, pt->hcount);
        }        
    }

    pad_key = rte_calloc_socket("hmhash", HM_MAX_DOMAIN_LEN, 1, 8 , self_socket);
    //hash_test(pad_key, "www.163.com");
    
    struct rte_hash * time_hash = NULL;
    if ( time_config != NULL )
        time_hash = time_config_create_hash(self_socket, lcore_id, time_config->max_ip_hash_entities);

    
    uint64_t total_pkts = 0;
    FILE *log_fp = hplog_init(lcore_id);
    const size_t log_data_size = 2048;
    char *log_data = rte_malloc_socket("logdata", log_data_size, 64, self_socket);
	while ( !rte_atomic16_read(&global_exit_flag) )
    {

		/* Get burst of RX packets, from first port of pair. */
		struct rte_mbuf *bufs[BURST_SIZE];
		const uint16_t nb_rx = rte_eth_rx_burst(port, queue_id, bufs, BURST_SIZE);

		if (unlikely(nb_rx == 0))
			continue;

        total_pkts += nb_rx;

        struct tm tms;
        const time_t t = time(NULL);
        localtime_r( &t, &tms );

		int n = 0;
        for (; n< nb_rx;n++)
        {
			struct rte_ether_hdr *eth_hdr;
			uint16_t ether_type,offset=0;

			eth_hdr = rte_pktmbuf_mtod(bufs[n], struct rte_ether_hdr *);
			ether_type = eth_hdr->ether_type;

			if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN))
				offset = get_vlan_offset(eth_hdr, &ether_type);

            if (ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
                continue;

            int ipv4_hdr_len;
            uint16_t ip_total_len;
            struct rte_ipv4_hdr *ipv4_hdr;

            ipv4_hdr = (struct rte_ipv4_hdr *)((uint8_t *)(eth_hdr + 1) + offset);
            ip_total_len = rte_be_to_cpu_16(ipv4_hdr->total_length);
            ipv4_hdr_len = rte_ipv4_hdr_len(ipv4_hdr);

            //uint32_t src_ip = rte_be_to_cpu_32(ipv4_hdr->src_addr);
            //uint32_t dst_ip = rte_be_to_cpu_32(ipv4_hdr->dst_addr);

            if (ipv4_hdr->next_proto_id == IPPROTO_TCP) {
                struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)((unsigned char *)ipv4_hdr + ipv4_hdr_len);
                uint16_t dst_port = rte_be_to_cpu_16(tcp->dst_port);
                //if ( dst_port == 5020 ) {
                if ( dst_port > 0 ) {
                    //printf("t1=%d %d\n", ipv4_hdr_len , sizeof(struct rte_ipv4_hdr) );
                    //printf("find my port(%d) flag=%d vlan_offset=%d\n", rx_port_id, tcp->tcp_flags, offset);
                    uint32_t tcp_head_len = (tcp->data_off & 0xf0) >> 2;
                    unsigned char *content = (unsigned char*)tcp + tcp_head_len;
                    uint32_t content_len = ip_total_len - ipv4_hdr_len - tcp_head_len;

                    size_t host_len = 0;
                    char *host = NULL;
                    char *url = NULL;
                    size_t url_length = 0;
                    char *refer = NULL;
                    size_t refer_length = 0;

                    if (!get_http_host(content, content_len, &host, &host_len, &url, &url_length, &refer, &refer_length)) {
                        continue;
                    }
                    if ( global_work_type == 0 || global_work_type == 3 ) {
                        uint32_t log_offset = 0;
                        uint32_t src_ip = rte_be_to_cpu_32(ipv4_hdr->src_addr);

                        log_offset += snprintf(log_data+log_offset, log_data_size ,"%d%02d%02d%02d%02d%02d ", 
                                        1900+tms.tm_year, 1+tms.tm_mon, tms.tm_mday, tms.tm_hour, tms.tm_min, tms.tm_sec );

                        log_offset += snprintf(log_data+log_offset, log_data_size - log_offset, "%d.%d.%d.%d ", 
                                        src_ip>>24, src_ip>>16&0xff, src_ip>>8&0xff, src_ip&0xff);
                        
                        log_offset += snprintf(log_data + log_offset, log_data_size - log_offset, "%05d ", dst_port);

                        if ( log_offset + url_length + host_len + 2 >= log_data_size )
                            continue;
                        
                        rte_memcpy(log_data + log_offset, host, host_len);
                        log_offset += host_len ;
                        log_data[log_offset] = ' ';
                        log_offset++;

                        rte_memcpy(log_data + log_offset, url , url_length);
                        log_offset += url_length;
                        log_data[log_offset] = '\n';
                        log_offset++;

                        hplog_append_line(log_fp, log_data, log_offset);                        
                    }
                    
                    if ( global_work_type == 1 || global_work_type == 3  ) {
                        //hook http host
                        //HM_INFO("tcp content len=%d, start send hook response\n", content_len);
                        //dump_packet_meta(eth_hdr, ipv4_hdr);
                        memset(pad_key, 0, HM_MAX_DOMAIN_LEN);
                        memcpy(pad_key, host, host_len);
                        snprintf( pad_key + host_len, HM_MAX_DOMAIN_LEN-host_len, ":%d", dst_port);
                        char *target = NULL;
                        hm_hash_search(self_socket, pad_key, (void**)&target );
                        if ( likely(target == NULL) )
                            continue;

                        bool use_time_config = false;
                        if ( target[0] == 'T' && target[1] == ':' ) {
                            use_time_config = true;
                            target += 2;
                        }      

                        // judge if we need hijack by time config
                        //int hcount = 100;
                        bool need_hook = true;
                        uint32_t src_ip = rte_be_to_cpu_32(ipv4_hdr->src_addr);
                        #if 0
                        if ( time_config_get_hijack(time_config, &tms, &hcount) && hcount < 100 && hcount >= 0 ) {
                            //配置文件里设置了当前时间段需要hijack，并且比例小于 100%                            
                            if ( hcount == 0 ) {
                                need_hook = false;
                            } else {                                
                                unsigned int value = 0;
                                /*
                                    使用线性同余法 x(n) = ( a*x(n-1) + b ) mod m                                    
                                    m = 100
                                */
                                value = ( (tms.tm_wday+1)*src_ip + tms.tm_mday*tms.tm_mday ) % 100;
                                if ( value > hcount ) {
                                    need_hook = false;
                                }
                                //HM_INFO("step4 need_hook=%d value=%d\n", need_hook, value);
                            }
                        } else {
                            need_hook = true;
                        }
                        #endif 
                        if ( use_time_config && time_hash )
                            need_hook = time_config_judge_hijack(time_hash, self_socket, src_ip, tms.tm_hour, time_config->default_hcount);

                        if ( global_log_hook ) {

                            /*
                            struct tm tms;
                            const time_t t = time(NULL);
                            localtime_r( &t, &tms);
                            */

                            #if 1
                            HM_INFO("%d%02d%02d%02d%02d%02d, " 
                                "%d.%d.%d.%d, %s Hook %s to %s \n",                                
                                1900+tms.tm_year,1+tms.tm_mon,tms.tm_mday, tms.tm_hour, tms.tm_min, tms.tm_sec,
                                src_ip>>24, src_ip>>16&0xff, src_ip>>8&0xff , src_ip&0xff,
                                need_hook?"need":"not",
                                pad_key, target);
                            #else
                            HM_INFO("%d, %d.%d.%d.%d, Hook %s to %s \n", time(NULL), 
                                src_ip>>24, src_ip>>16&0xff, src_ip>>8&0xff , src_ip&0xff,
                                pad_key, target);
                            #endif
                        }

                        if ( !need_hook )
                            continue;

                        const char *response_format = "HTTP/1.1 302 Found\r\nContent-Length: 0\r\nLocation: %s\r\n\r\n";
                        int data_len = snprintf(pad_key, HM_MAX_DOMAIN_LEN, response_format,  target );

                        uint16_t tx_queue_id = 0;
                        ModifyAndSendPacket(bufs[n],eth_hdr,ipv4_hdr,tcp, port_param->tx_port, tx_queue_id, pad_key, data_len, content_len);                    
                    }
                }
            }

		}  // end for(...)
		uint16_t nb;
		for (nb = 0; nb < nb_rx; nb++)
			rte_pktmbuf_free(bufs[nb]);
	} // end while(...)

    HM_INFO("core(%u) total packets=%lu \n", lcore_id, total_pkts);
}

int hm_worker_run(void *dummy)
{
    _hm_worker_run(dummy);
    return 0;
}

static inline bool
get_http_host(char *content, size_t content_length, char **host, size_t *host_length, char **url, size_t *url_length, char** refer, size_t *refer_length)
{
    if( unlikely(content == NULL || content_length == 0 || host_length == NULL) )
        return false;

    //采用整数，加速匹配
    const uint32_t http_get = 0x47455420; // "GET "
    const uint32_t http_host1 = 0x486F7374; // "Host"
    const uint32_t http_host2 = 0x686F7374; // "host"
    const uint32_t http_referer = 0x52656665; // "Refe rer"

    uint8_t *p = content;
    int n = 0;
    bool find_host = false;

    uint32_t field = rte_be_to_cpu_32(*(uint32_t*)(content));
    if (field != http_get)
        return false;
    n += 4;

    int url_start = -1;
    int url_end = -1;
    //find the url of "GET" line
    for (; n < content_length; n++) {
        if ( p[n] != ' ' && p[n] != '\n' && p[n] != '\r' ) {
            url_start = n;
            break;
        } else if ( p[n] == '\r' || p[n] == '\n' )
            break;
    }

    if ( unlikely(url_start == -1 ) )
        return false;

    for (; n < content_length; n++ ) {
        if ( p[n] == ' ') {
            url_end = n - 1;
            break;
        } else if ( p[n] == '\r' || p[n] == '\n' )
            break;
    }

    if ( unlikely(url_end == -1 ) )
        return false;

    *url_length = url_end - url_start + 1;
    *url = content + url_start;

    //find the "Host" line
    for (; n < content_length; n++) {
        if ( p[n-1] == '\n' ) {
            field = rte_be_to_cpu_32(*(uint32_t*)(p+n));
            if ( field == http_host1 || field == http_host2 ){
                find_host = true;
                break;
            }
        }
    }

    if ( find_host == false )
        return false;

    /*
        p[n] == "Host", now check  "Host: "
    */
   if (!( (n+5)<=content_length-1 && p[n+4] == ':' && p[n+5] == ' ' ))  {
       //printf("%d %c %c\n", (n+5)<=content_length-1, p[n+4], p[n+5]);
       return false;
   }

    n = n + 6;
    uint8_t *host_start = p + n;
    uint8_t *host_end = NULL;
    // now , p[n] point to xxxx in  "Host: xxxx\r\n"
    while ( n < content_length ) {
        if ( p[n] == '\n') {
            if (p[n-1] == '\r')
                host_end = p + n - 2 ;
            else
                host_end = p + n - 1;
            break;
        }
        n++;
    }

    if ( unlikely(host_end == NULL) ) {
        //printf("host_end is null\n");
        return false;
    }

    /*
        可能是以下格式：
        www.a.com
        www.a.com:80
        需要删除可能存在的 ":"
    */
    for ( p = host_start; p <= host_end; p++ ) {
        if ( *p == ':') {
            host_end = p-1;
            break;
        }
    }

    if ( refer != NULL ) {        
        p = content;
        // 开始读取 http refer 的信息
        uint8_t *referer_start = NULL;
        uint8_t *referer_end = NULL;
        bool find_referer = false;        
        n++;
        for (; n < content_length; n++) {            
            if ( p[n-1] == '\n' ) {
                field = rte_be_to_cpu_32(*(uint32_t*)(p+n));
                if ( field == http_referer ){
                    find_referer = true;
                    break;
                }
            }
        }
        if ( find_referer && strncmp( p + n + 4, "rer:", 4 ) == 0) {
            //TODO.... 分析  Referer:  xxxx 的内容
            n += 8;
            for (; n < content_length; n++) {
                if ( referer_start == NULL) {
                    if  ( p[n] == ' ' || p[n] == '\t' ) 
                        continue;
                    else {
                        referer_start = p + n;
                        continue;
                    }                    
                }

                if  ( p[n] == ' ' || p[n] == '\t'  || p[n] == '\r' || p[n] == '\n')  {
                    referer_end = p + n - 1;
                    break;
                }
            }
            if ( referer_start != NULL && referer_end != NULL ) {
                *refer = referer_start;
                *refer_length = referer_end - referer_start + 1;                
            }
        }
        
    }

    int len = host_end - host_start + 1;
    *host = host_start;
    *host_length = len;
    return true;
}
