#include "qq_types.h"

#include <login.h>
#include <async.h>
#include <info.h>
#include <msg.h>
//#include <regex.h>
#include <string.h>

#define START_THREAD(thread,data)\
do{pthread_t th;\
    pthread_create(&th,NULL,thread,data);\
}while(0)


static void* _background_login(void* data)
{
    qq_account* ac=(qq_account*)data;
    LwqqClient* lc = ac->qq;
    LwqqErrorCode err;

    lwqq_login(lc, &err);
    lwqq_async_set_error(lc,VERIFY_COME,err);
    if (err == LWQQ_EC_LOGIN_NEED_VC) {
        lwqq_async_dispatch(lc,VERIFY_COME,NULL);
    } else {
        lwqq_async_dispatch(lc,LOGIN_COMPLETE,NULL);
    }
    return NULL;
}


void background_login(qq_account* ac)
{
    START_THREAD(_background_login,ac);
}
static void* _background_friends_info(void* data)
{
    qq_account* ac=(qq_account*)data;
    LwqqErrorCode err;
    LwqqClient* lc=ac->qq;

    LwqqAsyncLock* lock = lwqq_async_lock_new();
    LwqqAsyncEvent* event;
    event = lwqq_info_get_friends_info(lc,&err);
    lwqq_async_lock_add_event(lock,event);
    event = lwqq_info_get_group_name_list(ac->qq,&err);
    lwqq_async_lock_add_event(lock,event);
    lwqq_async_wait(lock);

    lock = lwqq_async_lock_new();
    event = lwqq_info_get_online_buddies(ac->qq,&err);
    lwqq_async_lock_add_event(lock,event);
    lwqq_async_wait(lock);


    lwqq_info_get_friend_detail_info(lc,lc->myself,&err);


    lwqq_info_get_all_friend_qqnumbers(lc,&err);

    LwqqGroup* group;
    LIST_FOREACH(group,&ac->qq->groups,entries) {
        lwqq_info_get_friend_qqnumber(lc,group->gid);
    }

    qq_set_basic_info(ac);

    return NULL;
}
void background_friends_info(qq_account* ac)
{
    START_THREAD(_background_friends_info,ac);
}
static int msg_check_repeat = 1;
static gboolean msg_check(void* data)
{
    qq_msg_check((qq_account*)data);
    //repeat for ever;
    return msg_check_repeat;
}
static int msg_check_handle;
static pthread_t tid = 0;
static void* _background_msg_poll(void* data)
{
    qq_account* ac = (qq_account*)data;
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;

    /* Poll to receive message */
    l->poll_msg(l);

    msg_check_handle = purple_timeout_add(200,msg_check,ac);

    return NULL;
}
static pthread_t msg_th;
void background_msg_poll(qq_account* ac)
{
    pthread_create(&msg_th,NULL,_background_msg_poll,ac);
}
void background_msg_drain(qq_account* ac)
{
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;
    purple_timeout_remove(msg_check_handle);
    pthread_cancel(l->tid);
    l->tid = 0;
    tid = 0;
}

static void* _background_group_detail(void* d)
{
    void** data = (void**)d;
    qq_account* ac = data[0];
    LwqqClient* lc = ac->qq;
    LwqqGroup* group = data[1];
    free(data);
    LwqqErrorCode err;
    lwqq_info_get_group_detail_info(lc,group,&err);
    lwqq_async_dispatch(lc,GROUP_DETAIL,group);

    return NULL;
}
void background_group_detail(qq_account* ac,LwqqGroup* group)
{
    if(!LIST_EMPTY(&group->members)) {
        LwqqClient* lc = ac->qq;
        lwqq_async_dispatch(lc,GROUP_DETAIL,group);
        return;
    }
    void** data = malloc(sizeof(void*)*2);
    data[0] = ac;
    data[1] = group;
    START_THREAD(_background_group_detail,data);
}

