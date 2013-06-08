#include "utility.h"
#include "smemory.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

void lwqq_ct_free(LwqqConfirmTable* table)
{
    if(table){
        s_free(table->title);
        s_free(table->body);
        s_free(table->input_label);
        s_free(table->yes_label);
        s_free(table->no_label);
        s_free(table->exans_label);
        s_free(table->input);
        s_free(table);
    }
}


LwqqOpCode lwqq_util_save_img(void* ptr,size_t len,char* path,char* dir)
{
    if(!ptr||!path) return LWQQ_OP_FAILED;
    char fullpath[1024];
    snprintf(fullpath,sizeof(fullpath),"%s%s%s",dir?:"",dir?"/":"",path);
    FILE* f = fopen(fullpath,"wb");
    if(f==NULL&&errno==2){
        if(dir){
            mkdir(dir,0755);
            f = fopen(fullpath,"wb");
        }else return LWQQ_OP_FAILED;
    }
    if(f==NULL) return LWQQ_OP_FAILED;

    fwrite(ptr,len,1,f);
    fclose(f);
    return LWQQ_OP_OK;
}


void ds_cat_(struct ds* str,...)
{
    va_list args;
    const char* cat;
    va_start(args,str);
    while((cat = va_arg(args,const char*))!=0){
        ds_pokes(*str,cat);
    }
    va_end(args);
    ds_poke(*str,'\0');
}


const char* ds_itos(int n)
{
    static char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d",n);
    return buffer;
}
