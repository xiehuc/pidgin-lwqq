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

struct LwqqHttpRequest;
typedef int (*LwqqAsyncCallback)(struct LwqqHttpRequest* request, void* data);

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
typedef struct LwqqHttpRequest {
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

    /* Server response, used when do async request */
    char *response;

    /* Response length, NB: the response may not terminate with '\0' */
    int resp_len;
    int resp_realloc;

    /**
     * Send a request to server, method is GET(0) or POST(1), if we make a
     * POST request, we must provide a http body.
     */
    int (*do_request)(struct LwqqHttpRequest *request, int method, char *body);

    /**
     * Send a request to server asynchronous, method is GET(0) or POST(1),
     * if we make a POST request, we must provide a http body.
     */
    LwqqAsyncEvent* (*do_request_async)(struct LwqqHttpRequest *request, int method,
                            char *body, LwqqAsyncCallback callback, void *data);

    /* Set our http client header */
    void (*set_header)(struct LwqqHttpRequest *request, const char *name,
                       const char *value);

    /* Set default http header */
    void (*set_default_header)(struct LwqqHttpRequest *request);

    /**
     * Get header, return a alloca memory, so caller has responsibility
     * free the memory
     */
    const char * (*get_header)(struct LwqqHttpRequest *request, const char *name);

    /**
     * Get Cookie, return a alloca memory, so caller has responsibility
     * free the memory
     */
    char * (*get_cookie)(struct LwqqHttpRequest *request, const char *name);

    void (*add_form)(struct LwqqHttpRequest* request,LWQQ_FORM form,const char* name,const char* content);
    void (*add_file_content)(struct LwqqHttpRequest* request,const char* name,const char* filename,const void* data,size_t size,const char* extension);

} LwqqHttpRequest;

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
LwqqHttpRequest *lwqq_http_create_default_request(const char *url,
        LwqqErrorCode *err);

void lwqq_http_set_async(LwqqHttpRequest* request);
void lwqq_http_global_init();
void lwqq_http_global_free();
void lwqq_http_set_timeout(LwqqHttpRequest* req,int time_out);
void lwqq_http_save_file(LwqqHttpRequest* req,const char* filepath);


#endif  /* LWQQ_HTTP_H */
