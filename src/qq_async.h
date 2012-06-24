/**
 * @file   async.h
 * @author xiehuc<xiehuc@gmail.com>
 * @date   Sun May 20 22:24:30 2012
 * 
 * @brief  Linux WebQQ Async API
 * 
 * 
 */
#ifndef QQ_ASYNC_H
#define QQ_ASYNC_H
#include "qq_types.h"
#include <http.h>
typedef void (*QQ_ASYNC_CALLBACK)(qq_account* lc,int err,void* data);
typedef void (*QQ_CALLBACK)(qq_account* ac,LwqqHttpRequest* request,void* data);

typedef enum QQ_ListenerType{
    LOGIN_COMPLETE,
    VERIFY_COME,
    FRIEND_COME,
    FRIENDS_COMPLETE,
    GROUPS_COMPLETE,
    MSG_COME,
} ListenerType;

#define qq_async_enabled(ac) (ac->async!=NULL)
void qq_async_add_listener(qq_account* ac,ListenerType type,QQ_ASYNC_CALLBACK callback,void* data);
//void qq_async_watch(qq_account* ac,ListenerType type);
void qq_async_dispatch(qq_account* lc,ListenerType type,LwqqErrorCode err);
void qq_async_dispatch_2(qq_account* lc,ListenerType type,void* data);
void qq_async_set(qq_account* client,int enabled);

#endif 
