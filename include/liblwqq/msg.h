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

#define LWQQ_CONTENT_STRING 0
#define LWQQ_CONTENT_FACE 1

typedef struct LwqqMsgContent {
    int type;
    union {
        int face;
        char *str;
    } data;
    LIST_ENTRY(LwqqMsgContent) entries;
} LwqqMsgContent ;

typedef struct LwqqMsgMessage {
    char *from;
    char *to;
    char *send; /* only group use it to identify who send the group message */
    time_t time;

    /* For font  */
    char *f_name;
    int f_size;
    struct {
        int a, b, c; /* bold , italic , underline */
    } f_style;
    char *f_color;

    LIST_HEAD(, LwqqMsgContent) content;
} LwqqMsgMessage;

typedef struct LwqqMsgStatusChange {
    char *who;
    char *status;
    int client_type;
} LwqqMsgStatusChange;

typedef enum LwqqMsgType {
    LWQQ_MT_BUDDY_MSG = 0,
    LWQQ_MT_GROUP_MSG,
    LWQQ_MT_STATUS_CHANGE,
    LWQQ_MT_UNKNOWN,
} LwqqMsgType;

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
    SIMPLEQ_ENTRY(LwqqRecvMsg) entries;
} LwqqRecvMsg;

typedef struct LwqqRecvMsgList {
    int count;                  /**< Number of message  */
    pthread_mutex_t mutex;
    SIMPLEQ_HEAD(, LwqqRecvMsg) head;
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

/* LwqqRecvMsg API end */

/************************************************************************/
/*  LwqqSendMsg API */

#endif  /* LWQQ_MSG_H */
