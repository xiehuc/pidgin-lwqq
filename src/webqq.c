#define PURPLE_PLUGINS
#include <plugin.h>
#include <version.h>
#include <smemory.h>
#include <request.h>
#include <signal.h>
#include <accountopt.h>
#include <ft.h>

#include <type.h>
#include <async.h>
#include <msg.h>
#include <info.h>
#include <http.h>

#include "internal.h"
#include "qq_types.h"
#include "login.h"
#include "lwdb.h"
#include "background.h"
#include "translate.h"

enum RESET_OPT{RESET_BUDDY=0x1,RESET_GROUP=0x2,RESET_DISCU=0x4,RESET_ALL=-1};

#define try_get(val,fail) (val?val:fail)

char *qq_get_cb_real_name(PurpleConnection *gc, int id, const char *who);
static void client_connect_signals(PurpleConnection* gc);
static void group_member_list_come(LwqqAsyncEvent* event,void* data);
static void group_message_delay_display(LwqqAsyncEvent* event,void* data);
static void whisper_message_delay_display(LwqqAsyncEvent* event,void* data);
static void friend_avatar(LwqqAsyncEvent* ev,void* data);
static void group_avatar(LwqqAsyncEvent* ev,void* data);

static const char* get_name_from_file_from(qq_account* ac,const char* from_uin)
{
    if(ac->qq_use_qqnum){
        LwqqBuddy* buddy = find_buddy_by_uin(ac->qq,from_uin);
        return (buddy&&buddy->qqnumber) ?buddy->qqnumber:from_uin;
    }else
        return from_uin;
}

static const char* qq_get_type_from_chat(PurpleChat* chat)
{
    GHashTable* table = purple_chat_get_components(chat);
    return g_hash_table_lookup(table,QQ_ROOM_TYPE);
}
static LwqqGroup* qq_get_group_from_chat(PurpleChat* chat)
{
    PurpleAccount* account = purple_chat_get_account(chat);
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(account));
    LwqqClient* lc = ac->qq;
    LwqqGroup* ret = NULL;
    GHashTable* table = purple_chat_get_components(chat);
    const char* key = g_hash_table_lookup(table,QQ_ROOM_KEY_GID);
    ret = find_group_by_qqnumber(lc, key);
    if(ret == NULL)
        ret = find_group_by_gid(lc,key);
    return ret;
}
static LwqqGroup* find_group_by_name(LwqqClient* lc,const char* name)
{
    LwqqGroup* group;
    LIST_FOREACH(group,&lc->groups,entries) {
        if(group->name&&strcmp(group->name,name)==0)
            return group;
    }
    return NULL;
}
static LwqqSimpleBuddy* find_group_member_by_nick_or_card(LwqqGroup* group,const char* who)
{
    LwqqSimpleBuddy* sb;
    LIST_FOREACH(sb,&group->members,entries) {
        if(sb->nick&&strcmp(sb->nick,who)==0)
            return sb;
        if(sb->card&&strcmp(sb->card,who)==0)
            return sb;
    }
    return NULL;
}

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
static void visit_self_infocenter(PurplePluginAction *action)
{
    PurpleConnection* gc = action->context;
    qq_account* ac = purple_connection_get_protocol_data(gc);
    char url[256];
    snprintf(url,sizeof(url),"xdg-open 'http://user.qzone.qq.com/%s/infocenter'",ac->qq->myself->uin);
    system(url);
}
static void buddies_all_remove(void* data,void* userdata)
{
    PurpleBuddy* buddy = data;
    qq_account* ac = userdata;
    if(purple_buddy_get_account(buddy) == ac->account) {
        purple_blist_remove_buddy(buddy);
    }
}
//this remove all buddy and chat.
static void all_reset(qq_account* ac,int opt)
{
    if(opt & RESET_BUDDY){
        GSList* buddies = purple_blist_get_buddies();
        g_slist_foreach(buddies,buddies_all_remove,ac);
    }

    if(opt & (RESET_GROUP | RESET_DISCU)){
        //PurpleGroup* group = purple_find_group("QQ群");
        PurpleBlistNode *group,*node;
        for(group = purple_get_blist()->root;group!=NULL;group=group->next){
            node = group->child;
            while(node!=NULL) {
                if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
                    PurpleChat* chat = PURPLE_CHAT(node);
                    if(purple_chat_get_account(chat)==ac->account) {
                        node = purple_blist_node_next(node,1);
                        const char* type = qq_get_type_from_chat(chat);
                        if(!strcmp(type,QQ_ROOM_TYPE_QUN)){
                            if(opt & RESET_GROUP) purple_blist_remove_chat(chat);
                        }else{
                            if(opt & RESET_DISCU) purple_blist_remove_chat(chat);
                        }
                        continue;
                    }
                }
                node = purple_blist_node_next(node,1);
            }
        }
    }
}
static void all_reset_action(PurplePluginAction* action)
{
    PurpleConnection* gc = action->context;
    qq_account* ac = purple_connection_get_protocol_data(gc);

    all_reset(ac,RESET_ALL);

    purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,"全部重载,请重新登录");
}
static void visit_my_qq_center(PurplePluginAction* action)
{
    //PurpleConnection* gc = action->context;
    //qq_account* ac = purple_connection_get_protocol_data(gc);
    char buf[512];
    snprintf(buf,sizeof(buf),"xdg-open 'http://ptlogin2.qq.com/login?u=2501542492&p=146FA572EB4E2E1251BB197D7125E630&verifycode=!DGM&aid=1006102&u1=http5%%3A%%2F%%2Fid.qq.com%%2Findex.html%23myfriends&h=1&ptredirect=1&ptlang=2052&from_ui=1&dumy=&fp=loginerroralert&action=2-5-10785&mibao_css=&t=1&g=1'");
    system(buf);

    //system("xdg-open 'http://id.qq.com/#myfriends'");

}

static GList *plugin_actions(PurplePlugin *UNUSED(plugin), gpointer context)
{

    GList *m;
    PurplePluginAction *act;

    m = NULL;

    ///分割线
    m = g_list_append(m, NULL);

    act = purple_plugin_action_new(_("About WebQQ"), action_about_webqq);
    m = g_list_append(m, act);
    act = purple_plugin_action_new("访问个人中心",visit_self_infocenter);
    m = g_list_append(m, act);
    act = purple_plugin_action_new("好友管理",visit_my_qq_center);
    m = g_list_append(m, act);
    act = purple_plugin_action_new("全部重载",all_reset_action);
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
#define WEBQQ_STATUS_TYPE_ATTR \
    TRUE,TRUE,FALSE,\
    "nick","nick",purple_value_new(PURPLE_TYPE_STRING),\
    "mark","mark",purple_value_new(PURPLE_TYPE_STRING),NULL


    status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
    //status = purple_status_type_new_with_attrs(PURPLE_STATUS_MOBILE,
             "online", _("我在线上"), WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
             "callme",_("Q我吧"),WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY,
             "away", _("离开"), WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE,
             "busy", _("忙碌"), WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE,
             "slience", _("请勿打扰"), WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_INVISIBLE,
             "hidden", _("隐身"), WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE,
             "offline", _("离线"), WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_MOBILE,
             "mobile", _("我在线上"), WEBQQ_STATUS_TYPE_ATTR);
    types = g_list_append(types, status);

