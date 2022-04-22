
#include <stdint.h>
#include <inttypes.h>
#include "all.h"
#include <signal.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <getopt.h>
#include "manager.h"

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32




static rte_atomic16_t global_exit_flag ;

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

static const char short_options[] =
	"f:"  /* config file path */	
	"T:"  /* timer period */
	;

static char *global_config_filename = NULL;

static int
hm_parse_args(int argc, char **argv)
{
	int opt, ret, timer_secs;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	argvopt = argv;

	while ((opt = getopt(argc, argvopt, short_options)) != EOF) {

		switch (opt) {
		/* portmask */
	    	case 'f':
                global_config_filename = optarg;
                break;
            default:
                break;
        }    
    }

    return 0;
}

static void
signal_handler(int signum)
{
	printf("\nSignal %d received\n", signum);
	rte_atomic16_set(&global_exit_flag, 1);
}

int
main(int argc, char *argv[])
{
	void *sigret;
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint16_t portid;

	rte_atomic16_init(&global_exit_flag);	
	sigret = signal(SIGTERM, signal_handler);
	if (sigret == SIG_ERR)
		rte_exit(EXIT_FAILURE, "signal(%d, ...) failed", SIGTERM);

	sigret = signal(SIGINT, signal_handler);
	if (sigret == SIG_ERR)
		rte_exit(EXIT_FAILURE, "signal(%d, ...) failed", SIGINT);    


	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = hm_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid hmonitor arguments\n");


    //HM_LOG(INFO, "config file=%s\n", global_config_filename);

    struct worker_manager *wm = hm_worker_init(global_config_filename);

    hm_worker_test();

    return 0;
}