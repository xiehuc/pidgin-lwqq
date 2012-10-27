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

LwqqAsyncEvent* lwqq_info_get_discu_list(LwqqClient* lc);

LwqqAsyncEvent* lwqq_info_get_discu_detail_info(LwqqClient* lc,LwqqDiscu* discu);

/** 
 * Get detail information of QQ friend(NB: include myself)
 * QQ server need us to pass param like:
 * tuin=244569070&verifysession=&code=&vfwebqq=e64da25c140c66
 * 
 * @param lc 
 * @param buddy 
 * @param err 
 */
void lwqq_info_get_friend_detail_info(LwqqClient *lc, LwqqBuddy *buddy,
                                      LwqqErrorCode *err);
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
#define LWQQ_CACHE_DIR "/tmp/lwqq/"
#define lwqq_info_get_friend_avatar(lc,buddy) \
((buddy!=NULL) ? lwqq_info_get_avatar(lc,0,buddy):NULL) 

#define lwqq_info_get_group_avatar(lc,group) \
((group!=NULL) ? lwqq_info_get_avatar(lc,1,group):NULL) 


void lwqq_info_get_avatar(LwqqClient* lc,int isgroup,void* grouporbuddy);
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
#define lwqq_info_get_friend_qqnumber(lc,buddy) \
((buddy!=NULL) ? lwqq_info_get_qqnumber(lc,0,buddy):NULL) 

#define lwqq_info_get_group_qqnumber(lc,group) \
((group!=NULL) ? lwqq_info_get_qqnumber(lc,1,group):NULL) 

LwqqAsyncEvent* lwqq_info_get_qqnumber(LwqqClient* lc,int isgroup,void* grouporbuddy);

/**
 * Get QQ groups detail information. 
 * 
 * @param lc 
 * @param group
 * @param err 
 */
LwqqAsyncEvent* lwqq_info_get_group_detail_info(LwqqClient *lc, LwqqGroup *group,
                                     LwqqErrorCode *err);

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
LwqqAsyncEvent* lwqq_info_modify_buddy_category(LwqqClient* lc,LwqqBuddy* buddy,const char* new_cate);
typedef enum {
    LWQQ_DEL_FROM_OTHER = 2/* delete buddy and remove myself from other buddy list */
}LWQQ_DEL_FRIEND_TYPE;
LwqqAsyncEvent* lwqq_info_delete_friend(LwqqClient* lc,LwqqBuddy* buddy,LWQQ_DEL_FRIEND_TYPE del_type);
LwqqAsyncEvent* lwqq_info_allow_added_request(LwqqClient* lc,const char* account);
LwqqAsyncEvent* lwqq_info_deny_added_request(LwqqClient* lc,const char* account,const char* reason);
LwqqAsyncEvent* lwqq_info_allow_and_add(LwqqClient* lc,const char* account,const char* markname);
void lwqq_info_get_group_sig(LwqqClient* lc,LwqqGroup* group,const char* to_uin);
LwqqAsyncEvent* lwqq_info_change_status(LwqqClient* lc,LWQQ_STATUS status);
LwqqVerifyCode* lwqq_info_add_friend_get_image();
LwqqAsyncEvent* lwqq_info_mask_group(LwqqClient* lc,LwqqGroup* group,LWQQ_MASK mask);


#endif  /* LWQQ_INFO_H */
