/**
 * @file   msg.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Thu Jun 14 14:42:17 2012
 * 
 * @brief  Message receive and send API
 * 
 * 
 */
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include "type.h"
#include "smemory.h"
#include "json.h"
#include "http.h"
#include "url.h"
#include "logger.h"
#include "msg.h"
#include "queue.h"
#include "unicode.h"
#include "async.h"
#include "info.h"
#include "internal.h"
#include "util.h"
#include "json.h"

#define LWQQ_MT_BITS  (~((-1)<<8))

static void *start_poll_msg(void *msg_list);
static void lwqq_recvmsg_poll_msg(struct LwqqRecvMsgList *list);
static void lwqq_recvmsg_poll_close(LwqqRecvMsgList* list);
static json_t *get_result_json_object(json_t *json);
static int parse_recvmsg_from_json(LwqqRecvMsgList *list, const char *str);

static int msg_send_back(LwqqHttpRequest* req,void* data);
static int upload_cface_back(LwqqHttpRequest *req,LwqqClient* lc,LwqqMsgContent* c);
static int upload_offline_pic_back(LwqqHttpRequest* req,LwqqMsgContent* c,const char* to);
static int upload_offline_file_back(LwqqHttpRequest* req,void* data);
static int send_offfile_back(LwqqHttpRequest* req,void* data);
static void insert_recv_msg_with_order(LwqqRecvMsgList* list,LwqqMsg* msg);

typedef struct LwqqRecvMsgListInternal {
    struct LwqqRecvMsgList parent;
    LwqqAsyncTimer tip_loop;
    pthread_t tid;
    int on_quit;
} LwqqRecvMsgListInternal;
#define RET_INSERT_MSG 0
#define RET_DELAY_INS 1
#define RET_BAD_MSG -1

static struct LwqqStrMapEntry_ msg_type_map[] = {
    {"message",                 LWQQ_MS_BUDDY_MSG,          },
    {"group_message",           LWQQ_MS_GROUP_MSG,          },
    {"discu_message",           LWQQ_MS_DISCU_MSG,          },
    {"sess_message",            LWQQ_MS_SESS_MSG,           },
    {"buddies_status_change",   LWQQ_MT_STATUS_CHANGE,      },
    {"kick_message",            LWQQ_MT_KICK_MESSAGE,       },
    {"system_message",          LWQQ_MT_SYSTEM,             },
    {"buddylist_change",        LWQQ_MT_BLIST_CHANGE,       },
    {"sys_g_msg",               LWQQ_MT_SYS_G_MSG,          },
    {"push_offfile",            LWQQ_MT_OFFFILE,            },
    {"filesrv_transfer",        LWQQ_MT_FILETRANS,          },
    {"file_message",            LWQQ_MT_FILE_MSG,           },
    {"notify_offfile",          LWQQ_MT_NOTIFY_OFFFILE,     },
    {"input_notify",            LWQQ_MT_INPUT_NOTIFY,       },
    {"shake_message",           LWQQ_MT_SHAKE_MESSAGE,      },
    {"unknow",                  LWQQ_MT_UNKNOWN,            },
    {NULL,                      LWQQ_MT_UNKNOWN,            }
};


static int parse_content(json_t *json,const char* key, LwqqMsgMessage* opaque)
{
    json_t *tmp, *ctent;
    LwqqMsgMessage *msg = opaque;

    tmp = json_find_first_label_all(json, key);
    if (!tmp || !tmp->child || !tmp->child) {
        return -1;
    }
    tmp = tmp->child->child;
    for (ctent = tmp; ctent != NULL; ctent = ctent->next) {
        if (ctent->type == JSON_ARRAY) {
            /* ["font",{"size":10,"color":"000000","style":[0,0,0],"name":"\u5B8B\u4F53"}] */
            char *buf;
            /* FIXME: ensure NULL access */
            buf = ctent->child->text;
            if (!strcmp(buf, "font")) {
                const char *name, *color, *size;
                int sa, sb, sc;
                /* Font name */
                name = json_parse_simple_value(ctent, "name");
                name = name ?: "Arial";
                msg->f_name = ucs4toutf8(name);

                /* Font color */
                color = json_parse_simple_value(ctent, "color");
                strcpy(msg->f_color,color?:"000000");

                /* Font size */
                size = json_parse_simple_value(ctent, "size");
                size = size ?: "12";
                msg->f_size = atoi(size);

                /* Font style: style":[0,0,0] */
                tmp = json_find_first_label_all(ctent, "style");
                if (tmp) {
                    json_t *style = tmp->child->child;
                    const char *stylestr = style->text;
                    sa = (int)strtol(stylestr, NULL, 10);
                    style = style->next;
                    stylestr = style->text;
                    sb = (int)strtol(stylestr, NULL, 10);
                    style = style->next;
                    stylestr = style->text;
                    sc = (int)strtol(stylestr, NULL, 10);
                } else {
                    sa = 0;
                    sb = 0;
                    sc = 0;
                }
                msg->f_style.b = sa;
                msg->f_style.i = sb;
                msg->f_style.u = sc;
            } else if (!strcmp(buf, "face")) {
                /* ["face", 107] */
                /* FIXME: ensure NULL access */
                int facenum = (int)strtol(ctent->child->next->text, NULL, 10);
                LwqqMsgContent *c = s_malloc0(sizeof(*c));
                c->type = LWQQ_CONTENT_FACE;
                c->data.face = facenum; 
                TAILQ_INSERT_TAIL(&msg->content, c, entries);
            } else if(!strcmp(buf, "offpic")) {
                //["offpic",{"success":1,"file_path":"/d65c58ae-faa6-44f3-980e-272fb44a507f"}]
                LwqqMsgContent *c = s_malloc0(sizeof(*c));
                c->type = LWQQ_CONTENT_OFFPIC;
                c->data.img.success = s_atoi(json_parse_simple_value(ctent,"success"),0);
                c->data.img.file_path = s_strdup(json_parse_simple_value(ctent,"file_path"));
                TAILQ_INSERT_TAIL(&msg->content,c,entries);
            } else if(!strcmp(buf,"cface")){
                //["cface",{"name":"0C3AED06704CA9381EDCC20B7F552802.jPg","file_id":914490174,"key":"YkC3WaD3h5pPxYrY","server":"119.147.15.201:443"}]
                //["cface","0C3AED06704CA9381EDCC20B7F552802.jPg",""]
                LwqqMsgContent* c = s_malloc0(sizeof(*c));
                c->type = LWQQ_CONTENT_CFACE;
                c->data.cface.name = s_strdup(json_parse_simple_value(ctent,"name"));
                if(c->data.cface.name!=NULL){
                    c->data.cface.file_id = s_strdup(json_parse_simple_value(ctent,"file_id"));
                    c->data.cface.key = s_strdup(json_parse_simple_value(ctent,"key"));
                    char* server = s_strdup(json_parse_simple_value(ctent,"server"));
                    char* split = strchr(server,':');
                    strncpy(c->data.cface.serv_ip,server,split-server);
                    strncpy(c->data.cface.serv_port,split+1,strlen(split+1));
                }else{
                    c->data.cface.name = s_strdup(ctent->child->next->text);
                }
                TAILQ_INSERT_TAIL(&msg->content,c,entries);
            }
        } else if (ctent->type == JSON_STRING) {
            LwqqMsgContent *c = s_malloc0(sizeof(*c));
            c->type = LWQQ_CONTENT_STRING;
            c->data.str = json_unescape(ctent->text);
            TAILQ_INSERT_TAIL(&msg->content, c, entries);
        }
    }

    /* Make msg valid */
    if (!msg->f_name || !msg->f_color || TAILQ_EMPTY(&msg->content)) {
        return -1;
    }
    if (msg->f_size < 8) {
        msg->f_size = 8;
    }

    return 0;
}

