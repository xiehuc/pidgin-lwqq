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
    LWQQ_MF_SEQ = 1<<1,
    LWQQ_MT_MESSAGE = 1<<3|LWQQ_MF_SEQ,
    LWQQ_MS_BUDDY_MSG = LWQQ_MT_MESSAGE|(1<<8),
    LWQQ_MS_GROUP_MSG = LWQQ_MT_MESSAGE|(2<<8),
    LWQQ_MS_DISCU_MSG = LWQQ_MT_MESSAGE|(3<<8),
    LWQQ_MS_SESS_MSG = LWQQ_MT_MESSAGE|(4<<8), //group whisper message

    LWQQ_MT_STATUS_CHANGE = 2<<3,
    LWQQ_MT_KICK_MESSAGE = 3<<3,
    LWQQ_MT_SYSTEM = LWQQ_MF_SEQ|(4<<3),
    LWQQ_MS_ADD_BUDDY = LWQQ_MT_SYSTEM|(1<<8),
    LWQQ_MS_VERIFY_PASS = LWQQ_MT_SYSTEM|(2<<8),
    LWQQ_MS_VERIFY_PASS_ADD = LWQQ_MT_SYSTEM|(3<<8),
    LWQQ_MS_VERIFY_REQUIRE = LWQQ_MT_SYSTEM|(4<<8),

    LWQQ_MT_BLIST_CHANGE = 5<<3,
    LWQQ_MT_SYS_G_MSG = LWQQ_MF_SEQ|(6<<3),
    LWQQ_MS_G_CREATE = LWQQ_MT_SYS_G_MSG|(1<<8),
    LWQQ_MS_G_JOIN = LWQQ_MT_SYS_G_MSG|(2<<8),
    LWQQ_MS_G_LEAVE = LWQQ_MT_SYS_G_MSG|(3<<8),
    LWQQ_MS_G_REQUIRE = LWQQ_MT_SYS_G_MSG|(4<<8),

    LWQQ_MT_OFFFILE = LWQQ_MF_SEQ|(7<<3),
    LWQQ_MT_FILETRANS = LWQQ_MF_SEQ|(8<<3),
    LWQQ_MT_FILE_MSG = LWQQ_MF_SEQ|(9<<3),
    LWQQ_MT_NOTIFY_OFFFILE = LWQQ_MF_SEQ|(10<<3),
    LWQQ_MT_INPUT_NOTIFY = 11<<3,
    LWQQ_MT_SHAKE_MESSAGE = LWQQ_MF_SEQ|(12<<3),
    LWQQ_MT_UNKNOWN,
} LwqqMsgType;
#define lwqq_mt_bits(t) (t&~(-1<<8))

typedef struct LwqqMsg {
    LwqqMsgType type;
} LwqqMsg;

typedef struct LwqqMsgSeq {
    LwqqMsg super;
    char* from;
    char* to;
    int msg_id;
    int msg_id2;
} LwqqMsgSeq;


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
            char* direct_url;
        }cface;
    } data;
    TAILQ_ENTRY(LwqqMsgContent) entries;
}LwqqMsgContent;

typedef struct LwqqMsgMessage {
    LwqqMsgSeq super;
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
    char f_color[7];

    TAILQ_HEAD(LwqqMsgContentHead, LwqqMsgContent) content;
} LwqqMsgMessage;

typedef struct LwqqMsgStatusChange {
    LwqqMsg super;
    char *who;
    char *status;
    int client_type;
} LwqqMsgStatusChange;

typedef struct LwqqMsgKickMessage {
    LwqqMsg super;
    int show_reason;
    char *reason;
    char *way;
} LwqqMsgKickMessage;
typedef struct LwqqMsgSystem{
    LwqqMsgSeq super;
    char* seq;
    enum {
        VERIFY_REQUIRED,
        VERIFY_PASS_ADD,
        VERIFY_PASS,
        ADDED_BUDDY_SIG,
        SYSTEM_TYPE_UNKNOW
    }type;
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
    LwqqMsgSeq super;
    enum {
        GROUP_CREATE,
        GROUP_JOIN,                 //<admin or member(when admin operate) get this msg
        GROUP_LEAVE,                //<admin or member(when admin operate) get this msg
        GROUP_REQUEST_JOIN,         //<only admin get this msg
        GROUP_REQUEST_JOIN_AGREE,   //<only member get this msg
        GROUP_REQUEST_JOIN_DENY,    //<only member get this msg
        GROUP_UNKNOW,
    }type;
    char* group_uin;
    char* gcode;
    char* account;
    char* member_uin;
    char* member;
    char* admin_uin;
    char* admin;
    char* msg;
    int is_myself;                  //<true when group should add or delete
    LwqqGroup* group;               //<read group info from this.
}LwqqMsgSysGMsg;
typedef struct LwqqMsgBlistChange{
    LwqqMsg super;
    LIST_HEAD(,LwqqSimpleBuddy) added_friends;
    LIST_HEAD(,LwqqBuddy) removed_friends;
} LwqqMsgBlistChange;
typedef struct LwqqMsgOffFile{
    LwqqMsgSeq super;
    char* rkey;
    char ip[24];
    char port[8];
    size_t size;
    char* name;
    char* path;///< only used when upload
    time_t expire_time;
    time_t time;
    LwqqHttpRequest* req;
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
    LwqqMsgSeq super;
    int file_count;
    char* lc_id;
    size_t now;
    int operation;
    int type;
    LIST_HEAD(,FileTransItem) file_infos;
}LwqqMsgFileTrans;
typedef struct LwqqMsgFileMessage{
    LwqqMsgSeq super;
    int msg_id;
    enum {
        MODE_RECV,
        MODE_REFUSE,
        MODE_SEND_ACK
    } mode;
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
    LwqqHttpRequest* req;
}LwqqMsgFileMessage;

