/**
 * @file   info.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 27 19:48:58 2012
 * 
 * @brief  Fetch QQ information. e.g. friends information, group information.
 * 
 * 
 */

#include <string.h>
#include <stdlib.h>
#include "info.h"
#include "url.h"
#include "logger.h"
#include "http.h"
#include "smemory.h"
#include "json.h"

static json_t *get_result_json_object(json_t *json);
static void create_post_data(LwqqClient *lc, char *buf, int buflen);
static char *get_friend_qqnumber(LwqqClient *lc, const char *uin);
char *get_group_qqnumber(LwqqClient *lc, const char *code);

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
 * Just a utility function
 * 
 * @param lc 
 * @param buf 
 * @param buflen 
 */
static void create_post_data(LwqqClient *lc, char *buf, int buflen)
{
    char *s;
    char m[256];
    snprintf(m, sizeof(m), "{\"h\":\"hello\",\"vfwebqq\":\"%s\"}",
             lc->vfwebqq);
    s = url_encode(m);
    snprintf(buf, buflen, "r=%s", s);
    s_free(s);
}

/** 
 * Parse friend category information
 * 
 * @param lc 
 * @param json Point to the first child of "result"'s value
 */
static void parse_categories_child(LwqqClient *lc, json_t *json)
{
    LwqqFriendCategory *cate;
    json_t *cur;
    char *index, *sort, *name;
    
    /* Make json point category reference */
    while (json) {
        if (json->text && !strcmp(json->text, "categories")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }
    
    json = json->child;    //point to the array.[]
    for (cur = json->child; cur != NULL; cur = cur->next) {
        index = json_parse_simple_value(cur, "index");
        sort = json_parse_simple_value(cur, "sort");
        name = json_parse_simple_value(cur, "name");
        cate = s_malloc0(sizeof(*cate));
        if (index) {
            cate->index = atoi(index);
        }
        if (sort) {
            cate->sort = atoi(sort);
        }
        if (name) {
            cate->name = s_strdup(name);
        }

        /* Add to categories list */
        LIST_INSERT_HEAD(&lc->categories, cate, entries);
    }

    /* add the default category */
    cate = s_malloc0(sizeof(*cate));
    cate->index = 0;
    cate->index = 1;
    cate->name = s_strdup("My Friends");
    LIST_INSERT_HEAD(&lc->categories, cate, entries);
}

/** 
 * Parse info child
 * 
 * @param lc 
 * @param json Point to the first child of "result"'s value
 */
static void parse_info_child(LwqqClient *lc, json_t *json)
{
    LwqqBuddy *buddy;
    json_t *cur;
    
    /* Make json point "info" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "info")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }
    
    json = json->child;    //point to the array.[]
    for (cur = json->child; cur != NULL; cur = cur->next) {
        buddy = s_malloc0(sizeof(*buddy));
        buddy->face = s_strdup(json_parse_simple_value(cur, "face"));
        buddy->flag = s_strdup(json_parse_simple_value(cur, "flag"));
        buddy->nick = s_strdup(json_parse_simple_value(cur, "nick"));
        buddy->uin = s_strdup(json_parse_simple_value(cur, "uin"));
                                
        /* Add to categories list */
        LIST_INSERT_HEAD(&lc->friends, buddy, entries);
    }
}

/** 
 * Parse marknames child
 * 
 * @param lc 
 * @param json Point to the first child of "result"'s value
 */
static void parse_marknames_child(LwqqClient *lc, json_t *json)
{
    LwqqBuddy *buddy;
    char *uin, *markname;
    json_t *cur;
    
    /* Make json point "info" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "marknames")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }

    json = json->child;    //point to the array.[]
    for (cur = json->child; cur != NULL; cur = cur->next) {
        uin = json_parse_simple_value(cur, "uin");
        markname = json_parse_simple_value(cur, "markname");
        if (!uin || !markname)
            continue;
        
        buddy = lwqq_buddy_find_buddy_by_uin(lc, uin);
        if (!buddy)
            continue;

        /* Free old markname */
        if (buddy->markname) 
            s_free(buddy->markname);
        buddy->markname = s_strdup(markname);
    }
}

/** 
 * Parse friends child
 * 
 * @param lc 
 * @param json Point to the first child of "result"'s value
 */
