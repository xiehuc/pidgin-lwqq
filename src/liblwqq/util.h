#ifndef LWQQ_UTIL_H_H
#define LWQQ_UTIL_H_H
#include "type.h"

LwqqAsyncEvent* lwqq_util_request_captcha(LwqqClient* lc,LwqqVerifyCode* c);
#define lwqq_util_jump_if_http_fail(req,err) if(req->http_code !=200) {err=LWQQ_EC_ERROR;goto done;}
#define lwqq_util_jump_if_json_fail(json,str,err) \
    if(json_parse_document(&json,str)!=JSON_OK){\
        lwqq_log(LOG_ERROR, "Parse json object of add friend error: %s\n", req->response);\
        err = LWQQ_EC_ERROR; goto done;  }

#define lwqq_util_return_if_ev_fail(ev) do{\
    if(ev->failcode == LWQQ_CALLBACK_FAILED) return;\
    if(ev->result != WEBQQ_OK) return;\
}while(0);

#define lwqq_util_clean_json_and_req(json,req) do{\
    if(json) json_free_value(&json);\
    lwqq_http_request_free(req);\
}while(0);
    
#endif
