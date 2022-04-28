
#ifndef _MANAGER_H_
#define _MANAGER_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <rte_eal.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include "all.h"
#include "worker.h"
#include "hmconfig.h"

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
    struct _core_queue_maps *core_queue_maps;
};

struct worker_manager *hm_manager_init(char *config_filename);
struct rte_mempool *hm_manager_get_mbuf(struct worker_manager *wm, uint16_t port_id, uint16_t physical_socket_id);
struct rte_mempool *hm_manager_set_mbuf(struct worker_manager *wm, struct rte_mempool *mbuf, uint16_t port_id, uint16_t physical_socket_id);
void hm_manager_start_run(struct worker_manager *wm);
void hm_manager_wait_stop(struct worker_manager *wm);
void hm_manager_port_init(struct worker_manager *wm);
void hm_manager_port_stat(void);
void hm_manager_test();

#endif