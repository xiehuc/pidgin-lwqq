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
#include <errno.h>

#include "info.h"
#include "url.h"
#include "logger.h"
#include "http.h"
#include "smemory.h"
#include "json.h"
#include "async.h"
#include "util.h"
#include "internal.h"

#define LWQQ_CACHE_DIR "/tmp/lwqq/"

static void create_post_data(LwqqClient *lc, char *buf, int buflen);
static int get_avatar_back(LwqqHttpRequest* req,LwqqBuddy* buddy,LwqqGroup* group);
static int get_friends_info_back(LwqqHttpRequest* req);
static int get_group_name_list_back(LwqqHttpRequest* req,LwqqClient* lc);
static int group_detail_back(LwqqHttpRequest* req,LwqqClient* lc,LwqqGroup* group);
static int get_discu_list_back(LwqqHttpRequest* req,void* data);
static int get_discu_detail_info_back(LwqqHttpRequest* req,LwqqClient* lc,LwqqGroup* discu);
static void add_group_stage_1(LwqqAsyncEvent* called,LwqqVerifyCode* code,LwqqGroup* g);
static int add_group_stage_2(LwqqHttpRequest* req,LwqqGroup* g);
static void add_group_stage_4(LwqqAsyncEvent* called,LwqqVerifyCode* c,LwqqGroup* g,char* msg);

struct LwqqDiscuMemChange {
    struct str_list_* buddies;
    struct str_list_* group_members;
    struct str_list_* relate_groups;
};
#if USE_DEBUG
static int lwqq_gdb_list_group_member(LwqqGroup* g)
{
    LwqqSimpleBuddy* sb;
    int count = 0;
    LIST_FOREACH(sb,&g->members,entries){
        lwqq_verbose(1,"[uin:%s nick:%s card:%s ]",sb->uin,sb->nick,sb->card);
        count ++;
    }
    return count;
}

static int lwqq_gdb_list_buddies(LwqqClient* lc)
{
    LwqqBuddy* b;
    int count = 0;
    LIST_FOREACH(b,&lc->friends,entries){
        lwqq_verbose(1,"[uin:%s nick:%s card:%s ]",b->uin,b->nick,b->markname);
        count ++;
    }
    return count;
}
#endif

//=====================INSTRUCTION====================//
/**
 * process head is directly used in do_request_async
 * do_request_async only do process response work
 *
 *
 * parse  head is used when process json object
 * parse head return value return sub object
 * second param put result
 *
 * async event is used to do response action.
 * such as change data
 * do head is used in this.
 */


static void do_change_markname(LwqqAsyncEvent* ev,LwqqBuddy* b,LwqqGroup* g,char* mark)
{
    if(ev->failcode == LWQQ_CALLBACK_FAILED) {s_free(mark);return;}
    if(ev->result != WEBQQ_OK) {s_free(mark);return;}
    if(b){s_free(b->markname); b->markname = mark;}
    if(g){s_free(g->markname); g->markname = mark;}
}
static void do_modify_category(LwqqAsyncEvent* ev,LwqqBuddy* b,int cate)
{
    lwqq__return_if_ev_fail(ev);
    b->cate_index = cate;
}
static void do_change_status(LwqqAsyncEvent* ev,LwqqClient* lc,LwqqStatus s)
{
    if(ev->failcode == LWQQ_CALLBACK_FAILED) return;
    if(ev->result == WEBQQ_108)
        lc->dispatch(vp_func_p,(CALLBACK_FUNC)lc->async_opt->poll_lost,lc);
    if(ev->result != WEBQQ_OK) return;
    lc->stat = s;
}
static void do_mask_group(LwqqAsyncEvent* ev,LwqqGroup* g,LwqqMask m)
{
    lwqq__return_if_ev_fail(ev);
    g->mask = m;
}
static void do_rename_discu(LwqqAsyncEvent* ev,LwqqGroup* d,char* name)
{
    if(ev->failcode == LWQQ_CALLBACK_FAILED) {s_free(name);return;}
    if(ev->result != WEBQQ_OK) {s_free(name);return;}
    s_free(d->name);
    d->name = name;
}
static void do_delete_group(LwqqAsyncEvent* ev,LwqqGroup* g)
{
    lwqq__return_if_ev_fail(ev);
    LwqqClient* lc = ev->lc;
    LIST_REMOVE(g,entries);
    if(lc->async_opt && lc->async_opt->delete_group)
        lc->async_opt->delete_group(lc,g);
    lwqq_group_free(g);
}

static void do_change_discu_mem(LwqqAsyncEvent* ev,LwqqGroup* discu,LwqqDiscuMemChange* chg)
{
    int err = 0;
    LwqqClient* lc = ev->lc;
    lwqq__jump_if_ev_fail(ev,err);
    struct str_list_* ptr = chg->buddies,*g = NULL;
    while(ptr){
        if(!lwqq_group_find_group_member_by_uin(discu, ptr->str)){
            LwqqBuddy* target = lc->find_buddy_by_uin(lc,ptr->str);
            if(target){
                LwqqSimpleBuddy* sb  = lwqq_simple_buddy_new();
                sb->qq = s_strdup(target->qqnumber);
                sb->nick = s_strdup(target->nick);
                sb->card = s_strdup(target->markname);
                sb->uin = s_strdup(target->uin);
                LIST_INSERT_HEAD(&discu->members,sb,entries);
            }
        }
        ptr = ptr->next;
    }
    ptr = chg->group_members;
    g = chg->relate_groups;
    while(ptr){
        if(!lwqq_group_find_group_member_by_uin(discu, ptr->str)){
            LwqqGroup* group = lwqq_group_find_group_by_gid(lc, g->str);
            if(g){
                LwqqSimpleBuddy* target = lwqq_group_find_group_member_by_uin(group, ptr->str);
                if(target){
                    LwqqSimpleBuddy* sb  = lwqq_simple_buddy_new();
                    sb->qq = s_strdup(target->qq);
                    sb->nick = s_strdup(target->nick);
                    sb->card = s_strdup(target->card);
                    sb->uin = s_strdup(target->uin);
                    LIST_INSERT_HEAD(&discu->members,sb,entries);
                }
            }
        }
        ptr = ptr->next;
        g = g->next;
    }
done:
    if(err)
        lwqq_puts("[change discu member failed]");
    lc->async_opt->group_members_chg(lc,discu);
    lwqq_discu_mem_change_free(chg);
}

static void do_create_discu(LwqqAsyncEvent* ev,LwqqGroup* discu)
{
    int err=0;
    lwqq__jump_if_ev_fail(ev,err);
    LwqqClient* lc = ev->lc;
    LIST_INSERT_HEAD(&lc->discus,discu,entries);
    lc->async_opt->new_group(lc,discu);
done:
    if(err){
        lwqq_group_free(discu);
    }
}

static void do_modify_longnick(LwqqAsyncEvent* ev,char* longnick)
{
    int err=0;
    lwqq__jump_if_ev_fail(ev,err);
    LwqqClient* lc = ev->lc;
    lwqq__override(lc->myself->long_nick,longnick);
done:
    if(err)
        s_free(longnick);
}

#if 0
//most of the case webqq server would send blist_change message to add buddy
static void do_add_new_friend(LwqqAsyncEvent* ev,LwqqClient* lc,LwqqBuddy* b)
{
    int err=0;
    lwqq__jump_if_ev_fail(ev,err);
    LIST_INSERT_HEAD(&lc->friends,b,entries);
    lc->async_opt->new_friend(lc,b);
    b = NULL;
done:
    if(err)
        lwqq_buddy_free(b);
}
#endif

#define parse_key_value(k,v) {\
    char* s=json_parse_simple_value(json,v);\
    if(s){\
        s_free(k);\
        k=s_strdup(s);\
    }\
}
#define parse_key_string(k,v) {\
    char* s = json_parse_simple_value(json,v);\
    if(s){\
        s_free((k));\
        (k) = json_unescape(s);\
    }\
}
#define parse_key_int(k,v,d) {\
    char* s = json_parse_simple_value(json,v);\
    if(s) k = s_atoi(s,d);\
}

/**
 * Get the result object in a json object.
 *
 * @param json root object
 * @param [out] retcode ,must not be NULL
 *
 * @return result object pointer on success, else NULL on failure.
 */
static json_t* parse_key_child(json_t* json,const char* key)
{
    json_t* result = json_find_first_label(json, key);
    if(result == NULL) return NULL;
    return result->child;
}
static void parse_friend_detail(json_t* json,LwqqBuddy* buddy)
{

    //{"face":567,"birthday":{"month":6,"year":1991,"day":14},"occupation":"","phone":"","allow":1,"college":"","uin":289056851,"constel":5,"blood":0,"homepage":"","stat":10,"vip_info":0,"country":"中国","city":"威海","personal":"","nick":"d3dd","shengxiao":8,"email":"","client_type":41,"province":"山东","gender":"male","mobile":""}}
    //
    //
#define  SET_BUDDY_INFO(key, name) parse_key_value(buddy->key,name)
        SET_BUDDY_INFO(uin, "uin");
        SET_BUDDY_INFO(face, "face");
        json_t* birth_tmp = json_find_first_label(json, "birthday");
        if(birth_tmp && birth_tmp->child ){
            birth_tmp = birth_tmp->child;
            struct tm tm_ = {0};
            tm_.tm_year = s_atoi(json_parse_simple_value(birth_tmp, "year"),1991)-1900;
            tm_.tm_mon = s_atoi(json_parse_simple_value(birth_tmp, "month"), 1) -1;
            tm_.tm_mday = s_atoi(json_parse_simple_value(birth_tmp, "day"), 1);
            time_t t = mktime(&tm_);
            buddy->birthday = t;
        }
        SET_BUDDY_INFO(occupation, "occupation");
        SET_BUDDY_INFO(phone, "phone");
        SET_BUDDY_INFO(allow, "allow");
        SET_BUDDY_INFO(college, "college");
        SET_BUDDY_INFO(reg_time, "reg_time");
        SET_BUDDY_INFO(constel, "constel");
        SET_BUDDY_INFO(blood, "blood");
        SET_BUDDY_INFO(homepage, "homepage");
        buddy->stat = s_atoi(json_parse_simple_value(json,"stat"),LWQQ_STATUS_LOGOUT);
        SET_BUDDY_INFO(vip_info, "vip_info");
        SET_BUDDY_INFO(country, "country");
        SET_BUDDY_INFO(city, "city");
        SET_BUDDY_INFO(personal, "personal");
        SET_BUDDY_INFO(nick, "nick");
        SET_BUDDY_INFO(shengxiao, "shengxiao");
        SET_BUDDY_INFO(email, "email");
        buddy->client_type = s_atoi(json_parse_simple_value(json,"client_type"),LWQQ_CLIENT_PC);
        SET_BUDDY_INFO(province, "province");
        SET_BUDDY_INFO(gender, "gender");
        SET_BUDDY_INFO(mobile, "mobile");
        SET_BUDDY_INFO(token, "token");
        SET_BUDDY_INFO(qqnumber, "account");
#undef SET_BUDDY_INFO
}