#undef WEBQQ_STATUS_TYPE_ATTR
    return types;
}
static void qq_set_status(PurpleAccount* account,PurpleStatus* status)
{
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(account));
    lwqq_info_change_status(ac->qq,lwqq_status_from_str(purple_status_get_id(status)));
}

#define buddy_status(bu) ((bu->stat == LWQQ_STATUS_ONLINE && bu->client_type == LWQQ_CLIENT_MOBILE) \
        ? "mobile":lwqq_status_to_str(bu->stat))

static int friend_come(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    ac->disable_send_server = 1;
    PurpleAccount* account=ac->account;
    LwqqBuddy* buddy = data;
    PurpleBuddy* bu = NULL;
    LwqqFriendCategory* cate;

    int cate_index;
    cate_index = (buddy->cate_index) ? atoi(buddy->cate_index) : 0;
    PurpleGroup* group = purple_group_new("QQ好友");
    if(cate_index != 0) {
        LIST_FOREACH(cate,&lc->categories,entries) {
            if(cate->index==cate_index) {
                group = purple_group_new(cate->name);
                break;
            }
        }
    }

    const char* key = try_get(buddy->qqnumber,buddy->uin);
    bu = purple_find_buddy(account,key);
    if(bu == NULL) {
        bu = purple_buddy_new(ac->account,key,(buddy->markname)?buddy->markname:buddy->nick);
        purple_blist_add_buddy(bu,NULL,group,NULL);
    }
    //flush new alias
    /*const char* alias = purple_buddy_get_alias_only(bu);
    if(buddy->markname){
        if(alias==NULL||strcmp(alias,buddy->markname)!=0)
            purple_blist_alias_buddy(bu,buddy->markname);
    }else if(alias==NULL||strcmp(alias,buddy->nick)!=0){
        purple_blist_alias_buddy(bu,buddy->nick);
    }*/
    if(purple_buddy_get_group(bu)!=group) {
        purple_blist_add_buddy(bu,NULL,group,NULL);
    }
    if(buddy->stat)
        purple_prpl_got_user_status(account, key, buddy_status(buddy), NULL);
    //download avatar
    PurpleBuddyIcon* icon;
    if((icon = purple_buddy_icons_find(account,key))==0) {
        void **d = s_malloc0(sizeof(void*)*2);
        d[0] = ac;
        d[1] = buddy;
        lwqq_async_add_event_listener(
                lwqq_info_get_friend_avatar(lc,buddy),friend_avatar,d);
    } //else {
    //purple_buddy_set_icon(purple_find_buddy(account,key),icon);
    //}

    qq_account_insert_index_node(ac, NODE_IS_BUDDY, buddy);

    ac->disable_send_server = 0;
    return 0;
}
static const char* group_name(LwqqGroup* group)
{
    static char gname[128];
    if(group->markname) {
        strncpy(gname,group->markname,sizeof(gname));
    } else 
        strncpy(gname,group->name,sizeof(gname));
    if(group->mask == LWQQ_MASK_ALL)
        strcat(gname,"(屏蔽)");
    return gname;
}
static int group_come(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    ac->disable_send_server = 1;
    PurpleAccount* account=ac->account;
    LwqqGroup* group = data;
    PurpleGroup* qun = purple_group_new("QQ群");
    PurpleGroup* talk = purple_group_new("讨论组");
    GHashTable* components;
    PurpleChat* chat;
    const char* type;

    if( (chat = purple_blist_find_chat(account,try_get(group->account,group->gid))) == NULL) {
        components = g_hash_table_new_full(g_str_hash, g_str_equal, NULL , g_free);
        g_hash_table_insert(components,QQ_ROOM_KEY_GID,g_strdup(group->account));
        type = (group->type==LWQQ_GROUP_QUN)? QQ_ROOM_TYPE_QUN:QQ_ROOM_TYPE_DISCU;
        g_hash_table_insert(components,QQ_ROOM_TYPE,g_strdup(type));
        chat = purple_chat_new(account,try_get(group->account,group->gid),components);
        purple_blist_add_chat(chat,lwqq_group_is_qun(group)?qun:talk,NULL);
    } 

    if(lwqq_group_is_qun(group)){
        purple_blist_alias_chat(chat,group_name(group));
        if(purple_buddy_icons_node_has_custom_icon(PURPLE_BLIST_NODE(chat))==0){
            void **d = s_malloc0(sizeof(void*)*2);
            d[0] = ac;
            d[1] = group;
            lwqq_async_add_event_listener(
                    lwqq_info_get_group_avatar(lc,group),group_avatar,d);
        }
    }else{
        purple_blist_alias_chat(chat,group_name(group));
        purple_blist_node_set_flags(PURPLE_BLIST_NODE(chat), PURPLE_BLIST_NODE_FLAG_NO_SAVE);
    }

    qq_account_insert_index_node(ac, NODE_IS_GROUP, group);

    ac->disable_send_server = 0;
    return 0;
}
#define discu_come(lc,data) (group_come(lc,data))
static void buddy_message(LwqqClient* lc,LwqqMsgMessage* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    static char buf[8192];
    //clean buffer
    strcpy(buf,"");

    translate_struct_to_message(ac,msg,buf);
    serv_got_im(pc, get_name_from_file_from(ac,msg->from), buf, PURPLE_MESSAGE_RECV, msg->time);
}
static void offline_file(LwqqClient* lc,LwqqMsgOffFile* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    char buf[512];
    snprintf(buf,sizeof(buf),"您收到一个离线文件:%s\n"
             "到期时间:%s"
             "<a href=\"%s\">点此下载</a>",
             msg->name,ctime(&msg->expire_time),lwqq_msg_offfile_get_url(msg));
    serv_got_im(pc,get_name_from_file_from(ac,msg->from),buf,PURPLE_MESSAGE_RECV|PURPLE_MESSAGE_SYSTEM,time(NULL));
}
static void notify_offfile(LwqqClient* lc,LwqqMsgNotifyOfffile* notify)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    char buf[512];
    const char* action = (notify->action==NOTIFY_OFFFILE_REFUSE)?"拒绝":"同意";
    snprintf(buf,sizeof(buf),"对方%s接受离线文件(%s)\n",action,notify->filename);
    serv_got_im(pc,get_name_from_file_from(ac,notify->from),buf,PURPLE_MESSAGE_RECV|PURPLE_MESSAGE_SYSTEM,time(NULL));
}
//open chat conversation dialog
static void qq_conv_open(PurpleConnection* gc,LwqqGroup* group)
{
    g_return_if_fail(group);
    g_return_if_fail(group->gid);
    qq_account* ac = purple_connection_get_protocol_data(gc);
    PurpleConversation *conv = purple_find_chat(gc,opend_chat_search(ac,group));
    if(conv == NULL&&group->gid) {
        serv_got_joined_chat(gc,open_new_chat(ac,group),try_get(group->account,group->gid));
    }
}
static int group_message(LwqqClient* lc,LwqqMsgMessage* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    LwqqGroup* group = find_group_by_gid(lc,(msg->type==LWQQ_MT_DISCU_MSG)?msg->discu.did:msg->from);

    if(group == NULL) return FAILED;

    //force open dialog
    qq_conv_open(pc,group);
    static char buf[8192] ;
    strcpy(buf,"");

    translate_struct_to_message(ac,msg,buf);

    LwqqErrorCode err;
    void **data = s_malloc0(sizeof(void*)*5);
    data[0] = ac;
    data[1] = group;
    data[2] = s_strdup(msg->group.send);
    //because buf is not always too long .so it is not slowdown performance.
    data[3] = s_strdup(buf);
    data[4] = (void*)msg->time;
    //get all member list
    if(LIST_EMPTY(&group->members)) {
        //use block method to get list
        lwqq_async_add_event_listener(
            lwqq_info_get_group_detail_info(lc,group,&err),
            group_message_delay_display,
            data);
    } else {
        group_message_delay_display(NULL,data);
    }
    return SUCCESS;
}
static void group_message_delay_display(LwqqAsyncEvent* event,void* data)
{
    void **d = data;
    qq_account* ac = d[0];
    LwqqGroup* group = d[1];
    char* sender = d[2];
    char* buf = d[3];
    time_t t = (time_t)d[4];
    //LwqqClient* lc = ac->qq;
    PurpleConnection* pc = ac->gc;
    const char* who;

    if(purple_find_buddy(ac->account,sender)!=NULL) {
        who = sender;
    } else {
        LwqqSimpleBuddy* sb = lwqq_group_find_group_member_by_uin(group,sender);
        if(sb)
            who = sb->card?sb->card:sb->nick;
        else
            who = sender;
    }

    serv_got_chat_in(pc,opend_chat_search(ac,group),who,PURPLE_MESSAGE_RECV,buf,t);
    s_free(sender);
    s_free(buf);
    group_member_list_come(NULL,data);
}
static void whisper_message(LwqqClient* lc,LwqqMsgMessage* mmsg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;

    const char* from = mmsg->from;
    const char* gid = mmsg->sess.id;
    char name[70];
    static char buf[8192];
    strcpy(buf,"");

    translate_struct_to_message(ac,mmsg,buf);

    LwqqGroup* group = find_group_by_gid(lc,gid);
    if(group == NULL) {
        snprintf(name,sizeof(name),"%s #(broken)# %s",from,gid);
        serv_got_im(pc,name,buf,PURPLE_MESSAGE_RECV,mmsg->time);
        return;
    }
    void** data = s_malloc0(sizeof(void*)*5);
    data[0] = pc;
    data[1] = group;
    data[2] = s_strdup(from);
    data[3] = s_strdup(buf);
    data[4] = (void*)mmsg->time;
    if(LIST_EMPTY(&group->members)) {
        lwqq_async_add_event_listener(
            lwqq_info_get_group_detail_info(lc,group,NULL),
            whisper_message_delay_display,
            data);
    } else
        whisper_message_delay_display(NULL,data);
}
static void whisper_message_delay_display(LwqqAsyncEvent* event,void* data)
{
    void**d = data;
    PurpleConnection* pc = d[0];
    LwqqGroup* group = d[1];
    char* from = d[2];
    char* msg = d[3];
    time_t t = (time_t)d[4];
    s_free(data);
    char name[70];
    LwqqSimpleBuddy* sb = lwqq_group_find_group_member_by_uin(group,from);
    if(sb == NULL) {
        snprintf(name,sizeof(name),"%s #(broken)# %s",from,group->name);
    } else {
        snprintf(name,sizeof(name),"%s ### %s",(sb->card)?sb->card:sb->nick,group->name);
    }
    serv_got_im(pc,name,msg,PURPLE_MESSAGE_RECV,t);
}
static void status_change(LwqqClient* lc,LwqqMsgStatusChange* status)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;
    const char* who;
    if(ac->qq_use_qqnum){
        LwqqBuddy* buddy = find_buddy_by_uin(lc, status->who);
        if(buddy==NULL || buddy->qqnumber == NULL) return;
        who = buddy->qqnumber;
    }else{
        who = status->who;
    }
    purple_prpl_got_user_status(account,who,status->status,NULL);
}
static void kick_message(LwqqClient* lc,LwqqMsgKickMessage* kick)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    char* reason;
    if(kick->show_reason) reason = kick->reason;
    else reason = "您被不知道什么东西踢下线了额";
    purple_connection_error_reason(ac->gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,reason);
}
static void verify_required_confirm(void* data,PurpleRequestFields* root)
{
    void** d = data;
    qq_account* ac = d[0];
    char* account = d[1];
    s_free(data);
    LwqqClient* lc = ac->qq;
    int answer = purple_request_fields_get_choice(root,"answer");
    if(answer == 0) {
        lwqq_info_allow_and_add(lc,account,NULL);
    } else if(answer == 1) {
        lwqq_info_allow_added_request(lc,account);
    } else if(answer == 2) {
        lwqq_info_deny_added_request(lc,account,"no reason");
    }
    s_free(account);
}
static void verify_required_cancel(void* data,PurpleRequestFields* root)
{
}
static void system_message(LwqqClient* lc,LwqqMsgSystem* system)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    char buf1[128];
    char buf2[128];
    if(system->type == VERIFY_REQUIRED) {

        PurpleRequestFields* root = purple_request_fields_new();
        PurpleRequestFieldGroup *container = purple_request_field_group_new("好友确认");
        purple_request_fields_add_group(root,container);
        snprintf(buf1,sizeof(buf1),"%s请求加您为好友",system->account);
        snprintf(buf2,sizeof(buf2),"附加信息:%s",system->verify_required.msg);

        PurpleRequestField* choice = purple_request_field_choice_new("answer","请选择",0);
        purple_request_field_choice_add(choice,"同意并加为好友");
        purple_request_field_choice_add(choice,"同意");
        purple_request_field_choice_add(choice,"拒绝");
        purple_request_field_group_add_field(container,choice);

        void** data = s_malloc(sizeof(void*)*2);
        data[0] = ac;
        data[1] = s_strdup(system->account);
        purple_request_fields(ac->gc,NULL,buf1,buf2,root,
                              "确认",(GCallback)verify_required_confirm,
                              "取消",(GCallback)verify_required_cancel,
                              ac->account,NULL,NULL,data);
    } else if(system->type == VERIFY_PASS_ADD) {
        snprintf(buf1,sizeof(buf1),"%s通过了您的请求,并添加您为好友",system->account);
        purple_notify_message(ac->gc,PURPLE_NOTIFY_MSG_INFO,"系统消息","添加好友",buf1,NULL,NULL);
    } else if(system->type == VERIFY_PASS) {
        snprintf(buf1,sizeof(buf1),"%s通过了您的请求",system->account);
        purple_notify_message(ac->gc,PURPLE_NOTIFY_MSG_INFO,"系统消息","添加好友",buf1,NULL,NULL);
    }
}
static void friend_avatar(LwqqAsyncEvent* ev,void* data)
{
    void **d = data;
    qq_account* ac = d[0];
    LwqqBuddy* buddy = d[1];
    s_free(data);

    PurpleAccount* account = ac->account;
    if(buddy->avatar_len==0)return ;

    const char* key = try_get(buddy->qqnumber,buddy->uin);
    if(strcmp(buddy->uin,purple_account_get_username(account))==0) {
        purple_buddy_icons_set_account_icon(account,(guchar*)buddy->avatar,buddy->avatar_len);
    } else {
        purple_buddy_icons_set_for_user(account,key,(guchar*)buddy->avatar,buddy->avatar_len,NULL);
    }
    buddy->avatar = NULL;
}
static void group_avatar(LwqqAsyncEvent* ev,void* data)
{
    void **d = data;
    qq_account* ac = d[0];
    LwqqGroup* group = d[1];
    s_free(data);
    
    PurpleAccount* account = ac->account;
    PurpleChat* chat;
    if(group->avatar_len==0)return ;

    chat = purple_blist_find_chat(account,try_get(group->account,group->gid));
    if(chat==NULL) return ;
    purple_buddy_icons_node_set_custom_icon(PURPLE_BLIST_NODE(chat),(guchar*)group->avatar,group->avatar_len);
    //let free by purple;
    group->avatar = NULL;
    return ;
}
static int lost_connection(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* gc = ac->gc;
    purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,"webqq掉线了,请重新登录");
    return 0;
}
int qq_msg_check(LwqqClient* lc,void* data)
{
    LwqqRecvMsgList* l = lc->msg_list;
    LwqqRecvMsg *msg,*prev;

    pthread_mutex_lock(&l->mutex);
    if (TAILQ_EMPTY(&l->head)) {
        /* No message now, wait 100ms */
        pthread_mutex_unlock(&l->mutex);
        return 0;
    }
    msg = TAILQ_FIRST(&l->head);
    while(msg) {
        int res = SUCCESS;
        if(msg->msg) {
            switch(msg->msg->type) {
            case LWQQ_MT_BUDDY_MSG:
                buddy_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
                break;
            case LWQQ_MT_GROUP_MSG:
            case LWQQ_MT_DISCU_MSG:
                res = group_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
                break;
            case LWQQ_MT_SESS_MSG:
                whisper_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
                break;
            case LWQQ_MT_STATUS_CHANGE:
                status_change(lc,(LwqqMsgStatusChange*)msg->msg->opaque);
                break;
            case LWQQ_MT_KICK_MESSAGE:
                kick_message(lc,(LwqqMsgKickMessage*)msg->msg->opaque);
                break;
            case LWQQ_MT_SYSTEM:
                system_message(lc,(LwqqMsgSystem*)msg->msg->opaque);
                break;
            case LWQQ_MT_BLIST_CHANGE:
                //do no thing. it will raise friend_come
                break;
            case LWQQ_MT_OFFFILE:
                offline_file(lc,(LwqqMsgOffFile*)msg->msg->opaque);
                break;
            case LWQQ_MT_FILE_MSG:
                file_message(lc,(LwqqMsgFileMessage*)msg->msg->opaque);
                break;
            case LWQQ_MT_FILETRANS:
                //complete_file_trans(lc,(LwqqMsgFileTrans*)msg->msg->opaque);
                break;
            case LWQQ_MT_NOTIFY_OFFFILE:
                notify_offfile(lc,(LwqqMsgNotifyOfffile*)msg->msg->opaque);
                break;
            default:
                printf("unknow message\n");
                break;
            }
        }

        prev = msg;
        msg = TAILQ_NEXT(msg,entries);
        if(res == SUCCESS){
            TAILQ_REMOVE(&l->head,prev, entries);
            lwqq_msg_free(prev->msg);
            s_free(prev);
        }
    }
    pthread_mutex_unlock(&l->mutex);
    return 0;

}

