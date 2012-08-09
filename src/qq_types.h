#ifndef QQ_TYPES_H_H
#define QQ_TYPES_H_H
typedef struct _AsyncListener AsyncListener;

#include <type.h>
#include <connection.h>
#define QQ_MAGIC 0x4153
typedef struct qq_account {
    LwqqClient* qq;
    PurpleAccount* account;
    PurpleConnection* gc;
    int disable_send_server;///< this ensure not send buddy category change etc event to server
    int magic;//0x4153
} qq_account;
void qq_msg_check(qq_account* ac);
void qq_set_basic_info(int result,void* data);
#endif
