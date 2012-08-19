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

/**@param data this is special data passed by liblwqq.
 *              it's value is different in different ListenerType.
 *              for example FRIEND_COME this is LwqqBuddy*
 *                          GROUP_COME this is LwqqGroup*
 * @return you should return a errno.
 *          0 means success.
 */
typedef int (*ASYNC_CALLBACK)(LwqqClient* lc,void* data);
typedef enum ListenerType {
    LOGIN_COMPLETE,
    FRIEND_COME,///< after get friend qqnumber
    FRIEND_COMPLETE,
    GROUP_COME,///< after get group qqnumber
    FRIEND_AVATAR,///<after get_friend_avatar
    GROUP_AVATAR,///<after get_group_avatar
    POLL_LOST_CONNECTION,///
    POLL_MSG_COME,
    VERIFY_COME,
    ListenerTypeLength
} ListenerType;
typedef struct _LwqqAsync {
    ASYNC_CALLBACK listener[ListenerTypeLength];
    LwqqErrorCode err[ListenerTypeLength];
    void* data[ListenerTypeLength];
} _LwqqAsync;
/**set async enabled or disabled*/
void lwqq_async_set(LwqqClient* client,int enabled);
/** check if async enabled*/
#define lwqq_async_enabled(lc) (lc->async!=NULL)
/** add a ASYNC_CALLBACK type listener.*/
#define lwqq_async_add_listener(lc,type,callback) \
    ((lwqq_async_enabled(lc))?lc->async->listener[type] = callback:0)
/**  remove a listener */
#define lwqq_async_remove_listener(lc,type) (lwqq_async_add_listener(lc,type,NULL))
/** set some data for listener.
 *  note: one type listener can only set one pointer 
 */
#define lwqq_async_set_userdata(lc,type,value) \
    ((lwqq_async_enabled(lc))?lc->async->data[type] = value:0)
#define lwqq_async_get_userdata(lc,type) \
    ((lwqq_async_enabled(lc))?lc->async->data[type]:NULL)
/** set a errno for listener.
 *  note： one type listener can only set one error
 */
#define lwqq_async_set_error(lc,type,err) \
    ((lwqq_async_enabled(lc))?lc->async->err[type] = err:0)
#define lwqq_async_get_error(lc,type) \
    ((lwqq_async_enabled(lc))?lc->async->err[type]:0)
#define lwqq_async_has_listener(lc,type) (lc->async->listener[type]!=NULL)
/** dispatch a async listener */
void lwqq_async_dispatch(LwqqClient* lc,ListenerType type,void* extradata);

/**===================EVSET API==========================================**/
/** this api is better than async listener api.
 * it is more powerful and easier to use
 */
typedef struct _LwqqAsyncEvset LwqqAsyncEvset;
typedef void (*EVENT_CALLBACK)(LwqqAsyncEvent* event,void* data);
typedef void (*EVSET_CALLBACK)(int result,void* data);
/** return a new evset. a evset can link multi of event.
 * you can wait a evset. means it would block ultil all event is finished
 */
#define lwqq_async_evset_new() lwqq_async_evset_new_with_debug(__FILE__,__LINE__)
LwqqAsyncEvent* lwqq_async_event_new_with_debug(const char* file,int line);
/** return a new event. 
 * you can wait a event by use evset or LWQQ_SYNC macro simply.
 * you can also add a event listener
 */
#define lwqq_async_event_new() lwqq_async_event_new_with_debug(__FILE__,__LINE__)
LwqqAsyncEvset* lwqq_async_evset_new_with_debug(const char* file,int line);
/** this would remove a event.
 * and call a event listener if is set.
 * and try to wake up a evset if all events of evset are finished
 */
void lwqq_async_event_finish(LwqqAsyncEvent* event);
/** this is same as lwqq_async_event_finish */
#define lwqq_async_event_emit(event) lwqq_async_event_finish(event)
/** this would add a event to a evset.
 * note one event can only link to one evset.
 * one evset can link to multi event.
 * do not repeatly add one event twice
 */
void lwqq_async_evset_add_event(LwqqAsyncEvset* host,LwqqAsyncEvent *handle);
/**
 * @return the error number of evset.
 * it is one of event error number.
 * it can only store one error number.
 */
int lwqq_async_wait(LwqqAsyncEvset* host);
/** this add a event listener to a event.
 * it is better than lwqq_async_add_listener.
 * because you can set different data to different event which may the same function.
 * async listener can only set one data for one ListenerType.
 */
void lwqq_async_add_event_listener(LwqqAsyncEvent* event,EVENT_CALLBACK callback,void* data);

//void lwqq_async_add_evset_listener(LwqqAsyncEvset* evset,EVSET_CALLBACK callback,void* data);
/** this set the errno for a event.
 * it is a hack code.
 * ensure LwqqAsyncEvent first member is a int.
 */
#define lwqq_async_event_set_result(ev,res)\
do{\
    *((int*)ev) = res;\
}while(0)
/** this return a errno of a event. */
#define lwqq_async_event_get_result(ev) (*((int*)ev))
#define lwqq_async_evset_get_result(ev) (*((int*)ev))

/** this is a easy and useful macro.
 * it can keep sync for one function.
 * it is simple. it create one evset and add only one event.
 * then wait it finished.
 * so it is synced.
 */
#define LWQQ_SYNC(ev) \
    do{\
        LwqqAsyncEvset* evset = lwqq_async_evset_new();\
        LwqqAsyncEvent* event = ev;\
        lwqq_async_evset_add_event(evset,event);\
        lwqq_async_wait(evset);\
    }while(0)


#endif
