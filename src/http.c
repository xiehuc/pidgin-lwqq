#include <string.h>
#include <zlib.h>
#include <ghttp.h>
#include <plugin.h>
#include <eventloop.h>
#include "smemory.h"
#include "http.h"
#include "logger.h"

#define LWQQ_HTTP_USER_AGENT "User-Agent: Mozilla/5.0 \
(X11; Linux x86_64; rv:10.0) Gecko/20100101 Firefox/10.0"

static int lwqq_http_do_request(LwqqHttpRequest *request, int method, char *body);
static void lwqq_http_set_header(LwqqHttpRequest *request, const char *name,
                                 const char *value);
static void lwqq_http_set_default_header(LwqqHttpRequest *request);
static char *lwqq_http_get_header(LwqqHttpRequest *request, const char *name);
static char *lwqq_http_get_cookie(LwqqHttpRequest *request, const char *name);

/* For async request */
static int lwqq_http_do_request_async(struct LwqqHttpRequest *request, int method,
                                      char *body, LwqqAsyncCallback callback,
                                      void *data);

static void lwqq_http_set_header(LwqqHttpRequest *request, const char *name,
                                const char *value)
{
    if (!request->req || !name || !value)
        return ;

    ghttp_set_header(request->req, name, value);
}

static void lwqq_http_set_default_header(LwqqHttpRequest *request)
{
    lwqq_http_set_header(request, "User-Agent", LWQQ_HTTP_USER_AGENT);
    lwqq_http_set_header(request, "Accept", "text/html, application/xml;q=0.9, "
                         "application/xhtml+xml, image/png, image/jpeg, "
                         "image/gif, image/x-xbitmap, */*;q=0.1");
    lwqq_http_set_header(request, "Accept-Language", "en-US,zh-CN,zh;q=0.9,en;q=0.8");
    lwqq_http_set_header(request, "Accept-Charset", "GBK, utf-8, utf-16, *;q=0.1");
    lwqq_http_set_header(request, "Accept-Encoding", "deflate, gzip, x-gzip, "
                         "identity, *;q=0");
    lwqq_http_set_header(request, "Connection", "Keep-Alive");
}

static char *lwqq_http_get_header(LwqqHttpRequest *request, const char *name)
{
    if (!name) {
        lwqq_log(LOG_ERROR, "Invalid parameter\n");
        return NULL; 
    }

    const char *h = ghttp_get_header(request->req, name);
    if (!h) {
        lwqq_log(LOG_WARNING, "Cant get http header: %s\n", name);
        return NULL;
    }

    return s_strdup(h);
}

static char *lwqq_http_get_cookie(LwqqHttpRequest *request, const char *name)
{
    if (!name) {
        lwqq_log(LOG_ERROR, "Invalid parameter\n");
        return NULL; 
    }
    
    char *cookie = ghttp_get_cookie(request->req, name);
    if (!cookie) {
        lwqq_log(LOG_WARNING, "No cookie: %s\n", name);
        return NULL;
    }

    lwqq_log(LOG_DEBUG, "Parse Cookie: %s=%s\n", name, cookie);
    return cookie;
}

/** 
 * Free Http Request
 * 
 * @param request 
 */
void lwqq_http_request_free(LwqqHttpRequest *request)
{
    if (!request)
        return ;
    
    if (request) {
        s_free(request->response);
        ghttp_request_destroy(request->req);
        s_free(request);
    }
}

/** 
 * Create a new Http request instance
 *
 * @param uri Request service from
 * 
 * @return 
 */
LwqqHttpRequest *lwqq_http_request_new(const char *uri)
{
    if (!uri) {
        return NULL;
    }

    LwqqHttpRequest *request;
    request = s_malloc0(sizeof(*request));
    
    request->req = ghttp_request_new();
    if (!request->req) {
        /* Seem like request->req must be non null. FIXME */
        goto failed;
    }
    if (ghttp_set_uri(request->req, (char *)uri) == -1) {
        lwqq_log(LOG_WARNING, "Invalid uri: %s\n", uri);
        goto failed;
    }

    request->do_request = lwqq_http_do_request;
    request->do_request_async = lwqq_http_do_request_async;
    request->set_header = lwqq_http_set_header;
    request->set_default_header = lwqq_http_set_default_header;
    request->get_header = lwqq_http_get_header;
    request->get_cookie = lwqq_http_get_cookie;
    return request;

failed:
    if (request) {
        lwqq_http_request_free(request);
    }
    return NULL;
}

