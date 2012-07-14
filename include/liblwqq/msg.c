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

static int send_msg(struct LwqqSendMsg *sendmsg);

static LwqqMsg *lwqq_msg_message_new(const char *msg_type, const char *from,
                                     const char *to, const char *content);
static void lwqq_msg_message_free(LwqqMsg *msg);
static LwqqMsg *lwqq_msg_status_new(const char *msg_type, const char *who,
                                    const char *status);
static void lwqq_msg_status_free(LwqqMsg *msg);

static LwqqMsg *lwqq_msg_any_new(const char *msg_type);
static void lwqq_msg_any_free(LwqqMsg *msg);

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

/**
 * Create a new LwqqMsg object
 * 
 * @param msg_type
 * @param ... different type will call different constructor
 *            and pass these parameters to the constructor
 * 
 * @return NULL on failure
 */
LwqqMsg *lwqq_msg_new(const char *msg_type, ...)
{
    va_list ap;
    va_start(ap, msg_type);
    if (strncmp(msg_type, MT_MESSAGE, strlen(MT_MESSAGE)) == 0
        || strncmp(msg_type, MT_GROUP_MESSAGE, strlen(MT_GROUP_MESSAGE)) == 0) {
        char *from = va_arg(ap, char *);
        char *to = va_arg(ap, char *);
        char *content = va_arg(ap, char *);
        va_end(ap);
        return lwqq_msg_message_new(msg_type, from, to, content);
    } else if (strncmp(msg_type, MT_STATUS_CHANGE, strlen(MT_STATUS_CHANGE)) == 0) {
        char *who = va_arg(ap, char *);
        char *status = va_arg(ap, char *);
        va_end(ap);
        return lwqq_msg_status_new(msg_type, who, status);
    } else {
        va_end(ap);
        return lwqq_msg_any_new(msg_type);
    }
    return NULL;
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
    if (strncmp(msg->msg_type, MT_MESSAGE, strlen(MT_MESSAGE)) == 0
        || strncmp(msg->msg_type, MT_GROUP_MESSAGE, strlen(MT_GROUP_MESSAGE)) == 0) {
        lwqq_msg_message_free(msg);
    } else if (strncmp(msg->msg_type, MT_STATUS_CHANGE, strlen(MT_STATUS_CHANGE)) == 0) {
        lwqq_msg_status_free(msg);
    } else {
        lwqq_msg_any_free(msg);
    }
    s_free(msg);
}

/**
 * Create message object. msg_type must be MT_MESSAGE.
 * 
 * @param msg_type 
 * @param from 
 * @param to 
 * @param content 
 * 
 * @return 
 */
static LwqqMsg *lwqq_msg_message_new(const char *msg_type, const char *from,
                                     const char *to, const char *content)
{
    LwqqMsg *msg;

    if (strcmp(msg_type, MT_MESSAGE) != 0
        && strcmp(msg_type, MT_GROUP_MESSAGE) != 0)
        return NULL;
    
    msg = s_malloc0(sizeof(*msg));
    msg->message.from = s_strdup(from);
    msg->message.to = s_strdup(to);
    msg->message.msg_type = s_strdup(msg_type);
    msg->message.content = s_strdup(content);
    return msg;
}

/**
 * Free message object
 * 
 * @param msg 
 */
static void lwqq_msg_message_free(LwqqMsg *msg)
{
    if (!msg)
        return;
    if (strncmp(msg->msg_type, MT_MESSAGE, strlen(MT_MESSAGE)) != 0
        && strncmp(msg->msg_type, MT_GROUP_MESSAGE, strlen(MT_GROUP_MESSAGE)) != 0)
        return;
    s_free(msg->message.from);
    s_free(msg->message.to);
    s_free(msg->message.msg_type);
    s_free(msg->message.content);
}

/**
 * create status change message object, msg_type must be MT_STATUS_CHANGE
 * 
 * @param msg_type 
 * @param who 
 * @param status 
 * 
 * @return 
 */
static LwqqMsg *lwqq_msg_status_new(const char *msg_type, const char *who,
                                    const char *status)
{
    LwqqMsg *msg;

    if (strcmp(msg_type, MT_STATUS_CHANGE) != 0)
        return NULL;
    msg = s_malloc(sizeof(*msg));
    LwqqMsgStatus *m = (LwqqMsgStatus *)msg;
    
    m->who = s_strdup(who);
    m->status = s_strdup(status);
    m->msg_type = s_strdup(msg_type);
    return msg;
}

/**
 * free status change message object.
 * 
 * @param msg 
 */
static void lwqq_msg_status_free(LwqqMsg *msg)
{
    if (!msg)
        return;
    if (strncmp(msg->msg_type, MT_STATUS_CHANGE, strlen(MT_STATUS_CHANGE)) != 0)
        return;
    s_free(msg->status.who);
    s_free(msg->status.msg_type);
    s_free(msg->status.status);
}

/**
 * create "any" object. it only contains msg_type now.
 * 
 * @param msg_type 
 * 
 * @return NULL on failure.
 */
