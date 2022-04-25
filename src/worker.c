
#include "all.h"
#include "worker.h"
#include "hmconfig.h"
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_graph_worker.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_node_eth_api.h>
#include <rte_node_ip4_api.h>
#include <rte_per_lcore.h>
#include <rte_string_fns.h>
#include <rte_vect.h>

#define BURST_SIZE 32

extern rte_atomic16_t global_exit_flag ;

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

static inline bool is_rx_port(struct port_params *port_param) 
{
    return port_param->tx_port >= 0;
}

void hm_worker_run(void *dummy)
{
	uint32_t lcore_id;
    uint16_t queue_id;
	lcore_id = rte_lcore_id();

    if ( lcore_id == rte_get_main_lcore() )
        return;

    struct port_params *port_param = hm_config_get_core_rx_param(lcore_id, &queue_id);
    if ( port_param == NULL ) {
        HM_INFO("lcore can't get port param\n");
        return;
    }
    
    HM_INFO("lcore=%d lcore index=%d, queue id=%d\n", lcore_id, rte_lcore_index(-1), queue_id);
	uint16_t port = port_param->port_id;
    if ( !is_rx_port(port_param) ) {
        HM_INFO("port(%d) is tx port , return\n", port);
        return;
    } else {
        //HM_INFO("port(%d) is rx port\n", port);
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

	while ( !rte_atomic16_read(&global_exit_flag) ) 
    {

		/* Get burst of RX packets, from first port of pair. */
		struct rte_mbuf *bufs[BURST_SIZE];
		const uint16_t nb_rx = rte_eth_rx_burst(port, queue_id, bufs, BURST_SIZE);

		if (unlikely(nb_rx == 0))
			continue;

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

                    char data[1024];
                    size_t data_len = 1024;
                    char *host = NULL;
                    if (get_http_host(content, content_len, &host, &data_len)) {
                        //printf("tcp content len=%d, start send hook response\n", content_len);
                        //dump_packet_meta(eth_hdr, ipv4_hdr);                        
                        memcpy(data, host, data_len);
                        data[data_len] = '\0';
                        printf("log host=%s\n", data);
                        printf("log nq=%d core=%d\n", queue_id,lcore_id);
                        //hplog_append_line(fp, host, data_len);

                    }                    
                    #if 0
                    if (hook_http_response_fast(content, content_len, data, &data_len)) {
                        HM_INFO("tcp content len=%d, start send hook response\n", content_len);
                        dump_packet_meta(eth_hdr, ipv4_hdr);
                        HM_INFO("hook ok=%s\n", data);
                        //tx 
                        //modify_and_send_packet(bufs[n],eth_hdr,ipv4_hdr,tcp, port_param->tx_port, 0, data, data_len, content_len);
                    }
                    #endif
                }
            }
        
		}  // end for(...)
		uint16_t nb;
		for (nb = 0; nb < nb_rx; nb++)
			rte_pktmbuf_free(bufs[nb]);		
	} // end while(...)

}