int qq_sys_msg_write(LwqqClient* lc,void* data)
{
    system_msg* msg = data;
    PurpleConversation* conv = find_conversation(msg->msg_type,msg->who,msg->ac);
    if(conv)
        purple_conversation_write(conv,NULL,msg->msg,msg->type,msg->t);
    system_msg_free(msg);
    return 0;
}

/*static void check_exist(void* data,void* userdata)
{
    qq_account* ac = userdata;
    PurpleBuddy* bu = data;
    LwqqClient* lc = ac->qq;
    if(purple_buddy_get_account(bu)==ac->account&&
            find_buddy_by_qqnumber(lc,purple_buddy_get_name(bu))==NULL){
        purple_blist_remove_buddy(bu);
    }
}*/
static void write_buddy_to_db(LwqqAsyncEvent* ev,void* data)
{
    void **d = data;
    qq_account* ac = d[0];
    LwqqBuddy* buddy = d[1];
    s_free(data);

    lwdb_userdb_insert_buddy_info(ac->db, buddy);
    friend_come(ac->qq,buddy);
}
static void write_group_to_db(LwqqAsyncEvent* ev,void* data)
{
    void **d = data;
    qq_account* ac = d[0];
    LwqqGroup* group = d[1];
    s_free(data);

    lwdb_userdb_insert_group_info(ac->db, group);
    group_come(ac->qq,group);
}