static void parse_and_do_set_status(json_t* cur,LwqqClient* lc)
{
    //{"uin":1100872453,"status":"online","client_type":21}
    char *uin, *status, *client_type;
    LwqqBuddy *b;
    uin = json_parse_simple_value(cur, "uin");
    status = json_parse_simple_value(cur, "status");
    if (!uin || !status) return;
    client_type = json_parse_simple_value(cur, "client_type");
    b = lwqq_buddy_find_buddy_by_uin(lc, uin);
    if (b) {
        b->stat = lwqq_status_from_str(status);
        if (client_type) {
            b->client_type = s_atoi(client_type,LWQQ_CLIENT_PC);
        }
    }
}

static void parse_group_info(json_t* json,LwqqGroup* g)
{
    //{"face":0,"memo":"","member_cnt":2,"class":10011,"fingermemo":"","code":492623520,"createtime":1344433413,"flag":16842753,"name":"group\u0026test","gid":4072534964,"owner":586389001,"maxmember":100,"option":2}
    if(!json) return;
    parse_key_value(g->class,"class");
    parse_key_value(g->code,"code");
    parse_key_int(g->createtime,"createtime",0);
    parse_key_value(g->face,"face");
    parse_key_value(g->flag,"flag");
    parse_key_value(g->gid,"gid");
    parse_key_string(g->name,"name");
    parse_key_value(g->option,"option");
    parse_key_value(g->owner,"owner");
}

static void parse_business_card(json_t* json,LwqqBusinessCard* c)
{
    //{"phone":"","muin":xxxxxxxxx,"email":"","remark":"","gcode":409088807,"name":"xiehuc","gender":2}
    if(!json) return;
    parse_key_value(c->phone,"phone");
    parse_key_value(c->uin,"muin");
    parse_key_value(c->email,"email");
    parse_key_string(c->remark,"remark");
    parse_key_value(c->gcode,"gcode");
    parse_key_string(c->name,"name");
    parse_key_int(c->gender,"gender",LWQQ_MALE);
}
/**
 * this process simple result;
 */
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
static int process_friend_detail(LwqqHttpRequest* req,LwqqBuddy* out)
{
    //{"retcode":0,"result":{"face":567,"birthday":{"month":x,"year":xxxx,"day":xxxx},"occupation":"","phone":"","allow":1,"college":"","constel":5,"blood":0,"stat":20,"homepage":"","country":"中国","city":"xx","uiuin":"","personal":"","nick":"xxx","shengxiao":x,"email":"","token":"523678fd7cff12223ef9906a4e924a4bf733108b9532c27c","province":"xx","account":xxx,"gender":"male","tuin":3202680423,"mobile":""}}
    int err = 0;
    json_t *root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq_verbose(3,"%s\n",req->response);
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    //WEBQQ_FATAL:验证码输入错误.
    if(err != WEBQQ_OK) goto done;
    if(result)
        parse_friend_detail(result,out);
    else err = LWQQ_EC_NO_RESULT;

done:
    lwqq__clean_json_and_req(root,req);
    return err;
}