static void parse_friends_child(LwqqClient *lc, json_t *json)
{
    LwqqBuddy *buddy;
    char *uin, *cate_index;
    json_t *cur;
    
    /* Make json point "info" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "friends")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }

//    {"flag":0,"uin":1907104721,"categories":0}
    json = json->child;    //point to the array.[]
    for (cur = json->child; cur != NULL; cur = cur->next) {
        uin = json_parse_simple_value(cur, "uin");
        cate_index = json_parse_simple_value(cur, "categories");
        if (!uin || !cate_index)
            continue;
        
        buddy = lwqq_buddy_find_buddy_by_uin(lc, uin);
        if (!buddy)
            continue;

        buddy->cate_index = s_strdup(cate_index);
    }
}

/** 
 * Get QQ friends information. These information include basic friend
 * information, friends group information, and so on
 * 
 * @param lc 
 * @param err 
 */
void lwqq_info_get_friends_info(LwqqClient *lc, LwqqErrorCode *err)
{
    char msg[256] ={0};
    LwqqHttpRequest *req = NULL;  
    int ret;
    json_t *json = NULL, *json_tmp;
    char *cookies;

    if (!err) {
        return ;
    }
    
    /* Create post data: {"h":"hello","vfwebqq":"4354j53h45j34"} */
    create_post_data(lc, msg, sizeof(msg));

    /* Create a POST request */
    char url[512];
    snprintf(url, sizeof(url), "%s/api/get_user_friends2", "http://s.web2.qq.com");
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://s.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    ret = req->do_request(req, 1, msg);
    if (ret || req->http_code != 200) {
        *err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }

    /**
     * Here, we got a json object like this:
     * {"retcode":0,"result":{"friends":[{"flag":0,"uin":1907104721,"categories":0},{"flag":0,"uin":1745701153,"categories":0},{"flag":0,"uin":445284794,"categories":0},{"flag":0,"uin":4188952283,"categories":0},{"flag":0,"uin":276408653,"categories":0},{"flag":0,"uin":1526867107,"categories":0}],"marknames":[{"uin":276408653,"markname":""}],"categories":[{"index":1,"sort":1,"name":""},{"index":2,"sort":2,"name":""},{"index":3,"sort":3,"name":""}],"vipinfo":[{"vip_level":1,"u":1907104721,"is_vip":1},{"vip_level":1,"u":1745701153,"is_vip":1},{"vip_level":1,"u":445284794,"is_vip":1},{"vip_level":6,"u":4188952283,"is_vip":1},{"vip_level":0,"u":276408653,"is_vip":0},{"vip_level":1,"u":1526867107,"is_vip":1}],"info":[{"face":294,"flag":8389126,"nick":"","uin":1907104721},{"face":0,"flag":518,"nick":"","uin":1745701153},{"face":0,"flag":526,"nick":"","uin":445284794},{"face":717,"flag":8388614,"nick":"QQ","uin":4188952283},{"face":81,"flag":8389186,"nick":"Kernel","uin":276408653},{"face":0,"flag":2147484166,"nick":"Q","uin":1526867107}]}}
     * 
     */
    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of friends error: %s\n", req->response);
        *err = LWQQ_EC_ERROR;
        goto done;
    }

    json_tmp = get_result_json_object(json);
    if (!json_tmp) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", req->response);
        goto json_error;
    }
    /** It seems everything is ok, we start parsing information
     * now
     */
    if (json_tmp->child && json_tmp->child->child ) {
        json_tmp = json_tmp->child->child;

        /* Parse friend category information */
        parse_categories_child(lc, json_tmp);

        /**
         * Parse friends information.
         * Firse, we parse object's "info" child
         * Then, parse object's "marknames" child
         * Last, parse object's "friends" child
         */
        parse_info_child(lc, json_tmp);
        parse_marknames_child(lc, json_tmp);
        parse_friends_child(lc, json_tmp);
    }

done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return ;

json_error:
    *err = LWQQ_EC_ERROR;
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
}

/**
 * Parsing group info like this.
 *
 * "gnamelist":[
 *  {"flag":17825793,"name":"EGE...C/C++............","gid":3772225519,"code":1713443374},
 *  {"flag":1,"name":"............","gid":2698833507,"code":3968641865}
 * ]
 * 
 * @param lc 
 * @param json Point to the first child of "result"'s value
 */
static void parse_groups_gnamelist_child(LwqqClient *lc, json_t *json)
{
    LwqqGroup *group;
    json_t *cur;
    
    /* Make json point "gnamelist" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "gnamelist")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }
    
    json = json->child;    //point to the array.[]
    for (cur = json->child; cur != NULL; cur = cur->next) {
      	group = s_malloc0(sizeof(*group));
        group->flag = s_strdup(json_parse_simple_value(cur, "flag"));
        group->name = s_strdup(json_parse_simple_value(cur, "name"));
        group->gid = s_strdup(json_parse_simple_value(cur, "gid"));
        group->code = s_strdup(json_parse_simple_value(cur, "code"));

        /* we got the 'code', so we can get the qq group number now */
        group->account = get_group_qqnumber(lc, group->code);

        /* Add to groups list */
        LIST_INSERT_HEAD(&lc->groups, group, entries);
    }
}

/** 
 * Parse group info like this
 *
 * "gmarklist":[{"uin":2698833507,"markname":".................."}]
 * 
 * @param lc 
 * @param json Point to the first child of "result"'s value
 */