static char *unzlib(const char *source, int len, int *total, int isgzip)
{
#define CHUNK 16 * 1024
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char out[CHUNK];
    int totalsize = 0;
    char *dest = NULL;

    if (!source || len <= 0 || !total)
        return NULL;

/* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    if(isgzip) {
        /**
         * 47 enable zlib and gzip decoding with automatic header detection,
         * So if the format of compress data is gzip, we need passed it to
         * inflateInit2
         */
        ret = inflateInit2(&strm, 47);
    } else {
        ret = inflateInit(&strm);
    }

    if (ret != Z_OK) {
        lwqq_log(LOG_ERROR, "Init zlib error\n");
        return NULL;
    }

    strm.avail_in = len;
    strm.next_in = (Bytef *)source;

    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        switch (ret) {
        case Z_STREAM_END:
            break;
        case Z_BUF_ERROR:
            lwqq_log(LOG_ERROR, "Unzlib error\n");
            break;
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR; /* and fall through */
            break;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_STREAM_ERROR:
            lwqq_log(LOG_ERROR, "Ungzip stream error!", strm.msg);
            inflateEnd(&strm);
            goto failed;
        }
        have = CHUNK - strm.avail_out;
        totalsize += have;
        dest = s_realloc(dest, totalsize);
        memcpy(dest + totalsize - have, out, have);
    } while (strm.avail_out == 0);

/* clean up and return */
    (void)inflateEnd(&strm);
    if (ret != Z_STREAM_END) {
        goto failed;
    }
    *total = totalsize;
    return dest;

failed:
    if (dest) {
        s_free(dest);
    }
    lwqq_log(LOG_ERROR, "Unzip error\n");
    return NULL;
}

static char *ungzip(const char *source, int len, int *total)
{
    return unzlib(source, len, total, 1);
}

static int lwqq_http_do_request(LwqqHttpRequest *request, int method, char *body)
{
    if (!request->req)
        return -1;

    ghttp_status status;
    char *buf;
    int have_read_bytes = 0;
    ghttp_type m;
    char **resp = &request->response;

    /* Clear off last response */
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }

    /* Set http method */
    if (method == 0) {
        m = ghttp_type_get;
    } else if (method == 1) {
        m = ghttp_type_post;
    } else {
        lwqq_log(LOG_WARNING, "Wrong http method\n");
        goto failed;
    }
    if (ghttp_set_type(request->req, m) == -1) {
        lwqq_log(LOG_WARNING, "Set request type error\n");
        goto failed;
    }

    /* For POST method, set http body */
    if (m == ghttp_type_post && body) {
        ghttp_set_body(request->req, body, strlen(body));
    }
    
    if (ghttp_prepare(request->req)) {
        goto failed;
    }

    for ( ; ; ) {
        int len = 0;
        status = ghttp_process(request->req);
        if(status == ghttp_error) {
            lwqq_log(LOG_ERROR, "Http request failed: %s\n",
                     ghttp_get_error(request->req));
            goto failed;
        }
        /* NOTE: buf may NULL, notice it */
        buf = ghttp_get_body(request->req);
        if (buf) {
            len = ghttp_get_body_len(request->req);
            *resp = s_realloc(*resp, have_read_bytes + len);
            memcpy(*resp + have_read_bytes, buf, len);
            have_read_bytes += len;
        }
        if(status == ghttp_done) {
            /* NOTE: Ok, done */
            break;
        }
    }

    /* NB: *response may null */
    if (*resp == NULL) {
        goto failed;
    }

    /* Uncompress data here if we have a Content-Encoding header */
    char *enc_type = NULL;
    enc_type = lwqq_http_get_header(request, "Content-Encoding");
    if (enc_type && strstr(enc_type, "gzip")) {
        char *outdata;
        int total = 0;
        
        outdata = ungzip(*resp, have_read_bytes, &total);
        if (!outdata) {
            s_free(enc_type);
            goto failed;
        }

        s_free(*resp);
        /* Update response data to uncompress data */
        *resp = s_strdup(outdata);
        s_free(outdata);
        have_read_bytes = total;
    }
    s_free(enc_type);

    /* OK, done */
    if ((*resp)[have_read_bytes -1] != '\0') {
        /* Realloc a byte, cause *resp hasn't end with char '\0' */
        *resp = s_realloc(*resp, have_read_bytes + 1);
        (*resp)[have_read_bytes] = '\0';
    }
    request->http_code = ghttp_status_code(request->req);
    return 0;

failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return -1;
}

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
                                                  LwqqErrorCode *err)
{
    LwqqHttpRequest *req;
    
    if (!url) {
        if (err)
            *err = LWQQ_EC_ERROR;
        return NULL;
    }

    req = lwqq_http_request_new(url);
    if (!req) {
        lwqq_log(LOG_ERROR, "Create request object for url: %s failed\n", url);
        if (err)
            *err = LWQQ_EC_ERROR;
        return NULL;
    }

    req->set_default_header(req);
    lwqq_log(LOG_DEBUG, "Create request object for url: %s sucessfully\n", url);
    return req;
}

/************************************************************************/
/* Those Code for async API */
typedef struct AsyncWatchData
{
    LwqqHttpRequest *request;
    LwqqAsyncCallback callback;
    void *data;
    int handle;
} AsyncWatchData;

