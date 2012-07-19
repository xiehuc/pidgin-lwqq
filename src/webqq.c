
#define PURPLE_PLUGINS
#include <plugin.h>
#include <version.h>
#include "internal.h"
#include "webqq.h"
#include "qq_types.h"
#include "login.h"
#include "background.h"

#include <type.h>
#include <async.h>
#include <msg.h>

static void group_member_list_come(LwqqClient* lc,void* data);

static void action_about_webqq(PurplePluginAction *action)
{
    PurpleConnection *gc = (PurpleConnection *) action->context;
    GString *info;
    gchar *title;

    g_return_if_fail(NULL != gc);

    info = g_string_new("<html><body>");
    g_string_append(info, _("<p><b>Author</b>:<br>\n"));
    g_string_append(info, "xiehuc\n");
    g_string_append(info, "<br/>\n");

    g_string_append(info, _("pidgin-libwebqq mainly referenced: "
                            "1.openfetion for libpurple about<br/>"
                            "2.lwqq for webqq about<br/>"
                            "so it remaind a easy job<br/>"
                            "thanks riegamaths@gmail.com's great guide"));
    g_string_append(info, "<br/><br/></body></html>");
    title = g_strdup_printf(_("About WebQQ %s"), DISPLAY_VERSION);
    purple_notify_formatted(gc, title, title, NULL, info->str, NULL, NULL);

    g_free(title);
    g_string_free(info, TRUE);
}

static GList *plugin_actions(PurplePlugin *UNUSED(plugin), gpointer UNUSED(context))
{

    GList *m;
    PurplePluginAction *act;

    m = NULL;

    ///分割线
    m = g_list_append(m, NULL);

    act = purple_plugin_action_new(_("About WebQQ"), action_about_webqq);
    m = g_list_append(m, act);

    return m;
}


static const char *qq_list_icon(PurpleAccount *UNUSED(a), PurpleBuddy *UNUSED(b))
{
    return "webqq";
}

static GList *qq_status_types(PurpleAccount *UNUSED(account))
{
    PurpleStatusType *status;
    GList *types = NULL;

    status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE,
                                         "online", _("Online"), FALSE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_AWAY,
                                         "away", _("Away"), FALSE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_INVISIBLE,
                                         "hidden", _("Invisible"), FALSE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,
                                         "offline", _("Offline"), FALSE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_UNAVAILABLE,
                                         "busy", _("Busy"), FALSE, TRUE, FALSE);
    types = g_list_append(types, status);

    return types;
}