static void insert_msg_delay_by_request_content(LwqqRecvMsgList* list,LwqqMsg* msg)
{
    insert_recv_msg_with_order(list,msg);
    LwqqClient* lc = list->lc;
    lc->dispatch(vp_func_p,(CALLBACK_FUNC)lc->async_opt->poll_msg,list->lc);
}
static int process_simple_response(LwqqHttpRequest* req)
{
    //{"retcode":0,"result":{"ret":0}}
    int err = 0;
    json_t *root = NULL;
    lwqq__jump_if_http_fail(req,err);
    lwqq_puts(req->response);
    lwqq__jump_if_json_fail(root,req->response,err);
    int retcode = s_atoi(json_parse_simple_value(root, "retcode"),LWQQ_EC_ERROR);
    if(retcode != WEBQQ_OK){
        err = retcode;
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}
static int process_msg_list(LwqqHttpRequest* req,char* serv_id,LwqqHistoryMsgList* list)
{
    ///alloy.app.chatLogViewer.rederChatLog(
    //{ret:0,tuin:2141909423,page:14,total:14,chatlogs:[{ver:3,cmd:16,seq:160,time:1358935482,type:1,msg:["12"] }
    int err = 0;
    json_t* root = NULL;
    char buf[8192*3];
    memset(buf,0,sizeof(buf));
    lwqq__jump_if_http_fail(req,err);
    char* beg = strchr(req->response,'{');
    char* end = strrchr(req->response,')');
    char* write = buf;
    if(!beg||!end) goto done;
    *end = '\0';
    while(*beg!='\0'){
        if(*beg=='{'){
            if(!strncmp(beg+1,"ver:",4)||!strncmp(beg+1,"ret:",4)){
                strcpy(write,"{\"");
                beg++;
                end = strchr(beg,':');
                strncat(write,beg,end-beg);
                strcat(write,"\":");
                beg = end+1;
            }else
                *write++=*beg++;
        }else if(*beg==','){
            if(!strncmp(beg+1,"cmd:",4)||!strncmp(beg+1,"seq:",4)||
                    !strncmp(beg+1,"time:",5)||!strncmp(beg+1,"type:",5)||
                    !strncmp(beg+1,"msg:",4)||!strncmp(beg+1,"tuin:",5)||
                    !strncmp(beg+1,"page:",5)||!strncmp(beg+1,"total:",6)||
                    !strncmp(beg+1,"chatlogs:",9)
                    ){
                strcpy(write,",\"");
                beg++;
                end = strchr(beg,':');
                strncat(write,beg,end-beg);
                strcat(write,"\":");
                beg = end+1;
            }else *write++=*beg++;
        }else{
            end = strpbrk(beg, "{,");
            if(end==NULL){
                strcpy(write,beg);
                break;
            }
            strncpy(write,beg,end-beg);
            beg=end;
        }
        write+=strlen(write);
    }
    //*write='\0';
    lwqq__jump_if_json_fail(root,buf,err);
    err = lwqq__json_get_int(root, "ret",-1);
    if(err!=0) goto done;

    list->page = lwqq__json_get_int(root,"page",0);
    list->total = lwqq__json_get_int(root,"total",0);
    lwqq_verbose(3,"[online history page:%d,total:%d]\n",list->page,list->total);
    json_t* log;
    lwqq__json_parse_child(root,"chatlogs",log);
    if(log) log=log->child_end;
    const char* me = ((LwqqClient*)req->lc)->myself->uin;
    while(log){
        LwqqMsgMessage* msg = (LwqqMsgMessage*)lwqq_msg_new(LWQQ_MS_BUDDY_MSG);
        msg->time = lwqq__json_get_long(log,"time",0);
        int cmd = lwqq__json_get_int(log,"cmd",0);
        if(cmd==17){
            msg->super.from = s_strdup(serv_id);
            msg->super.to = s_strdup(me);
        }else{
            msg->super.from = s_strdup(me);
            msg->super.to = s_strdup(serv_id);
        }
        parse_content(log,"msg",msg);
        LwqqRecvMsg* wrapper = s_malloc0(sizeof(*wrapper));
        wrapper->msg = (LwqqMsg*)msg;
        TAILQ_INSERT_HEAD(&list->msg_list,wrapper,entries);
        log = log->previous;
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}

static int process_group_msg_list(LwqqHttpRequest* req,char* unused,LwqqHistoryMsgList* list)
{
    //{"retcode":0,"result":{"time":1359526607,"data":{"cl":[{"g":249818602,"cl":[{"u":2501542492,"t":1359449452,"il":[{"v":"1231\n【提示：此用户正在使用Q+ Web：http://web2.qq.com/】","t":0}]}

    int err = 0;
    json_t* root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    lwqq__jump_if_retcode_fail(err);
    if(!result) goto done;
    lwqq__json_parse_child(result,"data",result);
    if(!result || result->type == JSON_NULL) goto done;
    lwqq__json_parse_child(result,"cl",result);
    if(!result) goto done;
    result = result->child;
    list->begin = lwqq__json_get_int(result,"bs",0);
    list->end = lwqq__json_get_int(result,"es",0);
    //const char* gid = json_parse_simple_value(result, "g");
    lwqq__json_parse_child(result,"cl",result);
    if(!result) goto done;
    result = result->child;
    while(result){
        LwqqMsgMessage* msg = (LwqqMsgMessage*)lwqq_msg_new(LWQQ_MS_GROUP_MSG);
        msg->super.from = lwqq__json_get_value(result,"u");
        msg->time = lwqq__json_get_long(result,"t",0);
        //parse_content(result, "v", msg);
        LwqqMsgContent* c = s_malloc0(sizeof(*c));
        c->type = LWQQ_CONTENT_STRING;
        c->data.str = lwqq__json_get_string(result,"v");
        TAILQ_INSERT_TAIL(&msg->content,c,entries);
        LwqqRecvMsg* wrapper = s_malloc0(sizeof(*wrapper));
        wrapper->msg = (LwqqMsg*)msg;
        TAILQ_INSERT_TAIL(&list->msg_list,wrapper,entries);

        result = result->next;
    }

done:
    lwqq__clean_json_and_req(root,req);
    return err;
}
/**
 * Create a new LwqqRecvMsgList object
 * 
 * @param client Lwqq Client reference
 * 
 * @return NULL on failure
 */
LwqqRecvMsgList *lwqq_recvmsg_new(void *client)
{
    LwqqRecvMsgList *list;

    list = s_malloc0(sizeof(LwqqRecvMsgListInternal));
    list->count = 0;
    list->poll_flags = POLL_AUTO_REQUEST_PIC&POLL_AUTO_REQUEST_CFACE;
    list->lc = client;
    pthread_mutex_init(&list->mutex, NULL);
    TAILQ_INIT(&list->head);
    list->poll_msg = lwqq_recvmsg_poll_msg;
    list->poll_close = lwqq_recvmsg_poll_close;
    
    return list;
}
LwqqHistoryMsgList *lwqq_historymsg_list()
{
    LwqqHistoryMsgList* list;
    list = s_malloc0(sizeof(LwqqHistoryMsgList));
    list->row = 30;
    list->page = 0;
    TAILQ_INIT(&list->msg_list);
    return list;
}

/**
 * Free a LwqqRecvMsgList object
 * 
 * @param list 
 */
void lwqq_recvmsg_free(LwqqRecvMsgList *list)
{
    LwqqRecvMsg *recvmsg;
    
    if (!list)
        return ;

    pthread_mutex_lock(&list->mutex);
    while ((recvmsg = TAILQ_FIRST(&list->head))) {
        TAILQ_REMOVE(&list->head,recvmsg, entries);
        lwqq_msg_free(recvmsg->msg);
        s_free(recvmsg);
    }
    pthread_mutex_unlock(&list->mutex);

    s_free(list);
    return ;
}
void lwqq_historymsg_free(LwqqHistoryMsgList *list)
{
    LwqqRecvMsg* msg;
    while((msg = TAILQ_FIRST(&list->msg_list))){
        TAILQ_REMOVE(&list->msg_list,msg,entries);
        lwqq_msg_free(msg->msg);
        s_free(msg);
    }
    s_free(list);
}

LwqqMsg *lwqq_msg_new(LwqqMsgType msg_type)
{
    LwqqMsg* msg = NULL;
    int type = msg_type&LWQQ_MT_BITS;
    switch(type){
        case LWQQ_MT_MESSAGE:
            {
            msg = s_malloc0(sizeof(LwqqMsgMessage));
            LwqqMsgMessage* mmsg = (LwqqMsgMessage*)msg;
            strcpy(mmsg->f_color,"000000");
            TAILQ_INIT(&mmsg->content);
            }
            break;
        case LWQQ_MT_STATUS_CHANGE:
            msg = s_malloc0(sizeof(LwqqMsgStatusChange));
            break;
        case LWQQ_MT_KICK_MESSAGE:
            msg = s_malloc0(sizeof(LwqqMsgKickMessage));
            break;
        case LWQQ_MT_SYSTEM:
            msg = s_malloc0(sizeof(LwqqMsgSystem));
            break;
        case LWQQ_MT_BLIST_CHANGE:
            msg = s_malloc0(sizeof(LwqqMsgBlistChange));
            break;
        case LWQQ_MT_SYS_G_MSG:
            msg = s_malloc0(sizeof(LwqqMsgSysGMsg));
            break;
        case LWQQ_MT_OFFFILE:
            msg = s_malloc0(sizeof(LwqqMsgOffFile));
            break;
        case LWQQ_MT_FILETRANS:
            msg = s_malloc0(sizeof(LwqqMsgFileTrans));
            break;
        case LWQQ_MT_FILE_MSG:
            msg = s_malloc0(sizeof(LwqqMsgFileMessage));
            break;
        case LWQQ_MT_NOTIFY_OFFFILE:
            msg = s_malloc0(sizeof(LwqqMsgNotifyOfffile));
            break;
        case LWQQ_MT_INPUT_NOTIFY:
            msg = s_malloc0(sizeof(LwqqMsgInputNotify));
            break;
        case LWQQ_MT_SHAKE_MESSAGE:
            msg = s_malloc0(sizeof(LwqqMsgShakeMessage));
            break;
        default:
            msg = NULL;
            lwqq_log(LOG_ERROR,"No such message type\n");
            break;
    }
    if(msg) msg->type = msg_type;
    return msg;
}

static void msg_message_free(LwqqMsg *opaque)
{
    LwqqMsgMessage *msg = (LwqqMsgMessage*)opaque;
    if (!msg) {
        return ;
    }

    s_free(msg->f_name);
    if(opaque->type == LWQQ_MS_GROUP_MSG){
        s_free(msg->group.send);
        s_free(msg->group.group_code);
    }else if(opaque->type == LWQQ_MS_SESS_MSG){
        s_free(msg->sess.id);
        s_free(msg->sess.group_sig);
    }else if(opaque->type == LWQQ_MS_DISCU_MSG){
        s_free(msg->discu.send);
        s_free(msg->discu.did);
    }

    LwqqMsgContent *c;
    LwqqMsgContent *t;
    TAILQ_FOREACH_SAFE(c, &msg->content, entries,t) {
        switch(c->type){
            case LWQQ_CONTENT_STRING:
                s_free(c->data.str);
                break;
            case LWQQ_CONTENT_OFFPIC:
                s_free(c->data.img.file_path);
                s_free(c->data.img.name);
                s_free(c->data.img.data);
                break;
            case LWQQ_CONTENT_CFACE:
                s_free(c->data.cface.data);
                s_free(c->data.cface.name);
                s_free(c->data.cface.file_id);
                s_free(c->data.cface.key);
                s_free(c->data.cface.direct_url);
                break;
            default:
                break;
        }
        s_free(c);
    }
    
}

static void msg_status_free(void *opaque)
{
    LwqqMsgStatusChange *s = opaque;
    s_free(s->who);
    s_free(s->status);
}

static void msg_system_free(LwqqMsg* opaque)
{
    LwqqMsgSystem* system = (LwqqMsgSystem*)opaque;
    if(system){
        s_free(system->seq);
        s_free(system->account);
        s_free(system->stat);
        s_free(system->client_type);

        if(system->type==VERIFY_REQUIRED){
            s_free(system->verify_required.msg);
            s_free(system->verify_required.allow);
        }else if(system->type==ADDED_BUDDY_SIG){
            s_free(system->added_buddy_sig.sig);
        }else if(system->type==VERIFY_PASS||system->type==VERIFY_PASS_ADD){
            s_free(system->verify_pass.group_id);
        }
    }
}
static void msg_sys_g_msg_free(LwqqMsg* msg)
{
    LwqqMsgSysGMsg* gmsg = (LwqqMsgSysGMsg*)msg;
    if(gmsg){
        if(gmsg->type == GROUP_LEAVE)
            lwqq_group_free(gmsg->group);
        s_free(gmsg->gcode);
        s_free(gmsg->group_uin);
        s_free(gmsg->member);
        s_free(gmsg->member_uin);
        s_free(gmsg->admin_uin);
        s_free(gmsg->admin);
        s_free(gmsg->msg);
    }
}
static void msg_offfile_free(void* opaque)
{
    LwqqMsgOffFile* of = opaque;
    if(of){
        s_free(of->rkey);
        s_free(of->name);
    }
}
static void msg_seq_free(LwqqMsg* msg)
{
    LwqqMsgSeq* seq = (LwqqMsgSeq*)msg;
    s_free(seq->from);
    s_free(seq->to);
}
/**
 * Free a LwqqMsg object
 * 
 * @param msg 
 */
void lwqq_msg_free(LwqqMsg *msg)
{
    if (!msg)
        return;

    if(msg->type&LWQQ_MF_SEQ)
        msg_seq_free(msg);

    int type = msg->type&LWQQ_MT_BITS;
    switch(type){
        case 0:
            break;
        case LWQQ_MT_MESSAGE:
            msg_message_free(msg);
            break;
        case LWQQ_MT_STATUS_CHANGE:
            msg_status_free(msg);
            break;
        case LWQQ_MT_KICK_MESSAGE:
            {
            LwqqMsgKickMessage* kick = (LwqqMsgKickMessage*)msg;
            s_free(kick->reason);
            }break;
        case LWQQ_MT_SYSTEM:
            msg_system_free(msg);
            break;
        case LWQQ_MT_BLIST_CHANGE:
            {
                LwqqMsgBlistChange* blist = (LwqqMsgBlistChange*)msg;
                LwqqBuddy* buddy;
                LwqqBuddy* next;
                LwqqSimpleBuddy* simple;
                LwqqSimpleBuddy* simple_next;
                simple = LIST_FIRST(&blist->added_friends);
                while(simple){
                    simple_next = LIST_NEXT(simple,entries);
                    lwqq_simple_buddy_free(simple);
                    simple = simple_next;
                }
                buddy = LIST_FIRST(&blist->removed_friends);
                while(buddy){
                    next = LIST_NEXT(buddy,entries);
                    lwqq_buddy_free(buddy);
                    buddy = next;
                }
            } break;
        case LWQQ_MT_SYS_G_MSG:
            msg_sys_g_msg_free(msg);
            break;
        case LWQQ_MT_OFFFILE:
            msg_offfile_free(msg);
            break;
        case LWQQ_MT_FILETRANS:
            {
                LwqqMsgFileTrans* trans = (LwqqMsgFileTrans*)msg;
                FileTransItem* item;
                FileTransItem* item_next;
                s_free(trans->lc_id);
                item = LIST_FIRST(&trans->file_infos);
                while(item!=NULL){
                    item_next = LIST_NEXT(item,entries);
                    s_free(item->file_name);
                    s_free(item);
                    item = item_next;
                }
            }
            break;
        case LWQQ_MT_FILE_MSG:
            {
                LwqqMsgFileMessage* file = (LwqqMsgFileMessage*)msg;
                s_free(file->reply_ip);
                if(file->mode == MODE_RECV){
                    s_free(file->recv.name);
                }
            }
            break;
        case LWQQ_MT_NOTIFY_OFFFILE:
            {
                LwqqMsgNotifyOfffile* notify = (LwqqMsgNotifyOfffile*)msg;
                s_free(notify->filename);
            }
            break;
        case LWQQ_MT_INPUT_NOTIFY:
            {
                LwqqMsgInputNotify * input = (LwqqMsgInputNotify*)msg;
                s_free(input->from);
                s_free(input->to);
            }
            break;
        case LWQQ_MT_SHAKE_MESSAGE:
            s_free(msg);
            break;
        default:
            lwqq_log(LOG_ERROR, "No such message type\n");
            break;
    }
    s_free(msg);
}

/**
 * Get the result object in a json object.
 * 
 * @param str
 * 
 * @return result object pointer on success, else NULL on failure.
 */
static json_t *get_result_json_object(json_t *json)
{
    json_t *json_tmp;
    char *value;

    /**
     * Frist, we parse retcode that indicate whether we get
     * correct response from server
     */
    value = json_parse_simple_value(json, "retcode");
    if (!value || strcmp(value, "0")) {
        goto failed ;
    }

    /**
     * Second, Check whether there is a "result" key in json object
     */
    json_tmp = json_find_first_label_all(json, "result");
    if (!json_tmp) {
        goto failed;
    }
    
    return json_tmp;

failed:
    return NULL;
}

static LwqqMsgType parse_recvmsg_type(json_t *json)
{
    LwqqMsgType type = LWQQ_MT_UNKNOWN;
    char *msg_type = json_parse_simple_value(json, "poll_type");
    if (!msg_type) {
        return type;
    }
    return lwqq__map_to_type_(msg_type_map, msg_type);
}

/**
 * {"poll_type":"message","value":{"msg_id":5244,"from_uin":570454553,
 * "to_uin":75396018,"msg_id2":395911,"msg_type":9,"reply_ip":176752041,
 * "time":1339663883,"content":[["font",{"size":10,"color":"000000",
 * "style":[0,0,0],"name":"\u5B8B\u4F53"}],"hello\n "]}}
 * 
 * @param json
 * @param opaque
 * 
 * @return
 */
static int parse_new_msg(json_t *json, LwqqMsg *opaque)
{
    LwqqMsgMessage *msg = (LwqqMsgMessage*)opaque;
    
    char* time = json_parse_simple_value(json, "time");
    time = time ?: "0";
    msg->time = (time_t)strtoll(time, NULL, 10);

    //if it failed means it is not group message.
    //so it equ NULL.
    if(opaque->type == LWQQ_MS_GROUP_MSG){
        msg->group.send = s_strdup(json_parse_simple_value(json, "send_uin"));
        msg->group.group_code = s_strdup(json_parse_simple_value(json,"group_code"));
    }else if(opaque->type == LWQQ_MS_SESS_MSG){
        msg->sess.id = s_strdup(json_parse_simple_value(json,"id"));
    }else if(opaque->type == LWQQ_MS_DISCU_MSG){
        msg->discu.send = s_strdup(json_parse_simple_value(json, "send_uin"));
        msg->discu.did = s_strdup(json_parse_simple_value(json,"did"));
    }

    if (parse_content(json,"content", msg)) {
        return -1;
    }

    return 0;
}

/**
 * {"poll_type":"buddies_status_change",
 * "value":{"uin":570454553,"status":"offline","client_type":1}}
 * 
 * @param json
 * @param opaque
 * 
 * @return 
 */
static int parse_status_change(json_t *json, LwqqMsg *opaque)
{
    LwqqMsgStatusChange *msg = (LwqqMsgStatusChange*)opaque;
    char *c_type;

    msg->who = s_strdup(json_parse_simple_value(json, "uin"));
    if (!msg->who) {
        return -1;
    }
    msg->status = s_strdup(json_parse_simple_value(json, "status"));
    if (!msg->status) {
        return -1;
    }
    c_type = json_parse_simple_value(json, "client_type");
    c_type = c_type ?: "1";
    msg->client_type = atoi(c_type);

    return 0;
}
static int parse_kick_message(json_t *json,void *opaque)
{
    LwqqMsgKickMessage *msg = opaque;
    char* show;
    show = json_parse_simple_value(json,"show_reason");
    if(show)msg->show_reason = atoi(show);
    msg->reason = ucs4toutf8(json_parse_simple_value(json,"reason"));
    if(!msg->reason){
        if(!show) msg->show_reason = 0;
        else return -1;
    }
    return 0;
}
/*
static void confirm_friend_request_notify(LwqqClient* lc,LwqqBuddy* buddy)
{
    LIST_INSERT_HEAD(&lc->friends,buddy,entries);
    lc->async_opt->request_confirm(lc,buddy);
}*/
static int parse_system_message(json_t *json,void* opaque,void* _lc)
{
    LwqqMsgSystem* system = opaque;
    //LwqqClient* lc = _lc;
    system->seq = s_strdup(json_parse_simple_value(json,"seq"));
    const char* type = json_parse_simple_value(json,"type");
    if(strcmp(type,"verify_required")==0) system->type = VERIFY_REQUIRED;
    else if(strcmp(type,"added_buddy_sig")==0) system->type = ADDED_BUDDY_SIG;
    else if(strcmp(type,"verify_pass_add")==0) system->type = VERIFY_PASS_ADD;
    else if(strcmp(type,"verify_pass")==0) system->type = VERIFY_PASS;
    else system->type = SYSTEM_TYPE_UNKNOW;

    if(system->type == SYSTEM_TYPE_UNKNOW) return 1;

    //system->from_uin = s_strdup(json_parse_simple_value(json,"from_uin"));
    system->account = s_strdup(json_parse_simple_value(json,"account"));
    system->stat = s_strdup(json_parse_simple_value(json,"stat"));
    system->client_type = s_strdup(json_parse_simple_value(json,"client_type"));
    if(system->type == VERIFY_REQUIRED){
        system->verify_required.msg = json_unescape(json_parse_simple_value(json,"msg"));
        system->verify_required.allow = s_strdup(json_parse_simple_value(json,"allow"));
    }else if(system->type==ADDED_BUDDY_SIG){
        system->added_buddy_sig.sig = json_unescape(json_parse_simple_value(json,"sig"));
    }else if(system->type==VERIFY_PASS||system->type==VERIFY_PASS_ADD){
        system->verify_pass.group_id = s_strdup(json_parse_simple_value(json,"group_id"));
        /*LwqqBuddy* buddy = lwqq_buddy_new();
        buddy->uin = s_strdup(system->from_uin);
        buddy->cate_index = s_strdup(system->verify_pass.group_id);

        LwqqAsyncEvset *set = lwqq_async_evset_new();
        LwqqAsyncEvent *ev;
        ev = lwqq_info_get_friend_detail_info(lc,buddy);
        lwqq_async_evset_add_event(set,ev);
        ev = lwqq_info_get_friend_qqnumber(lc,buddy);
        lwqq_async_evset_add_event(set,ev);
        lwqq_async_add_evset_listener(set,_C_(2p,confirm_friend_request_notify,lc,buddy));*/
    }
    return 0;
}
static int parse_blist_change(json_t* json,void* opaque,void* _lc)
{
    LwqqClient* lc = _lc;
    LwqqMsgBlistChange* change = opaque;
    LwqqBuddy* buddy;
    LwqqSimpleBuddy* simple;
    json_t* ptr = json_find_first_label_all(json,"added_friends");
    ptr = ptr->child->child;
    while(ptr!=NULL){
        simple = lwqq_simple_buddy_new();
        simple->uin = s_strdup(json_parse_simple_value(ptr,"uin"));
        simple->cate_index = s_strdup(json_parse_simple_value(ptr,"groupid"));
        LIST_INSERT_HEAD(&change->added_friends,simple,entries);
        buddy = lwqq_buddy_new();
        buddy->uin = s_strdup(json_parse_simple_value(ptr,"uin"));
        buddy->cate_index = s_atoi(json_parse_simple_value(ptr,"groupid"),0);
        LIST_INSERT_HEAD(&lc->friends,buddy,entries);
        //note in here we didn't trigger request_confirm
        //you should watch LwqqMsgBlistChange object and read 
        //simple buddy list.
        //and get qqnumber by your self.
        //lwqq_info_get_friend_detail_info(lc,buddy);
        ptr = ptr->next;
    }
    ptr = json_find_first_label_all(json,"removed_friends");
    ptr = ptr->child->child;
    while(ptr!=NULL){
        const char* uin = json_parse_simple_value(ptr,"uin");
        ptr = ptr->next;
        //we first put these guys to removed_friends
        //give upper level a chance do some thing.
        //after when blist delete. these guys would free as well.

        buddy = lwqq_buddy_find_buddy_by_uin(lc,uin);
        if(buddy == NULL) continue;
        LIST_REMOVE(buddy,entries);
        LIST_INSERT_HEAD(&change->removed_friends,buddy,entries);
    }
    return 0;
}
static int parse_sys_g_msg(json_t *json,void* opaque,LwqqClient* lc)
{
    /*group create
      {"retcode":0,"result":[{"poll_type":"sys_g_msg","value":{"msg_id":39194,"from_uin":1528509098,"to_uin":350512021,"msg_id2":539171,"msg_type":38,"reply_ip":176752410,"type":"group_create","gcode":2676780935,"t_gcode":249818602,"owner_uin":350512021}}]}

      group join
      {"retcode":0,"result":[{"poll_type":"sys_g_msg","value":{"msg_id":5940,"from_uin":370154409,"to_uin":2501542492,"msg_id2":979390,"msg_type":33,"reply_ip":176498394,"type":"group_join","gcode":2570026216,"t_gcode":249818602,"op_type":3,"new_member":2501542492,"t_new_member":"","admin_uin":1448380605,"admin_nick":"\u521B\u5EFA\u8005"}}]}

      group leave
      {"retcode":0,"result":[{"poll_type":"sys_g_msg","value":{"msg_id":51488,"from_uin":1528509098,"to_uin":350512021,"msg_id2":180256,"msg_type":34,"reply_ip":176882139,"type":"group_leave","gcode":2676780935,"t_gcode":249818602,"op_type":2,"old_member":574849996,"t_old_member":""}}]}
      group request
      {"retcode":0,"result":[{"poll_type":"sys_g_msg","value":{"msg_id":10899,"from_uin":3060007976,"to_uin":350512021,"msg_id2":693883,"msg_type":35,"reply_ip":176752016,"type":"group_request_join","gcode":406247342,"t_gcode":249818602,"request_uin":2297680537,"t_request_uin":"","msg":""}}]}
      group request join agree
      {"retcode":0,"result":[{"poll_type":"sys_g_msg","value":{"msg_id":29407,"from_uin":1735178063,"to_uin":2501542492,"msg_id2":28428,"msg_type":36,"reply_ip":176498075,"type":"group_request_join_agree","gcode":3557387121,"t_gcode":249818602,"admin_uin":4005533729,"msg":""}}]}
      group request join deny
      {"retcode":0,"result":[{"poll_type":"sys_g_msg","value":{"msg_id":1253,"from_uin":1735178063,"to_uin":2501542492,"msg_id2":93655,"msg_type":37,"reply_ip":176622059,"type":"group_request_join_deny","gcode":3557387121,"t_gcode":249818602,"admin_uin":4005533729,"msg":"123"}}]}

      */
    LwqqMsgSysGMsg* msg = opaque;
    const char* type = json_parse_simple_value(json,"type");
    msg->group_uin = s_strdup(json_parse_simple_value(json,"from_uin"));
    msg->gcode = s_strdup(json_parse_simple_value(json,"gcode"));
    msg->account = s_strdup(json_parse_simple_value(json,"t_gcode"));
    int add_new_group = 0;
    if(strcmp(type,"group_create")==0)msg->type = GROUP_CREATE;
    else if(strcmp(type,"group_join")==0){
        msg->type = GROUP_JOIN;
        msg->member_uin = s_strdup(json_parse_simple_value(json,"new_member"));
        msg->member = json_unescape(json_parse_simple_value(json,"t_new_member"));
        msg->admin_uin = s_strdup(json_parse_simple_value(json,"admin_uin"));
        msg->admin = json_unescape(json_parse_simple_value(json, "admin_nick"));
        add_new_group = strcmp(msg->member_uin,lc->myself->uin)==0;
    }
    else if(strcmp(type,"group_leave")==0){
        msg->type = GROUP_LEAVE;
        msg->member_uin = s_strdup(json_parse_simple_value(json,"old_member"));
        msg->member = json_unescape(json_parse_simple_value(json,"t_old_member"));
        msg->group = lwqq_group_find_group_by_gid(lc, msg->group_uin);
        msg->is_myself = strcmp(lc->myself->uin,msg->member_uin)==0;
        if(msg->is_myself)
            LIST_REMOVE(msg->group,entries);
    }
    else if(strcmp(type,"group_request_join")==0){
        msg->type = GROUP_REQUEST_JOIN;
        msg->member_uin = s_strdup(json_parse_simple_value(json,"request_uin"));
        msg->member = json_unescape(json_parse_simple_value(json,"t_request_uin"));
        msg->msg = json_unescape(json_parse_simple_value(json, "msg"));
    }else if(strcmp(type,"group_request_join_agree")==0){
        msg->type = GROUP_REQUEST_JOIN_AGREE;
        if(!msg->group)
            msg->group = lwqq_group_find_group_by_gid(lc, msg->group_uin);
        add_new_group = 1;
    }else if(strcmp(type,"group_request_join_deny")==0){
        msg->type = GROUP_REQUEST_JOIN_DENY;
        if(!msg->group)
            msg->group = lwqq_group_find_group_by_gid(lc, msg->group_uin);
        msg->msg = json_unescape(json_parse_simple_value(json, "msg"));
    }
    else msg->type = GROUP_UNKNOW;
    if(add_new_group){
        msg->is_myself = 1;
        LwqqGroup* g = lwqq_group_new(LWQQ_GROUP_QUN);
        g->account = s_strdup(msg->account);
        g->code = s_strdup(msg->gcode);
        g->gid = s_strdup(msg->group_uin);
        msg->group = g;
        LIST_INSERT_HEAD(&lc->groups,g,entries);
        LwqqAsyncEvent* ev = lwqq_info_get_group_public(lc,g);
        lwqq_async_add_event_listener(ev, _C_(2p,insert_msg_delay_by_request_content,lc->msg_list,msg));
        return RET_DELAY_INS;
    }
    return 0;
}
static int parse_push_offfile(json_t* json,void* opaque)
{
    LwqqMsgOffFile * off = opaque;
    off->rkey = s_strdup(json_parse_simple_value(json,"rkey"));
    strncpy(off->ip,json_parse_simple_value(json,"ip"),24);
    strncpy(off->port,json_parse_simple_value(json,"port"),8);
    off->size = atol(json_parse_simple_value(json,"size"));
    off->name = json_unescape(json_parse_simple_value(json,"name"));
    off->expire_time = atol(json_parse_simple_value(json,"expire_time"));
    off->time = atol(json_parse_simple_value(json,"time"));
    return 0;
}
static int parse_file_transfer(json_t* json,void* opaque)
{
    LwqqMsgFileTrans* trans = opaque;
    trans->file_count = atoi(json_parse_simple_value(json,"file_count"));
    trans->lc_id = s_strdup(json_parse_simple_value(json,"lc_id"));
    trans->now = atol(json_parse_simple_value(json,"now"));
    trans->operation = atoi(json_parse_simple_value(json,"operation"));
    trans->type = atoi(json_parse_simple_value(json,"type"));
    json_t* ptr = json_find_first_label_all(json,"file_infos");
    ptr = ptr->child->child;
    while(ptr!=NULL){
        FileTransItem *item = s_malloc0(sizeof(*item));
        item->file_name = json_unescape(json_parse_simple_value(ptr,"file_name"));
        item->file_status = atoi(json_parse_simple_value(ptr,"file_status"));
        item->pro_id = atoi(json_parse_simple_value(ptr,"pro_id"));
        LIST_INSERT_HEAD(&trans->file_infos,item,entries);
        ptr = ptr->next;
    }
    return 0;
}
static int parse_file_message(json_t* json,void* opaque)
{
    LwqqMsgFileMessage* file = opaque;
    file->msg_id = atoi(json_parse_simple_value(json,"msg_id"));
    const char* mode = json_parse_simple_value(json,"mode");
    if(strcmp(mode,"recv")==0) file->mode = MODE_RECV;
    else if(strcmp(mode,"refuse")==0) file->mode = MODE_REFUSE;
    else if(strcmp(mode,"send_ack")==0) file->mode = MODE_SEND_ACK;
    file->reply_ip = s_strdup(json_parse_simple_value(json,"reply_ip"));
    file->type = atoi(json_parse_simple_value(json,"type"));
    file->time = atol(json_parse_simple_value(json,"time"));
    file->session_id = atoi(json_parse_simple_value(json,"session_id"));
    switch(file->mode){
        case MODE_RECV:
            file->recv.msg_type = atoi(json_parse_simple_value(json,"msg_type"));
            file->recv.name = json_unescape(json_parse_simple_value(json,"name"));
            file->recv.inet_ip = atoi(json_parse_simple_value(json,"inet_ip"));
            break;
        case MODE_REFUSE:
            file->refuse.cancel_type = atoi(json_parse_simple_value(json,"cancel_type"));
            break;
        default:break;
    }
    return 0;
}
static int parse_notify_offfile(json_t* json,void* opaque)
{
    /*
     * {"retcode":0,"result":[{"poll_type":"notify_offfile","value":
     * {"msg_id":9948,"from_uin":495653555,"to_uin":350512021,"msg_id2":460591,"msg_type":9,"reply_ip":178847911,"action":2,"filename":"12.jpg","filesize":137972}}]}
    */
    LwqqMsgNotifyOfffile* notify = opaque;
    notify->action = atoi(json_parse_simple_value(json,"action"));
    notify->filename = json_unescape(json_parse_simple_value(json,"filename"));
    notify->filesize = strtoul(json_parse_simple_value(json,"filesize"), NULL, 10);
    return 0;
}

static int parse_input_notify(json_t* json,void* opaque)
{
    /**
     *  {"retcode":0,"result":[{"poll_type":"input_notify","value":
     *  {"msg_id":21940,"from_uin":3228283951,"to_uin":350512021,"msg_id2":3588813984,"msg_type":121,"reply_ip":4294967295}
     *  }]}
     *
     */
    LwqqMsgInputNotify* input = opaque;
    input->from = s_strdup(json_parse_simple_value(json,"from_uin"));
    input->to = s_strdup(json_parse_simple_value(json,"to_uin"));
    return 0;
}
static int parse_shake_message(json_t* json,void* opaque)
{
    /**
     * {"retcode":0,"result":[{"poll_type":"shake_message","value":
     * {"msg_id":568,"from_uin":2822367047,"to_uin":2501542492,"msg_id2":405490,"msg_type":9,"reply_ip":176498151}}]}
     */
    LwqqMsgShakeMessage* shake = opaque;
    shake->reply_ip = strtoul(json_parse_simple_value(json, "reply_ip"), NULL, 10);
    return 0;
}
const char* get_host_of_url(const char* url,char* buffer)
{
    const char* ptr;
    const char* end;
    ptr = strstr(url,"://");
    if (ptr == NULL) return NULL;
    ptr+=3;
    end = strstr(ptr,"/");
    if(end == NULL)
        strcpy(buffer,ptr);
    else
        strncpy(buffer,ptr,end-ptr);
    return buffer;
}
static int set_content_picture_data(LwqqHttpRequest* req,LwqqMsgContent* c)
{
    int errno = 0;
    if((req->http_code!=200)){
        errno = LWQQ_EC_HTTP_ERROR;
        goto done;
    }
    switch(c->type){
        case LWQQ_CONTENT_OFFPIC:
            c->data.img.data = req->response;
            c->data.img.size = req->resp_len;
        break;
        case LWQQ_CONTENT_CFACE:
            c->data.cface.data = req->response;
            c->data.cface.size = req->resp_len;
        break;
        default:
        break;
    }
    req->response = NULL;
done:
    lwqq_http_request_free(req);
    return errno;
}
static LwqqAsyncEvent* request_content_offpic(LwqqClient* lc,const char* f_uin,LwqqMsgContent* c)
{
    LwqqHttpRequest* req;
    LwqqErrorCode error;
    LwqqErrorCode *err = &error;
    char url[512];
    char *file_path = url_encode(c->data.img.file_path);
    //there are face 1 to face 10 server to accelerate speed.
    snprintf(url, sizeof(url),
             "%s/get_offpic2?file_path=%s&f_uin=%s&clientid=%s&psessionid=%s",
             "http://d.web2.qq.com/channel",
             file_path,f_uin,lc->clientid,lc->psessionid);
    s_free(file_path);
    req = lwqq_http_create_default_request(lc,url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://web2.qq.com/");
    req->set_header(req,"Host","d.web2.qq.com");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));

    return req->do_request_async(req, 0, NULL,_C_(2p_i,set_content_picture_data,req,c));
done:
    lwqq_http_request_free(req);
    return NULL;
}
static LwqqAsyncEvent* request_content_cface(LwqqClient* lc,const char* group_code,const char* send_uin,LwqqMsgContent* c)
{
    LwqqHttpRequest* req;
    LwqqErrorCode error;
    LwqqErrorCode *err = &error;
    char url[512];
/*http://web2.qq.com/cgi-bin/get_group_pic?type=0&gid=3971957129&uin=4174682545&rip=120.196.211.216&rport=9072&fid=2857831080&pic=71A8E53B7F678D035656FECDA1BD7F31.jpg&vfwebqq=762a8682d17931d0cc647515e570435bd82e3a4e957bd052faa9615192eb7a3c4f1719006a7176c1&t=1343130567*/
    snprintf(url, sizeof(url),
             "%s/get_group_pic?type=0&gid=%s&uin=%s&rip=%s&rport=%s&fid=%s&pic=%s&vfwebqq=%s&t=%ld",
             "http://web2.qq.com/cgi-bin",
             group_code,send_uin,c->data.cface.serv_ip,c->data.cface.serv_port,
             c->data.cface.file_id,c->data.cface.name,lc->vfwebqq,time(NULL));
    req = lwqq_http_create_default_request(lc,url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://web2.qq.com/");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));

    return req->do_request_async(req,0,NULL,_C_(2p_i,set_content_picture_data,req,c));
done:
    lwqq_http_request_free(req);
    return NULL;
}
static LwqqAsyncEvent* request_content_cface2(LwqqClient* lc,int msg_id,const char* from_uin,LwqqMsgContent* c)
{
    LwqqHttpRequest* req;
    LwqqErrorCode error;
    LwqqErrorCode *err = &error;
    char url[1024];
/*http://d.web2.qq.com/channel/get_cface2?lcid=3588&guid=85930B6CCE38BDAEF176FA83F0491569.jpg&to=2217604723&count=5&time=1&clientid=6325200&psessionid=8368046764001d636f6e6e7365727665725f77656271714031302e3133342e362e31333800001c9b000000d8026e04009563e4146d0000000a403946423664616232666d00000028ceb438eb76f1bc88360fc303e9148cc5dac8652a7a4bb702ee6dcf9bb10adf571a48b8a76b599e44*/
    snprintf(url, sizeof(url),
             "%s/channel/get_cface2?lcid=%d&to=%s&guid=%s&count=5&time=1&clientid=%s&psessionid=%s",
             "http://d.web2.qq.com",
             msg_id,from_uin,c->data.cface.name,lc->clientid,lc->psessionid);
    req = lwqq_http_create_default_request(lc,url, err);
    lwqq_verbose(3,"%s\n",url);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://web2.qq.com/");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));

    return req->do_request_async(req,0,NULL,_C_(2p_i,set_content_picture_data,req,c));
done:
    lwqq_http_request_free(req);
    return NULL;
}
static LwqqAsyncEvset* lwqq_msg_request_picture(LwqqClient* lc,LwqqMsgMessage* msg)
{
    LwqqMsgContent* c;
    LwqqAsyncEvset* ret = NULL;
    LwqqAsyncEvent* event;
    TAILQ_FOREACH(c,&msg->content,entries){
        if(c->type == LWQQ_CONTENT_OFFPIC){
            if(ret == NULL) ret = lwqq_async_evset_new();
            event = request_content_offpic(lc,msg->super.from,c);
            lwqq_async_evset_add_event(ret,event);
        }else if(c->type == LWQQ_CONTENT_CFACE){
            if(ret == NULL) ret = lwqq_async_evset_new();
            if(msg->super.super.type == LWQQ_MS_BUDDY_MSG)
                event = request_content_cface2(lc,msg->super.msg_id,msg->super.from,c);
            else
                event = request_content_cface(lc,msg->group.group_code,msg->group.send,c);
            lwqq_async_evset_add_event(ret,event);
        }
    }
    return ret;
}
static int parse_msg_seq(json_t* json,LwqqMsg* msg)
{
    LwqqMsgSeq* seq = (LwqqMsgSeq*)msg;
    seq->from = s_strdup(json_parse_simple_value(json,"from_uin"));
    seq->to = s_strdup(json_parse_simple_value(json,"to_uin"));
    seq->msg_id = s_atoi(json_parse_simple_value(json,"msg_id"),0);
    seq->msg_id2 = s_atoi(json_parse_simple_value(json,"msg_id2"),0);
    return 0;
}
/**
 * Parse message received from server
 * Buddy message:
 * {"retcode":0,"result":[{"poll_type":"message","value":{"msg_id":5244,"from_uin":570454553,"to_uin":75396018,"msg_id2":395911,"msg_type":9,"reply_ip":176752041,"time":1339663883,"content":[["font",{"size":10,"color":"000000","style":[0,0,0],"name":"\u5B8B\u4F53"}],"hello\n "]}}]}
 * 
 * Message for Changing online status:
 * {"retcode":0,"result":[{"poll_type":"buddies_status_change","value":{"uin":570454553,"status":"offline","client_type":1}}]}
 * 
 * 
 * @param list 
 * @param response 
 */
static int parse_recvmsg_from_json(LwqqRecvMsgList *list, const char *str)
{
    int ret;
    WebqqRetCode retcode = 0;
    json_t *json = NULL, *json_tmp, *cur;

    ret = json_parse_document(&json, (char *)str);

    char* dbg_str = json_unescape((char*)str);
    lwqq_verbose(2,"[%s]%s\n",TIME_,dbg_str);
    s_free(dbg_str);
    
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of friends error: %s\n", str);
        goto done;
    }
    const char* retcode_str = json_parse_simple_value(json,"retcode");
    if(retcode_str)
        retcode = atoi(retcode_str);

    if(retcode == WEBQQ_NEW_PTVALUE){
        char * pt = json_parse_simple_value(json, "p");
        LwqqClient* lc = list->lc;
        s_free(lc->new_ptwebqq);
        lc->new_ptwebqq = s_strdup(pt);
    }
    if(retcode != WEBQQ_OK) goto done;

    json_tmp = get_result_json_object(json);
    if (!json_tmp) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", str);
        goto done;
    }

    if (!json_tmp->child || !json_tmp->child->child) {
        goto done;
    }

    /* make json_tmp point to first child of "result" */
    json_tmp = json_tmp->child->child_end;
    for (cur = json_tmp; cur != NULL; cur = cur->previous) {
        LwqqMsg *msg = NULL;
        LwqqMsgType msg_type;
        int ret;
        
        msg_type = parse_recvmsg_type(cur);
        msg = lwqq_msg_new(msg_type);
        LwqqAsyncEvset* ev;
        if (!msg) {
            continue;
        }
        if(msg_type&LWQQ_MF_SEQ){
            parse_msg_seq(cur,msg);
        }
        switch(msg_type&LWQQ_MT_BITS){
            case LWQQ_MT_MESSAGE:
                ret = parse_new_msg(cur,msg);
                ev = lwqq_msg_request_picture(list->lc, (LwqqMsgMessage*)msg);
                if(ev){
                    ret = -1;
                    lwqq_async_add_evset_listener(ev,_C_(2p,insert_msg_delay_by_request_content,list,msg));
                    //this jump the case
                    continue;
                }
                break;
            case LWQQ_MT_STATUS_CHANGE:
                ret = parse_status_change(cur, msg);
                break;
            case LWQQ_MT_KICK_MESSAGE:
                ret = parse_kick_message(cur,msg);
                break;
            case LWQQ_MT_SYSTEM:
                ret = parse_system_message(cur,msg,list->lc);
                break;
            case LWQQ_MT_BLIST_CHANGE:
                ret = parse_blist_change(cur,msg,list->lc);
                break;
            case LWQQ_MT_SYS_G_MSG:
                ret = parse_sys_g_msg(cur,msg,list->lc);
                break;
            case LWQQ_MT_OFFFILE:
                ret = parse_push_offfile(cur,msg);
                break;
            case LWQQ_MT_FILETRANS:
                ret = parse_file_transfer(cur,msg);
                break;
            case LWQQ_MT_FILE_MSG:
                ret = parse_file_message(cur,msg);
                break;
            case LWQQ_MT_NOTIFY_OFFFILE:
                ret = parse_notify_offfile(cur,msg);
                break;
            case LWQQ_MT_INPUT_NOTIFY:
                ret = parse_input_notify(cur,msg);
                break;
            case LWQQ_MT_SHAKE_MESSAGE:
                ret = parse_shake_message(cur,msg);
                break;
            default:
                ret = -1;
                lwqq_log(LOG_ERROR, "No such message type\n");
                break;
        }

        if (ret == RET_INSERT_MSG) {
            insert_recv_msg_with_order(list,msg);
        } else if(ret == RET_BAD_MSG){
            lwqq_msg_free(msg);
        }
    }
    
done:
    if (json) {
        json_free_value(&json);
    }
    return retcode;
}

