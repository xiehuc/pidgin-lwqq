#ifndef QQ_TYPES_H_H
#define QQ_TYPES_H_H
typedef struct _AsyncListener AsyncListener;

#include <type.h>
#include <connection.h>
#include "msg.h"
#include "lwdb.h"

#define QQ_MAGIC 0x4153
#define QQ_USE_FAST_INDEX 1
#define SUCCESS 0
#define FAILED -1

#ifdef UNUSED
#elif defined(__GNUC__)
# 	define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#else
#	define UNUSED(x) x
#endif

#define DISPLAY_VERSION "0.1"
#define DBGID   "webqq"
//this is qqnumber of a group
#define QQ_ROOM_KEY_QUN_ID "account"
#define QQ_ROOM_KEY_GID "gid"
#define QQ_ROOM_TYPE "type"
#define QQ_ROOM_TYPE_QUN "qun"
#define QQ_ROOM_TYPE_DISCU "discu"

typedef struct {
    enum {NODE_IS_BUDDY,NODE_IS_GROUP} type;
    void* node;
}index_node;

typedef struct qq_account {
    LwqqClient* qq;
    PurpleAccount* account;
    PurpleConnection* gc;
    LwdbUserDB* db;
    int disable_send_server;///< this ensure not send buddy category change etc event to server
    enum {
        DISCONNECT,
        CONNECTED,
        LOAD_COMPLETED
    }state;
    int msg_poll_handle;
    GPtrArray* opend_chat;
#if QQ_USE_FAST_INDEX
    struct{
        GHashTable* qqnum_index;
        GHashTable* uin_index;
    }fast_index;
#endif
    struct{
    gboolean qq_use_qqnum;
    gboolean disable_custom_font_face;
    gboolean disable_custom_font_size;
    gboolean dark_theme_fix;
    gboolean debug_file_send;
    };
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

void qq_account_insert_index_node(qq_account* ac,int type,void* data);
void qq_account_remove_index_node(qq_account* ac,int type,void* data);

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

LwqqBuddy* find_buddy_by_qqnumber(LwqqClient* lc,const char* qqnum);
LwqqGroup* find_group_by_qqnumber(LwqqClient* lc,const char* qqnum);
LwqqBuddy* find_buddy_by_uin(LwqqClient* lc,const char* uin);
LwqqGroup* find_group_by_gid(LwqqClient* lc,const char* gid);

#endif