static int process_qqnumber(LwqqHttpRequest* req,LwqqBuddy* b,LwqqGroup* g)
{
    //{"retcode":0,"result":{"uiuin":"","account":1154230227,"uin":2379149875}}
    int err = 0;
    json_t *root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    if(result){
        char* account = lwqq__json_get_value(result,"account");
        if(b&&account){
            s_free(b->qqnumber);b->qqnumber = account;
        }
        if(g&&account){
            s_free(g->account);g->account = account;
        }
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}

static int process_online_buddies(LwqqHttpRequest* req,LwqqClient* lc)
{
    //{"retcode":0,"result":[{"uin":1100872453,"status":"online","client_type":21},"
    // "{"uin":2726159277,"status":"busy","client_type":1}]}
    int err = 0;
    json_t *root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    req->response[req->resp_len] = '\0';
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    lwqq__jump_if_retcode_fail(err);
    if(result) {
        result = result->child;
        while(result){
            parse_and_do_set_status(result,lc);
            result = result->next;
        }
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}
static int process_group_info(LwqqHttpRequest*req,LwqqGroup* g)
{
    //{"retcode":0,"result":{"ginfo":{"face":0,"memo":"","member_cnt":2,"class":10011,"fingermemo":"","code":492623520,"createtime":1344433413,"flag":16842753,"name":"group\u0026test","gid":4072534964,"owner":586389001,"maxmember":100,"option":2}}}
    int err = 0;
    json_t *root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    lwqq__jump_if_retcode_fail(err);
    if(result){
        parse_group_info(parse_key_child(result, "ginfo"),g);
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}

static int process_business_card(LwqqHttpRequest* req,LwqqBusinessCard* card)
{
    //{"retcode":0,"result":{"phone":"","muin":xxxxxxxxx,"email":"","remark":"","gcode":409088807,"name":"xiehuc","gender":2}}
    int err = 0;
    json_t *root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq_verbose(3,"%s\n",req->response);
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    lwqq__jump_if_retcode_fail(err);
    if(result){
        parse_business_card(result,card);
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}

static int process_simple_string(LwqqHttpRequest* req,const char* key,char ** ptr)
{
    int err = 0;
    json_t *json = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq_verbose(3,"%s\n",req->response);
    lwqq__jump_if_json_fail(json,req->response,err);
    result = lwqq__parse_retcode_result(json, &err);
    lwqq__jump_if_retcode_fail(err);
    if(result){
        lwqq__override(*ptr,lwqq__json_get_string(result,key));
        /*char* str = parse_string(result, key);
        if(str){ s_free(*ptr); *ptr = str;}*/
    }
done:
    lwqq__clean_json_and_req(json,req);
    return err;
}

static int process_recent_list(LwqqHttpRequest* req,LwqqRecentList* list)
{
    //{"retcode":0,"result":[{"uin":273149476,"type":1},{"uin":2585596768,"type":0},{"uin":1971296934,"type":2}
    int err = 0;
    json_t *json = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq_verbose(3,"%s\n",req->response);
    lwqq__jump_if_json_fail(json,req->response,err);
    result = lwqq__parse_retcode_result(json, &err);
    lwqq__jump_if_retcode_fail(err);
    if(result&&result->child){
        result = result->child_end;
        while(result){
            LwqqRecentItem* recent = s_malloc0(sizeof(*recent));
            recent->type = lwqq__json_get_int(result,"type",0);
            recent->uin = lwqq__json_get_value(result,"uin");
            LIST_INSERT_HEAD(list,recent,entries);
            result = result->previous;
        }
    }
done:
    lwqq__clean_json_and_req(json,req);
    return err;
}

static int process_qq_level(LwqqHttpRequest* req,LwqqBuddy* b)
{
    //{"retcode":0,"result":{"level":37,"days":1534,"hours":12219,"remainDays":62,"tuin":1990734287}}
    int err = 0;
    json_t *root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    if(result){
        char* raw_level = lwqq__json_get_value(result,"level");
        b->level = s_atoi(raw_level, 0);
        s_free(raw_level);
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}



//======================OLD UNCLEARD API==================//
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
        cate = s_malloc0(sizeof(*cate));
        cate->index = s_atoi(json_parse_simple_value(cur, "index"),0);
        cate->sort = s_atoi(json_parse_simple_value(cur, "sort"),0);
        cate->name = s_strdup(json_parse_simple_value(cur, "name"));

        /* Add to categories list */
        LIST_INSERT_HEAD(&lc->categories, cate, entries);
    }

    /* add the default category */
    cate = s_malloc0(sizeof(*cate));
    cate->index = 0;
    cate->name = s_strdup(LWQQ_DEFAULT_CATE);
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
        buddy->nick = json_unescape(json_parse_simple_value(cur, "nick"));
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
        buddy->markname = json_unescape(markname);
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
    char *uin;
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
        int cate_index = s_atoi(json_parse_simple_value(cur, "categories"),0);
        if (!uin || !cate_index)
            continue;

        buddy = lwqq_buddy_find_buddy_by_uin(lc, uin);
        if (!buddy)
            continue;

        LIST_FOREACH(c_entry, &lc->categories, entries) {
            if (c_entry->index == cate_index) {
                c_entry->count++;
            }
        }
        buddy->cate_index = cate_index;
    }
}

#if 0
static char* hashN(const char* uin,const char* ptwebqq)
{
    int alen=strlen(uin);
    int *c = malloc(sizeof(int)*strlen(uin));
    int d,b,k,clen;
    int elen=strlen(ptwebqq);
    const char* e = ptwebqq;
    int h;
    int i;
    for(d=0;d<alen;d++){
        c[d]=uin[d]-'0';
    }
    clen = d;
    for(b=0,k=-1,d=0;d<clen;d++){
        b += c[d];
        b %= elen;
        int f = 0;
        if(b+4>elen){
            int g;
            for(g=4+b-elen,h=0;h<4;h++)
                f |= h<g?((e[b+h]&255)<<(3-h)*8):((e[h-g]&255)<<(3-h)*8);
        }else{
            for(h=0;h<4;h++)
                f |= (e[b+h]&255)<<(3-h)*8;
        }
        k ^= f;
    }
    memset(c,0,sizeof(int)*alen);
    c[0] = k >> 24&255;
    c[1] = k >> 16&255;
    c[2] = k >> 8&255;
    c[3] = k & 255;
    const char* ch = "0123456789ABCDEF";
    char* ret = malloc(10);
    memset(ret,0,10);
    for(b=0,i=0;b<4;b++){
        ret[i++]=ch[c[b]>>4&15];
        ret[i++]=ch[c[b]&15];
    }
    free(c);
    return ret;
}
#endif

static char* hashO(const char* uin,const char* ptwebqq)
{
    char* a = s_malloc0(strlen(ptwebqq)+strlen("password error")+3);
    const char* b = uin;
    strcat(strcpy(a,ptwebqq),"password error");
    size_t alen = strlen(a);
    char* s = s_malloc0(2048);
    int *j = malloc(sizeof(int)*alen);
    for(;;){
        if(strlen(s)<=alen){
            if(strcat(s,b),strlen(s)==alen) break;
        }else{
            s[alen]='\0';
            break;
        }
    }
    int d;
    for(d=0;d<strlen(s);d++){
        j[d]=s[d]^a[d];
    }
    const char* ch = "0123456789ABCDEF";
    s[0]=0;
    for(d=0;d<alen;d++){
        s[2*d]=ch[j[d]>>4&15];
        s[2*d+1]=ch[j[d]&15];
    }
    s_free(a);
    s_free(j);
    return s;
}
/**
 * Get QQ friends information. These information include basic friend
 * information, friends group information, and so on
 *
 * @param lc
 * @param err
 */
LwqqAsyncEvent* lwqq_info_get_friends_info(LwqqClient *lc, LwqqErrorCode *err)
{
    char post[512];
    LwqqHttpRequest *req = NULL;

    char* hash = hashO(lc->myself->uin, lc->cookies->ptwebqq);
    /* Create post data: {"h":"hello","vfwebqq":"4354j53h45j34"} */
    snprintf(post, sizeof(post), "r={\"h\":\"hello\",\"hash\":\"%s\",\"vfwebqq\":\"%s\"}",hash,lc->vfwebqq);
    s_free(hash);

    lwqq_verbose(3,"%s\n",post);
    /* Create a POST request */
    const char* url = WEBQQ_S_HOST"/api/get_user_friends2";
    req = lwqq_http_create_default_request(lc,url, err);
    if (!req) {
        goto done;
    }
    lwqq_verbose(3,"%s\n",url);
    req->set_header(req, "Referer", WEBQQ_S_REF_URL);
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    return req->do_request_async(req, 1, post,_C_(p_i,get_friends_info_back,req));

    /**
     * Here, we got a json object like this:
     * {"retcode":0,"result":{
     * "friends":[{"flag":0,"uin":1907104721,"categories":0},i...
     * "marknames":[{"uin":276408653,"markname":""},...
     * "categories":[{"index":1,"sort":1,"name":""},...
     * "vipinfo":[{"vip_level":1,"u":1907104721,"is_vip":1},i...
     * "info":[{"face":294,"flag":8389126,"nick":"","uin":1907104721},
     *
     */
done:
    lwqq_http_request_free(req);
    return NULL;
}
static int get_friends_info_back(LwqqHttpRequest* req)
{
    json_t *json = NULL, *json_tmp;
    int ret = 0;
    int err = 0;
    int retcode = 0;
    LwqqClient* lc = req->lc;

    if (req->http_code != 200) {
        err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }
    //force end with char zero.
    req->response[req->resp_len] = '\0';
    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of friends error: \n%s\n", req->response);
        err = LWQQ_EC_ERROR;
        goto done;
    }

    json_tmp = lwqq__parse_retcode_result(json, &retcode);
    if (retcode != WEBQQ_OK){
        err = retcode;
        goto done;
    }
    if (!json_tmp) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", req->response);
        err = LWQQ_EC_ERROR;
        goto done;
    }
    /** It seems everything is ok, we start parsing information
     * now
     */
    if (json_tmp->child) {
        json_tmp = json_tmp->child;

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
    if(err){
        lwqq_verbose(3,"%s\n",req->response);
    }
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return err;
}

LwqqAsyncEvent* lwqq_info_get_avatar(LwqqClient* lc,LwqqBuddy* buddy,LwqqGroup* group)
{
    static int serv_id = 0;
    if(!(lc&&(group||buddy))) return NULL;
    //there have avatar already do not repeat work;
    LwqqErrorCode error;
    int isgroup = group>0;
    //const char* qqnumber = (isgroup)?group->account:buddy->qqnumber;
    const char* uin = (isgroup)?group->code:buddy->uin;

    //to avoid chinese character
    //setlocale(LC_TIME,"en_US.utf8");
    //first we try to read from disk
    /*
    char path[32];
    time_t modify=0;
    if(qqnumber || uin) {
        snprintf(path,sizeof(path),LWQQ_CACHE_DIR"%s",qqnumber?qqnumber:uin);
        struct stat st = {0};
        //we read it last modify date
        stat(path,&st);
        modify = st.st_mtime;
    }*/
    //we send request if possible with modify time
    //to reduce download rate
    LwqqHttpRequest* req;
    char url[512];
    char host[32];
    int type = (isgroup)?4:1;
    //there are face 1 to face 10 server to accelerate speed.
    snprintf(host,sizeof(host),"face%d.qun.qq.com",++serv_id);
    serv_id %= 10;
    snprintf(url, sizeof(url),
             "http://%s/cgi/svr/face/getface?cache=0&type=%d&fid=0&uin=%s&vfwebqq=%s",
             host,type,uin, lc->vfwebqq);
    lwqq_verbose(3,"%s\n",url);
    req = lwqq_http_create_default_request(lc,url, &error);
    req->set_header(req, "Referer", "http://web2.qq.com/");
    req->set_header(req,"Host",host);

    //we ask server if it indeed need to update
    /*if(modify) {
        struct tm modify_tm;
        char buf[32];
        strftime(buf,sizeof(buf),"%a, %d %b %Y %H:%M:%S GMT",localtime_r(&modify,&modify_tm) );
        req->set_header(req,"If-Modified-Since",buf);
    }*/
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    return req->do_request_async(req, 0, NULL,_C_(3p_i,get_avatar_back,req,buddy,group));
}
static int get_avatar_back(LwqqHttpRequest* req,LwqqBuddy* buddy,LwqqGroup* group)
{
    if(req == NULL)
        return LWQQ_EC_ERROR;
    int isgroup = (group !=NULL);
    char** avatar = (isgroup)?&group->avatar:&buddy->avatar;
    size_t* len = (isgroup)?&group->avatar_len:&buddy->avatar_len;

    if((req->http_code!=200 && req->http_code!=304)){
        goto done;
    }

    if(req->http_code == 200){
        *avatar = req->response;
        *len = req->resp_len;
        req->response = NULL;
        req->resp_len = 0;
    }
done:
    lwqq_http_request_free(req);
    return -1;
}
LwqqErrorCode lwqq_info_save_avatar(LwqqBuddy* b,LwqqGroup* g,const char* path)
{
    char* path_;
    char* avatar = b?b->avatar:g->avatar;
    size_t avatar_len = b?b->avatar_len:g->avatar_len;
    int err = LWQQ_EC_OK;
    if(avatar_len==0||avatar==NULL) return LWQQ_EC_OK;

    if(path == NULL){
        char* qqnumber = b?b->qqnumber:g->account;
        char* uin = b?b->uin:g->gid;
        char* key = qqnumber?:uin;
        path_ = s_malloc0(256);
        snprintf(path_,256,LWQQ_CACHE_DIR"/%s",key);
    }else
        path_ = s_strdup(path);
    FILE* f = fopen(path_,"wb");
    if(f==NULL&&errno==2){
        //no such directory or file
        mkdir(LWQQ_CACHE_DIR,0777);
        f = fopen(path_,"wb");
    }
    if(f==NULL){
        lwqq_log(LOG_ERROR,"%d:%s\n",errno,strerror(errno));
        err = LWQQ_EC_ERROR;
        goto done;
    }

    fwrite(avatar, avatar_len, 1, f);
    fclose(f);
done:
    s_free(path_);
    return err;
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
        group = lwqq_group_new(0);
        group->flag = s_strdup(json_parse_simple_value(cur, "flag"));
        group->name = json_unescape(json_parse_simple_value(cur, "name"));
        group->gid = s_strdup(json_parse_simple_value(cur, "gid"));
        group->code = s_strdup(json_parse_simple_value(cur, "code"));

        /* we got the 'code', so we can get the qq group number now */
        //group->account = get_group_qqnumber(lc, group->code);

        /* Add to groups list */
        LIST_INSERT_HEAD(&lc->groups, group, entries);
    }
}

