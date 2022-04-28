#include "manager.h"


struct rte_mempool *hm_manager_get_mbuf(struct worker_manager *wm, uint16_t port_id, uint16_t physical_socket_id) {
    struct rte_mempool **ret = &(wm->mbuf[0]);

    uint32_t offset = port_id*wm->nb_ports + physical_socket_id;
    if ( offset  >=  wm->nb_ports*wm->nb_sockets ) {
        return NULL;
    }
    return ret[offset];
}

struct rte_mempool *hm_manager_set_mbuf(struct worker_manager *wm, struct rte_mempool *mbuf, uint16_t port_id, uint16_t physical_socket_id) {
    uint32_t offset = port_id*(wm->nb_ports-1) + physical_socket_id;

    if ( offset >= wm->nb_ports*wm->nb_sockets ) {
        printf("offset=%d  %d %d\n",offset, wm->nb_ports,wm->nb_sockets);
        return NULL;
    }
    struct rte_mempool **ret = &(wm->mbuf[offset]);
    *ret = mbuf;
    return mbuf;
}

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
			//.rss_hf = ETH_RSS_IPV4   ,
		}
	},
};

static inline int
port_init(struct worker_manager *wm, uint16_t port)
{
    #define RX_RING_SIZE 4096
    #define TX_RING_SIZE 4096
    struct rte_eth_conf port_conf;
	uint16_t rx_rings, tx_rings;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;
    struct rte_mempool *mbuf;

    if ( port >= wm->nb_ports )
        rte_exit(-1, "port init : port wrong=%d\n", port);

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0)
		rte_exit(-1, "Error during getting device (port %u) info: %s\n", port, strerror(-retval));

    struct port_params *port_param = hm_config_get_port_param(port);
    if ( port_param == NULL)
        rte_exit(-1, "get port param(%d) from config file failed\n", port);

    rx_rings = port_param->nb_rx_queues;
    tx_rings = port_param->nb_tx_queues;

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

    port_conf = port_conf_default;

	port_conf.rx_adv_conf.rss_conf.rss_hf &=
		dev_info.flow_type_rss_offloads;
	if (port_conf.rx_adv_conf.rss_conf.rss_hf !=
			port_conf_default.rx_adv_conf.rss_conf.rss_hf) {
		HM_LOG(WARNING,"Port %u modified RSS hash function based on hardware support,"
			"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			port,
			port_conf_default.rx_adv_conf.rss_conf.rss_hf,
			port_conf.rx_adv_conf.rss_conf.rss_hf);
	}

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;
    HM_INFO("port(%d) set rx_queue=%d tx_queue=%d\n", port, rx_rings, tx_rings);

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;
    HM_INFO("port(%d) set nb_rxd=%d nb_txd=%d\n",port, nb_rxd, nb_txd);

    //TODO:
    int port_socket_id = rte_eth_dev_socket_id(port);
    HM_INFO("port(%d) socket_id=%d, physical socketid=%d\n" , port , port_socket_id, rte_socket_id_by_idx(port_socket_id) );
    if ( rte_socket_id_by_idx(port_socket_id) != port_param->physical_socket) {
        rte_exit(-1, "port socket id in config file != real socket id\n");
    }

    char mbuf_name[32];
    snprintf(mbuf_name,32,"MBUF_POOL_%d_%d", port,port_socket_id);
	mbuf = rte_pktmbuf_pool_create(mbuf_name, 8191 * 16,
		250, 0, RTE_MBUF_DEFAULT_BUF_SIZE, port_socket_id);

    if (mbuf==NULL) {
        rte_exit(-1, "rte_pktmbuf_pool_create error (port %u , socket id %u) info: %s\n", port, port_socket_id, strerror(-retval));
    }

    if ( NULL == hm_manager_set_mbuf(wm, mbuf, port, port_socket_id) )
        rte_exit(-1, "hm_manager_set_mbuf wrong: %d %d\n", port, port_socket_id);

	/* Allocate and set up RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd, port_socket_id , NULL, mbuf);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up  TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd, port_socket_id, &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0)
		return retval;

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

    HM_INFO("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);
    HM_INFO("Port %u configure finished\n", port);

	return 0;
}

void hm_manager_port_init(struct worker_manager *wm)
{
    uint16_t portid;
    RTE_ETH_FOREACH_DEV(portid) {
        if (port_init(wm, portid) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n", portid);
    }

}

void hm_manager_port_stat(void)
{
    uint16_t portid;
    RTE_ETH_FOREACH_DEV(portid) {
        struct rte_eth_stats stat_info;
        int stat;
        if (!rte_eth_dev_is_valid_port(portid)) {
            HM_INFO("Error: Invalid port number %i\n", portid);
            return;
        }
        stat = rte_eth_stats_get(portid, &stat_info);
        if (stat == 0) {
            HM_INFO("Port %i stats\n", portid);
            HM_INFO("   In: %" PRIu64 " (%" PRIu64 " bytes)\n"
                "  Out: %"PRIu64" (%"PRIu64 " bytes)\n"
                "  RX-missed: %"PRIu64"\n"
                "  Err: %"PRIu64"\n",
                stat_info.ipackets,
                stat_info.ibytes,
                stat_info.opackets,
                stat_info.obytes,
                stat_info.imissed,
                stat_info.ierrors+stat_info.oerrors
                );
        } else if (stat == -ENOTSUP)
            HM_INFO("Port %i: Operation not supported\n", portid);
        else
            HM_INFO("Port %i: Error fetching statistics\n", portid);
    }

}


struct worker_manager *hm_manager_init(char *config_filename)
{
    uint16_t nb_ports;
    uint16_t port;
    struct rte_eth_dev_info dev_info;
    unsigned int lcore_id;
    unsigned int nb_sockets;
    int retval;
    int n = 0;

    struct worker_manager *wm = malloc(sizeof(struct worker_manager));
    if ( wm == NULL )
        rte_exit(-1, "malloc worker manager failed\n");

    nb_sockets = rte_socket_count();
    nb_ports = rte_eth_dev_count_avail();

    wm->nb_ports = nb_ports;
    wm->nb_sockets = nb_sockets;

    wm->mbuf = malloc( sizeof(struct rte_mempool *) * nb_sockets * nb_ports );
    if ( wm->mbuf == NULL )
        rte_exit(-1, "malloc worker manager mbuf failed\n");

    HM_INFO("malloc mbuf addr=%p, size=%ld\n", wm->mbuf, sizeof(struct rte_mempool *) * nb_sockets * nb_ports );
    memset(wm->mbuf, 0 , sizeof(struct rte_mempool *) * nb_sockets * nb_ports );

    return wm;
}

void hm_manager_start_run(struct worker_manager *wm) {
	rte_eal_mp_remote_launch(hm_worker_run, NULL, SKIP_MAIN);
}

void hm_manager_wait_stop(struct worker_manager *wm){
    int portid;
    rte_eal_mp_wait_lcore();

    hm_manager_port_stat();

    RTE_ETH_FOREACH_DEV(portid) {
        HM_INFO("Closing port %d...", portid);
        int ret = rte_eth_dev_stop(portid);
        if (ret != 0)
            HM_INFO("rte_eth_dev_stop: err=%d, port=%u\n",
                    ret, portid);
        rte_eth_dev_close(portid);
        HM_INFO(" Done\n");
    }
}

void hm_manager_test()
{
    uint16_t nb_ports;
    uint16_t port_id;
    struct rte_eth_dev_info dev_info;
    unsigned int lcore_id;
    unsigned int nb_sockets;
    int retval;
    int n = 0;
    struct rte_eth_conf port_conf = {
        .rxmode = {
            .split_hdr_size = 0,
        },
    };

    nb_sockets = rte_socket_count();
    nb_ports = rte_eth_dev_count_avail();

	HM_INFO("total sockets=%d\n", nb_sockets);
    HM_INFO("total ports=%d\n", nb_ports);
    HM_INFO("main core id=%d\n", rte_get_main_lcore() );

    for (n=0;n<nb_sockets;n++){
        HM_LOG(INFO,"physical socket idx=%d , eal socketid=%d\n" , n , rte_socket_id_by_idx(n) );
    }

    RTE_LCORE_FOREACH(lcore_id) {
		HM_LOG(INFO, "lcore id=%d, socket_id=%d\n", lcore_id, rte_lcore_to_socket_id(lcore_id));
	}

    RTE_ETH_FOREACH_DEV(port_id) {
       	uint16_t nb_rxd = 10240;
    	uint16_t nb_txd = 10240;

        retval = rte_eth_dev_info_get(port_id, &dev_info);
        if (retval != 0) {
            printf("Error during getting device (port %u) info: %s\n",
                    port_id, strerror(-retval));
            return;
        }

        /* Display the port MAC address. */
        struct rte_ether_addr addr;
        retval = rte_eth_macaddr_get(port_id, &addr);
        if (retval != 0)
            return;

	    HM_INFO("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port_id,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);
        HM_INFO("port socket id=%d\n", rte_eth_dev_socket_id(port_id) );

        HM_INFO("max queue: %d %d\n", dev_info.max_rx_queues, dev_info.max_tx_queues);
	    HM_INFO("nb queue: %d %d\n", dev_info.nb_rx_queues, dev_info.nb_tx_queues);
        retval = rte_eth_dev_configure(port_id, dev_info.max_rx_queues, dev_info.max_tx_queues, &port_conf);
        if (retval != 0)
            return;

        retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
	    if (retval != 0)
		    return;
        else
        {
            HM_INFO("adjust rx tx desc ok=%d %d\n", nb_rxd, nb_txd);
        }
    }
}