int qq_set_basic_info(LwqqClient* lc,void* data)
{
    qq_account* ac = data;
    //LwqqClient* lc = ac->qq;
    purple_account_set_alias(ac->account,lc->myself->nick);
    if(purple_buddy_icons_find_account_icon(ac->account)==NULL){
        void **d = s_malloc0(sizeof(void*)*2);
        d[0] = ac;
        d[1] = lc->myself;
        lwqq_async_add_event_listener(
                lwqq_info_get_friend_avatar(lc,lc->myself),friend_avatar,d);
    }

    if(ac->qq_use_qqnum)
    lwdb_userdb_write_to_client(ac->db, lc);
    
    LwqqBuddy* buddy;
    LIST_FOREACH(buddy,&lc->friends,entries) {
        if(ac->qq_use_qqnum && ! buddy->qqnumber){
            void **d = s_malloc0(sizeof(void*)*2);
            d[0] = ac;
            d[1] = buddy;
            lwqq_async_add_event_listener(
                lwqq_info_get_friend_qqnumber(lc,buddy),write_buddy_to_db,d);
        }
        else{
            friend_come(lc,buddy);
        }
    }
    LwqqGroup* group;
    LIST_FOREACH(group,&lc->groups,entries) {
        if(ac->qq_use_qqnum && ! group->account){
            void **d = s_malloc0(sizeof(void*)*2);
            d[0] = ac;
            d[1] = group;
            lwqq_async_add_event_listener(
                lwqq_info_get_group_qqnumber(lc,group),write_group_to_db,d);
        }else{
            group_come(lc,group);
            printf("%s:%s\n",group->name,group->account);
        }
    }
    LwqqGroup* discu;
    LIST_FOREACH(discu,&lc->discus,entries) {
        discu_come(lc,discu);
    }

    ac->state = LOAD_COMPLETED;

    background_msg_poll(ac);

    return 0;
}