static void parse_groups_gmasklist_child(LwqqClient *lc, json_t *json)
{
    while (json) {
        if (json->text && !strcmp(json->text, "gmasklist")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }
    json = json->child->child;

    const char* gid;
    int mask;
    LwqqGroup* group;
    while(json){
        gid = json_parse_simple_value(json,"gid");
        mask = s_atoi(json_parse_simple_value(json,"mask"),LWQQ_MASK_NONE);

        group = lwqq_group_find_group_by_gid(lc,gid);
        if(group){
            group->mask = mask;
        }
        json = json->next;
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
        group->markname = json_unescape(markname);
    }
}

/**
 * Get QQ groups' name information. Get only 'name', 'gid' , 'code' .
 *
 * @param lc
 * @param err
 */
LwqqAsyncEvent* lwqq_info_get_group_name_list(LwqqClient *lc, LwqqErrorCode *err)
{

    char msg[256];
    LwqqHttpRequest *req = NULL;

    /* Create post data: {"h":"hello","vfwebqq":"4354j53h45j34"} */
    create_post_data(lc, msg, sizeof(msg));

    /* Create a POST request */
    const char* url =  WEBQQ_S_HOST"/api/get_group_name_list_mask2";
    req = lwqq_http_create_default_request(lc,url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", WEBQQ_S_REF_URL);
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    return req->do_request_async(req, 1, msg,_C_(2p_i,get_group_name_list_back,req,lc));
done:
    lwqq_http_request_free(req);
    return NULL;
}
static int get_group_name_list_back(LwqqHttpRequest* req,LwqqClient* lc)
{
    json_t *json = NULL, *json_tmp;
    int ret=0;
    int err=0;

    if (req->http_code != 200) {
        err = LWQQ_EC_HTTP_ERROR;
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
    req->response[req->resp_len] = '\0';
    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of groups error: %s\n", req->response);
        err = LWQQ_EC_ERROR;
        goto done;
    }

    int retcode;
    json_tmp = lwqq__parse_retcode_result(json,&retcode);
    if (!json_tmp||retcode != WEBQQ_OK) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", req->response);
        err = LWQQ_EC_ERROR;
        goto done;
    }

    /** It seems everything is ok, we start parsing information
     * now
     */
    if (json_tmp->child ) {
        json_tmp = json_tmp->child;

        /* Parse friend category information */
        parse_groups_gnamelist_child(lc, json_tmp);
        parse_groups_gmasklist_child(lc, json_tmp);
        parse_groups_gmarklist_child(lc, json_tmp);

    }

done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return err;

}

LwqqAsyncEvent* lwqq_info_get_discu_name_list(LwqqClient* lc)
{
    if(!lc) return NULL;

    char url[512];
    snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/get_discu_list_new2?clientid=%s&psessionid=%s&vfwebqq=%s&t=%ld",
            lc->clientid,lc->psessionid,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url, NULL);
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    
    return req->do_request_async(req,0,NULL,_C_(2p_i,get_discu_list_back,req,lc));
}

static void parse_discus_discu_child(LwqqClient* lc,json_t* root)
{
    json_t* json = json_find_first_label(root, "dnamelist");
    if(json == NULL) return;
    json = json->child->child;
    char* name;
    while(json){
        LwqqGroup* discu = lwqq_group_new(LWQQ_GROUP_DISCU);
        discu->did = s_strdup(json_parse_simple_value(json,"did"));
        //for compability
        discu->account = s_strdup(discu->did);
        name = json_parse_simple_value(json,"name");
        if(strcmp(name,"")==0)
            discu->name = s_strdup("未命名讨论组");
        else
            discu->name = json_unescape(name);
        LIST_INSERT_HEAD(&lc->discus, discu, entries);
        json=json->next;
    }

    json = json_find_first_label(root,"dmasklist");
    if(json == NULL) return;
    json = json->child->child;
    const char* did;
    int mask;
    LwqqGroup* group;
    while(json){
        did = json_parse_simple_value(json,"did");
        mask = s_atoi(json_parse_simple_value(json,"mask"),LWQQ_MASK_NONE);
        group = lwqq_group_find_group_by_gid(lc,did);
        if(group!=NULL) group->mask = mask;
        json = json->next;
    }
}

static int get_discu_list_back(LwqqHttpRequest* req,void* data)
{
    LwqqClient* lc = data;
    int err=0;
    int retcode;
    json_t* root = NULL;
    json_t* json_temp = NULL;
    if(req->http_code!=200){
        err = 1;
        goto done;
    }
    req->response[req->resp_len] = '\0';
    json_parse_document(&root, req->response);
    if(!root){ err = 1; goto done;}
    json_temp = lwqq__parse_retcode_result(root, &retcode);
    if(!json_temp || retcode != WEBQQ_OK) {err=1;goto done;}

    if (json_temp ) {
        parse_discus_discu_child(lc,json_temp);
    }


done:
    if(root)
        json_free_value(&root);
    lwqq_http_request_free(req);
    return err;
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
            lwqq_info_get_friend_qqnumber(lc,buddy);
            //lwqq_log(LOG_DEBUG, "Get buddy qqnumber: %s\n", buddy->qqnumber);
        }
    }

    if (err) {
        *err = LWQQ_EC_OK;
    }
}


