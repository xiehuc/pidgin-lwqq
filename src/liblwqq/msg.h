/**
 * @file   msg.h
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Thu Jun 14 14:42:08 2012
 *
 * @brief
 *
 *
 */

#ifndef LWQQ_MSG_H
#define LWQQ_MSG_H

#include <pthread.h>
#include "queue.h"
#include "type.h"

typedef enum LwqqMsgType {
    LWQQ_MT_BUDDY_MSG = 0,
    LWQQ_MT_GROUP_MSG,
    LWQQ_MT_DISCU_MSG,
    LWQQ_MT_SESS_MSG, //group whisper message
    LWQQ_MT_STATUS_CHANGE,
    LWQQ_MT_KICK_MESSAGE,
    LWQQ_MT_SYSTEM,
    LWQQ_MT_BLIST_CHANGE,
    LWQQ_MT_SYS_G_MSG,
    LWQQ_MT_OFFFILE,
    LWQQ_MT_FILETRANS,
    LWQQ_MT_FILE_MSG,
    LWQQ_MT_UNKNOWN,
} LwqqMsgType;
typedef enum {
    LWQQ_MC_OK = 0,
    LWQQ_MC_FAST = 108,
    LWQQ_MC_LOST_CONN = 121
}LwqqMsgRetcode;

typedef struct LwqqMsgContent {
    enum {
        LWQQ_CONTENT_STRING,
        LWQQ_CONTENT_FACE,
        LWQQ_CONTENT_OFFPIC,
//custom face :this can send/recv picture
        LWQQ_CONTENT_CFACE
    }type;
    union {
        int face;
        char *str;
        //webqq protocol
        //this used in offpic format which may replaced by cface (custom face)
        struct {
            char* name;
            char* data;
            size_t size;
            int success;
            char* file_path;
        }img;
        struct {
            char* name;
            char* data;
            size_t size;
            char* file_id;
            char* key;
            char serv_ip[24];
            char serv_port[8];
        }cface;
    } data;
    TAILQ_ENTRY(LwqqMsgContent) entries;
} LwqqMsgContent ;

typedef struct LwqqMsgMessage {
    LwqqMsgType type;
    char *from;
    char *to;
    char *msg_id;
    int msg_id2;
    time_t time;
    union{
        struct {
            char *send; /* only group use it to identify who send the group message */
            char *group_code; /* only avaliable in group message */
        }group;
        struct {
            char *id;   /* only sess msg use it.means gid */
            char *group_sig; /* you should fill it before send */
        }sess;
        struct {
            char *send;
            char *did;
        }discu;
    };

    /* For font  */
    char *f_name;
    int f_size;
    struct {
        int b, i, u; /* bold , italic , underline */
    } f_style;
    char *f_color;

    TAILQ_HEAD(, LwqqMsgContent) content;
} LwqqMsgMessage;

typedef struct LwqqMsgStatusChange {
    char *who;
    char *status;
    int client_type;
} LwqqMsgStatusChange;

typedef struct LwqqMsgKickMessage {
    int show_reason;
    char *reason;
    char *way;
} LwqqMsgKickMessage;
typedef struct LwqqMsgSystem{
    char* seq;
    enum {
        VERIFY_REQUIRED,
        VERIFY_PASS_ADD,
        VERIFY_PASS,
        ADDED_BUDDY_SIG,
        SYSTEM_TYPE_UNKNOW
    }type;
    char* from_uin;
    char* account;
    char* stat;
    char* client_type;
    union{
        struct{
            char* sig;
        }added_buddy_sig;
        struct{
            char* msg;
            char* allow;
        }verify_required;
        struct{
            char* group_id;
        }verify_pass;
    };
} LwqqMsgSystem;
typedef struct LwqqMsgSysGMsg{
    enum {
        GROUP_CREATE,
        GROUP_JOIN,
        GROUP_LEAVE,
        GROUP_UNKNOW
    }type;
    char* gcode;
}LwqqMsgSysGMsg;
typedef struct LwqqMsgBlistChange{
    LIST_HEAD(,LwqqSimpleBuddy) added_friends;
    LIST_HEAD(,LwqqBuddy) removed_friends;
} LwqqMsgBlistChange;
typedef struct LwqqMsgOffFile{
    char* msg_id;
    char* rkey;
    char ip[24];
    char port[8];
    char* from;
    char* to;
    size_t size;
    char* name;
    char* path;///< only used when upload
    time_t expire_time;
    time_t time;
}LwqqMsgOffFile;
typedef struct FileTransItem{
    char* file_name;
    enum {
        TRANS_OK=0,
        TRANS_ERROR=50,
        TRANS_TIMEOUT=51,
        REFUSED_BY_CLIENT=53,
    }file_status;
    //int file_status;
    int pro_id;
    LIST_ENTRY(FileTransItem) entries;
}FileTransItem;
typedef struct LwqqMsgFileTrans{
    int file_count;
    char* from;
    char* to;
    char* lc_id;
    size_t now;
    int operation;
    int type;
    LIST_HEAD(,FileTransItem) file_infos;
}LwqqMsgFileTrans;
typedef struct LwqqMsgFileMessage{
    int msg_id;
    enum {
        MODE_RECV,
        MODE_REFUSE,
        MODE_SEND_ACK
    } mode;
    char* from;
    char* to;
    int msg_id2;
    int session_id;
    time_t time;
    int type;
    char* reply_ip;
    union{
        struct {
            int msg_type;
            char* name;
            int inet_ip;
        }recv;
        struct {
            enum{
                CANCEL_BY_USER=1,
                CANCEL_BY_OVERTIME=3
            } cancel_type;
        }refuse;
    };
}LwqqMsgFileMessage;


