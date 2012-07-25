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
//to enable strptime
#define __USE_XOPEN
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>

#include "info.h"
#include "url.h"
#include "logger.h"
#include "http.h"
#include "smemory.h"
#include "json.h"
#include "async.h"

static json_t *get_result_json_object(json_t *json);
static void create_post_data(LwqqClient *lc, char *buf, int buflen);
static char *get_friend_qqnumber(LwqqClient *lc, const char *uin);
char *get_group_qqnumber(LwqqClient *lc, const char *code);
static void get_friend_qqnumber_back(LwqqHttpRequest* request,void* data);
static void get_avatar_back(LwqqHttpRequest* req,void* data);

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
        buddy = lwqq_buddy_new();
        buddy->face = s_strdup(json_parse_simple_value(cur, "face"));
        buddy->flag = s_strdup(json_parse_simple_value(cur, "flag"));
        buddy->nick = s_strdup(json_parse_simple_value(cur, "nick"));
        buddy->uin = s_strdup(json_parse_simple_value(cur, "uin"));

        /* Add to buddies list */
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
        LwqqFriendCategory *c_entry;
        uin = json_parse_simple_value(cur, "uin");
        cate_index = json_parse_simple_value(cur, "categories");
        if (!uin || !cate_index)
            continue;

        buddy = lwqq_buddy_find_buddy_by_uin(lc, uin);
        if (!buddy)
            continue;

        LIST_FOREACH(c_entry, &lc->categories, entries) {
            if (c_entry->index == atoi(cate_index)) {
                c_entry->count++;
            }
        }
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
    char msg[256] = {0};
    LwqqHttpRequest *req = NULL;
    int ret;
    json_t *json = NULL, *json_tmp;
    char *cookies;

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
        if (err)
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
    if (err)
        *err = LWQQ_EC_ERROR;
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
}