static char* ibmpc_ascii_character_convert(char *str)
{
    static char buf[256];
    char *ptr = str;
    char *write = buf;
    const char* spec;
    while(*ptr!='\0'){
        switch(*ptr){
            case 0x01:spec="☺";break;
            case 0x02:spec="☻";break;
            case 0x03:spec="♥";break;
            case 0x04:spec="◆";break;
            case 0x05:spec="♣";break;
            case 0x06:spec="♠";break;
            case 0x07:spec="⚫";break;
            case 0x08:spec="⚫";break;
            case 0x09:spec="⚪";break;
            case 0x0A:spec="⚪";break;
            case 0x0B:spec="♂";break;
            case 0x0C:spec="♁";break;
            case 0x0D:spec="♪";break;
            case 0x0E:spec="♬";break;
            case 0x0F:spec="☼";break;
            case 0x10:spec="▶";break;
            case 0x11:spec="◀";break; 
            case 0x12:spec="↕";break;
            case 0x13:spec="‼";break;
            case 0x14:spec="¶";break;
            case 0x15:spec="§";break;
            case 0x16:spec="▁";break;
            case 0x17:spec="↨";break;
            case 0x18:spec="↑";break;
            case 0x19:spec="↓";break;
            case 0x1A:spec="→";break;
            case 0x1B:spec="←";break;
            case 0x1C:spec="∟";break;
            case 0x1D:spec="↔";break;
            case 0x1E:spec="▲";break;
            case 0x1F:spec="▼";break;
            default:
                *write++ = *ptr;
                ptr++;
                continue;
                break;
        }
        strcpy(write,spec);
        write+=strlen(write);
        ptr++;
    }
    *write = '\0';
    s_free(str);
    return s_strdup(buf);
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
    LwqqSimpleBuddy *member;
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

        //may solve group member duplicated problem
        if(lwqq_group_find_group_member_by_uin(group,uin)==NULL){

            member = lwqq_simple_buddy_new();

            member->uin = s_strdup(uin);
            member->nick = ibmpc_ascii_character_convert(json_unescape(nick));

            /* Add to members list */
            LIST_INSERT_HEAD(&group->members, member, entries);
        }
    }
}
static void parse_groups_ginfo_members_child(LwqqClient *lc, LwqqGroup *group,  json_t *json)
{
    while (json) {
        if (json->text && !strcmp(json->text, "ginfo")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }
    json_t* members = json_find_first_label_all(json,"members");
    members = members->child->child;
    const char* uin;
    int mflag;
    LwqqSimpleBuddy* sb;
    while(members){
        uin = json_parse_simple_value(members,"muin");
        mflag = s_atoi(json_parse_simple_value(members,"mflag"),0);
        sb = lwqq_group_find_group_member_by_uin(group,uin);
        if(sb) sb->mflag = mflag;

        members = members->next;
    }

}
static void parse_groups_cards_child(LwqqClient *lc, LwqqGroup *group,  json_t *json)
{
    LwqqSimpleBuddy *member;
    json_t *cur;
    char *uin;
    char *card;

    /* Make json point "minfo" reference */
    while (json) {
        if (json->text && !strcmp(json->text, "cards")) {
            break;
        }
        json = json->next;
    }
    if (!json) {
        return ;
    }

    json = json->child;    //point to the array.[]
    for (cur = json->child; cur != NULL; cur = cur->next) {
        uin = json_parse_simple_value(cur, "muin");
        card = json_parse_simple_value(cur, "card");

        if (!uin || !card)
            continue;

        member = lwqq_group_find_group_member_by_uin(group,uin);
        if(member != NULL){
            member->card = ibmpc_ascii_character_convert(json_unescape(card));
        }
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
    LwqqSimpleBuddy *member;
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
        member->client_type = s_atoi(json_parse_simple_value(cur, "client_type"),LWQQ_CLIENT_PC);
        member->stat = s_atoi(json_parse_simple_value(cur, "stat"),LWQQ_STATUS_LOGOUT);

    }
}

/**
 * Get QQ groups detail information.
 *
 * @param lc
 * @param group
 * @param err
 */
LwqqAsyncEvent* lwqq_info_get_group_detail_info(LwqqClient *lc, LwqqGroup *group,
                                     LwqqErrorCode *err)
{
    char url[512];
    LwqqHttpRequest *req = NULL;

    if (!lc || ! group) {
        return NULL;
    }

    LwqqAsyncEvent* ev = lwqq_async_queue_find(&group->ev_queue,lwqq_info_get_group_detail_info);
    if(ev) return ev;

    if(group->type == LWQQ_GROUP_QUN){
        /* Make sure we know code. */
        if (!group->code) {
            if (err)
                *err = LWQQ_EC_NULL_POINTER;
            return NULL;
        }

        /* Create a GET request */
        snprintf(url, sizeof(url),
                WEBQQ_S_HOST"/api/get_group_info_ext2?gcode=%s&vfwebqq=%s&t=%ld",
                group->code, lc->vfwebqq,time(NULL));
        /*snprintf(url, sizeof(url),
                "%s/api/get_group_info?gcode=%%5B%s%%5D&retainKey=minfo&vfwebqq=%s&t=%ld",
                "http://s.web2.qq.com", group->code, lc->vfwebqq,time(NULL));*/
        req = lwqq_http_create_default_request(lc,url, err);
        req->set_header(req, "Referer", WEBQQ_S_REF_URL);
        lwqq_verbose(3,"%s\n",url);

    }else if(group->type == LWQQ_GROUP_DISCU){

        snprintf(url,sizeof(url),
                WEBQQ_D_HOST"/channel/get_discu_info?"
                "did=%s&clientid=%s&psessionid=%s&t=%ld",
                group->did,lc->clientid,lc->psessionid,time(NULL));
        req = lwqq_http_create_default_request(lc,url,NULL);
        req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    }else
        //this never come. hotfix compile warnning
        return NULL;
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    lwqq_http_set_option(req, LWQQ_HTTP_TIMEOUT,120L);
    ev = req->do_request_async(req, 0, NULL,_C_(3p_i,group_detail_back,req,lc,group));
    lwqq_async_queue_add(&group->ev_queue,lwqq_info_get_group_detail_info,ev);
    return ev;
}

static int group_detail_back(LwqqHttpRequest* req,LwqqClient* lc,LwqqGroup* group)
{
    lwqq_async_queue_rm(&group->ev_queue,lwqq_info_get_group_detail_info);
    if(group->type == LWQQ_GROUP_DISCU){
        return get_discu_detail_info_back(req,lc,group);
    }
    int ret;
    int err = 0;
    json_t *json = NULL, *json_tmp;
    lwqq_puts("[group detail back]");
    if (req->http_code != 200) {
        err = LWQQ_EC_HTTP_ERROR;
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
    req->response[req->resp_len] = '\0';
    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        lwqq_log(LOG_ERROR, "Parse json object of groups error: %s\n", req->response);
        //assert(0);
        err = LWQQ_EC_ERROR;
        goto done;
    }

    int retcode;
    json_tmp = lwqq__parse_retcode_result(json, &retcode);
    if (!json_tmp || retcode != WEBQQ_OK) {
        lwqq_log(LOG_ERROR, "Parse json object error: %s\n", req->response);
        goto json_error;
    }

    /** It seems everything is ok, we start parsing information
     * now
     */
    if (json_tmp->child ) {
        json_tmp = json_tmp->child;

        /* first , get group information */
        parse_group_info(parse_key_child(json_tmp->parent, "ginfo"), group);
        /* second , get group members */
        parse_groups_minfo_child(lc, group, json_tmp);
        parse_groups_ginfo_members_child(lc,group,json_tmp);
        parse_groups_cards_child(lc, group, json_tmp);
        /* third , mark group's online members */
        parse_groups_stats_child(lc, group, json_tmp);

    }

done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return err;

json_error:
    err = LWQQ_EC_ERROR;
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return err;
}

static void parse_discus_info_child(LwqqClient* lc,LwqqGroup* discu,json_t* root)
{
    json_t* json = json_find_first_label(root,"info");
    json = json->child;

    discu->owner = s_strdup(json_parse_simple_value(json,"discu_owner"));

    json = json_find_first_label(json,"mem_list");
    json = json->child->child;
    while(json){
        LwqqSimpleBuddy* sb = lwqq_simple_buddy_new();
        sb->uin = s_strdup(json_parse_simple_value(json,"mem_uin"));
        sb->qq = s_strdup(json_parse_simple_value(json,"ruin"));
        LIST_INSERT_HEAD(&discu->members,sb,entries);
        json = json->next;
    }
}
static void parse_discus_other_child(LwqqClient* lc,LwqqGroup* discu,json_t* root)
{
    json_t* json = json_find_first_label(root,"mem_info");

    json = json->child->child;
    const char* uin;

    while(json){
        uin = json_parse_simple_value(json,"uin");
        LwqqSimpleBuddy* sb = lwqq_group_find_group_member_by_uin(discu,uin);
        sb->nick = json_unescape(json_parse_simple_value(json,"nick"));
        json = json->next;
    }

    json = json_find_first_label(root,"mem_status");
    
    json = json->child->child;

    while(json){
        uin = json_parse_simple_value(json,"uin");
        LwqqSimpleBuddy* sb = lwqq_group_find_group_member_by_uin(discu,uin);
        sb->client_type = s_atoi(json_parse_simple_value(json,"client_type"),LWQQ_CLIENT_PC);
        sb->stat = lwqq_status_from_str(json_parse_simple_value(json,"status"));
        json = json->next;
    }
}
static int get_discu_detail_info_back(LwqqHttpRequest* req,LwqqClient* lc,LwqqGroup* discu)
{
    int err = 0;
    int retcode;
    json_t* root = NULL,* json = NULL;

    if(req->http_code!=200){
        err = 1;
        goto done;
    }
    
    req->response[req->resp_len] = '\0';
    json_parse_document(&root,req->response);
    json = lwqq__parse_retcode_result(root, &retcode);
    if(json) {
        parse_discus_info_child(lc,discu,json);
        parse_discus_other_child(lc,discu,json);
    }
done:
    if(root)
        json_free_value(&root);
    lwqq_http_request_free(req);
    return err;
}

LwqqAsyncEvent* lwqq_info_get_qqnumber(LwqqClient* lc,LwqqBuddy* buddy,LwqqGroup* group)
{
    if (!lc || !(group || buddy) ) return NULL;
    const char* uin = NULL;
    uin = (buddy)?buddy->uin:group->code;
    char url[512];
    LwqqHttpRequest *req = NULL;
    snprintf(url, sizeof(url),
             WEBQQ_S_HOST"/api/get_friend_uin2?tuin=%s&verifysession=&type=1&code=&vfwebqq=%s&t=%ld",
             uin, lc->vfwebqq,time(NULL));
    req = lwqq_http_create_default_request(lc,url, NULL);
    req->set_header(req, "Referer", WEBQQ_S_REF_URL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req, 0, NULL,_C_(3p_i,process_qqnumber,req,buddy,group));
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
LwqqAsyncEvent* lwqq_info_get_friend_detail_info(LwqqClient *lc, LwqqBuddy *buddy)
{
    char url[512];
    LwqqHttpRequest *req = NULL;

    if (!lc || ! buddy) {
        return NULL;
    }

    /* Make sure we know uin. */
    if (!buddy->uin) {
        return NULL;
    }

    /* Create a GET request */
    snprintf(url, sizeof(url),
             WEBQQ_S_HOST"/api/get_friend_info2?tuin=%s&verifysession=&code=&vfwebqq=%s",
             buddy->uin, lc->vfwebqq);
    req = lwqq_http_create_default_request(lc,url, NULL);
    req->set_header(req, "Referer", WEBQQ_S_REF_URL);
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "utf-8");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req, 0, NULL,_C_(2p_i,process_friend_detail,req,buddy));
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
LwqqAsyncEvent* lwqq_info_get_online_buddies(LwqqClient *lc, LwqqErrorCode *err)
{
    char url[512];
    LwqqHttpRequest *req = NULL;

    if (!lc) {
        return NULL;
    }

    /* Create a GET request */
    snprintf(url, sizeof(url),
             WEBQQ_D_HOST"/channel/get_online_buddies2?clientid=%s&psessionid=%s",
             lc->clientid, lc->psessionid);
    req = lwqq_http_create_default_request(lc,url, err);
    if (!req) {
        goto done;
    }
    req->set_header(req, "Referer", WEBQQ_D_REF_URL);
    req->set_header(req, "Content-Transfer-Encoding", "binary");
    req->set_header(req, "Content-type", "utf-8");
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req, 0, NULL,_C_(2p_i,process_online_buddies,req,lc));
done:
    lwqq_http_request_free(req);
    return NULL;
}
LwqqAsyncEvent* lwqq_info_change_buddy_markname(LwqqClient* lc,LwqqBuddy* buddy,const char* alias)
{
    if(!lc||!buddy||!alias) return NULL;
    char url[512];
    char post[256];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/change_mark_name2");
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    snprintf(post,sizeof(post),"tuin=%s&markname=%s&vfwebqq=%s",
            buddy->uin,alias,lc->vfwebqq);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    LwqqAsyncEvent* ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(4p,do_change_markname,ev,buddy,NULL,s_strdup(alias)));
    return ev;
}

LwqqAsyncEvent* lwqq_info_change_group_markname(LwqqClient* lc,LwqqGroup* group,const char* alias)
{
    if(!lc||!group||!alias||group->type != LWQQ_GROUP_QUN) return NULL;
    char url[512];
    char post[256];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/update_group_info2");
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    snprintf(post,sizeof(post),"r={\"gcode\":%s,\"markname\":\"%s\",\"vfwebqq\":\"%s\"}",
            group->code,alias,lc->vfwebqq);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    LwqqAsyncEvent* ev;
    ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(4p,do_change_markname,ev,NULL,group,s_strdup(alias)));
    return ev;
}

LwqqAsyncEvent* lwqq_info_change_discu_topic(LwqqClient* lc,LwqqGroup* group,const char* alias)
{
    if(!lc || !group || !alias) return NULL;
    char url[256];
    char post[512];
    snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/modify_discu_info");
    snprintf(post,sizeof(post),"r={\"did\":\"%s\",\"discu_name\":\"%s\","
            "\"dtype\":1,\"clientid\":\"%s\",\"psessionid\":\"%s\",\"vfwebqq\":\"%s\"}",
            group->did,alias,lc->clientid,lc->psessionid,lc->vfwebqq);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    LwqqAsyncEvent* ev;
    ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(4p,do_change_markname,ev,NULL,group,s_strdup(alias)));
    return ev;
}


LwqqAsyncEvent* lwqq_info_modify_buddy_category(LwqqClient* lc,LwqqBuddy* buddy,int new_cate)
{
    if(!lc||!buddy||!new_cate) return NULL;
    long cate_idx = -1;
    LwqqFriendCategory *c;
    if(new_cate != 0){
        LIST_FOREACH(c,&lc->categories,entries){
            if(c->index == cate_idx) break;
        }
    }else{
        cate_idx = 0;
    }
    if(cate_idx==-1) return NULL;
    char url[512];
    char post[256];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/modify_friend_group");
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    snprintf(post,sizeof(post),"tuin=%s&newid=%ld&vfwebqq=%s",
            buddy->uin,cate_idx,lc->vfwebqq );
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    LwqqAsyncEvent* ev;
    ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(2pi,do_modify_category,ev,buddy,cate_idx));
    return ev;
}

