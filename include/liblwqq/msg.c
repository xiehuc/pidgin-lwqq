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

static void *start_poll_msg(void *msg_list);
static void lwqq_recvmsg_poll_msg(struct LwqqRecvMsgList *list);
static json_t *get_result_json_object(json_t *json);
static void parse_recvmsg_from_json(LwqqRecvMsgList *list, const char *str);

static void lwqq_msg_message_free(void *opaque);
static void lwqq_msg_status_free(void *opaque);

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

    list = s_malloc0(sizeof(*list));
    list->lc = client;
    pthread_mutex_init(&list->mutex, NULL);
    SIMPLEQ_INIT(&list->head);
    list->poll_msg = lwqq_recvmsg_poll_msg;
    
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
    while ((recvmsg = SIMPLEQ_FIRST(&list->head))) {
        SIMPLEQ_REMOVE_HEAD(&list->head, entries);
        lwqq_msg_free(recvmsg->msg);
        s_free(recvmsg);
    }
    pthread_mutex_unlock(&list->mutex);

    s_free(list);
    return ;
}

LwqqMsg *lwqq_msg_new(LwqqMsgType type)
{
    LwqqMsg *msg = NULL;

    msg = s_malloc0(sizeof(*msg));
    msg->type = type;

    switch (type) {
    case LWQQ_MT_BUDDY_MSG:
    case LWQQ_MT_GROUP_MSG:
        msg->opaque = s_malloc0(sizeof(LwqqMsgMessage));
        break;
    case LWQQ_MT_STATUS_CHANGE:
        msg->opaque = s_malloc0(sizeof(LwqqMsgStatusChange));
        break;
    default:
        lwqq_log(LOG_ERROR, "No such message type\n");
        goto failed;
        break;
    }

    return msg;
failed:
    lwqq_msg_free(msg);
    return NULL;
}

static void lwqq_msg_message_free(void *opaque)
{
    LwqqMsgMessage *msg = opaque;
    if (!msg) {
        return ;
    }

    s_free(msg->from);
    s_free(msg->to);
    s_free(msg->send);
    s_free(msg->f_name);
    s_free(msg->f_color);

    LwqqMsgContent *c;
    LIST_FOREACH(c, &msg->content, entries) {
        switch(c->type){
            case LWQQ_CONTENT_STRING:
                s_free(c->data.str);
                break;
            case LWQQ_CONTENT_OFFPIC:
                s_free(c->data.img.file_path);
                s_free(c->data.img.file);
        }
        s_free(c);
    }
    
    s_free(msg);
}

