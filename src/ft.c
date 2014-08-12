#include <prpl.h>
#include <ft.h>
#include <string.h>
#include "qq_types.h"
#include "type.h"
#include "msg.h"
#include "http.h"
#include "async.h"
#include "smemory.h"


static int file_trans_on_progress(void* data,size_t now,size_t total)
{
	PurpleXfer* xfer = data;
	if(purple_xfer_is_canceled(xfer)||purple_xfer_is_completed(xfer)) {
		return 1;
	}
	purple_xfer_set_size(xfer,total);
	xfer->bytes_sent = now;
	xfer->bytes_remaining = total-now;
	qq_dispatch(_C_(p,purple_xfer_update_progress,xfer),10);
	return 0;
}
static void recv_file_complete(PurpleXfer* xfer,LwqqAsyncEvent* ev)
{
	if(ev->result != LWQQ_EC_OK) return;
	qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
	LwqqMsgFileMessage* file = xfer->data;
	char buf[512];
	const char* extra = NULL;
	if(ev->result){
		snprintf(buf,sizeof(buf),_("Transport Failed,Error Code:%d\n"),ev->result);
		if(ev->result == 102)
			extra = _("Other may send via webqq.\nPlease call him send via email or offline file.");
		else extra = NULL;
		purple_notify_error(ac->gc,_("File Transport"),buf,extra);
	}
	purple_xfer_set_completed(xfer,1);
	lwqq_msg_free((LwqqMsg*)file);
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
		purple_xfer_error(PURPLE_XFER_RECEIVE, ac->account, purple_xfer_get_remote_user(xfer), _("Receive file failed"));
		purple_xfer_cancel_local(xfer);
		return;
	}
	LwqqHttpRequest* req = lwqq_async_event_get_conn(ev);
	lwqq_http_on_progress(req, file_trans_on_progress, xfer);
	lwqq_http_set_option(req, LWQQ_HTTP_CANCELABLE,1L);
	lwqq_async_add_event_listener(ev,_C_(2p,recv_file_complete,xfer,ev));
}
static void recv_file_request_denied(PurpleXfer* xfer)
{
	qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
	LwqqClient* lc = ac->qq;
	LwqqMsgFileMessage* file = xfer->data;
	lwqq_msg_refuse_file(lc, file);
	lwqq_msg_free((LwqqMsg*)file);
}
static void recv_file_cancel(PurpleXfer* xfer)
{
	LwqqMsg* msg = xfer->data;
	if(!msg) return;
	if(lwqq_mt_bits(msg->type)==LWQQ_MT_FILE_MSG){
		LwqqMsgFileMessage* file = (LwqqMsgFileMessage*)msg;
		lwqq_http_cancel(file->req);
		lwqq_msg_free((LwqqMsg*)file);
	}else if(lwqq_mt_bits(msg->type)==LWQQ_MT_OFFFILE){
		LwqqMsgOffFile* file = (LwqqMsgOffFile*)msg;
		lwqq_http_cancel(file->req);
		lwqq_msg_free((LwqqMsg*)file);
	}else{
		lwqq_log(LOG_ERROR,"msg file cast failed");
	}
}

static void send_offline_file_receipt(LwqqAsyncEvent* ev,PurpleXfer* xfer)
{
	int err = ev->result;
	qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
	LwqqMsgOffFile* file = xfer->data;

	if(err == 0){
		qq_sys_msg_write(ac, LWQQ_MS_BUDDY_MSG, file->super.to, _("Send offline file successful"), PURPLE_MESSAGE_SYSTEM, time(NULL));
	}else{
		char buf[512];
		snprintf(buf,sizeof(buf),_("Send offline file failed,Error Code:%d"),err);
		qq_sys_msg_write(ac, LWQQ_MS_BUDDY_MSG, file->super.to, buf, PURPLE_MESSAGE_ERROR, time(NULL));
	}
	lwqq_msg_free((LwqqMsg*)file);
	purple_xfer_set_completed(xfer,1);
}

