#ifndef LWQQ_UTIL_H_H
#define LWQQ_UTIL_H_H
#include "type.h"

typedef enum {
    LWQQ_CT_ENABLE_IGNORE = 1<<1,
    LWQQ_CT_CHOICE_MODE = 1<<2
}LwqqCTFlags;

typedef struct LwqqConfirmTable {
    LwqqAnswer answer;
    int flags;
    char* title;            //< read
    char* body;             //< read
    char* exans_label;      //< extra answer label, read
    char* input_label;      //< read
    char* yes_label;
    char* no_label;
    char* input;            //< write
    LwqqCommand cmd;
}LwqqConfirmTable;

typedef struct LwqqString{
    char* str;
} LwqqString;

void lwqq_ct_free(LwqqConfirmTable* table);

#define lwqq_group_pretty_name(g) (g->markname?:g->name)
#define lwqq_buddy_pretty_name(b) (b->markname?:b->nick)
    
#endif