int _lwqq_info_get_avatar(LwqqClient * lc,const char* uin,char** avatar,size_t* len,const char* qqnumber,LwqqErrorCode *err)
{
    static int serv_id = 0;
    if(!lc||!uin||!avatar||!len) return 0;
    //there have avatar already do not repeat work;
    if(*len) return 0;

    //to avoid chinese character
    setlocale(LC_TIME,"en_US.utf8");
    //first we try to read from disk
    char path[32];
    int hasfile=0;
    time_t modify=0;
    size_t filesize=0;
    if(qqnumber) {
        snprintf(path,sizeof(path),LWQQ_CACHE_DIR"%s",qqnumber);
        struct stat st = {0};
        //we read it last modify date
        stat(path,&st);
        hasfile = 1;
        modify = st.st_mtime;
        filesize = st.st_size;
    }

    //we send request if possible with modify time
    //to reduce download rate
    LwqqHttpRequest* req;
    char* cookies;
    int ret;
    char url[512];
    char host[32];
    //there are face 1 to face 10 server to accelerate speed.
    snprintf(host,sizeof(host),"face%d.qun.qq.com",++serv_id);
    serv_id %= 10;
    snprintf(url, sizeof(url),
             "http://%s/cgi/svr/face/getface?cache=0&type=1&fid=0&uin=%s&vfwebqq=%s",
             host,uin, lc->vfwebqq);
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://web2.qq.com/");
    req->set_header(req,"Host",host);

    //we ask server if it indeed need to update
    if(modify) {
        struct tm modify_tm;
        char buf[32];
        strftime(buf,sizeof(buf),"%a, %d %b %Y %H:%M:%S GMT",localtime_r(&modify,&modify_tm) );
        req->set_header(req,"If-Modified-Since",buf);
    }
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    ret = req->do_request(req, 0, NULL);

    if(ret||(req->http_code!=200 && req->http_code!=304)){
        if(err){
            *err = LWQQ_EC_HTTP_ERROR;
            goto done;
        }
    }

    FILE* f;
    //ok we need update our cache because 
    //our cache outdate
    if(req->http_code != 304) {
        //we 'move' it instead of copy it
        *avatar = req->response;
        req->response = NULL;
        *len = req->resp_len;
        req->resp_len = 0;

        //we cache it to file
        if(qqnumber) {
            f = fopen(path,"w");
            if(f==NULL) {
                mkdir(LWQQ_CACHE_DIR,0777);
                f = fopen(path,"w");
            }

            fwrite(*avatar,1,*len,f);
            fclose(f);
            //we read last modify time from response header
            struct tm wtm = {0};
            strptime(req->get_header(req,"Last-Modified"),
                    "%a, %d %b %Y %H:%M:%S GMT",&wtm);
            //and write it to file
            struct utimbuf wutime;
            wutime.modtime = mktime(&wtm);
            wutime.actime = wutime.modtime;//it is not important
            utime(path,&wutime);
        }

        lwqq_http_request_free(req);
        return 1;
    }


done:
    lwqq_http_request_free(req);
    //failed or we do not need update
    //we read from file

    if(hasfile){
        f = fopen(path,"r");
        *avatar = s_malloc(filesize);
        fread(*avatar,1,filesize,f);
        *len = filesize;
        return 1;
    }
    return 0;
}
static void get_avatar_back(LwqqHttpRequest* req,void* data)
{
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
        group = lwqq_group_new();
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
        if (err)
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
    if (err)
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
 * Parse group info like this
 * Is the "members" in "ginfo" useful ? Here not parsing it...
 *
 * "result":{
 * "ginfo":
 *   {"face":0,"memo":"","class":10026,"fingermemo":"","code":3968641865,"createtime":1339647698,"flag":1,
 *    "level":0,"name":"............","gid":2698833507,"owner":909998471,
 *    "members":[{"muin":56360327,"mflag":0},{"muin":909998471,"mflag":0}],
 *    "option":2},
 *
 * @param lc
 * @param group
 * @param json Point to the first child of "result"'s value
 */
static void parse_groups_ginfo_child(LwqqClient *lc, LwqqGroup *group,  json_t *json)
{
    char *gid;

    /* Make json point "ginfo" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "ginfo")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }

    //json = json->child;    // there's no array here , comment it.
    gid = json_parse_simple_value(json,"gid");
    if (strcmp(group->gid, gid) != 0) {
        lwqq_log(LOG_ERROR, "Parse json object error.");
        return;
    }

#define  SET_GINFO(key, name) {                                    \
        if (group->key) {                                               \
            s_free(group->key);                                         \
        }                                                               \
        group->key = s_strdup(json_parse_simple_value(json, name));     \
    }

    /* we have got the 'code','name' and 'gid', so we comment it here. */
    SET_GINFO(face, "face");
    SET_GINFO(memo, "memo");
    SET_GINFO(class, "class");
    SET_GINFO(fingermemo,"fingermemo");
    //SET_GINFO(code, "code");
    SET_GINFO(createtime, "createtime");
    SET_GINFO(flag, "flag");
    SET_GINFO(level, "level");
    //SET_GINFO(name, "name");
    //SET_GINFO(gid, "gid");
    SET_GINFO(owner, "owner");
    SET_GINFO(option, "option");

#undef SET_GINFO

}

/**
 * Parse group members info
 * we only get the "nick" and the "uin", and get the members' qq number.
 *
 * "minfo":[
 *   {"nick":"evildoer","province":"......","gender":"male","uin":56360327,"country":"......","city":"......"},
 *   {"nick":"evil...doer","province":"......","gender":"male","uin":909998471,"country":"......","city":"......"}],
 *
 * @param lc
 * @param group
 * @param json Point to the first child of "result"'s value
 */
static void parse_groups_minfo_child(LwqqClient *lc, LwqqGroup *group,  json_t *json)
{
    LwqqBuddy *member;
    json_t *cur;
    char *uin;
    char *nick;

    /* Make json point "minfo" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "minfo")) {
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
        nick = json_parse_simple_value(cur, "nick");

        if (!uin || !nick)
            continue;

        member = lwqq_buddy_new();

        member->uin = s_strdup(uin);
        member->nick = s_strdup(nick);

        // FIX ME: should we get group members qqnumber here ? 
        // we can get the members' qq number by uin 
        //member->qqnumber = get_friend_qqnumber(lc, member->uin);

        /* Add to members list */
        LIST_INSERT_HEAD(&group->members, member, entries);
    }
}

/**
 * mark qq group's online members
 *
 * "stats":[{"client_type":1,"uin":56360327,"stat":10},{"client_type":41,"uin":909998471,"stat":10}],
 *
 * @param lc
 * @param group
 * @param json Point to the first child of "result"'s value
 */
static void parse_groups_stats_child(LwqqClient *lc, LwqqGroup *group,  json_t *json)
{
    LwqqBuddy *member;
    json_t *cur;
    char *uin;

    /* Make json point "stats" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "stats")) {
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

        if (!uin)
            continue;
        member = lwqq_group_find_group_member_by_uin(group, uin);
        if (!member)
            continue;
        member->client_type = s_strdup(json_parse_simple_value(cur, "client_type"));
        member->stat = s_strdup(json_parse_simple_value(cur, "stat"));

    }
}

/**
 * Get QQ groups detail information.
 *
 * @param lc
 * @param group
 * @param err
 */
void lwqq_info_get_group_detail_info(LwqqClient *lc, LwqqGroup *group,
                                     LwqqErrorCode *err)
{
    lwqq_log(LOG_DEBUG, "in function.");

    char url[512];
    LwqqHttpRequest *req = NULL;
    int ret;
    json_t *json = NULL, *json_tmp;
    char *cookies;

    if (!lc || ! group) {
        return ;
    }

    /* Make sure we know code. */
    if (!group->code) {
        if (err)
            *err = LWQQ_EC_NULL_POINTER;
        return ;
    }

    /* Create a GET request */
    snprintf(url, sizeof(url),
             "%s/api/get_group_info_ext2?gcode=%s&vfwebqq=%s",
             "http://s.web2.qq.com", group->code, lc->vfwebqq);
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
     *
     * {"retcode":0,
     * "result":{
     * "stats":[{"client_type":1,"uin":56360327,"stat":10},{"client_type":41,"uin":909998471,"stat":10}],
     * "minfo":[
     *   {"nick":"evildoer","province":"......","gender":"male","uin":56360327,"country":"......","city":"......"},
     *   {"nick":"evil...doer","province":"......","gender":"male","uin":909998471,"country":"......","city":"......"}
     *   {"nick":"glrapl","province":"","gender":"female","uin":1259717843,"country":"","city":""}],
     * "ginfo":
     *   {"face":0,"memo":"","class":10026,"fingermemo":"","code":3968641865,"createtime":1339647698,"flag":1,
     *    "level":0,"name":"............","gid":2698833507,"owner":909998471,
     *    "members":[{"muin":56360327,"mflag":0},{"muin":909998471,"mflag":0}],
     *    "option":2},
     * "cards":[{"muin":3777107595,"card":""},{"muin":3437728033,"card":":FooTearth"}],
     * "vipinfo":[{"vip_level":0,"u":56360327,"is_vip":0},{"vip_level":0,"u":909998471,"is_vip":0}]}}
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
    if (json_tmp->child && json_tmp->child->child ) {
        json_tmp = json_tmp->child->child;

        /* first , get group information */
        parse_groups_ginfo_child(lc, group, json_tmp);
        /* second , get group members */
        parse_groups_minfo_child(lc, group, json_tmp);
        /* third , mark group's online members */
        parse_groups_stats_child(lc, group, json_tmp);

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
    json_t *json = NULL;
    //char *qqnumber = NULL;
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
    req->do_request_async(req, 0, NULL,get_friend_qqnumber_back,lc);
    return 0;

done:
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return 0;
}
static void get_friend_qqnumber_back(LwqqHttpRequest* req,void* data)
{
    LwqqClient* lc = data;
    char* uin;
    char* account;
    int ret;
    json_t* json=NULL;
    if (req->http_code != 200) {
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
    uin = json_parse_simple_value(json,"uin");
    account = json_parse_simple_value(json,"account");

    LwqqBuddy* buddy = lwqq_buddy_find_buddy_by_uin(lc,uin);
    if(buddy){
        buddy->qqnumber = s_strdup(account);
        lwqq_async_dispatch(lc,FRIEND_COME,buddy);
    }
    LwqqGroup* group = lwqq_group_find_group_by_gid(lc,uin);
    if(group){
        group->account = s_strdup(account);
        lwqq_async_dispatch(lc,GROUP_COME,group);
    }
done:
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return;
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

static void update_online_buddies(LwqqClient *lc, json_t *json)
{
    /**
     * The json object is:
     * [{"uin":1100872453,"status":"online","client_type":21},"
     * "{"uin":2726159277,"status":"busy","client_type":1}]
     */
    json_t *cur;
    for (cur = json->child; cur != NULL; cur = cur->next) {
        char *uin, *status, *client_type;
        LwqqBuddy *b;
        uin = json_parse_simple_value(cur, "uin");
        status = json_parse_simple_value(cur, "status");
        if (!uin || !status) {
            continue;
        }
        client_type = json_parse_simple_value(cur, "client_type");
        b = lwqq_buddy_find_buddy_by_uin(lc, uin);
        if (b) {
            s_free(b->status);
            b->status = s_strdup(status);
            if (client_type) {
                s_free(b->client_type);
                b->client_type = s_strdup(client_type);
            }
        }
    }
}

/**
 * Get online buddies
 * NB : This function must be called after lwqq_info_get_friends_info()
 * because we stored buddy's status in buddy object which is created in
 * lwqq_info_get_friends_info()
 *
 * @param lc
 * @param err
 */
void lwqq_info_get_online_buddies(LwqqClient *lc, LwqqErrorCode *err)
{
    char url[512];
    LwqqHttpRequest *req = NULL;
    int ret;
    json_t *json = NULL, *json_tmp;
    char *cookies;

    if (!lc) {
        return ;
    }

    /* Create a GET request */
    snprintf(url, sizeof(url),
             "%s/channel/get_online_buddies2?clientid=%s&psessionid=%s",
             "http://d.web2.qq.com", lc->clientid, lc->psessionid);
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", "http://d.web2.qq.com/proxy.html?v=20101025002");
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
     * {"retcode":0,"result":[{"uin":1100872453,"status":"online","client_type":21},"
     * "{"uin":2726159277,"status":"busy","client_type":1}]}
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

    if (json_tmp->child) {
        json_tmp = json_tmp->child;
        update_online_buddies(lc, json_tmp);
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
