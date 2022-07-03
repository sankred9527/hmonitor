
#ifndef _TIME_CONFIG_H_
#define _TIME_CONFIG_H_

#include "libconfig.h"

#define MAX_TIME_CONFIG (2)

struct hijack_time_config {
    int start_hour;
    int start_minute;
    int end_hour;
    int end_minute;
    int percent;
};

struct hijack_time_params {
    struct hijack_time_config items[MAX_TIME_CONFIG];
    int default_percent;
};

struct hijack_time_params *load_time_config_file(char *filename, int *err);

#endif