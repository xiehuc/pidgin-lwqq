#include "qq_types.h"

#include <login.h>
#include <async.h>
#include <info.h>
#include <msg.h>
#include <smemory.h>
#include <string.h>

#include "translate.h"

#define START_THREAD(thread,data)\
do{pthread_t th;\
    pthread_create(&th,NULL,thread,data);\
}while(0)


static void* _background_login(void* data)
{
    qq_account* ac=(qq_account*)data;
    LwqqClient* lc = ac->qq;
    LwqqErrorCode err;

    lwqq_login(lc, &err);
    lwqq_async_set_error(lc,VERIFY_COME,err);
    if (err == LWQQ_EC_LOGIN_NEED_VC) {
        lwqq_async_dispatch(lc,VERIFY_COME,NULL);
    } else {
        lwqq_async_dispatch(lc,LOGIN_COMPLETE,NULL);
    }
    return NULL;
}


void background_login(qq_account* ac)
{
    START_THREAD(_background_login,ac);
}
static void* _background_friends_info(void* data)
{
    qq_account* ac=(qq_account*)data;
    LwqqErrorCode err;
    LwqqClient* lc=ac->qq;

    LwqqAsyncEvset* lock = lwqq_async_evset_new();
    LwqqAsyncEvent* event;
    event = lwqq_info_get_friends_info(lc,&err);
    lwqq_async_evset_add_event(lock,event);
    event = lwqq_info_get_group_name_list(ac->qq,&err);
    lwqq_async_evset_add_event(lock,event);
    lwqq_async_wait(lock);

    lock = lwqq_async_evset_new();
    event = lwqq_info_get_online_buddies(ac->qq,&err);
    lwqq_async_evset_add_event(lock,event);
    lwqq_async_wait(lock);


    lwqq_info_get_friend_detail_info(lc,lc->myself,&err);


    //lwqq_info_get_all_friend_qqnumbers(lc,&err);
    lock = lwqq_async_evset_new();
    LwqqBuddy* buddy;
    LIST_FOREACH(buddy,&ac->qq->friends,entries){
        event = lwqq_info_get_friend_qqnumber(lc,buddy->uin);
        lwqq_async_evset_add_event(lock,event);
    }
    LwqqGroup* group;
    LIST_FOREACH(group,&ac->qq->groups,entries) {
        event = lwqq_info_get_friend_qqnumber(lc,group->code);
        lwqq_async_evset_add_event(lock,event);
    }
    lwqq_async_add_evset_listener(lock,qq_set_basic_info,ac);

    return NULL;
}
void background_friends_info(qq_account* ac)
{
    START_THREAD(_background_friends_info,ac);
}
static int msg_check_repeat = 1;
static gboolean msg_check(void* data)
{
    qq_msg_check((qq_account*)data);
    //repeat for ever;
    return msg_check_repeat;
}
static int msg_check_handle;
static pthread_t tid = 0;
static void* _background_msg_poll(void* data)
{
    qq_account* ac = (qq_account*)data;
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;

    /* Poll to receive message */
    l->poll_msg(l);

    msg_check_handle = purple_timeout_add(200,msg_check,ac);

    return NULL;
}
static pthread_t msg_th;
void background_msg_poll(qq_account* ac)
{
    pthread_create(&msg_th,NULL,_background_msg_poll,ac);
}
void background_msg_drain(qq_account* ac)
{
    LwqqRecvMsgList *l = (LwqqRecvMsgList *)ac->qq->msg_list;
    purple_timeout_remove(msg_check_handle);
    pthread_cancel(l->tid);
    l->tid = 0;
    tid = 0;
}


static PurpleConversation* find_conversation(int msg_type,const char* who,PurpleAccount* account)
{
    int type;
    if(msg_type == LWQQ_MT_BUDDY_MSG)
        type = PURPLE_CONV_TYPE_IM;
    else if(msg_type == LWQQ_MT_GROUP_MSG)
        type = PURPLE_CONV_TYPE_CHAT;
    else 
        return NULL;
    return purple_find_conversation_with_account(type,who,account);
}
static void send_back(LwqqAsyncEvent* event,void* data)
{
    static char buf[1024];
    void **d = data;
    LwqqMsg* msg = d[0];
    char* what = d[2];
    qq_account* ac = d[3];
    char* who = d[4];
    int errno = lwqq_async_event_get_result(event);
    s_free(data);
    if(errno){
        PurpleConversation* conv = find_conversation(msg->type,who,ac->account);
        snprintf(buf,sizeof(buf),"发送失败:\n%s",what);
        if(conv)purple_conversation_write(conv,NULL,buf,PURPLE_MESSAGE_DELAYED,time(NULL));
    }

    LwqqMsgMessage* mmsg = msg->opaque;
    mmsg->f_name = NULL;
    mmsg->f_color = NULL;
    mmsg->to = NULL;
    mmsg->group_code = NULL;
    s_free(what);
    lwqq_msg_free(msg);
}
void* _background_send_msg(void* data)
{
    void **d = data;
    LwqqMsg* msg = d[0];
    LwqqMsgMessage* mmsg = msg->opaque;
    const char* to = mmsg->to;
    const char* what = d[2];
    qq_account* ac = d[3];
    const char* who = d[4];
    LwqqClient* lc = ac->qq;
    int will_upload = (strstr(what,"<IMG")!=NULL);

    int ret = translate_message_to_struct(lc,to,what,mmsg,1);
    if(will_upload){
        PurpleConversation* conv = find_conversation(msg->type,who,ac->account);
        if(conv)purple_conversation_write(conv,NULL,"图片上传完成",PURPLE_MESSAGE_SYSTEM,time(NULL));
        //if(conv&&ret) purple_conversation_write(conv,NULL,"图片上传失败",PURPLE_MESSAGE_SYSTEM,time(NULL));
    }
    lwqq_async_add_event_listener(lwqq_msg_send(lc,msg),send_back,data);
    return NULL;
}

void background_send_msg(qq_account* ac,LwqqMsg* msg,const char* who,const char* what,PurpleConversation* conv)
{

    void** data = s_malloc0(sizeof(void*)*5);
    data[0] = msg;
    data[1] = conv;
    data[2] = s_strdup(what);
    data[3] = ac;
    data[4] = s_strdup(who);

    int will_upload = 0;
    will_upload = (strstr(what,"<IMG")!=NULL);
    if(will_upload)
        START_THREAD(_background_send_msg,data);
    else
        _background_send_msg(data);
}
