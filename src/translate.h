#ifndef INFO_H_H
#define INFO_H_H
#include <msg.h>

void translate_global_init();
void translate_global_free();
int translate_message_to_struct(LwqqClient* lc,const char* to,const char* what,LwqqMsgMessage* mmsg,int using_cface);
void translate_add_smiley_to_conversation(PurpleConversation* conv);
const char* translate_smile(int face);
#endif
