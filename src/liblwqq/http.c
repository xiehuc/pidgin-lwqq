#include <string.h>
#include <zlib.h>
#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <assert.h>
#include "async.h"
#include "smemory.h"
#include "http.h"
#include "logger.h"
#include "queue.h"

#define LWQQ_HTTP_USER_AGENT "Mozilla/5.0 (X11; Linux x86_64; rv:10.0) Gecko/20100101 Firefox/10.0"
//#define LWQQ_HTTP_USER_AGENT "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.11 (KHTML, like Gecko) Chrome/23.0.1271.91 Safari/537.11"

#define LWQQ_RETRY_VALUE 3
static int lwqq_http_do_request(LwqqHttpRequest *request, int method, char *body);
static void lwqq_http_set_header(LwqqHttpRequest *request, const char *name,
                                 const char *value);
static const char *lwqq_http_get_header(LwqqHttpRequest *request, const char *name);
static char *lwqq_http_get_cookie(LwqqHttpRequest *request, const char *name);
static void lwqq_http_add_form(LwqqHttpRequest* request,LWQQ_FORM form,
        const char* name,const char* value);
static void lwqq_http_add_file_content(LwqqHttpRequest* request,const char* name,
        const char* filename,const void* data,size_t size,const char* extension);
static LwqqAsyncEvent* lwqq_http_do_request_async(LwqqHttpRequest *request, int method,
        char *body,LwqqCommand);

typedef struct GLOBAL {
    CURLM* multi;
    CURLSH* share;
    pthread_mutex_t share_lock[2];
    int still_running;
    LwqqAsyncTimer timer_event;
    LIST_HEAD(,D_ITEM) conn_link;
    int pipe_fd[2];
    LwqqAsyncIo add_listener;
    LIST_HEAD(,D_ITEM) add_link;
}GLOBAL;
static GLOBAL global = {0};
static pthread_cond_t async_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t async_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t add_lock = PTHREAD_MUTEX_INITIALIZER;


typedef struct S_ITEM {
    /**@brief 全局事件循环*/
    curl_socket_t sockfd;
    int action;
    CURL *easy;
    /**@brief ev重用标志,一直为1 */
    int evset;
    LwqqAsyncIo ev;
}S_ITEM;
typedef struct D_ITEM{
    LwqqCommand cmd;
    LwqqHttpRequest* req;
    LwqqAsyncEvent* event;
    //void* data;
    LwqqAsyncTimer delay;
    LIST_ENTRY(D_ITEM) entries;
}D_ITEM;
/* For async request */

#define slist_free_all(list) \
while(list!=NULL){ \
    void *ptr = list; \
    list = list->next; \
    s_free(ptr); \
}
#define slist_append(list,node) \
(node->next = list,node)

static int lwqq_gdb_whats_running()
{
    D_ITEM* item;
    char* url;
    int num = 0;
    LIST_FOREACH(item,&global.conn_link,entries){
        curl_easy_getinfo(item->req->req,CURLINFO_EFFECTIVE_URL,&url);
        lwqq_puts(url);
        num++;
    }
    return num;
}

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

    int use_old = 0;
    struct curl_slist* list = request->header;
    while(list){
        if(!strncmp(list->data,name,strlen(name))){
            s_free(list->data);
            list->data = s_strdup(opt);
            use_old = 1;
            break;
        }
        list = list->next;
    }
    if(!use_old){
        request->header = curl_slist_append((struct curl_slist*)request->header,opt);
    }

    curl_easy_setopt(request->req,CURLOPT_HTTPHEADER,request->header);
    s_free(opt);
}

