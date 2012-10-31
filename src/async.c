
/**
 * @file   async.c
 * @author xiehuc<xiehuc@gmail.com>
 * @date   Sun May 20 02:21:43 2012
 *
 * @brief  Linux WebQQ Async API
 * use libev
 *
 */

#include <stdlib.h>
#include <string.h>
#include <plugin.h>
#include <eventloop.h>
#include "async.h"
#include "smemory.h"
#include "http.h"
typedef struct async_dispatch_data {
    ListenerType type;
    LwqqClient* client;
    int handle;
    void* data;
} async_dispatch_data;
typedef struct _LwqqAsyncEvset{
    int result;///<it must put first
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int cond_waiting;
    int ref_count;
    EVSET_CALLBACK callback;
    void* data;
}_LwqqAsyncEvset;
typedef struct _LwqqAsyncEvent {
    int result;///<it must put first
    LwqqAsyncEvset* host_lock;
    EVENT_CALLBACK callback;
    void* data;
    EVENT_CALLBACK start;
    void* start_data;
    LwqqHttpRequest* req;
}_LwqqAsyncEvent;

static gboolean timeout_come(void* p);

int LWQQ_ASYNC_GLOBAL_SYNC_ENABLED = 0;

void lwqq_async_dispatch(LwqqClient* lc,ListenerType type,void* param)
{
    if(!lwqq_client_valid(lc)||!lwqq_async_has_listener(lc,type))
        return;
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
    if(lwqq_client_valid(lc)&&lwqq_async_enabled(lc)){
        if(lc->async->listener[type]!=NULL)
            lc->async->listener[type](lc,data->data);
    }
    free(data);
    //remote handle;
    return 0;
}


void lwqq_async_set(LwqqClient* client,int enabled)
{
    if(enabled&&client->async==NULL) {
        client->async = s_malloc0(sizeof(LwqqAsync));
        client->async->_enabled=1;
    } else if(!enabled&&lwqq_async_enabled(client)) {
        client->async->_enabled=0;
        /*free(client->async);
        client->async=NULL;*/
    }

}
LwqqAsyncEvent* lwqq_async_event_new(void* req)
{
    LwqqAsyncEvent* event = s_malloc0(sizeof(LwqqAsyncEvent));
    event->req = req;
    return event;
}
LwqqAsyncEvset* lwqq_async_evset_new()
{
    LwqqAsyncEvset* l = s_malloc0(sizeof(*l));
    pthread_mutex_init(&l->lock,NULL);
    pthread_cond_init(&l->cond,NULL);
    return l;
}
void lwqq_async_event_finish(LwqqAsyncEvent* event)
{
    if(event->callback){
        event->callback(event,event->data);
    }
    LwqqAsyncEvset* evset = event->host_lock;
    if(evset !=NULL){
        pthread_mutex_lock(&evset->lock);
        evset->ref_count--;
        //this store evset result.
        //it can only store one error number.
        if(event->result != 0)
            evset->result = event->result;
        if(event->host_lock->ref_count==0){
            if(evset->callback)
                evset->callback(evset,evset->data);
            if(evset->cond_waiting)
                pthread_cond_signal(&evset->cond);
            else{
                pthread_mutex_unlock(&evset->lock);
                s_free(evset);
                s_free(event);
                return;
            }
        }
        pthread_mutex_unlock(&evset->lock);
    }
    s_free(event);
}
void lwqq_async_evset_add_event(LwqqAsyncEvset* host,LwqqAsyncEvent *handle)
{
    if(handle==NULL) return;
    pthread_mutex_lock(&host->lock);
    handle->host_lock = host;
    host->ref_count++;
    pthread_mutex_unlock(&host->lock);
}

int lwqq_async_wait(LwqqAsyncEvset* host)
{
    int ret = 0;
    pthread_mutex_lock(&host->lock);
    if(host->ref_count>0){
        host->cond_waiting = 1;
        pthread_cond_wait(&host->cond,&host->lock);
    }
    pthread_mutex_unlock(&host->lock);
    ret = host->result;
    s_free(host);
    return ret;
}

void lwqq_async_add_event_listener(LwqqAsyncEvent* event,EVENT_CALLBACK callback,void* data)
{
    if(event == NULL){
        callback(NULL,data);
        return ;
    }
    event->callback = callback;
    event->data = data;
}

void lwqq_async_add_evset_listener(LwqqAsyncEvset* evset,EVSET_CALLBACK callback,void* data)
{
    if(!evset) return;
    evset->callback = callback;
    evset->data = data;
}

void lwqq_async_event_set_progress(LwqqAsyncEvent* event,LWQQ_PROGRESS callback,void* data)
{
    lwqq_http_on_progress(event->req,callback,data);
}
void lwqq_async_event_on_start(LwqqAsyncEvent* event,EVENT_CALLBACK start,void* data)
{
    event->start = start;
    event->start_data = data;
}
void lwqq_async_event_start(LwqqAsyncEvent* event,int socket)
{
    event->result = socket;
    if(event->start)
        event->start(event,event->start_data);
}
typedef struct {
    LwqqAsyncIoCallback callback;
    void* data;
}LwqqAsyncIoWrap;
typedef struct {
    LwqqAsyncTimerCallback callback;
    void* data;
}LwqqAsyncTimerWrap;
#ifdef USE_LIBEV
static void event_cb_wrap(EV_P_ ev_io *w,int action)
{
    LwqqAsyncWrap* wrap = w->data;
    if(wrap->callback)
        wrap->callback(wrap->data,action);
}
void lwqq_async_io_watch(LwqqAsyncIoHandle io,int fd,int action,AsyncIoCallback fun,void* data)
{
    ev_io_init(io,event_cb_wrap,fd,action);
    LwqqAsyncWrap* wrap = s_malloc0(sizeof(*wrap));
    wrap->callback = fun;
    wrap->data = data;
    io->data = wrap;
    ev_io_start(EV_DEFAULT,io);
}
void lwqq_async_timer_watch(LwqqAsyncTimerHandle timer,int second,AsyncTimerCallback fun,void* data)
{
}
#endif
#ifdef USE_LIBPURPLE
static void event_cb_wrap(void* data,int fd,PurpleInputCondition action)
{
    LwqqAsyncIoWrap* wrap = data;
    if(wrap->callback)
        wrap->callback(wrap->data,fd,action);
}
void lwqq_async_io_watch(LwqqAsyncIoHandle io,int fd,int action,LwqqAsyncIoCallback fun,void* data)
{
    LwqqAsyncIoWrap* wrap = s_malloc0(sizeof(*wrap));
    wrap->callback = fun;
    wrap->data = data;
    io->ev = purple_input_add(fd,action,event_cb_wrap,wrap);
    io->wrap = wrap;
}
void lwqq_async_io_stop(LwqqAsyncIoHandle io)
{
    purple_input_remove(io->ev);
    io->ev = 0;
    s_free(io->wrap);
}
void lwqq_async_timer_watch(LwqqAsyncTimerHandle timer,unsigned int timeout_ms,LwqqAsyncTimerCallback fun,void* data)
{
    *timer = purple_timeout_add(timeout_ms,fun,data);
}
void lwqq_async_timer_stop(LwqqAsyncTimerHandle timer)
{
    purple_timeout_remove(*timer);
    *timer = 0;
}
#endif