static void parse_groups_gmarklist_child(LwqqClient *lc, json_t *json)
{
    LwqqGroup *group;
    json_t *cur;
    char *uin;
    char *markname;
    
    /* Make json point "gmarklist" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "gmarklist")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }
    
    json = json->child;    //point to the array.[]
    for (cur = json->child; cur != NULL; cur = cur->next) {
        uin = json_parse_simple_value(cur, "uin");
        markname = json_parse_simple_value(cur, "markname");

        if (!uin || !markname)
            continue;
      	group = lwqq_group_find_group_by_gid(lc, uin);
        if (!group)
            continue;
        group->markname = s_strdup(markname);
    }
}

/** 
 * Get QQ groups' name information. Get only 'name', 'gid' , 'code' .
 * 
 * @param lc 
 * @param err 
 */
void lwqq_info_get_group_name_list(LwqqClient *lc, LwqqErrorCode *err)
{

  	lwqq_log(LOG_DEBUG, "in function.");

    char msg[256];
    char url[512];
    LwqqHttpRequest *req = NULL;  
    int ret;
    json_t *json = NULL, *json_tmp;
    char *cookies;

    if (!err) {
        return ;
    }
    
    /* Create post data: {"h":"hello","vfwebqq":"4354j53h45j34"} */
    create_post_data(lc, msg, sizeof(msg));

    /* Create a POST request */
    snprintf(url, sizeof(url), "%s/api/get_group_name_list_mask2", "http://s.web2.qq.com");
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://s.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    ret = req->do_request(req, 1, msg);
    if (ret || req->http_code != 200) {
        *err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }

    /**
     * Here, we got a json object like this:
     * {"retcode":0,"result":{
     * "gmasklist":[],
     * "gnamelist":[
     *  {"flag":17825793,"name":"EGE...C/C++............","gid":3772225519,"code":1713443374},
     *  {"flag":1,"name":"............","gid":2698833507,"code":3968641865}
     * ],
     * "gmarklist":[{"uin":2698833507,"markname":".................."}]}}
     * 
     */
    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of groups error: %s\n", req->response);
        *err = LWQQ_EC_ERROR;
        goto done;
    }

    json_tmp = get_result_json_object(json);
    if (!json_tmp) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", req->response);
        goto json_error;
    }
    
    /** It seems everything is ok, we start parsing information
     * now
     */
    if (json_tmp->child && json_tmp->child->child ) {
        json_tmp = json_tmp->child->child;

        /* Parse friend category information */
        parse_groups_gnamelist_child(lc, json_tmp);
        parse_groups_gmarklist_child(lc, json_tmp);
               
    }
        
done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return ;

json_error:
    *err = LWQQ_EC_ERROR;
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
}

/** 
 * Get all friends qqnumbers
 * 
 * @param lc 
 * @param err 
 */
void lwqq_info_get_all_friend_qqnumbers(LwqqClient *lc, LwqqErrorCode *err)
{
    LwqqBuddy *buddy;

    if (!lc)
        return ;
    
    LIST_FOREACH(buddy, &lc->friends, entries) {
        if (!buddy->qqnumber) {
            /** If qqnumber hasnt been fetched(NB: lc->myself has qqnumber),
             * fetch it
             */
            buddy->qqnumber = get_friend_qqnumber(lc, buddy->uin);
            lwqq_log(LOG_DEBUG, "Get buddy qqnumber: %s\n", buddy->qqnumber);
        }
    }

    if (err) {
        *err = LWQQ_EC_OK;
    }
}

/** 
 * Get friend qqnumber
 * 
 * @param lc 
 * @param uin 
 * 
 * @return qqnumber on sucessful, NB: caller is responsible for freeing
 * the memory returned by this function
 */
char *lwqq_info_get_friend_qqnumber(LwqqClient *lc, const char *uin)
{
    return get_friend_qqnumber(lc, uin);
}

/** 
 * Get QQ friend number
 * 
 * @param lc 
 * @param uin Friend's uin
 *
 * @return 
 */
static char *get_friend_qqnumber(LwqqClient *lc, const char *uin)
{
    if (!lc || !uin) {
        return NULL;
    }

    char url[512];
    LwqqHttpRequest *req = NULL;  
    int ret;
    json_t *json = NULL;
    char *qqnumber = NULL;
    char *cookies;

    if (!lc || ! uin) {
        return NULL;
    }

    /* Create a GET request */
    /*     g_sprintf(params, GETQQNUM"?tuin=%s&verifysession=&type=1&code="
           "&vfwebqq=%s&t=%ld", uin */
    snprintf(url, sizeof(url),
             "%s/api/get_friend_uin2?tuin=%s&verifysession=&type=1&code=&vfwebqq=%s",
             "http://s.web2.qq.com", uin, lc->vfwebqq);
    req = lwqq_http_create_default_request(url, NULL);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://s.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "utf-8");
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    ret = req->do_request(req, 0, NULL);
    if (ret || req->http_code != 200) {
        goto done;
    }

    /**
     * Here, we got a json object like this:
     * {"retcode":0,"result":{"uiuin":"","account":615050000,"uin":954663841}}
     * 
     */
    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of groups error: %s\n", req->response);
        goto done;
    }

    qqnumber = s_strdup(json_parse_simple_value(json, "account"));

