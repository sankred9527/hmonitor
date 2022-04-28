#include "http_parse.h"


bool hook_http_response_fast(char *content, size_t content_length, char *retval, size_t *ret_length)
{
    char *url;
    size_t url_length;
    char *host;
    size_t host_len;
    if ( !get_http_host(content, content_length, &host, &host_len, &url, &url_length) )
        return false;

    bool need_hook = false;
    if (!need_hook)
        return false;

    const char *response_format = "HTTP/1.1 301 Moved Permanently\r\nContent-Length: 0\r\nLocation: %s\r\n\r\n";
    *ret_length = snprintf(retval, *ret_length, response_format, "http://www.baidu.com" );

    return true;
}

bool send_fake_http_response(struct rte_mbuf *m, int tx_port, char *content, size_t content_len)
{
    return true;
}
