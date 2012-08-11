#include "qq_types.h"

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
