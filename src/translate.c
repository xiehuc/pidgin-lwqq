#include <imgstore.h>
#include <string.h>
#include <smemory.h>
#include <type.h>
#include <assert.h>
#include <smiley.h>
#include <accountopt.h>
//#include <gtk/gtk.h>
//#include <gtksmiley.h>
#include "translate.h"
#include "trex.h"
#include "async.h"
#include "qq_types.h"

#ifndef INST_PREFIX
#define INST_PREFIX "/usr"
#endif

#define FACE_DIR INST_PREFIX"/share/pixmaps/pidgin/emotes/webqq/"
static GHashTable* smily_table;
static TRex* _regex;
static TRex* hs_regex;
const char* SMILY_EXP = "<IMG ID=\"\\d+\">|\\[FACE_\\d+\\]|"
    ":\\)|:-D|:-\\(|;-\\)|:P|=-O|:-\\*|8-\\)|:-\\[|"
    ":'\\(|:-/|O:-\\)|:-x|:-\\$|:-!";
    //:'( error
const char* HTML_SYMBOL = "<[^>]+>|&amp;|&quot;|&gt;|&lt;";
//font size map space : pidgin:[1,7] to webqq[8:20]
#define sizemap(num) (6+2*num)
//font size map space : webqq[8:22] to pidgin [1:8]
#define sizeunmap(px) ((px-6)/2)
static LwqqMsgContent* build_string_content(const char* from,const char* to,LwqqMsgMessage* msg)
{
    LwqqMsgContent* c;
    c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_STRING;
    if(to){
        c->data.str = s_malloc0(to-from+1);
        strncpy(c->data.str,from,to-from);
    }else{
        c->data.str = s_strdup(from);
    }
    const char *begin,*end;
    const char *read = c->data.str;
    char *write = c->data.str;
    const char *ptr;
    while(*read!='\0'){
        if(!trex_search(hs_regex,read,&begin,&end)){
            ptr = strchr(read,'\0');
            ptr++;
            memmove(write,read,ptr-read);
            write+=ptr-read;
            break;
        }
        if(begin>read){
            memmove(write,read,begin-read);
            write+=(begin-read);
        }
        if(strncmp(begin,"<br>",strlen("<br>"))==0){
            strcpy(write,"\n");
            write++;
        }else if(strncmp(begin,"&amp;",strlen("&amp;"))==0){
            strcpy(write,"&");
            write++;
        }else if(strncmp(begin,"&quot;",strlen("&quot;"))==0){
            strcpy(write,"\"");
            write++;
        }else if(strncmp(begin,"&lt;",strlen("&lt;"))==0){
            strcpy(write,"<");
            write++;
        }else if(strncmp(begin,"&gt;",strlen("&gt;"))==0){
            strcpy(write,">");
            write++;
        }else if(begin[0]=='<'){
            if(begin[1]!='/'){
                if(begin[1]=='b')msg->f_style.b = 1;
                else if(begin[1]=='i')msg->f_style.i = 1;
                else if(begin[1]=='u')msg->f_style.u = 1;
                else if(strncmp(begin+1,"font",4)==0){
                    const char *key = begin+6;
                    const char *value = strchr(begin,'=')+2;
                    const char *end = strchr(value,'"');
                    if(strncmp(key,"size",4)==0){
                        int size = atoi(value);
                        msg->f_size = sizemap(size);
                    }else if(strncmp(key,"color",5)==0){
                        strncpy(msg->f_color,value+1,6);
                        msg->f_color[6] = '\0';
                    }else if(strncmp(key,"face",4)==0){
                        s_free(msg->f_name);
                        msg->f_name = s_malloc0(end-value+1);
                        strncpy(msg->f_name,value,end-value);
                        msg->f_name[end-value]='\0';
                    }
                }
            }
        }
        read = end;
    }
    *write = '\0';
    return c;
}
static LwqqMsgContent* build_face_content(const char* face,int len)
{
    static char buf[20];
    memcpy(buf,face,len);
    buf[len]= '\0';
    LwqqMsgContent* c;
    if(smily_table==NULL) translate_global_init();
    int num = (long)g_hash_table_lookup(smily_table,buf);
    if(num==0) return NULL;
    c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_FACE;
    if(num==-1)num++;
    c->data.face = num;
    return c;
}
static LwqqMsgContent* build_face_direct(int num)
{
    LwqqMsgContent* c;
    c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_FACE;
    c->data.face = num;
    return c;
}
static void img_unref(LwqqAsyncEvent* event,void* data)
{
    PurpleStoredImage* img = data;
    purple_imgstore_unref(img);
}
int translate_message_to_struct(LwqqClient* lc,const char* to,const char* what,LwqqMsg* msg,int using_cface)
{
    const char* ptr = what;
    int img_id;
    LwqqMsgContent *c;
    puts(what);
    const char* begin,*end;
    TRexMatch m;
    if(_regex==NULL) translate_global_init();
    TRex* x = _regex;
    LwqqMsgMessage* mmsg = msg->opaque;

    LwqqAsyncEvset* set = lwqq_async_evset_new();
    LwqqAsyncEvent* event;
     
    while(*ptr!='\0'){
        c = NULL;
        if(!trex_search(x,ptr,&begin,&end)){
            ///last part.
            c = build_string_content(ptr,NULL,mmsg);
            TAILQ_INSERT_TAIL(&mmsg->content,c,entries);
            break;
        }
        if(begin>ptr){
            //note you can not pass c directly. 
            //because insert_tail is a macro.
            c = build_string_content(ptr,begin,mmsg);
            TAILQ_INSERT_TAIL(&mmsg->content,c,entries);
        }
        trex_getsubexp(x,0,&m);
        puts(m.begin);
        if(strstr(begin,"<IMG")==begin){
            //process ing img.
            sscanf(begin,"<IMG ID=\"%d\">",&img_id);
            PurpleStoredImage* simg = purple_imgstore_find_by_id(img_id);
            c = s_malloc0(sizeof(*c));
            if(using_cface||msg->type == LWQQ_MT_GROUP_MSG){
                c->type = LWQQ_CONTENT_CFACE;
                c->data.cface.name = s_strdup(purple_imgstore_get_filename(simg));
                c->data.cface.data = (char*)purple_imgstore_get_data(simg);
                c->data.cface.size = purple_imgstore_get_size(simg);
                event = lwqq_msg_upload_cface(lc,msg->type,c);
            }else{
                c->type = LWQQ_CONTENT_OFFPIC;
                c->data.img.name = s_strdup(purple_imgstore_get_filename(simg));
                c->data.img.data = (char*)purple_imgstore_get_data(simg);
                c->data.img.size = purple_imgstore_get_size(simg);
                event = lwqq_msg_upload_offline_pic(lc,to,c);
            }
            purple_imgstore_ref(simg);
            lwqq_async_add_event_listener(event,img_unref,simg);
            lwqq_async_evset_add_event(set,event);
        }else if(strstr(begin,"[FACE")==begin){
            //processing face
            sscanf(begin,"[FACE_%d]",&img_id);
            c = build_face_direct(img_id);
        }else if(begin[0]=='&'){
        }else{
            //other face
            c = build_face_content(m.begin,m.len);
        }
        ptr = end;
        if(c!=NULL)
            TAILQ_INSERT_TAIL(&mmsg->content,c,entries);
    }
    return lwqq_async_wait(set);
}
static void paste_content_string(const char* from,char* to)
{
    const char* read = from;
    char* write = to;
    size_t idx;
    while(*read!='\0'){
        idx = strcspn(read,"<>");
        if(read[idx]=='\0'){
            strcpy(write,read);
            write+=idx;
            break;
        }
        strncpy(write,read,idx);
        write+=idx;
        switch(read[idx]){
            case '<' : strcpy(write,"&lt;");break;
            case '>' : strcpy(write,"&gt;");break;
            case '&' : strcpy(write,"&amp;");break;
            case '"' : strcpy(write,"&quote;");break;
        }
        read+=idx+1;
        write+=strlen(write);
    }
    *write='\0';
}
void translate_struct_to_message(qq_account* ac, LwqqMsgMessage* msg, char* buf)
{
    LwqqMsgContent* c;
    char piece[24] = {0};
    if(msg->f_style.b==1) strcat(buf,"<b>");
    if(msg->f_style.i==1) strcat(buf,"<i>");
    if(msg->f_style.u==1) strcat(buf,"<u>");
    snprintf(buf+strlen(buf),300,"<font color=\"#%s\" ",msg->f_color);
    if(!ac->disable_custom_font_face)
        snprintf(buf+strlen(buf),300,"face=\"%s\" ",msg->f_name);
    if(!ac->disable_custom_font_size)
        snprintf(buf+strlen(buf),300,"size=\"%d\" ",sizeunmap(msg->f_size));
    strcat(buf,">");
    
    TAILQ_FOREACH(c, &msg->content, entries) {
        switch(c->type){
            case LWQQ_CONTENT_STRING:
                paste_content_string(c->data.str,buf+strlen(buf));
                break;
            case LWQQ_CONTENT_FACE:
                strcat(buf,translate_smile(c->data.face));
                break;
            case LWQQ_CONTENT_OFFPIC:
                if(c->data.img.size>0){
                    int img_id = purple_imgstore_add_with_id(c->data.img.data,c->data.img.size,NULL);
                    //"<IMG ID=\"1\">
                    snprintf(piece,sizeof(piece),"<IMG ID=\"%d\">",img_id);
                    strcat(buf,piece);
                }else{
                    strcat(buf,"【图片】");
                }
                break;
            case LWQQ_CONTENT_CFACE:
                if(c->data.cface.size>0){
                    int img_id = purple_imgstore_add_with_id(c->data.cface.data,c->data.cface.size,NULL);
                    snprintf(piece,sizeof(piece),"<IMG ID=\"%d\">",img_id);
                    strcat(buf,piece);
                }else
                    strcat(buf,"【图片】");
                break;
        }
    }
    strcat(buf,"</font>");
    if(msg->f_style.u==1) strcat(buf,"</u>");
    if(msg->f_style.i==1) strcat(buf,"</i>");
    if(msg->f_style.b==1) strcat(buf,"</b>");
}
void translate_global_init()
{
    if(_regex==NULL){
        const char* err = NULL;
        _regex = trex_compile(_TREXC(SMILY_EXP),&err);
        if(err) {puts(err);assert(0);}
        assert(_regex!=NULL);
    }
    if(hs_regex==NULL){
        const char* err = NULL;
        hs_regex = trex_compile(_TREXC(HTML_SYMBOL),&err);
        if(err){puts(err);assert(0);}
        assert(_regex!=NULL);
    }
    if(smily_table ==NULL){
#define GPOINTER(n) (gpointer)n
        GHashTable *t = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,NULL);
        g_hash_table_insert(t,":)",GPOINTER(14));
        g_hash_table_insert(t,":-D",GPOINTER(13));
        g_hash_table_insert(t,":-(",GPOINTER(50));
        g_hash_table_insert(t,":(",GPOINTER(50));
        g_hash_table_insert(t,";-)",GPOINTER(12));
        g_hash_table_insert(t,":P",GPOINTER(12));
        g_hash_table_insert(t,"=-O",GPOINTER(57));
        g_hash_table_insert(t,":-*",GPOINTER(116));
        g_hash_table_insert(t,"8-)",GPOINTER(51));
        g_hash_table_insert(t,":-[",GPOINTER(74));
        g_hash_table_insert(t,":'(",GPOINTER(9));
        g_hash_table_insert(t,":-/",GPOINTER(76));
        g_hash_table_insert(t,"O:-)",GPOINTER(58));
        g_hash_table_insert(t,":-x",GPOINTER(106));
        g_hash_table_insert(t,":-$",GPOINTER(107));
        g_hash_table_insert(t,":-!",GPOINTER(85));
        smily_table = t;
        purple_smiley_new_from_file("[FACE_14]",FACE_DIR"0.gif");
        purple_smiley_new_from_file("[FACE_1]",FACE_DIR"1.gif");
        purple_smiley_new_from_file("[FACE_2]",FACE_DIR"2.gif");
        purple_smiley_new_from_file("[FACE_3]",FACE_DIR"3.gif");
        purple_smiley_new_from_file("[FACE_4]",FACE_DIR"4.gif");
        purple_smiley_new_from_file("[FACE_5]",FACE_DIR"5.gif");
        purple_smiley_new_from_file("[FACE_6]",FACE_DIR"6.gif");
        purple_smiley_new_from_file("[FACE_7]",FACE_DIR"7.gif");
        purple_smiley_new_from_file("[FACE_8]",FACE_DIR"8.gif");
        purple_smiley_new_from_file("[FACE_9]",FACE_DIR"9.gif");
        purple_smiley_new_from_file("[FACE_10]",FACE_DIR"10.gif");
        purple_smiley_new_from_file("[FACE_11]",FACE_DIR"11.gif");
        purple_smiley_new_from_file("[FACE_12]",FACE_DIR"12.gif");
        purple_smiley_new_from_file("[FACE_13]",FACE_DIR"13.gif");
        purple_smiley_new_from_file("[FACE_0]",FACE_DIR"14.gif");
        purple_smiley_new_from_file("[FACE_50]",FACE_DIR"15.gif");
        purple_smiley_new_from_file("[FACE_51]",FACE_DIR"16.gif");
        purple_smiley_new_from_file("[FACE_96]",FACE_DIR"17.gif");
        purple_smiley_new_from_file("[FACE_53]",FACE_DIR"18.gif");
        purple_smiley_new_from_file("[FACE_54]",FACE_DIR"19.gif");
        purple_smiley_new_from_file("[FACE_73]",FACE_DIR"20.gif");
        purple_smiley_new_from_file("[FACE_74]",FACE_DIR"21.gif");
        purple_smiley_new_from_file("[FACE_75]",FACE_DIR"22.gif");
        purple_smiley_new_from_file("[FACE_76]",FACE_DIR"23.gif");
        purple_smiley_new_from_file("[FACE_77]",FACE_DIR"24.gif");
        purple_smiley_new_from_file("[FACE_78]",FACE_DIR"25.gif");
        purple_smiley_new_from_file("[FACE_55]",FACE_DIR"26.gif");
        purple_smiley_new_from_file("[FACE_56]",FACE_DIR"27.gif");
        purple_smiley_new_from_file("[FACE_57]",FACE_DIR"28.gif");
        purple_smiley_new_from_file("[FACE_58]",FACE_DIR"29.gif");
        purple_smiley_new_from_file("[FACE_79]",FACE_DIR"30.gif");
        purple_smiley_new_from_file("[FACE_80]",FACE_DIR"31.gif");
        purple_smiley_new_from_file("[FACE_81]",FACE_DIR"32.gif");
        purple_smiley_new_from_file("[FACE_82]",FACE_DIR"33.gif");
        purple_smiley_new_from_file("[FACE_83]",FACE_DIR"34.gif");
        purple_smiley_new_from_file("[FACE_84]",FACE_DIR"35.gif");
        purple_smiley_new_from_file("[FACE_85]",FACE_DIR"36.gif");
        purple_smiley_new_from_file("[FACE_86]",FACE_DIR"37.gif");
        purple_smiley_new_from_file("[FACE_87]",FACE_DIR"38.gif");
        purple_smiley_new_from_file("[FACE_88]",FACE_DIR"39.gif");
        purple_smiley_new_from_file("[FACE_97]",FACE_DIR"40.gif");
        purple_smiley_new_from_file("[FACE_98]",FACE_DIR"41.gif");
        purple_smiley_new_from_file("[FACE_99]",FACE_DIR"42.gif");
        purple_smiley_new_from_file("[FACE_100]",FACE_DIR"43.gif");
        purple_smiley_new_from_file("[FACE_101]",FACE_DIR"44.gif");
        purple_smiley_new_from_file("[FACE_102]",FACE_DIR"45.gif");
        purple_smiley_new_from_file("[FACE_103]",FACE_DIR"46.gif");
        purple_smiley_new_from_file("[FACE_104]",FACE_DIR"47.gif");
        purple_smiley_new_from_file("[FACE_105]",FACE_DIR"48.gif");
        purple_smiley_new_from_file("[FACE_106]",FACE_DIR"49.gif");
        purple_smiley_new_from_file("[FACE_107]",FACE_DIR"50.gif");
        purple_smiley_new_from_file("[FACE_108]",FACE_DIR"51.gif");
        purple_smiley_new_from_file("[FACE_109]",FACE_DIR"52.gif");
        purple_smiley_new_from_file("[FACE_110]",FACE_DIR"53.gif");
        purple_smiley_new_from_file("[FACE_111]",FACE_DIR"54.gif");
        purple_smiley_new_from_file("[FACE_112]",FACE_DIR"55.gif");
        purple_smiley_new_from_file("[FACE_32]",FACE_DIR"56.gif");
        purple_smiley_new_from_file("[FACE_113]",FACE_DIR"57.gif");
        purple_smiley_new_from_file("[FACE_114]",FACE_DIR"58.gif");
        purple_smiley_new_from_file("[FACE_115]",FACE_DIR"59.gif");
        purple_smiley_new_from_file("[FACE_63]",FACE_DIR"60.gif");
        purple_smiley_new_from_file("[FACE_64]",FACE_DIR"61.gif");
        purple_smiley_new_from_file("[FACE_59]",FACE_DIR"62.gif");
        purple_smiley_new_from_file("[FACE_33]",FACE_DIR"63.gif");
        purple_smiley_new_from_file("[FACE_34]",FACE_DIR"64.gif");
        purple_smiley_new_from_file("[FACE_116]",FACE_DIR"65.gif");
        purple_smiley_new_from_file("[FACE_36]",FACE_DIR"66.gif");
        purple_smiley_new_from_file("[FACE_37]",FACE_DIR"67.gif");
        purple_smiley_new_from_file("[FACE_38]",FACE_DIR"68.gif");
        purple_smiley_new_from_file("[FACE_91]",FACE_DIR"69.gif");
        purple_smiley_new_from_file("[FACE_92]",FACE_DIR"70.gif");
        purple_smiley_new_from_file("[FACE_93]",FACE_DIR"71.gif");
        purple_smiley_new_from_file("[FACE_29]",FACE_DIR"72.gif");
        purple_smiley_new_from_file("[FACE_117]",FACE_DIR"73.gif");
        purple_smiley_new_from_file("[FACE_72]",FACE_DIR"74.gif");
        purple_smiley_new_from_file("[FACE_45]",FACE_DIR"75.gif");
        purple_smiley_new_from_file("[FACE_42]",FACE_DIR"76.gif");
        purple_smiley_new_from_file("[FACE_39]",FACE_DIR"77.gif");
        purple_smiley_new_from_file("[FACE_62]",FACE_DIR"78.gif");
        purple_smiley_new_from_file("[FACE_46]",FACE_DIR"79.gif");
        purple_smiley_new_from_file("[FACE_47]",FACE_DIR"80.gif");
        purple_smiley_new_from_file("[FACE_71]",FACE_DIR"81.gif");
        purple_smiley_new_from_file("[FACE_95]",FACE_DIR"82.gif");
        purple_smiley_new_from_file("[FACE_118]",FACE_DIR"83.gif");
        purple_smiley_new_from_file("[FACE_119]",FACE_DIR"84.gif");
        purple_smiley_new_from_file("[FACE_120]",FACE_DIR"85.gif");
        purple_smiley_new_from_file("[FACE_121]",FACE_DIR"86.gif");
        purple_smiley_new_from_file("[FACE_122]",FACE_DIR"87.gif");
        purple_smiley_new_from_file("[FACE_123]",FACE_DIR"88.gif");
        purple_smiley_new_from_file("[FACE_124]",FACE_DIR"89.gif");
        purple_smiley_new_from_file("[FACE_27]",FACE_DIR"90.gif");
        purple_smiley_new_from_file("[FACE_21]",FACE_DIR"91.gif");
        purple_smiley_new_from_file("[FACE_23]",FACE_DIR"92.gif");
        purple_smiley_new_from_file("[FACE_25]",FACE_DIR"93.gif");
        purple_smiley_new_from_file("[FACE_26]",FACE_DIR"94.gif");
        purple_smiley_new_from_file("[FACE_125]",FACE_DIR"95.gif");
        purple_smiley_new_from_file("[FACE_126]",FACE_DIR"96.gif");
        purple_smiley_new_from_file("[FACE_127]",FACE_DIR"97.gif");
        purple_smiley_new_from_file("[FACE_128]",FACE_DIR"98.gif");
        purple_smiley_new_from_file("[FACE_129]",FACE_DIR"99.gif");
        purple_smiley_new_from_file("[FACE_130]",FACE_DIR"100.gif");
        purple_smiley_new_from_file("[FACE_131]",FACE_DIR"101.gif");
        purple_smiley_new_from_file("[FACE_132]",FACE_DIR"102.gif");
        purple_smiley_new_from_file("[FACE_133]",FACE_DIR"103.gif");
        purple_smiley_new_from_file("[FACE_134]",FACE_DIR"104.gif");
    }
}
void remove_all_smiley(void* data,void* userdata)
{
    purple_smiley_delete((PurpleSmiley*)data);
}
void translate_global_free()
{
    if(_regex){
        trex_free(_regex);
        _regex = NULL;
    }
    if(hs_regex){
        trex_free(hs_regex);
        hs_regex = NULL;
    }
    if(smily_table) {
        g_hash_table_remove_all(smily_table);
        smily_table = NULL;
        GList* list = purple_smileys_get_all();
        g_list_foreach(list,remove_all_smiley,NULL);
        g_list_free(list);
    }
}
#define MAP(face,str) \
    case face:\
    ret=str;\
    break;
