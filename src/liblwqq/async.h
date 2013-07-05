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
#include "msg.h"
#include <assert.h>
#include <ev.h>

/**======================EVSET API=====================================**/
/** 
 * function:: this api provide easy used asynced event callback ability
 *            LwqqAsyncEvent is used for a single http request
 *            LwqqAsyncEvset is used for operate multi LwqqAsyncEvent
 *            so that brings more complicate action
 *
 * design  :: a event have a LwqqCommand to trigger callback
 *            and a pointer to a evset
 *
 *            a evset have a reference count of events
 *            and a LwqqCommand to trigger callback
 *
 *            so you can add a event to only one evset
 *
 *            when a event finished it would decrease evset reference count
 *            and when it decreased to zero, evset would trigger callback and free itself.
 *
 *            use s_free to free event or evset when necessary
 */
typedef struct LwqqAsyncEvent {
    /** 0 : success
     *  >0: errno form webqq server
     *  <0: errno from lwqq inner
     */
    int result;
    LwqqCallbackCode failcode; ///< would be depreciate
    LwqqClient* lc;
}LwqqAsyncEvent;
typedef struct LwqqAsyncEvset{
    int err_count;
}LwqqAsyncEvset;
/** 
 * create a new evset. 
 * if it successfully trigger callback function, it would be freed by library.
 * if it add zero event, and not trigger any thing. it would be a garbage and need freed by you.
 */
LwqqAsyncEvset* lwqq_async_evset_new();
void lwqq_async_evset_free(LwqqAsyncEvset* set);
/** 
 * create a new event. 
 * @param req : the bind http request with event
 *              NULL if it is controled by you
 */
LwqqAsyncEvent* lwqq_async_event_new(void* req);
/** 
 * this would trigger event callback and free event.
 * and decrease reference count of binded evset if have.
 * @param event : force makes a event finished.
 */
void lwqq_async_event_finish(LwqqAsyncEvent* event);
/** this is same as lwqq_async_event_finish */
#define lwqq_async_event_emit(event) lwqq_async_event_finish(event)
/** 
 * this would add a event to a evset.
 * @note one event can add to only one evset.
 *       one evset can link to multi events.
 */
void lwqq_async_evset_add_event(LwqqAsyncEvset* host,LwqqAsyncEvent *handle);
/** 
 * this add a callback to a event.
 * @param event : if NULL cmd would triggerd immediately
 *                because http request failed may return a NULL event pointer.
 *                need run cmd to do some clean work.
 * @param cmd   : if event is finished ,cmd would triggerd
 */
void lwqq_async_add_event_listener(LwqqAsyncEvent* event,LwqqCommand cmd);
/**
 * this add a callback to a evset.
 * @param evset : if NULL nothing would happened,
 *                and cmd would canceled without any notify(not friendly indeed)
 *                because evset is create by evset_new, shouldn't be NULL
 *
 *                if evset reference count is zero, evset would automaticly freed
 *                because it never trigger. if not freed now , no chance to free anymore
 * @param cmd   : if evset reference count decreased to zero, cmd would trigger
 */
void lwqq_async_add_evset_listener(LwqqAsyncEvset* evset,LwqqCommand cmd);
/** 
 * a event chain is helpful 
 * when caller finished with lwqq_async_event_finish
 * it trigger called finished also.
 *
 * so you can make a chain very long to create some complicate behavior
 */
void lwqq_async_add_event_chain(LwqqAsyncEvent* caller,LwqqAsyncEvent* called);
//#define type_assert(what,type) {type ptr = what;assert(ptr);}
#define lwqq_async_event_set_result(ev,res) (ev->result=res)
#define lwqq_async_event_set_code(ev,code)  (ev->failcode=code)
#define lwqq_async_event_get_result(ev)     (ev?ev->result:LWQQ_EC_ERROR)
#define lwqq_async_event_get_code(ev)       (ev?ev->failcode:LWQQ_CALLBACK_FAILED)
#define lwqq_async_event_get_owner(ev)      (ev?ev->lc:NULL)
/** this return the http request */
LwqqHttpRequest* lwqq_async_event_get_conn(LwqqAsyncEvent* ev);

