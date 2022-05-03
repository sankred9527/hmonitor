#ifndef _WORKER_H_
#define _WORKER_H_

#include <stdint.h>
#include <sys/types.h>
#include <string.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
//#include <rte_graph_worker.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_node_eth_api.h>
#include <rte_node_ip4_api.h>
#include <rte_per_lcore.h>
#include <rte_string_fns.h>
#include <rte_vect.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_errno.h>

#include "all.h"
#include "hmconfig.h"
#include "../lib/llhttp/llhttp.h"
#include "hmconfig.h"

typedef struct {
    char *host;
    char *port;
    char *url;
    char *referer;
    char *data; // wrap to original parser->(void *)data;
} what_we_want_t;

int hm_worker_run(void *dummy);

#endif
