#include "internal.h"
#include "http.h"
#include "logger.h"
#include "async.h"
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


