#ifndef QQ_TYPES_H_H
#define QQ_TYPES_H_H

#include "config.h"

#include <connection.h>
#include <lwqq.h>
#include <lwjs.h>
#include <lwdb.h>

#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#include <locale.h>
#else
#define _(s) s
#endif

#ifdef WIN32
#include "win.h"
#endif

#define QQ_MAGIC 0x4153
#define BUFLEN 15000

#define bit_set(var, bit, value) (var = (value) ? var | bit : var & ~bit)
#define bit_get(var, bit) ((var & bit) > 0)

#define DBGID "webqq"
#define QQ_DEFAULT_CATE _("Friend")
#define QQ_PASSERBY_CATE _("Passerby")
#define QQ_GROUP_DEFAULT_CATE _("Chat")
// this is qqnumber of a group
#define QQ_ROOM_KEY_QUN_ID "account"
#define QQ_ROOM_KEY_GID "gid"
#define QQ_ROOM_TYPE "type"
#define QQ_ROOM_TYPE_QUN "qun"
#define QQ_ROOM_TYPE_DISCU "discu"

typedef enum {
   RESET_BUDDY = 1 << 0,
   RESET_GROUP = 1 << 1,
   RESET_DISCU = 1 << 2,
   RESET_GROUP_SOFT = 1 << 3, ///<this only delete duplicated chat
   RESET_DISCU_SOFT = 1 << 4,
   RESET_ALL = RESET_BUDDY | RESET_GROUP | RESET_DISCU
}QQResetFlags;

typedef struct {
   enum { NODE_IS_BUDDY, NODE_IS_GROUP } type;
   const void* node;
} index_node;
typedef struct qq_account {
   LwqqClient* qq;
   PurpleAccount* account;
   PurpleConnection* gc;
   LwdbUserDB* db;
   int disable_send_server; ///< this ensure not send buddy category change etc
                            ///event to server
   enum { DISCONNECT, CONNECTED, LOAD_COMPLETED } state;
   int msg_poll_handle;
   int relink_timer;
   GList* rewrite_pic_list;
   char* recent_group_name;
   PurpleLog* sys_log;
   struct {
      char* family;
      int size;
      LwqqFontStyle style;
   } font;
   enum {
      QQ_USE_QQNUM = 1 << 0,
      IGNORE_FONT_FACE = 1 << 1,
      IGNORE_FONT_SIZE = 1 << 2,
      DARK_THEME_ADAPT = 1 << 3,
      DEBUG_FILE_SEND = 1 << 4,
      REMOVE_DUPLICATED_MSG = 1 << 5,
      QQ_DONT_EXPECT_100_CONTINUE = 1 << 6,
      NOT_DOWNLOAD_GROUP_PIC = 1 << 7,
      SEND_VISUALBILITY = 1 << 8,
   } flag;
#if QQ_USE_FAST_INDEX
   struct {
      GHashTable* qqnum_index;
      GHashTable* uin_index; ///< key:char*,value:struct index_node
   } fast_index;
#endif
   lwqq_js_t* js;
   int magic; // 0x4153
} qq_account;
typedef struct system_msg {
   int msg_type;
   char* who;
   qq_account* ac;
   char* msg;
   int type;
   time_t t;
} system_msg;

void qq_dispatch(LwqqCommand cmd, unsigned long timeout);

LwqqErrorCode qq_download(const char* url, const char* file, const char* dir);
#define try_get(val, fail) (val ? val : fail)

qq_account* qq_account_new(PurpleAccount* account);
void qq_account_free(qq_account* ac);
#define qq_account_valid(ac) (ac->magic == QQ_MAGIC)

void qq_account_insert_index_node(qq_account* ac, const LwqqBuddy* b,
                                  const LwqqGroup* g);
void qq_account_remove_index_node(qq_account* ac, const LwqqBuddy* b,
                                  const LwqqGroup* g);

void qq_sys_msg_write(qq_account* ac, LwqqMsgType m_t, const char* serv_id,
                      const char* msg, PurpleMessageFlags type, time_t t);
void qq_system_log(qq_account* ac, const char* log);

#if 0
//----------------------------ft.h-----------------------------
void file_message(LwqqClient* lc,LwqqMsgFileMessage* file);
void qq_send_file(PurpleConnection* gc,const char* who,const char* filename);
void qq_send_offline_file(PurpleBlistNode* node);
//=============================================================
#endif

LwqqBuddy* find_buddy_by_qqnumber(LwqqClient* lc, const char* qqnum);
LwqqBuddy* find_buddy_by_uin(LwqqClient* lc, const char* uin);
LwqqGroup* find_group_by_qqnumber(LwqqClient* lc, const char* qqnum);
LwqqGroup* find_group_by_gid(LwqqClient* lc, const char* gid);

const char* qq_gender_to_str(LwqqGender gender);
const char* qq_constel_to_str(LwqqConstel constel);
const char* qq_blood_to_str(LwqqBloodType bt);
const char* qq_shengxiao_to_str(LwqqShengxiao shengxiao);
const char* qq_client_to_str(LwqqClientType client);
const char* qq_level_to_str(int level);
const char* qq_status_to_str(LwqqStatus status);
LwqqStatus qq_status_from_str(const char* str);

void vp_func_4pl(CALLBACK_FUNC func, vp_list* vp, void* p);

char* strtrim(char* source);

//-----------------------login.c------------------------
void qq_login(LwqqClient* lc, LwqqErrorCode* p_err);
//======================================================
//
//----------------------webqq.c-------------------------
void friend_avatar(qq_account* ac, LwqqBuddy* buddy);
void qq_all_reset(qq_account* ac, QQResetFlags opt);
void friend_come(LwqqClient* lc, LwqqBuddy** p_buddy);
void group_come(LwqqClient* lc, LwqqGroup** p_group);
#define discu_come(lc, data) (group_come(lc, data))
//======================================================

#endif

