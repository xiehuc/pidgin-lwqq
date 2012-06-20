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

/**
 * Lwqq Http request struct, this http object worked donw for lwqq,
 * But for other app, it may work bad.
 * 
 */
typedef struct LwqqHttpRequest {
    void *req;

    /**
     * Http code return from server. e.g. 200, 404, this maybe changed
     * after do_request() called.
     */
    int http_code;
    
    /* Server response, this maybe changed after do_request() called */
    char *response;

    /**
     * Send a request to server, method is GET(0) or POST(1), if we make a
     * POST request, we must provide a http body.
     */
    int (*do_request)(struct LwqqHttpRequest *request, int method, char *body);

    /* Set our http client header */
    void (*set_header)(struct LwqqHttpRequest *request, const char *name,
                       const char *value);

    /* Set default http header */
    void (*set_default_header)(struct LwqqHttpRequest *request);

    /**
     * Get header, return a alloca memory, so caller has responsibility
     * free the memory
     */
    char * (*get_header)(struct LwqqHttpRequest *request, const char *name);

    /**
     * Get Cookie, return a alloca memory, so caller has responsibility
     * free the memory
     */
    char * (*get_cookie)(struct LwqqHttpRequest *request, const char *name);
    
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

#endif  /* LWQQ_HTTP_H */