static void pic_ok_cb(qq_account *ac, PurpleRequestFields *fields)
{
    const gchar *code;
    code = purple_request_fields_get_string(fields, "code_entry");
    ac->qq->vc->str = s_strdup(code);
    background_login(ac);
}
static void pic_cancel_cb(qq_account* ac, PurpleRequestFields *fields)
{
    PurpleConnection* gc = purple_account_get_connection(ac->account);
    purple_connection_error_reason(gc,
                                   PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                   _("Login Failed."));
}
static int verify_come(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleRequestFieldGroup *field_group;
    PurpleRequestField *code_entry;
    PurpleRequestField *code_pic;
    PurpleRequestFields *fields;

    fields = purple_request_fields_new();
    field_group = purple_request_field_group_new((gchar*)0);
    purple_request_fields_add_group(fields, field_group);

    code_pic = purple_request_field_image_new("code_pic", _("Confirmation code"), lc->vc->data, lc->vc->size);
    purple_request_field_group_add_field(field_group, code_pic);

    code_entry = purple_request_field_string_new("code_entry", _("Please input the code"), "", FALSE);
    purple_request_field_set_required(code_entry,TRUE);
    purple_request_field_group_add_field(field_group, code_entry);

    purple_request_fields(ac->account, NULL,
                          "验证码", (gchar*)0,
                          fields, _("OK"), G_CALLBACK(pic_ok_cb),
                          _("Cancel"), G_CALLBACK(pic_cancel_cb),
                          ac->account, NULL, NULL, ac);

    return 0;
}
static int login_complete(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* gc = purple_account_get_connection(ac->account);
    LwqqErrorCode err = lwqq_async_get_error(lc,LOGIN_COMPLETE);
    if(err==LWQQ_EC_LOGIN_ABNORMAL) {
        purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,"帐号出现问题,需要解禁");
        return 0;
    } else if(err!=LWQQ_EC_OK) {
        purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,lc->last_err);
        return 0;
    }

    purple_connection_set_state(gc,PURPLE_CONNECTED);
    ac->state = CONNECTED;

    /*if(ac->compatible_pidgin_conversation_integration){
        //this is for pidgin-conversation plugin for gnome-shell hot fix
        purple_buddy_icons_set_caching(1);
        mkdir("/tmp/lwqq/icon_cache",0777);
        purple_buddy_icons_set_cache_dir("/tmp/lwqq/icon_cache");
    }else{
        purple_buddy_icons_set_caching(0);
    }*/

    gc->flags |= PURPLE_CONNECTION_HTML;

    lwqq_async_add_listener(ac->qq,FRIEND_COMPLETE,qq_set_basic_info);
    lwqq_async_add_listener(ac->qq,POLL_LOST_CONNECTION,lost_connection);
    lwqq_async_add_listener(ac->qq,POLL_MSG_COME,qq_msg_check);
    lwqq_async_add_listener(ac->qq,SYS_MSG_COME,qq_sys_msg_write);
    background_friends_info(ac);
    return 0;
}

//send back receipt
static void send_receipt(LwqqAsyncEvent* ev,void* data)
{
    void **d = data;
    qq_account* ac = d[0];
    LwqqMsg* msg = d[1];
    char* who = d[2];
    char* what = d[3];
    s_free(data);

    int err = lwqq_async_event_get_result(ev);
    static char buf[1024];
    PurpleConversation* conv = find_conversation(msg->type,who,ac);

    if(err == LWQQ_MC_LOST_CONN){
        lwqq_async_dispatch(ac->qq,POLL_LOST_CONNECTION,NULL);
    }
    if(conv && err > 0){
        if(err == LWQQ_MC_TOO_FAST)
            snprintf(buf,sizeof(buf),"发送速度过快:\n%s",what);
        else
            snprintf(buf,sizeof(buf),"发送失败(err:%d):\n%s",err,what);
        lwqq_async_dispatch(ac->qq,SYS_MSG_COME,system_msg_new(msg->type,who,ac,buf,PURPLE_MESSAGE_ERROR,time(NULL)));
    }

    LwqqMsgMessage* mmsg = msg->opaque;
    if(mmsg->type == LWQQ_MT_GROUP_MSG) mmsg->group.group_code = NULL;
    else if(mmsg->type == LWQQ_MT_DISCU_MSG) mmsg->discu.did = NULL;
    s_free(what);
    s_free(who);
    lwqq_msg_free(msg);
}

//send a message to a friend.
//called by purple
static int qq_send_im(PurpleConnection *gc, const gchar *who, const gchar *what, PurpleMessageFlags UNUSED(flags))
{
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    char nick[32],gname[32];
    const char* pos;
    LwqqMsg* msg;
    LwqqMsgMessage *mmsg;
    if((pos = strstr(who," ### "))!=NULL) {
        strcpy(gname,pos+strlen(" ### "));
        strncpy(nick,who,pos-who);
        nick[pos-who] = '\0';
        msg = lwqq_msg_new(LWQQ_MT_SESS_MSG);
        mmsg = msg->opaque;
        LwqqGroup* group = find_group_by_name(lc,gname);
        LwqqSimpleBuddy* sb = find_group_member_by_nick_or_card(group,nick);
        mmsg->to = s_strdup(sb->uin);
        if(!sb->group_sig)
            lwqq_info_get_group_sig(lc,group,sb->uin);
        mmsg->sess.group_sig = s_strdup(sb->group_sig);
    } else {
        msg = lwqq_msg_new(LWQQ_MT_BUDDY_MSG);
        mmsg = msg->opaque;
        if(ac->qq_use_qqnum){
            LwqqBuddy* buddy = find_buddy_by_qqnumber(lc,who);
            if(buddy)
                mmsg->to = s_strdup(buddy->uin);
            else mmsg->to = s_strdup(who);
        }else{
            mmsg->to = s_strdup(who);
        }
    }
    mmsg->f_name = s_strdup("宋体");
    mmsg->f_size = 13;
    mmsg->f_style.b = 0,mmsg->f_style.i = 0,mmsg->f_style.u = 0;
    mmsg->f_color = s_strdup("000000");
    //PurpleConversation* conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,who,ac->account);

    translate_message_to_struct(lc, who, what, msg, 0);

    void **d = s_malloc0(sizeof(void*)*4);
    d[0] = ac;
    d[1] = msg;
    d[2] = s_strdup(who);
    d[3] = s_strdup(what);
    lwqq_async_add_event_listener(lwqq_msg_send(lc,msg), send_receipt, d);

    return 1;
}

static int qq_send_chat(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqGroup* group = opend_chat_index(ac,id);
    LwqqMsg* msg;

    
    msg = lwqq_msg_new(LWQQ_MT_GROUP_MSG);
    LwqqMsgMessage *mmsg = msg->opaque;
    mmsg->to = s_strdup(group->gid);
    if(group->type == LWQQ_GROUP_QUN){
        msg->type = mmsg->type =  LWQQ_MT_GROUP_MSG;
        mmsg->group.group_code = group->code;
    }else if(group->type == LWQQ_GROUP_DISCU){
        msg->type = mmsg->type =  LWQQ_MT_DISCU_MSG;
        mmsg->discu.did = group->did;
    }
    mmsg->f_name = s_strdup("宋体");
    mmsg->f_size = 13;
    mmsg->f_style.b = 0,mmsg->f_style.i = 0,mmsg->f_style.u = 0;
    mmsg->f_color = s_strdup("000000");
    PurpleConversation* conv = purple_find_chat(gc,id);

    translate_message_to_struct(ac->qq, group->gid, message, msg, 1);

    void **d = s_malloc0(sizeof(void*)*4);
    d[0] = ac;
    d[1] = msg;
    d[2] = s_strdup(group->gid);
    d[3] = s_strdup(message);
    lwqq_async_add_event_listener(lwqq_msg_send(ac->qq,msg), send_receipt, d);
    //background_send_msg(ac,msg,group->gid,message,conv);

    //write message by hand
    purple_conversation_write(conv,NULL,message,flags,time(NULL));

    return 1;
}

#if 0
static void qq_leave_chat(PurpleConnection* gc,int id)
{
    printf("test\n");
}

