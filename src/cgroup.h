#include "lwqq.h"

#include "qq_types.h"

struct qq_chat_group_opt;

/*typedef enum {
  QQ_CGROUP_HIDE_NEW = 1<<1,
  }qq_cgroup_properties;*/

//this is data model for special
//qq chat group to purple chat group 
//adapter
typedef struct qq_chat_group
{
	PurpleChat * chat;
	LwqqGroup * group;
	LwqqMask mask_local;
	int properties;
	struct qq_chat_group_opt* opt;
} qq_chat_group;

struct qq_chat_group_opt
{
	void (*new_msg_notice)(struct qq_chat_group* cg);
};

#define CGROUP_LWQQ(cg) (cg->group)
#define CGROUP_PURPLE(cg) (cg->chat)

#define CGROUP_SET_PROP(cg,prop,val) (val?(cg->properties|=prop):(cg->properties&=~prop))
#define CGROUP_GET_PROP(cg,prop) ((cg->properties&prop)>0)

#define CGROUP_GET_CONV(cg) purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, try_get(cg->group->account,cg->group->gid), cg->chat->account)

qq_chat_group* qq_cgroup_new(struct qq_chat_group_opt* opt);

void qq_cgroup_free(qq_chat_group* cg);

void qq_cgroup_open(qq_chat_group* cg);

void qq_cgroup_got_msg(qq_chat_group* cg,const char* local_id,PurpleMessageFlags flags,const char* message,time_t t);

void qq_cgroup_mask_local(qq_chat_group* cg, LwqqMask m);

void qq_cgroup_flush_members(qq_chat_group* cg);

unsigned int qq_cgroup_unread_num(qq_chat_group* cg);
#define CGROUP_UNREAD(cg) qq_cgroup_unread_num(cg)

// vim: tabstop=3 sw=3 sts=3 noexpandtab