static void insert_recv_msg_with_order(LwqqRecvMsgList* list,LwqqMsg* msg)
{
    LwqqRecvMsg *rmsg = s_malloc0(sizeof(*rmsg));
    rmsg->msg = msg;
    LwqqRecvMsg *iter;
    /* Parse a new message successfully, link it to our list */
    pthread_mutex_lock(&list->mutex);
    //sort the order for messages.
    if((msg->type & LWQQ_MT_BITS) ==  LWQQ_MT_MESSAGE){
        int id2 = ((LwqqMsgSeq*)msg)->msg_id2;
        int inserted = 0;
        TAILQ_FOREACH_REVERSE(iter,&list->head,RecvMsgListHead,entries){
            if((iter->msg->type&LWQQ_MT_BITS)!=LWQQ_MT_MESSAGE)
                continue;
            LwqqMsgSeq* iter_msg = (LwqqMsgSeq*)iter->msg;
            if(iter_msg->msg_id2<id2){
                TAILQ_INSERT_AFTER(&list->head,iter,rmsg,entries);
                inserted = 1;
                break;
            }else if(iter_msg->msg_id2==id2){
                //this is duplicated message. we destroy it.
                inserted = 1;
                lwqq_msg_free(msg);
                break;
            }
        }
        if(!inserted){
            TAILQ_INSERT_HEAD(&list->head,rmsg,entries);
        }
    }else{
        TAILQ_INSERT_TAIL(&list->head, rmsg, entries);
    }
    pthread_mutex_unlock(&list->mutex);
}

