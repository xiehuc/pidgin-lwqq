#ifndef REMOTE_H_H
#define REMOTE_H_H
#include "qq_types.h"

void qq_remote_init();
void qq_remote_call(qq_account* ac,const char* url);
void qq_remove_destroy();
#endif
