#ifndef INFO_H_H
#define INFO_H_H

typedef struct qq_account qq_account;

void translate_global_init();
void translate_global_free();
int translate_message_to_struct(qq_account* ac, const char* to,
                                const char* what, LwqqMsg*, int using_cface);
struct ds translate_struct_to_message(qq_account* ac, LwqqMsgMessage* msg,
                                      PurpleMessageFlags flags);
void translate_add_smiley_to_conversation(PurpleConversation* conv);
const char* translate_smile(int face);
char* translate_to_html_symbol(const char* s);
void translate_append_string(LwqqMsg* msg, const char* text);
#endif
