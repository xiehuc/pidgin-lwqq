#include <prpl.h>
#include <ft.h>
#include <string.h>
#include "qq_types.h"
#include "type.h"
#include "msg.h"
#include "async.h"
#include "smemory.h"
#include "background.h"


static void recv_file_request_denied(PurpleXfer* xfer)
{
}
static int file_trans_on_progress(void* data,size_t now,size_t total)
{
    PurpleXfer* xfer = data;
    if(purple_xfer_is_canceled(xfer)||purple_xfer_is_completed(xfer)) {
        return 1;
    }
    purple_xfer_set_size(xfer,total);
    xfer->bytes_sent = now;
    xfer->bytes_remaining = total-now;
    qq_dispatch(vp_func_p,(CALLBACK_FUNC)purple_xfer_update_progress,xfer);
    return 0;
}
static void recv_file_complete(PurpleXfer* xfer)
{
    purple_xfer_set_completed(xfer,1);
}
static void recv_file_init(PurpleXfer* xfer)
{
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
    LwqqClient* lc = ac->qq;
    LwqqMsgFileMessage* file = xfer->data;
    const char* filename = purple_xfer_get_local_filename(xfer);
    xfer->start_time = time(NULL);
    LwqqAsyncEvent* ev = lwqq_msg_accept_file(lc,file,filename);
    if(ev == NULL){ 
        lwqq_puts("file trans error ");
        purple_xfer_error(PURPLE_XFER_RECEIVE, ac->account, purple_xfer_get_remote_user(xfer), "接受文件失败");
        purple_xfer_cancel_local(xfer);
        return;
    }
    lwqq_async_event_set_progress(ev,file_trans_on_progress,xfer);
    lwqq_async_add_event_listener(ev,_C_(p,recv_file_complete,xfer));
}
static void recv_file_cancel(PurpleXfer* xfer)
{
}

void file_message(LwqqClient* lc,LwqqMsgFileMessage* file)
{
    qq_account* ac = lwqq_client_userdata(lc);
    if(file->mode == MODE_RECV) {
        PurpleAccount* account = ac->account;
        LwqqClient* lc = ac->qq;
        //for(i=0;i<file->file_count;i++){
        LwqqBuddy* buddy = lc->find_buddy_by_uin(lc,file->from);
        if(buddy == NULL ) return;
        const char* key = try_get(buddy->qqnumber,buddy->uin);
        PurpleXfer* xfer = purple_xfer_new(account,PURPLE_XFER_RECEIVE,key);
        purple_xfer_set_filename(xfer,file->recv.name);
        purple_xfer_set_init_fnc(xfer,recv_file_init);
        purple_xfer_set_request_denied_fnc(xfer,recv_file_request_denied);
        purple_xfer_set_cancel_recv_fnc(xfer,recv_file_cancel);
        LwqqMsgFileMessage* fdup = s_malloc(sizeof(*fdup));
        memcpy(fdup,file,sizeof(*fdup));
        file->from = file->to = file->reply_ip = file->recv.name = NULL;
        xfer->data = fdup;
        purple_xfer_request(xfer);
    } else if(file->mode == MODE_REFUSE) {
        if(file->refuse.cancel_type == CANCEL_BY_USER) {
            qq_sys_msg_write(ac, LWQQ_MT_BUDDY_MSG, file->from, "对方取消文件传输", PURPLE_MESSAGE_SYSTEM, time(NULL));
        } else if(file->refuse.cancel_type == CANCEL_BY_OVERTIME) {
            qq_sys_msg_write(ac, LWQQ_MT_BUDDY_MSG, file->from, "文件传输超时", PURPLE_MESSAGE_SYSTEM, time(NULL));
        }

    }

}

static void send_offline_file_receipt(LwqqAsyncEvent* ev,PurpleXfer* xfer)
{
    int errno = lwqq_async_event_get_result(ev);
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
    LwqqMsgOffFile* file = xfer->data;

    if(errno == 0){
        qq_sys_msg_write(ac, LWQQ_MT_BUDDY_MSG, file->to, "发送离线文件成功", PURPLE_MESSAGE_SYSTEM, time(NULL));
    }else{
        qq_sys_msg_write(ac, LWQQ_MT_BUDDY_MSG, file->to, "发送离线文件失败", PURPLE_MESSAGE_ERROR, time(NULL));
    }
    lwqq_msg_offfile_free(file);
    purple_xfer_set_completed(xfer,1);
}

