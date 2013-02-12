/**
 * @file   info.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 27 19:49:51 2012
 *
 * @brief  Fetch QQ information. e.g. friends information, group information.
 * 
 * 
 */

#ifndef LWQQ_INFO_H
#define LWQQ_INFO_H

#include "type.h"
#include "msg.h"

typedef struct LwqqBusinessCard {
    char* phone;
    char* uin;
    char* email;
    char* remark;
    char* gcode;
    char* name;
    LwqqGender gender;
} LwqqBusinessCard;

typedef struct LwqqRecentItem {
    enum {
        ITEM_IS_BUDDY,
        ITEM_IS_GROUP,
        ITEM_IS_DISCU
    } type;
    char* uin;
    LIST_ENTRY(LwqqRecentItem) entries;
} LwqqRecentItem;

typedef LIST_HEAD(,LwqqRecentItem) LwqqRecentList;


/** 
 * Get QQ friends information. These information include basic friend
 * information, friends group information, and so on
 * 
 * @param lc 
 * @param err 
 */
LwqqAsyncEvent* lwqq_info_get_friends_info(LwqqClient *lc, LwqqErrorCode *err);

/** 
 * Get QQ groups' name information. Get only 'name', 'gid' , 'code' .
 * 
 * @param lc 
 * @param err 
 */
LwqqAsyncEvent* lwqq_info_get_group_name_list(LwqqClient *lc, LwqqErrorCode *err);

LwqqAsyncEvent* lwqq_info_get_discu_name_list(LwqqClient* lc);


/** 
 * Get detail information of QQ friend(NB: include myself)
 * QQ server need us to pass param like:
 * tuin=244569070&verifysession=&code=&vfwebqq=e64da25c140c66
 * 
 * @param lc 
 * @param buddy 
 * @param err 
 */
LwqqAsyncEvent* lwqq_info_get_friend_detail_info(LwqqClient *lc, LwqqBuddy *buddy);
/**
 * Store QQ face to LwqqBuddy::avatar
 * and save it possibly
 * @note you want to cache it you 
 *       first need provide qqnumber of group or buddy
 *       or lwqq do not know save it to where
 *       understand?
 * @param lc
 * @param buddy or group
 * @param err the pointer of LwqqErrorCode
 * @return 1 means updated from server.
 *         2 means read from local cache
 *         0 means failed
 */
#define lwqq_info_get_friend_avatar(lc,buddy) \
((buddy!=NULL) ? lwqq_info_get_avatar(lc,buddy,NULL):NULL) 

#define lwqq_info_get_group_avatar(lc,group) \
((group!=NULL) ? lwqq_info_get_avatar(lc,NULL,group):NULL) 


LwqqAsyncEvent* lwqq_info_get_avatar(LwqqClient* lc,LwqqBuddy* buddy,LwqqGroup* group);
/** 
 * Get all friends qqnumbers
 * 
 * @param lc 
 * @param err 
 */
void lwqq_info_get_all_friend_qqnumbers(LwqqClient *lc, LwqqErrorCode *err);

/** 
 * Get friend qqnumber
 * 
 * @param lc 
 * @param uin 
 * 
 * @return qqnumber on sucessful, NB: caller is responsible for freeing
 * the memory returned by this function
 */
#define lwqq_info_get_friend_qqnumber(lc,buddy) (lwqq_info_get_qqnumber(lc,buddy,NULL))
#define lwqq_info_get_group_qqnumber(lc,group) (lwqq_info_get_qqnumber(lc,NULL,group))

LwqqAsyncEvent* lwqq_info_get_qqnumber(LwqqClient* lc,LwqqBuddy* buddy,LwqqGroup* group);

/**
 * Get QQ groups detail information. 
 * 
 * @param lc 
 * @param group
 * @param err 
 */
LwqqAsyncEvent* lwqq_info_get_group_detail_info(LwqqClient *lc, LwqqGroup *group,
                                     LwqqErrorCode *err);
#define lwqq_info_get_discu_detail_info(lc,group) (lwqq_info_get_group_detail_info(lc,group,NULL));

/** 
 * Get online buddies
 * NB : This function must be called after lwqq_info_get_friends_info()
 * because we stored buddy's status in buddy object which is created in
 * lwqq_info_get_friends_info()
 * 
 * @param lc 
 * @param err 
 */
