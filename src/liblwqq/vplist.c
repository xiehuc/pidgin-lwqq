#include "vplist.h"
#include <string.h>

struct vp_d_table{
    const char* id;
    VP_DISPATCH d;
};
vp_command vp_make_command(VP_DISPATCH dsph,VP_CALLBACK func,...)
{
    vp_command ret;
    ret.dsph = dsph;
    ret.func = func;
    ret.next = NULL;
    va_list args;
    va_start(args,func);
    dsph(NULL,&ret.data,&args);
    va_end(args);
    return ret;
}
void vp_do(vp_command cmd,void* retval)
{
    if(cmd.dsph==NULL||cmd.func==NULL) return;
    vp_start(cmd.data);
    cmd.dsph(cmd.func,&cmd.data,retval);
    vp_end(cmd.data);
    cmd.dsph = (VP_DISPATCH)NULL;
    cmd.func = (VP_CALLBACK)NULL;
    vp_command* n = cmd.next;
    cmd.next = NULL;
    vp_command* p;
    while(n){
        vp_start(n->data);
        n->dsph(n->func,&n->data,NULL);
        vp_end(n->data);
        p = n;
        n = n->next;
        free(p);
    }
}
void vp_link(vp_command* head,vp_command* elem)
{
    vp_command* cmd = head;
    while(cmd->next)
        cmd = cmd->next;
    vp_command* item = malloc(sizeof(vp_command));
    memcpy(item,elem,sizeof(vp_command));
    memset(elem,sizeof(vp_command),0);
    cmd->next = item;
}

void vp_func_void(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef void (*f)(void);
    if( !func ){
        vp_init(*vp,0);
        return ;
    }
    ((f)func)();
}
void vp_func_p(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef void (*f)(void*);
    if( !func ){
        va_list* va = q;
        vp_init(*vp,sizeof(void*));
        vp_push(*vp,*va,void*);
        return ;
    }
    void* p1 = vp_arg(*vp,void*);
    ((f)func)(p1);
}

void vp_func_2p(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef void (*f)(void*,void*);
    if( !func ){
        va_list va;
        va_copy(va,*(va_list*)q);
        vp_init(*vp,sizeof(void*)*2);
        vp_push(*vp,va,void*);
        vp_push(*vp,va,void*);
        va_end(va);
        return ;
    }
    void* p1 = vp_arg(*vp,void*);
    void* p2 = vp_arg(*vp,void*);
    ((f)func)(p1,p2);
}
void vp_func_3p(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef void (*f)(void*,void*,void*);
    if( !func ){
        va_list* va = q;
        vp_init(*vp,sizeof(void*)*3);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        return ;
    }
    void* p1 = vp_arg(*vp,void*);
    void* p2 = vp_arg(*vp,void*);
    void* p3 = vp_arg(*vp,void*);
    ((f)func)(p1,p2,p3);
}
void vp_func_4p(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef void (*f)(void*,void*,void*,void*);
    if( !func ){
        va_list* va = q;
        vp_init(*vp,sizeof(void*)*4);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        return ;
    }
    void* p1 = vp_arg(*vp,void*);
    void* p2 = vp_arg(*vp,void*);
    void* p3 = vp_arg(*vp,void*);
    void* p4 = vp_arg(*vp,void*);
    ((f)func)(p1,p2,p3,p4);
}
void vp_func_pi(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef void (*f)(void*,int);
    if( !func ){
        va_list* va = q;
        vp_init(*vp,sizeof(void*)+sizeof(int));
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,int);
        return;
    }
    void* p1 = vp_arg(*vp,void*);
    int p2 = vp_arg(*vp,int);
    ((f)func)(p1,p2);
}
void vp_func_p_i(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef int (*f)(void*);
    if( !func ){
        va_list* va = q;
        vp_init(*vp,sizeof(void*)*2);
        vp_push(*vp,*va,void*);
        return;
    }
    void* p1 = vp_arg(*vp,void*);
    int ret = ((f)func)(p1);
    if(q) *(int*)q = ret;
}
void vp_func_2p_i(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef int (*f)(void*,void*);
    if( !func ){
        va_list* va = q;
        vp_init(*vp,sizeof(void*)*2);
        vp_push(*vp,*va,void*);
        vp_push(*vp,*va,void*);
        return;
    }
    void* p1 = vp_arg(*vp,void*);
    void* p2 = vp_arg(*vp,void*);
    int ret = ((f)func)(p1,p2);
    if(q) *(int*)q = ret;
}
void vp_func_3p_i(VP_CALLBACK func,vp_list* vp,void* q)
{
    typedef int (*f)(void*,void*,void*);
    if( !func ){
        va_list va;
        va_copy(va,*(va_list*)q);
        vp_init(*vp,sizeof(void*)*3);
        vp_push(*vp,va,void*);
        vp_push(*vp,va,void*);
        vp_push(*vp,va,void*);
        va_end(va);
        return;
    }
    void* p1 = vp_arg(*vp,void*);
    void* p2 = vp_arg(*vp,void*);
    void* p3 = vp_arg(*vp,void*);
    int ret = ((f)func)(p1,p2,p3);
    if(q) *(int*)q = ret;
}
static struct vp_d_table tables[]= {
    {"",vp_func_void},
    {"p",vp_func_p},
    {"pp",vp_func_2p},
    {"ppp",vp_func_3p},
    {"pppp",vp_func_4p},
    {"pi",vp_func_pi},
    {"i:pp",vp_func_2p_i},
};