static LwqqMsg *lwqq_msg_any_new(const char *msg_type)
{
    LwqqMsg *msg;
    
    if (!msg_type)
        return NULL;
    msg = s_malloc(sizeof(*msg));
    LwqqMsgAny *m = (LwqqMsgAny *)msg;
    
    m->msg_type = s_strdup(msg_type);
    return msg;
}

/**
 * free "any" object.
 * 
 * @param msg 
 */
static void lwqq_msg_any_free(LwqqMsg *msg)
{
    if (!msg)
        return;
    s_free(msg->any.msg_type);
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
        LwqqRecvMsg *msg = NULL;
        char *msg_type;
        
        msg_type = json_parse_simple_value(cur, "poll_type");

        if (!msg_type)
            continue;

        /* FIXME, MT_MESSAGE should be a MACRO */
        msg = s_malloc0(sizeof(*msg));
        if (strncmp(msg_type, MT_MESSAGE, strlen(MT_MESSAGE)) == 0
            || strncmp(msg_type, MT_GROUP_MESSAGE, strlen(MT_GROUP_MESSAGE)) == 0) {
            char *from, *to, *content = NULL;
            json_t *tmp;
            from = json_parse_simple_value(cur, "from_uin");
            to = json_parse_simple_value(cur, "to_uin");
            tmp = json_find_first_label_all(cur, "content");
            if (tmp && tmp->child && tmp->child->child) {
                json_t *ctent;
                for (ctent = tmp->child->child; ctent != NULL; ctent = ctent->next) {
                    if (ctent->type != JSON_STRING)
                        continue;
                    /* Convert to utf-8 */
                    content = ucs4toutf8(ctent->text);
                }
            } else {
                content = NULL;
            }

            if (!from || !to || !content) {
                continue;
            }
            msg->msg = lwqq_msg_new(msg_type, from, to, content);
            s_free(content);
        } else if (strncmp(msg_type, MT_STATUS_CHANGE, strlen(MT_STATUS_CHANGE)) == 0) {
            char *who = json_parse_simple_value(cur, "uin");
            char *status = json_parse_simple_value(cur, "status");
            if (!who || !status) {
                continue;
            }
            msg->msg = lwqq_msg_new(msg_type, who, status);
        } else {
            msg->msg = lwqq_msg_new(msg_type);
        }

        if (msg->msg) {
            pthread_mutex_lock(&list->mutex);
            SIMPLEQ_INSERT_TAIL(&list->head, msg, entries);
            pthread_mutex_unlock(&list->mutex);
        } else {
            s_free(msg);
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
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&tid, &attr, start_poll_msg, list);
}

/**
 * Create a new LwqqSendMsg object
 * 
 * @param client 
 * @param to
 * @param msg_type 
 * @param content 
 * 
 * @return 
 */
LwqqSendMsg *lwqq_sendmsg_new(void *client, const char *to,
                              const char *msg_type, const char *content)
{
    LwqqSendMsg *sendmsg;
    LwqqClient *lc = client;

    if (!client || !to || !msg_type || !content) {
        return NULL;
    }
    
    sendmsg = s_malloc0(sizeof(*sendmsg));
    sendmsg->lc = client;
    sendmsg->msg = lwqq_msg_new(msg_type, lc->username, to, content);
    sendmsg->send = send_msg; 

    return sendmsg;
}

/**
 * Free a LwqqSendMsg object
 * 
 * @param msg 
 */
void lwqq_sendmsg_free(LwqqSendMsg *msg)
{
    if (!msg)
        return;

    lwqq_msg_free(msg->msg);
    s_free(msg);
}

/* FIXME: So much hard code */
char *create_default_content(const char *content)
{
    char s[2048];

    snprintf(s, sizeof(s), "\"[\\\"%s\\\\n\\\",[\\\"font\\\","
             "{\\\"name\\\":\\\"宋体\\\",\\\"size\\\":\\\"10\\\","
             "\\\"style\\\":[0,0,0],\\\"color\\\":\\\"000000\\\"}]]\"", content);
    return strdup(s);
}

/**
 * 
 * 
 * @param msg 
 * @param lc 
 * 
 * @return 
 */
static int send_msg(struct LwqqSendMsg *sendmsg)
{
    int ret;
    LwqqClient *lc;
    LwqqMsgMessage *msg;
    LwqqHttpRequest *req = NULL;  
    char *cookies;
    char *s;
    char *content = NULL;
    char data[1024];

    lc = (LwqqClient *)(sendmsg->lc);
    if (!lc) {
        goto failed;
    }
    msg = &sendmsg->msg->message;
    content = create_default_content(msg->content);
    snprintf(data, sizeof(data), "{\"to\":%s,\"face\":0,\"content\":%s,"
             "\"msg_id\":%ld,\"clientid\":\"%s\",\"psessionid\":\"%s\"}",
             msg->to, content, lc->msg_id, lc->clientid, lc->psessionid);
    s_free(content);
    s = url_encode(data);
    snprintf(data, sizeof(data), "r=%s", s);
    s_free(s);

    /* Create a POST request */
    char url[512];
    snprintf(url, sizeof(url), "%s/channel/send_buddy_msg2", "http://d.web2.qq.com");
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
    return 0;

failed:
    return -1;
}
