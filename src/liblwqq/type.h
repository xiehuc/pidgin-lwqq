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

#include <pthread.h>
#include "queue.h"


typedef struct _LwqqAsyncEvent LwqqAsyncEvent;
/************************************************************************/
/* Struct defination */

typedef struct LwqqFriendCategory {
    int index;
    int sort;
    char *name;
    int count;
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
    char *stat;                 /** 10:online 20:offline 30:away 50:busy 70:请勿打扰*/
    char *country;
    char *city;
    char *personal;
    char *nick;
    char *long_nick;
    char *shengxiao;
    char *email;
    char *province;
    char *gender;
    char *mobile;
    char *vip_info;
    char *markname;

    char *flag;

    char *avatar;
    size_t avatar_len;

    char *cate_index;           /**< Index of the category */

    /*
     * 1 : Desktop client
     * 21: Mobile client
     * 41: Web QQ Client
     */
    char *client_type;

    char *status;               /* Online status */
    int ref;
    pthread_mutex_t mutex;
    LIST_ENTRY(LwqqBuddy) entries; /* FIXME: Do we really need this? */
} LwqqBuddy;

typedef struct LwqqSimpleBuddy{
    char* uin;
    char* cate_index;
    LIST_ENTRY(LwqqSimpleBuddy) entries;
}LwqqSimpleBuddy;

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

    char *avatar;
    size_t avatar_len;

    LIST_ENTRY(LwqqGroup) entries;
    LIST_HEAD(, LwqqBuddy) members; /** < QQ Group members */
} LwqqGroup;

typedef struct LwqqVerifyCode {
    char *str;
    char *type;
    char *img;
    char *uin;
    char *data;
    size_t size;
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
typedef struct _LwqqAsync LwqqAsync;
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
    char *gface_key;                  /**< use at cface */
    char *gface_sig;                  /**<use at cfage */
    LwqqAsync* async;
    LwqqCookies *cookies;
    LIST_HEAD(, LwqqBuddy) friends; /**< QQ friends */
    LIST_HEAD(, LwqqFriendCategory) categories; /**< QQ friends categories */
    LIST_HEAD(, LwqqGroup) groups; /**< QQ groups */
    struct LwqqRecvMsgList *msg_list;
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
 * Free LwqqVerifyCode object
 * 
 * @param vc 
 */
void lwqq_vc_free(LwqqVerifyCode *vc);

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
#define lwqq_simple_buddy_new() ((LwqqSimpleBuddy*)s_malloc0(sizeof(LwqqSimpleBuddy)))

//add the ref count of buddy.
//that means only ref count down to zero.
//we really free the memo space.
#define lwqq_buddy_ref(buddy) buddy->ref++;
/** 
 * Free a LwqqBuddy instance
 * 
 * @param buddy 
 */
void lwqq_buddy_free(LwqqBuddy *buddy);
void lwqq_simple_buddy_free(LwqqSimpleBuddy* buddy);

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

/** 
 * Find group member object by member's uin
 * 
 * @param group Our Lwqq group object
 * @param uin The uin of group member which we want to find
 * 
 * @return A LwqqBuddy instance 
 */
LwqqBuddy *lwqq_group_find_group_member_by_uin(LwqqGroup *group, const char *uin);

#define format_append(str,format...)\
snprintf(str+strlen(str),sizeof(str)-strlen(str),##format)

/************************************************************************/

#endif  /* LWQQ_TYPE_H */
