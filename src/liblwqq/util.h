#ifndef LWQQ_UTIL_H_H
#define LWQQ_UTIL_H_H
#include "type.h"

LwqqAsyncEvent* lwqq_util_request_captcha(LwqqClient* lc,LwqqVerifyCode* c);
#define lwqq_util_valid_http_request(req) if(req->http_code !=200) {err=LWQQ_EC_ERROR;goto done;}
#define lwqq_util_valid_and_parse_json(json,value) if(json_parse_document(&json,value)!=JSON_OK){err = LWQQ_EC_ERROR;goto done;}
#define lwqq_util_json_get_retcode(json) s_atoi(json_parse_simple_value(json,"retcode"),1)
    
#endif
