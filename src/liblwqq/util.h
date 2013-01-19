#ifndef LWQQ_UTIL_H_H
#define LWQQ_UTIL_H_H
#include "type.h"

typedef struct LwqqConfirmTable {
    LwqqAnswer answer;
    char* title;
    char* body;
    LwqqCommand cmd;
}LwqqConfirmTable;

void lwqq_ct_free(LwqqConfirmTable* table);
    
#endif