#if ! USE_MSG_THREAD
static int _continue_poll(LwqqHttpRequest* req,void* data)
{
    void **d = data;
    LwqqRecvMsgList* list = d[0];
    char* msg = d[1];
    LwqqClient* lc = list->lc;

    if(req == LWQQ_CALLBACK_FAILED){
        s_free(msg);
        s_free(data);
        return -1;
    }

    int retcode;
    if(req->http_code==200){
        retcode = parse_recvmsg_from_json(list, req->response);
        if(retcode == 121 || retcode == 108){
            lc->async_opt->poll_lost(lc);
            return 0;
        }else{
            lc->async_opt->poll_msg(lc);
        }
    }

    req->do_request_async(req,1,msg,_continue_poll,data);
    return 0;
}
#endif

static int poll_progress(void * data,size_t now,size_t total)
{
    LwqqRecvMsgListInternal* list = data;
    return list->on_quit;
}
/**
 * Poll to receive message.
 * 
 * @param list
 */
static void *start_poll_msg(void *msg_list)
{
    LwqqClient *lc;
    LwqqHttpRequest *req = NULL;  
    char *s;
    char msg[1024];
    LwqqRecvMsgList *list;

    list = (LwqqRecvMsgList *)msg_list;
    lc = (LwqqClient *)(list->lc);
    if (!lc) {
        goto failed;
    }
    snprintf(msg, sizeof(msg), "{\"clientid\":\"%s\",\"psessionid\":\"%s\"}",
             lc->clientid, lc->psessionid);
    s = url_encode(msg);
    snprintf(msg, sizeof(msg), "r=%s", s);
    s_free(s);

    /* Create a POST request */
    char url[512];
    snprintf(url, sizeof(url), "%s/channel/poll2", "http://d.web2.qq.com");
    req = lwqq_http_create_default_request(lc,url, NULL);
    if (!req) {
        goto failed;
    }
    req->set_header(req, "Referer", "http://d.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    //long poll timeout is 90s.official value
    lwqq_http_set_option(req, LWQQ_HTTP_TIMEOUT,90);

#if USE_MSG_THREAD
    int retcode;
    int ret;
    lwqq_http_on_progress(req, poll_progress, list);
    while(1) {
        ret = req->do_request(req, 1, msg);
        if(ret != 0){
            lwqq_verbose(2,"poll_msg:err:%d\n",ret);
        }
        if(!lwqq_client_logined(lc)) break;
        if(ret == LWQQ_EC_TIMEOUT_OVER){
            lc->dispatch(vp_func_p,(CALLBACK_FUNC)lc->async_opt->poll_lost,lc);
            break;
        }

        if (ret || req->http_code != 200) continue;
        retcode = parse_recvmsg_from_json(list, req->response);
        if(!lwqq_client_logined(lc)) break;
        switch(retcode){
            case WEBQQ_OK:
                lc->dispatch(vp_func_p,(CALLBACK_FUNC)lc->async_opt->poll_msg,lc);
                break;
            case WEBQQ_NO_MESSAGE:
                continue;
                break;
            case 109:
            case WEBQQ_LOST_CONN:
                lc->dispatch(vp_func_p,(CALLBACK_FUNC)lc->async_opt->poll_lost,lc);
                break;
            case WEBQQ_NEW_PTVALUE:
                //just need do some things when relogin
                break;
        }
    }
    lwqq_puts("quit the msg_thread");
failed:
    if(req) lwqq_http_request_free(req);
    return NULL;
#else
    void ** data = s_malloc0(sizeof(void*)*2);
    data[0] = list;
    data[1] = strdup(msg);
    req->do_request_async(req,1,msg,_continue_poll,data);
    return NULL;
failed:
    if(req) lwqq_http_request_free(req);
    return NULL;
#endif
}

static void lwqq_recvmsg_poll_msg(LwqqRecvMsgList *list)
{
#if USE_MSG_THREAD
    LwqqRecvMsgListInternal* internal = (LwqqRecvMsgListInternal*)list;

    pthread_create(&internal->tid, NULL/*&list->attr*/, start_poll_msg, list);
#else
    start_poll_msg(list);
#endif
}

static void lwqq_recvmsg_poll_close(LwqqRecvMsgList* list)
{
    if(!list) return;
    LwqqRecvMsgListInternal* internal = (LwqqRecvMsgListInternal*)list;
    if(internal->tid == 0) return;
    internal->on_quit = 1;
    pthread_join(internal->tid,NULL);
    internal->on_quit = 0;
    internal->tid = 0;
}

///low level special char mapping
static void parse_unescape(char* source,char *buf,int buf_len)
{
    char* ptr = source;
    size_t idx;
    while(*ptr!='\0'){
        if(buf_len<=0) return;
        idx = strcspn(ptr,"\n\t\\;&\"+");
        if(ptr[idx] == '\0'){
            strncpy(buf,ptr,buf_len);
            buf+=idx;
            buf_len-=idx;
            break;
        }
        strncpy(buf,ptr,(idx<buf_len)?idx:buf_len);
        buf+=idx;
        buf_len-=idx;
        if(buf_len<=0) return;
        switch(ptr[idx]){
            //note buf point the end position
            case '\n': strncpy(buf,"\\\\n",buf_len);break;
            case '\t': strncpy(buf,"\\\\t",buf_len);break;
            case '\\': strncpy(buf,"\\\\\\\\",buf_len);break;
            //i dont know why ; is not worked.so we use another expression
            case ';' : strncpy(buf,"\\u003B",buf_len);break;
            case '&' : strncpy(buf,"\\u0026",buf_len);break;
            case '"' : strncpy(buf,"\\\\\\\"",buf_len);break;
            case '+' : strncpy(buf,"\\u002B",buf_len);break;
        }
        ptr+=idx+1;
        idx=strlen(buf);
        buf+=idx;
        buf_len-=idx;
    }
    *buf = '\0';
}
#define LEFT "\\\""
#define RIGHT "\\\""
#define KEY(key) "\\\""key"\\\""

static char* content_parse_string(LwqqMsgMessage* msg,int msg_type,int *has_cface)
{
    //not thread safe. 
    //you need ensure only one thread send msg in one time.
    static char buf[8192];
    strcpy(buf,"\"[");
    LwqqMsgContent* c;

    TAILQ_FOREACH(c,&msg->content,entries){
        switch(c->type){
            case LWQQ_CONTENT_FACE:
                format_append(buf,"["KEY("face")",%d],",c->data.face);
                break;
            case LWQQ_CONTENT_OFFPIC:
                format_append(buf,"["KEY("offpic")","KEY("%s")","KEY("%s")",%ld],",
                        c->data.img.file_path,
                        c->data.img.name,
                        c->data.img.size);
                break;
            case LWQQ_CONTENT_CFACE:
                //[\"cface\",\"group\",\"0C3AED06704CA9381EDCC20B7F552802.jPg\"]
                if(msg_type == LWQQ_MS_GROUP_MSG)
                    format_append(buf,"["KEY("cface")","KEY("group")","KEY("%s")"],",
                            c->data.cface.name);
                else if(msg_type == LWQQ_MS_BUDDY_MSG || msg_type == LWQQ_MS_SESS_MSG)
                    format_append(buf,"["KEY("cface")","KEY("%s")"],",
                            c->data.cface.name);
                *has_cface = 1;
                break;
            case LWQQ_CONTENT_STRING:
                strcat(buf,LEFT);
                parse_unescape(c->data.str,buf+strlen(buf),sizeof(buf)-strlen(buf));
                strcat(buf,RIGHT",");
                break;
        }
    }
    snprintf(buf+strlen(buf),sizeof(buf)-strlen(buf),
            "["KEY("font")",{"
            KEY("name")":"KEY("%s")","
            KEY("size")":"KEY("%d")","
            KEY("style")":[%d,%d,%d],"
            KEY("color")":"KEY("%s")
            "}]]\"",
            msg->f_name,msg->f_size,
            msg->f_style.b,msg->f_style.i,msg->f_style.u,
            msg->f_color);
    return buf;
}

static LwqqAsyncEvent* lwqq_msg_upload_offline_pic(
        LwqqClient* lc,LwqqMsgContent* c,const char* to)
{
    if(c->type != LWQQ_CONTENT_OFFPIC) return NULL;
    if(!c->data.img.data || !c->data.img.name || !c->data.img.size) return NULL;

    LwqqHttpRequest *req;
    LwqqErrorCode err;
    const char* filename = c->data.img.name;
    const char* buffer = c->data.img.data;
    static int fileid = 1;
    size_t size = c->data.img.size;
    char url[512];
    char piece[22];

    snprintf(url,sizeof(url),"http://weboffline.ftn.qq.com/ftn_access/upload_offline_pic?time=%ld",
            time(NULL));
    req = lwqq_http_create_default_request(lc,url,&err);
    req->set_header(req,"Origin","http://web2.qq.com");
    req->set_header(req,"Referer","http://web2.qq.com/");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Cache-Control","max-age=0");

    req->add_form(req,LWQQ_FORM_CONTENT,"callback","parent.EQQ.Model.ChatMsg.callbackSendPic");
    req->add_form(req,LWQQ_FORM_CONTENT,"locallangid","2052");
    req->add_form(req,LWQQ_FORM_CONTENT,"clientversion","1409");
    req->add_form(req,LWQQ_FORM_CONTENT,"uin",lc->username);///<this may error
    req->add_form(req,LWQQ_FORM_CONTENT,"skey",lc->cookies->skey);
    req->add_form(req,LWQQ_FORM_CONTENT,"appid","1002101");
    req->add_form(req,LWQQ_FORM_CONTENT,"peeruin",to);///<what this means?
    req->add_file_content(req,"file",filename,buffer,size,NULL);
    snprintf(piece,sizeof(piece),"%d",fileid++);
    req->add_form(req,LWQQ_FORM_CONTENT,"fileid",piece);
    req->add_form(req,LWQQ_FORM_CONTENT,"vfwebqq",lc->vfwebqq);
    req->add_form(req,LWQQ_FORM_CONTENT,"senderviplevel","0");
    req->add_form(req,LWQQ_FORM_CONTENT,"reciverviplevel","0");

    return req->do_request_async(req,0,NULL,_C_(3p_i,upload_offline_pic_back,req,c,to));
}
static int upload_offline_pic_back(LwqqHttpRequest* req,LwqqMsgContent* c,const char* to)
{
    json_t* json = NULL;
    if(req->http_code!=200){
        goto done;
    }
    lwqq_verbose(3,"%s\n",req->response);

    char *end = strchr(req->response,'}');
    *(end+1) = '\0';
    json_parse_document(&json,strchr(req->response,'{'));
    if(strcmp(json_parse_simple_value(json,"retcode"),"0")!=0){
        goto done;
    }
    c->type = LWQQ_CONTENT_OFFPIC;
    c->data.img.size = atol(json_parse_simple_value(json,"filesize"));
    c->data.img.file_path = s_strdup(json_parse_simple_value(json,"filepath"));
    if(!strcmp(c->data.img.file_path,"")){
        LwqqClient* lc = req->lc;
        lc->async_opt->upload_fail(lc,to,c);
    }
    s_free(c->data.img.name);
    c->data.img.name = s_strdup(json_parse_simple_value(json,"filename"));
    s_free(c->data.img.data);
done:
    if(json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return 0;
}
static int process_gface_sig(LwqqHttpRequest* req)
{
    int err = 0;
    json_t* json = NULL;
    lwqq__jump_if_http_fail(req,err);
    lwqq__jump_if_json_fail(json,req->response,err);
    LwqqClient* lc = req->lc;

    lc->gface_key = s_strdup(json_parse_simple_value(json,"gface_key"));
    lc->gface_sig = s_strdup(json_parse_simple_value(json,"gface_sig"));
    
done:
    lwqq__clean_json_and_req(json,req);
    return err;
}
static LwqqAsyncEvent* query_gface_sig(LwqqClient* lc)
{
    if(!lc||(lc->gface_key&&lc->gface_sig)){
        return NULL;
    }
    LwqqErrorCode err;
    char url[512];

    snprintf(url,sizeof(url),"%s/get_gface_sig2?clientid=%s&psessionid=%s&t=%ld",
            "https://d.web2.qq.com/channel",lc->clientid,lc->psessionid,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,&err);
    req->set_header(req,"Referer","https://d.web2.qq.com/cfproxy.html?v=20110331002&callback=1");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));

    return req->do_request_async(req,0,NULL,_C_(p,process_gface_sig,req));
}

