#include "qq_types.h"
#include <unistd.h>

#include <sys/stat.h>
#ifdef WIN32
#include <direct.h>
#define mkdir(a, b) _mkdir(a)
#endif

TABLE_BEGIN_LONG(qq_shengxiao_to_str, const char*, LwqqShengxiao, "")
TR(LWQQ_MOUTH, _("Mouth")) TR(LWQQ_CATTLE, _("Cattle"))
    TR(LWQQ_TIGER, _("Tiger")) TR(LWQQ_RABBIT, _("Rabbit"))
    TR(LWQQ_DRAGON, _("Dragon")) TR(LWQQ_SNACK, _("Snack"))
    TR(LWQQ_HORSE, _("Horse")) TR(LWQQ_SHEEP, _("Sheep"))
    TR(LWQQ_MONKEY, _("Monkey")) TR(LWQQ_CHOOK, _("Chook"))
    TR(LWQQ_DOG, _("Dog")) TR(LWQQ_PIG, _("Pig")) TABLE_END()

    TABLE_BEGIN_LONG(qq_blood_to_str, const char*, LwqqBloodType, "")
    TR(LWQQ_BLOOD_A, _("A")) TR(LWQQ_BLOOD_B, _("B")) TR(LWQQ_BLOOD_O, _("O"))
    TR(LWQQ_BLOOD_AB, _("AB")) TR(LWQQ_BLOOD_OTHER, _("Other")) TABLE_END()

    TABLE_BEGIN_LONG(qq_constel_to_str, const char*, LwqqConstel, "")
    TR(LWQQ_AQUARIUS, _("Aquarius")) TR(LWQQ_PISCES, _("Pisces"))
    TR(LWQQ_ARIES, _("Aries")) TR(LWQQ_TAURUS, _("Taurus"))
    TR(LWQQ_GEMINI, _("Gemini")) TR(LWQQ_CANCER, _("Cancer"))
    TR(LWQQ_LEO, _("Leo")) TR(LWQQ_VIRGO, _("Virgo")) TR(LWQQ_LIBRA, _("Libra"))
    TR(LWQQ_SCORPIO, _("Scorpio")) TR(LWQQ_SAGITTARIUS, _("Sagittarius"))
    TR(LWQQ_CAPRICORNUS, _("Capricornus")) TABLE_END()

    TABLE_BEGIN_LONG(qq_gender_to_str, const char*, LwqqGender, "")
    TR(LWQQ_FEMALE, _("Female")) TR(LWQQ_MALE, _("Male")) TABLE_END()

    TABLE_BEGIN_LONG(qq_client_to_str, const char*, LwqqClientType, "")
    TR(LWQQ_CLIENT_PC, _("Desktop")) TR(LWQQ_CLIENT_MOBILE, _("Phone"))
    TR(LWQQ_CLIENT_WEBQQ, _("WebQQ")) TR(LWQQ_CLIENT_QQFORPAD, _("PadQQ"))
    TABLE_END()

    const char* qq_status_to_str(LwqqStatus status)
{
   if (status == LWQQ_STATUS_ONLINE)
      return "available";
   return lwqq_status_to_str(status);
}
LwqqStatus qq_status_from_str(const char* str)
{
   if (str == NULL)
      return LWQQ_STATUS_LOGOUT;
   if (strcmp(str, "available") == 0)
      return LWQQ_STATUS_ONLINE;
   return lwqq_status_from_str(str);
}

const char* qq_level_to_str(int level)
{
   static const char* symbol[] = { "♔", "⚙", "☾", "☆" };
   static const int number[] = { 64, 16, 4, 1 };
   static char buf[128];
   int l = level;
   int repeat;
   int i, j;
   memset(buf, 0, sizeof(buf));
   for (i = 0; i < 4; i++) {
      repeat = l / number[i];
      l = l % number[i];
      for (j = 0; j < repeat; j++) {
         strcat(buf, symbol[i]);
      }
   }
   format_append(buf, "(%d)", level);
   return buf;
}

static int did_dispatch(void* param)
{
   LwqqCommand* d = param;
   vp_do(*d, NULL);
   s_free(d);
   return 0;
}

