#include <string.h>
#include <zlib.h>
#include <ev.h>
#include <stdio.h>
#include <curl/curl.h>
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

typedef struct _LWQQ_HTTP_HANDLE{
    CURLM* multi;
    struct ev_loop* loop;
    int still_running;
    ev_timer timer_event;
    pthread_t tid;
    pthread_cond_t cond ;
    int async_running ;
}_LWQQ_HTTP_HANDLE;

typedef struct S_ITEM {
    /**@brief 全局事件循环*/
    curl_socket_t sockfd;
    int action;
    CURL *easy;
    /**@brief ev重用标志,一直为1 */
    int evset;
    ev_io ev;
}S_ITEM;
typedef struct D_ITEM{
    LwqqAsyncCallback callback;
    void* data;
    LwqqHttpRequest* req;
    ev_timer delay;
}D_ITEM;
/* For async request */
static int lwqq_http_do_request_async(struct LwqqHttpRequest *request, int method,
        char *body, LwqqAsyncCallback callback,
                                      void *data);

#define slist_free_all(list) \
while(list!=NULL){ \
    void *ptr = list; \
    list = list->next; \
    s_free(ptr); \
}
#define slist_append(list,node) \
(node->next = list,node)
static void lwqq_http_set_header(LwqqHttpRequest *request, const char *name,
                                const char *value)
{
    if (!request->req || !name || !value)
        return ;

    size_t name_len = strlen(name);
    size_t value_len = strlen(value);
    char* opt = s_malloc(name_len+value_len+3);

    strcpy(opt,name);
    opt[name_len] = ':';
    //need a blank space
    opt[name_len+1] = ' ';
    strcpy(opt+name_len+2,value);

    request->header = curl_slist_append((struct curl_slist*)request->header,opt);
    //curl_easy_setopt(request->req,CURLOPT_HTTPHEADER,request->header);

    s_free(opt);
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

    const char *h = NULL;
    struct curl_slist* list = request->recv_head;
    while(list!=NULL){
        if(strncmp(name,list->data,strlen(name))==0){
            h = list->data+strlen(name)+2;
            break;
        }
        list = list->next;
    }
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
    
    char* cookie = NULL;
    struct cookie_list* list = request->cookie;
    while(list!=NULL){
        if(strcmp(list->name,name)==0){
            cookie = list->value;
            break;
        }
        list = list->next;
    }
    printf("cookie:%s\n",cookie);
    if (!cookie) {
        lwqq_log(LOG_WARNING, "No cookie: %s\n", name);
        return NULL;
    }

    lwqq_log(LOG_DEBUG, "Parse Cookie: %s=%s\n", name, cookie);
    return s_strdup(cookie);
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
        curl_slist_free_all(request->header);
        curl_slist_free_all(request->recv_head);
        slist_free_all(request->cookie);
        curl_easy_cleanup(request->req);
        s_free(request);
    }
}

