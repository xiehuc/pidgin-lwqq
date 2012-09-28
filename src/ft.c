#include <prpl.h>
#include <ft.h>
#include <string.h>
#include "qq_types.h"
#include "type.h"
#include "msg.h"
#include "async.h"
#include "smemory.h"


static void file_trans_request_denied(PurpleXfer* xfer)
{
}
static void file_trans_on_progress(void* data,size_t now,size_t total)
{
    PurpleXfer* xfer = data;
    purple_xfer_set_size(xfer,total);
    xfer->bytes_sent = now;
    xfer->bytes_remaining = total-now;
    purple_xfer_update_progress(xfer);
    //printf("%d:%d\n",now,total);
}
static void file_trans_complete(LwqqAsyncEvent* event,void* data)
{
    PurpleXfer* xfer = data;
    purple_xfer_set_completed(xfer,1);
}
static void file_trans_init(PurpleXfer* xfer)
{
    void** data = xfer->data;
    LwqqClient* lc = data[0];
    LwqqMsgFileMessage* file = data[1];
    s_free(data);
    const char* filename = purple_xfer_get_local_filename(xfer);
    lwqq_async_add_event_listener(
            lwqq_msg_accept_file(lc,file,filename,file_trans_on_progress,xfer),
            file_trans_complete,xfer);
}
static void file_trans_cancel(PurpleXfer* cancel)
{
}

void file_message(LwqqClient* lc,LwqqMsgFileMessage* file)
{
    if(file->mode != MODE_RECV) return;
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;
    //for(i=0;i<file->file_count;i++){
    PurpleXfer* xfer = purple_xfer_new(account,PURPLE_XFER_RECEIVE,file->from);
    purple_xfer_set_filename(xfer,file->recv.name);
    purple_xfer_set_init_fnc(xfer,file_trans_init);
    purple_xfer_set_request_denied_fnc(xfer,file_trans_request_denied);
    purple_xfer_set_cancel_recv_fnc(xfer,file_trans_cancel);
    LwqqMsgFileMessage* fmsg = s_malloc(sizeof(*fmsg));
    memcpy(fmsg,file,sizeof(*fmsg));
    file->from = file->to = file->reply_ip = file->recv.name = NULL;
    void** data = s_malloc(sizeof(void*)*2);
    data[0] = lc;
    data[1] = fmsg;
    xfer->data = data;
    purple_xfer_request(xfer);
    //purple_xfer_add(xfer);
}
