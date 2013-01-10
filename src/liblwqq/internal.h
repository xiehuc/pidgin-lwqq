#ifndef LWQQ_INTERNAL_H_H
#define LWQQ_INTERNAL_H_H

struct LwqqStrMapEntry_ {
    const char* str;
    int type;
};


int lwqq_map_to_type_(const struct LwqqStrMapEntry_* maps,const char* key);
const char* lwqq_map_to_str_(const struct LwqqStrMapEntry_* maps,int type);

#endif

