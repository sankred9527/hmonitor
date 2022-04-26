#ifndef _HTTP_PARSE_H_
#define _HTTP_PARSE_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <rte_eal.h>
#include <rte_mbuf.h>

bool hook_http_response_fast(char *content, size_t content_length, char *retval, size_t *ret_length);

inline bool 
get_http_host(char *content, size_t content_length, char **host, size_t *host_length, char **url, size_t *url_length);

bool send_fake_http_response(struct rte_mbuf *m, int tx_port, char *content, size_t content_len);

#endif
