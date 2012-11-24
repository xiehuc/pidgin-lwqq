#include "qq_types.h"
#include "smemory.h"
#include "msg.h"
#include "async.h"

qq_account* qq_account_new(PurpleAccount* account)
{
    qq_account* ac = g_malloc0(sizeof(qq_account));
    ac->account = account;
    ac->magic = QQ_MAGIC;
    ac->opend_chat = g_ptr_array_sized_new(10);
#if QQ_USE_FAST_INDEX
    ac->fast_index.uin_index = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,g_free);
    ac->fast_index.qqnum_index = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,NULL);
#endif
    return ac;
}
void qq_account_free(qq_account* ac)
{
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

/**m_t == 0 buddy_message m_t == 1 chat_message*/
system_msg* system_msg_new(int m_t,const char* who,qq_account* ac,const char* msg,int type,time_t t)
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
void system_msg_free(system_msg* m)
{
    if(m){
        s_free(m->who);
        s_free(m->msg);
    }
    s_free(m);
}

PurpleConversation* find_conversation(int msg_type,const char* who,qq_account* ac)
{
    PurpleAccount* account = ac->account;
    if(msg_type == LWQQ_MT_BUDDY_MSG)
        return purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,who,account);
    else if(msg_type == LWQQ_MT_GROUP_MSG || msg_type == LWQQ_MT_DISCU_MSG)
        return purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,who,account);
    else 
        return NULL;
}

#if QQ_USE_FAST_INDEX
#if QQ_USE_QQNUM
LwqqBuddy* find_buddy_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    index_node* node = g_hash_table_lookup(ac->fast_index.qqnum_index,qqnum);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_BUDDY) return NULL;
    return node->node;
}
LwqqGroup* find_group_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    index_node* node = g_hash_table_lookup(ac->fast_index.qqnum_index,qqnum);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_GROUP) return NULL;
    return node->node;
}
#else
LwqqBuddy* find_buddy_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    LwqqBuddy* buddy;
    LIST_FOREACH(buddy,&lc->friends,entries) {
        //this may caused by qqnumber not loaded successful.
        if(!buddy->qqnumber) continue;
        if(strcmp(buddy->qqnumber,qqnum)==0)
            return buddy;
    }
    return NULL;
}
LwqqGroup* find_group_by_qqnumber(LwqqClient* lc,const char* qqnum)
{
    LwqqGroup* group;
    LIST_FOREACH(group,&lc->groups,entries) {
        if(!group->account) continue;
        if(strcmp(group->account,qqnum)==0)
            return group;
    }
    return NULL;
}
#endif


LwqqBuddy* find_buddy_by_uin(LwqqClient* lc,const char* uin)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    index_node* node = g_hash_table_lookup(ac->fast_index.uin_index,uin);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_BUDDY) return NULL;
    return node->node;
}
LwqqGroup* find_group_by_gid(LwqqClient* lc,const char* gid)
{
    qq_account* ac = lwqq_async_get_userdata(lc,LOGIN_COMPLETE);
    index_node* node = g_hash_table_lookup(ac->fast_index.uin_index,gid);
    if(node == NULL) return NULL;
    if(node->type != NODE_IS_GROUP) return NULL;
    return node->node;
}
#else
LwqqBuddy* find_buddy_by_uin(LwqqClient* lc,const char* uin)
{
    return lwqq_buddy_find_buddy_by_uin(lc, uin);
}
LwqqGroup* find_group_by_gid(LwqqClient* lc,const char* gid)
{
    return lwqq_group_find_group_by_gid(lc, gid);
}

#endif
