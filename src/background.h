#ifndef BACKGROUND_H_H
#define BACKGROUND_H_H

void background_login(qq_account* ac);
void background_friends_info(qq_account* ac);
void background_msg_poll(qq_account* ac);
void background_msg_drain(qq_account* ac);
void background_group_detail(qq_account* ac,LwqqGroup* group);
void background_send_msg(qq_account* ac,LwqqMsg* msg,const char* who,const char* what,PurpleConversation* conv);
void background_upload_file(LwqqClient* lc,LwqqMsgOffFile* file,LWQQ_PROGRESS file_trans_on_progress,PurpleXfer* xfer);


#endif