static LwqqAsyncEvent* lwqq_msg_upload_cface(
        LwqqClient* lc,LwqqMsgContent* c,LwqqMsgType type)
{
    if(c->type != LWQQ_CONTENT_CFACE) return NULL;
    if(!c->data.cface.name || !c->data.cface.data || !c->data.cface.size) return NULL;
    const char *filename = c->data.cface.name;
    const char *buffer = c->data.cface.data;
    size_t size = c->data.cface.size;
    LwqqHttpRequest *req;
    LwqqErrorCode err;
    char url[512];
    static int fileid = 1;
    char fileid_str[20];

    snprintf(url,sizeof(url),"http://up.web2.qq.com/cgi-bin/cface_upload?time=%ld",
            time(NULL));
    req = lwqq_http_create_default_request(lc,url,&err);
    //curl_easy_setopt(req->req,CURLOPT_VERBOSE,1);
    req->set_header(req,"Origin","http://web2.qq.com");
    req->set_header(req,"Referer","http://web2.qq.com/");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));

    req->add_form(req,LWQQ_FORM_CONTENT,"vfwebqq",lc->vfwebqq);
    //this is special for group msg.it can upload over 250K
    req->add_form(req,LWQQ_FORM_CONTENT,"from","control");
    if(type == LWQQ_MS_GROUP_MSG){
        req->add_form(req,LWQQ_FORM_CONTENT,"f","EQQ.Model.ChatMsg.callbackSendPicGroup");
    } else if(type == LWQQ_MS_BUDDY_MSG){
        req->add_form(req,LWQQ_FORM_CONTENT,"f","EQQ.Model.ChatMsg.callbackSendPic");
    }
    req->add_file_content(req,"custom_face",filename,buffer,size,NULL);
    snprintf(fileid_str,sizeof(fileid_str),"%d",fileid++);
    //cface 上传是会占用自定义表情的空间的.这里的fileid是几就是占用第几个格子.
    req->add_form(req,LWQQ_FORM_CONTENT,"fileid","1");

    /*void **data = s_malloc0(sizeof(void*)*2);
    data[0] = lc;
    data[1] = c;*/
    return req->do_request_async(req,0,NULL,_C_(3p_i,upload_cface_back,req,lc,c));
}
static int upload_cface_back(LwqqHttpRequest *req,LwqqClient* lc,LwqqMsgContent* c)
{
    int ret;
    int errno = 0;
    char msg[256];

    lwqq_verbose(3,"%s\n",req->response);
    if(req->http_code!=200){
        errno = 1;
        goto done;
    }
    char* ptr = strstr(req->response,"({");
    if(ptr==NULL){
        errno = 1;
        goto done;
    }
    sscanf(ptr,"({'ret':%d,'msg':'%[^\']'",&ret,msg);
    if(ret !=0 && ret !=4){
        errno = 1;
        goto done;
    }
    c->type = LWQQ_CONTENT_CFACE;
    char *file = msg;
    char *pos;
    //force to cut down the filename
    if((pos = strchr(file,' '))){
        *pos = '\0';
    }
    s_free(c->data.cface.name);
    s_free(c->data.cface.data);
    c->data.cface.name = s_strdup(file);
    c->data.cface.size = 0;

done:
    lwqq_http_request_free(req);
    return errno;
}