static void lwqq_msg_status_free(void *opaque)
{
    LwqqMsgStatusChange *s = opaque;
    if (!s) {
        return ;
    }

    s_free(s->who);
    s_free(s);
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

    printf ("type: %d\n", msg->type);
    switch (msg->type) {
    case LWQQ_MT_BUDDY_MSG:
    case LWQQ_MT_GROUP_MSG:
        lwqq_msg_message_free(msg->opaque);
        break;
    case LWQQ_MT_STATUS_CHANGE:
        lwqq_msg_status_free(msg->opaque);
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
    if (!strncmp(msg_type, "message", strlen("message"))) {
        type = LWQQ_MT_BUDDY_MSG;
    } else if (!strncmp(msg_type, "group_message", strlen("group_message"))) {
        type = LWQQ_MT_GROUP_MSG;
    } else if (!strncmp(msg_type, "buddies_status_change",
                        strlen("buddies_status_change"))) {
        type = LWQQ_MT_STATUS_CHANGE;
    }
    return type;
}
static int parse_content(json_t *json, void *opaque)
{
    json_t *tmp, *ctent;
    LwqqMsgMessage *msg = opaque;

    tmp = json_find_first_label_all(json, "content");
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
                color = color ?: "000000";
                msg->f_color = s_strdup(color);

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
                LIST_INSERT_HEAD(&msg->content, c, entries);
            } else if(!strcmp(buf, "offpic")) {
                //["offpic",{"success":1,"file_path":"/d65c58ae-faa6-44f3-980e-272fb44a507f"}]
                LwqqMsgContent *c = s_malloc0(sizeof(*c));
                json_t *cur = ctent->child->next->child;
                c->type = LWQQ_CONTENT_OFFPIC;
                c->data.img.success = atoi(cur->child->text);
                cur = cur->next;
                c->data.img.file_path = s_strdup(cur->child->text);
                LIST_INSERT_HEAD(&msg->content,c,entries);
            }
        } else if (ctent->type == JSON_STRING) {
            LwqqMsgContent *c = s_malloc0(sizeof(*c));
            c->type = LWQQ_CONTENT_STRING;
            c->data.str = ucs4toutf8(ctent->text);
            LIST_INSERT_HEAD(&msg->content, c, entries);
        }
    }

    /* Make msg valid */
    if (!msg->f_name || !msg->f_color || LIST_EMPTY(&msg->content)) {
        return -1;
    }
    if (msg->f_size < 10) {
        msg->f_size = 10;
    }

    return 0;
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
static int parse_new_msg(json_t *json, void *opaque)
{
    LwqqMsgMessage *msg = opaque;
    char *time;
    
    msg->from = s_strdup(json_parse_simple_value(json, "from_uin"));
    if (!msg->from) {
        return -1;
    }

    time = json_parse_simple_value(json, "time");
    time = time ?: "0";
    msg->time = (time_t)strtoll(time, NULL, 10);

    msg->to = s_strdup(json_parse_simple_value(json, "to_uin"));

    //if it failed means it is not group message.
    //so it equ NULL.
    msg->send = s_strdup(json_parse_simple_value(json, "send_uin"));

    if (!msg->to) {
        return -1;
    }
    
    if (parse_content(json, opaque)) {
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
static int parse_status_change(json_t *json, void *opaque)
{
    LwqqMsgStatusChange *msg = opaque;
    char *c_type;

    msg->who = s_strdup(json_parse_simple_value(json, "uin"));
    if (!msg->who) {
        return -1;
    }
    msg->status = s_strdup(json_parse_simple_value(json, "status"));
    if (!msg->status) {
        return -1;
    }
    c_type = s_strdup(json_parse_simple_value(json, "client_type"));
    c_type = c_type ?: "1";
    msg->client_type = atoi(c_type);

    return 0;
}
static void request_content_offpic(LwqqClient* lc,const char* f_uin,LwqqMsgContent* c)
{
    LwqqHttpRequest* req;
    LwqqErrorCode error;
    LwqqErrorCode *err = &error;
    char* cookies;
    int ret;
    char url[512];
    char *file_path = url_encode(c->data.img.file_path);
    //there are face 1 to face 10 server to accelerate speed.
    snprintf(url, sizeof(url),
             "%s/get_offpic2?file_path=%s&f_uin=%s&clientid=%s&psessionid=%s",
             "http://d.web2.qq.com/channel",
             file_path,f_uin,lc->clientid,lc->psessionid);
    s_free(file_path);
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://web2.qq.com/");
    req->set_header(req,"Host","d.web2.qq.com");

    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    //req->set_header(req,"Cache-Control","max-age=0");
    ret = req->do_request(req, 0, NULL);

    if(ret||(req->http_code!=200 && req->http_code!=302)){
        if(err){
            *err = LWQQ_EC_HTTP_ERROR;
            goto done;
        }
    }
    const char* location = req->get_header(req,"Location");
    lwqq_http_request_free(req);

    req = lwqq_http_create_default_request(location,err);
    req->set_header(req,"Referer","http://web2.qq.com");

    ret = req->do_request(req,0,NULL);

    if(ret||req->http_code!=200){
        if(err){
            *err = LWQQ_EC_HTTP_ERROR;
            goto done;
        }
    }

    c->data.img.file = req->response;
    c->data.img.size = req->resp_len;
    req->response = NULL;
    puts("content img ok");
    
done:
    lwqq_http_request_free(req);
}
static void request_msg_offpic(LwqqClient* lc,LwqqMsgMessage* msg)
{
    LwqqMsgContent* c;
    LIST_FOREACH(c,&msg->content,entries){
        if(c->type == LWQQ_CONTENT_OFFPIC){
            request_content_offpic(lc,msg->from,c);
        }
    }
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
static void parse_recvmsg_from_json(LwqqRecvMsgList *list, const char *str)
{
    int ret;
    json_t *json = NULL, *json_tmp, *cur;

    ret = json_parse_document(&json, (char *)str);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of friends error: %s\n", str);
        goto done;
    }

    json_tmp = get_result_json_object(json);
    if (!json_tmp) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", str);
        goto done;
    }

    if (!json_tmp->child || !json_tmp->child->child) {
        goto done;
    }

    /* make json_tmp point to first child of "result" */
    json_tmp = json_tmp->child->child;
    for (cur = json_tmp; cur != NULL; cur = cur->next) {
        LwqqMsg *msg = NULL;
        LwqqMsgType msg_type;
        int ret;
        
        msg_type = parse_recvmsg_type(cur);
        msg = lwqq_msg_new(msg_type);
        if (!msg) {
            continue;
        }

        switch (msg_type) {
        case LWQQ_MT_BUDDY_MSG:
        case LWQQ_MT_GROUP_MSG:
            ret = parse_new_msg(cur, msg->opaque);
            request_msg_offpic(list->lc,msg->opaque);
            break;
        case LWQQ_MT_STATUS_CHANGE:
            ret = parse_status_change(cur, msg->opaque);
            break;
        default:
            ret = -1;
            lwqq_log(LOG_ERROR, "No such message type\n");
            break;
        }

        if (ret == 0) {
            LwqqRecvMsg *rmsg = s_malloc0(sizeof(*rmsg));
            rmsg->msg = msg;
            /* Parse a new message successfully, link it to our list */
            pthread_mutex_lock(&list->mutex);
            SIMPLEQ_INSERT_TAIL(&list->head, rmsg, entries);
            pthread_mutex_unlock(&list->mutex);
        } else {
            lwqq_msg_free(msg);
        }
    }
    
done:
    if (json) {
        json_free_value(&json);
    }
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
    int ret;
    char *cookies;
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
    req = lwqq_http_create_default_request(url, NULL);
    if (!req) {
        goto failed;
    }
    req->set_header(req, "Referer", "http://d.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    while(1) {
        ret = req->do_request(req, 1, msg);
        if (ret || req->http_code != 200) {
            continue;
        }
        parse_recvmsg_from_json(list, req->response);
    }
failed:
    pthread_exit(NULL);
}

static void lwqq_recvmsg_poll_msg(LwqqRecvMsgList *list)
{
    
    pthread_attr_init(&list->attr);
    pthread_attr_setdetachstate(&list->attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&list->tid, &list->attr, start_poll_msg, list);
}

#define LEFT "\\\""
#define RIGHT "\\\""
#define KEY(key) "\\\""key"\\\""

static char* content_parse_string_r(LwqqMsgMessage* msg,char* buf)
{
    strcpy(buf,"\"[");
    LwqqMsgContent* c;
    static char piece[10];

    LIST_FOREACH(c,&msg->content,entries){
        switch(c->type){
            case LWQQ_CONTENT_FACE:
                strcat(buf,"["KEY("face")",");
                snprintf(piece,sizeof(piece),"%d",c->data.face);
                strcat(buf,piece);
                strcat(buf,"],");
                break;
            case LWQQ_CONTENT_OFFPIC:
                strcat(buf,"["KEY("offpic")","LEFT);
                strcat(buf,c->data.img.file_path);
                strcat(buf,RIGHT","LEFT);
                strcat(buf,c->data.img.file);
                strcat(buf,RIGHT",");
                snprintf(piece,sizeof(piece),"%ld",c->data.img.size);
                strcat(buf,piece);
                strcat(buf,"],");
                break;
            case LWQQ_CONTENT_STRING:
                strcat(buf,LEFT);
                strcat(buf,c->data.str);
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
static char* content_parse_string(LwqqMsgMessage* msg)
{
    //not thread safe. 
    //you need ensure only one thread send msg in one time.
    static char buf[2048];
    return content_parse_string_r(msg,buf);
}

LwqqMsgContent* lwqq_msg_upload_offline_pic(LwqqClient* lc,const char* to,const char* filename,const char* buffer,size_t size)
{
    LwqqHttpRequest *req;
    LwqqErrorCode err;
    LwqqMsgContent* c = NULL;
    char url[512];
    char *cookies;
    int ret;

    snprintf(url,sizeof(url),"http://weboffline.ftn.qq.com/ftn_access/upload_offline_pic?time=%ld",
            time(NULL));
    req = lwqq_http_create_default_request(url,&err);
    req->set_header(req,"Origin","http://web2.qq.com");
    req->set_header(req,"Referer","http://web2.qq.com/");
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    req->add_form(req,LWQQ_FORM_CONTENT,"callback","parent.EQQ.Model.ChatMsg.callbackSendPic");
    req->add_form(req,LWQQ_FORM_CONTENT,"locallangid","2052");
    req->add_form(req,LWQQ_FORM_CONTENT,"clientversion","1409");
    req->add_form(req,LWQQ_FORM_CONTENT,"uin",lc->username);///<this may error
    puts(lc->username);
    req->add_form(req,LWQQ_FORM_CONTENT,"skey",lc->cookies->skey);
    req->add_form(req,LWQQ_FORM_CONTENT,"appid","1002101");
    req->add_form(req,LWQQ_FORM_CONTENT,"peeruin","593023668");///<what this means?
    //req->add_form(req,LWQQ_FORM_FILE,"file",pic_path);
    req->add_file_content(req,"file",filename,buffer,size);
    req->add_form(req,LWQQ_FORM_CONTENT,"field","1");
    req->add_form(req,LWQQ_FORM_CONTENT,"vfwebqq",lc->vfwebqq);
    req->add_form(req,LWQQ_FORM_CONTENT,"senderviplevel","0");
    req->add_form(req,LWQQ_FORM_CONTENT,"reciverviplevel","0");
    ret = req->do_request(req,0,NULL);

    if(ret||req->http_code!=200){
        goto done;
    }

    json_t* json = NULL;
    char *end = strchr(req->response,'}');
    *(end+1) = '\0';
    json_parse_document(&json,strchr(req->response,'{'));
    if(strcmp(json_parse_simple_value(json,"retcode"),"0")!=0){
        goto done;
    }
    c = s_malloc0(sizeof(*c));
    c->type = LWQQ_CONTENT_OFFPIC;
    c->data.img.size = atol(json_parse_simple_value(json,"filesize"));
    c->data.img.file_path = s_strdup(json_parse_simple_value(json,"filepath"));
    c->data.img.file = s_strdup(json_parse_simple_value(json,"filename"));

done:
    if(json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return c;
}
/** 
 * 
 * 
 * @param lc 
 * @param sendmsg 
 * 
 * @return 1 means ok
 *         0 means failed or send failed
 */
int lwqq_msg_send(LwqqClient *lc, LwqqMsg *msg)
{
    int ret;
    LwqqHttpRequest *req = NULL;  
    char *cookies;
    char *content = NULL;
    char data[1024];
    LwqqMsgMessage *mmsg;
    const char *tonam;
    const char *apistr;

    if (!msg || (msg->type != LWQQ_MT_BUDDY_MSG &&
                 msg->type != LWQQ_MT_GROUP_MSG)) {
        goto failed;
    }
    if(msg->type == LWQQ_MT_BUDDY_MSG){
        tonam = "to";
        apistr = "send_buddy_msg2";
    }else if(msg->type == LWQQ_MT_GROUP_MSG){
        tonam = "group_uin";
        apistr = "send_qun_msg2";
    }
    mmsg = msg->opaque;
    content = content_parse_string(mmsg);
    snprintf(data,sizeof(data),"r={"
            "\"%s\":%s,"
            "\"face\":0,"
            "\"content\":%s,"
            "\"msg_id\":%ld,"
            "\"clientid\":\"%s\","
            "\"psessionid\":\"%s\"}",
            tonam,mmsg->to,content,lc->msg_id,lc->clientid,lc->psessionid);
    puts(data);

    /* Create a POST request */
    char url[512];
    snprintf(url, sizeof(url), "%s/channel/%s", "http://d.web2.qq.com",apistr);
    req = lwqq_http_create_default_request(url, NULL);
    if (!req) {
        goto failed;
    }
    req->set_header(req, "Referer", "http://d.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    
    ret = req->do_request(req, 1, data);
    if (ret || req->http_code != 200) {
        goto failed;
    }

    puts(req->response);

    //we check result if ok return 1,fail return 0;
    json_t *root = NULL;
    json_t *res;
    json_parse_document(&root,req->response);
    res = get_result_json_object(root);
    if(!res){
        goto failed;
    }
    return 1;

failed:
    if(root)
        json_free_value(&root);
    return 0;
}

int lwqq_msg_send_simple(LwqqClient* lc,int type,const char* to,const char* message)
{
    if(!lc||!to||!message)
        return 0;
    int ret;
    LwqqMsg *msg = lwqq_msg_new(type);
    LwqqMsgMessage *mmsg = msg->opaque;
    mmsg->to = s_strdup(to);
    mmsg->f_name = "宋体";
    mmsg->f_size = 13;
    mmsg->f_style.b = 0,mmsg->f_style.i = 0,mmsg->f_style.u = 0;
    mmsg->f_color = "000000";
    LwqqMsgContent * c = s_malloc(sizeof(*c));
    c->type = LWQQ_CONTENT_STRING;
    c->data.str = s_strdup(message);
    LIST_INSERT_HEAD(&mmsg->content,c,entries);

    ret = lwqq_msg_send(lc,msg);

    mmsg->f_name = NULL;
    mmsg->f_color = NULL;

    lwqq_msg_free(msg);

    return ret;
}