typedef struct LwqqMsgNotifyOfffile{
    LwqqMsgSeq super;
    enum {
        NOTIFY_OFFFILE_REFUSE = 2,
    }action;
    char* filename;
    size_t filesize;
}LwqqMsgNotifyOfffile;

typedef struct LwqqMsgInputNotify{
    LwqqMsg super;
    char* from;
    char* to;
    int msg_type;
}LwqqMsgInputNotify;
typedef struct LwqqMsgShakeMessage{
    LwqqMsgSeq super;
    unsigned long reply_ip;
}LwqqMsgShakeMessage;


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
typedef enum {
    POLL_AUTO_REQUEST_PIC = 1<<1,
    POLL_AUTO_REQUEST_CFACE = 1<<2,
    POLL_REMOVE_DUPLICATED_MSG = 1<<3,
}LwqqPollFlag;
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
    void *lc;                   /**< Lwqq Client reference */
    pthread_mutex_t mutex;
    //int poll_flags;
    TAILQ_HEAD(RecvMsgListHead, LwqqRecvMsg) head;
    void (*poll_msg)(struct LwqqRecvMsgList *list,int flags); /**< Poll to fetch msg */
    void (*poll_close)(struct LwqqRecvMsgList* list);/**< Close Poll */
} LwqqRecvMsgList;
typedef struct LwqqHistoryMsgList {
    int row;
    union{
    short page;
    short begin;
    };
    union{
    short total;
    short end;
    };
    long reserve;       //<can store other data or a pointer.
    TAILQ_HEAD(,LwqqRecvMsg) msg_list;
} LwqqHistoryMsgList;

/**
 * Create a new LwqqRecvMsgList object
 *
 * @param client Lwqq Client reference
 *
 * @return NULL on failure
 */
LwqqRecvMsgList *lwqq_recvmsg_new(void *client);
LwqqHistoryMsgList *lwqq_historymsg_list();

/**
 * Free a LwqqRecvMsgList object
 *
 * @param list
 */
void lwqq_recvmsg_free(LwqqRecvMsgList *list);
void lwqq_historymsg_free(LwqqHistoryMsgList* list);

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
LwqqAsyncEvent* lwqq_msg_send(LwqqClient *lc, LwqqMsgMessage *msg);

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
//
//-----------------------------LWQQ MSG UPLOAD API---------------------------------///
//helper function : fill a msg content with upload cface
//then add it to LwqqMsgMessage::content
LwqqMsgContent* lwqq_msg_fill_upload_cface(const char* filename,
        const void* buffer,size_t buf_size);
//helper function : fill a msg content with upload offline pic
//then add it to LwqqMsgMessage::content
LwqqMsgContent* lwqq_msg_fill_upload_offline_pic(const char* filename,
        const void* buffer,size_t buf_size);

//get file url from a receieve offline msg 
//then use this url to download file whatever you like
const char* lwqq_msg_offfile_get_url(LwqqMsgOffFile* msg);
//helper function : fill a message content with upload offline file
//then use upload_offline_file function to do upload
LwqqMsgOffFile* lwqq_msg_fill_upload_offline_file(const char* filename,
        const char* from,const char* to);
//do upload 
//you should always check server return to see it upload successful.
//if successed use send_offfile function do send the message.
//or use offfile_free to clean memory.
typedef enum {
    DONT_EXPECTED_100_CONTINUE = 1<<1
}LwqqUploadFlag;
LwqqAsyncEvent* lwqq_msg_upload_offline_file(LwqqClient* lc,LwqqMsgOffFile* file,int flags);
//call this function when upload_offline_file finished.
LwqqAsyncEvent* lwqq_msg_send_offfile(LwqqClient* lc,LwqqMsgOffFile* file);
//not finished yet
LwqqAsyncEvent* lwqq_msg_upload_file(LwqqClient* lc,LwqqMsgOffFile* file,
        LWQQ_PROGRESS progress,void* prog_data);
///================================================================================///

//----------------------------LWQQ MSG DOWNLOAD API-------------------------------///
/** use this when you recvd a file message .
 * @param file : first use lwqq_msg_new(LWQQ_MT_FILE_MESSAGE) to create a empty message.
 *               then use lwqq_move_msg to move original data to it.
 *               finally pass it to here.
 *               you have response to free it after used.
 * @param saveto : the store file path you want to write to.
 */
LwqqAsyncEvent* lwqq_msg_accept_file(LwqqClient* lc,LwqqMsgFileMessage* file,const char* saveto);
/** use this when you recved a file message .
 * @param file : @see lwqq_msg_accept_file{file}
 */
LwqqAsyncEvent* lwqq_msg_refuse_file(LwqqClient* lc,LwqqMsgFileMessage* file);
///===============================================================================///

LwqqAsyncEvent* lwqq_msg_input_notify(LwqqClient* lc,const char* serv_id);
LwqqAsyncEvent* lwqq_msg_shake_window(LwqqClient* lc,const char* serv_id);
#define lwqq_msg_move(to,from) {memcpy(to,from,sizeof(*from));memset(from,0,sizeof(*from));}

LwqqAsyncEvent* lwqq_msg_friend_history(LwqqClient* lc,const char* serv_id,LwqqHistoryMsgList* list);
LwqqAsyncEvent* lwqq_msg_group_history(LwqqClient* lc,LwqqGroup* g,LwqqHistoryMsgList* list);

/************************************************************************/
/*  LwqqSendMsg API */

#endif  /* LWQQ_MSG_H */