static void send_file(LwqqAsyncEvent* event,PurpleXfer *xfer)
{
	//now xfer is not valid.
	if(event->result != LWQQ_EC_OK) 
		goto done;

	qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
	LwqqClient* lc = ac->qq;
	long err = 0;
	err = event->result;
	LwqqMsgOffFile* file = xfer->data;
	if(err) {
		qq_sys_msg_write(ac,LWQQ_MS_BUDDY_MSG, file->super.to,_("Send offline file failed"),PURPLE_MESSAGE_ERROR,time(NULL));
		lwqq_msg_free((LwqqMsg*)file);
		purple_xfer_set_completed(xfer,1);
	} else {
		LwqqAsyncEvent* ev = lwqq_msg_send_offfile(lc,file);
		lwqq_async_add_event_listener(ev,_C_(2p,send_offline_file_receipt,ev,xfer));
	}
	return;
done:
	lwqq_msg_free((LwqqMsg*)xfer->data);
	purple_xfer_set_completed(xfer, 1);
	return;
}
static void upload_offline_file_init(PurpleXfer* xfer)
{
	qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(xfer->account));
	LwqqClient* lc = ac->qq;

	const char* serv_id;
	if(ac->flag&QQ_USE_QQNUM){
		const char* qqnum = purple_xfer_get_remote_user(xfer);
		LwqqBuddy* b = find_buddy_by_qqnumber(ac->qq, qqnum);
		if(b == NULL) return;
		serv_id = b->uin;
	}else{
		serv_id = purple_xfer_get_remote_user(xfer);
	}

	LwqqMsgOffFile* file = lwqq_msg_fill_upload_offline_file(
			xfer->local_filename, lc->myself->uin, serv_id);
	xfer->start_time = time(NULL);
	xfer->data = file;
	int flags = 0;
	if(ac->flag&QQ_DONT_EXPECT_100_CONTINUE) flags |= DONT_EXPECTED_100_CONTINUE;
	LwqqAsyncEvent* ev = lwqq_msg_upload_offline_file(lc,file,flags);
	lwqq_async_add_event_listener(ev,_C_(2p,send_file,ev,xfer));
	LwqqHttpRequest* req = lwqq_async_event_get_conn(ev);
	lwqq_http_on_progress(req, file_trans_on_progress, xfer);
	lwqq_http_set_option(req, LWQQ_HTTP_CANCELABLE,1L);
}
static void upload_file_init(PurpleXfer* xfer)
{
	void** data = xfer->data;
	qq_account* ac = data[0];
	LwqqClient* lc = ac->qq;
	LwqqMsgOffFile* file = s_malloc0(sizeof(*file));
	file->super.from = s_strdup(lc->myself->uin);
	file->super.to = s_strdup(purple_xfer_get_remote_user(xfer));
	file->name = s_strdup(purple_xfer_get_local_filename(xfer));
	xfer->start_time = time(NULL);
	data[1] = file;
	data[2] = xfer;
	//lwqq_async_add_event_listener(
	//background_upload_file(lc,file,file_trans_on_progress,xfer);
	//send_file,data);
}

void qq_send_file(PurpleConnection* gc,const char* who,const char* filename)
{
	qq_account* ac = purple_connection_get_protocol_data(gc);
	if(!(ac->flag&DEBUG_FILE_SEND)){
		//purple_notify_warning(gc,NULL,"难题尚未攻破,曙光遥遥无期","请先用离线文件传输");
		PurpleXfer* xfer = purple_xfer_new(ac->account,PURPLE_XFER_SEND,who);
		purple_xfer_set_init_fnc(xfer,upload_offline_file_init);
		purple_xfer_set_request_denied_fnc(xfer,recv_file_request_denied);
		purple_xfer_set_cancel_send_fnc(xfer,recv_file_cancel);
		if(filename)
			purple_xfer_request_accepted(xfer, filename);
		else
			purple_xfer_request(xfer);
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
	PurpleXfer* xfer = purple_xfer_new(account,PURPLE_XFER_SEND,buddy->name);
	purple_xfer_set_init_fnc(xfer,upload_offline_file_init);
	purple_xfer_set_request_denied_fnc(xfer,recv_file_request_denied);
	purple_xfer_set_cancel_send_fnc(xfer,recv_file_cancel);
	purple_xfer_request(xfer);
}
//the entrience for recv file messages
void file_message(LwqqClient* lc,LwqqMsgFileMessage* file)
{
	qq_account* ac = lwqq_client_userdata(lc);
	if(file->mode == MODE_RECV) {
		PurpleAccount* account = ac->account;
		LwqqClient* lc = ac->qq;
		LwqqBuddy* buddy = lc->find_buddy_by_uin(lc,file->super.from);
		if(buddy == NULL ) return;
		const char* key = try_get(buddy->qqnumber,buddy->uin);
		PurpleXfer* xfer = purple_xfer_new(account,PURPLE_XFER_RECEIVE,key);
		purple_xfer_set_filename(xfer,file->recv.name);
		purple_xfer_set_init_fnc(xfer,recv_file_init);
		purple_xfer_set_request_denied_fnc(xfer,recv_file_request_denied);
		purple_xfer_set_cancel_recv_fnc(xfer,recv_file_cancel);
		LwqqMsgFileMessage* fdup = s_malloc0(sizeof(*fdup));
		lwqq_msg_move(fdup,file);
		xfer->data = fdup;
		purple_xfer_request(xfer);
	} else if(file->mode == MODE_REFUSE) {
		if(file->refuse.cancel_type == CANCEL_BY_USER) 
			qq_sys_msg_write(ac, LWQQ_MS_BUDDY_MSG, file->super.from, 
					_("Other canceled file transport"), 
					PURPLE_MESSAGE_SYSTEM, 
					time(NULL));
		else if(file->refuse.cancel_type == CANCEL_BY_OVERTIME) 
			qq_sys_msg_write(ac, LWQQ_MS_BUDDY_MSG, file->super.from, 
					_("File transport timeout"), 
					PURPLE_MESSAGE_SYSTEM, 
					time(NULL));
	}
}

// vim: tabstop=3 sw=3 sts=3 noexpandtab
