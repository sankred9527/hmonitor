#include "hmconfig.h"

#include <sys/socket.h>
#include <arpa/inet.h>

struct hm_config *global_hm_config;
extern size_t global_max_log_size_in_bytes;

extern void hm_hash_init();

//bool reload_config();

struct port_params *hm_config_get_port_param(uint16_t port){
    int n ;
    for (n=0; n< HM_MAX_ETHPORTS; n++)
        if ( global_hm_config->port_config[n]->port_id == port )
            return global_hm_config->port_config[n];
    return NULL;
}

struct port_params *hm_config_get_core_rx_param(uint16_t core, uint16_t *queue_id){
    int n ;
    for (n=0; n< HM_MAX_ETHPORTS; n++) {
        struct port_params *p = global_hm_config->port_config[n];
        if ( p == NULL || p->tx_port < 0 ) // this is port for tx
        {
            continue;
        }
        int k;
        for (k=0; k<HM_MAX_PORT_QUEUE; k++ ){
            if ( p->rxqueue_to_core[k] < 0 )
                break;
            if ( p->rxqueue_to_core[k] == core ) {
                *queue_id = k;
                return p;
            }
        }
    }

    *queue_id = -1;
    return NULL;
}

long parse_size_str(char *cfg){
    const size_t oneK = 1024;
    const size_t oneM = oneK*oneK;
    const size_t oneG = 1024*1024*1024*1;

    char *p = cfg;

    while ( isspace(*p) || isdigit(*p) ) {
        p++;
    }

    char k = *p;
    *p = 0;

    long n = atol(cfg);

    switch(k) {
        case 'G':
        case 'g':
            n = oneG * n;
            break;
        case 'm':
        case 'M':
            n = oneM * n;
            break;
        case 'k':
        case 'K':
            n = oneK * n;
            break;
        default:
            if ( isspace(k) || k == 0  ) {
                //do nothing
            } 
            else {
                return -1;
            }
    }


    return n;
}


static int ipstr_to_range(char *ip_str, uint32_t *ip, uint32_t *mask)
{
    char *prefix_end, *prefix;
    unsigned int prefixlen ;
    const int PREFIXMAX = 32;

    prefix = strrchr(ip_str, '/');
    if (prefix == NULL) {
        prefixlen = PREFIXMAX;
    } else {
        *prefix = '\0';
        prefix++;
        errno = 0;
        prefixlen = strtol(prefix, &prefix_end, 10); 

        if (errno || (*prefix_end != '\0')
                || prefixlen < 0 || prefixlen > PREFIXMAX) {

            HM_INFO("ip mask %d error \n", prefixlen);
            return -1;
        }
    }
    if ( inet_pton(AF_INET, ip_str, ip) != 1 ) {
        HM_INFO("load ip %s error\n", ip_str);
        return -1;
    }
    *ip = ntohl(*ip);
    if ( prefixlen >= 32  ) 
        *mask = 0xffffffff;
    else
        *mask = ( (1<<prefixlen) - 1 ) << (PREFIXMAX-prefixlen) ;

    return 0 ;
}