void lwqq_http_set_default_header(LwqqHttpRequest *request)
{
    lwqq_http_set_header(request, "User-Agent", LWQQ_HTTP_USER_AGENT);
    lwqq_http_set_header(request, "Accept", "*/*,text/html, application/xml;q=0.9, "
                         "application/xhtml+xml, image/png, image/jpeg, "
                         "image/gif, image/x-xbitmap,;q=0.1");
    lwqq_http_set_header(request, "Accept-Language", "zh-cn,zh;q=0.9,en;q=0.8");
    //lwqq_http_set_header(request, "Accept-Charset", "GBK, utf-8, utf-16, *;q=0.1");
    lwqq_http_set_header(request, "Accept-Encoding", "deflate, gzip, x-gzip, "
                         "identity, *;q=0");
    lwqq_http_set_header(request, "Connection", "keep-alive");
}

static const char *lwqq_http_get_header(LwqqHttpRequest *request, const char *name)
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

    return h;
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
    if (!cookie) {
        lwqq_log(LOG_WARNING, "No cookie: %s\n", name);
        return NULL;
    }

    //lwqq_log(LOG_DEBUG, "Parse Cookie: %s=%s\n", name, cookie);
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
        s_free(request->location);
        curl_slist_free_all(request->header);
        curl_slist_free_all(request->recv_head);
        slist_free_all(request->cookie);
        curl_formfree(request->form_start);
        if(request->req){
            curl_easy_cleanup(request->req);
        }
        s_free(request);
    }
}

