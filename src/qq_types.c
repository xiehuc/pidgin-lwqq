#include "qq_types.h"
#include "smemory.h"
#include "msg.h"

qq_account* qq_account_new(PurpleAccount* account)
{
    qq_account* ac = g_malloc0(sizeof(qq_account));
    ac->account = account;
    ac->magic = QQ_MAGIC;
    ac->opend_chat = g_ptr_array_sized_new(10);
    return ac;
}
void qq_account_free(qq_account* ac)
{
    g_ptr_array_free(ac->opend_chat,0);
    g_free(ac);
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