const char* translate_smile(int face)
{
    const char* ret;
    static char piece[20];
    switch(face) {
        MAP(14,":)");
        MAP(13,":-D");
        MAP(50,":-(");
        MAP(12,";-)");
        //MAP(12,":P");
        MAP(57,"=-O");
        MAP(116,":-*");
        MAP(51,"8-)");
        MAP(74,":-[");
        MAP(9,":'(");
        MAP(76,":-/");
        MAP(58,"O:-)");
        MAP(106,":-x");
        MAP(107,":-$");
        MAP(85,":-!");
        MAP(110,":-!");
    default:
        snprintf(piece,sizeof(piece),"[FACE_%d]",face);
        ret = piece;
        break;
    }
    return ret;
}

void add_smiley(void* data,void* userdata)
{
    PurpleConversation* conv = userdata;
    PurpleSmiley* smiley = data;
    const char* shortcut = purple_smiley_get_shortcut(smiley);
    purple_conv_custom_smiley_add(conv,shortcut,NULL,NULL,0);
    size_t len;
    const void* d = purple_smiley_get_data(smiley,&len);
    purple_conv_custom_smiley_write(conv,shortcut,d,len);
    purple_conv_custom_smiley_close(conv,shortcut);
}
void translate_add_smiley_to_conversation(PurpleConversation* conv)
{
    GList* list = purple_smileys_get_all();
    g_list_foreach(list,add_smiley,conv);
    g_list_free(list);
}
