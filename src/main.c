#include "main.h"
#include <net/ethernet.h>
/*
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include<linux/sockios.h>
*/
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <linux/if.h>
#include <sys/stat.h>
#include <sys/types.h>


static int
hm_parse_args(int argc, char **argv)
{
	const char short_options[] =
	"f:"  /* config file path */
	"q:"  /* config file path */
	"t:"  /* config file path */
	"T:"  /* timer period */
	"d"   /* dump port info */
	"r:"   /* use raw socket to send */
	"w:"  /* work type */
	"l"   /* log hook */
	;

	int opt;
	char **argvopt;

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
			case 't':
				global_timeconf_filename = optarg;
				break;
			case 'r':
                {
                    struct ifreq  ifr;
                    strncpy(ifr.ifr_name, optarg, IFNAMSIZ);
				    global_rawsocket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
                    if ( global_rawsocket < 0 ) {
                        HM_INFO("raw socket error\n");
                        return -1;
                    }

                    if (ioctl(global_rawsocket, SIOCGIFINDEX, &ifr) == -1) {
                        HM_INFO("ioctl SIOCGIFINDEX error\n");
                        return -1;
                    }
                    global_bind_dev_idx = ifr.ifr_ifindex;

                    struct sockaddr_ll  addr;
                    // Bind socket to interface
                    memset(&addr, 0x0, sizeof(addr));
                    addr.sll_family   = AF_PACKET;
                    addr.sll_protocol = htons(ETH_P_ALL);
                    addr.sll_ifindex  = ifr.ifr_ifindex;
                    if (bind(global_rawsocket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                        close(global_rawsocket);
                        HM_INFO("Failed to bind socket to interface device %s!\n", optarg);
                        return -1; 
                    } else {
                        HM_INFO("bind socket to interface device %s!\n", optarg);
                    }
                }
				break;
			case 'd':
				global_dump_info = true;
				break;
			case 'l':
				global_log_hook = true;
				break;
			case 'w':
				if ( strcmp(optarg, "log") == 0 )
					global_work_type = 0;
				else if ( strcmp(optarg, "hook") == 0)
					global_work_type = 1;
				else if ( strcmp(optarg, "hooklog") == 0)
					global_work_type = 3;
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

    mkdir("./logs/", 0777);

	hm_config_init(global_config_filename, global_coreport_filename, global_timeconf_filename);

    struct worker_manager *wm = hm_manager_init(global_config_filename);

	hm_manager_port_init(wm);

	hm_manager_start_run(wm);

	hm_manager_wait_stop(wm);

    //hm_manager_test();

    return 0;
}
