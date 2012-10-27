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
#define LWQQ_MAGIC 0x4153

typedef struct _LwqqAsyncEvent LwqqAsyncEvent;
typedef struct _LwqqAsyncEvset LwqqAsyncEvset;
typedef struct _LwqqAsync LwqqAsync;
typedef struct _LWQQ_HTTP_HANDLE LWQQ_HTTP_HANDLE;
/************************************************************************/
/* Struct defination */

typedef struct LwqqFriendCategory {
    int index;
    int sort;
    char *name;
    int count;
    LIST_ENTRY(LwqqFriendCategory) entries;
} LwqqFriendCategory;
typedef enum LWQQ_STATUS{
    LWQQ_STATUS_UNKNOW = 0,
    LWQQ_STATUS_ONLINE = 10,
    LWQQ_STATUS_OFFLINE = 20,
    LWQQ_STATUS_AWAY = 30,
    LWQQ_STATUS_HIDDEN = 40,
    LWQQ_STATUS_BUSY = 50,
    LWQQ_STATUS_CALLME = 60,
    LWQQ_STATUS_SLIENT = 70
}LWQQ_STATUS;
typedef enum LWQQ_CTYPE{
    LWQQ_CLIENT_DESKTOP=1,
    LWQQ_CLIENT_MOBILE=21,
    LWQQ_CLIENT_WEBQQ=41,
}LWQQ_CTYPE;
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
    LWQQ_STATUS stat;                 /** 10:online 20:offline 30:away 50:busy 70:请勿打扰*/
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
    LWQQ_CTYPE client_type;

    //char *status;               /* Online status */
    int ref;
    pthread_mutex_t mutex;
    LIST_ENTRY(LwqqBuddy) entries; /* FIXME: Do we really need this? */
} LwqqBuddy;
enum LWQQ_FLAG_ENUM{
    LWQQ_MEMBER_IS_ADMIN = 0x1,
};
typedef int LWQQ_FLAG;
typedef struct LwqqSimpleBuddy{
    char* uin;
    char* nick;
    char* card;                 /* 群名片 */
    LWQQ_CTYPE client_type;
    //char* stat;
    LWQQ_STATUS stat;
    LWQQ_FLAG mflag;
    char* cate_index;
    char* group_sig;            /* only use at sess message */
    LIST_ENTRY(LwqqSimpleBuddy) entries;
}LwqqSimpleBuddy;

typedef enum LWQQ_MASK{ 
    LWQQ_MASK_NONE = 0,
    LWQQ_MASK_1 = 1,
    LWQQ_MASK_ALL=2 
}LWQQ_MASK;
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
    LWQQ_MASK mask;             /** < group mask */

    char *group_sig;            /** < use in sess msg */

    char *avatar;
    size_t avatar_len;

    LIST_ENTRY(LwqqGroup) entries;
    LIST_HEAD(, LwqqSimpleBuddy) members; /** < QQ Group members */
} LwqqGroup;
#define lwqq_member_is_founder(member,group) (strcmp(member->uin,group->owner)==0)

typedef struct LwqqDiscu{
    char* did;
    char* name;
    LIST_ENTRY(LwqqDiscu) entries;
}LwqqDiscu;

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
    char *vfwebqq;
    char *psessionid;
    const char* last_err;
    char *gface_key;                  /**< use at cface */
    char *gface_sig;                  /**<use at cfage */
    LwqqAsync* async;
    LwqqCookies *cookies;
    const char *status;
    LWQQ_STATUS stat;
    char *error_description;
    //LWQQ_HTTP_HANDLE* http_handle;

    LIST_HEAD(, LwqqBuddy) friends; /**< QQ friends */
    LIST_HEAD(, LwqqFriendCategory) categories; /**< QQ friends categories */
    LIST_HEAD(, LwqqGroup) groups; /**< QQ groups */
    LIST_HEAD(, LwqqDiscu) discus; /**< QQ discus */
    struct LwqqRecvMsgList *msg_list;
    long msg_id;            /**< Used to send message */
    int magic;          /**< 0x4153 **/
} LwqqClient;

/* Lwqq Error Code */
typedef enum {
    LWQQ_EC_OK,
    LWQQ_EC_ERROR,
    LWQQ_EC_NULL_POINTER,
    LWQQ_EC_FILE_NOT_EXIST,
    LWQQ_EC_LOGIN_NEED_VC = 10,
    LWQQ_EC_LOGIN_ABNORMAL = 60,///<登录需要解禁
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

#define lwqq_client_valid(lc) (lc!=0&&lc->magic==LWQQ_MAGIC)

/** 
 * Get cookies needby by webqq server
 * 
 * @param lc 
 * 
 * @return Cookies string on success, or null on failure
 */
const char *lwqq_get_cookies(LwqqClient *lc);

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
LwqqSimpleBuddy* lwqq_simple_buddy_new();

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
LwqqSimpleBuddy *lwqq_group_find_group_member_by_uin(LwqqGroup *group, const char *uin);

#define format_append(str,format...)\
snprintf(str+strlen(str),sizeof(str)-strlen(str),##format)


///////discu api


#define lwqq_discu_new() ((LwqqDiscu*)s_malloc0(sizeof(LwqqDiscu)))
void lwqq_discu_free(LwqqDiscu* dis);


/************************************************************************/

const char* lwqq_status_to_str(LWQQ_STATUS status);
LWQQ_STATUS lwqq_status_from_str(const char* str);
//return zero means continue.>1 means abort
typedef int (*LWQQ_PROGRESS)(void* data,size_t now,size_t total);
#endif  /* LWQQ_TYPE_H */
