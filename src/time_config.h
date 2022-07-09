
#ifndef _TIME_CONFIG_H_
#define _TIME_CONFIG_H_
#include <time.h>
#include <stdbool.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_errno.h>
#include <rte_malloc.h>

#include "libconfig.h"

#define MAX_TIME_CONFIG (2)

struct hijack_time_config {
    int start_hour;
    int start_minute;
    int end_hour;
    int end_minute;
    int hcount;
};

struct hijack_time_params {
    struct hijack_time_config items[MAX_TIME_CONFIG];
    unsigned int items_count;
    int default_hcount;
    uint32_t max_ip_hash_entities;
};

struct hijack_time_hcout {
    uint16_t hour;
    uint16_t hcount;
};

struct hijack_time_params *load_time_config_file(char *filename, int *err);

bool time_config_get_hijack(struct hijack_time_params *tp, struct tm *t, int *hcount);

struct rte_hash* time_config_create_hash(int socketid, int coreid, uint32_t max_items);

bool time_config_judge_hijack(struct rte_hash * thash, uint32_t socketid, uint32_t src_ip, uint32_t current_hour, uint32_t max_hcount);

#endif