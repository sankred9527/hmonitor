
#ifndef _HM_CONFIG_H_
#define _HM_CONFIG_H_

#include <search.h>
#include <string.h>
#include <unistd.h>

#include "all.h"
#include "libconfig.h"
#include "time_config.h"

#define HM_MAX_ETHPORTS (8)
#define HM_MAX_PORT_QUEUE (32)
#define HM_MAX_DOMAINS (8192*100)
#define HM_MAX_DOMAIN_LEN (1024)
#define HM_MAX_CPU_SOCKET (32)

typedef struct {
    char* domain;
    char* target;
} domain_t;

typedef struct {
    domain_t* domains[HM_MAX_DOMAINS];
    int domains_count;
} domain_conf_t;

struct port_params {
    unsigned int port_id;
    unsigned int nb_rx_queues;
    unsigned int nb_tx_queues;
    unsigned int tx_port;
    unsigned int physical_socket;
    unsigned int rxqueue_to_core[HM_MAX_PORT_QUEUE];
};

struct hm_config {
    // number queues of each port
    struct port_params *port_config[HM_MAX_ETHPORTS];
    const domain_conf_t* domain_config;
    const struct hijack_time_params *time_config;
    void *domain_hash_handle;
};

extern struct hm_config *global_hm_config;

int hm_config_init(char *domain_config_file, char *coreport_config_file, char* time_config_file);

struct port_params *hm_config_get_port_param(uint16_t port);

struct port_params *hm_config_get_core_rx_param(uint16_t core, uint16_t *queue_id);

#endif