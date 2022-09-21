#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include "all.h"
#include "manager.h"
#include "hmconfig.h"


#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32


rte_atomic16_t global_exit_flag;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_hf = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_SCTP,
		}
	},
};

static char *global_config_filename = NULL;
static char *global_coreport_filename = NULL;
static char *global_timeconf_filename = NULL;
static bool global_dump_info = false;
bool global_log_hook = false;
int global_work_type = -1;
struct rte_hash *global_domain_hash_sockets[HM_MAX_CPU_SOCKET];
int global_rawsocket = -1;
int global_bind_dev_idx = -1;
size_t global_max_log_size_in_bytes = 200; //1024*1024*1024;