//pidgin not use send_whisper .
//may use it in v 3.0.0
static void qq_send_whisper(PurpleConnection* gc,int id,const char* who,const char* message)
{
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    LwqqGroup* group = opend_chat_index(ac,id);

    LwqqBuddy* buddy = find_buddy_by_uin(lc,who);
    if(buddy!=NULL) {
        qq_send_im(gc,who,message,PURPLE_MESSAGE_WHISPER);
        return;
    }

    LwqqSimpleBuddy* sb = find_group_member_by_nick(group,who);
    if(sb==NULL)
        return;

    LwqqMsg* msg = lwqq_msg_new(LWQQ_MT_SESS_MSG);
    LwqqMsgMessage *mmsg = msg->opaque;
    mmsg->to = sb->uin;
    if(!sb->group_sig)
        lwqq_info_get_group_sig(lc,group,sb->uin);
    mmsg->group_sig = sb->group_sig;
    mmsg->f_name = "宋体";
    mmsg->f_size = 13;
    mmsg->f_style.b = 0,mmsg->f_style.i = 0,mmsg->f_style.u = 0;
    mmsg->f_color = "000000";

    lwqq_msg_send(lc,msg);

}
#endif

GList *qq_chat_info(PurpleConnection *gc)
{
    GList *m;
    struct proto_chat_entry *pce;

    m = NULL;

    pce = g_new0(struct proto_chat_entry, 1);
    pce->label = "QQ: ";
    pce->identifier = QQ_ROOM_KEY_GID;
    m = g_list_append(m, pce);

    return m;
}
static GHashTable *qq_chat_info_defaults(PurpleConnection *gc, const gchar *chat_name)
{
    GHashTable *defaults;

    defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    if (chat_name != NULL){
        g_hash_table_insert(defaults, QQ_ROOM_KEY_GID, g_strdup(chat_name));
        g_hash_table_insert(defaults, QQ_ROOM_TYPE,g_strdup(QQ_ROOM_TYPE_QUN));
    }

    return defaults;
}
static PurpleRoomlist* qq_get_all_group_list(PurpleConnection* gc)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    PurpleAccount* account = ac->account;
    PurpleRoomlist* list = purple_roomlist_new(account);

    GList* fields = NULL;
    PurpleRoomlistField* field;
    field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING,"QQ号",QQ_ROOM_KEY_GID,FALSE);
    fields = g_list_append(fields,field);
    field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING,"类型",QQ_ROOM_TYPE,FALSE);
    fields = g_list_append(fields,field);
    purple_roomlist_set_fields(list,fields);

    LwqqGroup* group;
    LIST_FOREACH(group,&ac->qq->groups,entries) {
        PurpleRoomlistRoom* room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,group->name,NULL);
        purple_roomlist_room_add_field(list,room,group->gid);
        purple_roomlist_room_add_field(list,room,QQ_ROOM_TYPE_QUN);
        purple_roomlist_room_add(list,room);
    }
    LwqqGroup* discu;
    LIST_FOREACH(discu,&ac->qq->discus,entries){
        PurpleRoomlistRoom* room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,discu->name,NULL);
        purple_roomlist_room_add_field(list,room,discu->did);
        purple_roomlist_room_add_field(list,room,QQ_ROOM_TYPE_DISCU);
        purple_roomlist_room_add(list,room);
    }
    return list;
}

static void group_member_list_come(LwqqAsyncEvent* event,void* data)
{
    void** d = data;
    qq_account* ac = d[0];
    LwqqGroup* group = d[1];
    s_free(data);
    LwqqClient* lc = ac->qq;
    LwqqSimpleBuddy* member;
    LwqqBuddy* buddy;
    PurpleConvChatBuddyFlags flag;
    GList* users = NULL;
    GList* flags = NULL;
    GList* extra_msgs = NULL;


    PurpleConversation* conv = purple_find_chat(
                                   purple_account_get_connection(ac->account),opend_chat_search(ac,group));
    PurpleConvChat* chat = PURPLE_CONV_CHAT(conv);
    //only there are no member we add it.
    if(purple_conv_chat_get_users(PURPLE_CONV_CHAT(conv))==NULL) {
        LIST_FOREACH(member,&group->members,entries) {
            extra_msgs = g_list_append(extra_msgs,NULL);
            flag = 0;

            if(lwqq_member_is_founder(member,group)) flag |= PURPLE_CBFLAGS_FOUNDER;
            if(member->stat != LWQQ_STATUS_OFFLINE) flag |= PURPLE_CBFLAGS_VOICE;
            if(member->mflag & LWQQ_MEMBER_IS_ADMIN) flag |= PURPLE_CBFLAGS_OP;

            flags = g_list_append(flags,GINT_TO_POINTER(flag));
            if((buddy = find_buddy_by_uin(lc,member->uin))) {
                if(ac->qq_use_qqnum)
                    users = g_list_append(users,try_get(buddy->qqnumber,buddy->uin));
                else
                    users = g_list_append(users,buddy->uin);
            } else {
                users = (member->card)?g_list_append(users,member->card):g_list_append(users,member->nick);
            }
        }
        purple_conv_chat_add_users(chat,users,extra_msgs,flags,FALSE);
        g_list_free(users);
        g_list_free(flags);
        g_list_free(extra_msgs);
    }
    return ;
}
static void qq_group_join(PurpleConnection *gc, GHashTable *data)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    LwqqGroup* group = NULL;

    if(ac->state != LOAD_COMPLETED) {
        purple_notify_warning(gc,NULL,"加载尚未完成","请稍后重新尝试打开");
        return;
    }

    char* key = g_hash_table_lookup(data,QQ_ROOM_KEY_GID);
    if(key==NULL) return;
    group = find_group_by_qqnumber(lc,key);
    if(group == NULL) group = find_group_by_gid(lc,key);
    if(group == NULL) return;

    //this will open chat dialog.
    qq_conv_open(gc,group);
    void** d = s_malloc0(sizeof(void*)*2);
    d[0] = ac;
    d[1] = group;
    if(!LIST_EMPTY(&group->members)) {
        group_member_list_come(NULL,d);
    } else {
        lwqq_async_add_event_listener(
            lwqq_info_get_group_detail_info(lc,group,NULL),
            group_member_list_come,
            d);
    }
    return;
}
static gboolean qq_offline_message(const PurpleBuddy *buddy)
{
    //webqq support offline message indeed
    return TRUE;
}
#if 0
static gboolean qq_can_receive_file(PurpleConnection* gc,const char* who)
{
    //webqq support send recv file indeed.
    return TRUE;
}
#endif
//this return the member of group 's real name
//it is only used when create dialog;
char *qq_get_cb_real_name(PurpleConnection *gc, int id, const char *who)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    char conv_name[70];
    if(purple_find_buddy(ac->account,who)!=NULL)
        return NULL;
    else {
        LwqqGroup* group = opend_chat_index(ac,id);
        LwqqSimpleBuddy* sb = find_group_member_by_nick_or_card(group,who);
        snprintf(conv_name,sizeof(conv_name),"%s ### %s",(sb->card)?sb->card:sb->nick,group->name);
        return s_strdup(conv_name);
    }
    return NULL;
}

