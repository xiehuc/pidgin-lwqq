#include "qq_types.h"

struct QQLoginStage {
   unsigned stage;
   LwqqAsyncEvset* set;
};
typedef LwqqAsyncEvset* (*QQLoginFunc)(qq_account* ac, struct QQLoginStage*);
static void qq_login_stage(qq_account* ac, struct QQLoginStage* s);

void qq_login(LwqqClient* lc, LwqqErrorCode* p_err)
{
   // add a valid check
   LwqqErrorCode err = *p_err;
   if (!lwqq_client_valid(lc))
      return;
   qq_account* ac = lwqq_client_userdata(lc);
   PurpleConnection* gc = purple_account_get_connection(ac->account);
   switch (err) {
   case LWQQ_EC_OK:
      break;
   case LWQQ_EC_LOGIN_ABNORMAL:
      purple_connection_error_reason(
          gc, PURPLE_CONNECTION_ERROR_OTHER_ERROR,
          _("Account Problem Occurs,Need lift the ban"));
      return;
   case LWQQ_EC_NETWORK_ERROR:
      purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_OTHER_ERROR,
                                     _("Network Error"));
      return;
   case LWQQ_EC_LOGIN_NEED_BARCODE:
      purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_OTHER_ERROR,
                                     lc->error_description);
      return;
   default:
      purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
                                     lc->error_description);
      return;
   }

   ac->state = CONNECTED;

   gc->flags |= PURPLE_CONNECTION_HTML;

   struct QQLoginStage* s = s_malloc0(sizeof(*s));
   qq_login_stage(ac, s);
}


static void get_friend_list_cb(LwqqAsyncEvent* ev);
static LwqqAsyncEvset* get_friend_list(qq_account* ac, struct QQLoginStage* s)
{
   LwqqClient* lc = ac->qq;
   LwqqAsyncEvset* set = lwqq_async_evset_new();
   // use lwqq 0.3.1 hash auto select api
   LwqqAsyncEvent* ev = lwqq_info_get_friends_info(lc, NULL, NULL);
   lwqq_async_add_event_listener(ev, _C_(p, get_friend_list_cb, ev));
   lwqq_async_evset_add_event(set, ev);
   s->stage ++;
   return set;
}
static void get_friend_list_cb(LwqqAsyncEvent* ev)
{
   LwqqClient* lc = ev->lc;
   qq_account* ac = lc->data;
   if (ev->result == LWQQ_EC_HASH_WRONG) {
      purple_connection_error_reason(
          ac->gc, PURPLE_CONNECTION_ERROR_OTHER_ERROR,
#ifndef WITH_MOZJS
          _("Hash Function Wrong, Please recompile with mozjs \n"
            "https://github.com/xiehuc/pidgin-lwqq/wiki/"
            "simple-user-guide#wiki-hash-function")
#else
          _("Hash Function Wrong, Please try again later or report to author")
#endif
          );
      return;
   }
   if (ev->result != LWQQ_EC_OK) {
      purple_connection_error_reason(ac->gc,
                                     PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
                                     _("Get Friend List Failed"));
      return;
   }
   const LwqqHashEntry* succ_hash = lwqq_hash_get_last(lc);
   lwdb_userdb_write(ac->db, "last_hash", succ_hash->name);
}

static void get_group_list_cb(LwqqAsyncEvent* ev, qq_account* ac);
static LwqqAsyncEvset* get_group_list(qq_account* ac, struct QQLoginStage* s)
{
   LwqqClient* lc = ac->qq;
   LwqqAsyncEvset* set = lwqq_async_evset_new();

   LwqqAsyncEvent* event = lwqq_info_get_group_name_list(lc, NULL, NULL);
   lwqq_async_add_event_listener(event, _C_(2p, get_group_list_cb, event, ac));
   lwqq_async_evset_add_event(set, event);
   s->stage ++;
   return set;
}

static void get_group_list_cb(LwqqAsyncEvent* ev, qq_account* ac)
{
   if (ev->result != LWQQ_EC_OK) {
      purple_connection_error_reason(ac->gc,
                                     PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
                                     _("Get Group List Failed"));
   }
}