void qq_dispatch(LwqqCommand cmd, unsigned long timeout)
{
   if (timeout == 0)
      timeout = 10;
   LwqqCommand* d = s_malloc0(sizeof(*d));
   *d = cmd;

   purple_timeout_add(timeout, did_dispatch, d);
}
#ifdef WITH_MOZJS
static char* hash_with_local_file(const char* uin, const char* ptwebqq,
                                  lwqq_js_t* js)
{
   char* js_txt = lwqq_util_load_res("hash.js", 0);
   if (js_txt == NULL)
      return NULL;

   lwqq_js_load_buffer(js, js_txt);

   char* res = lwqq_js_hash(uin, ptwebqq, js);
   s_free(js_txt);

   return res;
}

static char* hash_with_remote_file(const char* uin, const char* ptwebqq,
                                   void* js)
{
   // github.com is too slow
   const char* url = "http://pidginlwqq.sinaapp.com/hash.js";
   LwqqErrorCode ec = qq_download(url, "hash.js", lwdb_get_config_dir());
   if (ec) {
      lwqq_log(LOG_ERROR, "Could not download JS From %s", url);
   }
   return hash_with_local_file(uin, ptwebqq, js);
}

static char* hash_with_db_url(const char* uin, const char* ptwebqq,
                              qq_account* ac)
{
   const char* url = lwdb_userdb_read(ac->db, "hash.js");
   if (url == NULL)
      return NULL;
   if (qq_download(url, "hash.js", lwdb_get_config_dir()) == LWQQ_EC_ERROR)
      return NULL;
   return hash_with_local_file(uin, ptwebqq, ac->js);
}
#endif

qq_account* qq_account_new(PurpleAccount* account)
{
   qq_account* ac = g_malloc0(sizeof(qq_account));
   ac->account = account;
   ac->magic = QQ_MAGIC;
   ac->flag = 0;
   // this is auto increment sized array . so don't worry about it.
   const char* username = purple_account_get_username(account);
   const char* password = purple_account_get_password(account);
   ac->qq = lwqq_client_new(username, password);
   ac->js = lwqq_js_init();
   ac->sys_log = purple_log_new(PURPLE_LOG_SYSTEM, "system", account, NULL,
                                time(NULL), NULL);
   // add ~/.config/lwqq into search path
   lwqq_util_add_path(lwdb_get_config_dir());
#ifdef WITH_MOZJS
   lwqq_hash_add_entry(ac->qq, "hash_local", (LwqqHashFunc)hash_with_local_file,
                       ac->js);
   lwqq_hash_add_entry(ac->qq, "hash_url", (LwqqHashFunc)hash_with_remote_file,
                       ac->js);
   lwqq_hash_add_entry(ac->qq, "hash_db", (LwqqHashFunc)hash_with_db_url, ac);
#endif

   ac->font.family = s_strdup("宋体");
   ac->font.size = 12;
   ac->font.style = 0;

// lwqq_async_set(ac->qq,1);
#if QQ_USE_FAST_INDEX
   ac->qq->find_buddy_by_uin = find_buddy_by_uin;
   ac->qq->find_buddy_by_qqnumber = find_buddy_by_qqnumber;
   ac->fast_index.uin_index
       = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
   ac->fast_index.qqnum_index
       = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
#endif
   ac->qq->dispatch = qq_dispatch;

   char cookie[256];
   snprintf(cookie, sizeof(cookie), "%s/%s.cookie", lwdb_get_config_dir(),
            username);
   LwqqExtension* ext = lwqq_make_cookie_extension(ac->qq, cookie);
   ext->init(ac->qq, ext);

   return ac;
}
void qq_account_free(qq_account* ac)
{
   // int i;
   // PurpleConnection* gc = purple_account_get_connection(ac->account);
   /*for(i=0;i<ac->opend_chat->len;i++){
     purple_conversation_destroy(purple_find_chat(gc, i));
     }*/
   purple_log_free(ac->sys_log);
   lwqq_js_close(ac->js);
   // g_ptr_array_free(ac->opend_chat,1);
   s_free(ac->recent_group_name);
   s_free(ac->font.family);
#if QQ_USE_FAST_INDEX
   g_hash_table_destroy(ac->fast_index.qqnum_index);
   g_hash_table_destroy(ac->fast_index.uin_index);
#endif
   lwqq_http_cleanup(ac->qq, LWQQ_CLEANUP_IGNORE);
   lwqq_client_free(ac->qq);
   g_free(ac);
}