LwqqAsyncEvent* lwqq_info_get_online_buddies(LwqqClient *lc, LwqqErrorCode *err);
LwqqAsyncEvent* lwqq_info_change_buddy_markname(LwqqClient* lc,LwqqBuddy* buddy,const char* alias);
LwqqAsyncEvent* lwqq_info_change_group_markname(LwqqClient* lc,LwqqGroup* group,const char* alias);
LwqqAsyncEvent* lwqq_info_change_discu_topic(LwqqClient* lc,LwqqGroup* group,const char* alias);
/**
 * @param new_cate the catetory name. if lwqq cannot find out such cate .would return NULL
 */
LwqqAsyncEvent* lwqq_info_modify_buddy_category(LwqqClient* lc,LwqqBuddy* buddy,const char* new_cate);
/** after call this. poll would recv blist change message.
 * in there do real delete work and ui should delete buddy info
 */
LwqqAsyncEvent* lwqq_info_delete_friend(LwqqClient* lc,LwqqBuddy* buddy,LwqqDelFriendType del_type);
/** after call this. before real delete. lwqq would call async_opt->delete_group.
 * in this ui should delete linked group info
 */
LwqqAsyncEvent* lwqq_info_delete_group(LwqqClient* lc,LwqqGroup* group);
//no necessary to call
void lwqq_info_get_group_sig(LwqqClient* lc,LwqqGroup* group,const char* to_uin);

LwqqAsyncEvent* lwqq_info_change_status(LwqqClient* lc,LwqqStatus status);
LwqqAsyncEvent* lwqq_info_mask_group(LwqqClient* lc,LwqqGroup* group,LwqqMask mask);

/**@param out use lwqq_buddy_new to create buddy and passit to here.
 * when succees. lwqq would fill out necessary info.
 * note lwqq would call async_opt->need_verify to process captcha
 */
LwqqAsyncEvent* lwqq_info_search_friend_by_qq(LwqqClient* lc,const char* qq,LwqqBuddy* out);
/**@param out : use what you get in lwqq_info_search_friend_by_qq. and you can add other info
 * such as cate_index
 * @param message : the extra reason .
 * note lwqq would call async_opt->need_verify to process captcha
 */
LwqqAsyncEvent* lwqq_info_add_friend(LwqqClient* lc,LwqqBuddy* out,const char* message);
//just like search_friend
LwqqAsyncEvent* lwqq_info_search_group_by_qq(LwqqClient* lc,const char* qq,LwqqGroup* out);
//just like add_friend
LwqqAsyncEvent* lwqq_info_add_group(LwqqClient* lc,LwqqGroup* group,const char* msg);
/**use this when you received sys g message with type (request join).
 * @param out : use lwqq_buddy_new to create a empty buddy.
 * @param msg : first use lwqq_msg_new(LWQQ_MT_SYS_G_MSG) to create a empty message.
 *              then use lwqq_msg_move to fill original message data.
 *              then pass it into here.
 *              because poll msg would auto free original message data. so we should 
 *              'move' it to another handle.
 */
LwqqAsyncEvent* lwqq_info_get_stranger_info(LwqqClient* lc,LwqqMsgSysGMsg* msg,LwqqBuddy* out);
/** use this when you received sys g message with type (request join).
 * normally you should use get_stanger_info first and ask user whether accept or deny request.
 * @param msg : use you moved message.
 * @param answer : yes or no.
 * @param reason : if answer is yes . then there is markname.
 *                 if answer is no . then there is reason.
 */
LwqqAsyncEvent* lwqq_info_answer_request_join_group(LwqqClient* lc,LwqqMsgSysGMsg* msg ,LwqqAnswer answer,const char* reason);
LwqqAsyncEvent* lwqq_info_answer_request_friend(LwqqClient* lc,const char* qq,LwqqAnswer allow,const char* extra);

LwqqAsyncEvent* lwqq_info_get_group_public(LwqqClient* lc,LwqqGroup* g);
void lwqq_card_free(LwqqBusinessCard* card);
LwqqAsyncEvent* lwqq_info_get_self_card(LwqqClient* lc,LwqqGroup* g,LwqqBusinessCard* card);
LwqqAsyncEvent* lwqq_info_set_self_card(LwqqClient* lc,LwqqBusinessCard* card);
LwqqAsyncEvent* lwqq_info_get_single_long_nick(LwqqClient* lc,LwqqBuddy* buddy);
LwqqAsyncEvent* lwqq_info_get_group_memo(LwqqClient* lc,LwqqGroup* g);
LwqqAsyncEvent* lwqq_info_set_dicsu_topic(LwqqClient* lc,LwqqGroup* d,const char* topic);
void lwqq_recent_list_free(LwqqRecentList* list);
LwqqAsyncEvent* lwqq_info_recent_list(LwqqClient* lc,LwqqRecentList* list);

#endif  /* LWQQ_INFO_H */
