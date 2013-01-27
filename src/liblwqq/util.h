#ifndef LWQQ_UTIL_H_H
#define LWQQ_UTIL_H_H
#include "type.h"

typedef enum {
    LWQQ_CT_YES_NEED_INPUT = 1<<1,
    LWQQ_CT_NO_NEED_INPUT = 1<<2
}LwqqCTFlags;

typedef struct LwqqConfirmTable {
    LwqqAnswer answer;
    int flags;
    char* title;            //< read
    char* body;             //< read
    char* input_title;      //< read
    char* input;            //< write
    LwqqCommand cmd;
}LwqqConfirmTable;

typedef struct LwqqString{
    char* str;
} LwqqString;

void lwqq_ct_free(LwqqConfirmTable* table);
    
#endif