static void on_create(void *data,PurpleConnection* gc)
{
    //on conversation create we add smileys to it.
    PurpleConversation* conv = data;
    translate_add_smiley_to_conversation(conv);
}
static void qq_login(PurpleAccount *account)
{
    PurpleConnection* pc= purple_account_get_connection(account);
    qq_account* ac = qq_account_new(account);
    const char* username = purple_account_get_username(account);
    const char* password = purple_account_get_password(account);
    if(password==NULL||strcmp(password,"")==0) {
        purple_connection_error_reason(pc,PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,"密码为空");
        return;
    }
    ac->gc = pc;
    ac->disable_custom_font_size=purple_account_get_bool(account, "disable_custom_font_size", FALSE);
    ac->disable_custom_font_face=purple_account_get_bool(account, "disable_custom_font_face", FALSE);
    ac->dark_theme_fix=purple_account_get_bool(account, "dark_theme_fix", FALSE);
    ac->debug_file_send = purple_account_get_bool(account,"debug_file_send",FALSE);
    char db_path[64];
    snprintf(db_path,sizeof(db_path),"%s/.config/lwqq",getenv("HOME"));
    ac->db = lwdb_userdb_new(username,db_path);
    //for empathy
    ac->qq_use_qqnum = (ac->db != NULL);
    purple_buddy_icons_set_caching(ac->qq_use_qqnum);
    
    //all_reset(ac,RESET_DISCU);

    lwqq_async_set(ac->qq,1);
    purple_connection_set_protocol_data(pc,ac);
    client_connect_signals(ac->gc);

    lwqq_async_set_userdata(ac->qq,LOGIN_COMPLETE,ac);
    lwqq_async_add_listener(ac->qq,LOGIN_COMPLETE,login_complete);
    lwqq_async_add_listener(ac->qq,VERIFY_COME,verify_come);
    background_login(ac);
}
static void qq_close(PurpleConnection *gc)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqErrorCode err;

    lwqq_async_set(ac->qq,0);
    if(ac->qq->status!=NULL&&strcmp(ac->qq->status,"online")==0) {
        background_msg_drain(ac);
        lwqq_logout(ac->qq,&err);
    }
    lwqq_client_free(ac->qq);
    lwdb_userdb_free(ac->db);
    qq_account_free(ac);
    purple_connection_set_protocol_data(gc,NULL);
    translate_global_free();
    lwqq_http_global_free();
    lwqq_async_global_quit();
    lwdb_global_free();
}
//send change markname to server.
static void qq_change_markname(PurpleConnection* gc,const char* who,const char *alias)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    if(ac->disable_send_server) return;
    LwqqClient* lc = ac->qq;
    LwqqBuddy* buddy = ac->qq_use_qqnum?find_buddy_by_qqnumber(lc,who):find_buddy_by_uin(lc,who);
    if(buddy == NULL) return;
    lwqq_info_change_buddy_markname(lc,buddy,alias);
}
#if 0
static void change_group_markname_back(LwqqAsyncEvent* event,void* data)
{
    void **d = data;
    char* old_alias = d[2];
    if(lwqq_async_event_get_result(event)!=0) {
        qq_account* ac = d[0];
        PurpleChat* chat = d[1];

        purple_notify_error(ac->gc,NULL,"更改组备注失败","服务器返回错误");
        ac->disable_send_server = 1;
        purple_blist_alias_chat(chat,old_alias);
        ac->disable_send_server = 0;
    }
    s_free(old_alias);
    s_free(data);
}
static void qq_change_group_markname(void* node,const char* old_alias,void* _gc)
{
    PurpleBlistNode* n = node;
    PurpleConnection* gc = _gc;
    qq_account* ac = purple_connection_get_protocol_data(gc);
    if(ac==NULL) return;
    //verify this is qq_account
    if(ac->magic != QQ_MAGIC) return;
    if(ac->disable_send_server) return;
    LwqqClient* lc = ac->qq;
    if(PURPLE_BLIST_NODE_IS_CHAT(n)) {
        PurpleChat* chat = PURPLE_CHAT(n);
        GHashTable* hash = purple_chat_get_components(chat);
        const char* qqnum = g_hash_table_lookup(hash,QQ_ROOM_KEY_QUN_ID);
        LwqqGroup* group = find_group_by_qqnumber(lc,qqnum);
        const char* alias = purple_chat_get_name(chat);
        if(group == NULL) return;
        void** data = s_malloc0(sizeof(void*)*3);
        data[0] = ac;
        data[1] = chat;
        data[2] = s_strdup(old_alias);
        lwqq_async_add_event_listener(
            lwqq_info_change_group_markname(lc,group,alias),
            change_group_markname_back,
            data);
    }
}
#endif
void move_buddy_back(void* data)
{
    void** d = data;
    PurpleBuddy* buddy = d[0];
    char* group_name = d[1];
    PurpleGroup* group = purple_find_group(group_name);
    if(group==NULL) group = purple_group_new(group_name);
    s_free(group_name);
    qq_account* ac = d[2];
    s_free(data);
    ac->disable_send_server = 1;
    purple_blist_add_buddy(buddy,NULL,group,NULL);
    ac->disable_send_server = 0;
}
static void change_category_back(LwqqAsyncEvent* event,void* data)
{
    void**d = data;
    qq_account* ac = d[2];
    if(lwqq_async_event_get_result(event)!=0) {
        move_buddy_back(data);
        purple_notify_error(ac->gc,NULL,"更改好友分组失败","服务器返回错误");
    } else {
        s_free(d[1]);
        s_free(data);
    }
}
static void qq_change_category(PurpleConnection* gc,const char* who,const char* old_group,const char* new_group)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    if(ac->disable_send_server) return;
    LwqqClient* lc = ac->qq;
    LwqqBuddy* buddy = ac->qq_use_qqnum?find_buddy_by_qqnumber(lc,who):find_buddy_by_uin(lc,who);
    if(buddy==NULL) return;

    const char* category;
    if(strcmp(new_group,"QQ好友")==0)
        category = "My Friends";
    else category = new_group;
    LwqqAsyncEvent* event;
    event = lwqq_info_modify_buddy_category(lc,buddy,category);

    void** data = s_malloc0(sizeof(void*)*3);
    data[0] = purple_find_buddy(ac->account,who);
    data[1] = s_strdup(old_group);
    data[2] = ac;
    if(event == NULL) {
        purple_notify_message(gc,PURPLE_NOTIFY_MSG_ERROR,NULL,"更改好友分组失败","不存在该分组",move_buddy_back,data);
    } else
        lwqq_async_add_event_listener(event,change_category_back,data);
}
//keep this empty to ensure rename category dont crash
static void qq_rename_category(PurpleConnection* gc,const char* old_name,PurpleGroup* group,GList* moved_buddies)
{
}
static void qq_remove_buddy(PurpleConnection* gc,PurpleBuddy* buddy,PurpleGroup* group)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    LwqqBuddy* friend ;
    if(ac->qq_use_qqnum){
        const char* qqnum = purple_buddy_get_name(buddy);
        friend = find_buddy_by_qqnumber(lc,qqnum);
    }else{
        const char* uin = purple_buddy_get_name(buddy);
        friend = find_buddy_by_uin(lc,uin);
    }
    if(friend==NULL) return;
    lwqq_info_delete_friend(lc,friend,LWQQ_DEL_FROM_OTHER);
}

static void visit_qqzone(LwqqAsyncEvent* event,void* data)
{
    LwqqBuddy* buddy = data;
    char url[256];
    snprintf(url,sizeof(url),"xdg-open 'http://user.qzone.qq.com/%s'",buddy->qqnumber);
    system(url);
}

