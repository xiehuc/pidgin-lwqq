#ifndef LWQQ_INTERNAL_H_H
#define LWQQ_INTERNAL_H_H
#include "type.h"
#include "json.h"

struct LwqqStrMapEntry_ {
    const char* str;
    int type;
};


int lwqq__map_to_type_(const struct LwqqStrMapEntry_* maps,const char* key);
const char* lwqq__map_to_str_(const struct LwqqStrMapEntry_* maps,int type);
int lwqq__get_retcode_from_str(const char* str);
json_t *lwqq__parse_retcode_result(json_t *json,int* retcode);

LwqqAsyncEvent* lwqq__request_captcha(LwqqClient* lc,LwqqVerifyCode* c);


#define lwqq__jump_if_http_fail(req,err) if(req->http_code !=200) {err=LWQQ_EC_ERROR;goto done;}
#define lwqq__jump_if_json_fail(json,str,err) \
    if(json_parse_document(&json,str)!=JSON_OK){\
        lwqq_log(LOG_ERROR, "Parse json object of add friend error: %s\n", str);\
        err = LWQQ_EC_ERROR; goto done;  }
#define lwqq__jump_if_retcode_fail(retcode) if(retcode != WEBQQ_OK) goto done;

#define lwqq__return_if_ev_fail(ev) do{\
    if(ev->failcode == LWQQ_CALLBACK_FAILED) return;\
    if(ev->result != WEBQQ_OK) return;\
}while(0);

#define lwqq__clean_json_and_req(json,req) do{\
    if(json) json_free_value(&json);\
    lwqq_http_request_free(req);\
}while(0);

#define lwqq__json_get_int(json,k,def) s_atoi(json_parse_simple_value(json,k),def)
#define lwqq__json_get_long(json,k,def) s_atol(json_parse_simple_value(json,k),def)
#define lwqq__json_get_value(json,k) s_strdup(json_parse_simple_value(json,k))
#define lwqq__json_get_string(json,k) json_unescape_s(json_parse_simple_value(json,k))
#define lwqq__json_parse_child(json,k,sub) sub=json_find_first_label(json,k);if(sub) sub=sub->child;

//json function expand
json_t *json_find_first_label_all (const json_t * json, const char *text_label);
char *json_parse_simple_value(json_t *json, const char *key);
char *json_unescape_s(char* str);

#endif

