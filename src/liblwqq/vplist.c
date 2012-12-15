#include "vplist.h"
#include <string.h>


void vp_func_void(VP_CALLBACK func,vp_list* vp,va_list* va)
{
    typedef void (*f)(void);
    if( va ){
        vp_init(*vp,0);
        return ;
    }
    ((f)func)();
}
void vp_func_1p(VP_CALLBACK func,vp_list* vp,va_list* va)
{
    typedef void (*f)(void*);
    if( va ){
        vp_init(*vp,sizeof(void*));
        vp_push(*vp,*va,void*);
        return ;
    }
    void* p1 = vp_arg(*vp,void*);
    ((f)func)(p1);
}

void vp_func_2p(VP_CALLBACK func,vp_list* vp,va_list* va)
{
    typedef void (*f)(void*,void*);
    if( va ){
        vp_init(*vp,sizeof(void*)*2);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        return ;
    }
    void* p1 = vp_arg(*vp,void*);
    void* p2 = vp_arg(*vp,void*);
    ((f)func)(p1,p2);
}
void vp_func_1p1i(VP_CALLBACK func,vp_list* vp,va_list* va)
{
    typedef void (*f)(void*,int);
    if( va ){
        vp_init(*vp,sizeof(void*)+sizeof(int));
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,int);
        return;
    }
    void* p1 = vp_arg(*vp,void*);
    int p2 = vp_arg(*vp,int);
    ((f)func)(p1,p2);
}