#define _load_one_core_port(port_param , name, point ) \
    do \
        if(!config_setting_lookup_int(port_param, #name, &(point->name))) { \
            HM_INFO("load " #name " wrong\n"); \
            return false; \
        } \
    while (0)

static bool
load_core_config(char *config_file, struct hm_config* _hm_config) {
    if ( 0 != access(config_file, F_OK) ){
        HM_LOG(ERR, "%s not exists\n", config_file);
        exit(EXIT_FAILURE);
    }
    struct port_params **port_config = _hm_config->port_config;
    ip2ttl_t **ip2ttl_configs = _hm_config->ip2ttls;

    unsigned int max_port = RTE_MAX_ETHPORTS;
    HM_LOG(INFO, "load config file %s\n", config_file);
    domain_conf_t* conf = calloc(1, sizeof(domain_conf_t));


    config_t cfg;
    config_init(&cfg);

    /* Read the file. If there is an error, report it and exit. */
    if(!config_read_file(&cfg, config_file)) {
        HM_LOG(ERR,
            "%s:%d - %s\n",
            config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg)
        );
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }

    config_setting_t *all_ports = config_lookup(&cfg, "ports_conf");
    if ( all_ports == NULL ) {
        rte_exit(-1, "Can't find ports_conf in config file\n");
    }

    int count = config_setting_length(all_ports);
    if ( count <= 0 )
        rte_exit(-1, "zero ports_conf in config file\n");

    if ( count > max_port )
        rte_exit(-1, "max ports in config file\n");


    int i;
    for(i = 0; i < count; ++i) {
        config_setting_t *port_param = config_setting_get_elem(all_ports, i);
        port_config[i] = calloc(sizeof(struct port_params), 1);
        struct port_params *pc = port_config[i];

        _load_one_core_port(port_param , port_id, pc);
        _load_one_core_port(port_param , nb_rx_queues, pc);
        _load_one_core_port(port_param , nb_tx_queues, pc);
        _load_one_core_port(port_param , tx_port, pc);
        _load_one_core_port(port_param , physical_socket, pc);

        //HM_INFO("nb rx queue=%d port=%d\n", pc->nb_rx_queues, pc->port_id);
        config_setting_t *rxqueue_to_core = config_setting_lookup(port_param, "rxqueue_to_core");

        int k = 0;
        int core_queue_cnt = config_setting_length(rxqueue_to_core);
        for (k=0; k < HM_MAX_PORT_QUEUE; k++) {
            if ( k < core_queue_cnt )
                pc->rxqueue_to_core[k] = config_setting_get_int_elem(rxqueue_to_core, k);
            else
                pc->rxqueue_to_core[k] = -1;
        }
    }

    //log size
    char *size_cfg = NULL;
    if ( !config_lookup_string(&cfg, "default_log_size", (const char**)&size_cfg) ) {
        size_cfg = strdup("1G");
    } 
    global_max_log_size_in_bytes = parse_size_str(size_cfg);
    HM_INFO("default log_size is %d\n", global_max_log_size_in_bytes);
    

    // ttl configs 
    if ( !config_lookup_int(&cfg, "default_ttl", &_hm_config->default_ttl)  ) {
        return false;
    } 
    HM_INFO("default TTL is %d\n", _hm_config->default_ttl);
    if ( _hm_config->default_ttl > 0xff ) {
        HM_INFO("default TTL must less than 256 \n");
        return false;
    }

    config_setting_t *all_ttls = config_lookup(&cfg, "ttl_conf");
    if ( all_ttls == NULL ) {
        return true;
    }

    count = config_setting_length(all_ttls);
    if ( count <= 0 )
        return true;

    if ( count > HM_MAX_IP2TTL_CONFIG)
        rte_exit(-1, "max ttl in config file\n");

    for(i = 0; i < count; ++i) {
        config_setting_t *ttl_param = config_setting_get_elem(all_ttls, i);
        ip2ttl_t *ttl = ip2ttl_configs[i] = calloc(sizeof(ip2ttl_t), 1);

        char *ip_range = NULL;
        if(!config_setting_lookup_string(ttl_param, "src", (const char**)&ip_range)) {
            HM_INFO("ttlconfig: load src  wrong\n"); 
            return false; 
        } 

        if ( 0 != ipstr_to_range((char*)ip_range, &ttl->src, &ttl->mask) ) {
            HM_INFO("ttlconfig : %s format error\n", ip_range);
            return false;
        }
        HM_INFO("ip range=%s %x %x\n",ip_range, ttl->src, ttl->mask);

        if(!config_setting_lookup_int(ttl_param, "ttl", &(ttl->ttl))) {
            HM_INFO("ttlconfig: load ttl wrong\n"); 
            return false; 
        } 

        if ( ttl->ttl > 0xff ) {
            HM_INFO("load TTL(%d) must less than 256 \n", ttl->ttl);
            return false;
        }
    }


    return true;
}

static const domain_conf_t*
load_domain_config(char *config_file) {
    if ( 0 != access(config_file, F_OK) ){
        HM_LOG(ERR, "%s not exists\n", config_file);
        exit(EXIT_FAILURE);
    }
    HM_LOG(INFO, "load config file %s\n", config_file);
    domain_conf_t* conf = calloc(1, sizeof(domain_conf_t));

    config_t cfg;
    config_init(&cfg);

    /* Read the file. If there is an error, report it and exit. */
    if(!config_read_file(&cfg, config_file)) {
        HM_LOG(ERR,
            "%s:%d - %s\n",
            config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg)
        );
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }


    config_setting_t *domains = config_lookup(&cfg, "domains");
    if(domains != NULL)
    {
        int count = config_setting_length(domains);
        int i;

        for(i = 0; i < count; ++i)
        {
            if(i >= HM_MAX_DOMAINS) {
                HM_INFO("maxium %d / %d domains loaded\n", i, count);
                break;
            }

            config_setting_t *domain = config_setting_get_elem(domains, i);
            const char * _domain = NULL;
            const char * _target = NULL;

            conf->domains[i] = NULL;
            if(!config_setting_lookup_string(domain, "domain", &_domain)) {
                continue;
            }

            if(strlen(_domain) >= HM_MAX_DOMAIN_LEN) {
                HM_LOG(WARNING, "domain length too long( > %d): %s\n", HM_MAX_DOMAIN_LEN, _domain);
                continue;
            }

            if(!config_setting_lookup_string(domain, "target", &_target)) {
                HM_LOG(WARNING, "domain %s has no target\n", _domain);
                continue;
            } else if(strlen(_target) >= HM_MAX_DOMAIN_LEN) {
                HM_LOG(WARNING, "target length too long( > %d): %s\n", HM_MAX_DOMAIN_LEN, _target);
                continue;
            }

            int use_time_config = 0;
            config_setting_lookup_bool(domain, "use_time_config", &use_time_config);

            conf->domains[i] = calloc(1, sizeof(domain_t));
            conf->domains[i]->domain = calloc(HM_MAX_DOMAIN_LEN, sizeof(char));
            conf->domains[i]->target = calloc(HM_MAX_DOMAIN_LEN, sizeof(char));
            conf->domains[i]->use_time_config = use_time_config;

            memcpy(conf->domains[i]->domain, _domain, strlen(_domain));
            memcpy(conf->domains[i]->target, _target, strlen(_target));
            //HM_LOG(INFO, "domain: %s --> target: %s tc=%d\n", conf->domains[i]->domain, conf->domains[i]->target, conf->domains[i]->use_time_config);
        }
        conf->domains_count = i;
        HM_LOG(INFO, "total: %d domains\n", conf->domains_count);
    }
    config_destroy(&cfg);
    return conf;
}

