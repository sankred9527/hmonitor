
#ifndef _HM_CONFIG_H_
#define _HM_CONFIG_H_

#include "all.h"

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

};

extern struct hm_config *global_hm_config;

int hm_config_init(char *config_file);

#endif