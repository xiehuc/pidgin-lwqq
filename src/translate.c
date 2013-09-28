#include <imgstore.h>
#include <string.h>
#include <assert.h>
#include <smiley.h>
#include <accountopt.h>
#include "translate.h"
#include "trex.h"
#include "qq_types.h"

#define LOCAL_SMILEY_PATH(path) (snprintf(path,sizeof(path),"%s"LWQQ_PATH_SEP"smiley.txt",lwdb_get_config_dir()),path)
#define GLOBAL_SMILEY_PATH(path) (snprintf(path,sizeof(path),"%s"LWQQ_PATH_SEP"smiley.txt",GLOBAL_DATADIR),path)

static GHashTable* smiley_hash;
static TRex* _regex;
static TRex* hs_regex;
struct smile_entry{
    int id;
    const char* smile[6];
};

static 
const char *smiley_tables[] = {
    "😲"/*0*/   , "😖"/*1*/  , "😍"/*2*/   , ""          , "😎"/*4*/   ,
    "T_T"/*5*/ , ""        , "😷"/*7*/   , "😴"/*8*/    , ":'("/*9*/ ,
    "😰"/*10*/  , "😡"/*11*/ , ":p"/*12*/ , ":D"/*13*/  , ":)"/*14*/ ,
    ":("/*50*/ , "😠"/*51*/ , 
    "😭"/*53*/  , ":吐:"/*54*/,
    "😨"/*55*/  , ":汗:"/*56*/, "😄"/*57*/, ":屌:"/*58*/,
    ":偷笑:"/*73*/, "😊"/*74*/, "😮"/*75*/,
    ":\\"/*76*/, "😛"/*77*/, ":困:"/*78*/, ":奋斗:"/*79*/, ":fuck:"/*80*/,
    "😓"/*96*/,
    "???"/*81*/, ":嘘:"/*82*/, "@@@"/*83*/, "😱"/*84*/,
    "😢"/*85*/, ":die:"/*86*/, ":ding:"/*87*/, ":bye:"/*88*/,
    ":雷:"/*97*/,
    ":抠鼻:"/*98*/,
    ":咻咻:"/*99*/,
    ":糗:"/*100*/,
    ":坏笑:"/*101*/,
    ":左哼:"/*102*/,
    ":右哼:"/*103*/
};
/*static struct smile_entry smile_tables[] = {
    {101,   {	"B-)",	":坏笑:",	"",   0}},
    {102,   {	"[@",	":左哼哼:",	"",     0}},
    {103,   {	"@]",	":右哼哼:",	"",     0}},
    {104,   {	":-O",	":哈欠:",	"",      0}},
    {105,   {	"]-|",	":鄙视:",	"",      0}},
    {106,   {	"P-(",	":委屈:",	"",      0}},
    {107,   {	":'|",	":快哭了:",	"",      0}},
    {108,   {	"X-)",	":阴险:",	"",      0}},
    {109,   {	":*",	":亲亲:",	":chu:",      0}},
    {110,   {	"@x",	":吓:",		"",     0}},
    {111,   {	"",	":可怜:",	"",     0}},
    {112,   {	"",	":菜刀:",	"",      0}},
    {32,    {	"",	":西瓜:",	"",     0}},
    {113,   {	":beer:",":啤酒:",	"",      0}},
    {114,   {	":basketb:",		    ":篮球:",    "",      0}},
    {115,   {	"",	":乒乓:",	"",      0}},
    {63,    {	":coffee:",			":咖啡:",	"",      0}},
    {59,    {	":pig:",	":猪:",	"",      0}},
    {33,    {	":rose:",":玫瑰:",	"",      0}},
    {34,    {	"",":凋谢:",	"",      0}},
    {116,   {	":-*",	":示爱:",	"",      0}},
    {36,    {	":heart:",           ":爱:", 	"",     0}},
    {37,    {	":break:",           ":心碎:", 	"",      0}},
    {38,    {	":cake:",":蛋糕:",	"",      0}},
    {91,    {	"",	":闪电:",	":shd:",     0}},
    {92,    {	":bomb:",":炸弹:",	"",     0}},
    {93,    {	"",	":刀:",		"",     0}},
    {29,    {	":soccer:",	        ":足球:",	"",      0}},
    {117,   {	":瓢虫:",            "",     0}},
    {72,    {	"",	":大便:",	":shit:",      0}},
    {45,    {	":moon:",":月亮:",	"",      0}},
    {42,    {	":sun:",	":太阳:",	"",      0}},
    {39,    {	":gift:",":礼物:",	"",      0}},
    {62,    {	":hug:",	":拥抱:",	"",      0}},
    {46,    {	":强:",	":strong:",   0}},
    {47,    {	":weak:",":弱:",		"",     0}},
    {71,    {	":share:",	        ":握手:",	"",      0}},
    {95,    {	"",	":胜利:",	":V:",     0}},
    {118,   {	"@)",	":抱拳:",	"",      0}},
    {119,   {	"",	":勾引:",	"",      0}},
    {120,   {	"@@",	":拳头:",	"",      0}},
    {121,   {	":bad:",	":差劲:",	"",      0}},
    {122,   {	":loveu:",	        ":爱你:",	"",    0}},
    {123,   {	":NO:",	":no:",		"",      0}},
    {124,   {	":OK:",	":ok:",		"",      0}},
    {27,    {	":love:",":爱:",	"",     0}},
    {21,    {	"",	":飞吻:",	"",      0}},
    {23,    {	":jump:", ":跳:",	"",    0}},
    {25,    {	":shake:",           ":发抖:",	"",     0}},
    {26,    {	"",	":怄火:",	"",      0}},
    {125,   {	":转圈:","",     0}},
    {126,   {	":磕头:", "",      0}},
    {127,   {	":回头:","",      0}},
    {128,   {	":跳绳:","",     0}},
    {129,   {	"",	":挥手:",	"",     0}},
    {130,   {	"#-O",	":激动:",	"",      0}},
    {131,   {	":街舞:","",	    0}},
    {132,   {	":kiss:","",	"",      0}},
    {133,   {	"",	":左太极:",	"",     0}},
    {134,   {	"",	":右太极:",	"",     0}},
    {-1,    {   0   }}
};*/
const char* HTML_SYMBOL = "<[^>]+>|&amp;|&quot;|&gt;|&lt;";
const char* REGEXP_HEAD = "<[^>]+>|:face\\d+:|:-face:";
const char* REGEXP_TAIL = "|:[^ :]+:";
//font size map space : pidgin:[1,7] to webqq[8:20]
#define sizemap(num) (6+2*num)
//font size map space : webqq[8:22] to pidgin [1:8]
#define sizeunmap(px) ((px-6)/2)
//this is used for build smiley regex expression
/*
static void build_smiley_exp(char* exp)
{
    //this charactor would esacpe to '\?'
    char* spec_char = "?()[]*$\\|+.";
    //first html label .then smiley
    struct smile_entry* entry = &smile_tables[0];
    const char *smiley,*beg,*end;
    const char** ptr;
    while(entry->id !=-1){
        ptr = entry->smile;
        long id = entry->id+1;
        while(*ptr){
            smiley = *ptr;
            g_hash_table_insert(smiley_hash,s_strdup(smiley),(gpointer)id);
            if(smiley[0]==':'&&smiley[strlen(smiley)-1]==':'){
                //move to next smiley
                ptr++;
                continue;
            }
            if(smiley[0]==0){
                //ignore empty smiley
                ptr++;
                continue;
            }
            strcat(exp,"|");
            beg = smiley;
            do{
                end=strpbrk(beg,spec_char);
                if(end==NULL) strcat(exp,beg);
                else {
                    strncat(exp, beg, end-beg);
                    strcat(exp,"\\");
                    strncat(exp,end,1);
                    beg = end+1;
                }
            }while(end);
            ptr++;
        }
        entry++;
    }
}
*/
static void build_smiley_exp_from_file(char* exp,const char* path)
{
    char smiley[256];
    long id,num;
    char *beg,*end;
    const char* spec_char = "?()[]*$\\|+.";
    FILE* f =fopen(path,"r");
    if(f==NULL) return;
    while(fscanf(f,"%s",smiley)!=EOF){
        num = strtoul(smiley, &end, 10);
        if(end-smiley == strlen(smiley)){
            id=num+1;//remap 0->1
            continue;
        }

        //insert hash table
        g_hash_table_insert(smiley_hash,s_strdup(smiley),(gpointer)id);
        if(smiley[0]==':'&&smiley[strlen(smiley)-1]==':'){
            //move to next smiley
            continue;
        }
        strcat(exp,"|");
        beg = smiley;
        do{
            end=strpbrk(beg,spec_char);
            if(end==NULL) strcat(exp,beg);
            else {
                strncat(exp, beg, end-beg);
                strcat(exp,"\\");
                strncat(exp,end,1);
                beg = end+1;
            }
        }while(end);
    }
    fclose(f);
}
static LwqqMsgContent* build_string_content(const char* from,const char* to,LwqqMsgMessage* msg)
{
    LwqqMsgContent* c;
    LwqqMsgContent* last;
    const char *begin,*end,*read;
    char* write;
    if(to==NULL)to = from+strlen(from)+1;
    last = TAILQ_LAST(&msg->content,LwqqMsgContentHead);
    if(last && last->type == LWQQ_CONTENT_STRING){
        c = NULL;//return NULL
        size_t sz = strlen(last->data.str);
        last->data.str = s_realloc(last->data.str,sz+to-from+3);
        read = write = last->data.str+sz;
    }else{
        c = s_malloc0(sizeof(*c));
        c->type = LWQQ_CONTENT_STRING;
        c->data.str = s_malloc0(to-from+3);
        read = write = c->data.str;
    }
    //initial value
    strncpy(write,from,to-from);
    write[to-from]='\0';
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
            if(begin[1]=='/'){
            }else{
                if(begin[1]=='b')lwqq_bit_set(msg->f_style,LWQQ_FONT_BOLD,1);
                else if(begin[1]=='i')lwqq_bit_set(msg->f_style,LWQQ_FONT_ITALIC,1);
                else if(begin[1]=='u')lwqq_bit_set(msg->f_style,LWQQ_FONT_UNDERLINE,1);
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
    char buf[20];
    char* ptr = buf;
    if(len>=20){
        ptr = s_malloc(len+2);
    }
    memcpy(ptr,face,len);
    ptr[len]= '\0';
    LwqqMsgContent* c;
    if(smiley_hash==NULL) translate_global_init();
    int num = (long)g_hash_table_lookup(smiley_hash,ptr);
    if(num==0) return NULL;
    //unshift face because when build it we shift it.
    num--;
    c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_FACE;
    c->data.face = num;
    if(len>=20)
        s_free(ptr);
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
int translate_message_to_struct(LwqqClient* lc,const char* to,const char* what,LwqqMsg* msg,int using_cface)
{
    const char* ptr = what;
    int img_id;
    LwqqMsgContent *c;
    const char* begin,*end;
    TRexMatch m;
    if(_regex==NULL) translate_global_init();
    TRex* x = _regex;
    LwqqMsgMessage* mmsg = (LwqqMsgMessage*)msg;
    int translate_face=1;
     
    while(*ptr!='\0'){
        c = NULL;
        if(!trex_search(x,ptr,&begin,&end)){
            ///last part.
            c = build_string_content(ptr,NULL,mmsg);
            if(c) TAILQ_INSERT_TAIL(&mmsg->content,c,entries);
            //this is finished yet.
            break;
        }
        if(begin>ptr){
            //this is used to build string before match
            //note you can not pass c directly. 
            //because insert_tail is a macro.
            c = build_string_content(ptr,begin,mmsg);
            //you should insert twice now. like |inserted|begin|
            if(c) TAILQ_INSERT_TAIL(&mmsg->content,c,entries);
        }
        trex_getsubexp(x,0,&m);
        if(strstr(begin,"<IMG")==begin){
            //process ing img.
            sscanf(begin,"<IMG ID=\"%d\">",&img_id);
            PurpleStoredImage* simg = purple_imgstore_find_by_id(img_id);
            if(using_cface||msg->type == LWQQ_MS_GROUP_MSG){
                c = lwqq_msg_fill_upload_cface(
                        purple_imgstore_get_filename(simg),
                        purple_imgstore_get_data(simg),
                        purple_imgstore_get_size(simg));
            }else{
                c = lwqq_msg_fill_upload_offline_pic(
                        purple_imgstore_get_filename(simg), 
                        purple_imgstore_get_data(simg), 
                        purple_imgstore_get_size(simg));
            }
        }else if(*begin==':'&&*(end-1)==':'){
            if(strstr(begin,":face")==begin){
                sscanf(begin,":face%d:",&img_id);
                c = build_face_direct(img_id);
            }else if(strstr(begin,":-face:")==begin){
                translate_face=!translate_face;
            }else{
                //other :faces:
                c = translate_face?build_face_content(m.begin, m.len):NULL;
                if(c==NULL) c = build_string_content(begin, end, mmsg);
            }
        }else if(begin[0]==':'){
            //other :)
            c = translate_face?build_face_content(m.begin, m.len):NULL;
            if(c==NULL) c = build_string_content(begin, end, mmsg);
        }else if(begin[0]=='&'){
        }else{
            //other face with no fix style
            c = translate_face?build_face_content(m.begin,m.len):NULL;
            if(c==NULL) c = build_string_content(begin, end, mmsg);
        }
        ptr = end;
        if(c!=NULL)
            lwqq_msg_content_append(mmsg, c);
    }
    return 0;
}
static void paste_content_string(const char* from,char* to)
{
    const char* read = from;
    const char* ptr = read;
    char* write = to;
    size_t n = 0;
    while((ptr = strpbrk(read,"<>&\""))){
        if(ptr>read){
            n = ptr-read;
            strncpy(write,read,n);
            write += n;
        }
        switch(*ptr){
            case '<' : strcpy(write,"&lt;");break;
            case '>' : strcpy(write,"&gt;");break;
            case '&' : strcpy(write,"&amp;");break;
            case '"' : strcpy(write,"&quot;");break;
        }
        read = ptr+1;
        write+=strlen(write);
    }
    if(*read != '\0'){
        strcpy(write,read);
        write+=strlen(read);
    }
    *write = '\0';
}
char* translate_to_html_symbol(const char* s)
{
    char buf[2048] = {0};
    paste_content_string(s, buf);
    return s_strdup(buf);
}
void translate_struct_to_message(qq_account* ac, LwqqMsgMessage* msg, char* buf)
{
    LwqqMsgContent* c;
    char piece[24] = {0};
    if(lwqq_bit_get(msg->f_style,LWQQ_FONT_BOLD)) strcat(buf,"<b>");
    if(lwqq_bit_get(msg->f_style,LWQQ_FONT_ITALIC)) strcat(buf,"<i>");
    if(lwqq_bit_get(msg->f_style,LWQQ_FONT_UNDERLINE)) strcat(buf,"<u>");
    snprintf(buf+strlen(buf),300,"<font ");
    if(ac->flag&DARK_THEME_ADAPT){
        int c = strtoul(msg->f_color, NULL, 16);
        int t = (c==0)?0xffffff:(c%256)/2+128+((c/256%256)/2+128)*256+((c/256/256%256)/2+128)*256*256;
        snprintf(buf+strlen(buf),300,"color=\"#%x\" ",t);
    }else
        snprintf(buf+strlen(buf),300,"color=\"#%s\" ",msg->f_color);
    if(!(ac->flag&IGNORE_FONT_FACE)&&msg->f_name)
        snprintf(buf+strlen(buf),300,"face=\"%s\" ",msg->f_name);
    if(!(ac->flag&IGNORE_FONT_SIZE))
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
                    //let it freed by purple
                    c->data.img.data = NULL;
                    //make it room to change num if necessary.
                    snprintf(piece,sizeof(piece),"<IMG ID=\"%4d\">",img_id);
                    strcat(buf,piece);
                }else{
                    if((msg->super.super.type==LWQQ_MS_GROUP_MSG&&ac->flag&NOT_DOWNLOAD_GROUP_PIC)){
                        strcat(buf,_("【DISABLE PIC】"));
                    }else if(c->data.img.url){
                        format_append(buf, "<a href=\"%s\">%s</a>",
                                c->data.img.url,
                                _("【PIC】")
                                );
                    }else{
                        strcat(buf,_("【PIC NOT FOUND】"));
                    }
                }
                break;
            case LWQQ_CONTENT_CFACE:
                if(c->data.cface.size>0){
                    int img_id = purple_imgstore_add_with_id(c->data.cface.data,c->data.cface.size,NULL);
                    //let it freed by purple
                    c->data.cface.data = NULL;
                    snprintf(piece,sizeof(piece),"<IMG ID=\"%4d\">",img_id);
                    strcat(buf,piece);
                }else{
                    if((msg->super.super.type==LWQQ_MS_GROUP_MSG&&ac->flag&NOT_DOWNLOAD_GROUP_PIC)){
                        strcat(buf,_("【DISABLE PIC】"));
                    }else if(c->data.cface.url){
                        format_append(buf, "<a href=\"%s\">%s</a>",
                                c->data.cface.url,
                                _("【PIC】")
                                );
                    }else{
                        strcat(buf,_("【PIC NOT FOUND】"));
                    }
                }
                break;
        }
    }
    strcat(buf,"</font>");
    if(lwqq_bit_get(msg->f_style,LWQQ_FONT_BOLD)) strcat(buf,"</u>");
    if(lwqq_bit_get(msg->f_style,LWQQ_FONT_ITALIC)) strcat(buf,"</i>");
    if(lwqq_bit_get(msg->f_style,LWQQ_FONT_UNDERLINE)) strcat(buf,"</b>");
}
void translate_global_init()
{
    
    if(_regex==NULL){
        const char* err = NULL;
        char *smiley_exp = s_malloc0(2048);
        smiley_hash = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,NULL);
        char path[1024];
        strcat(smiley_exp,REGEXP_HEAD);
        //build_smiley_exp(smiley_exp);
        build_smiley_exp_from_file(smiley_exp, GLOBAL_SMILEY_PATH(path));
        build_smiley_exp_from_file(smiley_exp, LOCAL_SMILEY_PATH(path));
        strcat(smiley_exp,REGEXP_TAIL);
        _regex = trex_compile(_TREXC(smiley_exp),&err);
        if(err) {lwqq_puts(err);assert(0);}
        free(smiley_exp);
        assert(_regex!=NULL);
    }
    if(hs_regex==NULL){
        const char* err = NULL;
        hs_regex = trex_compile(_TREXC(HTML_SYMBOL),&err);
        if(err){lwqq_puts(err);assert(0);}
        assert(_regex!=NULL);
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
    if(smiley_hash) {
        g_hash_table_remove_all(smiley_hash);
        smiley_hash = NULL;
        GList* list = purple_smileys_get_all();
        g_list_foreach(list,remove_all_smiley,NULL);
        g_list_free(list);
    }
}
const char* translate_smile(int face)
{
    static char buf[64];
    snprintf(buf, sizeof(buf), ":face%d:",face);
    if(face<2){
        strcpy(buf,smiley_tables[face]);
    }
    /*struct smile_entry* entry = &smile_tables[0];
    while(entry->id != face&&entry->id!=-1){
        entry++;
    }
    buf[0]=0;
    if(entry->id!=-1){
        strncpy(buf,entry->smile[0],sizeof(buf));
        if(buf[0]=='/') strcat(buf," ");
    }
    */
    return buf;
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

