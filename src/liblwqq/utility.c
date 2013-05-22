#include "utility.h"
#include "smemory.h"
#include <string.h>

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
