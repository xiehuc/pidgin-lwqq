#include <prpl.h>
#include <ft.h>
#include <string.h>
#include <unistd.h>

#include "qq_types.h"

#if 0
static void recv_file_request_denied(PurpleXfer* xfer)
{
   qq_account* ac = purple_connection_get_protocol_data(
       purple_account_get_connection(xfer->account));
   LwqqClient* lc = ac->qq;
   LwqqMsgFileMessage* file = xfer->data;
   lwqq_msg_refuse_file(lc, file);
   lwqq_msg_free((LwqqMsg*)file);
}

static void send_offline_file_receipt(LwqqAsyncEvent* ev, PurpleXfer* xfer)
{
   int err = ev->result;
   qq_account* ac = purple_connection_get_protocol_data(
       purple_account_get_connection(xfer->account));
   LwqqMsgOffFile* file = xfer->data;

   if (err == 0) {
      qq_sys_msg_write(ac, LWQQ_MS_BUDDY_MSG, file->super.to,
                       _("Send offline file successful"), PURPLE_MESSAGE_SYSTEM,
                       time(NULL));
   } else {
      char buf[512];
      snprintf(buf, sizeof(buf), _("Send offline file failed,Error Code:%d"),
               err);
      qq_sys_msg_write(ac, LWQQ_MS_BUDDY_MSG, file->super.to, buf,
                       PURPLE_MESSAGE_ERROR, time(NULL));
   }
   lwqq_msg_free((LwqqMsg*)file);
   purple_xfer_set_completed(xfer, 1);
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
   // lwqq_async_add_event_listener(
   // background_upload_file(lc,file,file_trans_on_progress,xfer);
   // send_file,data);
}


// the entrience for recv file messages
void file_message(LwqqClient* lc, LwqqMsgFileMessage* file)
{
}
#endif

static int file_trans_on_progress(void* data, size_t now, size_t total)
{
   PurpleXfer* xfer = data;
   if (purple_xfer_is_canceled(xfer) || purple_xfer_is_completed(xfer)) {
      return 1;
   }
   purple_xfer_set_size(xfer, total);
   xfer->bytes_sent = now;
   xfer->bytes_remaining = total - now;
   //qq_dispatch(_C_(p, purple_xfer_update_progress, xfer), 10);
   purple_xfer_update_progress(xfer);
   return 0;
}

static void upload_file_to_server(PurpleXfer* xfer)
{
   qq_account* ac = purple_connection_get_protocol_data(
       purple_account_get_connection(xfer->account));
   LwqqClient* lc = ac->qq;

   const char* user = purple_xfer_get_remote_user(xfer);
   LwqqMsgOffFile* file = lwqq_msg_fill_upload_offline_file(
       xfer->local_filename, lc->myself->uin, user);

   xfer->data = file;
   xfer->start_time = time(NULL);
   LwqqHttpRequest* req = lwqq_http_request_new(ac->settings.upload_server);
   file->req = req;
   req->lc = lc;
   req->add_form(req, LWQQ_FORM_CONTENT, "user", user);
   req->add_form(req, LWQQ_FORM_FILE, "fname", xfer->local_filename);
   lwqq_http_on_progress(req, file_trans_on_progress, xfer);
   lwqq_http_set_option(req, LWQQ_HTTP_CANCELABLE, 1L);
   req->do_request_async(req, 0, "", _C_(2p, send_file_message, req, xfer));
}
static void download_file_finish(PurpleXfer* xfer, LwqqHttpRequest* req)
{
   if(req->err != LWQQ_EC_CANCELED){
      fclose(xfer->dest_fp);
      purple_xfer_set_completed(xfer, 1);
   }
   lwqq_http_request_free(req);
}
static void download_file_from_server(PurpleXfer* xfer)
{
   qq_account* ac = purple_connection_get_protocol_data(
       purple_account_get_connection(xfer->account));
   xfer->start_time = time(NULL);
   LwqqHttpRequest* req = lwqq_http_request_new(xfer->message);
   xfer->data = req;
   req->lc = ac->qq;
   lwqq_http_on_progress(req, file_trans_on_progress, xfer);
   lwqq_http_set_option(req, LWQQ_HTTP_CANCELABLE, 1L);
   xfer->dest_fp = fopen(xfer->local_filename, "w");
   lwqq_http_set_option(req, LWQQ_HTTP_SAVE_FILE, xfer->dest_fp);
   req->do_request_async(req,0,"", _C_(2p, download_file_finish, xfer, req));
}