void qq_account_insert_index_node(qq_account* ac, const LwqqBuddy* b,
                                  const LwqqGroup* g)
{
#if QQ_USE_FAST_INDEX
   if (!ac || (!b && !g))
      return;
   index_node* node = s_malloc0(sizeof(*node));
   int type = b ? NODE_IS_BUDDY : NODE_IS_GROUP;
   node->type = type;
   if (type == NODE_IS_BUDDY) {
      node->node = b;
      const LwqqBuddy* buddy = b;
      g_hash_table_insert(ac->fast_index.uin_index, s_strdup(buddy->uin), node);
      if (buddy->qqnumber)
         g_hash_table_insert(ac->fast_index.qqnum_index,
                             s_strdup(buddy->qqnumber), node);
   } else {
      node->node = g;
      const LwqqGroup* group = g;
      g_hash_table_insert(ac->fast_index.uin_index, s_strdup(group->gid), node);
      if (group->account)
         g_hash_table_insert(ac->fast_index.qqnum_index,
                             s_strdup(group->account), node);
   }
#endif
}
void qq_account_remove_index_node(qq_account* ac, const LwqqBuddy* b,
                                  const LwqqGroup* g)
{
#if QQ_USE_FAST_INDEX
   if (!ac || !(b && !g))
      return;
   int type = b ? NODE_IS_BUDDY : NODE_IS_GROUP;
   if (type == NODE_IS_BUDDY) {
      const LwqqBuddy* buddy = b;
      if (buddy->qqnumber)
         g_hash_table_remove(ac->fast_index.qqnum_index, buddy->qqnumber);
      g_hash_table_remove(ac->fast_index.uin_index, buddy->uin);
   } else {
      const LwqqGroup* group = g;
      if (group->account)
         g_hash_table_remove(ac->fast_index.qqnum_index, group->account);
      g_hash_table_remove(ac->fast_index.uin_index, group->gid);
   }
#endif
}

static PurpleConversation* find_conversation(LwqqMsgType msg_type,
                                             const char* serv_id,
                                             qq_account* ac,
                                             const char** local_id_out)
{
   PurpleAccount* account = ac->account;
   LwqqClient* lc = ac->qq;
   // add a valid check
   if (!lwqq_client_valid(lc))
      return NULL;
   const char* local_id = NULL;
   PurpleConversation* conv = NULL;
   switch (msg_type) {
   case LWQQ_MS_BUDDY_MSG:
   case LWQQ_MS_SESS_MSG:
      if (ac->flag & QQ_USE_QQNUM) {
         LwqqBuddy* buddy = ac->qq->find_buddy_by_uin(ac->qq, serv_id);
         local_id = (buddy && buddy->qqnumber) ? buddy->qqnumber : serv_id;
      } else
         local_id = serv_id;
      conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
                                                   local_id, account);
      break;

   case LWQQ_MS_GROUP_MSG:
   case LWQQ_MS_DISCU_MSG:
      if (ac->flag & QQ_USE_QQNUM) {
         LwqqGroup* group = find_group_by_gid(ac->qq, serv_id);
         local_id = (group && group->account) ? group->account : serv_id;
      } else
         local_id = serv_id;
      conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                   local_id, account);
      break;
   default:
      break;
   }
   *local_id_out = local_id;
   return conv;
}

void qq_sys_msg_write(qq_account* ac, LwqqMsgType m_t, const char* serv_id,
                      const char* msg, PurpleMessageFlags type, time_t t)
{
   // ac->qq->dispatch(vp_func_2p,(CALLBACK_FUNC)sys_msg_write,ac->qq,system_msg_new(m_t,serv_id,ac,msg,type,t));

   const char* local_id;
   PurpleConversation* conv = find_conversation(m_t, serv_id, ac, &local_id);
   if (conv) {
      purple_conversation_write(conv, NULL, msg, type, t);
      const char* signal = (m_t == LWQQ_MS_BUDDY_MSG || m_t == LWQQ_MS_SESS_MSG)
                               ? "received-im-msg"
                               : "recieved-chat-msg";
      purple_signal_emit(purple_conversations_get_handle(), signal, ac->account,
                         conv->name, msg, conv, type);
   }
}

