
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
#include "hmconfig.h"

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32




rte_atomic16_t global_exit_flag ;

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
static bool global_dump_info = false;
int global_work_type = -1;
struct rte_hash *global_domain_hash_sockets[HM_MAX_CPU_SOCKET];

static int
hm_parse_args(int argc, char **argv)
{
	const char short_options[] =
	"f:"  /* config file path */	
	"q:"  /* config file path */
	"T:"  /* timer period */
	"d"   /* dump port info */
	"w:"  /* work type */
	;

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
			case 'q':
                global_coreport_filename = optarg;
                break;
			case 'd':
				global_dump_info = true;
				break;
			case 'w':
				if ( strcmp(optarg, "log") == 0 )
					global_work_type = 0;
				else if ( strcmp(optarg, "hook") == 0)				
					global_work_type = 1;
				else {
					rte_exit(-1, "work type (-w) must be log or hook\n");
				}
				break;
            default:
                break;
        }    
    }

	if ( global_work_type < 0 ) {
		rte_exit(-1, "work type (-w) must be 'log' or 'hook'\n");
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

	if ( global_dump_info ) {
		hm_manager_test();
		return 0;
	}

    HM_LOG(INFO, "config file=%s\n", global_config_filename);

	hm_config_init(global_config_filename, global_coreport_filename);

    struct worker_manager *wm = hm_manager_init(global_config_filename);

	hm_manager_port_init(wm);

	hm_manager_start_run(wm);

	hm_manager_wait_stop(wm);

    //hm_manager_test();

    return 0;
}