done:
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return qqnumber;
}

/** 
 * Get QQ group number
 * 
 * @param lc 
 * @param code is group‘s code
 *
 * @return 
 */
char *get_group_qqnumber(LwqqClient *lc, const char *code)
{
    return get_friend_qqnumber(lc, code);
}

/** 
 * Get detail information of QQ friend(NB: include myself)
 * QQ server need us to pass param like:
 * tuin=244569070&verifysession=&code=&vfwebqq=e64da25c140c66
 * 
 * @param lc 
 * @param buddy 
 * @param err 
 */
void lwqq_info_get_friend_detail_info(LwqqClient *lc, LwqqBuddy *buddy,
                                      LwqqErrorCode *err)
{
    lwqq_log(LOG_DEBUG, "in function.");

    char url[512];
    LwqqHttpRequest *req = NULL;  
    int ret;
    json_t *json = NULL, *json_tmp;
    char *cookies;

    if (!lc || ! buddy) {
        return ;
    }

    /* Make sure we know uin. */
    if (!buddy->uin) {
        if (err)
            *err = LWQQ_EC_NULL_POINTER;
        return ;
    }
    
    /* Create a GET request */
    snprintf(url, sizeof(url),
             "%s/api/get_friend_info2?tuin=%s&verifysession=&code=&vfwebqq=%s",
             "http://s.web2.qq.com", buddy->uin, lc->vfwebqq);
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://s.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "utf-8");
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    ret = req->do_request(req, 0, NULL);
    if (ret || req->http_code != 200) {
        if (err)
            *err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }

    /**
     * Here, we got a json object like this:
     * {"retcode":0,"result":{"face":519,"birthday":
     * {"month":9,"year":1988,"day":26},"occupation":"学生",
     * "phone":"82888909","allow":1,"college":"西北工业大学","reg_time":0,
     * "uin":1421032531,"constel":8,"blood":2,
     * "homepage":"http://www.ifeng.com","stat":10,"vip_info":0,
     * "country":"中国","city":"西安","personal":"给力啊~~","nick":"阿凡达",
     * "shengxiao":5,"email":"avata@126.com","client_type":41,
     * "province":"陕西","gender":"male","mobile":"139********"}}
     * 
     */
    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of groups error: %s\n", req->response);
        if (err)
            *err = LWQQ_EC_ERROR;
        goto done;
    }

    json_tmp = get_result_json_object(json);
    if (!json_tmp) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", req->response);
        goto json_error;
    }

    /** It seems everything is ok, we start parsing information
     * now
     */
    if (json_tmp->child) {
        json_tmp = json_tmp->child;
#define  SET_BUDDY_INFO(key, name) {                                    \
            if (buddy->key) {                                           \
                s_free(buddy->key);                                     \
            }                                                           \
            buddy->key = s_strdup(json_parse_simple_value(json, name)); \
        }
        SET_BUDDY_INFO(uin, "uin");
        SET_BUDDY_INFO(face, "face");
        /* SET_BUDDY_INFO(birthday, "birthday"); */
        SET_BUDDY_INFO(occupation, "occupation");
        SET_BUDDY_INFO(phone, "phone");
        SET_BUDDY_INFO(allow, "allow");
        SET_BUDDY_INFO(college, "college");
        SET_BUDDY_INFO(reg_time, "reg_time");
        SET_BUDDY_INFO(constel, "constel");
        SET_BUDDY_INFO(blood, "blood");
        SET_BUDDY_INFO(homepage, "homepage");
        SET_BUDDY_INFO(stat, "stat");
        SET_BUDDY_INFO(vip_info, "vip_info");
        SET_BUDDY_INFO(country, "country");
        SET_BUDDY_INFO(city, "city");
        SET_BUDDY_INFO(personal, "personal");
        SET_BUDDY_INFO(nick, "nick");
        SET_BUDDY_INFO(shengxiao, "shengxiao");
        SET_BUDDY_INFO(email, "email");
        SET_BUDDY_INFO(client_type, "client_type");
        SET_BUDDY_INFO(province, "province");
        SET_BUDDY_INFO(gender, "gender");
        SET_BUDDY_INFO(mobile, "mobile");
#undef SET_BUDDY_INFO
    }

    
done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return ;

json_error:
    if (err)
        *err = LWQQ_EC_ERROR;
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
}