typedef struct LwqqMsg {
    /* Message type. e.g. buddy message or group message */
    LwqqMsgType type;
    void *opaque;               /**< Message details */
} LwqqMsg;

/**
 * Create a new LwqqMsg object
 *
 * @param msg_type
 *
 * @return NULL on failure
 */
LwqqMsg *lwqq_msg_new(LwqqMsgType type);

/**
 * Free a LwqqMsg object
 *
 * @param msg
 */
void lwqq_msg_free(LwqqMsg *msg);

/************************************************************************/
/* LwqqRecvMsg API */
/**
 * Lwqq Receive Message object, used by receiving message
 *
 */
typedef struct LwqqRecvMsg {
    LwqqMsg *msg;
    TAILQ_ENTRY(LwqqRecvMsg) entries;
} LwqqRecvMsg;

typedef struct LwqqRecvMsgList {
    int count;                  /**< Number of message  */
    pthread_t tid;
    pthread_attr_t attr;
    pthread_mutex_t mutex;
    TAILQ_HEAD(RecvMsgListHead, LwqqRecvMsg) head;
    void *lc;                   /**< Lwqq Client reference */
    void (*poll_msg)(struct LwqqRecvMsgList *list); /**< Poll to fetch msg */
} LwqqRecvMsgList;

/**
 * Create a new LwqqRecvMsgList object
 *
 * @param client Lwqq Client reference
 *
 * @return NULL on failure
 */
LwqqRecvMsgList *lwqq_recvmsg_new(void *client);

/**
 * Free a LwqqRecvMsgList object
 *
 * @param list
 */
void lwqq_recvmsg_free(LwqqRecvMsgList *list);

//insert msg content
#define lwqq_msg_content_append(msg,c) \
    TAILQ_INSERT_TAIL(&msg->content,c,entries)

/**
 *
 *
 * @param lc
 * @param sendmsg
 * @return return a async event. you can wait it or add a event listener.
 *
 */
LwqqAsyncEvent* lwqq_msg_send(LwqqClient *lc, LwqqMsg *msg);

/**
 * easy way to send message
 * @param type LWQQ_MT_BUDDY_MSG or LWQQ_MT_GROUP_MSG
 * @param to buddy uin or group gid
 * @param message
 * @return same as lwqq_msg_send
 */
int lwqq_msg_send_simple(LwqqClient* lc,int type,const char* to,const char* message);
/**
 * more easy way to send message
 */
#define lwqq_msg_send_buddy(lc,buddy,message) \
    ((buddy!=NULL)? lwqq_msg_send_simple(lc,LWQQ_MT_BUDDY_MSG,buddy->uin,message) : NULL)
#define lwqq_msg_send_group(lc,group,message) \
    ((group!=NULL)? lwqq_msg_send_simple(lc,LWQQ_MT_GROUP_MSG,group->gid,message) : NULL)
/* LwqqRecvMsg API end */
//fill a msg content with upload cface
LwqqMsgContent* lwqq_msg_fill_upload_cface(const char* filename,
        const void* buffer,size_t buf_size);
//fill a msg content with upload offline pic
LwqqMsgContent* lwqq_msg_fill_upload_offline_pic(const char* filename,
        const void* buffer,size_t buf_size);

LwqqAsyncEvset* lwqq_msg_request_picture(LwqqClient* lc,int type,LwqqMsgMessage* msg);
const char* lwqq_msg_offfile_get_url(LwqqMsgOffFile* msg);
LwqqAsyncEvent* lwqq_msg_accept_file(LwqqClient* lc,LwqqMsgFileMessage* msg,const char* saveto,
        LWQQ_PROGRESS progress,void* prog_data);
//call this function when upload_offfile finished.
void lwqq_msg_send_offfile(LwqqClient* lc,LwqqMsgOffFile* file);
LwqqAsyncEvent* lwqq_msg_upload_offline_file(LwqqClient* lc,LwqqMsgOffFile* file);
void lwqq_msg_offfile_free(void* opaque);
LwqqAsyncEvent* lwqq_msg_upload_file(LwqqClient* lc,LwqqMsgOffFile* file,
        LWQQ_PROGRESS progress,void* prog_data);

/************************************************************************/
/*  LwqqSendMsg API */

#endif  /* LWQQ_MSG_H */