static size_t write_header( void *ptr, size_t size, size_t nmemb, void *userdata)
{
    char* str = (char*)ptr;
    LwqqHttpRequest* request = (LwqqHttpRequest*) userdata;

    long http_code;
    curl_easy_getinfo(request->req,CURLINFO_RESPONSE_CODE,&http_code);
    //this is a redirection. ignore it.
    if(http_code == 301||http_code == 302){
        if(strncmp(str,"Location",strlen("Location"))==0){
            const char* location = str+strlen("Location: ");
            request->location = s_strdup(location);
            int len = strlen(request->location);
            //remove the last \r\n
            request->location[len-1] = 0;
            request->location[len-2] = 0;
        }
        return size*nmemb;
    }
    request->recv_head = curl_slist_append(request->recv_head,(char*)ptr);
    //read cookie from header;
    if(strncmp(str,"Set-Cookie",strlen("Set-Cookie"))==0){
        struct cookie_list * node = s_malloc0(sizeof(*node));
        sscanf(str,"Set-Cookie: %[^=]=%[^;];",node->name,node->value);
        request->cookie = slist_append(request->cookie,node);
        LwqqClient* lc = request->lc;
        if(!lc->cookies) lc->cookies = s_malloc0(sizeof(LwqqCookies));
        lwqq_set_cookie(lc->cookies, node->name, node->value);
    }
    return size*nmemb;
}
static size_t write_content(void* ptr,size_t size,size_t nmemb,void* userdata)
{
    LwqqHttpRequest* request = (LwqqHttpRequest*) userdata;
    long http_code;
    curl_easy_getinfo(request->req,CURLINFO_RESPONSE_CODE,&http_code);
    //this is a redirection. ignore it.
    if(http_code == 301||http_code == 302){
        return size*nmemb;
    }
    int resp_len = request->resp_len;
    if(request->response==NULL){
        const char* content_length = request->get_header(request,"Content-Length");
        if(content_length){
            size_t length = atol(content_length);
            request->response = s_malloc0(length+10);
            request->resp_realloc = 0;
        }else{
            request->response = s_malloc0(size*nmemb+10);
            request->resp_realloc = 1;
        }
        resp_len = 0;
        request->resp_len = 0;
    }
    if(request->resp_realloc){
        request->response = s_realloc(request->response,resp_len+size*nmemb+5);
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
    request->retry = LWQQ_RETRY_VALUE;
    if (!request->req) {
        /* Seem like request->req must be non null. FIXME */
        goto failed;
    }
    if(curl_easy_setopt(request->req,CURLOPT_URL,uri)!=0){
        lwqq_log(LOG_WARNING, "Invalid uri: %s\n", uri);
        goto failed;
    }
    if(global.share==NULL) lwqq_http_global_init();
    curl_easy_setopt(request->req,CURLOPT_SHARE,global.share);
    curl_easy_setopt(request->req,CURLOPT_HEADERFUNCTION,write_header);
    curl_easy_setopt(request->req,CURLOPT_HEADERDATA,request);
    curl_easy_setopt(request->req,CURLOPT_WRITEFUNCTION,write_content);
    curl_easy_setopt(request->req,CURLOPT_WRITEDATA,request);
    curl_easy_setopt(request->req,CURLOPT_NOSIGNAL,1);
    curl_easy_setopt(request->req,CURLOPT_FOLLOWLOCATION,1);
    curl_easy_setopt(request->req,CURLOPT_CONNECTTIMEOUT,10);
    //set normal operate timeout to 30.official value.
    //curl_easy_setopt(request->req,CURLOPT_TIMEOUT,30);
    //5B/s
    curl_easy_setopt(request->req,CURLOPT_LOW_SPEED_LIMIT,8*5);
    curl_easy_setopt(request->req,CURLOPT_LOW_SPEED_TIME,30);
    request->do_request = lwqq_http_do_request;
    request->do_request_async = lwqq_http_do_request_async;
    request->set_header = lwqq_http_set_header;
    request->get_header = lwqq_http_get_header;
    request->get_cookie = lwqq_http_get_cookie;
    request->add_form = lwqq_http_add_form;
    request->add_file_content = lwqq_http_add_file_content;
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
        dest = s_realloc(dest, totalsize+1);
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

    lwqq_http_set_default_header(req);
    req->lc = lc;
    //lwqq_log(LOG_DEBUG, "Create request object for url: %s sucessfully\n", url);
    return req;
}

/************************************************************************/
/* Those Code for async API */


static void async_complete(D_ITEM* conn)
{
    LwqqHttpRequest* request = conn->req;
    int have_read_bytes;
    int res;
    char** resp = &request->response;

    have_read_bytes = request->resp_len;
    curl_easy_getinfo(request->req,CURLINFO_RESPONSE_CODE,&request->http_code);

    /* NB: *response may null */
    if (*resp == NULL) {
        goto failed;
    }

    /* Uncompress data here if we have a Content-Encoding header */
    const char *enc_type = NULL;
    enc_type = lwqq_http_get_header(request, "Content-Encoding");
    if (enc_type && strstr(enc_type, "gzip")) {
        char *outdata;
        int total = 0;
        
        outdata = ungzip(*resp, have_read_bytes, &total);
        outdata[total] = '\0';
        if (!outdata) {
            goto failed;
        }

        s_free(*resp);
        /* Update response data to uncompress data */
        *resp = s_strdup(outdata);
        (*resp)[total] = '\0';
        s_free(outdata);
        have_read_bytes = total;
        request->resp_len = total;
    }
failed:
    vp_do(conn->cmd,&res);
    lwqq_async_event_set_result(conn->event,res);
    lwqq_async_event_finish(conn->event);
    s_free(conn);
    return ;
}

static void check_multi_info(GLOBAL *g)
{
    CURLMsg *msg=NULL;
    int msgs_left;
    D_ITEM *conn;
    LwqqHttpRequest* req;
    LwqqAsyncEvent* ev;
    CURL *easy;
    CURLcode ret;

    //printf("still_running:%d\n",g->still_running);
    while ((msg = curl_multi_info_read(g->multi, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            easy = msg->easy_handle;
            ret = msg->data.result;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);
            req = conn->req;
            ev = conn->event;
            if(ret != 0){
                lwqq_log(LOG_WARNING,"async retcode:%d\n",ret);
            }
            if(ret != CURLE_OK){
                if(ret == CURLE_ABORTED_BY_CALLBACK){
                    ev->failcode = LWQQ_CALLBACK_CANCELED;
                }
                req->retry --;
                if(req->retry > 0){
                    //re add it to libcurl
                    curl_multi_remove_handle(g->multi, easy);
                    curl_multi_add_handle(g->multi, easy);
                    continue;
                }
                if(ret == CURLE_OPERATION_TIMEDOUT)
                    ev->failcode = LWQQ_CALLBACK_TIMEOUT;
                else
                    ev->failcode = LWQQ_CALLBACK_FAILED;
            }
/*            if(ret == CURLE_OPERATION_TIMEDOUT){
                req->retry --;
                if(req->retry == 0){
                    ev->failcode = LWQQ_CALLBACK_TIMEOUT;
                }else{
                    //re add it to libcurl
                    curl_multi_remove_handle(g->multi, easy);
                    curl_multi_add_handle(g->multi, easy);
                    continue;
                }
            }else if(ret == CURLE_ABORTED_BY_CALLBACK){
                ev->failcode = LWQQ_CALLBACK_CANCELED;
            }*/

            curl_multi_remove_handle(g->multi, easy);
            LIST_REMOVE(conn,entries);
            LwqqClient* lc = conn->req->lc;

            //if this handle doesn't timeout.so we restore it retry times
            if(ev->failcode != LWQQ_CALLBACK_TIMEOUT) req->retry = LWQQ_RETRY_VALUE;
            //执行完成时候的回调
            if(lwqq_client_valid(lc))
            lc->dispatch(vp_func_p,(CALLBACK_FUNC)async_complete,conn);
        }
    }
}
static void timer_cb(LwqqAsyncTimerHandle timer,void* data)
{
    //这个表示有超时任务出现.
    GLOBAL* g = data;
    //printf("timeout_come\n");

    if(!g->multi){
        lwqq_async_timer_stop(timer);
        return;
    }
    curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
    check_multi_info(g);
    //this is inner timeout 
    //always keep it
    lwqq_async_timer_repeat(timer);
}
static int multi_timer_cb(CURLM *multi, long timeout_ms, void *userp)
{
    //lwqq_log(LOG_NOTICE,"multi_timer,timeout:%ld\n",timeout_ms);
    lwqq_verbose(5,"multi_timer,timeout:%ld\n",timeout_ms);
    //this function call only when timeout clock '''changed'''.
    //called by curl
    GLOBAL* g = userp;
    //printf("timer_cb:%ld\n",timeout_ms);
    lwqq_async_timer_stop(&g->timer_event);
#if USE_DEBUG && LWQQ_VERBOSE_LEVEL>=5
   if(g->still_running>1){
        lwqq_gdb_whats_running();
    }
#endif
    if (timeout_ms > 0) {
        //change time clock
        lwqq_async_timer_watch(&g->timer_event,timeout_ms,timer_cb,g);
    } else{
        //keep time clock
        timer_cb(&g->timer_event,g);
    }
    //close time clock
    //this should always return 0 this is curl!!
    return 0;
}
static void event_cb(void* data,int fd,int revents)
{
    GLOBAL* g = data;

    int action = (revents&LWQQ_ASYNC_READ?CURL_POLL_IN:0)|
                 (revents&LWQQ_ASYNC_WRITE?CURL_POLL_OUT:0);
    curl_multi_socket_action(g->multi, fd, action, &g->still_running);
    check_multi_info(g);
    if ( g->still_running <= 0 ) {
        lwqq_async_timer_stop(&g->timer_event);
    }
}
static void setsock(S_ITEM*f, curl_socket_t s, CURL*e, int act,GLOBAL* g)
{
    int kind = ((act&CURL_POLL_IN)?LWQQ_ASYNC_READ:0)|((act&CURL_POLL_OUT)?LWQQ_ASYNC_WRITE:0);

    f->sockfd = s;
    f->action = act;
    f->easy = e;
    if ( f->evset )
        lwqq_async_io_stop(&f->ev);
    //since read+write works fine. we find out 'kind' not worked when have time
    //lwqq_async_io_watch(&f->ev,f->sockfd,LWQQ_ASYNC_READ|LWQQ_ASYNC_WRITE,event_cb,g);
    //set both direction may cause upload file failed.so we restore it.
    lwqq_async_io_watch(&f->ev,f->sockfd,kind,event_cb,g);

    f->evset=1;
}
static int sock_cb(CURL* e,curl_socket_t s,int what,void* cbp,void* sockp)
{
    S_ITEM *si = (S_ITEM*)sockp;
    GLOBAL* g = cbp;

    if(what == CURL_POLL_REMOVE) {
        //清除socket关联对象
        if ( si ) {
            if ( si->evset )
                lwqq_async_io_stop(&si->ev);
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
static void delay_add_handle(void* data,int fd,int act)
{
    pthread_mutex_lock(&add_lock);
    char buf[16];
    //remove from pipe
    read(fd,buf,sizeof(buf));
    D_ITEM* di,*tvar;
    LIST_FOREACH_SAFE(di,&global.add_link,entries,tvar){
        LIST_REMOVE(di,entries);
        LIST_INSERT_HEAD(&global.conn_link,di,entries);
        CURLMcode rc = curl_multi_add_handle(global.multi,di->req->req);

        if(rc != CURLM_OK){
            lwqq_puts(curl_multi_strerror(rc));
        }
    }
    pthread_mutex_unlock(&add_lock);
}
static LwqqAsyncEvent* lwqq_http_do_request_async(LwqqHttpRequest *request, int method,
                                      char *body, LwqqCommand command)
{
    if (!request->req)
        return NULL;

    if(LWQQ_SYNC_ENABLED()){
        lwqq_http_do_request(request,method,body);
        vp_do(command,NULL);
        //if(callback) callback(request,data);
        return NULL;
    }

    char **resp = &request->response;

    /* Clear off last response */
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
        request->http_code = 0;
        request->resp_len = 0;
        curl_slist_free_all(request->recv_head);
        request->recv_head = NULL;
    }

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
    curl_easy_setopt(request->req,CURLOPT_PRIVATE,di);
    di->cmd = command;
    di->req = request;
    di->event = lwqq_async_event_new(request);
    pthread_mutex_lock(&add_lock);
    LIST_INSERT_HEAD(&global.add_link,di,entries);
    write(global.pipe_fd[1],"ok",3);
    pthread_mutex_unlock(&add_lock);
    //LIST_INSERT_HEAD(&global.conn_link,di,entries);
    //lwqq_async_timer_watch(&di->delay,10,delay_add_handle,di);
    return di->event;

failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return NULL;
}
static int lwqq_http_do_request(LwqqHttpRequest *request, int method, char *body)
{
    if (!request->req)
        return -1;
    CURLcode ret;
retry:
    ret=0;

    int have_read_bytes = 0;
    char **resp = &request->response;

    /* Clear off last response */
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
        request->http_code = 0;
        request->resp_len = 0;
        curl_slist_free_all(request->recv_head);
        request->recv_head = NULL;
    }

    /* Set http method */
    if (method==0){
    }else if (method == 1 && body) {
        curl_easy_setopt(request->req,CURLOPT_POST,1);
        curl_easy_setopt(request->req,CURLOPT_COPYPOSTFIELDS,body);
    } else {
        lwqq_log(LOG_WARNING, "Wrong http method\n");
        goto failed;
    }

    ret = curl_easy_perform(request->req);
    if(ret != CURLE_OK){
        lwqq_log(LOG_ERROR,"do_request fail curlcode:%d\n",ret);
        if(ret == CURLE_ABORTED_BY_CALLBACK){
            return LWQQ_EC_CANCELED;
        }
        request->retry -- ;
        if(request->retry > 0){
            goto retry;
        }
        if(ret == CURLE_OPERATION_TIMEDOUT)
            return LWQQ_EC_TIMEOUT_OVER;
        else return LWQQ_EC_NETWORK_ERROR;
    }
    //perduce timeout.
    /*if(ret == CURLE_OPERATION_TIMEDOUT ){
        lwqq_log(LOG_WARNING,"do_request timeout\n");
        request->retry --;
        if(request->retry == 0){
            lwqq_log(LOG_WARNING,"do_request retry used out\n");
            return LWQQ_EC_TIMEOUT_OVER;
        }
        goto retry;
    }
    if(ret == CURLE_ABORTED_BY_CALLBACK ){
    }
    if(ret != CURLE_OK){
        lwqq_log(LOG_ERROR,"do_request fail curlcode:%d\n",ret);
        return ret;
    }*/
    request->retry = LWQQ_RETRY_VALUE;
    have_read_bytes = request->resp_len;
    curl_easy_getinfo(request->req,CURLINFO_RESPONSE_CODE,&request->http_code);

    // NB: *response may null 
    // jump it .that is no problem.
    if (*resp == NULL) {
        goto failed;
    }

    /* Uncompress data here if we have a Content-Encoding header */
    const char *enc_type = NULL;
    enc_type = lwqq_http_get_header(request, "Content-Encoding");
    if (enc_type && strstr(enc_type, "gzip")) {
        char *outdata;
        int total = 0;
        
        outdata = ungzip(*resp, have_read_bytes, &total);
        if (!outdata) {
            goto failed;
        }

        s_free(*resp);
        /* Update response data to uncompress data */
        *resp = s_strdup(outdata);
        s_free(outdata);
        have_read_bytes = total;
    }

    return 0;

failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return 0;
}
static void share_lock(CURL* handle,curl_lock_data data,curl_lock_access access,void* userptr)
{
    //this is shared access.
    //no need to lock it.
    if(access == CURL_LOCK_ACCESS_SHARED) return;
    GLOBAL* g = userptr;
    int idx;
    if(data == CURL_LOCK_DATA_DNS) idx=0;
    else if(data == CURL_LOCK_DATA_CONNECT) idx=1;
    else return;
    pthread_mutex_lock(&g->share_lock[idx]);

}
static void share_unlock(CURL* handle,curl_lock_data data,void* userptr)
{
    GLOBAL* g = userptr;
    int idx;
    if(data == CURL_LOCK_DATA_DNS) idx=0;
    else if(data == CURL_LOCK_DATA_CONNECT) idx=1;
    else return;
    pthread_mutex_unlock(&g->share_lock[idx]);
}
void lwqq_http_global_init()
{
    if(global.multi==NULL){
        curl_global_init(CURL_GLOBAL_ALL);
#if LWQQ_ENABLE_SSL
        signal(SIGPIPE,SIG_IGN);
#endif
        global.multi = curl_multi_init();
        curl_multi_setopt(global.multi,CURLMOPT_SOCKETFUNCTION,sock_cb);
        curl_multi_setopt(global.multi,CURLMOPT_SOCKETDATA,&global);
        curl_multi_setopt(global.multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
        curl_multi_setopt(global.multi, CURLMOPT_TIMERDATA, &global);
        pipe(global.pipe_fd);
        lwqq_async_io_watch(&global.add_listener, global.pipe_fd[0], LWQQ_ASYNC_READ, delay_add_handle, NULL);
    }
    if(global.share==NULL){
        global.share = curl_share_init();
        CURLSH* share = global.share;
        curl_share_setopt(share,CURLSHOPT_SHARE,CURL_LOCK_DATA_DNS);
        curl_share_setopt(share,CURLSHOPT_SHARE,CURL_LOCK_DATA_CONNECT);
        curl_share_setopt(share,CURLSHOPT_LOCKFUNC,share_lock);
        curl_share_setopt(share,CURLSHOPT_UNLOCKFUNC,share_unlock);
        curl_share_setopt(share,CURLSHOPT_USERDATA,&global);
        pthread_mutex_init(&global.share_lock[0],NULL);
        pthread_mutex_init(&global.share_lock[1],NULL);
    }
}

static void safe_remove_link(LwqqClient* lc)
{
    D_ITEM * item,* tvar;
    CURL* easy;
    LIST_FOREACH_SAFE(item,&global.conn_link,entries,tvar){
        if(lc && (item->req->lc != lc) ) continue;
        easy = item->req->req;
        curl_easy_pause(easy, CURLPAUSE_ALL);
        curl_multi_remove_handle(global.multi, easy);
    }
    pthread_cond_signal(&async_cond);
}
void lwqq_http_global_free()
{
    if(global.multi){
        if(!LIST_EMPTY(&global.conn_link)){
            lwqq_async_dispatch(vp_func_p,(CALLBACK_FUNC)safe_remove_link,NULL);
            pthread_mutex_lock(&async_lock);
            pthread_cond_wait(&async_cond,&async_lock);
            pthread_mutex_unlock(&async_lock);
        }

        D_ITEM * item,* tvar;
        LIST_FOREACH_SAFE(item,&global.conn_link,entries,tvar){
            LIST_REMOVE(item,entries);
            //let callback delete data
            vp_do(item->cmd,NULL);
            lwqq_async_event_set_code(item->event,LWQQ_CALLBACK_FAILED);
            lwqq_async_event_finish(item->event);
            s_free(item);
        }
        curl_multi_cleanup(global.multi);
        global.multi = NULL;
        lwqq_async_io_stop(&global.add_listener);
        close(global.pipe_fd[0]);
        close(global.pipe_fd[1]);
        lwqq_async_timer_stop(&global.timer_event);
        memset(&global.timer_event,sizeof(LwqqAsyncTimer),0);
        curl_global_cleanup();
    }
    if(global.share){
        curl_share_cleanup(global.share);
        global.share = NULL;
        pthread_mutex_destroy(&global.share_lock[0]);
        pthread_mutex_destroy(&global.share_lock[1]);
    }
}
void lwqq_http_cleanup(LwqqClient*lc)
{
    if(global.multi){
        if(!LIST_EMPTY(&global.conn_link)){
            lwqq_async_dispatch(vp_func_p,(CALLBACK_FUNC)safe_remove_link,lc);
            pthread_mutex_lock(&async_lock);
            pthread_cond_wait(&async_cond,&async_lock);
            pthread_mutex_unlock(&async_lock);
        }

        D_ITEM * item,* tvar;
        LIST_FOREACH_SAFE(item,&global.conn_link,entries,tvar){
            if(item->req->lc != lc) continue;
            LIST_REMOVE(item,entries);
            //let callback delete data
            vp_do(item->cmd,NULL);
            lwqq_async_event_set_code(item->event,LWQQ_CALLBACK_FAILED);
            lwqq_async_event_finish(item->event);
            s_free(item);
        }
    }
}

static void lwqq_http_add_form(LwqqHttpRequest* request,LWQQ_FORM form,const char* name,const char* value)
{
    struct curl_httppost** post = (struct curl_httppost**)&request->form_start;
    struct curl_httppost** last = (struct curl_httppost**)&request->form_end;
    switch(form){
        case LWQQ_FORM_FILE:
            curl_formadd(post,last,CURLFORM_COPYNAME,name,CURLFORM_FILE,value,CURLFORM_END);
            break;
        case LWQQ_FORM_CONTENT:
            curl_formadd(post,last,CURLFORM_COPYNAME,name,CURLFORM_COPYCONTENTS,value,CURLFORM_END);
            break;
    }
    curl_easy_setopt(request->req,CURLOPT_HTTPPOST,request->form_start);
}
static void lwqq_http_add_file_content(LwqqHttpRequest* request,const char* name,
        const char* filename,const void* data,size_t size,const char* extension)
{
    struct curl_httppost** post = (struct curl_httppost**)&request->form_start;
    struct curl_httppost** last = (struct curl_httppost**)&request->form_end;
    char *type = NULL;
    if(extension == NULL){
        extension = strrchr(filename,'.');
        if(extension !=NULL) extension++;
    }
    if(extension == NULL) type = NULL;
    else{
        if(strcmp(extension,"jpg")==0||strcmp(extension,"jpeg")==0)
            type = "image/jpeg";
        else if(strcmp(extension,"png")==0)
            type = "image/png";
        else if(strcmp(extension,"gif")==0)
            type = "image/gif";
        else if(strcmp(extension,"bmp")==0)
            type = "image/bmp";
        else type = NULL;
    }
    if(type==NULL){
        curl_formadd(post,last,
                CURLFORM_COPYNAME,name,
                CURLFORM_BUFFER,filename,
                CURLFORM_BUFFERPTR,data,
                CURLFORM_BUFFERLENGTH,size,
                CURLFORM_END);
    }else{
        curl_formadd(post,last,
                CURLFORM_COPYNAME,name,
                CURLFORM_BUFFER,filename,
                CURLFORM_BUFFERPTR,data,
                CURLFORM_BUFFERLENGTH,size,
                CURLFORM_CONTENTTYPE,type,
                CURLFORM_END);
    }
    curl_easy_setopt(request->req,CURLOPT_HTTPPOST,request->form_start);
}

static int lwqq_http_progress_trans(void* d,double dt,double dn,double ut,double un)
{
    LwqqHttpRequest* req = d;
    if(req->retry == 0) return 1;
    time_t ct = time(NULL);
    if(ct<=req->last_prog) return 0;

    req->last_prog = ct;
    size_t now = dn+un;
    size_t total = dt+ut;
    return req->progress_func?req->progress_func(req->prog_data,now,total):0;
}

void lwqq_http_on_progress(LwqqHttpRequest* req,LWQQ_PROGRESS progress,void* prog_data)
{
    if(!req) return;
    curl_easy_setopt(req->req,CURLOPT_PROGRESSFUNCTION,lwqq_http_progress_trans);
    req->progress_func = progress;
    req->prog_data = prog_data;
    req->last_prog = time(NULL);
    curl_easy_setopt(req->req,CURLOPT_PROGRESSDATA,req);
    curl_easy_setopt(req->req,CURLOPT_NOPROGRESS,0L);
}

void lwqq_http_set_option(LwqqHttpRequest* req,LwqqHttpOption opt,...)
{
    va_list args;
    va_start(args,opt);
    switch(opt){
        case LWQQ_HTTP_TIMEOUT:
            curl_easy_setopt(req->req,CURLOPT_LOW_SPEED_TIME,va_arg(args,unsigned long));
            break;
        case LWQQ_HTTP_NOT_FOLLOW:
            curl_easy_setopt(req->req,CURLOPT_FOLLOWLOCATION,!va_arg(args,long));
            break;
        case LWQQ_HTTP_SAVE_FILE:
            curl_easy_setopt(req->req,CURLOPT_WRITEFUNCTION,NULL);
            curl_easy_setopt(req->req,CURLOPT_WRITEDATA,va_arg(args,FILE*));
            break;
        case LWQQ_HTTP_RESET_URL:
            curl_easy_setopt(req->req,CURLOPT_URL,va_arg(args,const char*));
            break;
        case LWQQ_HTTP_VERBOSE:
            curl_easy_setopt(req->req,CURLOPT_VERBOSE,va_arg(args,long));
            break;
        case LWQQ_HTTP_CANCELABLE:
            if(va_arg(args,long)&&req->progress_func==NULL)
                lwqq_http_on_progress(req, NULL, NULL);
            break;
    }
    va_end(args);
}
void lwqq_http_cancel(LwqqHttpRequest* req)
{
    req->retry = 0;
}
