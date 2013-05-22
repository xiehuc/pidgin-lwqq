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
#include <stdarg.h>
#include "queue.h"
#include "vplist.h"
#define LWQQ_MAGIC 0x4153

#define USE_MSG_THREAD 1

#ifndef USE_DEBUG
#define USE_DEBUG 0
#endif

#define LWQQ_DEFAULT_CATE "My Friends"
#define LWQQ_PASSERBY_CATE "Passerby"
#define LWQQ_RETRY_VALUE 3


typedef struct _LwqqAsyncEvent LwqqAsyncEvent;
typedef struct _LwqqAsyncEvset LwqqAsyncEvset;
typedef struct _LwqqAsyncOption LwqqAsyncOption;
typedef struct _LwqqHttpRequest LwqqHttpRequest;
//typedef struct _LwqqClient LwqqClient;
typedef LIST_HEAD(,LwqqAsyncEntry) LwqqAsyncQueue;


typedef VP_DISPATCH DISPATCH_FUNC;
typedef VP_CALLBACK CALLBACK_FUNC;
typedef vp_command  LwqqCommand;
#define _F_(f) (CALLBACK_FUNC)f
#define _C_2(d,c,...) vp_make_command(vp_func_##d,_F_(c),__VA_ARGS__)
#define _C_(d,c,...) _C_2(d,c,__VA_ARGS__)
#define _P_2(d,...) vp_make_params(vp_func_##d,__VA_ARGS__)
#define _P_(d,...) _P_2(d,__VA_ARGS__)
//return zero means continue.>1 means abort
typedef int (*LWQQ_PROGRESS)(void* data,size_t now,size_t total);
/************************************************************************/

//=========================INSTRUCTION=================================//
/**
 * LWQQ head is defined local enum
 * WEBQQ head is defined server enum
 * that is it's value same as Server
 *
 * there are some old LWQQ head that should be WEBQQ
 */

typedef enum {
    LWQQ_STATUS_LOGOUT = 0,
    LWQQ_STATUS_ONLINE = 10,
    LWQQ_STATUS_OFFLINE = 20,
    LWQQ_STATUS_AWAY = 30,
    LWQQ_STATUS_HIDDEN = 40,
    LWQQ_STATUS_BUSY = 50,
    LWQQ_STATUS_CALLME = 60,
    LWQQ_STATUS_SLIENT = 70
}LwqqStatus;
typedef enum {
    LWQQ_CLIENT_PC=1,/*1-10*/
    LWQQ_CLIENT_MOBILE=21,/*21-24*/
    LWQQ_CLIENT_WEBQQ=41,
    LWQQ_CLIENT_QQFORPAD=42
}LwqqClientType;
typedef enum { 
    LWQQ_MASK_NONE = 0,
    LWQQ_MASK_1 = 1,
    LWQQ_MASK_ALL=2 
}LwqqMask;
typedef enum {
    LWQQ_MEMBER_IS_ADMIN = 0x1,
}LwqqMemberFlags;
#define LWQQ_UNKNOW 0
typedef enum {
    LWQQ_MOUTH=1,  LWQQ_CATTLE,    LWQQ_TIGER,    LWQQ_RABBIT,
    LWQQ_DRAGON,   LWQQ_SNACK,     LWQQ_HORSE,    LWQQ_SHEEP,
    LWQQ_MONKEY,   LWQQ_CHOOK,     LWQQ_DOG,      LWQQ_PIG
}LwqqShengxiao;
typedef enum {
    LWQQ_AQUARIUS=1,  LWQQ_PISCES,    LWQQ_ARIES,    LWQQ_TAURUS,
    LWQQ_GEMINI,      LWQQ_CANCER,    LWQQ_LEO,      LWQQ_VIRGO,
    LWQQ_LIBRA,       LWQQ_SCORPIO,   LWQQ_SAGITTARIUS,    LWQQ_CAPRICORNUS
}LwqqConstel;
typedef enum {
    LWQQ_BLOOD_A=1,   LWQQ_BLOOD_B,    LWQQ_BLOOD_O,
    LWQQ_BLOOD_AB,    LWQQ_BLOOD_OTHER
}LwqqBloodType;
typedef enum {
    LWQQ_FEMALE = 1,
    LWQQ_MALE = 2
}LwqqGender;
typedef enum {
    LWQQ_DEL_FROM_OTHER = 2/* delete buddy and remove myself from other buddy list */
}LwqqDelFriendType;