LwqqAsyncEvent* lwqq_info_delete_friend(LwqqClient* lc,LwqqBuddy* buddy,LwqqDelFriendType del_type)
{
    if(!lc||!buddy) return NULL;
    char url[512];
    char post[256];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/delete_friend");
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    snprintf(post,sizeof(post),"tuin=%s&delType=%d&vfwebqq=%s",
            buddy->uin,del_type,lc->vfwebqq );
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    LwqqAsyncEvent* ev;
    ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    //would have blist remove buddies msg
    //do delete work.
    return ev;
}
LwqqAsyncEvent* lwqq_info_answer_request_friend(LwqqClient* lc,const char* qq,LwqqAnswer allow,const char* extra)
{
    if(!lc||!qq) return NULL;
    char url[512];
    char post[256];
    switch(allow){
        case LWQQ_NO:
            snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/deny_added_request2");
            snprintf(post,sizeof(post),"r={\"account\":%s,\"vfwebqq\":\"%s\",\"msg\":\"%s\"}",
                    qq,lc->vfwebqq,extra?extra:"");
        break;
        case LWQQ_YES:
            snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/allow_added_request2");
            snprintf(post,sizeof(post),"r={\"account\":%s,\"vfwebqq\":\"%s\"}",
                    qq,lc->vfwebqq );
        break;
        case LWQQ_ALLOW_AND_ADD:
            snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/allow_and_add2");
            snprintf(post,sizeof(post),"r={\"account\":%s,\"gid\":0,\"vfwebqq\":\"%s\"",
                    qq,lc->vfwebqq);
            if(extra)
                format_append(post,",\"mname\",\"%s\"",extra);
            strcat(post,"}");
        break;
        default:
        return NULL;
        break;
    }
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    return req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
}

void lwqq_info_get_group_sig(LwqqClient* lc,LwqqGroup* group,const char* to_uin)
{
    if(!lc||!group||!to_uin) return;
    if(group->group_sig) return;
    LwqqSimpleBuddy* sb = lwqq_group_find_group_member_by_uin(group,to_uin);
    if(sb==NULL) return;
    char url[512];
    int ret;
    json_t* root = NULL;
    snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/get_c2cmsg_sig2?"
            "id=%s&"
            "to_uin=%s&"
            "service_type=0&"
            "clientid=%s&"
            "psessionid=%s&"
            "t=%ld",
            group->gid,to_uin,lc->clientid,lc->psessionid,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    if(req==NULL){
        goto done;
    }
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    ret = req->do_request(req,0,NULL);
    if(req->http_code!=200){
        goto done;
    }
    ret = json_parse_document(&root,req->response);
    if(ret!=JSON_OK){
        goto done;
    }
    sb->group_sig = s_strdup(json_parse_simple_value(root,"value"));

done:
    if(root)
        json_free_value(&root);
    lwqq_http_request_free(req);
}

LwqqAsyncEvent* lwqq_info_change_status(LwqqClient* lc,LwqqStatus status)
{
    if(!lc||!status) return NULL;
    char url[512];
    snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/change_status2?"
            "newstatus=%s&"
            "clientid=%s&"
            "psessionid=%s&"
            "t=%ld",
            lwqq_status_to_str(status),lc->clientid,lc->psessionid,time(NULL));
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    LwqqAsyncEvent* ev = req->do_request_async(req,0,NULL,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(2pi,do_change_status,ev,lc,status));
    return ev;
}

static void add_friend_stage_2(LwqqAsyncEvent* called,LwqqVerifyCode* code,char* qq_email,LwqqBuddy* out)
{
    char* uin = NULL;
    if(!code->str){
        called->result = LWQQ_EC_ERROR;
        lwqq_async_event_finish(called);
        goto done;
    }
    char url[512];
    uin = url_encode(qq_email);
    LwqqClient* lc = called->lc;
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/search_qq_by_uin2?tuin=%s&verifysession=%s&code=%s&vfwebqq=%s&t=%ld",
            uin,lc->cookies->verifysession,code->str,lc->vfwebqq,time(NULL));
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    req->set_header(req,"Connection","keep-alive");
    LwqqAsyncEvent* ev = req->do_request_async(req,0,NULL,_C_(2p_i,process_friend_detail,req,out));
    lwqq_async_add_event_chain(ev, called);
done:
    s_free(uin);
    s_free(qq_email);
    lwqq_vc_free(code);
}

LwqqAsyncEvent* lwqq_info_search_friend(LwqqClient* lc,const char* qq_email,LwqqBuddy* out)
{
    if(!lc||!qq_email||!out) return NULL;

    /*char* qq_ = s_strdup(qq);
    s_free(out->qqnumber);
    out->qqnumber = qq_;*/
    LwqqAsyncEvent* ev = lwqq_async_event_new(NULL);
    ev->lc = lc;
    LwqqVerifyCode* c = s_malloc0(sizeof(*c));
    c->cmd = _C_(4p,add_friend_stage_2,ev,c,s_strdup(qq_email),out);
    lwqq__request_captcha(lc, c);
    return ev;
}
LwqqAsyncEvent* lwqq_info_add_friend(LwqqClient* lc,LwqqBuddy* buddy,const char* message)
{
    if(!lc||!buddy) return NULL;
    if(!buddy->token||!buddy->qqnumber) return NULL;
    if(message==NULL)message="";
    char url[512];
    char post[1024];
    if(buddy->allow&&s_atoi(buddy->allow,0)==0){
        snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/add_no_verify2");
        snprintf(post, sizeof(post), "r={\"account\":%s,\"myallow\":1,"
                "\"groupid\":%d,\"mname\":\"%s\",\"token\":\"%s\",\"vfwebqq\":\"%s\"}",buddy->qqnumber,buddy->cate_index,message,buddy->token,lc->vfwebqq);
    }else{
        snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/add_need_verify2");
        snprintf(post,sizeof(post),"r={\"account\":%s,\"myallow\":1,"
                "\"groupid\":%d,\"msg\":\"%s\","
                "\"token\":\"%s\",\"vfwebqq\":\"%s\"}",buddy->qqnumber,buddy->cate_index,message,buddy->token,lc->vfwebqq);
    }

    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    lwqq_verbose(3,"%s\n",lwqq_get_cookies(lc));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    LwqqAsyncEvent* ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    return ev;
}

LwqqAsyncEvent* lwqq_info_search_group_by_qq(LwqqClient* lc,const char* qq,LwqqGroup* group)
{
    if(!lc||!qq||!group) return NULL;

    s_free(group->qq);
    group->qq = s_strdup(qq);
    LwqqAsyncEvent* ev = lwqq_async_event_new(NULL);
    ev->lc = lc;
    LwqqVerifyCode* code = s_malloc0(sizeof(*code));
    code->cmd = _C_(3p,add_group_stage_1,ev,code,group);
    lwqq__request_captcha(lc, code);
    return ev;
}

static void add_group_stage_1(LwqqAsyncEvent* called,LwqqVerifyCode* code,LwqqGroup* g)
{
    if(!code->str) goto done;
    char url[512];
    LwqqClient* lc = called->lc;
    snprintf(url,sizeof(url),
            "http://cgi.web2.qq.com/keycgi/qqweb/group/search.do?"
            "pg=1&perpage=10&all=%s&c1=0&c2=0&c3=0&st=0&vfcode=%s&type=1&vfwebqq=%s&t=%ld",
            g->qq,code->str,lc->vfwebqq,time(NULL));
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://cgi.web2.qq.com/proxy.html?v=20110412001&id=1");
    LwqqAsyncEvent* ev = req->do_request_async(req,0,NULL,_C_(2p_i,add_group_stage_2,req,g));
    lwqq_async_add_event_chain(ev, called);
done:
    lwqq_vc_free(code);
}

static int add_group_stage_2(LwqqHttpRequest* req,LwqqGroup* g)
{
    //{"result":[{"GC":"","GD":"","GE":3090888,"GF":"","TI":"永远的电子商务033班","GA":"","BU":"","GB":"","DT":"","RQ":"","QQ":"","MD":"","TA":"","HF":"","UR":"","HD":"","HE":"","HB":"","HC":"","HA":"","LEVEL":0,"PD":"","TX":"","PA":"","PB":"","CL":"","GEX":1013409473,"PC":""}],"retcode":0,"responseHeader":{"CostTime":18,"Status":0,"TotalNum":1,"CurrentNum":1,"CurrentPage":1}}
    int err = 0;
    json_t* root = NULL,*result;
    lwqq__jump_if_http_fail(req,err);
    lwqq__jump_if_json_fail(root,req->response,err);
    result = lwqq__parse_retcode_result(root, &err);
    if(err != WEBQQ_OK) {goto done;}
    if(result && result->child){
        result = result->child;
        g->name = json_unescape(json_parse_simple_value(result, "TI"));
        g->code = s_strdup(json_parse_simple_value(result, "GEX"));
    }else{
        err = LWQQ_EC_NO_RESULT;
    }
done:
    lwqq__clean_json_and_req(root,req);
    return err;
}

LwqqAsyncEvent* lwqq_info_add_group(LwqqClient* lc,LwqqGroup* g,const char* msg)
{
    if(!lc||!g) return NULL;
    if(msg==NULL) msg = "";
    LwqqAsyncEvent* ev = lwqq_async_event_new(NULL);
    ev->lc = lc;
    LwqqVerifyCode* c = s_malloc0(sizeof(*c));
    c->cmd = _C_(4p,add_group_stage_4,ev,c,g,s_strdup(msg));
    lwqq__request_captcha(lc, c);
    return ev;
}

