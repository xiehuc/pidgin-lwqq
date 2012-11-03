#ifndef INFO_H_H
#define INFO_H_H
#include <msg.h>
#include "qq_types.h"

void translate_global_init();
void translate_global_free();
int translate_message_to_struct(LwqqClient* lc,const char* to,const char* what,LwqqMsg*,int using_cface);
void translate_struct_to_message(qq_account* ac, LwqqMsgMessage* c,char* buf);
void translate_add_smiley_to_conversation(PurpleConversation* conv);
const char* translate_smile(int face);
#endif
