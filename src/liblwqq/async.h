/**
 * @file   async.h
 * @author xiehuc<xiehuc@gmail.com>
 * @date   Sun May 20 22:24:30 2012
 *
 * @brief  Linux WebQQ Async API
 *
 *
 */
#ifndef LWQQ_ASYNC_H
#define LWQQ_ASYNC_H
#include "type.h"

typedef int (*ASYNC_CALLBACK)(LwqqClient* lc,void* data);
typedef enum ListenerType {
    LOGIN_COMPLETE,
    FRIEND_COME,///< after get friend qqnumber
    GROUP_COME,///< after get group qqnumber
    FRIEND_AVATAR,///<after get_friend_avatar
    GROUP_AVATAR,///<after get_group_avatar
    GROUP_DETAIL,
    VERIFY_COME,
    IM_SEND_FAILED,
    ListenerTypeLength
} ListenerType;
typedef struct _LwqqAsync {
    ASYNC_CALLBACK listener[ListenerTypeLength];
    LwqqErrorCode err[ListenerTypeLength];
    void* data[ListenerTypeLength];
} _LwqqAsync;
void lwqq_async_set(LwqqClient* client,int enabled);
#define lwqq_async_enabled(lc) (lc->async!=NULL)
#define lwqq_async_add_listener(lc,type,callback) \
    ((lwqq_async_enabled(lc))?lc->async->listener[type] = callback:0)
#define lwqq_async_remove_listener(lc,type) (lwqq_async_add_listener(lc,type,NULL))
#define lwqq_async_set_userdata(lc,type,value) \
    ((lwqq_async_enabled(lc))?lc->async->data[type] = value:0)
#define lwqq_async_get_userdata(lc,type) \
    ((lwqq_async_enabled(lc))?lc->async->data[type]:NULL)
#define lwqq_async_set_error(lc,type,err) \
    ((lwqq_async_enabled(lc))?lc->async->err[type] = err:0)
#define lwqq_async_get_error(lc,type) \
    ((lwqq_async_enabled(lc))?lc->async->err[type]:0)
#define lwqq_async_has_listener(lc,type) (lc->async->listener[type]!=NULL)
//void lwqq_async_watch(LwqqClient* client,LwqqHttpRequest* request,ListenerType type);
void lwqq_async_dispatch(LwqqClient* lc,ListenerType type,void* extradata);

/**===================EVSET API==========================================**/
typedef struct _LwqqAsyncEvset LwqqAsyncEvset;
typedef void (*EVENT_CALLBACK)(LwqqAsyncEvent* event,void* data);
LwqqAsyncEvset* lwqq_async_evset_new();
LwqqAsyncEvent* lwqq_async_event_new();
void lwqq_async_event_finish(LwqqAsyncEvent* event);
#define lwqq_async_event_emit(event) lwqq_async_event_finish(event)
void lwqq_async_evset_add_event(LwqqAsyncEvset* host,LwqqAsyncEvent *handle);
void lwqq_async_wait(LwqqAsyncEvset* host);
void lwqq_async_add_event_listener(LwqqAsyncEvent* event,EVENT_CALLBACK callback,void* data);
#define lwqq_async_event_set_result(ev,res)\
do{\
    *((int*)ev) = res;\
}while(0)
#define lwqq_async_event_get_result(ev) (*((int*)ev))

#define LWQQ_SYNC(ev) \
    do{\
        LwqqAsyncEvset* evset = lwqq_async_evset_new();\
        LwqqAsyncEvent* event = ev;\
        lwqq_async_evset_add_event(evset,event);\
        lwqq_async_wait(evset);\
    }while(0)


#endif