static void add_group_stage_4(LwqqAsyncEvent* called,LwqqVerifyCode* c,LwqqGroup* g,char* msg)
{
    if(c->str==NULL){
        called->result = LWQQ_EC_ERROR;
        lwqq_async_event_finish(called);
        goto done;
    }
    char url[256];
    char post[512];
    LwqqClient* lc = called->lc;
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/apply_join_group2");
    snprintf(post,sizeof(post),"r={\"gcode\":%s,\"code\":\"%s\",\"vfy\":\"%s\","
            "\"msg\":\"%s\",\"vfwebqq\":\"%s\"}",
            g->code,c->str,lc->cookies->verifysession,msg,lc->vfwebqq);
    lwqq_verbose(3,"%s\n",post);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    LwqqAsyncEvent* ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_chain(ev, called);
done:
    lwqq_vc_free(c);
    s_free(msg);
}


LwqqAsyncEvent* lwqq_info_mask_group(LwqqClient* lc,LwqqGroup* group,LwqqMask mask)
{
    if(!lc||!group) return NULL;
    char url[512];
    //char post[2048];
    struct ds post = ds_initializer;
    snprintf(url,sizeof(url),"http://cgi.web2.qq.com/keycgi/qqweb/uac/messagefilter.do");
    const char* mask_type;

    mask_type = (group->type == LWQQ_GROUP_QUN)? "groupmask":"discumask";

    //snprintf(post,sizeof(post),"retype=1&app=EQQ&itemlist={\"%s\":{",mask_type);
    ds_cat(post,"retype=1&app=EQQ&itemlist={\"",mask_type,"\":{",NULL);
    LwqqMask mask_ori = group->mask;
    group->mask = mask;
    if(group->type == LWQQ_GROUP_QUN){
        LwqqGroup* g;
        LIST_FOREACH(g,&lc->groups,entries){
            //format_append(post,"\"%s\":\"%d\",",g->gid,g->mask);
            ds_cat(post,"\"",g->gid,"\":\"",ds_itos(g->mask),"\",",NULL);
        }
    }else{
        LwqqGroup* d;
        LIST_FOREACH(d,&lc->discus,entries){
            //format_append(post,"\"%s\":\"%d\",",d->did,d->mask);
            ds_cat(post,"\"",d->did,"\":\"",ds_itos(d->mask),"\",",NULL);
        }
    }
    group->mask = mask_ori;
    //format_append(post,"\"cAll\":0,\"idx\":%s,\"port\":%s}}&vfwebqq=%s",
    //        lc->index,lc->port,lc->vfwebqq);
    ds_cat(post,"\"cAll\":0,\"idx\":",lc->index,",\"port\":",lc->port,"}}&vfwebqq=",lc->vfwebqq,NULL);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",ds_c_str(post));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://cgi.web2.qq.com/proxy.html?v=20110412001&id=2");
    LwqqAsyncEvent* ev;
    ev = req->do_request_async(req,1,ds_c_str(post),_C_(p_i,process_simple_response,req));
    ds_free(post);
    lwqq_async_add_event_listener(ev, _C_(2pi,do_mask_group,ev,group,mask));
    return ev;
}

LwqqAsyncEvent* lwqq_info_get_stranger_info(LwqqClient* lc,const char* serv_id,LwqqBuddy* out)
{
    if(!lc||!serv_id||!out) return NULL;
    char url[512];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_stranger_info2?tuin=%s&verifysession=&gid=0&code=&vfwebqq=%s&t=%ld",serv_id,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req,0,NULL,_C_(2p,process_friend_detail,req,out));
}
LwqqAsyncEvent* lwqq_info_get_stranger_info_by_msg(LwqqClient* lc,LwqqMsgSysGMsg* msg,LwqqBuddy* out)
{
    if(!lc||!msg||!out) return NULL;
    char url[512];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_stranger_info2?tuin=%s&verifysession=&gid=0&code=%s-%s&vfwebqq=%s&t=%ld",msg->member_uin,"group_request_join",msg->group_uin,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req,0,NULL,_C_(2p,process_friend_detail,req,out));
}

LwqqAsyncEvent* lwqq_info_answer_request_join_group(LwqqClient* lc,LwqqMsgSysGMsg* msg ,LwqqAnswer answer,const char* reason)
{
    if(!lc||!msg) return NULL;
    if(reason==NULL) reason="";
    char url[512];
    int op = (answer)?2:3;
    snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/op_group_join_req?"
            "group_uin=%s&req_uin=%s&msg=%s&op_type=%d&clientid=%s&psessionid=%s&t=%ld",
            msg->group_uin,msg->member_uin,reason,op,lc->clientid,lc->psessionid,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req,0,NULL,_C_(p_i,process_simple_response,req));
}
LwqqAsyncEvent* lwqq_info_get_group_public(LwqqClient* lc,LwqqGroup* g)
{
    if(!lc||!g) return NULL;
    if(!g->code) return NULL;
    char url[512];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_group_public_info2?gcode=%s&vfwebqq=%s&t=%ld",
            g->code,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req,0,NULL,_C_(2p_i,process_group_info,req,g));

}

void lwqq_card_free(LwqqBusinessCard* card)
{
    if(card){
        s_free(card->phone);
        s_free(card->uin);
        s_free(card->email);
        s_free(card->remark);
        s_free(card->gcode);
        s_free(card->name);
        s_free(card);
    }
}
LwqqAsyncEvent* lwqq_info_get_self_card(LwqqClient* lc,LwqqGroup* g,LwqqBusinessCard* card)
{
    if(!lc||!g||!card) return NULL;
    char url[512];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_self_business_card2?gcode=%s&vfwebqq=%s&t=%ld",
            g->code,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    lwqq_verbose(3,"%s\n",url);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL"/proxy.html?v=20110412001&id=1");
    return req->do_request_async(req,0,NULL,_C_(2p_i,process_business_card,req,card));
}

LwqqAsyncEvent* lwqq_info_set_self_card(LwqqClient* lc,LwqqBusinessCard* card)
{
    if(!lc||!card) return NULL;
    char url[512];
    char post[1024];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/update_group_business_card2");
    snprintf(post,sizeof(post),"r={\"gcode\":\"%s\",",card->gcode);
    if(card->phone) format_append(post,"\"phone\":\"%s\",",card->phone);
    if(card->email) format_append(post,"\"email\":\"%s\",",card->email);
    if(card->remark) format_append(post,"\"remark\":\"%s\",",card->remark);
    if(card->name) format_append(post,"\"name\":\"%s\",",card->name);
    if(card->gender) format_append(post,"\"name\":\"%d\",",card->gender);
    format_append(post,"\"vfwebqq\":\"%s\"}",lc->vfwebqq);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    return req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
}

LwqqAsyncEvent* lwqq_info_get_single_long_nick(LwqqClient* lc,LwqqBuddy* buddy)
{
    //{"retcode":0,"result":[{"uin":1503539798,"lnick":"有梦想的人是幸福的，哪怕是梦想没有实现，只要努力奋斗了，也无怨无悔"}]}
    if(!lc||!buddy||!buddy->uin) return NULL;
    char url[512];

    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_single_long_nick2?tuin=%s&vfwebqq=%s&t=%ld",
            buddy->uin,lc->vfwebqq,time(NULL));
    lwqq_verbose(3,"%s\n",url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    return req->do_request_async(req,0,NULL,_C_(3p_i,process_simple_string,req,"lnick",&buddy->long_nick));
}

LwqqAsyncEvent* lwqq_info_set_self_long_nick(LwqqClient* lc,const char* nick)
{
    if(!lc||!nick) return NULL;
    char url[512];
    char post[1024];

    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/set_long_nick2");
    snprintf(post,sizeof(post),"r={\"nlk\":\"%s\",\"vfwebqq\":\"%s\"}",nick,lc->vfwebqq);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    LwqqAsyncEvent* ev =  req->do_request_async(req,1,post,_C_(3p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(2p,do_modify_longnick,ev,s_strdup(nick)));
    return ev;
}

LwqqAsyncEvent* lwqq_info_delete_group(LwqqClient* lc,LwqqGroup* group)
{
    if(!lc||!group) return NULL;
    char url[512];
    char post[512];

    LwqqHttpRequest* req;
    LwqqAsyncEvent* ev;
    if(group->type == LWQQ_GROUP_QUN){
        snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/quit_group2");
        snprintf(post,sizeof(post),"r={\"gcode\":\"%s\",\"vfwebqq\":\"%s\"}",group->code,lc->vfwebqq);
        req = lwqq_http_create_default_request(lc, url, NULL);
        req->set_header(req,"Cookie",lwqq_get_cookies(lc));
        req->set_header(req,"Referer",WEBQQ_S_REF_URL);
        lwqq_verbose(3,"%s\n",post);
        ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    }else{
        snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/quit_discu?did=%s&clientid=%s&psessionid=%s&vfwebqq=%s&t=%ld",
                group->did,lc->clientid,lc->psessionid,lc->vfwebqq,time(NULL));
        req = lwqq_http_create_default_request(lc, url, NULL);
        req->set_header(req,"Cookie",lwqq_get_cookies(lc));
        req->set_header(req,"Referer",WEBQQ_D_REF_URL);
        ev = req->do_request_async(req,0,NULL,_C_(p_i,process_simple_response,req));
    }
    lwqq_verbose(3,"%s\n",url);
    lwqq_async_add_event_listener(ev, _C_(2p,do_delete_group,ev,group));
    return ev;
}

LwqqAsyncEvent* lwqq_info_get_group_memo(LwqqClient* lc,LwqqGroup* g)
{
    if(!lc||!g) return NULL;
    char url[512];

    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_group_info?"
            "retainKey=memo,gid&vfwebqq=%s&t=%ld&gcode=[%s]",
            lc->vfwebqq,time(NULL),g->code);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    return req->do_request_async(req,0,NULL,_C_(3p_i,process_simple_string,req,"memo",&g->memo));
}

