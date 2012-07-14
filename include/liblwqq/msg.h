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

/**
 * define the strings used for poll_type.
 */
#define MT_MESSAGE "message"
#define MT_GROUP_MESSAGE "group_message"
#define MT_STATUS_CHANGE  "buddies_status_change"

/**
 * Every message will contain these elems.
 */
typedef struct LwqqMsgAny {
    char * msg_type; /* must not be changed */
} LwqqMsgAny;

/**
 * Message object, receiving and sending chat message
 * 
 */
typedef struct LwqqMsgMessage {
    char * msg_type;            /**< must not be changed */

    char *from;                 /**< Message sender(qqnumber) */
    char *to;                   /**< Message receiver(qqnumber) */
    
    char *content;              /**< Message content */

} LwqqMsgMessage;

/**
 * buddies status change message body.
 */
typedef struct LwqqMsgStatus {
    char * msg_type; /* must not be changed */

    char * who;
    char * status;
} LwqqMsgStatus;

/**
 * Lwqq Message object. different message type base on msg_type.
 * It can be convert to LwqqMsgMessage or LwqqMsgStatus because
 * they have the same member "char * msg_typ"". Please do NOT change
 * the first member in these struct.
 * 
 */
typedef union LwqqMsg {
    /* Message type. e.g. buddy message or group message */
    char *msg_type;
    LwqqMsgAny any;
    LwqqMsgMessage message;
    LwqqMsgStatus status;
} LwqqMsg;

/** 
 * Create a new LwqqMsg object
 * 
 * @param from 
 * @param to 
 * @param msg_type 
 * @param content 
 * 
 * @return NULL on failure
 */
LwqqMsg *lwqq_msg_new(const char *msg_type, ...);

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
    void (*close_msg)(struct LwqqRecvMsgList *list);
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
typedef struct LwqqSendMsg {
    void *lc;                   /**< Suck Code now */
    LwqqMsg *msg;
    int (*send)(struct LwqqSendMsg *msg);
} LwqqSendMsg;

/** 
 * Create a new LwqqSendMsg object
 * 
 * @param client 
 * @param to
 * @param msg_type 
 * @param content 
 * 
 * @return 
 */
LwqqSendMsg *lwqq_sendmsg_new(void *client, const char *to,
                              const char *msg_type, const char *content);

/** 
 * Free a LwqqSendMsg object
 * 
 * @param msg 
 */
void lwqq_sendmsg_free(LwqqSendMsg *msg);

/*  LwqqSendMsg API End */

#endif  /* LWQQ_MSG_H */