void lwqq_msg_send_continue(LwqqClient* lc,LwqqMsgMessage* msg,LwqqAsyncEvent* event)
{
    LwqqAsyncEvent* ret = lwqq_msg_send(lc,msg);
    lwqq_async_add_event_chain(ret, event);
}
/** 
 * 
 * 
 * @param lc 
 * @param sendmsg 
 * @note sess message can not send picture
 * 
 * @return 1 means ok
 *         0 means failed or send failed
 */
LwqqAsyncEvent* lwqq_msg_send(LwqqClient *lc, LwqqMsgMessage *msg)
{
    LwqqHttpRequest *req = NULL;  
    char *content = NULL;
    char data[8192];
    data[0] = '\0';
    LwqqMsgMessage *mmsg = msg;
    const char *apistr = NULL;
    int has_cface = 0;


    //this would check msg content to see if it need do upload picture first
    LwqqMsgContent* c;
    int will_upload = 0;
    LwqqAsyncEvent* event;
    LwqqAsyncEvset* evset = lwqq_async_evset_new();
    TAILQ_FOREACH(c,&mmsg->content,entries){
        event = NULL;
        if(c->type == LWQQ_CONTENT_CFACE && c->data.cface.data > 0){
            event = lwqq_msg_upload_cface(lc,c,mmsg->super.super.type);
            if(!lc->gface_sig) lwqq_async_evset_add_event(evset,query_gface_sig(lc));
        } else if(c->type == LWQQ_CONTENT_OFFPIC && c->data.img.data > 0)
            event = lwqq_msg_upload_offline_pic(lc,c,mmsg->super.to);
        //event = lwqq_msg_upload_content(lc,mmsg,c);
        lwqq_async_evset_add_event(evset,event);
        will_upload |= (event!=NULL);
    }
    if(will_upload){
        event = lwqq_async_event_new(NULL);
        lwqq_async_add_evset_listener(evset, _C_(3p,lwqq_msg_send_continue, lc,msg,event));
        //if we need upload first. we break this send msg 
        //and use event chain to resume later.
        return event;
    }else{
        s_free(evset);
    }

    //we do send msg
    format_append(data,"r={");
    content = content_parse_string(mmsg,msg->super.super.type,&has_cface);
    if(msg->super.super.type == LWQQ_MS_BUDDY_MSG){
        format_append(data,"\"to\":%s,",mmsg->super.to);
        apistr = "send_buddy_msg2";
    }else if(msg->super.super.type == LWQQ_MS_GROUP_MSG){
        format_append(data,"\"group_uin\":%s,",mmsg->super.to);
        if(has_cface){
            format_append(data,"\"group_code\":%s,\"key\":\"%s\",\"sig\":\"%s\",",
                    mmsg->group.group_code,lc->gface_key,lc->gface_sig);
        }
        apistr = "send_qun_msg2";
    }else if(msg->super.super.type == LWQQ_MS_SESS_MSG){
        format_append(data,"\"to\":%s,\"group_sig\":\"%s\",",mmsg->super.to,mmsg->sess.group_sig);
        apistr = "send_sess_msg2";
    }else if(msg->super.super.type == LWQQ_MS_DISCU_MSG){
        format_append(data,"\"did\":\"%s\",",mmsg->discu.did);
        apistr = "send_discu_msg2";
    }else{
        //this would never come.
        assert(0);
        return NULL;
    }
    format_append(data,
            //"\"face\":0,"
            "\"content\":%s,"
            "\"msg_id\":%ld,"
            "\"clientid\":\"%s\","
            "\"psessionid\":\"%s\"}",
            content,lc->msg_id,lc->clientid,lc->psessionid);
    format_append(data,"&clientid=%s&psessionid=%s",lc->clientid,lc->psessionid);
    if(strlen(data)+1==sizeof(data)) return NULL;
    lwqq_verbose(3,"%s\n",data);

    /* Create a POST request */
    char url[512];
    snprintf(url, sizeof(url), "%s/channel/%s", "http://d.web2.qq.com",apistr);
    req = lwqq_http_create_default_request(lc,url, NULL);
    if (!req) {
        goto failed;
    }
    req->set_header(req, "Referer", "http://d.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    
    return req->do_request_async(req, 1, data,_C_(2p_i,msg_send_back,req,lc));

failed:
    lwqq_http_request_free(req);
    return NULL;
}
static int msg_send_back(LwqqHttpRequest* req,void* data)
{
    if (req == LWQQ_CALLBACK_FAILED ) return -1;
    json_t *root = NULL;
    int ret;
    int errno = 0;
    if (req->http_code != 200) {
        errno = 1;
        goto failed;
    }
    lwqq_verbose(3,"%s\n",req->response);

    //we check result if ok return 1,fail return 0;
    ret = json_parse_document(&root,req->response);
    if(ret != JSON_OK) goto failed;
    const char* retcode = json_parse_simple_value(root,"retcode");
    if(!retcode){
        errno = 1;
        goto failed;
    }
    errno = atoi(retcode);
failed:
    if(root)
        json_free_value(&root);
    lwqq_http_request_free(req);
    return errno;
}

int lwqq_msg_send_simple(LwqqClient* lc,int type,const char* to,const char* message)
{
    if(!lc||!to||!message)
        return 0;
    int ret = 0;
    LwqqMsg *msg = lwqq_msg_new(type);
    LwqqMsgMessage *mmsg = (LwqqMsgMessage*)msg;
    mmsg->super.to = s_strdup(to);
    mmsg->f_name = "宋体";
    mmsg->f_size = 13;
    mmsg->f_style.b = 0,mmsg->f_style.i = 0,mmsg->f_style.u = 0;
    strcpy(mmsg->f_color,"000000");
    LwqqMsgContent * c = s_malloc(sizeof(*c));
    c->type = LWQQ_CONTENT_STRING;
    c->data.str = s_strdup(message);
    TAILQ_INSERT_TAIL(&mmsg->content,c,entries);

    LWQQ_SYNC_BEGIN();
    lwqq_msg_send(lc,mmsg);
    LWQQ_SYNC_END();

    mmsg->f_name = NULL;

    lwqq_msg_free(msg);

    return ret;
}
const char* lwqq_msg_offfile_get_url(LwqqMsgOffFile* msg)
{
    static char url[1024];
    char* file_name = url_encode(msg->name);
    snprintf(url,sizeof(url),"http://%s:%s/%s?ver=2173&rkey=%s&range=0",
            msg->ip,msg->port,file_name,msg->rkey);
    s_free(file_name);
    return url;
}
static int file_download_finish(LwqqHttpRequest* req,void* data)
{
    FILE* f = data;
    fclose(f);
    lwqq_http_request_free(req);
    return LWQQ_EC_OK;
}

static int accept_file_back(LwqqHttpRequest* req,LwqqAsyncEvent* ret,char* saveto)
{
    int err = 0;
    if(req->http_code == 200){
        lwqq_log(LOG_WARNING, "recv_file failed(%d):%s\n",req->http_code,req->response);
        err = lwqq__get_retcode_from_str(req->response);
        goto failed;
    }
    if(req->http_code != 302){
        lwqq_log(LOG_WARNING, "recv_file failed(%d):%s\n",req->http_code,req->response);
        err = LWQQ_EC_ERROR;
        goto failed;
    }
    char * follow_url = req->location;
    char* name;
    req->location = NULL;
    name = url_whole_encode(follow_url);
    s_free(follow_url);
    lwqq_http_set_option(req, LWQQ_HTTP_RESET_URL,name);
    s_free(name);

    FILE* file = fopen(saveto,"w");
    if(file==NULL){
        perror("recv_file write error:");
        err = LWQQ_EC_ERROR;
        goto failed;
    }
    lwqq_http_set_option(req, LWQQ_HTTP_SAVE_FILE,file);
    LwqqAsyncEvent* ev = req->do_request_async(req,0,NULL,_C_(2p_i,file_download_finish,req,file));
    lwqq_async_add_event_chain(ev, ret);
    s_free(saveto);
    return LWQQ_EC_OK;
failed:
    ret->result = err;
    lwqq_async_event_finish(ret);
    lwqq_http_request_free(req);
    s_free(saveto);
    return err;
}

LwqqAsyncEvent* lwqq_msg_accept_file(LwqqClient* lc,LwqqMsgFileMessage* msg,const char* saveto)
{
    char url[512];
    //char* gbk = to_gbk(msg->recv.name);
    char* name = url_encode(msg->recv.name);
    snprintf(url,sizeof(url),"http://d.web2.qq.com/channel/get_file2?"
            "lcid=%d&guid=%s&to=%s&psessionid=%s&count=1&time=%ld&clientid=%s",
            msg->session_id,name,msg->super.from,lc->psessionid,time(NULL),lc->clientid);
    s_free(name);
    //s_free(gbk);
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://d.web2.qq.com/proxy.html?v=20110331002&id=4");
    //req->set_header(req,"Referer","http://web2.qq.com/");
    //followlocation by hand
    //because curl doesn't escape url after auto follow;
    //lwqq_http_not_follow(req);
    lwqq_http_set_option(req, LWQQ_HTTP_NOT_FOLLOW,1L);
    LwqqAsyncEvent* ev = lwqq_async_event_new(req);
    req->do_request_async(req,0,NULL,_C_(3p_i,accept_file_back,req,ev,s_strdup(saveto)));
    //because we use one req do every thing.
    //so we can set req here.and without change it dynamicly.
    msg->req = req;
    return ev;
}
LwqqAsyncEvent* lwqq_msg_refuse_file(LwqqClient* lc,LwqqMsgFileMessage* file)
{
    char url[512];
    snprintf(url,sizeof(url),"http://d.web2.qq.com/channel/refuse_file2?"
            "lcid=%d&to=%s&psessionid=%s&count=1&time=%ld&clientid=%s",
            file->session_id,file->super.from,lc->psessionid,time(NULL),lc->clientid);
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://d.web2.qq.com/proxy.html?v=20110331002&id=4");
    return req->do_request_async(req,0,NULL,_C_(p_i,process_simple_response,req));
}

LwqqAsyncEvent* lwqq_msg_upload_offline_file(LwqqClient* lc,LwqqMsgOffFile* file)
{
    char url[512];
    snprintf(url,sizeof(url),"http://weboffline.ftn.qq.com/ftn_access/upload_offline_file?time=%ld",time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://web2.qq.com/");
    req->set_header(req,"Origin","http://web2.qq.com");
    req->set_header(req,"Cache-Control","max-age=0");

    //some server didn't response this.
    //such as cache server
    req->set_header(req,"Expect","");

    req->add_form(req,LWQQ_FORM_CONTENT,"callback","parent.EQQ.Model.ChatMsg.callbackSendOffFile");
    req->add_form(req,LWQQ_FORM_CONTENT,"locallangid","2052");
    req->add_form(req,LWQQ_FORM_CONTENT,"clientversion","1409");
    req->add_form(req,LWQQ_FORM_CONTENT,"uin",file->super.from);
    req->add_form(req,LWQQ_FORM_CONTENT,"skey",lc->cookies->skey);
    req->add_form(req,LWQQ_FORM_CONTENT,"appid","1002101");
    req->add_form(req,LWQQ_FORM_CONTENT,"peeruin",file->super.to);
    req->add_form(req,LWQQ_FORM_CONTENT,"vfwebqq",lc->vfwebqq);
    req->add_form(req,LWQQ_FORM_FILE,"file",file->name);
    char fileid[128];
    snprintf(fileid,sizeof(fileid),"%s_%ld",file->super.to,time(NULL));
    req->add_form(req,LWQQ_FORM_CONTENT,"fileid",fileid);
    req->add_form(req,LWQQ_FORM_CONTENT,"senderviplevel","0");
    req->add_form(req,LWQQ_FORM_CONTENT,"reciverviplevel","0");
    file->req = req;
    return req->do_request_async(req,0,NULL,_C_(2p_i,upload_offline_file_back,req,file));
}

static int upload_offline_file_back(LwqqHttpRequest* req,void* data)
{
    LwqqMsgOffFile* file = data;
    json_t* json = NULL;
    int errno = 0;
    if(req->http_code!=200){
        errno = 1;
        goto done;
    }
    lwqq_verbose(3,"%s\n",req->response);

    char *end = strchr(req->response,'}');
    *(end+1) = '\0';
    json_parse_document(&json,strchr(req->response,'{'));
    if(strcmp(json_parse_simple_value(json,"retcode"),"0")!=0){
        errno = 1;
        goto done;
    }
    s_free(file->name);
    file->name = s_strdup(json_parse_simple_value(json,"filename"));
    file->path = s_strdup(json_parse_simple_value(json,"filepath"));
done:
    if(json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    file->req = NULL;
    return errno;
}

LwqqAsyncEvent* lwqq_msg_send_offfile(LwqqClient* lc,LwqqMsgOffFile* file)
{
    char url[512];
    char post[512];
    snprintf(url,sizeof(url),"http://d.web2.qq.com/channel/send_offfile2");
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://d.web2.qq.com/proxy.html?v=20110331002&id=3");
    snprintf(post,sizeof(post),"r={\"to\":\"%s\",\"file_path\":\"%s\","
            "\"filename\":\"%s\",\"to_uin\":\"%s\","
            "\"clientid\":\"%s\",\"psessionid\":\"%s\"}&"
            "clientid=%s&psessionid=%s",
            file->super.to,file->path,file->name,file->super.to,lc->clientid,lc->psessionid,
            lc->clientid,lc->psessionid);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    return req->do_request_async(req,1,post,_C_(2p_i,send_offfile_back,req,file));
}

static int send_offfile_back(LwqqHttpRequest* req,void* data)
{
    json_t* json = NULL;
    int errno = 0;
    if(req->http_code != 200){
        errno = 1;
        goto done;
    }
    lwqq_verbose(3,"%s\n",req->response);
    json_parse_document(&json, req->response);
    errno = atoi(json_parse_simple_value(json, "retcode"));
done:
    if(json){
        json_free_value(&json);
    }
    lwqq_http_request_free(req);
    return errno;
}
#define rand(n) (rand()%9000+1000)
LwqqAsyncEvent* lwqq_msg_upload_file(LwqqClient* lc,LwqqMsgOffFile* file,
        LWQQ_PROGRESS progress,void* prog_data)
{
    char url[512];
    snprintf(url,sizeof(url),"http://file1.web.qq.com/v2/%s/%s/%ld/%s/%s/1/f/1/0/0?psessionid=%s",
            file->super.from,file->super.to,time(NULL)%4096,lc->index,lc->port,lc->psessionid
            );
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://web2.qq.com/");

    req->add_form(req,LWQQ_FORM_FILE,"file",file->name);
    if(progress)
        lwqq_http_on_progress(req,progress,prog_data);
    //do{
        req->do_request(req,0,NULL);
    //}while(req->http_code==204);
    //lwqq_http_request_free(req);
    return NULL;
}

LwqqMsgContent* lwqq_msg_fill_upload_cface(const char* filename,
        const void* buffer,size_t buf_size)
{
    LwqqMsgContent* c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_CFACE;
    c->data.cface.name = s_strdup(filename);
    c->data.cface.data = s_malloc(buf_size);
    memcpy(c->data.cface.data,buffer,buf_size);
    c->data.cface.size = buf_size;
    return c;
}
LwqqMsgContent* lwqq_msg_fill_upload_offline_pic(const char* filename,
        const void* buffer,size_t buf_size)
{
    LwqqMsgContent* c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_OFFPIC;
    c->data.img.name = s_strdup(filename);
    c->data.img.data = s_malloc(buf_size);
    memcpy(c->data.cface.data,buffer,buf_size);
    c->data.img.size = buf_size;
    return c;
}
LwqqMsgOffFile* lwqq_msg_fill_upload_offline_file(const char* filename,
        const char* from,const char* to)
{
    LwqqMsgOffFile* file = s_malloc0(sizeof(*file));
    file->super.super.type = LWQQ_MT_OFFFILE;
    file->name = s_strdup(filename);
    file->super.from = s_strdup(from);
    file->super.to = s_strdup(to);
    return file;
}
LwqqAsyncEvent* lwqq_msg_input_notify(LwqqClient* lc,const char* serv_id)
{
    if(!lc || !serv_id) return NULL;
    char url[512];
    snprintf(url,sizeof(url),"http://d.web2.qq.com/channel/input_notify2?to_uin=%s&clientid=%s&psessionid=%s&t=%ld",
            serv_id,lc->clientid,lc->psessionid,time(NULL)
            );
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req,"Referer","http://d.web2.qq.com/proxy.html?v=20110331002&id=2");
    return req->do_request_async(req,0,NULL,_C_(p_i,process_simple_response,req));
}

LwqqAsyncEvent* lwqq_msg_shake_window(LwqqClient* lc,const char* serv_id)
{
    if(!lc || !serv_id) return NULL;
    char url[512];
    snprintf(url,sizeof(url),"http://d.web2.qq.com/channel/shake2?to_uin=%s&clientid=%s&psessionid=%s&t=%ld",
            serv_id,lc->clientid,lc->psessionid,time(NULL));
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer","http://d.web2.qq.com/proxy.html?v=20110331002&id=2");
    return req->do_request_async(req,0,NULL,_C_(p_i,process_simple_response,req));
}

LwqqAsyncEvent* lwqq_msg_friend_history(LwqqClient* lc,const char* serv_id,LwqqHistoryMsgList* list)
{
    if(!lc||!serv_id||!list) return NULL;
    char url[512];
    snprintf(url,sizeof(url),"http://web2.qq.com/cgi-bin/webqq_chat/?cmd=1&tuin=%s&vfwebqq=%s&page=%d&row=%d&callback=alloy.app.chatLogViewer.renderChatLog&t=%ld",serv_id,lc->vfwebqq,list->page,list->row,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer","http://web2.qq.com/");
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req,0,NULL,_C_(3p,process_msg_list,req,s_strdup(serv_id),list));
}

LwqqAsyncEvent* lwqq_msg_group_history(LwqqClient* lc,LwqqGroup* g,LwqqHistoryMsgList* list)
{
    if(!lc||!g||!list) return NULL;
    char url[512];
    snprintf(url,sizeof(url),"http://cgi.web2.qq.com/keycgi/top/groupchatlog?ps=%d&bs=%d&es=%d&gid=%s&mode=1&vfwebqq=%s&t=%ld",
            list->row,list->begin,list->end,g->code,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer","http://cgi.web2.qq.com/proxy.html?v=20110412001&id=2");
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req,0,NULL,_C_(3p,process_group_msg_list,req,NULL,list));
}

#if 0
//msg_tip is advertisement about.
//so this is unnecessary
void get_msg_tip_loop(LwqqAsyncTimerHandle timer,void* data)
{
    LwqqClient* lc = data;
    LwqqRecvMsgList* list = lc->msg_list;
    lwqq_msg_get_msg_tip(lc,++list->count);
    lwqq_async_timer_repeat(timer);
}
static LwqqAsyncEvent* lwqq_msg_get_msg_tip(LwqqClient* lc,unsigned int counter);
static int get_msg_tip_back(LwqqHttpRequest* req);
static LwqqAsyncEvent* lwqq_msg_get_msg_tip(LwqqClient* lc,unsigned int counter)
{
    if(!lc) return NULL;
    LwqqAsyncEvent* ev = lwqq_async_queue_find(&lc->ev_queue, lwqq_msg_get_msg_tip);
    if(ev) return ev;
    char url[512];
    snprintf(url, sizeof(url), "http://web2.qq.com/web2/get_msg_tip?uin=&tp=1&id=0&retype=1&rc=%u&lv=3&t=%lu",counter,time(NULL));
    lwqq_puts(url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req,"Referer","http://web2.qq.com/");
    ev = req->do_request_async(req,0,NULL,_C_(p_i,get_msg_tip_back,req));
    lwqq_async_queue_add(&lc->ev_queue,lwqq_msg_get_msg_tip,ev);
    return ev;
}

static int get_msg_tip_back(LwqqHttpRequest* req)
{
    LwqqClient* lc = req->lc;
    lwqq_async_queue_rm(&lc->ev_queue,lwqq_msg_get_msg_tip);

    lwqq_http_request_free(req);
    return 0;
}
#endif
