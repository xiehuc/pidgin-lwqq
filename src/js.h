#ifndef QQ_JS_H_H
#define QQ_JS_H_H
void qq_js_init();
void qq_js_close();


void qq_js_load(const char* file);
char* qq_js_hash(const char* uin,const char* ptwebqq);
#endif
