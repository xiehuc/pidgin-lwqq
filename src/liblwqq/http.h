/**
 * @file   http.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Mon May 21 23:07:29 2012
 *
 * @brief  Linux WebQQ Http API
 *
 *
 */

#ifndef LWQQ_HTTP_H
#define LWQQ_HTTP_H

#include "type.h"
#include <stdio.h>

typedef int (*LwqqAsyncCallback)(LwqqHttpRequest* request, void* data);

struct cookie_list {
    char name[32];
    char value[256];
    struct cookie_list* next;
};
typedef enum {
    LWQQ_FORM_FILE,// use add_file_content instead
    LWQQ_FORM_CONTENT
} LWQQ_FORM;
/**
 * Lwqq Http request struct, this http object worked donw for lwqq,
 * But for other app, it may work bad.
 *
 */
struct _LwqqHttpRequest {
    void *req;
    void *lc;
    void *header;// read and write.
    void *recv_head;
    struct cookie_list* cookie;
    void *form_start;
    void *form_end;

    /**
     * Http code return from server. e.g. 200, 404, this maybe changed
     * after do_request() called.
     */
    long http_code;
    LwqqCallbackCode failcode;
    int retry;

    /* Server response, used when do async request */
    char *location;
    char *response;

    /* Response length, NB: the response may not terminate with '\0' */
    int resp_len;

    /**
     * Send a request to server, method is GET(0) or POST(1), if we make a
     * POST request, we must provide a http body.
     */
    int (*do_request)(LwqqHttpRequest *request, int method, char *body);

    /**
     * Send a request to server asynchronous, method is GET(0) or POST(1),
     * if we make a POST request, we must provide a http body.
     */
    LwqqAsyncEvent* (*do_request_async)(LwqqHttpRequest *request, int method,
                            char *body,LwqqCommand);

    /* Set our http client header */
    void (*set_header)(LwqqHttpRequest *request, const char *name,
                       const char *value);

    /* Set default http header */
    //void (*set_default_header)(struct LwqqHttpRequest *request);

    /**
     * Get header, return a alloca memory, so caller has responsibility
     * free the memory
     */
    const char * (*get_header)(LwqqHttpRequest *request, const char *name);

    /**
     * Get Cookie, return a alloca memory, so caller has responsibility
     * free the memory
     */
    char * (*get_cookie)(LwqqHttpRequest *request, const char *name);

    void (*add_form)(LwqqHttpRequest* request,LWQQ_FORM form,const char* name,const char* content);
    void (*add_file_content)(LwqqHttpRequest* request,const char* name,const char* filename,const void* data,size_t size,const char* extension);
    int (*progress_func)(void* data,size_t now,size_t total);
    void* prog_data;
    time_t last_prog;

} ;

typedef struct {
    struct {
        enum {
            LWQQ_HTTP_PROXY_NOT_SET = -1, //let curl auto set proxy
            LWQQ_HTTP_PROXY_NONE,
            LWQQ_HTTP_PROXY_HTTP,
            LWQQ_HTTP_PROXY_SOCKS4,
            LWQQ_HTTP_PROXY_SOCKS5
        }type;
        char* host;
        int port;
        char* username;
        char* password;
    }proxy;
}LwqqHttpHandle;

void lwqq_http_handle_remove(LwqqHttpHandle* http);
#define lwqq_http_proxy_set(_handle,_type,_host,_port,_username,_password)\
do{\
    LwqqHttpHandle* h = (LwqqHttpHandle*) (_handle);\
    h->proxy.type = _type;\
    h->proxy.host = s_strdup(_host);\
    h->proxy.port = _port;\
    h->proxy.username = s_strdup(_username);\
    h->proxy.password = s_strdup(_password);\
}while(0);

void lwqq_http_proxy_apply(LwqqHttpHandle* handle,LwqqHttpRequest* req);

/**
 * Free Http Request
 *
 * @param request
 */
void lwqq_http_request_free(LwqqHttpRequest *request);

/**
 * Create a new Http request instance
 *
 * @param uri Request service from
 *
 * @return
 */
LwqqHttpRequest *lwqq_http_request_new(const char *uri);

/**
 * Create a default http request object using default http header.
 *
 * @param url Which your want send this request to
 * @param err This parameter can be null, if so, we dont give thing
 *        error information.
 *
 * @return Null if failed, else a new http request object
 */
LwqqHttpRequest *lwqq_http_create_default_request(LwqqClient* lc,const char *url,
        LwqqErrorCode *err);

typedef enum {
    LWQQ_HTTP_TIMEOUT,
    LWQQ_HTTP_NOT_FOLLOW,
    LWQQ_HTTP_SAVE_FILE,
    LWQQ_HTTP_RESET_URL,
    LWQQ_HTTP_VERBOSE,
    LWQQ_HTTP_CANCELABLE,
    LWQQ_HTTP_NOT_SET_COOKIE = 1<<7
}LwqqHttpOption;
void lwqq_http_global_init();
void lwqq_http_global_free();
void lwqq_http_cleanup(LwqqClient*lc);
void lwqq_http_set_option(LwqqHttpRequest* req,LwqqHttpOption opt,...);
void lwqq_http_on_progress(LwqqHttpRequest* req,LWQQ_PROGRESS progress,void* prog_data);
void lwqq_http_cancel(LwqqHttpRequest* req);
char* lwqq_http_escape(LwqqHttpRequest* req,const char* url);


#endif  /* LWQQ_HTTP_H */
