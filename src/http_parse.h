#ifndef _HTTP_PARSE_H_
#define _HTTP_PARSE_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <rte_eal.h>
#include <rte_mbuf.h>


bool send_fake_http_response(struct rte_mbuf *m, int tx_port, char *content, size_t content_len);

#endif
