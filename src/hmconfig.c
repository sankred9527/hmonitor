
#include "hmconfig.h"
#include "libconfig.h"
#include <search.h>
#include <string.h>
#include <unistd.h>



struct hm_config *global_hm_config;

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


#define _load_one_core_port(port_param , name, point ) \
    do \
        if(!config_setting_lookup_int(port_param, #name, &(point->name))) { \
            HM_INFO("load " #name " wrong\n"); \
            return false; \
        } \
    while (0); 

static bool
load_core_port_config(char *config_file, struct port_params *port_config[], unsigned int max_port) {
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
                HM_INFO("maxium %d / %d domains loaded", i, count);
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

            conf->domains[i] = calloc(1, sizeof(domain_t));
            conf->domains[i]->domain = calloc(HM_MAX_DOMAIN_LEN, sizeof(char));
            conf->domains[i]->target = calloc(HM_MAX_DOMAIN_LEN, sizeof(char));
            
            strncpy(conf->domains[i]->domain, _domain, strlen(_domain));
            strncpy(conf->domains[i]->target, _target, strlen(_target));
            //HM_LOG(INFO, "domain: %s --> target: %s\n", conf->domains[i]->domain, conf->domains[i]->target);
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

extern void hm_hash_init();

int hm_config_init(char *domain_config_file, char *coreport_config_file)
{
    global_hm_config = calloc(sizeof(*global_hm_config), 1);
    if ( global_hm_config == NULL )
        rte_exit(-1, "malloc hm config failed");

    global_hm_config->domain_config = load_domain_config(domain_config_file);

    if ( !load_core_port_config(coreport_config_file, global_hm_config->port_config, RTE_MAX_ETHPORTS) ){
        rte_exit(-1, "load coreport config failed\n");
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


