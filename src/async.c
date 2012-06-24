
/**
 * @file   async.c
 * @author xiehuc<xiehuc@gmail.com>
 * @date   Sun May 20 02:21:43 2012
 *
 * @brief  Linux WebQQ Async API
 * use libev
 *
 */

#include <ghttp.h>
#include <stdlib.h>
#include <string.h>
#include <plugin.h>
#include "async.h"
typedef struct _LwqqAsync {
    ASYNC_CALLBACK listener[5];
    LwqqErrorCode err[5];
    void* data[5];
} _LwqqAsync;
typedef struct async_dispatch_data {
    ListenerType type;
    LwqqClient* client;
    gint handle;
    void* data;
} async_dispatch_data;

void lwqq_async_add_listener(LwqqClient* lc,ListenerType type,ASYNC_CALLBACK callback)
{
    LwqqAsync* async = lc->async;
    async->listener[type] = callback;
}
void lwqq_async_set_userdata(LwqqClient* lc,ListenerType type,void *data)
{
    LwqqAsync* async = lc->async;
    async->data[type] = data;
}
void* lwqq_async_get_userdata(LwqqClient* lc,ListenerType type)
{
    return lc->async->data[type];
}
void lwqq_async_set_error(LwqqClient* lc,ListenerType type,LwqqErrorCode err)
{
    lc->async->err[type] = err;
}
LwqqErrorCode lwqq_async_get_error(LwqqClient* lc,ListenerType type)
{
    return lc->async->err[type];
}
static gboolean timeout_come(void* p);

void lwqq_async_dispatch(LwqqClient* lc,ListenerType type,void* param)
{
    async_dispatch_data* data = malloc(sizeof(async_dispatch_data));
    data->type = type;
    data->client = lc;
    data->data = param;
    data->handle = purple_timeout_add(50,timeout_come,data);
}

static gboolean timeout_come(void* p)
{
    async_dispatch_data* data = (async_dispatch_data*)p;
    LwqqClient* lc = data->client;
    ListenerType type = data->type;
    if(lc->async->listener[type]!=NULL)
        lc->async->listener[type](lc,data->data);
    /*if(data->handle>0)
        purple_timeout_remove(data->handle);*/
    free(data);
    //remote handle;
    return 0;
}

/*thread_t th;
int running=0;
void* lwqq_async_thread(void* data)
{
    struct ev_loop* loop = EV_DEFAULT;
    running=1;
    while(1){
        ev_run(loop,0);
    }
    running=0;
}*/
/*void check_start_thread(){
    if(th==NULL) thread_init(th);
    if(!running)
        thread_create(th,lwqq_async_thread,NULL,"thread");
}*/


/*#define FOREACH_CALLBACK(prefix) \
    while(async->prefix##_len>0){\
        data = async->prefix##_data[--async->prefix##_len];\
        async->prefix##_complete[async->prefix##_len](lc,request,data);\
        async->prefix##_complete[async->prefix##_len] = NULL;\
    }
void lwqq_async_callback(async_watch_data* data){
    LwqqClient* lc = data->client;
    LwqqAsyncListener* async = lc->async;
    LwqqHttpRequest* request = data->request;
    ListenerType type = data->type;
    switch(type){
        case LOGIN_COMPLETE:
            while(async->login_len>0){
                data = async->login_data[--async->login_len];
                async->login_complete[async->login_len](lc,request,data);
                async->login_complete[async->login_len] = NULL;
            }
            break;
        case FRIENDS_ALL_COMPLETE:
            FOREACH_CALLBACK(friends_all);
            break;
    }

}
int lwqq_async_has_listener(LwqqClient* lc,ListenerType type)
{
    switch(type){
        case LOGIN_COMPLETE:
            return lc->async->login_complete!=NULL;
            break;
    }
    return 0;
}*/
/*static void ev_io_come(EV_P_ ev_io* w,int revent)
{
    async_watch_data* data = (async_watch_data*)w->data;
    ghttp_status status;
    LwqqHttpRequest* request = data->request;
    status = ghttp_process(request->req);
    if(status!=ghttp_done) return ;

    //restore do_request end part
    lwqq_http_do_request_async(request);
    lwqq_async_callback(data);


    ev_io_stop(EV_DEFAULT,w);
    free(data);
    free(w);
}
void lwqq_async_watch(LwqqClient* client,LwqqHttpRequest* request,ListenerType type)
{
    ev_io *watcher = (ev_io*)malloc(sizeof(ev_io));
    ghttp_request* req = (ghttp_request*)request->req;
    ev_io_init(watcher,ev_io_come,ghttp_get_socket(req),EV_READ);
    async_watch_data* data = malloc(sizeof(async_watch_data));
    data->request = request;
    data->type = type;
    data->client = client;
    watcher->data = data;
    ev_io_start(EV_DEFAULT,watcher);
    check_start_thread();
}*/

void lwqq_async_set(LwqqClient* client,int enabled)
{
    if(enabled&&!lwqq_async_enabled(client)) {
        client->async = malloc(sizeof(LwqqAsync));
        memset(client->async,0,sizeof(LwqqAsync));
    } else if(!enabled&&lwqq_async_enabled(client)) {
        free(client->async);
        client->async=NULL;
    }

}