LwqqAsyncEvent* lwqq_info_set_dicsu_topic(LwqqClient* lc,LwqqGroup* d,const char* topic)
{
    if(!lc || !d || !topic || d->type != LWQQ_GROUP_DISCU) return NULL;
    char url[512];
    char post[1024];
    snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/modify_discu_info");
    snprintf(post,sizeof(post),"r={\"did\":\"%s\",\"discu_name\":\"%s\",\"dtype\":1,\"clientid\":\"%s\",\"psessionid\":\"%s\",\"vfwebqq\":\"%s\"}",
            d->did,topic,lc->clientid,lc->psessionid,lc->vfwebqq);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    LwqqAsyncEvent* ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(3p,do_rename_discu,ev,d,s_strdup(topic)));
    return ev;
}

LwqqAsyncEvent* lwqq_info_recent_list(LwqqClient* lc,LwqqRecentList* list)
{
    if(!lc||!list) return NULL;
    char url[512];
    char post[512];
    snprintf(url,sizeof(url),WEBQQ_D_HOST"/channel/get_recent_list2");
    snprintf(post,sizeof(post),"r={\"vfwebqq\":\"%s\",\"clientid\":\"%s\",\"psessionid\":\"%s\"}",
            lc->vfwebqq,lc->clientid,lc->psessionid);
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    return req->do_request_async(req,1,post,_C_(2p_i,process_recent_list,req,list));
}
void lwqq_recent_list_free(LwqqRecentList* list)
{
    if(!list) return;
    LwqqRecentItem* recent;
    while((recent = LIST_FIRST(list))){
        LIST_REMOVE(recent,entries);
        s_free(recent->uin);
        s_free(recent);
    }
    memset(list,0,sizeof(*list));
}


LwqqAsyncEvent* lwqq_info_qq_get_level(LwqqClient* lc, LwqqBuddy* b)
{
    if(!lc||!b) return NULL;
    char url[512];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_qq_level2?tuin=%s&vfwebqq=%s&t=%ld",b->uin,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    lwqq_verbose(3,"%s\n",url);
    return req->do_request_async(req,0,NULL,_C_(2p,process_qq_level,req,b));
}
LwqqDiscuMemChange* lwqq_discu_mem_change_new()
{
    return s_malloc0(sizeof(LwqqDiscuMemChange));
}
void lwqq_discu_mem_change_free(LwqqDiscuMemChange* chg)
{
    if(!chg) return;
    str_list_free_all(chg->buddies);
    str_list_free_all(chg->group_members);
    str_list_free_all(chg->relate_groups);
    s_free(chg);
}
LwqqErrorCode lwqq_discu_add_buddy(LwqqDiscuMemChange* mem,LwqqBuddy* b)
{
    if(!mem||!b) return LWQQ_EC_ERROR;
    mem->buddies = str_list_prepend(mem->buddies, b->uin);
    return LWQQ_EC_OK;
}

LwqqErrorCode lwqq_discu_add_group_member(LwqqDiscuMemChange* mem,LwqqSimpleBuddy* sb,LwqqGroup* g)
{
    if(!mem||!sb||!g) return LWQQ_EC_ERROR;
    if(g->type != LWQQ_GROUP_QUN) return LWQQ_EC_ERROR;
    mem->group_members = str_list_prepend(mem->group_members, sb->uin);
    mem->relate_groups = str_list_prepend(mem->relate_groups, g->gid);
    return LWQQ_EC_OK;
}

static void lwqq_discu_mem_change_to_str(LwqqDiscuMemChange* chg,char* buf,size_t len)
{
    strcat(buf,"\"mem_list\":[");
    struct str_list_* ptr = chg->buddies;
    while(ptr){
        format_append(buf,"\"%s\"",ptr->str);
        ptr = ptr->next;
        if(ptr) strcat(buf,",");
    }
    strcat(buf,"],\"mem_list_u\":[");
    ptr = chg->group_members;
    while(ptr){
        format_append(buf,"\"%s\"",ptr->str);
        ptr = ptr->next;
        if(ptr) strcat(buf,",");
    }
    strcat(buf,"],\"mem_list_g\":[");
    ptr = chg->relate_groups;
    while(ptr){
        format_append(buf,"\"%s\"",ptr->str);
        ptr = ptr->next;
        if(ptr) strcat(buf,",");
    }
    strcat(buf,"]");
}

LwqqAsyncEvent* lwqq_info_change_discu_mem(LwqqClient* lc,LwqqGroup* discu,LwqqDiscuMemChange* chg)
{
    if(!discu||!chg) return NULL;
    if(discu->type != LWQQ_GROUP_DISCU) return NULL;
    char url[128];
    char post[1024];
    snprintf(url, sizeof(url), WEBQQ_D_HOST"/channel/change_discu_mem");
    snprintf(post, sizeof(post), "r={\"did\":\"%s\",",discu->did);

    lwqq_discu_mem_change_to_str(chg, post, sizeof(post)-strlen(post));
    
    format_append(post,",\"clientid\":\"%s\",\"psessionid\":\"%s\",\"vfwebqq\":\"%s\"}",
            lc->clientid,lc->psessionid,lc->vfwebqq);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    lwqq_verbose(3,"%s\n",url);
    lwqq_verbose(3,"%s\n",post);

    LwqqAsyncEvent* ev = req->do_request_async(req,1,post,_C_(p_i,process_simple_response,req));
    lwqq_async_add_event_listener(ev, _C_(3p,do_change_discu_mem,ev,discu,chg));
    return ev;
}

static LwqqAsyncEvent* lwqq_info_get_allow_info(LwqqClient* lc,LwqqBuddy* b)
{
//http://s.web2.qq.com/api/get_allow_info2?tuin=106017864&retainKey=allow&vfwebqq=561652d029ab3b84cc233b85a15b8f47123d0b4b69e92ba4e0efd4e73d4bc4712ecec28375961d44&t=1365477470708
    if(!lc||!b) return NULL;
    char url[512];
    snprintf(url,sizeof(url),WEBQQ_S_HOST"/api/get_allow_info2?tuin=%s&retainKey=allow&vfwebqq=%s&t=%ld",b->uin,lc->vfwebqq,time(NULL));
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_S_REF_URL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    return req->do_request_async(req,0,NULL,_C_(3p_i,process_simple_string,req,"allow",&b->allow));
}

static void add_group_member_as_friend_stage_2(LwqqAsyncEvent* event,LwqqBuddy* b,char* markname,LwqqAsyncEvent* ret)
{
    int err = 0;
    LwqqClient* lc = event->lc;
    lwqq__jump_if_ev_fail(event,err);
    LwqqAsyncEvent* ev = lwqq_info_add_friend(lc, b, markname);
    lwqq_async_add_event_chain(ev, ret);
done:
    s_free(markname);
    if(err){
        ret->result = LWQQ_EC_ERROR;
        lwqq_async_event_finish(ret);
        lwqq_buddy_free(b);
    }
}

LwqqAsyncEvent* lwqq_info_add_group_member_as_friend(LwqqClient* lc,LwqqBuddy* mem_detail,const char* markname)
{
    LwqqBuddy* buddy = mem_detail;
    if(!lc||!buddy||!buddy->token||!buddy->qqnumber) return NULL;
    if(!markname) markname="";
    LwqqAsyncEvent* ret = lwqq_async_event_new(NULL);
    ret->lc = lc;
    LwqqAsyncEvent* ev = lwqq_info_get_allow_info(lc, buddy);
    lwqq_async_add_event_listener(ev, _C_(4p,add_group_member_as_friend_stage_2,ev,buddy,s_strdup(markname),ret));
    return ret;
}


static void create_discu_stage_2(LwqqAsyncEvent* called,LwqqVerifyCode* code,LwqqDiscuMemChange* chg,char* dname)
{
    if(!code->str){
        called->failcode = LWQQ_EC_ERROR;
        lwqq_async_event_finish(called);
        goto done;
    }
    LwqqClient* lc = called->lc;
    char url[128];
    char post[1024];
    snprintf(url, sizeof(url), WEBQQ_D_HOST"/channel/create_discu");
    snprintf(post, sizeof(post), "r={\"discu_name\":\"%s\",",dname);

    lwqq_discu_mem_change_to_str(chg, post, sizeof(post)-strlen(post));
    
    format_append(post,",\"code\":\"%s\",\"verifysession\":\"%s\",\"clientid\":\"%s\",\"psessionid\":\"%s\",\"vfwebqq\":\"%s\"}",
            code->str,lc->cookies->verifysession,lc->clientid,lc->psessionid,lc->vfwebqq);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc, url, NULL);
    req->set_header(req,"Referer",WEBQQ_D_REF_URL);
    req->set_header(req,"Cookie",lwqq_get_cookies(lc));
    LwqqGroup* discu = lwqq_group_new(LWQQ_GROUP_DISCU);
    discu->name = s_strdup(dname);
    LwqqAsyncEvent* ev = req->do_request_async(req,1,post,_C_(3p_i,process_simple_string,req,"did",&discu->did));
    lwqq_async_add_event_listener(ev, _C_(2p,do_create_discu,ev,discu));
    lwqq_async_add_event_chain(ev, called);
done:
    lwqq_discu_mem_change_free(chg);
    s_free(dname);
    lwqq_vc_free(code);
}
LwqqAsyncEvent* lwqq_info_create_discu(LwqqClient* lc,LwqqDiscuMemChange* chg,const char* dname)
{
    if(!lc||!chg) return NULL;
    if(!dname) dname="";

    LwqqAsyncEvent* ev = lwqq_async_event_new(NULL);
    ev->lc = lc;
    LwqqVerifyCode* code = s_malloc0(sizeof(*code));
    code->cmd = _C_(4p,create_discu_stage_2,ev,code,chg,s_strdup(dname));
    lwqq__request_captcha(lc, code);
    return ev;
}
