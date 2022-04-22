
#ifndef _MANAGER_H_
#define _MANAGER_H_

#include <stdbool.h>
#include <stdint.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

struct hm_port {
    uint16_t port_id;
    uint16_t nb_queues;    
};

struct _core_queue_maps {
    uint16_t port_id;
    uint16_t queue_id;    
    int core_id;
};

struct worker_manager {
    uint32_t nb_ports;
    uint32_t nb_sockets;
    uint32_t nb_cores;
    struct rte_mempool **mbuf;
};

struct worker_manager *hm_worker_init(char *config_filename);

void hm_worker_test();


#endif