/**
 * begin a sync area, must end with sync end
 * in area, all http request is synced and blocked
 */
#define LWQQ_SYNC_BEGIN(lc) {((LwqqHttpHandle*)lwqq_get_http_handle(lc))->synced=1;
#define LWQQ_SYNC_END(lc) ((LwqqHttpHandle*)lwqq_get_http_handle(lc))->synced=0;}
/*check client is set to synced*/
#define LWQQ_SYNC_ENABLED(lc) (((LwqqHttpHandle*)lwqq_get_http_handle(lc))->synced==1)
/**======================EVSET API END==================================**/

/**======================  QUEUE API  ==================================**/
/**
 * async queue is used for some heavy http request.need much time to receive complete data.
 * and last http request is still running, but call info api to make another http request.
 * which makes network traffic crowd.
 *
 * then need async queue. in function first search queue if there is already a request running.
 * if have return it and append eventlistener after it.
 * if not continue and makes a new request. this would create a event. then add it to queue, 
 * for next time search
 */
/**
 * find a async event with specified function
 * @param func : just a identifier, most time it is a function pointer
 */
LwqqAsyncEvent* lwqq_async_queue_find(LwqqAsyncQueue* queue,void* func);
/** add a event and function to a queue */
void lwqq_async_queue_add(LwqqAsyncQueue* queue,void* func,LwqqAsyncEvent* ev);
/** when request is finished, need remove identifier from queue */
void lwqq_async_queue_rm(LwqqAsyncQueue* queue,void* func);
/**======================QUEUE API END==================================**/

/** 
 * the default dispatch within libev event loop
 * @see LwqqClient::dispatch
 */
void lwqq_async_dispatch(LwqqCommand cmd);
//initialize lwqq client with default dispatch function
void lwqq_async_init(LwqqClient* lc);
/** 
 * call this function when you quit your program.
 * NOTE!! you must call lwqq_http_global_free first !!
 * */
void lwqq_async_global_quit();


//=========================LOW LEVEL EVENT LOOP API====================//
/** watch an io socket for special event
 * implement by libev or libpurple
 * @param io this is pointer to a LwqqAsyncIo struct
 * @param fd socket
 * @param action combination of LWQQ_ASYNC_READ and LWQQ_ASYNC_WRITE
 */

typedef struct LwqqAsyncTimer{
    ev_timer h;
    void (*func)(struct LwqqAsyncTimer* timer,void* data);
    void* data;
    int on_call;
}LwqqAsyncTimer;
typedef ev_io     LwqqAsyncIo;
typedef LwqqAsyncTimer* LwqqAsyncTimerHandle;
typedef ev_io*    LwqqAsyncIoHandle;
#define LWQQ_ASYNC_READ EV_READ
#define LWQQ_ASYNC_WRITE EV_WRITE
/** the call back of io watch 
 * @param data user defined data
 * @param fd the socket
 * @param action read/write enum value
 */
typedef void (*LwqqAsyncIoCallback)(void* data,int fd,int action);
/** the call back of timer watch
 * @param data user defined data
 * return 1 to continue timer 0 to stop timer.
 */
typedef void (*LwqqAsyncTimerCallback)(LwqqAsyncTimer* timer,void* data);
/** start a io watch */
void lwqq_async_io_watch(LwqqAsyncIoHandle io,int fd,int action,LwqqAsyncIoCallback func,void* data);
/** stop a io watcher */
void lwqq_async_io_stop(LwqqAsyncIoHandle io);
/** start a timer count down
 * @param timer the pointer to a LwqqAsyncTimer struct
 * @param ms microsecond time
 */
void lwqq_async_timer_watch(LwqqAsyncTimerHandle timer,unsigned int ms,LwqqAsyncTimerCallback func,void* data);
/** stop a timer */
void lwqq_async_timer_stop(LwqqAsyncTimerHandle timer);
/** 
 * reset a timer 
 * used at the end of LwqqAsyncTimerCallback function
 */
void lwqq_async_timer_repeat(LwqqAsyncTimerHandle timer);
//when caller finished . it would raise called finish yet.
//so it is calld event chain.
//=========================LWQQ ASYNC LOW LEVEL EVENT LOOP API====================//

#endif
