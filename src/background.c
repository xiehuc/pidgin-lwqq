#include "qq_types.h"

#include <login.h>
#include <async.h>
#include <info.h>
#include <msg.h>

#define START_THREAD(thread,data)\
do{pthread_t th;\
    pthread_create(&th,NULL,thread,data);\
}while(0)

typedef struct {
    LwqqClient* lc;
    union{
        LwqqBuddy* buddy;
        LwqqGroup* group;
    };
}ThreadFuncPar;

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
static int friend_ref = 0;
static void get_buddy_qqnumber_thread(gpointer data,gpointer userdata)
{
    ThreadFuncPar* par = data;
    LwqqClient* lc = par->lc;
    LwqqBuddy* buddy = par->buddy;
    LwqqErrorCode err;
    g_slice_free(ThreadFuncPar,par);

    s_free(buddy->qqnumber);

    buddy->qqnumber = lwqq_info_get_friend_qqnumber(lc,buddy->uin);
    lwqq_info_get_friend_avatar(lc,buddy,&err);

    lwqq_async_dispatch(lc,FRIEND_COME,buddy);
    friend_ref--;
    if(friend_ref==0)
        lwqq_async_dispatch(lc,FRIENDS_ALL_COMPLETE,NULL);
}
static int group_ref = 0;
static void get_group_qqnumber_thread(gpointer data,gpointer userdata)
{
    ThreadFuncPar* par = data;
    LwqqClient* lc = par->lc;
    LwqqGroup* group = par->group;
    LwqqErrorCode err;
    g_slice_free(ThreadFuncPar,par);

    s_free(group->account);

    group->account = lwqq_info_get_friend_qqnumber(lc,group->gid);
    lwqq_info_get_group_avatar(lc,group,&err);

    lwqq_async_dispatch(lc,GROUP_COME,group);
    group_ref--;
    if(group_ref==0)
        lwqq_async_dispatch(lc,GROUPS_ALL_COMPLETE,NULL);
}

static void* _background_friends_info(void* data)
{
    qq_account* ac=(qq_account*)data;
    LwqqErrorCode err;
    LwqqClient* lc=ac->qq;

    lwqq_info_get_friends_info(lc,&err);
    lwqq_info_get_online_buddies(ac->qq,&err);

    GThreadPool* thread_pool = g_thread_pool_new(
            get_buddy_qqnumber_thread,NULL,50,TRUE,NULL);
    ThreadFuncPar* par = NULL;
    LwqqBuddy* buddy;

    LIST_FOREACH(buddy,&lc->friends,entries){
        par = g_slice_new0(ThreadFuncPar);
        par->lc = lc;
        par->buddy = buddy;
        friend_ref++;
        g_thread_pool_push(thread_pool,(gpointer)par,NULL);
    }
    g_thread_pool_free(thread_pool,0,0);

    lwqq_info_get_group_name_list(ac->qq,&err);

    LwqqGroup* group;
    thread_pool = g_thread_pool_new(
            get_group_qqnumber_thread,NULL,10,TRUE,NULL);
    LIST_FOREACH(group,&lc->groups,entries){
        par = g_slice_new0(ThreadFuncPar);
        par->lc = lc;
        par->group = group;
        group_ref++;
        g_thread_pool_push(thread_pool,(gpointer)par,NULL);
    }
    g_thread_pool_free(thread_pool,0,0);

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
    LwqqClient* lc = ac->qq;
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;
    LwqqErrorCode err;

    /* Poll to receive message */
    l->poll_msg(l);

    msg_check_handle = purple_timeout_add(200,msg_check,ac);
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
    l->tid = NULL;
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
}
void background_group_detail(qq_account* ac,LwqqGroup* group)
{
    if(!LIST_EMPTY(&group->members)){
        LwqqClient* lc = ac->qq;
        lwqq_async_dispatch(lc,GROUP_DETAIL,group);
        return;
    }
    void** data = malloc(sizeof(void*)*2);
    data[0] = ac;
    data[1] = group;
    START_THREAD(_background_group_detail,data);
}
