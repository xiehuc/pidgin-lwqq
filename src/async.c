
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
typedef struct async_dispatch_data {
    ListenerType type;
    LwqqClient* client;
    int handle;
    void* data;
} async_dispatch_data;
typedef struct _LwqqAsyncLock {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int ref_count;
}_LwqqAsyncLock;
typedef struct _LwqqAsyncEvent {
    LwqqAsyncLock* host_lock;
}_LwqqAsyncEvent;

static gboolean timeout_come(void* p);


void lwqq_async_dispatch(LwqqClient* lc,ListenerType type,void* param)
{
    if(!lwqq_async_has_listener(lc,type))
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
    if(!lwqq_async_enabled(lc)) return 0;
    if(lc->async->listener[type]!=NULL)
        lc->async->listener[type](lc,data->data);

    free(data);
    //remote handle;
    return 0;
}


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
LwqqAsyncEvent* lwqq_async_event_new()
{
    return s_malloc0(sizeof(LwqqAsyncEvent));
}
LwqqAsyncLock* lwqq_async_lock_new()
{
    LwqqAsyncLock* l = s_malloc(sizeof(*l));
    pthread_mutex_init(&l->lock,NULL);
    pthread_cond_init(&l->cond,NULL);
    l->ref_count = 0;
    return l;
}
void lwqq_async_event_finish(LwqqAsyncEvent* event)
{
    if(event->host_lock !=NULL){
        pthread_mutex_lock(&event->host_lock->lock);
        event->host_lock->ref_count--;
        pthread_mutex_unlock(&event->host_lock->lock);
        if(event->host_lock->ref_count==0){
            pthread_cond_signal(&event->host_lock->cond);
        }
    }
    s_free(event);
}
void lwqq_async_lock_add_event(LwqqAsyncLock* host,LwqqAsyncEvent *handle)
{
    pthread_mutex_lock(&host->lock);
    handle->host_lock = host;
    host->ref_count++;
    pthread_mutex_unlock(&host->lock);
}

void lwqq_async_wait(LwqqAsyncLock* host)
{
    pthread_mutex_lock(&host->lock);
    if(host->ref_count>0){
        pthread_cond_wait(&host->cond,&host->lock);
    }
    pthread_mutex_unlock(&host->lock);
    s_free(host);
}
