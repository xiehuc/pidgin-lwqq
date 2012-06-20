#include "qq_types.h"
#include "qq_async.h"

#include <login.h>

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
    if (err == LWQQ_EC_LOGIN_NEED_VC) {
        qq_async_dispatch(ac,VERIFY_COME,err);
    }else{
        qq_async_dispatch(ac,LOGIN_COMPLETE,err);
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

    lwqq_info_get_group_name_list(lc,&err);
    qq_async_dispatch(ac,GROUPS_COMPLETE,err);

    lwqq_info_get_friends_info(lc,&err);
    lwqq_info_get_all_friend_qqnumbers(lc,err);
    qq_async_dispatch(ac,FRIENDS_COMPLETE,err);
}
void background_friends_info(qq_account* ac)
{
    START_THREAD(_background_friends_info,ac);
}

void* _background_msg_poll(void* data)
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
            qq_async_dispatch(ac,MSG_COME,err);
        }
    }
}
void background_msg_poll(qq_account* ac)
{
    START_THREAD(_background_msg_poll,ac);
}