/* Lwqq Error Code */
typedef enum {
    LWQQ_EC_ERROR =                -1,                 //<general error
    LWQQ_EC_NO_RESULT =            -2,
    LWQQ_EC_TIMEOUT_OVER =         -3,
    LWQQ_EC_CANCELED =             -4,
    LWQQ_EC_UPLOADRETRY_OVER =     -10,
    LWQQ_EC_NULL_POINTER,
    LWQQ_EC_FILE_NOT_EXIST,

    LWQQ_EC_NETWORK_ERROR = 20,
    LWQQ_EC_HTTP_ERROR = 30,
    LWQQ_EC_DB_EXEC_FAIELD = 50,
    LWQQ_EC_DB_CLOSE_FAILED,
    //old uncleard api
    LWQQ_EC_OK = 0,
    LWQQ_EC_LOGIN_NEED_VC = 10,
    LWQQ_EC_LOGIN_ABNORMAL = 60,///<登录需要解禁
    LWQQ_EC_NO_MESSAGE = 102,
    LWQQ_EC_PTWEBQQ = 116
} LwqqErrorCode;
//**should depreciate **/
typedef enum {
    LWQQ_CALLBACK_VALID = LWQQ_EC_OK,
    LWQQ_CALLBACK_FAILED = LWQQ_EC_ERROR,
    LWQQ_CALLBACK_TIMEOUT = LWQQ_EC_TIMEOUT_OVER,
    LWQQ_CALLBACK_CANCELED = LWQQ_EC_CANCELED,
}LwqqCallbackCode;
//**should depreciate **/
typedef enum {
    WEBQQ_OK = 0,
    WEBQQ_NO_MESSAGE = 102,
    WEBQQ_108 = 108,
    WEBQQ_NEW_PTVALUE = 116,
    WEBQQ_LOST_CONN = 121,
    WEBQQ_FATAL = 100000
}WebqqRetCode;

#define LWQQ_FRIEND_CATE_IDX_DEFAULT 0
#define LWQQ_FRIEND_CATE_IDX_PASSERBY -1
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
    LwqqConstel constel;
    LwqqBloodType blood;
    char *homepage;
    char *country;
    char *city;
    char *personal;
    char *nick;
    char *long_nick;
    LwqqShengxiao shengxiao;
    char *email;
    char *province;
    LwqqGender gender;
    char *mobile;
    char *vip_info;
    char *markname;
    LwqqStatus stat;
    LwqqClientType client_type;
    time_t birthday;
    char *flag;
    int cate_index;           /**< Index of the category */
    //extra data
    char *avatar;
    size_t avatar_len;
    time_t last_modify;
    char *token;                /**< Only used in add friend */
    //char *uiuin;                /**< Only used in add friend */
    void *data;                 /**< user defined data */
    int level;
    //short page;
    //pthread_mutex_t mutex;
    LIST_ENTRY(LwqqBuddy) entries; /* FIXME: Do we really need this? */
} LwqqBuddy;


typedef LIST_HEAD(LwqqFriendList,LwqqBuddy) 
    LwqqFriendList;
typedef struct LwqqSimpleBuddy{
    char* uin;
    char* qq;
    char* nick;
    char* card;                 /* 群名片 */
    LwqqClientType client_type;
    LwqqStatus stat;
    LwqqMemberFlags mflag;
    char* cate_index;
    char* group_sig;            /* only use at sess message */
    LIST_ENTRY(LwqqSimpleBuddy) entries;
}LwqqSimpleBuddy;