static LwqqAsyncEvset* get_other_list(qq_account* ac, struct QQLoginStage* s)
{
   LwqqAsyncEvset* set = lwqq_async_evset_new();
   LwqqAsyncEvent* ev;
   LwqqClient* lc = ac->qq;
   ev = lwqq_info_get_discu_name_list(lc);
   lwqq_async_evset_add_event(set, ev);
   ev = lwqq_info_get_online_buddies(lc, NULL);
   lwqq_async_evset_add_event(set, ev);

   const char* alias = purple_account_get_alias(ac->account);
   if (alias == NULL) {
      ev = lwqq_info_get_friend_detail_info(lc, lc->myself);
      lwqq_async_evset_add_event(set, ev);
   }
   s->stage++;
   return set;
}

static void get_qq_numbers_cb(qq_account* ac);
static LwqqAsyncEvset* get_qq_numbers(qq_account* ac, struct QQLoginStage* s)
{
   LwqqClient* lc = ac->qq;
   lwdb_userdb_flush_buddies(ac->db, 5, 5);
   lwdb_userdb_flush_groups(ac->db, 1, 10);

   if (ac->flag & QQ_USE_QQNUM) {
      lwdb_userdb_query_qqnumbers(ac->db, lc);
   }

   // make sure all qqnumber is setup,then make state connected
   purple_connection_set_state(purple_account_get_connection(ac->account),
                               PURPLE_CONNECTED);

   if (!purple_account_get_alias(ac->account))
      purple_account_set_alias(ac->account, lc->myself->nick);
   if (purple_buddy_icons_find_account_icon(ac->account) == NULL) {
      LwqqAsyncEvent* ev = lwqq_info_get_friend_avatar(lc, lc->myself);
      lwqq_async_add_event_listener(ev, _C_(2p, friend_avatar, ac, lc->myself));
   }

   LwqqAsyncEvent* ev = NULL;

   // we must put buddy and group clean before any add operation.
   GSList* ptr = purple_blist_get_buddies();
   while (ptr) {
      PurpleBuddy* buddy = ptr->data;
      if (buddy->account == ac->account) {
         const char* qqnum = purple_buddy_get_name(buddy);
         // if it isn't a qqnumber,we should delete it whatever.
         if (lwqq_buddy_find_buddy_by_qqnumber(lc, qqnum) == NULL) {
            purple_blist_remove_buddy(buddy);
         }
      }
      ptr = ptr->next;
   }

   // clean extra duplicated node
   qq_all_reset(ac, RESET_GROUP_SOFT | RESET_DISCU_SOFT);

   LwqqAsyncEvset* qq_pool = lwqq_async_evset_new();

   LwqqBuddy* buddy;
   LIST_FOREACH(buddy, &lc->friends, entries)
   {
      lwdb_userdb_query_buddy(ac->db, buddy);
      if ((ac->flag & QQ_USE_QQNUM) && !buddy->qqnumber) {
         ev = lwqq_info_get_friend_qqnumber(lc, buddy);
         lwqq_async_evset_add_event(qq_pool, ev);
      }
      if (buddy->last_modify != -1 && buddy->last_modify != 0)
         friend_come(lc, &buddy);
   }

   LwqqGroup* group;
   LIST_FOREACH(group, &lc->groups, entries)
   {
      lwdb_userdb_query_group(ac->db, group);
      if ((ac->flag && QQ_USE_QQNUM) && !group->account) {
         ev = lwqq_info_get_group_qqnumber(lc, group);
         lwqq_async_evset_add_event(qq_pool, ev);
      }
      // because group avatar less changed.
      // so we dont reload it.
      if (group->last_modify != -1 && group->last_modify != 0)
         group_come(lc, &group);
   }

   LwqqGroup* discu;
   LIST_FOREACH(discu, &lc->discus, entries)
   {
      if (discu->last_modify == LWQQ_LAST_MODIFY_UNKNOW)
         // discu is imediately date, doesn't need get info from server, we can
         // directly write it into database
         lwdb_userdb_insert_discu_info(ac->db, &discu);
      discu_come(lc, &discu);
   }

   s->stage ++;
   lwqq_async_add_evset_listener(qq_pool, _C_(p, get_qq_numbers_cb, ac));
   return qq_pool;
}


