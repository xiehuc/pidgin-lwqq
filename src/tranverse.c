#include "tranverse.h"
#include <imgstore.h>
#include <string.h>
#include <smemory.h>
#include <type.h>


static void tranverse_escape(char * str)
{
    char* ptr;
    while((ptr = strchr(str,'\\'))){
        switch(*(ptr+1)){
            case 'n':
                *ptr = '\n';
                break;
            case 'r':
                *ptr = ' ';
                break;
            default:
                *ptr = *(ptr+1);
                break;
        }
        *(ptr+1) = ' ';
    }
    
}
void tranverse_message_to_struct(LwqqClient* lc,const char* to,const char* what,LwqqMsgMessage* mmsg)
{
    const char* ptr = what;
    const char* img;
    int img_id;
    LwqqMsgContent *c;
    while(*ptr!='\0'){
        img = strstr(ptr,"<IMG");
        if(img==NULL){
            c = s_malloc0(sizeof(*c));
            c->type = LWQQ_CONTENT_STRING;
            c->data.str = s_strdup(ptr);
            LIST_INSERT_HEAD(&mmsg->content,c,entries);
            ptr += strlen(ptr);
        }else{
            //before img there are still text.
            if(img-ptr>0){
                c = s_malloc0(sizeof(*c));
                c->type = LWQQ_CONTENT_STRING;
                c->data.str = s_malloc0(img-ptr+1);
                strncpy(c->data.str,ptr,img-ptr);
                LIST_INSERT_HEAD(&mmsg->content,c,entries);
            }else{
                sscanf(img,"<IMG ID=\"%d\">",&img_id);
                PurpleStoredImage* simg = purple_imgstore_find_by_id(img_id);
                c = lwqq_msg_upload_offline_pic(lc,to,
                        purple_imgstore_get_filename(simg),
                        purple_imgstore_get_data(simg),
                        purple_imgstore_get_size(simg));
                puts(purple_imgstore_get_filename(simg));
                LIST_INSERT_HEAD(&mmsg->content,c,entries);
            }
            ptr = strchr(img,'>')+1;
        }
    }
}
