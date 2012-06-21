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
typedef void (*ASYNC_CALLBACK)(qq_account* lc,LwqqErrorCode err,void* data);

typedef enum ListenerType{
    LOGIN_COMPLETE,
    VERIFY_COME,
    FRIENDS_COMPLETE,
    GROUPS_COMPLETE,
    MSG_COME,
    ONLINE_COME,
} ListenerType;

void qq_async_add_listener(qq_account* ac,ListenerType type,ASYNC_CALLBACK callback,void* data);
//void qq_async_watch(qq_account* ac,ListenerType type);
void qq_async_dispatch(qq_account* lc,ListenerType type,LwqqErrorCode err);
void qq_async_set(qq_account* client,int enabled);
#define qq_async_enabled(ac) (ac->async!=NULL)
#endif 
