
#include "all.h"
#include "manager.h"

struct rte_mempool *hm_worker_get_mbuf(struct worker_manager *wm, uint16_t port_id, uint16_t physical_socket_id) {
    struct rte_mempool **ret = &(wm->mbuf[0]);

    uint32_t offset = port_id*wm->nb_ports + physical_socket_id;
    if ( offset  >=  wm->nb_ports*wm->nb_sockets ) {
        return NULL;
    }
    return ret[offset];
}

struct rte_mempool *hm_worker_set_mbuf(struct worker_manager *wm, struct rte_mempool *mbuf, uint16_t port_id, uint16_t physical_socket_id) {
    uint32_t offset = port_id*wm->nb_ports + physical_socket_id;

    if ( offset >= wm->nb_ports*wm->nb_sockets ) {
        return NULL;
    }

    struct rte_mempool **ret = &(wm->mbuf[offset]);
    *ret = mbuf;
    return mbuf;
}

struct worker_manager *hm_worker_init(char *config_filename)
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

    HM_INFO("malloc mbuf addr=%p, size=%d\n", wm->mbuf, sizeof(struct rte_mempool *) * nb_sockets * nb_ports );
    memset(wm->mbuf, 0 , sizeof(struct rte_mempool *) * nb_sockets * nb_ports );
    
    return wm;
}

void hm_worker_test()
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
		HM_LOG(INFO, "lcore id=%d, cpu id=%d socket_id=%d\n", lcore_id, rte_lcore_to_cpu_id(lcore_id),  rte_lcore_to_socket_id(lcore_id)  );        
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