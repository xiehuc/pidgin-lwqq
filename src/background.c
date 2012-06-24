#include "qq_types.h"

#include <login.h>
#include <async.h>

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
    }else{
        lwqq_async_dispatch(lc,LOGIN_COMPLETE,NULL);
    }
    return NULL;
}


void background_login(qq_account* ac){
    START_THREAD(_background_login,ac);
}

static void* _background_friends_info(void* data)
{
    qq_account* ac=(qq_account*)data;
    LwqqErrorCode err;
    LwqqClient* lc=ac->qq;

    lwqq_info_get_friends_info(lc,&err);
    lwqq_info_get_online_buddies(ac->qq,&err);
    //lwqq_info_get_all_friend_qqnumbers(lc,&err);
    //qq_fake_get_all_friend_qqnumbers(ac,&err);
    lwqq_async_dispatch(ac->qq,FRIENDS_ALL_COMPLETE,NULL);

}
void background_friends_info(qq_account* ac)
{
    START_THREAD(_background_friends_info,ac);
}

static void* _background_msg_poll(void* data)
{
    qq_account* ac = (qq_account*)data;
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;
    LwqqErrorCode err;

    /* Poll to receive message */
    l->poll_msg(l);

    /* Need to wrap those code so look like more nice */
    while (1) {
        sleep(1);
        LwqqRecvMsg *msg;
        if (!SIMPLEQ_EMPTY(&l->head)) {
            lwqq_async_dispatch(ac->qq,MSG_COME,NULL);
        }
    }
}
static pthread_t msg_th;
void background_msg_poll(qq_account* ac)
{
    pthread_create(&msg_th,NULL,_background_msg_poll,ac);
}
void background_msg_drain(qq_account* ac)
{
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;
    //l->poll_close(l);
    pthread_cancel(msg_th);
}
