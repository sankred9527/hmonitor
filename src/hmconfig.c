
#include "hmconfig.h"

struct hm_config *global_hm_config;

int hm_config_init(char *config_file)
{
    global_hm_config = calloc(sizeof(*global_hm_config), 1);
    if ( global_hm_config == NULL )
        rte_exit(-1, "malloc hm config failed");

    

    return 0;
}