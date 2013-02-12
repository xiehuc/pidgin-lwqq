#ifndef LWQQ_UTIL_H_H
#define LWQQ_UTIL_H_H
#include "type.h"


typedef struct LwqqConfirmTable {
    LwqqAnswer answer;
    int flags;
    char* title;            //< read
    char* body;             //< read
    char* exans_label;      //< extra answer label, read
    char* input_label;      //< read
    char* input;            //< write
    LwqqCommand cmd;
}LwqqConfirmTable;

typedef struct LwqqString{
    char* str;
} LwqqString;

void lwqq_ct_free(LwqqConfirmTable* table);
    
#endif