/* QQ group */
typedef struct LwqqGroup {
    enum{
        LWQQ_GROUP_QUN,
        LWQQ_GROUP_DISCU,
    }type;
    char *name;                  /**< QQ Group name */
    union{
    char *gid;                   /**< QQ Group id */
    char *did;                   /**< QQ Discu id */
    };
    union{
    char *account;               /** < QQ Group number */
    char *qq;                    /** < QQ number */
    };
    char *code;    
    char *markname;              /** < QQ Group mark name */

    /* ginfo */
    char *face;
    char *memo;
    char *class;
    char *fingermemo;
    time_t createtime;
    char *level;
    char *owner;                 /** < owner's QQ number  */
    char *flag;
    char *option;
    LwqqMask mask;             /** < group mask */

    char *group_sig;            /** < use in sess msg */

    time_t last_modify;
    char *avatar;
    size_t avatar_len;
    void *data;                 /** < user defined data */

    LIST_ENTRY(LwqqGroup) entries;
    LIST_HEAD(, LwqqSimpleBuddy) members; /** < QQ Group members */
    LwqqAsyncQueue ev_queue;
} LwqqGroup;
#define lwqq_member_is_founder(member,group) (strcmp(member->uin,group->owner)==0)
#define lwqq_group_is_qun(group) (group->type==LWQQ_GROUP_QUN)
#define lwqq_group_is_discu(group) (group->type==LWQQ_GROUP_DISCU)

typedef struct LwqqVerifyCode {
    char *str;
    char *uin;
    char *data;
    size_t size;
    LwqqCommand cmd;
} LwqqVerifyCode ;

typedef enum {LWQQ_NO,LWQQ_YES,LWQQ_EXTRA_ANSWER,LWQQ_IGNORE} LwqqAnswer;
#define LWQQ_ALLOW_AND_ADD LWQQ_EXTRA_ANSWER

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
    char *RK;
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
    const LwqqAsyncOption* async_opt;
    LwqqCookies *cookies;

    LwqqStatus stat;
    char *error_description;
    char *new_ptwebqq;              /**< this only used when relogin */

    LwqqFriendList friends; /**< QQ friends */
    LIST_HEAD(, LwqqFriendCategory) categories; /**< QQ friends categories */
    LIST_HEAD(, LwqqGroup) groups; /**< QQ groups */
    LIST_HEAD(, LwqqGroup) discus; /**< QQ discus */
    struct LwqqRecvMsgList *msg_list;
    long msg_id;            /**< Used to send message */

    LwqqAsyncQueue ev_queue;

    LwqqBuddy* (*find_buddy_by_uin)(struct LwqqClient* lc,const char* uin);
    LwqqBuddy* (*find_buddy_by_qqnumber)(struct LwqqClient* lc,const char* qqnumber);

    /** non data area **/

    void* data;                     /**< user defined data*/
    void (*dispatch)(DISPATCH_FUNC,CALLBACK_FUNC,...);

    int magic;          /**< 0x4153 **/
} LwqqClient;
#define lwqq_client_userdata(lc) (lc->data)


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
#define lwqq_client_logined(lc) (lc->stat != LWQQ_STATUS_LOGOUT)

/** 
 * Get cookies needby by webqq server
 * 
 * @param lc 
 * 
 * @return Cookies string on success, or null on failure
 */
const char *lwqq_get_cookies(LwqqClient *lc);

void lwqq_set_cookie(LwqqCookies* ,const char* key,const char* value);
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
void* lwqq_get_http_handle(LwqqClient* lc);

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
LwqqBuddy *lwqq_buddy_find_buddy_by_qqnumber(LwqqClient *lc, const char *qqnumber);
LwqqBuddy* lwqq_buddy_find_buddy_by_name(LwqqClient* lc,const char* name);


LwqqFriendCategory* lwqq_category_find_by_name(LwqqClient* lc,const char* name);

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
 * @param type LWQQ_GROUP_DISCU or LWQQ_GROUP_QUN
 * 
 * @return A LwqqGroup instance
 */
LwqqGroup *lwqq_group_new(int type);

/** 
 * Find group object or discus object by group's gid member
 * 
 * @param lc Our Lwqq client object
 * @param uin The gid of group which we want to find
 * 
 * @return A LwqqGroup instance
 */
LwqqGroup *lwqq_group_find_group_by_gid(LwqqClient *lc, const char *gid);
LwqqGroup* lwqq_group_find_group_by_qqnumber(LwqqClient* lc,const char* qqnumber);

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



/************************************************************************/

const char* lwqq_status_to_str(LwqqStatus status);
LwqqStatus lwqq_status_from_str(const char* str);

//long time
#define LTIME lwqq_time()
long lwqq_time();



#endif  /* LWQQ_TYPE_H */
