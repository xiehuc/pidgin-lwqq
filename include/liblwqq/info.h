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
void lwqq_info_get_friends_info(LwqqClient *lc, LwqqErrorCode *err);

/** 
 * Get QQ groups' name information. Get only 'name', 'gid' , 'code' .
 * 
 * @param lc 
 * @param err 
 */
void lwqq_info_get_group_name_list(LwqqClient *lc, LwqqErrorCode *err);

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
char *lwqq_info_get_friend_qqnumber(LwqqClient *lc, const char *uin);

#endif  /* LWQQ_INFO_H */