static qq_account* qq_account_new(PurpleAccount* account)
{
    qq_account* ac = g_malloc0(sizeof(qq_account));
    ac->account = account;
    return ac;
}
static void qq_account_free(qq_account* ac)
{
    g_free(ac);
}
static void friend_come(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account=ac->account;
    LwqqBuddy* buddy = data;
    PurpleBuddy* bu = NULL;
    PurpleGroup* group;
    LwqqFriendCategory* cate;
    int cate_index;
    LIST_FOREACH(cate,&lc->categories,entries){
        cate_index = atoi(buddy->cate_index);
        if(cate_index == 0){
            group = purple_group_new("QQ好友");
            break;
        }
        if(cate->index==cate_index){
            group = purple_group_new(cate->name);
            break;
        }
    }

    bu = purple_find_buddy(account,buddy->qqnumber);
    if(bu == NULL){
        bu = purple_buddy_new(ac->account,buddy->qqnumber,buddy->nick);
        if(buddy->markname) purple_blist_alias_buddy(bu,buddy->markname);
        purple_blist_add_buddy(bu,NULL,group,NULL);
        if(buddy->avatar_len)
            purple_buddy_icons_set_for_user(account, buddy->qqnumber, buddy->avatar, buddy->avatar_len, NULL);
    }
    if(buddy->status)
        purple_prpl_got_user_status(account, buddy->qqnumber, buddy->status, NULL);
}
static void group_come(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account=ac->account;
    LwqqGroup* group = data;
    PurpleGroup* gp = purple_group_new("QQ群");
    GHashTable* components;
    PurpleChat* chat;

    if(purple_blist_find_chat(account,group->account) == NULL){
        components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(components,g_strdup(QQ_ROOM_KEY_QUN_ID),g_strdup(group->account));
        chat = purple_chat_new(account,group->account,components);
        purple_blist_alias_chat(chat,group->name);
        purple_blist_add_chat(chat,gp,NULL);
        if(group->avatar_len)
            purple_buddy_icons_node_set_custom_icon(PURPLE_BLIST_NODE(chat),(guchar*)group->avatar,group->avatar_len);
    }

}
static void buddy_message(LwqqClient* lc,LwqqMsgMessage* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    char buf[1024] = {0};
    LwqqMsgContent *c;
    LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,msg->from);
    if(buddy==NULL) return;

    LIST_FOREACH(c, &msg->content, entries) {
        if (c->type == LWQQ_CONTENT_STRING) {
            strcat(buf, c->data.str);
        } else {
            printf ("Receive face msg: %d\n", c->data.face);
        }
    }
    serv_got_im(pc, buddy->qqnumber, buf, PURPLE_MESSAGE_RECV, time(NULL));
}
//open chat conversation dialog
static void qq_conv_open(PurpleConnection* gc,LwqqGroup* group)
{
    int id = atoi(group->gid);
    PurpleConversation *conv = purple_find_chat(gc,id);
    if(conv == NULL) serv_got_joined_chat(gc,id,group->account);
}
static void group_message(LwqqClient* lc,LwqqMsgMessage* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    LwqqGroup* group = lwqq_group_find_group_by_gid(lc,msg->from);

    //force open dialog
    qq_conv_open(pc,group);
    char buf[1024] = {0};
    LwqqMsgContent *c;
    LIST_FOREACH(c, &msg->content, entries) {
        if (c->type == LWQQ_CONTENT_STRING) {
            strcat(buf, c->data.str);
        } else {
            printf ("Receive face msg: %d\n", c->data.face);
        }
    }

    LwqqBuddy* buddy;
    buddy = lwqq_buddy_find_buddy_by_uin(lc,msg->send);

    serv_got_chat_in(pc,atoi(group->gid),buddy->qqnumber,PURPLE_MESSAGE_RECV,buf,time(NULL));

    if(!LIST_EMPTY(&group->members)) {
        group_member_list_come(lc,group);
    } else {
        lwqq_async_add_listener(lc,GROUP_DETAIL,group_member_list_come);
        lwqq_async_set_userdata(lc,GROUP_DETAIL,ac);
        background_group_detail(ac,group);
    }
}
static void status_change(LwqqClient* lc,LwqqMsgStatusChange* status)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;
    LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,status->who);
    if(buddy==NULL) return;

    purple_prpl_got_user_status(account,buddy->qqnumber,status->status,NULL);
}
static void avatar_complete(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;
    LwqqBuddy* buddy;
    LwqqGroup* group;
    PurpleChat* chat;
    //do not catch icons 
    purple_buddy_icons_set_caching(0);
    
    LIST_FOREACH(buddy,&lc->friends,entries){
        if(buddy->avatar_len)
            purple_buddy_icons_set_for_user(account, buddy->uin, buddy->avatar, buddy->avatar_len, NULL);
    }
    LIST_FOREACH(group,&lc->groups,entries){
        if(group->avatar_len){
            chat = purple_blist_find_chat(account,group->gid);
            if(chat==NULL) continue;
            purple_buddy_icons_node_set_custom_icon(PURPLE_BLIST_NODE(chat),(guchar*)group->avatar,group->avatar_len);
        }
    }
}
void qq_msg_check(qq_account* ac)
{
    LwqqClient* lc = ac->qq;
    if(lc==NULL)return;
    LwqqRecvMsgList* l = lc->msg_list;
    LwqqRecvMsg *msg;

    pthread_mutex_lock(&l->mutex);
    if (SIMPLEQ_EMPTY(&l->head)) {
        /* No message now, wait 100ms */
        pthread_mutex_unlock(&l->mutex);
        return ;
    }

    msg = SIMPLEQ_FIRST(&l->head);
    switch(msg->msg->type){
        case LWQQ_MT_BUDDY_MSG:
            buddy_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
            break;
        case LWQQ_MT_GROUP_MSG:
            group_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
            break;
        case LWQQ_MT_STATUS_CHANGE:
            /*LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,msg->msg->status.who);
              if(buddy->status!=NULL)s_free(buddy->status);
              buddy->status = s_strdup(msg->msg->status.status);*/
            status_change(lc,(LwqqMsgStatusChange*)msg->msg->opaque);
            break;
        default:
            printf("unknow message\n");
            break;
    }

    SIMPLEQ_REMOVE_HEAD(&l->head, entries);

    pthread_mutex_unlock(&l->mutex);
    lwqq_msg_free(msg->msg);
    s_free(msg);

}


