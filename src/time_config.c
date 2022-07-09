
#include <stdlib.h>
#include <unistd.h>

#include "all.h"
#include "time_config.h"

struct hijack_time_params *load_time_config_file(char *config_file, int *err)
{    
    if ( 0 != access(config_file, F_OK) ){
        *err = -1;
        return NULL;
    }
        
    struct hijack_time_params *tparams = calloc(1, sizeof(*tparams));
    config_t cfg;
    config_init(&cfg);

    /* Read the file. If there is an error, report it and exit. */
    if(!config_read_file(&cfg, config_file)) {
        *err = -2;
        goto out;
    }
    
    if ( !config_lookup_int(&cfg, "default_hcount", &tparams->default_hcount) ) {
        *err = -3;
        goto out;
    }

    if ( !config_lookup_int(&cfg, "max_ip_hash_entities", &tparams->max_ip_hash_entities) ) {
        *err = -4;
        goto out;
    }

    

    config_setting_t *time_config = config_lookup(&cfg, "time_config");
    if ( time_config == NULL ) {
        *err = -5;
        goto out;
    }

    int count = config_setting_length(time_config);
    if ( count > MAX_TIME_CONFIG ) {
        *err = -100;
        goto out;
    }

    tparams->items_count = count;

    int i;
    for(i = 0; i < count; ++i)
    {
        config_setting_t *tc = config_setting_get_elem(time_config, i);             
        if(!config_setting_lookup_int(tc, "start_hour", &(tparams->items[i].start_hour) )) {
            continue;
        }
        if(!config_setting_lookup_int(tc, "start_minute", &tparams->items[i].start_minute )) {
            continue;
        }
        if(!config_setting_lookup_int(tc, "end_hour", &tparams->items[i].end_hour )) {
            continue;
        }
        if(!config_setting_lookup_int(tc, "end_minute", &tparams->items[i].end_minute )) {
            continue;
        }
        if(!config_setting_lookup_int(tc, "hcount", &tparams->items[i].hcount )) {
            continue;
        }
    }
    config_destroy(&cfg);
    return tparams;
out:
    config_destroy(&cfg);
    free(tparams);    
    return NULL;
    
}

inline bool time_config_get_hijack(struct hijack_time_params *tp, struct tm *t, int *hcount) {
    int n;
    bool find = false;
    
    if ( tp == NULL )
        return false;

    *hcount = -1;

    for (n = 0; n< tp->items_count; n++) {
        if ( t->tm_hour >= tp->items[n].start_hour && t->tm_min >= tp->items[n].start_minute &&
            t->tm_hour <= tp->items[n].end_hour && t->tm_min <= tp->items[n].end_minute 
        ) {
            *hcount = tp->items[n].hcount;
            find = true;
            break;
        }
    }

    if ( find == false ) {
        *hcount = tp->default_hcount;
    }

    return true;
}

 struct rte_hash * time_config_create_hash(int socketid, int coreid, uint32_t max_items){

    char *name = malloc(32);
    snprintf(name,32 , "timehash%d-%d", socketid, coreid);
	struct rte_hash_parameters params_pseudo_hash = {
		.name = name,
		.entries = max_items, // 256*256*256 = 16777216
		.key_len = 4,
		.hash_func = rte_jhash,
		.hash_func_init_val = 0,
		.socket_id = socketid,
	};
    return rte_hash_create(&params_pseudo_hash);
}

/*
    current_hour : 0-23
*/
bool time_config_judge_hijack(struct rte_hash * thash, uint32_t socketid, uint32_t src_ip, uint32_t current_hour, uint32_t max_hcount) {
    struct hijack_time_hcout *data;

    int ret = rte_hash_lookup_data(thash, &src_ip, (void**)&data);
    if ( ret < 0 ) {
        if ( ret == -ENOENT ) {
            data = rte_calloc_socket(NULL, 1, sizeof(struct hijack_time_hcout), 8, socketid);
            if ( data == NULL ) {
                HM_INFO("memory too low\n");
                return false;
            }
            
            int ret = rte_hash_add_key_data(thash, &src_ip, data);
            if ( ret < 0 ) {
                HM_INFO("add hash key failed\n");
                return false;
            }

        } else {
            HM_INFO("rte_hash_lookup_data ip error = %d\n", ret);
            return false;
        }
    }

    if ( data->hour != current_hour ) {
        data->hour = current_hour;
        data->hcount = 0;        
    }        

    
    if ( data->hour == current_hour )   {
        if ( data->hcount < max_hcount ){
            data->hcount += 1;
            //HM_INFO("add hcount=%d hour=%d\n", data->hcount, current_hour);
            return true;
        } else {
            //HM_INFO("false hcount=%d hour=%d\n", data->hcount, current_hour);
            return false;
        }        
    }

    return false;
}