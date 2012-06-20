
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

void login_complete(qq_account* ac,LwqqErrorCode err,void* data)
{
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
    g_free(ac);
    purple_connection_set_protocol_data(gc,NULL);
}

static int qq_send_im(PurpleConnection *gc, const gchar *who, const gchar *what, PurpleMessageFlags UNUSED(flags))
{
    return 1;
}


gboolean plugin_load(PurplePlugin* plugin)
{
    purple_debug_info(DBGID,"plugin loaded\n");
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
