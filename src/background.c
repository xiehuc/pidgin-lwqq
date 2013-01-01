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

#if 0
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
        lc->dispatch(vp_func_pi,(CALLBACK_FUNC)extra_async_opt.need_verify,lc,err);
    }else{
        lc->dispatch(vp_func_pi,(CALLBACK_FUNC)extra_async_opt.login_complete,lc,err);
    }
    return NULL;
}


void background_login(qq_account* ac)
{
    START_THREAD(_background_login,ac);
}
#endif

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