static void dump_one_port_config(struct port_params *p) {
    HM_INFO("port id=%d nb_rx_queues=%d nb_tx_queues=%d pyhiscal_socket=%d tx_port=%d\n", p->port_id, p->nb_rx_queues, p->nb_tx_queues, p->physical_socket,p->tx_port);
    int k ;
    char data[256];
    char *s = data;
    for (k=0; k < HM_MAX_PORT_QUEUE; k++) {
        s += sprintf(s, "%d ", p->rxqueue_to_core[k]);
    }
    HM_INFO("port queue map=%s\n", data);
}

static void dump_port_config() {
    int n;
    for (n=0;n <RTE_MAX_ETHPORTS; n++) {
        struct port_params *p = global_hm_config->port_config[n];
        if ( p == NULL )
            continue;

        HM_INFO("port id=%d nb_rx_queues=%d nb_tx_queues=%d pyhiscal_socket=%d tx_port=%d\n", p->port_id, p->nb_rx_queues, p->nb_tx_queues, p->physical_socket,p->tx_port);
        int k ;
        char data[256];
        char *s = data;
        for (k=0; k < HM_MAX_PORT_QUEUE; k++) {
            s += sprintf(s, "%d ", p->rxqueue_to_core[k]);
        }
        HM_INFO("port queue map=%s\n", data);
    }
}

int hm_config_init(char *domain_config_file, char *coreport_config_file, char *time_config_file)
{
    global_hm_config = calloc(sizeof(*global_hm_config), 1);
    if ( global_hm_config == NULL )
        rte_exit(-1, "malloc hm config failed");

    global_hm_config->domain_config = load_domain_config(domain_config_file);

    if ( !load_core_config(coreport_config_file, global_hm_config) ){
        rte_exit(-1, "load coreport config failed\n");
    }

    if ( time_config_file != NULL ) {
        int err = 0;
        global_hm_config->time_config = load_time_config_file(time_config_file, &err);
        if ( global_hm_config->time_config == NULL ) {
            rte_exit(-1, "load time config failed, err=%d\n", err);
        }
        HM_INFO("load time config file done\n");
    }
    

    hm_hash_init();

#if 0
    uint16_t qid;
    int core = 0;
    struct port_params *pp = hm_config_get_core_rx_param(core, &qid);
    if ( pp == NULL ) {
        printf("can't find core %d params\n", core);
    } else {
        printf("find core %d params, queue=%d\n", core, qid);
        dump_one_port_config(pp);
    }
    //dump_port_config();
#endif

    return 0;
}