static pthread_t lwqq_async_tid;
static pthread_cond_t lwqq_async_cond = PTHREAD_COND_INITIALIZER;
static int lwqq_async_running = -1;

static void ev_io_come(void* data,int sock,PurpleInputCondition cond)
{
    AsyncWatchData *d = (AsyncWatchData *)data;
    LwqqErrorCode ec;
    char *buf;
    LwqqHttpRequest *lhr = d->request;
    ghttp_request *req = lhr->req;


    int status = ghttp_process(req);
    if (status == ghttp_not_done)
        return;
    if (status == ghttp_error) {
        ec = LWQQ_EC_ERROR;
        goto done;
    }
    
    /* NOTE: buf may NULL, notice it */
    buf = ghttp_get_body(req);
    if (buf) {
        int len;
        len = ghttp_get_body_len(req);
        lhr->response = s_realloc(lhr->response, lhr->resp_len + len);
        memcpy(lhr->response + lhr->resp_len, buf, len);
        lhr->resp_len += len;
    }
    if (status == ghttp_done) {
        ec = LWQQ_EC_OK;
        goto done;
    }

    /* Go on */
    return ;
    
done:
    if (ec == LWQQ_EC_OK && lhr->response) {
        /* Uncompress data here if we have a Content-Encoding header */
        char *enc_type = NULL;
        enc_type = lwqq_http_get_header(lhr, "Content-Encoding");
        if (enc_type && strstr(enc_type, "gzip")) {
            char *outdata;
            int total = 0;
        
            outdata = ungzip(lhr->response, lhr->resp_len, &total);
            if (outdata) {
                s_free(lhr->response);
                /* Update response data to uncompress data */
                lhr->response = s_strdup(outdata);
                s_free(outdata);
                lhr->resp_len = total;
            }
        }
        s_free(enc_type);

        /* OK, done */
        if (lhr->response[lhr->resp_len -1] != '\0') {
            /* Realloc a byte, cause lhr->response hasn't end with char '\0' */
            lhr->response = s_realloc(lhr->response, lhr->resp_len + 1);
            lhr->response[lhr->resp_len] = '\0';
        }
    }

    /* Callback */
    if(d->callback)
        d->callback(ec, lhr->response, d->data);

    /* OK, exit this request */
    purple_input_remove(d->handle);
    lwqq_http_request_free(d->request);
    s_free(d);
}

static int lwqq_http_do_request_async(struct LwqqHttpRequest *request, int method,
                                      char *body, LwqqAsyncCallback callback,
                                      void *data)
{
    ghttp_type m;
    int status;
    
    /* Set http method */
    if (method == 0) {
        m = ghttp_type_get;
    } else if (method == 1) {
        m = ghttp_type_post;
    } else {
        lwqq_log(LOG_WARNING, "Wrong http method\n");
        lwqq_http_request_free(request);
        return -1;
    }
    if (ghttp_set_type(request->req, m) == -1) {
        lwqq_log(LOG_WARNING, "Set request type error\n");
        lwqq_http_request_free(request);
        return -1;
    }

    /* For POST method, set http body */
    if (m == ghttp_type_post && body) {
        ghttp_set_body(request->req, body, strlen(body));
    }

    ghttp_set_sync(request->req, ghttp_async);
    if (ghttp_prepare(request->req)) {
        lwqq_http_request_free(request);
        return -1;
    }
    
    status = ghttp_process(request->req);
    if (status != ghttp_not_done){
        lwqq_log(LOG_ERROR, "BUG!!!async error\n");
        lwqq_http_request_free(request);
        return -1;
    }

    
    ghttp_request* req = (ghttp_request*)request->req;
    
    AsyncWatchData *d = s_malloc(sizeof(AsyncWatchData));
    d->request = request;
    d->callback = callback;
    d->data = data;

    d->handle = purple_input_add(ghttp_get_socket(req),PURPLE_INPUT_READ,ev_io_come,d);

    /*if (lwqq_async_running == -1) {
        lwqq_async_running = 1;
        pthread_create(&lwqq_async_tid, NULL, lwqq_async_thread, NULL);
    } else if(lwqq_async_running == 0) {
        pthread_cond_signal(&lwqq_async_cond);
    }*/
    
    return 0;
}

#if 0
int main(int argc, char *argv[])
{
    if (argc != 2)
        return -1;
    
    char *uri = argv[1];
    LwqqHttpRequest *req = lwqq_http_request_new(uri);
    if (req) {
        int ret = 0;
        ret = req->do_request(req, 0, NULL);
        ret = req->do_request(req, 0, NULL);
        ret = req->do_request(req, 0, NULL);
        ret = req->do_request(req, 0, NULL);
        if (ret == 0) {
            lwqq_log(LOG_NOTICE, "Http response code: %d\n", req->http_code);
            lwqq_log("LOG_NOTICE, Http response buf: %s\n", req->response);
        }
        lwqq_http_request_free(req);
    }
        
    return 0;
}
#endif
