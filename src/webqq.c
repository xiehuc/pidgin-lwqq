
#define PURPLE_PLUGINS
#include <plugin.h>
#include <version.h>
#include <smemory.h>
#include <request.h>
#include <signal.h>

#include <type.h>
#include <async.h>
#include <msg.h>
#include <info.h>
#include <http.h>

#include "internal.h"
#include "webqq.h"
#include "qq_types.h"
#include "login.h"
#include "background.h"
#include "translate.h"


char *qq_get_cb_real_name(PurpleConnection *gc, int id, const char *who);
static void client_connect_signals(PurpleConnection* gc);
static void group_member_list_come(LwqqAsyncEvent* event,void* data);
static void group_message_delay_display(LwqqAsyncEvent* event,void* data);
static void whisper_message_delay_display(LwqqAsyncEvent* event,void* data);

static LwqqBuddy* find_buddy_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    LwqqBuddy* buddy;
    LIST_FOREACH(buddy,&lc->friends,entries){
        //this may caused by qqnumber not loaded successful.
        if(!buddy->qqnumber) continue;
        if(strcmp(buddy->qqnumber,qqnum)==0)
            return buddy;
    }
    return NULL;
}
static LwqqGroup* find_group_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    LwqqGroup* group;
    LIST_FOREACH(group,&lc->groups,entries){
        if(!group->account) continue;
        if(strcmp(group->account,qqnum)==0)
            return group;
    }
    return NULL;
}
static LwqqGroup* find_group_by_name(LwqqClient* lc,const char* name)
{
    LwqqGroup* group;
    LIST_FOREACH(group,&lc->groups,entries){
        if(group->name&&strcmp(group->name,name)==0)
            return group;
    }
    return NULL;
}
static LwqqSimpleBuddy* find_group_member_by_nick(LwqqGroup* group,const char* nick)
{
    LwqqSimpleBuddy* sb;
    LIST_FOREACH(sb,&group->members,entries){
        if(sb->nick&&strcmp(sb->nick,nick)==0)
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
    snprintf(url,sizeof(url),"gnome-open 'http://user.qzone.qq.com/%s/infocenter'",ac->qq->myself->uin);
    system(url);
}
static void buddies_all_remove(void* data,void* userdata)
{
    PurpleBuddy* buddy = data;
    qq_account* ac = userdata;
    if(purple_buddy_get_account(buddy) == ac->account){
        purple_blist_remove_buddy(buddy);
    }
}
static void all_reset(qq_account* ac)
{
    GSList* buddies = purple_blist_get_buddies();
    g_slist_foreach(buddies,buddies_all_remove,ac);

    PurpleGroup* group = purple_find_group("QQ群");
    PurpleBlistNode* node = purple_blist_node_get_first_child(PURPLE_BLIST_NODE(group));
    while(node!=NULL){
        if(PURPLE_BLIST_NODE_IS_CHAT(node)){
            PurpleChat* chat = PURPLE_CHAT(node);
            if(purple_chat_get_account(chat)!=ac->account){
                node = purple_blist_node_next(node,1);
                continue;
            }
            purple_blist_remove_chat(chat);
        }
        node = purple_blist_node_next(node,1);
    }
}
static void all_reset_action(PurplePluginAction* action)
{
    PurpleConnection* gc = action->context;
    qq_account* ac = purple_connection_get_protocol_data(gc);

    all_reset(ac);

    purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,"全部重载,请重新登录");
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

    return types;
}
static void qq_set_status(PurpleAccount* account,PurpleStatus* status)
{
    qq_account* ac = purple_connection_get_protocol_data(purple_account_get_connection(account));
    lwqq_info_change_status(ac->qq,lwqq_status_from_str(purple_status_get_id(status)));
}

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
    if(cate_index != 0){
        LIST_FOREACH(cate,&lc->categories,entries){
            if(cate->index==cate_index){
                group = purple_group_new(cate->name);
                break;
            }
        }
    }

    bu = purple_find_buddy(account,buddy->uin);
    if(bu == NULL){
        bu = purple_buddy_new(ac->account,buddy->uin,buddy->nick);
        purple_blist_add_buddy(bu,NULL,group,NULL);
    }
    //flush new alias
    const char* alias = purple_buddy_get_alias_only(bu);
    if(buddy->markname){
        if(alias==NULL||strcmp(alias,buddy->markname)!=0)
            purple_blist_alias_buddy(bu,buddy->markname);
    }else if(alias==NULL||strcmp(alias,buddy->nick)!=0){
        purple_blist_alias_buddy(bu,buddy->nick);
    }
    if(purple_buddy_get_group(bu)!=group){
        purple_blist_add_buddy(bu,NULL,group,NULL);
    }
    if(buddy->status)
        purple_prpl_got_user_status(account, buddy->uin, buddy->status, "nick",buddy->nick,"mark",buddy->markname,NULL);
    PurpleBuddyIcon* icon;
    if((icon = purple_buddy_icons_find(account,buddy->uin))==0){
        lwqq_info_get_friend_avatar(lc,buddy);
    }else{
        purple_buddy_set_icon(purple_find_buddy(account,buddy->uin),icon);
    }
    ac->disable_send_server = 0;
    return 0;
}
static int group_come(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    ac->disable_send_server = 1;
    PurpleAccount* account=ac->account;
    LwqqGroup* group = data;
    PurpleGroup* gp = purple_group_new("QQ群");
    GHashTable* components;
    PurpleChat* chat;

    if(purple_blist_find_chat(account,group->gid) == NULL){
        components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(components,g_strdup(QQ_ROOM_KEY_GID),g_strdup(group->gid));
        chat = purple_chat_new(account,group->gid,components);
        purple_blist_add_chat(chat,gp,NULL);
    }else
        chat = purple_blist_find_chat(account,group->gid);
    const char* alias = purple_chat_get_name(chat);
    if(group->markname){
        if(alias==NULL||strcmp(alias,group->markname)!=0)
            purple_blist_alias_chat(chat,group->markname);
    }else if(alias==NULL||strcmp(alias,group->name)!=0)
        purple_blist_alias_chat(chat,group->name);
    if(purple_buddy_icons_node_has_custom_icon(PURPLE_BLIST_NODE(chat))==0)
        lwqq_info_get_group_avatar(lc,group);
    ac->disable_send_server = 0;
    return 0;
}
static void buddy_message(LwqqClient* lc,LwqqMsgMessage* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    char buf[1024] = {0};
    //LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,msg->from);
    //if(buddy==NULL) return;

    translate_struct_to_message(msg,buf);
    serv_got_im(pc, msg->from, buf, PURPLE_MESSAGE_RECV, time(NULL));
}
//open chat conversation dialog
static void qq_conv_open(PurpleConnection* gc,LwqqGroup* group)
{
    g_return_if_fail(group);
    g_return_if_fail(group->gid);
    qq_account* ac = purple_connection_get_protocol_data(gc);
    PurpleConversation *conv = purple_find_chat(gc,opend_chat_search(ac,group));
    if(conv == NULL&&group->gid){
        serv_got_joined_chat(gc,open_new_chat(ac,group),group->gid);
    }
}
static void group_message(LwqqClient* lc,LwqqMsgMessage* msg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;
    LwqqGroup* group = lwqq_group_find_group_by_gid(lc,msg->from);

    g_return_if_fail(group);

    //force open dialog
    qq_conv_open(pc,group);
    char buf[1024] = {0};
    
    translate_struct_to_message(msg,buf);
    
    LwqqErrorCode err;
    void **data = s_malloc0(sizeof(void*)*4);
    data[0] = ac;
    data[1] = group;
    data[2] = s_strdup(msg->send);
    data[3] = s_strdup(buf);
    //get all member list
    if(LIST_EMPTY(&group->members)) {
        //use block method to get list
        lwqq_async_add_event_listener(
                lwqq_info_get_group_detail_info(lc,group,&err),
                group_message_delay_display,
                data);
    }else{
        group_message_delay_display(NULL,data);
    }
}
static void group_message_delay_display(LwqqAsyncEvent* event,void* data)
{
    void **d = data;
    qq_account* ac = d[0];
    LwqqGroup* group = d[1];
    char* sender = d[2];
    char* buf = d[3];
    LwqqClient* lc = ac->qq;
    PurpleConnection* pc = ac->gc;
    const char* who;

    LwqqSimpleBuddy* sb = lwqq_group_find_group_member_by_uin(group,sender);
    if(sb)
        who = sb->nick;
    else
        //this means it is uin indeed. or we dont know it else .
        who = sender;

    serv_got_chat_in(pc,opend_chat_search(ac,group),who,PURPLE_MESSAGE_RECV,buf,time(NULL));
    s_free(sender);
    s_free(buf);
    group_member_list_come(NULL,data);
}
static void whisper_message(LwqqClient* lc,LwqqMsgMessage* mmsg)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleConnection* pc = ac->gc;

    const char* from = mmsg->from;
    const char* gid = mmsg->id;
    char name[70];
    char buf[1024] = {0};

    translate_struct_to_message(mmsg,buf);

    LwqqGroup* group = lwqq_group_find_group_by_gid(lc,gid);
    if(group == NULL){
        snprintf(name,sizeof(name),"%s #(broken)# %s",from,gid);
        serv_got_im(pc,name,buf,PURPLE_MESSAGE_RECV,time(NULL));
        return;
    }
    void** data = s_malloc0(sizeof(void*)*4);
    data[0] = pc;
    data[1] = group;
    data[2] = s_strdup(from);
    data[3] = s_strdup(buf);
    if(LIST_EMPTY(&group->members)){
        lwqq_async_add_event_listener(
                lwqq_info_get_group_detail_info(lc,group,NULL),
                whisper_message_delay_display,
                data);
    }else
        whisper_message_delay_display(NULL,data);
}
static void whisper_message_delay_display(LwqqAsyncEvent* event,void* data)
{
    void**d = data;
    PurpleConnection* pc = d[0];
    LwqqGroup* group = d[1];
    char* from = d[2];
    char* msg = d[3];
    s_free(data);
    char name[70];
    LwqqSimpleBuddy* sb = lwqq_group_find_group_member_by_uin(group,from);
    if(sb == NULL){
        snprintf(name,sizeof(name),"%s #(broken)# %s",from,group->name);
    }else{
        snprintf(name,sizeof(name),"%s ### %s",sb->nick,group->name);
    }
    serv_got_im(pc,name,msg,PURPLE_MESSAGE_RECV,time(NULL));
}
static void status_change(LwqqClient* lc,LwqqMsgStatusChange* status)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;
    purple_prpl_got_user_status(account,status->who,status->status,NULL);
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
    if(answer == 0){
        lwqq_info_allow_and_add(lc,account,NULL);
    }else if(answer == 1){
        lwqq_info_allow_added_request(lc,account);
    }else if(answer == 2){
        lwqq_info_deny_added_request(lc,account,"no reason");
    }
    s_free(account);
}
static void verify_required_cancel(void* data,PurpleRequestFields* root)
{
}
static void system_message(LwqqClient* lc,LwqqMsgSystem* system)
{
    if(system->type != VERIFY_REQUIRED) return;
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);

    PurpleRequestFields* root = purple_request_fields_new();
    PurpleRequestFieldGroup *container = purple_request_field_group_new("好友确认");
    purple_request_fields_add_group(root,container);
    char buf1[128];
    char buf2[128];
    snprintf(buf1,sizeof(buf1),"%s请求加您为好友",system->account);
    snprintf(buf2,sizeof(buf2),"附加信息:%s",system->msg);

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

}
static int friend_avatar(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;
    LwqqBuddy* buddy = data;
    if(buddy->avatar_len==0)return 0;

    if(strcmp(buddy->uin,purple_account_get_username(account))==0){
        purple_buddy_icons_set_account_icon(account,(guchar*)buddy->avatar,buddy->avatar_len);
    }else{
        purple_buddy_icons_set_for_user(account,buddy->uin,(guchar*)buddy->avatar,buddy->avatar_len,NULL);
    }
    //let free by purple;
    buddy->avatar = NULL;
    return 0;
}
static int group_avatar(LwqqClient* lc,void* data)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    PurpleAccount* account = ac->account;
    LwqqGroup* group = data;
    PurpleChat* chat;
    if(group->avatar_len==0)return 0;

    chat = purple_blist_find_chat(account,group->gid);
    if(chat==NULL) return 0;
    purple_buddy_icons_node_set_custom_icon(PURPLE_BLIST_NODE(chat),(guchar*)group->avatar,group->avatar_len);
    //let free by purple;
    group->avatar = NULL;
    return 0;
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
    LwqqRecvMsg *msg;

    pthread_mutex_lock(&l->mutex);
    if (SIMPLEQ_EMPTY(&l->head)) {
        /* No message now, wait 100ms */
        pthread_mutex_unlock(&l->mutex);
        return 0;
    }

    msg = SIMPLEQ_FIRST(&l->head);
    switch(msg->msg->type){
        case LWQQ_MT_BUDDY_MSG:
            lwqq_msg_request_picture(lc,msg->msg->type,(LwqqMsgMessage*)msg->msg->opaque);
            buddy_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
            break;
        case LWQQ_MT_GROUP_MSG:
            lwqq_msg_request_picture(lc,msg->msg->type,(LwqqMsgMessage*)msg->msg->opaque);
            group_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
            break;
        case LWQQ_MT_SESS_MSG:
            lwqq_msg_request_picture(lc,msg->msg->type,(LwqqMsgMessage*)msg->msg->opaque);
            whisper_message(lc,(LwqqMsgMessage*)msg->msg->opaque);
            break;
        case LWQQ_MT_STATUS_CHANGE:
            /*LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,msg->msg->status.who);
              if(buddy->status!=NULL)s_free(buddy->status);
              buddy->status = s_strdup(msg->msg->status.status);*/
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
        default:
            printf("unknow message\n");
            break;
    }

    SIMPLEQ_REMOVE_HEAD(&l->head, entries);

    pthread_mutex_unlock(&l->mutex);
    lwqq_msg_free(msg->msg);
    s_free(msg);
    return 0;

}
static void check_exist(void* data,void* userdata)
{
    qq_account* ac = userdata;
    PurpleBuddy* bu = data;
    LwqqClient* lc = ac->qq;
    if(purple_buddy_get_account(bu)==ac->account&&
            find_buddy_by_qqnumber(lc,purple_buddy_get_name(bu))==NULL){
        purple_blist_remove_buddy(bu);
    }
}
int qq_set_basic_info(LwqqClient* lc,void* data)
{
    qq_account* ac = data;
    //LwqqClient* lc = ac->qq;
    purple_account_set_alias(ac->account,lc->myself->nick);
    if(purple_buddy_icons_find_account_icon(ac->account)==NULL)
        lwqq_info_get_friend_avatar(lc,lc->myself);
    //search buddy list see if alread delete from server
    //GSList* list = purple_blist_get_buddies();
    //g_slist_foreach(list,check_exist,ac);

    /*PurpleChat* chat;
    PurpleGroup* group = purple_find_group("QQ群");
    PurpleBlistNode* node = purple_blist_node_get_first_child(PURPLE_BLIST_NODE(group));
    while(node){
        if(PURPLE_BLIST_NODE_IS_CHAT(node)){
            chat = PURPLE_CHAT(node);
            GHashTable* table = purple_chat_get_components(chat);
            const char* qqnum = g_hash_table_lookup(table,QQ_ROOM_KEY_QUN_ID);
            if(purple_chat_get_account(chat)==ac->account&&
                    qqnum&&
                    find_group_by_qqnumber(lc,qqnum)==NULL){
                node = purple_blist_node_next(node,1);
                purple_blist_remove_chat(chat);
                continue;
            }
        }
        node = purple_blist_node_next(node,1);
    }*/
    LwqqBuddy* buddy;
    LIST_FOREACH(buddy,&lc->friends,entries){
        friend_come(lc,buddy);
    }
    LwqqGroup* group;
    LIST_FOREACH(group,&lc->groups,entries){
        group_come(lc,group);
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
	purple_request_field_group_add_field(field_group, code_entry);
    purple_request_field_set_required(code_entry,TRUE);

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
    if(err==LWQQ_EC_LOGIN_ABNORMAL){
        //browser cookie may not be pidgin account
        /*char buf[512];
        snprintf(buf,sizeof(buf),"gnome-open '%s'",lc->error_description);
        puts(buf);
        system(buf);*/
        purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_OTHER_ERROR,"帐号出现问题,需要解禁");
        return 0;
    }else if(err!=LWQQ_EC_OK){
        purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,"Network Error");
        return 0;
    }

    purple_connection_set_state(gc,PURPLE_CONNECTED);
    ac->state = CONNECTED;
    purple_buddy_icons_set_caching(0);

    lwqq_async_add_listener(ac->qq,FRIEND_COME,friend_come);
    lwqq_async_add_listener(ac->qq,FRIEND_COMPLETE,qq_set_basic_info);
    lwqq_async_add_listener(ac->qq,GROUP_COME,group_come);
    lwqq_async_add_listener(ac->qq,FRIEND_AVATAR,friend_avatar);
    lwqq_async_add_listener(ac->qq,GROUP_AVATAR,group_avatar);
    lwqq_async_add_listener(ac->qq,POLL_LOST_CONNECTION,lost_connection);
    lwqq_async_add_listener(ac->qq,POLL_MSG_COME,qq_msg_check);
    background_friends_info(ac);
    return 0;
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
    if((pos = strstr(who," ### "))!=NULL){
        strcpy(gname,pos+strlen(" ### "));
        strncpy(nick,who,pos-who);
        nick[pos-who] = '\0';
        msg = lwqq_msg_new(LWQQ_MT_SESS_MSG);
        mmsg = msg->opaque;
        LwqqGroup* group = find_group_by_name(lc,gname);
        LwqqSimpleBuddy* sb = find_group_member_by_nick(group,nick);
        mmsg->to = sb->uin;
        if(!sb->group_sig)
            lwqq_info_get_group_sig(lc,group,sb->uin);
        mmsg->group_sig = s_strdup(sb->group_sig);
    }else{
        //LwqqBuddy* buddy = find_buddy_by_qqnumber(lc,who);
        msg = lwqq_msg_new(LWQQ_MT_BUDDY_MSG);
        mmsg = msg->opaque;
        mmsg->to = s_strdup(who);
    }
    mmsg->f_name = "宋体";
    mmsg->f_size = 13;
    mmsg->f_style.b = 0,mmsg->f_style.i = 0,mmsg->f_style.u = 0;
    mmsg->f_color = "000000";
    PurpleConversation* conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,who,ac->account);


    background_send_msg(ac,msg,who,what,conv);
    

    return 1;
}

