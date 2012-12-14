#include "qq_types.h"

#include <login.h>
#include <async.h>
#include <info.h>
#include <msg.h>
#include <smemory.h>
#include <string.h>

#include "translate.h"

#define START_THREAD(thread,data)\
do{pthread_t th;\
    pthread_create(&th,NULL,thread,data);\
}while(0)


static void* _background_login(void* data)
{
    qq_account* ac=(qq_account*)data;
    if(!qq_account_valid(ac)) return NULL;
    LwqqClient* lc = ac->qq;
    LwqqErrorCode err;
    //it would raise a invalid ac when wake up from sleep.
    //it would login twice,why? 
    //so what i can do is disable the invalid one.
    if(!lwqq_client_valid(lc)) return NULL;
    const char* status = purple_status_get_id(purple_account_get_active_status(ac->account));

    lwqq_login(lc,lwqq_status_from_str(status), &err);

    if (err == LWQQ_EC_LOGIN_NEED_VC) {
        lc->dispatch(lc,lwqq_func_1_int,extra_async_opt.need_verify,err);
    }else{
        lc->dispatch(lc,lwqq_func_1_int,extra_async_opt.login_complete,err);
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

    LWQQ_SYNC_BEGIN();
    lwqq_info_get_friends_info(lc,&err);
    lwqq_info_get_group_name_list(ac->qq,&err);
    LWQQ_SYNC_END();

    LWQQ_SYNC_BEGIN();
    lwqq_info_get_online_buddies(ac->qq,&err);
    lwqq_info_get_discu_name_list(ac->qq);
    LWQQ_SYNC_END();

    lwqq_info_get_friend_detail_info(lc,lc->myself,NULL);

    lc->dispatch(lc,lwqq_func_1_pointer,qq_set_basic_info,ac);

    return NULL;
}
void background_friends_info(qq_account* ac)
{
    START_THREAD(_background_friends_info,ac);
}
void background_msg_poll(qq_account* ac)
{
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;

    /* Poll to receive message */
    l->poll_msg(l);
}
void background_msg_drain(qq_account* ac)
{
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;
    if(l->tid){
        pthread_cancel(l->tid);
        l->tid = 0;
    }
    /*purple_timeout_remove(msg_check_handle);
    tid = 0;*/
}

static void* _background_upload_file(void* d)
{
    void **data = d;
    LwqqClient* lc = data[0];
    LwqqMsgOffFile* file = data[1];
    LWQQ_PROGRESS progress = data[2];
    PurpleXfer* xfer = data[3];
    s_free(d);
    lwqq_msg_upload_file(lc,file,progress,xfer);
    lwqq_puts("quit upload file");
    return NULL;
}

void background_upload_file(LwqqClient* lc,LwqqMsgOffFile* file,LWQQ_PROGRESS file_trans_on_progress,PurpleXfer* xfer)
{
    void** data = s_malloc(sizeof(void*)*4);
    data[0] = lc;
    data[1] = file;
    data[2] = file_trans_on_progress;
    data[3] = xfer;
    START_THREAD(_background_upload_file,data);
}