static size_t write_header( void *ptr, size_t size, size_t nmemb, void *userdata)
{
    char* str = (char*)ptr;
    LwqqHttpRequest* request = (LwqqHttpRequest*) userdata;
    request->recv_head = curl_slist_append(request->recv_head,(char*)ptr);
    //read cookie from header;
    if(strncmp(str,"Set-Cookie",strlen("Set-Cookie"))==0){
        struct cookie_list * node = s_malloc0(sizeof(*node));
        sscanf(str,"Set-Cookie: %[^=]=%[^;];",node->name,node->value);
        request->cookie = slist_append(request->cookie,node);
    }
    return size*nmemb;
}
static size_t write_content(void* ptr,size_t size,size_t nmemb,void* userdata)
{
    LwqqHttpRequest* request = (LwqqHttpRequest*) userdata;
    double s;
    int resp_len = request->resp_len;
    curl_easy_getinfo(request->req,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&s);
    if(s==-1.0){
        request->response = s_realloc(request->response,resp_len+size*nmemb+5);
    }
    if(request->response==NULL){
        //add one to ensure the last \0;
        request->response = s_malloc0((size_t)s+5);
        resp_len = 0;
    }
    memcpy(request->response+resp_len,ptr,size*nmemb);
    request->resp_len+=size*nmemb;
    return size*nmemb;
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
    
    request->req = curl_easy_init();
    if (!request->req) {
        /* Seem like request->req must be non null. FIXME */
        goto failed;
    }
    if(curl_easy_setopt(request->req,CURLOPT_URL,uri)!=0){
        lwqq_log(LOG_WARNING, "Invalid uri: %s\n", uri);
        goto failed;
    }
    
    curl_easy_setopt(request->req,CURLOPT_HEADERFUNCTION,write_header);
    curl_easy_setopt(request->req,CURLOPT_HEADERDATA,request);
    curl_easy_setopt(request->req,CURLOPT_WRITEFUNCTION,write_content);
    curl_easy_setopt(request->req,CURLOPT_WRITEDATA,request);
    curl_easy_setopt(request->req,CURLOPT_NOSIGNAL,1);
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

    int have_read_bytes = 0;
    char **resp = &request->response;

    /* Clear off last response */
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }

    curl_easy_setopt(request->req,CURLOPT_HTTPHEADER,request->header);
    /* Set http method */
    if (method==0){
    }else if (method == 1 && body) {
        curl_easy_setopt(request->req,CURLOPT_POST,1);
        curl_easy_setopt(request->req,CURLOPT_COPYPOSTFIELDS,body);
    } else {
        lwqq_log(LOG_WARNING, "Wrong http method\n");
        goto failed;
    }

    curl_easy_perform(request->req);
    have_read_bytes = request->resp_len;
    curl_easy_getinfo(request->req,CURLINFO_RESPONSE_CODE,&request->http_code);

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
        // Realloc a byte, cause *resp hasn't end with char '\0' 
        *resp = s_realloc(*resp, have_read_bytes + 1);
        (*resp)[have_read_bytes] = '\0';
    }
    return 0;

failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return 0;
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


static void *lwqq_async_thread(void* data)
{
    GLOBAL *g = data;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    while (1) {
        g->async_running = 1;
        ev_run(EV_DEFAULT, 0);
        g->async_running = 0;
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&g->cond, &mutex);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}


static void async_complete(D_ITEM* conn)
{
    LwqqHttpRequest* request = conn->req;
    int have_read_bytes;
    char** resp = &request->response;

    have_read_bytes = request->resp_len;
    curl_easy_getinfo(request->req,CURLINFO_RESPONSE_CODE,&request->http_code);

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
        // Realloc a byte, cause *resp hasn't end with char '\0' 
        *resp = s_realloc(*resp, have_read_bytes + 1);
        (*resp)[have_read_bytes] = '\0';
    }

    conn->callback(request,conn->data);
    return ;

failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return ;
}

