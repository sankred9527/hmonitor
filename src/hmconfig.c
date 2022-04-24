
#include "hmconfig.h"
#include "libconfig.h"
#include <search.h>
#include <string.h>
#include <unistd.h>



struct hm_config *global_hm_config;

//bool reload_config();

const domain_conf_t* conf;

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
            if(i >= MAX_DOMAINS) {
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
            
            if(strlen(_domain) >= MAX_DOMAIN_LEN) {
                HM_LOG(WARNING, "domain length too long( > %d): %s\n", MAX_DOMAIN_LEN, _domain);
                continue;
            } 
            
            if(!config_setting_lookup_string(domain, "target", &_target)) {
                HM_LOG(WARNING, "domain %s has no target\n", _domain);
                continue;
            } else if(strlen(_target) >= MAX_DOMAIN_LEN) {
                HM_LOG(WARNING, "target length too long( > %d): %s\n", MAX_DOMAIN_LEN, _target);
                continue;
            }

            conf->domains[i] = calloc(1, sizeof(domain_t));
            conf->domains[i]->domain = calloc(MAX_DOMAIN_LEN, sizeof(char));
            conf->domains[i]->target = calloc(MAX_DOMAIN_LEN, sizeof(char));
            
            strncpy(conf->domains[i]->domain, _domain, strlen(_domain));
            strncpy(conf->domains[i]->target, _target, strlen(_target));
            HM_LOG(INFO, "domain: %s --> target: %s\n", conf->domains[i]->domain, conf->domains[i]->target);
        }
        conf->domains_count = i;
        HM_LOG(INFO, "total: %d domains\n", conf->domains_count);

        // create hash table for domains
        if(!hcreate(conf->domains_count)) {
            HM_LOG(ERR, "\ncreate hash table for %d domains failed, exit.\n", conf->domains_count);
            exit(EXIT_FAILURE);
        }

        ENTRY item;
        for(i=0; i<conf->domains_count; i++) {
            item.key = conf->domains[i]->domain;
            item.data = conf->domains[i]->target;
            
            (void) hsearch(item, ENTER);
        }
    }    
    config_destroy(&cfg);
    return conf;
}


int hm_config_init(char *domain_config_file)
{
    global_hm_config = calloc(sizeof(*global_hm_config), 1);
    if ( global_hm_config == NULL )
        rte_exit(-1, "malloc hm config failed");

    //global_hm_config->domain_config = load_domain_config(domain_config_file);



    return 0;
}


