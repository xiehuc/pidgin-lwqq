#include "internal.h"
#include <string.h>


int lwqq_map_to_type_(const struct LwqqStrMapEntry_* maps,const char* key)
{
    while(maps->str != NULL){
        if(!strncmp(maps->str,key,strlen(maps->str))) return maps->type;
        maps++;
    }
    return maps->type;
}

const char* lwqq_map_to_str_(const struct LwqqStrMapEntry_* maps,int type)
{
    while(maps->str != NULL){
        if(maps->type == type) return maps->str;
        maps++;
    }
    return NULL;
}

