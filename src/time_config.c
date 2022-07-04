
#include <stdlib.h>
#include <unistd.h>


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
    
    if ( !config_lookup_int(&cfg, "default_percent", &tparams->default_percent) ) {
        *err = -3;
        goto out;
    }

    config_setting_t *time_config = config_lookup(&cfg, "time_config");
    if ( time_config == NULL ) {
        *err = -4;
        goto out;
    }

    int count = config_setting_length(time_config);
    if ( count > MAX_TIME_CONFIG ) {
        *err = -100;
        goto out;
    }

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
        if(!config_setting_lookup_int(tc, "percent", &tparams->items[i].percent )) {
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

inline bool time_config_get_hijack(struct hijack_time_params *tp, struct tm *t, int *percent) {
    int n;
    bool find = false;
    
    if ( tp == NULL )
        return false;

    *percent = -1;

    for (n = 0; n< MAX_TIME_CONFIG; n++) {
        if ( t->tm_hour >= tp->items[n].start_hour && t->tm_min >= tp->items[n].start_minute &&
            t->tm_hour <= tp->items[n].end_hour && t->tm_min <= tp->items[n].end_minute 
        ) {
            *percent = tp->items[n].percent;
            find = true;
            break;
        }
    }

    if ( find == false ) {
        *percent = tp->default_percent;
    }

    return true;
}