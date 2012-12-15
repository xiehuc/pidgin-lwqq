#include <stdarg.h>
#include <stdlib.h>
/**
 * # variable param list
 *
 * inspired from va_list 
 * runs on heap so can passed to 
 * another thread
 *
 * author : xiehuc@gmail.com
 */


typedef struct {void* st;void* cur; size_t sz;}vp_list;
#define vp_init(vp,size) do{(vp).st = (vp).cur = malloc(size);(vp).sz = size;}while(0)
#define _ptr_self_inc(ptr,sz) ((ptr+=sz)-sz)
#define vp_push(vp,va,type) do{type t = va_arg((va),type);memcpy(_ptr_self_inc((vp).cur,sizeof(type)),&t,sizeof(type));}while(0)
#define vp_start(vp) (vp).cur = (vp).st
#define vp_arg(vp,type) *(type*)(_ptr_self_inc((vp).cur,sizeof(type)))
#define vp_end(vp) free((vp).st)

typedef void (*VP_CALLBACK)(void);
typedef void (*VP_DISPATCH)(VP_CALLBACK,vp_list*,va_list*);

/**
 * p : pointer
 * i : int
 */
void vp_func_void(VP_CALLBACK,vp_list*,va_list*);
void vp_func_1p(VP_CALLBACK,vp_list*,va_list*);
void vp_func_2p(VP_CALLBACK,vp_list*,va_list*);
void vp_func_1p1i(VP_CALLBACK,vp_list*,va_list*);
