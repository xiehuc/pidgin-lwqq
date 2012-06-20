/**
 * @file   type.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 20 22:24:30 2012
 * 
 * @brief  Linux WebQQ Data Struct API
 * 
 * 
 */

#ifndef LWQQ_TYPE_H
#define LWQQ_TYPE_H

#include "queue.h"
#include "msg.h"

/************************************************************************/
/* Struct defination */

typedef struct LwqqFriendCategory {
    int index;
    int sort;
    char *name;
    LIST_ENTRY(LwqqFriendCategory) entries;
} LwqqFriendCategory;

/* QQ buddy */
typedef struct LwqqBuddy {
    char *uin;                  /**< Uin. Change every login */
    char *qqnumber;             /**< QQ number */
    char *face;
    char *occupation;
    char *phone;
    char *allow;
    char *college;
    char *reg_time;
    char *constel;
    char *blood;
    char *homepage;
    char *stat;
    char *country;
    char *city;
    char *personal;
    char *nick;
    char *shengxiao;
    char *email;
    char *province;
    char *gender;
    char *mobile;
    char *vip_info;
    char *markname;

    char *flag;

    char *cate_index;           /**< Index of the category */

    /*
     * 1 : Desktop client
     * 21: Mobile client
     * 41: Web QQ Client
     */
    char *client_type;
    LIST_ENTRY(LwqqBuddy) entries;
} LwqqBuddy;

/* QQ group */
typedef struct LwqqGroup {
    char *name;                  /**< QQ Group name */
    char *gid;
    char *code;    
    char *account;               /** < QQ Group number */
    char *markname;              /** < QQ Group mark name */

    /* ginfo */
    char *face;
    char *memo;
    char *class;
    char *fingermemo;
    char *createtime;
    char *level;
    char *owner;                 /** < owner's QQ number  */
    char *flag;
    char *option;

    LIST_ENTRY(LwqqGroup) entries;

} LwqqGroup;

typedef struct LwqqVerifyCode {
    char *str;
    char *type;
    char *img;
    char *uin;
} LwqqVerifyCode ;

typedef struct LwqqCookies {
    char *ptvfsession;          /**< ptvfsession */
    char *ptcz;
    char *skey;
    char *ptwebqq;
    char *ptuserinfo;
    char *uin;
    char *ptisp;
    char *pt2gguin;
    char *verifysession;
    char *lwcookies;
} LwqqCookies;

/* LwqqClient API */
typedef struct LwqqClient {
    char *username;             /**< Username */
    char *password;             /**< Password */
    LwqqBuddy *myself;          /**< Myself */
    char *version;              /**< WebQQ version */
    LwqqVerifyCode *vc;         /**< Verify Code */
    char *clientid;
    char *seskey;
    char *cip;
    char *index;
    char *port;
    char *status;
    char *vfwebqq;
    char *psessionid;
    LwqqCookies *cookies;
    LIST_HEAD(, LwqqBuddy) friends; /**< QQ friends */
    LIST_HEAD(, LwqqFriendCategory) categories; /**< QQ friends categories */
    LIST_HEAD(, LwqqGroup) groups; /**< QQ groups */
    LwqqRecvMsgList *msg_list;
    long msg_id;            /**< Used to send message */
} LwqqClient;

/* Lwqq Error Code */
typedef enum {
    LWQQ_EC_OK,
    LWQQ_EC_ERROR,
    LWQQ_EC_NULL_POINTER,
    LWQQ_EC_FILE_NOT_EXIST,
    LWQQ_EC_LOGIN_NEED_VC = 10,
    LWQQ_EC_NETWORK_ERROR = 20,
    LWQQ_EC_HTTP_ERROR = 30,
    LWQQ_EC_DB_EXEC_FAIELD = 50,
    LWQQ_EC_DB_CLOSE_FAILED,
} LwqqErrorCode;

/* Struct defination end */

/************************************************************************/
/* LwqqClient API */
/** 
 * Create a new lwqq client
 * 
 * @param username QQ username
 * @param password QQ password
 * 
 * @return A new LwqqClient instance, or NULL failed
 */
LwqqClient *lwqq_client_new(const char *username, const char *password);

/** 
 * Get cookies needby by webqq server
 * 
 * @param lc 
 * 
 * @return Cookies string on success, or null on failure
 */
char *lwqq_get_cookies(LwqqClient *lc);

/** 
 * Free LwqqClient instance
 * 
 * @param client LwqqClient instance
 */
void lwqq_client_free(LwqqClient *client);

/* LwqqClient API end */

/************************************************************************/
/* LwqqBuddy API  */
/** 
 * 
 * Create a new buddy
 * 
 * @return A LwqqBuddy instance
 */
LwqqBuddy *lwqq_buddy_new();

/** 
 * Free a LwqqBuddy instance
 * 
 * @param buddy 
 */
void lwqq_buddy_free(LwqqBuddy *buddy);

/** 
 * Find buddy object by buddy's uin member
 * 
 * @param lc Our Lwqq client object
 * @param uin The uin of buddy which we want to find
 * 
 * @return 
 */
LwqqBuddy *lwqq_buddy_find_buddy_by_uin(LwqqClient *lc, const char *uin);

/* LwqqBuddy API END*/


/** 
 * Free a LwqqGroup instance
 * 
 * @param group
 */
void lwqq_group_free(LwqqGroup *group);

/** 
 * 
 * Create a new group
 * 
 * @return A LwqqGroup instance
 */
LwqqGroup *lwqq_group_new();

/** 
 * Find group object by group's gid member
 * 
 * @param lc Our Lwqq client object
 * @param uin The gid of group which we want to find
 * 
 * @return A LwqqGroup instance
 */
LwqqGroup *lwqq_group_find_group_by_gid(LwqqClient *lc, const char *gid);

/************************************************************************/

#endif  /* LWQQ_TYPE_H */
