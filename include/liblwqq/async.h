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
#include "http.h"
typedef void (*ASYNC_CALLBACK)(LwqqClient* lc,void* data);
typedef enum ListenerType {
    LOGIN_COMPLETE,
    FRIENDS_ALL_COMPLETE,
    VERIFY_COME,
    MSG_COME,
    STATUS_CHANGE
} ListenerType;
void lwqq_async_set(LwqqClient* client,int enabled);
#define lwqq_async_enabled(lc) (lc->async!=NULL)
void lwqq_async_set_userdata(LwqqClient* lc,ListenerType type,void* data);
void* lwqq_async_get_userdata(LwqqClient* lc,ListenerType type);
void lwqq_async_set_error(LwqqClient* lc,ListenerType type,LwqqErrorCode err);
LwqqErrorCode lwqq_async_get_error(LwqqClient* lc,ListenerType type);
void lwqq_async_add_listener(LwqqClient* lc,ListenerType type,ASYNC_CALLBACK callback);
void lwqq_async_watch(LwqqClient* client,LwqqHttpRequest* request,ListenerType type);
void lwqq_async_dispatch(LwqqClient* lc,ListenerType type,void* extradata);
#endif