LwqqBuddy* find_buddy_by_qqnumber(LwqqClient* lc, const char* qqnum)
{
   qq_account* ac = lwqq_client_userdata(lc);
#if QQ_USE_FAST_INDEX
   index_node* node = g_hash_table_lookup(ac->fast_index.qqnum_index, qqnum);
   if (node == NULL)
      return NULL;
   if (node->type != NODE_IS_BUDDY)
      return NULL;
   return (LwqqBuddy*)node->node;
#else
   return lwqq_buddy_find_buddy_by_qqnumber(lc, qqnum);
#endif
}
LwqqGroup* find_group_by_qqnumber(LwqqClient* lc, const char* qqnum)
{
   qq_account* ac = lwqq_client_userdata(lc);
#if QQ_USE_FAST_INDEX
   index_node* node = g_hash_table_lookup(ac->fast_index.qqnum_index, qqnum);
   if (node == NULL)
      return NULL;
   if (node->type != NODE_IS_GROUP)
      return NULL;
   return (LwqqGroup*)node->node;
#else
   return lwqq_group_find_group_by_qqnumber(lc, qqnum);
#endif
}

LwqqBuddy* find_buddy_by_uin(LwqqClient* lc, const char* uin)
{
#if QQ_USE_FAST_INDEX
   qq_account* ac = lwqq_client_userdata(lc);
   index_node* node = g_hash_table_lookup(ac->fast_index.uin_index, uin);
   if (node == NULL)
      return NULL;
   if (node->type != NODE_IS_BUDDY)
      return NULL;
   return (LwqqBuddy*)node->node;
#else
   return lwqq_buddy_find_buddy_by_uin(lc, uin);
#endif
}
LwqqGroup* find_group_by_gid(LwqqClient* lc, const char* gid)
{
#if QQ_USE_FAST_INDEX
   qq_account* ac = lwqq_client_userdata(lc);
   index_node* node = g_hash_table_lookup(ac->fast_index.uin_index, gid);
   if (node == NULL)
      return NULL;
   if (node->type != NODE_IS_GROUP)
      return NULL;
   return (LwqqGroup*)node->node;
#else
   return lwqq_group_find_group_by_gid(lc, gid);
#endif
}

void vp_func_4pl(CALLBACK_FUNC func, vp_list* vp, void* q)
{
   typedef void (*f)(void*, void*, void*, void*, long);
   if (q) {
      va_list* va = q;
      vp_init(*vp, sizeof(void*) * 4 + sizeof(long));
      vp_dump(*vp, *va, void*);
      vp_dump(*vp, *va, void*);
      vp_dump(*vp, *va, void*);
      vp_dump(*vp, *va, void*);
      vp_dump(*vp, *va, long);
      return;
   }
   void* p1 = vp_arg(*vp, void*);
   void* p2 = vp_arg(*vp, void*);
   void* p3 = vp_arg(*vp, void*);
   void* p4 = vp_arg(*vp, void*);
   long p5 = vp_arg(*vp, long);
   ((f)func)(p1, p2, p3, p4, p5);
}

LwqqErrorCode qq_download(const char* url, const char* file, const char* dir)
{
   LwqqHttpRequest* req = lwqq_http_request_new(url);
   req->do_request(req, 0, NULL);
   if (req->http_code != 200)
      return LWQQ_EC_ERROR;
   if (!req->response)
      return LWQQ_EC_ERROR;

   char path[2048];
   snprintf(path, sizeof(path), "%s/%s", dir, file);
   char* content = req->response;
   FILE* f;
   f = fopen(path, "w");
   if (!f)
      mkdir(dir, 0755);
   if (!f)
      return LWQQ_EC_ERROR;
   fwrite(content, 1, strlen(content), f);
   fclose(f);
   lwqq_http_request_free(req);
   return LWQQ_EC_OK;
}

void qq_system_log(qq_account* ac, const char* log)
{
   char buf[8192];
   snprintf(buf, sizeof(buf), "[帐号 %s]:<br>%s", ac->account->username, log);
   purple_log_write(ac->sys_log, PURPLE_LOG_SYSTEM, "system", time(NULL), buf);
}

char* strtrim(char* source)
{
   char* beg = source;
   while (*beg == ' ')
      ++beg;
   char* end = source + strlen(source) - 1;
   while (*end == ' ')
      --end;
   end[1] = '\0';
   return beg;
}

