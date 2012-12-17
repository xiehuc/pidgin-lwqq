#ifndef BACKGROUND_H_H
#define BACKGROUND_H_H

void background_login(qq_account* ac);
void background_msg_poll(qq_account* ac);
void background_msg_drain(qq_account* ac);
void background_upload_file(LwqqClient* lc,LwqqMsgOffFile* file,LWQQ_PROGRESS file_trans_on_progress,PurpleXfer* xfer);


#endif