static void login_complete(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* gc = ac->gc;
    LwqqErrorCode err = lwqq_async_get_error(lc,LOGIN_COMPLETE);
    if(err!=LWQQ_EC_OK)
        purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,"Network Error");

    purple_connection_set_state(gc,PURPLE_CONNECTED);

    //lwqq_async_add_listener(ac->qq,FRIENDS_ALL_COMPLETE,friends_complete);
    lwqq_async_add_listener(ac->qq,FRIEND_COME,friend_come);
    lwqq_async_add_listener(ac->qq,GROUP_COME,group_come);
    background_friends_info(ac);

    background_msg_poll(ac);
}

static void qq_login(PurpleAccount *account)
{
    PurpleConnection* pc= purple_account_get_connection(account);
    qq_account* ac = qq_account_new(account);
    const char* username = purple_account_get_username(account);
    const char* password = purple_account_get_password(account);
    ac->gc = pc;
    ac->qq = lwqq_client_new(username,password);
    lwqq_async_set(ac->qq,1);
    purple_connection_set_protocol_data(pc,ac);

    lwqq_async_set_userdata(ac->qq,LOGIN_COMPLETE,ac);
    lwqq_async_add_listener(ac->qq,LOGIN_COMPLETE,login_complete);
    background_login(ac);
}
static void qq_close(PurpleConnection *gc)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqErrorCode err;

    //lwqq_async_set(ac->qq,0);
    if(ac->qq->status!=NULL&&strcmp(ac->qq->status,"online")==0) {
        background_msg_drain(ac);
        lwqq_logout(ac->qq,&err);
    }
    lwqq_client_free(ac->qq);
    qq_account_free(ac);
    purple_connection_set_protocol_data(gc,NULL);
}

static int qq_send_im(PurpleConnection *gc, const gchar *who, const gchar *what, PurpleMessageFlags UNUSED(flags))
{
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    LwqqBuddy* buddy;
    LIST_FOREACH(buddy,&lc->friends,entries){
        if(strcmp(buddy->qqnumber,who)==0)
            break;
    }
    
    lwqq_msg_send_buddy(lc,buddy,what);

    return 1;
}

static int qq_send_chat(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
    char gid[32];
    snprintf(gid,32,"%u",id);
    printf("%s\n",gid);
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    LwqqGroup *group = lwqq_group_find_group_by_gid(lc,gid);

    lwqq_msg_send_group(lc,group,message);

    PurpleConversation* conv = purple_find_chat(gc,id);

    //write message by hand
    //purple_conv_chat_write(PURPLE_CONV_CHAT(conv),NULL,message,flags,time(NULL));
    purple_conversation_write(conv,NULL,message,flags,time(NULL));

    return 1;
}

GList *qq_chat_info(PurpleConnection *gc)
{
    GList *m;
    struct proto_chat_entry *pce;

    m = NULL;

    pce = g_new0(struct proto_chat_entry, 1);
    pce->label = "ID: ";
    pce->identifier = QQ_ROOM_KEY_QUN_ID;
    m = g_list_append(m, pce);

    return m;
}
GHashTable *qq_chat_info_defaults(PurpleConnection *gc, const gchar *chat_name)
{
    GHashTable *defaults;

    defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    if (chat_name != NULL)
        g_hash_table_insert(defaults, QQ_ROOM_KEY_QUN_ID, g_strdup(chat_name));

    return defaults;
}

