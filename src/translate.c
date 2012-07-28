#include <imgstore.h>
#include <string.h>
#include <smemory.h>
#include <type.h>
#include <assert.h>
#include <smiley.h>
#include "translate.h"
#include "trex.h"

#ifndef INST_PREFIX
#define INST_PREFIX "/usr/local"
#endif

#define FACE_DIR INST_PREFIX"/share/lwqq/icons/qqfaces/"
static GHashTable* smily_table;
static TRex* _regex;
const char* SMILY_EXP = "<IMG ID=\"\\d+\">|\\[FACE \\d+\\]|"
    ":\\)|:-D|:-\\(|;-\\)|:P|=-O|:-\\*|8-\\)|:-\\[|"
    ":'\\(|:-/|O:-\\)|:-x|:-\\$|:-!";
    //:'( error
static void translate_escape(char * str)
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
static LwqqMsgContent* build_string_content(const char* from,const char* to)
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
    return c;
}
static LwqqMsgContent* build_face_content(const char* face)
{
    LwqqMsgContent* c;
    c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_FACE;
    int num = (long)g_hash_table_lookup(smily_table,face);
    if(num==0) return NULL;
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
void translate_message_to_struct(LwqqClient* lc,const char* to,const char* what,LwqqMsgMessage* mmsg,int using_cface)
{
    const char* ptr = what;
    int img_id;
    LwqqMsgContent *c;
    puts(what);
    const char* begin,*end;
    TRexMatch m;
    TRex* x = _regex;
    //trex_clear(x);
    
    while(*ptr!='\0'){
        c = NULL;
        if(!trex_search(x,ptr,&begin,&end)){
            ///last part.
            TAILQ_INSERT_TAIL(&mmsg->content,build_string_content(ptr,NULL),entries);
            break;
        }
        if(begin>ptr){
            TAILQ_INSERT_TAIL(&mmsg->content,build_string_content(ptr,begin),entries);
        }
        trex_getsubexp(x,0,&m);
        puts(m.begin);
        if(strstr(begin,"<IMG")==begin){
            sscanf(begin,"<IMG ID=\"%d\">",&img_id);
            PurpleStoredImage* simg = purple_imgstore_find_by_id(img_id);
            if(using_cface){
                c = lwqq_msg_upload_cface(lc,
                        purple_imgstore_get_filename(simg),
                        purple_imgstore_get_data(simg),
                        purple_imgstore_get_size(simg),
                        purple_imgstore_get_extension(simg));
            }else{
                c = lwqq_msg_upload_offline_pic(lc,to,
                        purple_imgstore_get_filename(simg),
                        purple_imgstore_get_data(simg),
                        purple_imgstore_get_size(simg),
                        purple_imgstore_get_extension(simg));
            }
        }else if(strstr(begin,"[FACE")==begin){
            sscanf(begin,"[FACE %d]",&img_id);
            c = build_face_direct(img_id);
        }else{
            //other face
            c = build_face_content(m.begin);
        }
        ptr = end;
        if(c!=NULL)
            TAILQ_INSERT_TAIL(&mmsg->content,c,entries);
    }
}
void translate_global_init()
{
    if(_regex==NULL){
        const char* err = NULL;
        _regex = trex_compile(_TREXC(SMILY_EXP),&err);
        if(err) {puts(err);assert(0);}
        assert(_regex!=NULL);
    }
    if(smily_table ==NULL){
#define GPOINTER(n) (gpointer)n
        GHashTable *t = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,NULL);
        g_hash_table_insert(t,":)",GPOINTER(GPOINTER(14)));
        g_hash_table_insert(t,":-D",GPOINTER(13));
        g_hash_table_insert(t,":-(",GPOINTER(50));
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
        /*purple_smiley_new_from_file(":)",FACE_DIR"0.gif");
        purple_smiley_new_from_file(":-D",FACE_DIR"13.gif");
        purple_smiley_new_from_file(":-(",FACE_DIR"15.gif");
        purple_smiley_new_from_file(";-)",FACE_DIR"12.gif");
        purple_smiley_new_from_file(":P",FACE_DIR"12.gif");
        purple_smiley_new_from_file(":P",FACE_DIR"12.gif");*/
        purple_smiley_new_from_file("[FACE 14]",FACE_DIR"0.gif");
        purple_smiley_new_from_file("[FACE 1]",FACE_DIR"1.gif");
        purple_smiley_new_from_file("[FACE 2]",FACE_DIR"2.gif");
        purple_smiley_new_from_file("[FACE 3]",FACE_DIR"3.gif");
        purple_smiley_new_from_file("[FACE 4]",FACE_DIR"4.gif");
        purple_smiley_new_from_file("[FACE 5]",FACE_DIR"5.gif");
        purple_smiley_new_from_file("[FACE 6]",FACE_DIR"6.gif");
        purple_smiley_new_from_file("[FACE 7]",FACE_DIR"7.gif");
        purple_smiley_new_from_file("[FACE 8]",FACE_DIR"8.gif");
        purple_smiley_new_from_file("[FACE 9]",FACE_DIR"9.gif");
        purple_smiley_new_from_file("[FACE 10]",FACE_DIR"10.gif");
        purple_smiley_new_from_file("[FACE 11]",FACE_DIR"11.gif");
        purple_smiley_new_from_file("[FACE 12]",FACE_DIR"12.gif");
        purple_smiley_new_from_file("[FACE 13]",FACE_DIR"13.gif");
        purple_smiley_new_from_file("[FACE 0]",FACE_DIR"14.gif");
        purple_smiley_new_from_file("[FACE 50]",FACE_DIR"15.gif");
        purple_smiley_new_from_file("[FACE 51]",FACE_DIR"16.gif");
        purple_smiley_new_from_file("[FACE 96]",FACE_DIR"17.gif");
        purple_smiley_new_from_file("[FACE 53]",FACE_DIR"18.gif");
        purple_smiley_new_from_file("[FACE 54]",FACE_DIR"19.gif");
        purple_smiley_new_from_file("[FACE 73]",FACE_DIR"20.gif");
        purple_smiley_new_from_file("[FACE 74]",FACE_DIR"21.gif");
        purple_smiley_new_from_file("[FACE 75]",FACE_DIR"22.gif");
        purple_smiley_new_from_file("[FACE 76]",FACE_DIR"23.gif");
        purple_smiley_new_from_file("[FACE 77]",FACE_DIR"24.gif");
        purple_smiley_new_from_file("[FACE 78]",FACE_DIR"25.gif");
        purple_smiley_new_from_file("[FACE 55]",FACE_DIR"26.gif");
        purple_smiley_new_from_file("[FACE 56]",FACE_DIR"27.gif");
        purple_smiley_new_from_file("[FACE 57]",FACE_DIR"28.gif");
        purple_smiley_new_from_file("[FACE 58]",FACE_DIR"29.gif");
        purple_smiley_new_from_file("[FACE 79]",FACE_DIR"30.gif");
        purple_smiley_new_from_file("[FACE 80]",FACE_DIR"31.gif");
        purple_smiley_new_from_file("[FACE 81]",FACE_DIR"32.gif");
        purple_smiley_new_from_file("[FACE 82]",FACE_DIR"33.gif");
        purple_smiley_new_from_file("[FACE 83]",FACE_DIR"34.gif");
        purple_smiley_new_from_file("[FACE 84]",FACE_DIR"35.gif");
        purple_smiley_new_from_file("[FACE 85]",FACE_DIR"36.gif");
        purple_smiley_new_from_file("[FACE 86]",FACE_DIR"37.gif");
        purple_smiley_new_from_file("[FACE 87]",FACE_DIR"38.gif");
        purple_smiley_new_from_file("[FACE 88]",FACE_DIR"39.gif");
        purple_smiley_new_from_file("[FACE 97]",FACE_DIR"40.gif");
        purple_smiley_new_from_file("[FACE 98]",FACE_DIR"41.gif");
        purple_smiley_new_from_file("[FACE 99]",FACE_DIR"42.gif");
        purple_smiley_new_from_file("[FACE 100]",FACE_DIR"43.gif");
        purple_smiley_new_from_file("[FACE 101]",FACE_DIR"44.gif");
        purple_smiley_new_from_file("[FACE 102]",FACE_DIR"45.gif");
        purple_smiley_new_from_file("[FACE 103]",FACE_DIR"46.gif");
        purple_smiley_new_from_file("[FACE 104]",FACE_DIR"47.gif");
        purple_smiley_new_from_file("[FACE 105]",FACE_DIR"48.gif");
        purple_smiley_new_from_file("[FACE 106]",FACE_DIR"49.gif");
        purple_smiley_new_from_file("[FACE 107]",FACE_DIR"50.gif");
        purple_smiley_new_from_file("[FACE 108]",FACE_DIR"51.gif");
        purple_smiley_new_from_file("[FACE 109]",FACE_DIR"52.gif");
        purple_smiley_new_from_file("[FACE 110]",FACE_DIR"53.gif");
        purple_smiley_new_from_file("[FACE 111]",FACE_DIR"54.gif");
        purple_smiley_new_from_file("[FACE 112]",FACE_DIR"55.gif");
        purple_smiley_new_from_file("[FACE 32]",FACE_DIR"56.gif");
        purple_smiley_new_from_file("[FACE 113]",FACE_DIR"57.gif");
        purple_smiley_new_from_file("[FACE 114]",FACE_DIR"58.gif");
        purple_smiley_new_from_file("[FACE 115]",FACE_DIR"59.gif");
        purple_smiley_new_from_file("[FACE 63]",FACE_DIR"60.gif");
        purple_smiley_new_from_file("[FACE 64]",FACE_DIR"61.gif");
        purple_smiley_new_from_file("[FACE 59]",FACE_DIR"62.gif");
        purple_smiley_new_from_file("[FACE 33]",FACE_DIR"63.gif");
        purple_smiley_new_from_file("[FACE 34]",FACE_DIR"64.gif");
        purple_smiley_new_from_file("[FACE 116]",FACE_DIR"65.gif");
        purple_smiley_new_from_file("[FACE 36]",FACE_DIR"66.gif");
        purple_smiley_new_from_file("[FACE 37]",FACE_DIR"67.gif");
        purple_smiley_new_from_file("[FACE 38]",FACE_DIR"68.gif");
        purple_smiley_new_from_file("[FACE 91]",FACE_DIR"69.gif");
        purple_smiley_new_from_file("[FACE 92]",FACE_DIR"70.gif");
        purple_smiley_new_from_file("[FACE 93]",FACE_DIR"71.gif");
        purple_smiley_new_from_file("[FACE 29]",FACE_DIR"72.gif");
        purple_smiley_new_from_file("[FACE 117]",FACE_DIR"73.gif");
        purple_smiley_new_from_file("[FACE 72]",FACE_DIR"74.gif");
        purple_smiley_new_from_file("[FACE 45]",FACE_DIR"75.gif");
        purple_smiley_new_from_file("[FACE 42]",FACE_DIR"76.gif");
        purple_smiley_new_from_file("[FACE 39]",FACE_DIR"77.gif");
        purple_smiley_new_from_file("[FACE 62]",FACE_DIR"78.gif");
        purple_smiley_new_from_file("[FACE 46]",FACE_DIR"79.gif");
        purple_smiley_new_from_file("[FACE 47]",FACE_DIR"80.gif");
        purple_smiley_new_from_file("[FACE 71]",FACE_DIR"81.gif");
        purple_smiley_new_from_file("[FACE 95]",FACE_DIR"82.gif");
        purple_smiley_new_from_file("[FACE 118]",FACE_DIR"83.gif");
        purple_smiley_new_from_file("[FACE 119]",FACE_DIR"84.gif");
        purple_smiley_new_from_file("[FACE 120]",FACE_DIR"85.gif");
        purple_smiley_new_from_file("[FACE 121]",FACE_DIR"86.gif");
        purple_smiley_new_from_file("[FACE 122]",FACE_DIR"87.gif");
        purple_smiley_new_from_file("[FACE 123]",FACE_DIR"88.gif");
        purple_smiley_new_from_file("[FACE 124]",FACE_DIR"89.gif");
        purple_smiley_new_from_file("[FACE 27]",FACE_DIR"90.gif");
        purple_smiley_new_from_file("[FACE 21]",FACE_DIR"91.gif");
        purple_smiley_new_from_file("[FACE 23]",FACE_DIR"92.gif");
        purple_smiley_new_from_file("[FACE 25]",FACE_DIR"93.gif");
        purple_smiley_new_from_file("[FACE 26]",FACE_DIR"94.gif");
        purple_smiley_new_from_file("[FACE 125]",FACE_DIR"95.gif");
        purple_smiley_new_from_file("[FACE 126]",FACE_DIR"96.gif");
        purple_smiley_new_from_file("[FACE 127]",FACE_DIR"97.gif");
        purple_smiley_new_from_file("[FACE 128]",FACE_DIR"98.gif");
        purple_smiley_new_from_file("[FACE 129]",FACE_DIR"99.gif");
        purple_smiley_new_from_file("[FACE 130]",FACE_DIR"100.gif");
        purple_smiley_new_from_file("[FACE 131]",FACE_DIR"101.gif");
        purple_smiley_new_from_file("[FACE 132]",FACE_DIR"102.gif");
        purple_smiley_new_from_file("[FACE 133]",FACE_DIR"103.gif");
        purple_smiley_new_from_file("[FACE 134]",FACE_DIR"104.gif");
    }
}
void translate_global_free()
{
    if(_regex) trex_free(_regex);
    if(smily_table) g_hash_table_remove_all(smily_table);
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
        snprintf(piece,sizeof(piece),"[FACE %d]",face);
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
