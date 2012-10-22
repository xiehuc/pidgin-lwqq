#ifndef QQ_TYPES_H_H
#define QQ_TYPES_H_H
typedef struct _AsyncListener AsyncListener;

#include <type.h>
#include <connection.h>
#include "msg.h"
#define QQ_MAGIC 0x4153
typedef struct qq_account {
    LwqqClient* qq;
    PurpleAccount* account;
    PurpleConnection* gc;
    int disable_send_server;///< this ensure not send buddy category change etc event to server
    enum {
        DISCONNECT,
        CONNECTED,
        LOAD_COMPLETED
    }state;
    GPtrArray* opend_chat;
    gboolean disable_custom_font_face;
    gboolean disable_custom_font_size;
    gboolean dark_theme_fix;
    gboolean debug_file_send;
    gboolean compatible_pidgin_conversation_integration;
    int magic;//0x4153
} qq_account;
typedef struct system_msg {
    int msg_type;
    char* who;
    qq_account* ac;
    char* msg;
    int type;
    time_t t;
}system_msg;
qq_account* qq_account_new(PurpleAccount* account);
void qq_account_free(qq_account* ac);
#define qq_account_valid(ac) (ac->magic == QQ_MAGIC)
int open_new_chat(qq_account* ac,LwqqGroup* group);
#define opend_chat_search(ac,group) open_new_chat(ac,group)
#define opend_chat_index(ac,id) g_ptr_array_index(ac->opend_chat,id)
system_msg* system_msg_new(int m_t,const char* who,qq_account* ac,const char* msg,int type,time_t t);
void system_msg_free(system_msg* m);
PurpleConversation* find_conversation(int msg_type,const char* who,qq_account* ac);
void file_message(LwqqClient* lc,LwqqMsgFileMessage* file);
void qq_send_file(PurpleConnection* gc,const char* who,const char* filename);
int qq_sys_msg_write(LwqqClient* lc,void* data);
void qq_send_offline_file(PurpleBlistNode* node);




//void qq_msg_check(qq_account* ac);
//void qq_set_basic_info(int result,void* data);
#endif
