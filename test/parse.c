#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../lib/llhttp/llhttp.h"

typedef struct {
    char *host;
    char *port;
    char *url;
    char *referer;
    char *data; // wrap to original parser->(void *)data;
} what_we_want_t;

static inline
what_we_want_t *
we_want(void *parser_data) {
    return (what_we_want_t *)parser_data;
}

int
my_url_callback(llhttp_t *parser, const char *at, size_t length) {
    char *url = calloc(length, sizeof(char));
    memcpy(url, at, length);
    //printf("url: %s\n", url);
    we_want(parser->data)->url = url;
    return HPE_OK;
}

int
my_header_field_callback(llhttp_t *parser, const char *at, size_t length) {
    char *field = calloc(length, sizeof(char));
    memcpy(field, at, length);
    //printf("field: %s ", field);
    we_want(parser->data)->data = field;
    return HPE_OK;
}

int
my_header_value_callback(llhttp_t *parser, const char *at, size_t length) {
    char *value = calloc(length, sizeof(char));
    memcpy(value, at, length);
    //printf("value: %s\n", value);
    // stricmp ?
    if(we_want(parser->data)->data != NULL) {
        if(strcmp(we_want(parser->data)->data, "Referer") == 0) {
            we_want(parser->data)->referer = value;
        } else if(strcmp(we_want(parser->data)->data, "Host") == 0) {
            char *token = strtok(value, ":");
            we_want(parser->data)->host = token;
            token = strtok(NULL, ":");
            if(token != NULL) {
                we_want(parser->data)->port = token;
            }
        } else {
            free(we_want(parser->data)->data);
            we_want(parser->data)->data = NULL;
        }
    }
    return HPE_OK;
}

int
my_headers_complete_callback(llhttp_t *parser) {
    return HPE_OK;
}


int main() {
    char *content = "GET /assets/github-f660eb690a5f.css HTTP/1.1\r\n\
Accept: text/css,*/*;q=0.1\r\n\
Accept-Encoding: gzip, deflate, br\r\n\
Accept-Language: en-US,en;q=0.8,zh-CN;q=0.5,zh;q=0.3\r\n\
Cache-Control: no-cache\r\n\
Connection: keep-alive\r\n\
Host: github.githubassets.com\r\n\
Origin: https://github.com\r\n\
Pragma: no-cache\r\n\
Referer: https://github.com/nodejs/llhttp/blob/main/test/request/sample.md\r\n\
Sec-Fetch-Dest: style\r\n\
Sec-Fetch-Mode: cors\r\n\
Sec-Fetch-Site: cross-site\r\n\
TE: trailers\r\n\
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:99.0) Gecko/20100101 Firefox/99.0\r\n\
\r\n";
    size_t content_length = strlen(content);
    printf("parse content_length: %ld\n", content_length);

    llhttp_settings_t settings;
    llhttp_settings_init(&settings);
    settings.on_url = my_url_callback;
    settings.on_header_field = my_header_field_callback;
    settings.on_header_value = my_header_value_callback;
    settings.on_headers_complete = my_headers_complete_callback;

    llhttp_t parser;
    llhttp_init(&parser, HTTP_REQUEST, &settings);
    parser.data = calloc(1, sizeof(what_we_want_t));
    enum llhttp_errno err = llhttp_execute(&parser, content, content_length);
    if (err != HPE_OK) {
        printf("parse error: %s %s\n", llhttp_errno_name(err), parser.reason);
        return -1;
    } else {
        printf("parse ok\n");
        what_we_want_t *what_we_want = we_want(parser.data);
        printf("host: %s port: %s url: %s referer: %s\n", what_we_want->host, what_we_want->port, what_we_want->url, what_we_want->referer);
        return 0;
    }
}