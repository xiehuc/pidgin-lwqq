
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
static void friends_complete(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account=ac->account;
    LwqqBuddy* buddy;
    LwqqFriendCategory* category;
    LwqqGroup* group;
    PurpleGroup** lst;
    int lst_len=0;
    PurpleBuddy* bu;
    PurpleGroup* gp;
    const char* gp_name;

    LIST_FOREACH(category,&lc->categories,entries) {
        lst_len = category->index>lst_len?category->index:lst_len;
    }
    lst_len++;//index 从1开始起始.
    lst = (PurpleGroup**)malloc(sizeof(PurpleGroup*)*lst_len);
    LIST_FOREACH(category,&lc->categories,entries) {
        lst[category->index]=purple_group_new(category->name);
    }
    lst[0] = purple_group_new("QQ好友");
    LIST_FOREACH(buddy,&lc->friends,entries) {
        bu = purple_buddy_new(ac->account,buddy->uin,buddy->nick);
        if(buddy->markname) purple_blist_alias_buddy(bu,buddy->markname);
        else purple_blist_alias_buddy(bu,buddy->nick);
        purple_blist_add_buddy(bu,NULL,lst[atoi(buddy->cate_index)],NULL);
        if(buddy->status)
            purple_prpl_got_user_status(account, buddy->uin, buddy->status, NULL);
    }

    free(lst);
}
static void groups_complete(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account=ac->account;
    LwqqGroup* group;
    PurpleChat *chat;
    PurpleGroup* gp = purple_group_new("QQ群");
    purple_blist_add_group(gp,NULL);
    GHashTable *components;
    LIST_FOREACH(group,&lc->groups,entries) {
        components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(components,g_strdup(QQ_ROOM_KEY_QUN_ID),g_strdup(group->gid));
        chat = purple_chat_new(account,group->gid,components);
        purple_blist_alias_chat(chat,group->name);
        purple_blist_add_chat(chat,gp,NULL);
    }
}
static void buddy_message(LwqqClient* lc,LwqqMsgMessage* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    serv_got_im(pc, msg->from, msg->content, PURPLE_MESSAGE_RECV, time(NULL));
}
static void group_message(LwqqClient* lc,LwqqMsgGroup* msg)
{
    printf("%s\n",msg->content);
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    LwqqGroup* group = lwqq_group_find_group_by_gid(lc,msg->from);

    //force open dialog
    serv_got_joined_chat(pc,atoi(group->gid),group->gid);
    serv_got_chat_in(pc,atoi(group->gid),msg->send,PURPLE_MESSAGE_RECV,msg->content,time(NULL));
    if(!LIST_EMPTY(&group->members)) {
        group_member_list_come(lc,group);
    } else {
        lwqq_async_add_listener(lc,GROUP_DETAIL,group_member_list_come);
        lwqq_async_set_userdata(lc,GROUP_DETAIL,ac);
        background_group_detail(ac,group);
    }

}
static void status_change(LwqqClient* lc,LwqqMsgStatus* status)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;

    purple_prpl_got_user_status(account,status->who,status->status,NULL);
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
    char *msg_type = msg->msg->msg_type;

    if (strcmp(msg_type, MT_MESSAGE) == 0) {
        buddy_message(lc,(LwqqMsgMessage*)msg->msg);
    } else if (strcmp(msg_type, MT_GROUP_MESSAGE) == 0) {
        group_message(lc,(LwqqMsgGroup*)msg->msg);
    } else if (strcmp(msg_type, MT_STATUS_CHANGE) == 0) {
        /*LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,msg->msg->status.who);
        if(buddy->status!=NULL)s_free(buddy->status);
        buddy->status = s_strdup(msg->msg->status.status);*/
        status_change(lc,(LwqqMsgStatus*)msg->msg);
    } else {
        printf("unknow message\n");
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

    lwqq_async_add_listener(ac->qq,FRIENDS_ALL_COMPLETE,friends_complete);
    lwqq_async_add_listener(ac->qq,GROUPS_ALL_COMPLETE,groups_complete);
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
    GSList* list = purple_blist_get_buddies();
    PurpleBuddy* buddy;
    //remove all buddy;
    while(list->next!=NULL) {
        buddy = list->data;
        list = list->next;
        purple_blist_remove_buddy(buddy);
    }
    PurpleGroup* qun = purple_find_group("QQ群");
    PurpleBlistNode * node;
    PurpleChat* chat;
    //remove all chat
    if(qun!=NULL) {
        node = purple_blist_node_get_first_child(PURPLE_BLIST_NODE(qun));
        while(node!=NULL) {
            if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
                chat = PURPLE_CHAT(node);
                node = purple_blist_node_next(node,1);
                purple_blist_remove_chat(chat);
            } else {
                node = purple_blist_node_next(node,1);
            }
        }
    }
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
    PurpleBuddy* buddy;
    LwqqBuddy* friend;
    LwqqBuddy* item;
    LwqqSendMsg* msg;
    LwqqClient* lc = ac->qq;

    msg = lwqq_sendmsg_new(lc,who,"message",what);
    if(!msg) return 1;

    msg->send(msg);

    return 1;
}

static int qq_send_chat(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
    char gid[32];
    snprintf(gid,32,"%u",id);
    printf("%s\n",gid);
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    LwqqSendMsg* msg;

    msg = lwqq_sendmsg_new(lc,gid,"group_message",message);
    if(!msg) return 1;

    msg->send(msg);

    return 1;
}

GList *qq_chat_info(PurpleConnection *gc)
{
    GList *m;
    struct proto_chat_entry *pce;

    m = NULL;

    pce = g_new0(struct proto_chat_entry, 1);
    pce->label = _("ID: ");
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

    PurpleConversation* conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                               group->gid,account);
    //only there are no member we add it.
    if(purple_conv_chat_get_users(PURPLE_CONV_CHAT(conv))==NULL) {
        LIST_FOREACH(member,&group->members,entries) {
            flag |= PURPLE_CBFLAGS_TYPING;
            purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),member->uin,NULL,flag,FALSE);
        }
    }
}

static void qq_group_join(PurpleConnection *gc, GHashTable *data)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    char* gid = g_hash_table_lookup(data,QQ_ROOM_KEY_QUN_ID);
    LwqqGroup* group;
    LwqqBuddy* member;
    if(gid!=NULL) {
        group = lwqq_group_find_group_by_gid(lc,gid);
        //this will open chat dialog.
        serv_got_joined_chat(gc,atoi(gid),group->gid);
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

PurplePluginProtocolInfo tsina_prpl_info = {
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
    .extra_info=    &tsina_prpl_info, /* extra_info */
    .actions=       plugin_actions
};

PURPLE_INIT_PLUGIN(webqq, init_plugin, info)
