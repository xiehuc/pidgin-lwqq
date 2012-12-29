#include "qq_types.h"
#include "smemory.h"
#include "msg.h"
#include "async.h"

struct dispatch_data{
    DISPATCH_FUNC dsph;
    CALLBACK_FUNC func;
    vp_list data;
};

static int did_dispatch(void* param)
{
    struct dispatch_data *d = param;
    DISPATCH_FUNC dsph = d->dsph;
    CALLBACK_FUNC func = d->func;
    vp_start(d->data);
    dsph(func,&d->data,NULL);
    vp_end(d->data);
    s_free(d);
    return 0;
}

void qq_dispatch(DISPATCH_FUNC dsph,CALLBACK_FUNC func,...)
{
    struct dispatch_data* d = s_malloc0(sizeof(*d));
    d->dsph = dsph;
    d->func = func;

    va_list args;
    va_start(args,func);
    dsph(NULL,&d->data,&args);
    va_end(args);

    purple_timeout_add(10,did_dispatch,d);
}
static void add_p_buddy_to_ac(void* a,void* b)
{
    PurpleBuddy* buddy = a;
    qq_account* ac = b;
    if(purple_buddy_get_account(buddy) == ac->account)
        ac->p_buddy_list = g_list_prepend(ac->p_buddy_list,buddy);

}
qq_account* qq_account_new(PurpleAccount* account)
{
    qq_account* ac = g_malloc0(sizeof(qq_account));
    ac->account = account;
    ac->magic = QQ_MAGIC;
    ac->qq_use_qqnum = 0;
    //this is auto increment sized array . so don't worry about it.
    ac->opend_chat = g_ptr_array_sized_new(10);
    ac->p_buddy_list = NULL;
    g_slist_foreach(purple_blist_get_buddies(),add_p_buddy_to_ac,ac);
    const char* username = purple_account_get_username(account);
    const char* password = purple_account_get_password(account);
    ac->qq = lwqq_client_new(username,password);
    //lwqq_async_set(ac->qq,1);
#if QQ_USE_FAST_INDEX
    ac->qq->find_buddy_by_uin = find_buddy_by_uin;
    ac->qq->find_buddy_by_qqnumber = find_buddy_by_qqnumber;
    ac->fast_index.uin_index = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,g_free);
    ac->fast_index.qqnum_index = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,NULL);
#endif
    ac->qq->dispatch = qq_dispatch;
    return ac;
}
void qq_account_free(qq_account* ac)
{
    int i;
    PurpleConnection* gc = purple_account_get_connection(ac->account);
    for(i=0;i<ac->opend_chat->len;i++){
        purple_conversation_destroy(purple_find_chat(gc, i));
    }
    g_ptr_array_free(ac->opend_chat,0);
#if QQ_USE_FAST_INDEX
    g_hash_table_destroy(ac->fast_index.qqnum_index);
    g_hash_table_destroy(ac->fast_index.uin_index);
#endif
    g_free(ac);
}

void qq_account_insert_index_node(qq_account* ac,int type,void* data)
{
#if QQ_USE_FAST_INDEX
    if(!ac || !data) return;
    index_node* node = s_malloc(sizeof(*node));
    node->type = type;
    node->node = data;
    if(type == NODE_IS_BUDDY){
        LwqqBuddy* buddy = data;
        g_hash_table_insert(ac->fast_index.uin_index,buddy->uin,node);
        if(buddy->qqnumber)
            g_hash_table_insert(ac->fast_index.qqnum_index,buddy->qqnumber,node);
    }else{
        LwqqGroup* group = data;
        g_hash_table_insert(ac->fast_index.uin_index,group->gid,node);
        if(group->account)
            g_hash_table_insert(ac->fast_index.qqnum_index,group->account,node);
    }
#endif
}
void qq_account_remove_index_node(qq_account* ac,int type,void* data)
{
#if QQ_USE_FAST_INDEX
    if(!ac || !data) return;
    if(type == NODE_IS_BUDDY){
        LwqqBuddy* buddy = data;
        if(buddy->qqnumber)g_hash_table_remove(ac->fast_index.qqnum_index,buddy->qqnumber);
        g_hash_table_remove(ac->fast_index.uin_index,buddy->uin);
    }else{
        LwqqGroup* group = data;
        if(group->account) g_hash_table_remove(ac->fast_index.qqnum_index,group->account);
        g_hash_table_remove(ac->fast_index.uin_index,group->gid);
    }
#endif
}

