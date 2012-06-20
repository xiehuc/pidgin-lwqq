
#define PURPLE_PLUGINS
#include <plugin.h>
#include <version.h>
#include "internal.h"
#include "webqq.h"
#include "qq_types.h"
#include "qq_async.h"
#include "login.h"

#include <type.h>

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

qq_account* qq_account_new(PurpleAccount* account)
{
    qq_account* ac = g_malloc0(sizeof(qq_account));
    ac->account = account;
    qq_async_set(ac,1);
    return ac;
}
void qq_account_free(qq_account* ac)
{
    if(qq_async_enabled(ac))
        qq_async_set(ac,0);
    g_free(ac);
}
static void groups_complete(qq_account* ac,LwqqErrorCode err,void* data)
{
    LwqqClient* lc = ac->qq;
    LwqqGroup* group;
    /*LIST_FOREACH(group,&lc->groups,entries){
        if(purple_find_group(group->name)==NULL)
            purple_group_new(group->name);
    }*/
}
static void friends_complete(qq_account* ac,LwqqErrorCode err,void* data)
{
    LwqqClient* lc = ac->qq;
    PurpleAccount* account=ac->account;
    LwqqBuddy* buddy;
    PurpleBuddy* bu;
    PurpleGroup* gp;
    gp = purple_group_new("QQ");
    LIST_FOREACH(buddy,&lc->friends,entries){
        if(purple_find_buddy(ac->account,buddy->qqnumber)==NULL){
            bu = purple_buddy_new(ac->account,buddy->qqnumber,buddy->nick);
            //gp = purple_find_group(buddy->group);
            purple_blist_add_buddy(bu,NULL,gp,NULL);
        }
        purple_prpl_got_user_status(account, buddy->qqnumber, "online", NULL);
    }
}

static void msg_come(qq_account* ac,LwqqErrorCode err,void* data)
{
    LwqqRecvMsgList* l = ac->qq->msg_list;
    LwqqBuddy* buddy;
    PurpleConnection* pc = ac->gc;
    LwqqRecvMsg *msg;

    pthread_mutex_lock(&l->mutex);
    if (!SIMPLEQ_EMPTY(&l->head)) {
        msg = SIMPLEQ_FIRST(&l->head);
        if (msg->msg->content) {
            buddy = lwqq_buddy_find_buddy_by_uin(ac->qq,msg->msg->from);
            serv_got_im(pc, buddy->qqnumber, msg->msg->content, PURPLE_MESSAGE_RECV, time(NULL));
        }
        SIMPLEQ_REMOVE_HEAD(&l->head, entries);
    }
    pthread_mutex_unlock(&l->mutex);
}

static void login_complete(qq_account* ac,LwqqErrorCode err,void* data)
{
    PurpleConnection* gc = ac->gc;
    if(err!=LWQQ_EC_OK)
        purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,"Network Error");

    purple_connection_set_state(gc,PURPLE_CONNECTED);

    //qq_async_add_listener(ac,FRIENDS_COMPLETE,friends_complete,NULL);
    qq_async_add_listener(ac,GROUPS_COMPLETE,groups_complete,NULL);
    qq_async_add_listener(ac,FRIENDS_COMPLETE,friends_complete,NULL);
    background_friends_info(ac);
    qq_async_add_listener(ac,MSG_COME,msg_come,NULL);
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
    purple_connection_set_protocol_data(pc,ac);

    qq_async_add_listener(ac,LOGIN_COMPLETE,login_complete,NULL);
    background_login(ac);
}
static void qq_close(PurpleConnection *gc)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqErrorCode err;
    if(ac->qq->status!=NULL&&strcmp(ac->qq->status,"online")==0)
        lwqq_logout(ac->qq,&err);
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

	if(!(buddy = purple_find_buddy(ac->account, who))) return 0;
    LIST_FOREACH(item, &ac->qq->friends, entries) {
        if(strcmp(buddy->name,item->qqnumber)==0){
            friend=item;
            break;
        }
    }
    
    msg = lwqq_sendmsg_new(lc,friend->uin,"message",what);
    if(!msg) return 1;

    msg->send(msg);

    return 1;
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
	NULL,                   /* chat_info */
	NULL,                   /* chat_info_defaults */
	.login=             qq_login,       /* login */
	.close=             qq_close,       /* close */
	.send_im=           qq_send_im,     /* send_im */
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
