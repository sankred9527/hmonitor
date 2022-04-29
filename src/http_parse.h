#ifndef _HTTP_PARSE_H_
#define _HTTP_PARSE_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <rte_eal.h>
#include <rte_mbuf.h>

bool hook_http_response_fast(char *content, size_t content_length, char *retval, size_t *ret_length);

static inline bool
get_http_host(char *content, size_t content_length, char **host, size_t *host_length, char **url, size_t *url_length)
{
    if( unlikely(content == NULL || content_length == 0 || host_length == NULL) )
        return false;

    //采用整数，加速匹配
    const uint32_t http_get = 0x47455420; // "GET "
    const uint32_t http_host1 = 0x486F7374; // "Host"
    const uint32_t http_host2 = 0x686F7374; // "host"

    uint8_t *p = content;
    int n = 0;
    bool find_host = false;

    uint32_t field = rte_be_to_cpu_32(*(uint32_t*)(content));
    if (field != http_get)
        return false;
    n += 4;

    int url_start = -1;
    int url_end = -1;
    //find the url of "GET" line
    for (; n < content_length; n++) {
        if ( p[n] != ' ' && p[n] != '\n' && p[n] != '\r' ) {
            url_start = n;
            break;
        } else if ( p[n] == '\r' || p[n] == '\n' )
            break;
    }

    if ( unlikely(url_start == -1 ) )
        return false;

    for (; n < content_length; n++ ) {
        if ( p[n] == ' ') {
            url_end = n - 1;
            break;
        } else if ( p[n] == '\r' || p[n] == '\n' )
            break;
    }

    if ( unlikely(url_end == -1 ) )
        return false;

    *url_length = url_end - url_start + 1;
    *url = content + url_start;

    //find the "Host" line
    for (; n < content_length; n++) {
        if ( p[n-1] == '\n' ) {
            field = rte_be_to_cpu_32(*(uint32_t*)(p+n));
            if ( field == http_host1 || field == http_host2 ){
                find_host = true;
                break;
            }
        }
    }

    if ( find_host == false )
        return false;

    /*
        p[n] == "Host", now check  "Host: "
    */
   if (!( (n+5)<=content_length-1 && p[n+4] == ':' && p[n+5] == ' ' ))  {
       //printf("%d %c %c\n", (n+5)<=content_length-1, p[n+4], p[n+5]);
       return false;
   }

    n = n + 6;
    uint8_t *host_start = p + n;
    uint8_t *host_end = NULL;
    // now , p[n] point to xxxx in  "Host: xxxx\r\n"
    while ( n < content_length ) {
        if ( p[n] == '\n') {
            if (p[n-1] == '\r')
                host_end = p + n - 2 ;
            else
                host_end = p + n - 1;
            break;
        }
        n++;
    }

    if ( unlikely(host_end == NULL) ) {
        //printf("host_end is null\n");
        return false;
    }

    /*
        可能是以下格式：
        www.a.com
        www.a.com:80
        需要删除可能存在的 ":"
    */
    for ( p = host_start; p <= host_end; p++ ) {
        if ( *p == ':') {
            host_end = p-1;
            break;
        }
    }

    int len = host_end - host_start + 1;
    *host = host_start;
    *host_length = len;
    return true;
}

bool send_fake_http_response(struct rte_mbuf *m, int tx_port, char *content, size_t content_len);

#endif
