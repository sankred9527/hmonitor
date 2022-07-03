#include "main.h"


static int
hm_parse_args(int argc, char **argv)
{
	const char short_options[] =
	"f:"  /* config file path */
	"q:"  /* config file path */
	"t:"  /* config file path */
	"T:"  /* timer period */
	"d"   /* dump port info */
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

	hm_config_init(global_config_filename, global_coreport_filename, global_timeconf_filename);

    struct worker_manager *wm = hm_manager_init(global_config_filename);

	hm_manager_port_init(wm);

	hm_manager_start_run(wm);

	hm_manager_wait_stop(wm);

    //hm_manager_test();

    return 0;
}