
#ifndef _HM_CONFIG_H_
#define _HM_CONFIG_H_

#include "all.h"

#define MAX_DOMAINS (8192)
#define MAX_DOMAIN_LEN (1024)

typedef struct {
    char* domain;
    char* target;
} domain_t;

typedef struct {
    domain_t* domains[MAX_DOMAINS];
    int domains_count;
} domain_conf_t;

struct _core_portqueue {
  int port_id;
  int queue_id;
  int core_id;
};

struct hm_config {
    // number queues of each port
    int port_rx_queue[RTE_MAX_ETHPORTS];
    int port_tx_queue[RTE_MAX_ETHPORTS];
    struct _core_portqueue *core_queue_maps;
    void *domain_hash_handle;
};

extern struct hm_config *global_hm_config;

int hm_config_init(char *config_file);

#endif