static void qq_visit_qzone(PurpleBlistNode* node)
{
    PurpleBuddy* buddy = PURPLE_BUDDY(node);
    PurpleAccount* account = purple_buddy_get_account(buddy);
    qq_account* ac = purple_connection_get_protocol_data(
                         purple_account_get_connection(account));
    if(ac->qq_use_qqnum){
        const char* qqnum = purple_buddy_get_name(buddy);
        char url[256];
        snprintf(url,sizeof(url),"xdg-open 'http://user.qzone.qq.com/%s'",qqnum);
        system(url);
        return;
    }else{
        const char* uin = purple_buddy_get_name(buddy);
        LwqqBuddy* friend = find_buddy_by_uin(ac->qq,uin);
        if(friend==NULL) return;
        if(!friend->qqnumber) {
            lwqq_async_add_event_listener(
                    lwqq_info_get_friend_qqnumber(ac->qq,friend),
                    visit_qqzone,friend);
        } else {
            visit_qqzone(NULL,friend);
        }
    }
}
static void qq_add_buddy_with_invite(PurpleConnection* pc,PurpleBuddy* buddy,PurpleGroup* group,const char* message)
{
    //qq_account* ac = purple_connection_get_protocol_data(pc);
    //LwqqVerifyCode* code = lwqq_info_add_friend_get_image(ac->qq);
}
#if 0
static void qq_visit_qun_air(PurpleBlistNode* node)
{
    PurpleChat* chat = PURPLE_CHAT(node);
    GHashTable* table = purple_chat_get_components(chat);
    const char* qqnum = g_hash_table_lookup(table,QQ_ROOM_KEY_QUN_ID);
    char url[256];
    snprintf(url,sizeof(url),"xdg-open 'http://qun.qq.com/air/%s'",qqnum);
    system(url);
}
#endif
static LwqqGroup* find_group_by_chat(PurpleChat* chat)
{
    PurpleAccount* account = purple_chat_get_account(chat);
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(account));
    LwqqClient* lc = ac->qq;
    GHashTable* table = purple_chat_get_components(chat);
    if(ac->qq_use_qqnum){
        const char* qqnum = g_hash_table_lookup(table,QQ_ROOM_KEY_GID);
        return find_group_by_qqnumber(lc, qqnum);
    }else{
        const char* gid = g_hash_table_lookup(table,QQ_ROOM_KEY_GID);
        return find_group_by_gid(lc,gid);
    }
}

static void flush_group_name(LwqqAsyncEvent* event,void* data)
{
    PurpleChat* chat = data;
    LwqqGroup* group = find_group_by_chat(chat);
    purple_blist_alias_chat(chat,group_name(group));
}

static void qq_block_chat(PurpleBlistNode* node)
{
    PurpleChat* chat = PURPLE_CHAT(node);
    PurpleAccount* account = purple_chat_get_account(chat);
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(account));
    LwqqClient* lc = ac->qq;
    LwqqGroup* group = find_group_by_chat(chat);
    lwqq_async_add_event_listener(
            lwqq_info_mask_group(lc,group,LWQQ_MASK_ALL),
            flush_group_name,chat);
}
static void qq_unblock_chat(PurpleBlistNode* node)
{
    PurpleChat* chat = PURPLE_CHAT(node);
    PurpleAccount* account = purple_chat_get_account(chat);
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(account));
    LwqqClient* lc = ac->qq;
    LwqqGroup* group = find_group_by_chat(chat);
    lwqq_async_add_event_listener(
            lwqq_info_mask_group(lc,group,LWQQ_MASK_NONE),
            flush_group_name,chat);
}
static GList* qq_blist_node_menu(PurpleBlistNode* node)
{
    GList* act = NULL;
    PurpleMenuAction* action;
    if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
        action = purple_menu_action_new("访问空间",(PurpleCallback)qq_visit_qzone,node,NULL);
        act = g_list_append(act,action);
        action = purple_menu_action_new("发送离线文件",(PurpleCallback)qq_send_offline_file,node,NULL);
        act = g_list_append(act,action);
    } else if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
        PurpleChat* chat = PURPLE_CHAT(node);
        LwqqGroup* group = qq_get_group_from_chat(chat);
        if(group->mask == LWQQ_MASK_NONE)
            action = purple_menu_action_new("屏蔽",(PurpleCallback)qq_block_chat,node,NULL);
        else
            action = purple_menu_action_new("取消屏蔽",(PurpleCallback)qq_unblock_chat,node,NULL);
        act = g_list_append(act,action);
    }
    return act;
}
static void client_connect_signals(PurpleConnection* gc)
{
    static int handle;

    void* h = &handle;
    purple_signal_connect(purple_conversations_get_handle(),"conversation-created",h,
                          PURPLE_CALLBACK(on_create),gc);
    //purple_signal_connect(purple_blist_get_handle(),"blist-node-aliased",h,
    //        PURPLE_CALLBACK(qq_change_group_markname),gc);
}
PurplePluginProtocolInfo webqq_prpl_info = {
    /* options */
    .options=           OPT_PROTO_IM_IMAGE,
    .icon_spec=         {"jpg,jpeg,gif,png", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_SEND},	/* icon_spec */
    .list_icon=         qq_list_icon,   /* list_icon */
    .login=             qq_login,       /* login */
    .close=             qq_close,       /* close */
    //NULL,//twitter_status_text, /* status_text */
//	twitterim_tooltip_text,/* tooltip_text */
    .status_types=      qq_status_types,    /* status_types */
    .set_status=        qq_set_status,
    .blist_node_menu=   qq_blist_node_menu,                   /* blist_node_menu */
    /**group part start*/
    .chat_info=         qq_chat_info,    /* chat_info implement this to enable chat*/
    .chat_info_defaults=qq_chat_info_defaults, /* chat_info_defaults */
    .roomlist_get_list =qq_get_all_group_list,
    .join_chat=         qq_group_join,
    .get_cb_real_name=  qq_get_cb_real_name,
    /**group part end*/
    .send_im=           qq_send_im,     /* send_im */
    .chat_send=         qq_send_chat,
    //.chat_leave=        qq_leave_chat,
    .send_file=         qq_send_file,
    //.chat_whisper=      qq_send_whisper,
    .offline_message=   qq_offline_message,
    .alias_buddy=       qq_change_markname, /* change buddy alias on server */
    .group_buddy=       qq_change_category  /* change buddy category on server */,
    .rename_group=      qq_rename_category,
    .remove_buddy=      qq_remove_buddy,
    .add_buddy_with_invite=qq_add_buddy_with_invite,
    .struct_size=       sizeof(PurplePluginProtocolInfo)
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
    .actions=       plugin_actions,
};

static void
init_plugin(PurplePlugin *plugin)
{
    PurpleAccountOption *option;
    GList* options = NULL;
    //option = purple_account_option_bool_new("兼容Pidgin Conversation integration", 
    //        "compatible_pidgin_conversation_integration", FALSE);
    //options = g_list_append(options, option);
    option = purple_account_option_bool_new("禁用自定义接收消息字体", "disable_custom_font_face", FALSE);
    options = g_list_append(options, option);
    option = purple_account_option_bool_new("禁用自定义接收消息文字大小", "disable_custom_font_size", FALSE);
    options = g_list_append(options, option);
    option = purple_account_option_bool_new("修复暗色系统主题下的消息显示(将黑色文字转为白色)", "dark_theme_fix", FALSE);
    options = g_list_append(options, option);
    option = purple_account_option_bool_new("调试文件传输", "debug_file_send", FALSE);
    options = g_list_append(options, option);
    webqq_prpl_info.protocol_options = options;

#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    bindtextdomain(GETTEXT_PACKAGE , LOCALE_DIR);
    textdomain(GETTEXT_PACKAGE);
#endif
}

PURPLE_INIT_PLUGIN(webqq, init_plugin, info)