static void send_file(LwqqAsyncEvent* event,PurpleXfer *xfer)
{
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
    LwqqClient* lc = ac->qq;
    long errno = 0;
    if(lwqq_async_event_get_code(event)==LWQQ_CALLBACK_FAILED){
        s_free(xfer->data);
        return;
    }
    errno = lwqq_async_event_get_result(event);
    LwqqMsgOffFile* file = xfer->data;
    //purple_xfer_unref(xfer);
    if(errno) {
        qq_sys_msg_write(ac,LWQQ_MT_BUDDY_MSG, file->to,"上传空间不足",PURPLE_MESSAGE_ERROR,time(NULL));
        lwqq_msg_offfile_free(file);
        s_free(xfer->data);
        purple_xfer_set_completed(xfer,1);
    } else {
        LwqqAsyncEvent* ev = lwqq_msg_send_offfile(lc,file);
        lwqq_async_add_event_listener(ev,_C_(2p,send_offline_file_receipt,ev,xfer));
    }
}
static void upload_offline_file_init(PurpleXfer* xfer)
{
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
    LwqqClient* lc = ac->qq;
    LwqqMsgOffFile* file = lwqq_msg_fill_upload_offline_file(
            xfer->local_filename, lc->myself->uin, purple_xfer_get_remote_user(xfer));
    xfer->start_time = time(NULL);
    xfer->data = file;
    LwqqAsyncEvent* ev = lwqq_msg_upload_offline_file(lc,file);
    lwqq_async_add_event_listener(ev,_C_(2p,send_file,ev,xfer));
    lwqq_async_event_set_progress(ev, file_trans_on_progress, xfer);
}
static void upload_file_init(PurpleXfer* xfer)
{
    void** data = xfer->data;
    qq_account* ac = data[0];
    LwqqClient* lc = ac->qq;
    LwqqMsgOffFile* file = s_malloc0(sizeof(*file));
    file->from = s_strdup(lc->myself->uin);
    file->to = s_strdup(purple_xfer_get_remote_user(xfer));
    file->name = s_strdup(purple_xfer_get_local_filename(xfer));
    xfer->start_time = time(NULL);
    data[1] = file;
    data[2] = xfer;
    //lwqq_async_add_event_listener(
    background_upload_file(lc,file,file_trans_on_progress,xfer);
    //send_file,data);
}

void qq_send_file(PurpleConnection* gc,const char* who,const char* filename)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    if(!ac->debug_file_send){
        purple_notify_warning(gc,NULL,"难题尚未攻破,曙光遥遥无期","请先用离线文件传输");
        return;
    }
    PurpleAccount* account = ac->account;
    PurpleXfer* xfer = purple_xfer_new(account,PURPLE_XFER_SEND,who);
    purple_xfer_set_init_fnc(xfer,upload_file_init);
    //purple_xfer_set_init_fnc(xfer,upload_offline_file_init);
    //purple_xfer_set_request_denied_fnc(xfer,file_trans_request_denied);
    //purple_xfer_set_cancel_send_fnc(xfer,file_trans_cancel);
    void** data = s_malloc(sizeof(void*)*3);
    data[0] = ac;
    xfer->data = data;
    purple_xfer_request(xfer);
}

void qq_send_offline_file(PurpleBlistNode* node)
{
    PurpleBuddy* buddy = PURPLE_BUDDY(node);
    PurpleAccount* account = purple_buddy_get_account(buddy);
    qq_account* ac = purple_connection_get_protocol_data(
                         purple_account_get_connection(account));
    const char* who;
    if(ac->qq_use_qqnum){
        const char* qqnum = purple_buddy_get_name(buddy);
        LwqqBuddy* b = find_buddy_by_qqnumber(ac->qq, qqnum);
        if(b == NULL) return;
        who = b->uin;
    }else{
        who = purple_buddy_get_name(buddy);
    }
    PurpleXfer* xfer = purple_xfer_new(account,PURPLE_XFER_SEND,who);
    purple_xfer_set_init_fnc(xfer,upload_offline_file_init);
    purple_xfer_set_request_denied_fnc(xfer,recv_file_request_denied);
    purple_xfer_set_cancel_send_fnc(xfer,recv_file_cancel);
    purple_xfer_request(xfer);
}