static void check_multi_info(GLOBAL *g)
{
    CURLMsg *msg;
    int msgs_left;
    D_ITEM *conn;
    CURL *easy;

    while ((msg = curl_multi_info_read(g->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            easy = msg->easy_handle;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);

            curl_multi_remove_handle(g->multi, easy);
            curl_easy_cleanup(easy);

            //执行完成时候的回调
            async_complete(conn);
            conn->callback(conn->req,conn->data);
            s_free(conn);
        }
    }
}
static void timer_cb(EV_P_ struct ev_timer *w, int revents)
{
    //这个表示有超时任务出现.
    GLOBAL* g = w->data;

    curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
    check_multi_info(g);
}
static int multi_timer_cb(CURLM *multi, long timeout_ms, void *userp)
{
    GLOBAL* g = userp;
    ev_timer_stop(EV_DEFAULT, &g->timer_event);
    printf("timer_cb:%ld\n",timeout_ms);
    if (timeout_ms > 0) {
        double t = timeout_ms / 1000.0;
        printf("time ms:%lf",t);
        ev_timer_init(&g->timer_event,timer_cb,t,0.);
        ev_timer_start(EV_DEFAULT, &g->timer_event);
    } else if(timeout_ms==0)
        timer_cb(EV_DEFAULT, &g->timer_event, 0);
    else{}
    return 0;
}
static void event_cb(EV_P_ struct ev_io *w, int revents)
{
    GLOBAL* g = w->data;

    int action = (revents&EV_READ?CURL_POLL_IN:0)|
                 (revents&EV_WRITE?CURL_POLL_OUT:0);
    curl_multi_socket_action(g->multi, w->fd, action, &g->still_running);
    check_multi_info(g);
    if ( g->still_running <= 0 ) {
        ev_timer_stop(EV_DEFAULT, &g->timer_event);
    }
}
static void setsock(S_ITEM*f, curl_socket_t s, CURL*e, int act,GLOBAL* g)
{
    int kind = (act&CURL_POLL_IN?EV_READ:0)|(act&CURL_POLL_OUT?EV_WRITE:0);

    f->sockfd = s;
    f->action = act;
    f->easy = e;
    if ( f->evset )
        ev_io_stop(EV_DEFAULT, &f->ev);
    ev_io_init(&f->ev, event_cb, f->sockfd, kind);
    f->ev.data = g;
    f->evset=1;
    ev_io_start(EV_DEFAULT, &f->ev);
}
static int sock_cb(CURL* e,curl_socket_t s,int what,void* cbp,void* sockp)
{
    S_ITEM *si = (S_ITEM*)sockp;
    GLOBAL* g = cbp;

    if(what == CURL_POLL_REMOVE) {
        //清除socket关联对象
        if ( si ) {
            if ( si->evset )
                ev_io_stop(EV_DEFAULT, &si->ev);
            s_free(si);
        }
    } else {
        if(si == NULL) {
            //关联socket;
            si = s_malloc0(sizeof(*si));
            setsock(si,s,e,what,g);
            curl_multi_assign(g->multi, s, si);
        } else {
            //重新关联socket;
            setsock(si,s,e,what,g);
        }
    }
    return 0;
}
static int delay_add_handle(EV_P_ ev_timer* w,int e)
{
    D_ITEM* di = w->data;
    CURLMcode rc = curl_multi_add_handle(global.multi,di->req->req);
    if(rc != CURLM_OK){
        puts(curl_multi_strerror(rc));
    }
    ev_timer_stop(EV_DEFAULT,w);
    return 0;
}
static int lwqq_http_do_request_async(struct LwqqHttpRequest *request, int method,
                                      char *body, LwqqAsyncCallback callback,
                                      void *data)
{
    if (!request->req)
        return -1;

    char **resp = &request->response;

    /* Clear off last response */
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }

    curl_easy_setopt(request->req,CURLOPT_HTTPHEADER,request->header);
    /* Set http method */
    if (method==0){
    }else if (method == 1 && body) {
        curl_easy_setopt(request->req,CURLOPT_POST,1);
        curl_easy_setopt(request->req,CURLOPT_COPYPOSTFIELDS,body);
    } else {
        lwqq_log(LOG_WARNING, "Wrong http method\n");
        goto failed;
    }

    if(global.multi == NULL){
        lwqq_http_global_init();
    }
    D_ITEM* di = s_malloc0(sizeof(*di));
    di->callback = callback;
    di->data = data;
    di->req = request;
    curl_easy_setopt(request->req,CURLOPT_PRIVATE,di);
    di->delay.data = di;
    ev_timer_init(&di->delay,delay_add_handle,0.1,0);
    ev_timer_start(EV_DEFAULT,&di->delay);
    if(global.async_running==0)
        pthread_cond_signal(&global.cond);
    return 0;

failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return 0;
}
void lwqq_http_global_init()
{
    global.multi = curl_multi_init();
    CURLM* multi = global.multi;
    curl_multi_setopt(multi,CURLMOPT_SOCKETFUNCTION,sock_cb);
    curl_multi_setopt(multi,CURLMOPT_SOCKETDATA,&global);
    curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
    curl_multi_setopt(multi, CURLMOPT_TIMERDATA, &global);
    pthread_create(&global.tid,NULL,lwqq_async_thread,&global);
    global.async_running = 1;
}
void lwqq_http_global_free()
{
    pthread_cancel(global.tid);

}