static void update_list_and_db(qq_account* ac);
// stage finish, all qqnumber downloaded
static void get_qq_numbers_cb(qq_account* ac)
{
   update_list_and_db(ac);
   // wait all download finished, start msg pool, to get all qqnumber
   LwqqPollOption flags = POLL_AUTO_DOWN_DISCU_PIC | POLL_AUTO_DOWN_GROUP_PIC
                          | POLL_AUTO_DOWN_BUDDY_PIC;
   if (ac->flag & REMOVE_DUPLICATED_MSG)
      flags |= POLL_REMOVE_DUPLICATED_MSG;
   if (ac->flag & NOT_DOWNLOAD_GROUP_PIC)
      flags &= ~POLL_AUTO_DOWN_GROUP_PIC;

   lwqq_msglist_poll(ac->qq->msg_list, flags);
   lwqq_puts("[all download finished]");

   // now open status, allow user start talk
   ac->state = LOAD_COMPLETED;
}

static LwqqAsyncEvset* get_extra_info(qq_account* ac, struct QQLoginStage* s)
{
   LwqqAsyncEvset* info_pool = lwqq_async_evset_new();
   LwqqClient* lc = ac->qq;
   LwqqAsyncEvent* ev;

   LwqqBuddy* buddy;
   LIST_FOREACH(buddy, &lc->friends, entries)
   {
      if (buddy->last_modify == 0 || buddy->last_modify == -1) {
         ev = lwqq_info_get_single_long_nick(lc, buddy);
         lwqq_async_evset_add_event(info_pool, ev);
         ev = lwqq_info_get_level(lc, buddy);
         lwqq_async_evset_add_event(info_pool, ev);
         // if buddy is unknow we should update avatar in friend_come
         // for better speed in first load
         if (buddy->last_modify == LWQQ_LAST_MODIFY_RESET) {
            ev = lwqq_info_get_friend_avatar(lc, buddy);
            lwqq_async_evset_add_event(info_pool, ev);
         }
      }
   }

   LwqqGroup* group;
   LIST_FOREACH(group, &lc->groups, entries)
   {
      if (group->last_modify == -1 || group->last_modify == 0) {
         ev = lwqq_info_get_group_memo(lc, group);
         lwqq_async_evset_add_event(info_pool, ev);
      }
   }

   s->stage ++;
   lwqq_async_add_evset_listener(info_pool, _C_(p, update_list_and_db, ac));
   return info_pool;
}

static void update_list_and_db(qq_account* ac)
{
   LwqqBuddy* buddy;
   LwqqGroup* group;
   LwqqClient* lc = ac->qq;

   lwdb_userdb_begin(ac->db);
   LIST_FOREACH(buddy, &lc->friends, entries)
   {
      if (buddy->last_modify == -1 || buddy->last_modify == 0) {
         lwdb_userdb_insert_buddy_info(ac->db, &buddy);
         friend_come(lc, &buddy);
      }
   }
   LIST_FOREACH(group, &lc->groups, entries)
   {
      if (group->last_modify == -1 || group->last_modify == 0) {
         lwdb_userdb_insert_group_info(ac->db, &group);
         group_come(lc, &group);
      }
   }
   lwdb_userdb_commit(ac->db);
}

static QQLoginFunc qq_login_seq[] = {
   get_friend_list,
   get_group_list,
   get_other_list,
   get_qq_numbers,
   get_extra_info,
   NULL
};

static void qq_login_stage(qq_account* ac, struct QQLoginStage* s)
{
   LwqqErrorCode err;
   if(s->set && s->set->err_count > 0){
      err = LWQQ_EC_ERROR;
      goto fail;
   }
   if(qq_login_seq[s->stage]==NULL){
      err = LWQQ_EC_OK;
      goto done;
   }

   s->set = qq_login_seq[s->stage](ac, s);
   lwqq_async_add_evset_listener(s->set, _C_(2p, qq_login_stage, ac, s));
   lwqq_async_evset_unref(s->set);
   return;
done:
fail:
   if(err){
      lwqq_puts("[ error when login ]");
   }
   s_free(s);
}