int open_new_chat(qq_account* ac,LwqqGroup* group)
{
    GPtrArray* opend_chat = ac->opend_chat;
    int index;
    for(index = 0;index<opend_chat->len;index++){
        if(g_ptr_array_index(opend_chat,index)==group)
            return index;
    }
    g_ptr_array_add(opend_chat,group);
    return index;
}
#if 0
/**m_t == 0 buddy_message m_t == 1 chat_message*/
static system_msg* system_msg_new(LwqqMsgType m_t,const char* who,qq_account* ac,const char* msg,int type,time_t t)
{
    system_msg* ret = s_malloc0(sizeof(*ret));
    ret->msg_type = m_t;
    ret->who = s_strdup(who);
    ret->ac = ac;
    ret->msg = s_strdup(msg);
    ret->type = type;
    ret->t = t;
    return ret;
}
static void system_msg_free(system_msg* m)
{
    if(m){
        s_free(m->who);
        s_free(m->msg);
    }
    s_free(m);
}
static int sys_msg_write(LwqqClient* lc,void* data)
{
    system_msg* msg = data;
    PurpleConversation* conv = find_conversation(msg->msg_type,msg->who,msg->ac);
    if(conv)
        purple_conversation_write(conv,NULL,msg->msg,msg->type,msg->t);
    system_msg_free(msg);
    return 0;
}
#endif

void qq_sys_msg_write(qq_account* ac,LwqqMsgType m_t,const char* serv_id,const char* msg,PurpleMessageFlags type,time_t t)
{
    //ac->qq->dispatch(vp_func_2p,(CALLBACK_FUNC)sys_msg_write,ac->qq,system_msg_new(m_t,serv_id,ac,msg,type,t));

    PurpleConversation* conv = find_conversation(m_t,serv_id,ac);
    if(conv)
        purple_conversation_write(conv,NULL,msg,type,t);
}

PurpleConversation* find_conversation(LwqqMsgType msg_type,const char* serv_id,qq_account* ac)
{
    PurpleAccount* account = ac->account;
    const char* local_id;
    if(msg_type == LWQQ_MT_BUDDY_MSG){
        if(ac->qq_use_qqnum){
            LwqqBuddy* buddy = ac->qq->find_buddy_by_uin(ac->qq,serv_id);
            local_id = (buddy&&buddy->qqnumber)?buddy->qqnumber:serv_id;
        }else local_id = serv_id;
        return purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,local_id,account);
    } else if(msg_type == LWQQ_MT_GROUP_MSG || msg_type == LWQQ_MT_DISCU_MSG){
        if(ac->qq_use_qqnum){
            LwqqGroup* group = find_group_by_gid(ac->qq,serv_id);
            local_id = (group&&group->account)?group->account:serv_id;
        }else local_id = serv_id;
        return purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,local_id,account);
    } else 
        return NULL;
}

LwqqBuddy* find_buddy_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    qq_account* ac = lwqq_client_userdata(lc);
#if QQ_USE_FAST_INDEX
    index_node* node = g_hash_table_lookup(ac->fast_index.qqnum_index,qqnum);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_BUDDY) return NULL;
    return node->node;
#else
    return lwqq_buddy_find_buddy_by_qqnumber(lc, qqnum);
#endif
}
LwqqGroup* find_group_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    qq_account* ac = lwqq_client_userdata(lc);
#if QQ_USE_FAST_INDEX
    index_node* node = g_hash_table_lookup(ac->fast_index.qqnum_index,qqnum);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_GROUP) return NULL;
    return node->node;
#else
    LwqqGroup* group;
    LIST_FOREACH(group,&lc->groups,entries) {
        if(!group->account) continue;
        if(strcmp(group->account,qqnum)==0)
            return group;
    }
    return NULL;
#endif
}

LwqqBuddy* find_buddy_by_uin(LwqqClient* lc,const char* uin)
{
#if QQ_USE_FAST_INDEX
    qq_account* ac = lwqq_client_userdata(lc);
    index_node* node = g_hash_table_lookup(ac->fast_index.uin_index,uin);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_BUDDY) return NULL;
    return node->node;
#else
    return lwqq_buddy_find_buddy_by_uin(lc, uin);
#endif
}
LwqqGroup* find_group_by_gid(LwqqClient* lc,const char* gid)
{
#if QQ_USE_FAST_INDEX
    qq_account* ac = lwqq_client_userdata(lc);
    index_node* node = g_hash_table_lookup(ac->fast_index.uin_index,gid);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_GROUP) return NULL;
    return node->node;
#else
    return lwqq_group_find_group_by_gid(lc, gid);
#endif
}
void vp_func_4pl(CALLBACK_FUNC func,vp_list* vp,void* q)
{
    typedef void (*f)(void*,void*,void*,void*,long);
    if( q ){
        va_list* va = q;
        vp_init(*vp,sizeof(void*)*4+sizeof(long));
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,long);
        return ;
    }
    void* p1 = vp_arg(*vp,void*);
    void* p2 = vp_arg(*vp,void*);
    void* p3 = vp_arg(*vp,void*);
    void* p4 = vp_arg(*vp,void*);
    long p5 = vp_arg(*vp,long);
    ((f)func)(p1,p2,p3,p4,p5);
}