static int qq_send_chat(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqGroup* group = opend_chat_index(ac,id);

    LwqqMsg* msg = lwqq_msg_new(LWQQ_MT_GROUP_MSG);
    LwqqMsgMessage *mmsg = msg->opaque;
    mmsg->to = group->gid;
    mmsg->group_code = group->code;
    mmsg->f_name = "宋体";
    mmsg->f_size = 13;
    mmsg->f_style.b = 0,mmsg->f_style.i = 0,mmsg->f_style.u = 0;
    mmsg->f_color = "000000";
    PurpleConversation* conv = purple_find_chat(gc,id);

    background_send_msg(ac,msg,group->gid,message,conv);

    //write message by hand
    purple_conversation_write(conv,NULL,message,flags,time(NULL));

    return 1;
}

#if 0
//pidgin not use send_whisper .
//may use it in v 3.0.0
static void qq_send_whisper(PurpleConnection* gc,int id,const char* who,const char* message)
{
    qq_account* ac = (qq_account*)purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    LwqqGroup* group = opend_chat_index(ac,id);

    LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,who);
    if(buddy!=NULL){
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
    pce->label = "ID: ";
    pce->identifier = QQ_ROOM_KEY_GID;
    m = g_list_append(m, pce);

    return m;
}
static GHashTable *qq_chat_info_defaults(PurpleConnection *gc, const gchar *chat_name)
{
    GHashTable *defaults;

    defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

    if (chat_name != NULL)
        g_hash_table_insert(defaults, QQ_ROOM_KEY_GID, g_strdup(chat_name));

    return defaults;
}
static PurpleRoomlist* qq_get_all_group_list(PurpleConnection* gc)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    PurpleAccount* account = ac->account;
    PurpleRoomlist* list = purple_roomlist_new(account);
    GList* fields = NULL;
    PurpleRoomlistField* field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING,"QQ号",QQ_ROOM_KEY_GID,FALSE);
    fields = g_list_append(fields,field);
    purple_roomlist_set_fields(list,fields);
    LwqqGroup* group;
    LIST_FOREACH(group,&ac->qq->groups,entries){
        PurpleRoomlistRoom* room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,group->name,NULL);
        purple_roomlist_room_add_field(list,room,group->gid);
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
            flag |= PURPLE_CBFLAGS_TYPING;
            flags = g_list_append(flags,GINT_TO_POINTER(flag));
            if(lwqq_buddy_find_buddy_by_uin(lc,member->uin)){
                users = g_list_append(users,member->uin);
            }else{
                users = g_list_append(users,member->nick);
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
    char* gid = g_hash_table_lookup(data,QQ_ROOM_KEY_GID);
    LwqqGroup* group = NULL;
    if(gid==NULL) return;

    if(ac->state != LOAD_COMPLETED){
        purple_notify_warning(gc,NULL,"加载尚未完成","请稍后重新尝试打开");
        return;
    }

    //group = find_group_by_qqnumber(lc,account);
    group = lwqq_group_find_group_by_gid(lc,gid);
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
static gboolean qq_can_receive_file(PurpleConnection* gc,const char* who)
{
    //webqq support send recv file indeed.
    return TRUE;
}
//this return the member of group 's real name
//it is only used when create dialog;
char *qq_get_cb_real_name(PurpleConnection *gc, int id, const char *who)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    char conv_name[70];
    if(purple_find_buddy(ac->account,who)!=NULL)
        return NULL;
    else{
        LwqqGroup* group = opend_chat_index(ac,id);
        LwqqSimpleBuddy* sb = find_group_member_by_nick(group,who);
        snprintf(conv_name,sizeof(conv_name),"%s ### %s",sb->nick,group->name);
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
    translate_global_init();
    PurpleConnection* pc= purple_account_get_connection(account);
    qq_account* ac = qq_account_new(account);
    const char* username = purple_account_get_username(account);
    const char* password = purple_account_get_password(account);
    if(password==NULL||strcmp(password,"")==0){
        purple_connection_error_reason(pc,PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,"密码为空");
        return;
    }
    ac->gc = pc;
    ac->qq = lwqq_client_new(username,password);
    all_reset(ac);
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

    //lwqq_async_set(ac->qq,0);
    if(ac->qq->status!=NULL&&strcmp(ac->qq->status,"online")==0) {
        background_msg_drain(ac);
        lwqq_logout(ac->qq,&err);
    }
    lwqq_client_free(ac->qq);
    qq_account_free(ac);
    purple_connection_set_protocol_data(gc,NULL);
    translate_global_free();
    lwqq_http_global_free();
}
static void
init_plugin(PurplePlugin *plugin)
{
#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    bindtextdomain(GETTEXT_PACKAGE , LOCALE_DIR);
    textdomain(GETTEXT_PACKAGE);
#endif
}
//send change markname to server.
static void qq_change_markname(PurpleConnection* gc,const char* who,const char *alias)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    if(ac->disable_send_server) return;
    LwqqClient* lc = ac->qq;
    LwqqBuddy* buddy = find_buddy_by_qqnumber(lc,who);
    if(buddy == NULL) return;
    lwqq_info_change_buddy_markname(lc,buddy,alias);
}
#if 0
static void change_group_markname_back(LwqqAsyncEvent* event,void* data)
{
    void **d = data;
    char* old_alias = d[2];
    if(lwqq_async_event_get_result(event)!=0){
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
    if(PURPLE_BLIST_NODE_IS_CHAT(n)){
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
    PurpleGroup* group = d[1];
    qq_account* ac = d[2];
    s_free(data);
    ac->disable_send_server = 1;
    purple_blist_add_buddy(buddy,NULL,group,NULL);
    ac->disable_send_server = 0;
}
static void change_category_back(LwqqAsyncEvent* event,void* data)
{
    void** d = data;
    qq_account* ac = d[2];
    if(lwqq_async_event_get_result(event)!=0){
        move_buddy_back(data);
        purple_notify_error(ac->gc,NULL,"更改好友分组失败","服务器返回错误");
    }
}
static void qq_change_category(PurpleConnection* gc,const char* who,const char* old_group,const char* new_group)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    if(ac->disable_send_server) return;
    LwqqClient* lc = ac->qq;
    LwqqBuddy* buddy = find_buddy_by_qqnumber(lc,who);
    if(buddy==NULL) return;
    
    const char* category;
    if(strcmp(new_group,"QQ好友")==0) 
        category = "My Friends";
    else category = new_group;
    LwqqAsyncEvent* event;
    event = lwqq_info_modify_buddy_category(lc,buddy,category);

    void** data = s_malloc0(sizeof(void*)*3);
    data[0] = purple_find_buddy(ac->account,who);
    data[1] = purple_find_group(old_group);
    data[2] = ac;
    if(event == NULL){
        purple_notify_message(gc,PURPLE_NOTIFY_MSG_ERROR,NULL,"更改好友分组失败","不存在该分组",move_buddy_back,data);
    }else
        lwqq_async_add_event_listener(event,change_category_back,data);
}
static void qq_remove_buddy(PurpleConnection* gc,PurpleBuddy* buddy,PurpleGroup* group)
{
    qq_account* ac = purple_connection_get_protocol_data(gc);
    LwqqClient* lc = ac->qq;
    const char* qqnum = purple_buddy_get_name(buddy);
    LwqqBuddy* friend = find_buddy_by_qqnumber(lc,qqnum);
    if(friend==NULL) return;
    lwqq_info_delete_friend(lc,friend,LWQQ_DEL_FROM_OTHER);

}
static void qq_visit_qzone(PurpleBlistNode* node)
{
    PurpleBuddy* buddy = PURPLE_BUDDY(node);
    char url[256];
    snprintf(url,sizeof(url),"gnome-open 'http://user.qzone.qq.com/%s'",purple_buddy_get_name(buddy));
    system(url);
}
static void qq_visit_qun_air(PurpleBlistNode* node)
{
    PurpleChat* chat = PURPLE_CHAT(node);
    GHashTable* table = purple_chat_get_components(chat);
    const char* qqnum = g_hash_table_lookup(table,QQ_ROOM_KEY_QUN_ID);
    char url[256];
    snprintf(url,sizeof(url),"gnome-open 'http://qun.qq.com/air/%s'",qqnum);
    system(url);
}
static GList* qq_blist_node_menu(PurpleBlistNode* node)
{
    GList* act = NULL;
    PurpleMenuAction* action;
    if(PURPLE_BLIST_NODE_IS_BUDDY(node)){
        action = purple_menu_action_new("访问空间",(PurpleCallback)qq_visit_qzone,node,NULL);
        act = g_list_append(act,action);
    }else if(PURPLE_BLIST_NODE_IS_CHAT(node)){
        //action = purple_menu_action_new("访问群社区",(PurpleCallback)qq_visit_qun_air,node,NULL);
        //act = g_list_append(act,action);
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
    //.chat_whisper=      qq_send_whisper,
    .offline_message=   qq_offline_message,
    .alias_buddy=       qq_change_markname, /* change buddy alias on server */
    .group_buddy=       qq_change_category  /* change buddy category on server */,
    .remove_buddy=      qq_remove_buddy,
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
    .actions=       plugin_actions,
};

PURPLE_INIT_PLUGIN(webqq, init_plugin, info)
