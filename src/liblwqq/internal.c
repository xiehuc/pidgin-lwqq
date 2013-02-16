#include "internal.h"
#include "http.h"
#include "logger.h"
#include "async.h"
#include "smemory.h"
#include <string.h>


int lwqq__map_to_type_(const struct LwqqStrMapEntry_* maps,const char* key)
{
    while(maps->str != NULL){
        if(!strncmp(maps->str,key,strlen(maps->str))) return maps->type;
        maps++;
    }
    return maps->type;
}

const char* lwqq__map_to_str_(const struct LwqqStrMapEntry_* maps,int type)
{
    while(maps->str != NULL){
        if(maps->type == type) return maps->str;
        maps++;
    }
    return NULL;
}

static int request_captcha_back(LwqqHttpRequest* req,LwqqVerifyCode* code)
{
    int err = 0;
    if(req->http_code!=200){
        err = -1;
        goto done;
    }
    LwqqClient* lc = req->lc;
    code->data = req->response;
    code->size = req->resp_len;
    req->response = NULL;
    lc->async_opt->need_verify2(lc,code);
done:
    lwqq_http_request_free(req);
    return err;
}

LwqqAsyncEvent* lwqq__request_captcha(LwqqClient* lc,LwqqVerifyCode* c)
{
    if(!lc||!c) return NULL;
    char url[512];

    double random = drand48();
    snprintf(url,sizeof(url),"%s/getimage?"
            "aid=1003901&%.16lf",
            "http://captcha.qq.com",random);
    lwqq_puts(url);
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    req->set_header(req,"Cookie", lwqq_get_cookies(lc));
    req->set_header(req,"Referer","http://web2.qq.com/");
    req->set_header(req,"Connection","keep-alive");
    return req->do_request_async(req,0,NULL,_C_(2p_i,request_captcha_back,req,c));
}


json_t *lwqq__parse_retcode_result(json_t *json,int* retcode)
{
    //{"retcode":0,"result":......}

    /**
     * Frist, we parse retcode that indicate whether we get
     * correct response from server
     */
    char* value = json_parse_simple_value(json, "retcode");
    if(!value){
        *retcode = LWQQ_EC_ERROR;
        return NULL;
    }

    *retcode = s_atoi(value,LWQQ_EC_ERROR);

    /**
     * Second, Check whether there is a "result" key in json object
     * if success it would return result;
     * if failed it would return NULL;
     */
    json_t* result = json_find_first_label_all(json, "result");
    if(result == NULL) return NULL;
    return result->child;
}
int lwqq__get_retcode_from_str(const char* str)
{
    if(!str) return LWQQ_EC_ERROR;
    const char* beg = strstr(str,"retcode");
    if(beg == NULL) return LWQQ_EC_ERROR;
    beg+=strlen("retcode:\"");
    int ret = 0;
    char* end;
    ret = strtoul(beg, &end,10);
    if(end == beg) return LWQQ_EC_ERROR;
    return ret;
}

json_t *json_find_first_label_all (const json_t * json, const char *text_label)
{
    json_t *cur, *tmp;

    assert (json != NULL);
    assert (text_label != NULL);

    if(json -> text != NULL && strcmp(json -> text, text_label) == 0){
        return (json_t *)json;
    }

    for(cur = json -> child; cur != NULL; cur = cur -> next){
        tmp = json_find_first_label_all(cur, text_label);
        if(tmp != NULL){
            return tmp;
        }
    }

    return NULL;
}
/**
 * Parse value from json object whose value is a simle string
 * Caller dont need to free the returned string.
 * 
 * @param json Json object setup by json_parse_document()
 * @param key the key you want to search
 * 
 * @return Key whose value will be searched
 */
char *json_parse_simple_value(json_t *json, const char *key)
{
    json_t *val;

    if (!json || !key)
        return NULL;
    
    val = json_find_first_label_all(json, key);
    if (val && val->child && val->child->text) {
        return val->child->text;
    }
    
    return NULL;
}
char *json_unescape_s(char* str)
{
    if(str==NULL) return NULL;
    return json_unescape(str);
}