static void group_member_list_come(LwqqClient* lc,void* data)
{
    LwqqGroup* group = (LwqqGroup*)data;
    qq_account* ac = lwqq_async_get_userdata(lc,GROUP_DETAIL);
    PurpleAccount* account = ac->account;
    LwqqBuddy* member;
    PurpleConvChatBuddyFlags flag;

    PurpleConversation* conv = purple_find_chat(
            purple_account_get_connection(ac->account),atoi(group->gid));
    //only there are no member we add it.
    if(purple_conv_chat_get_users(PURPLE_CONV_CHAT(conv))==NULL) {
        LIST_FOREACH(member,&group->members,entries) {
            flag |= PURPLE_CBFLAGS_TYPING;
            purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),member->qqnumber,NULL,flag,FALSE);
        }
    }
}
static void qq_group_join(PurpleConnection *gc, GHashTable *data)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    char* account = g_hash_table_lookup(data,QQ_ROOM_KEY_QUN_ID);
    LwqqGroup* group,*gp;
    LwqqBuddy* member;
    if(account!=NULL) {
        LIST_FOREACH(gp,&lc->groups,entries){
            if(strcmp(account,gp->account)==0){
                group = gp;
                break;
            }
        }
        if(group == NULL) return;
        //this will open chat dialog.
        qq_conv_open(gc,group);
        if(!LIST_EMPTY(&group->members)) {
            group_member_list_come(lc,group);
        } else {
            lwqq_async_add_listener(lc,GROUP_DETAIL,group_member_list_come);
            lwqq_async_set_userdata(lc,GROUP_DETAIL,ac);
            background_group_detail(ac,group);
        }
    }
    return;
}
static gboolean qq_offline_message(const PurpleBuddy *buddy)
{
    //webqq support offline message indeed
    return TRUE;
}

static void
init_plugin(PurplePlugin *UNUSED(plugin))
{
#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    bindtextdomain(GETTEXT_PACKAGE , LOCALE_DIR);
    textdomain(GETTEXT_PACKAGE);
#endif
    purple_debug_info(DBGID,"plugin has loaded\n");
}

PurplePluginProtocolInfo webqq_prpl_info = {
    /* options */
    .options=           0,
    .icon_spec=         {"svg,png", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_SEND},	/* icon_spec */
    .list_icon=         qq_list_icon,   /* list_icon */
    NULL,//twitter_status_text, /* status_text */
//	twitterim_tooltip_text,/* tooltip_text */
    .status_types=      qq_status_types,    /* status_types */
    NULL,                   /* blist_node_menu */
    .chat_info=         qq_chat_info,    /* chat_info implement this to enable chat*/
    .chat_info_defaults=qq_chat_info_defaults, /* chat_info_defaults */
    .join_chat=         qq_group_join,
    .login=             qq_login,       /* login */
    .close=             qq_close,       /* close */
    .send_im=           qq_send_im,     /* send_im */
    .chat_send=         qq_send_chat,
    .offline_message=   qq_offline_message,
    NULL,//twitter_set_status,/* set_status */
    NULL,                   /* set_idle */
    NULL,                   /* change_passwd */
//	twitterim_add_buddy,   /* add_buddy */
    NULL,
    NULL,                   /* add_buddies */
//	twitterim_remove_buddy,/* remove_buddy */
    NULL,
    .struct_size=           sizeof(PurplePluginProtocolInfo)
};

static PurplePluginInfo info = {
    .magic=         PURPLE_PLUGIN_MAGIC,
    .major_version= PURPLE_MAJOR_VERSION,
    .minor_version= PURPLE_MINOR_VERSION,
    .type=          PURPLE_PLUGIN_PROTOCOL, /* type */
    .flags=         0, /* flags */
    .priority=      PURPLE_PRIORITY_DEFAULT, /* priority */
    .id=            "prpl-webqq", /* id */
    .name=          "WebQQ", /* name */
    .version=       "0.1", /* version */
    .summary=       "WebQQ Protocol Plugin", /* summary */
    .description=   "a webqq plugin based on lwqq", /* description */
    .author=        "xiehuc<xiehuc@gmail.com>", /* author */
    .homepage=      "https://github.com/xiehuc/pidgin-lwqq",
    .extra_info=    &webqq_prpl_info, /* extra_info */
    .actions=       plugin_actions
};

PURPLE_INIT_PLUGIN(webqq, init_plugin, info)