static void cancel_upload(PurpleXfer* xfer)
{
   LwqqMsgOffFile* file = xfer->data;
   lwqq_http_cancel(file->req);
}

static void cancel_download(PurpleXfer* xfer)
{
   if (xfer->data)
      lwqq_http_cancel(xfer->data);
}

void qq_send_file(PurpleConnection* gc, const char* who, const char* filename)
{
   PurpleXfer* xfer = purple_xfer_new(gc->account, PURPLE_XFER_SEND, who);
   purple_xfer_set_init_fnc(xfer, upload_file_to_server);
   purple_xfer_set_request_denied_fnc(xfer, NULL); // send offline file to server, would not denied
   purple_xfer_set_cancel_send_fnc(xfer, cancel_upload);
   purple_xfer_request(xfer);
}

void qq_send_offline_file(PurpleBlistNode* node)
{
   PurpleChat* chat = PURPLE_CHAT(node);
   PurpleAccount* account = purple_chat_get_account(chat);
   PurpleXfer* xfer = purple_xfer_new(account, PURPLE_XFER_SEND, get_name_from_chat(chat));
   purple_xfer_set_init_fnc(xfer, upload_file_to_server);
   purple_xfer_set_request_denied_fnc(xfer, NULL);
   purple_xfer_set_cancel_send_fnc(xfer, cancel_upload);
   purple_xfer_request(xfer);
}

void qq_ask_download_file(qq_account* ac, LwqqMsgContent* C, const char* local_id)
{
   PurpleXfer* xfer = purple_xfer_new(ac->account, PURPLE_XFER_RECEIVE, local_id);
   xfer->data = NULL;
   purple_xfer_set_filename(xfer, C->data.ext.param[1]);
   purple_xfer_set_message(xfer, C->data.ext.param[0]);
   purple_xfer_set_size(xfer, s_atol(C->data.ext.param[2], 0));
   purple_xfer_set_init_fnc(xfer, download_file_from_server);
   purple_xfer_set_request_denied_fnc(xfer, cancel_download);
   purple_xfer_set_cancel_recv_fnc(xfer, cancel_download);
   purple_xfer_request(xfer);
}

static void set_img_url(LwqqHttpRequest* req, LwqqMsgContent* C, void* data)
{
   char buffer[1024];
   snprintf(buffer,sizeof(buffer),"%s ", req->response); // append a blank to url
   C->data.ext.param[0] = s_strdup(buffer);
   s_free(data);
   lwqq_http_request_free(req);
}

static void replace_img_ext(LwqqHttpRequest* req, LwqqMsgContent* C)
{
   char* name = s_strdup(C->data.ext.name);
   lwqq_msg_content_clean(C);
   C->type = LWQQ_CONTENT_OFFPIC;
   C->data.img.name = name;
   C->data.img.data = req->response;
   C->data.img.size = req->resp_len;
   C->data.img.url = s_strdup(name);
   req->response = NULL;
   lwqq_http_request_free(req);
}

LwqqAsyncEvent* upload_image_to_server(qq_account* ac, PurpleStoredImage* img, LwqqMsgContent** Content)
{
   const char* name = purple_imgstore_get_filename(img);
   LwqqMsgContent* C = LWQQ_CONTENT_EXT_IMG(name);
   LwqqHttpRequest* req
       = lwqq_http_request_new(ac->settings.upload_server);
   req->lc = ac->qq;
   size_t len = purple_imgstore_get_size(img);
   void* buffer = s_malloc(len);
   memcpy(buffer, purple_imgstore_get_data(img), len);
   const char* ext = purple_imgstore_get_extension(img);
   req->add_form(req, LWQQ_FORM_CONTENT, "user", ac->qq->myself->qqnumber);
   req->add_file_content(req, "fname", name, buffer, len, ext);
   *Content = C;
   return req->do_request_async(req, 0, "", _C_(3p, set_img_url, req, C, buffer));
}

LwqqAsyncEvent* download_image_from_server(qq_account* ac, LwqqMsgContent* C)
{
   LwqqHttpRequest* req = lwqq_http_request_new(C->data.ext.param[0]);
   req->lc = ac->qq;
   return req->do_request_async(req, 0, "", _C_(2p, replace_img_ext, req, C));
}
