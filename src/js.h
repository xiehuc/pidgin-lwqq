#ifndef QQ_JS_H_H
#define QQ_JS_H_H
#include "config.h"

typedef struct qq_js_t qq_js_t;
typedef void   qq_jso_t;

qq_js_t* qq_js_init();
void qq_js_close(qq_js_t* js);
#ifdef WITH_MOZJS
qq_jso_t* qq_js_load(qq_js_t* js,const char* file);
void qq_js_unload(qq_js_t* js,qq_jso_t* obj);
#endif


char* qq_js_hash(const char* uin,const char* ptwebqq,qq_js_t* js);
#endif
