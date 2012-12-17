/**
 * @file   login.c
 * @author mathslinux <riegamaths@gmail.com>
 * @date   Sun May 20 02:21:43 2012
 * 
 * @brief  Linux WebQQ Login API
 *  Login logic is based on the gtkqq implementation
 *  written by HuangCongyu <huangcongyu2006@gmail.com>
 * 
 * 
 */

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <alloca.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "login.h"
#include "logger.h"
#include "http.h"
#include "smemory.h"
#include "md5.h"
#include "url.h"
#include "json.h"
#include "async.h"

/* URL for webqq login */
#define LWQQ_URL_LOGIN_HOST "http://ptlogin2.qq.com"
#define LWQQ_URL_CHECK_HOST "http://check.ptlogin2.qq.com"
#define LWQQ_URL_VERIFY_IMG "http://captcha.qq.com/getimage?aid=%s&uin=%s"
#define VCCHECKPATH "/check"
#define APPID "1003903"
#define LWQQ_URL_SET_STATUS "http://d.web2.qq.com/channel/login2"

/* URL for get webqq version */
#define LWQQ_URL_VERSION "http://ui.ptlogin2.qq.com/cgi-bin/ver"

static LwqqAsyncEvent* set_online_status(LwqqClient *lc,const char *status);
static int get_version_back(LwqqHttpRequest* req);
static int get_verify_code_back(LwqqHttpRequest* req);
static int do_login_back(LwqqHttpRequest* req);
static int set_online_status_back(LwqqHttpRequest* req);
static int get_verify_image_back(LwqqHttpRequest* req);
static void login_stage_2(LwqqAsyncEvent* ev);
static void login_stage_3(LwqqAsyncEvent* ev);
static void login_stage_4(LwqqClient* lc);
static void login_stage_5(LwqqAsyncEvent* ev);
static void login_stage_f(LwqqAsyncEvent* ev);

/** 
 * Update the cookies needed by webqq
 *
 * @param req  
 * @param key 
 * @param value 
 * @param update_cache Weather update lwcookies member
 */
static void update_cookies(LwqqCookies *cookies, LwqqHttpRequest *req,
                           const char *key, int update_cache)
{
    if (!cookies || !req || !key) {
        lwqq_log(LOG_ERROR, "Null pointer access\n");
        return ;
    }

    char *value = req->get_cookie(req, key);
    if (!value)
        return ;
    
#define FREE_AND_STRDUP(a, b)                   \
    if (a)                                      \
        s_free(a);                              \
    a = s_strdup(b);
    
    if (!strcmp(key, "ptvfsession")) {
        FREE_AND_STRDUP(cookies->ptvfsession, value);
    } else if (!strcmp(key, "ptcz")) {
        FREE_AND_STRDUP(cookies->ptcz, value);
    } else if (!strcmp(key, "skey")) {
        FREE_AND_STRDUP(cookies->skey, value);
    } else if (!strcmp(key, "ptwebqq")) {
        FREE_AND_STRDUP(cookies->ptwebqq, value);
    } else if (!strcmp(key, "ptuserinfo")) {
        FREE_AND_STRDUP(cookies->ptuserinfo, value);
    } else if (!strcmp(key, "uin")) {
        FREE_AND_STRDUP(cookies->uin, value);
    } else if (!strcmp(key, "ptisp")) {
        FREE_AND_STRDUP(cookies->ptisp, value);
    } else if (!strcmp(key, "pt2gguin")) {
        FREE_AND_STRDUP(cookies->pt2gguin, value);
    } else if (!strcmp(key, "verifysession")) {
        FREE_AND_STRDUP(cookies->verifysession, value);
    } else {
        lwqq_log(LOG_WARNING, "No this cookie: %s\n", key);
    }
    s_free(value);

    if (update_cache) {
        char buf[4096] = {0};           /* 4K is enough for cookies. */
        int buflen = 0;
        if (cookies->ptvfsession) {
            snprintf(buf, sizeof(buf), "ptvfsession=%s; ", cookies->ptvfsession);
            buflen = strlen(buf);
        }
        if (cookies->ptcz) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "ptcz=%s; ", cookies->ptcz);
            buflen = strlen(buf);
        }
        if (cookies->skey) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "skey=%s; ", cookies->skey);
            buflen = strlen(buf);
        }
        if (cookies->ptwebqq) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "ptwebqq=%s; ", cookies->ptwebqq);
            buflen = strlen(buf);
        }
        if (cookies->ptuserinfo) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "ptuserinfo=%s; ", cookies->ptuserinfo);
            buflen = strlen(buf);
        }
        if (cookies->uin) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "uin=%s; ", cookies->uin);
            buflen = strlen(buf);
        }
        if (cookies->ptisp) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "ptisp=%s; ", cookies->ptisp);
            buflen = strlen(buf);
        }
        if (cookies->pt2gguin) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "pt2gguin=%s; ", cookies->pt2gguin);
            buflen = strlen(buf);
        }
        if (cookies->verifysession) {
            snprintf(buf + buflen, sizeof(buf) - buflen, "verifysession=%s; ", cookies->verifysession);
            buflen = strlen(buf);
        }
        
        FREE_AND_STRDUP(cookies->lwcookies, buf);
    }
#undef FREE_AND_STRDUP
}

// ptui_checkVC('0','!IJG, ptui_checkVC('0','!IJG', '\x00\x00\x00\x00\x54\xb3\x3c\x53');
static char *parse_verify_uin(const char *str)
{
    char *start;
    char *end;
    char uin[128] = {0};

    start = strchr(str, '\\');
    if (!start)
        return NULL;

    end = strchr(start, '\'');
    if (!end)
        return NULL;

    strncpy(uin, start, end - start);

    return s_strdup(uin);
}

static LwqqAsyncEvent* get_verify_code(LwqqClient *lc)
{
    LwqqHttpRequest *req;
    char url[512];
    char chkuin[64];

    snprintf(url, sizeof(url), "%s%s?uin=%s&appid=%s", LWQQ_URL_CHECK_HOST,
             VCCHECKPATH, lc->username, APPID);
    req = lwqq_http_create_default_request(lc,url,NULL);
    
    snprintf(chkuin, sizeof(chkuin), "chkuin=%s", lc->username);
    req->set_header(req, "Cookie", chkuin);
    return req->do_request_async(req, 0, NULL,_C_(p_i,get_verify_code_back,req));
}

static int get_verify_code_back(LwqqHttpRequest* req)
{
    int err;
    char response[256];
    LwqqClient* lc = req->lc;
    if (req->http_code != 200) {
        err = LWQQ_EC_HTTP_ERROR;
        goto failed;
    }

    /**
     * 
	 * The http message body has two format:
	 *
	 * ptui_checkVC('1','9ed32e3f644d968809e8cbeaaf2cce42de62dfee12c14b74');
	 * ptui_checkVC('0','!LOB');
	 * The former means we need verify code image and the second
	 * parameter is vc_type.
	 * The later means we don't need the verify code image. The second
	 * parameter is the verify code. The vc_type is in the header
	 * "Set-Cookie".
	 */
    snprintf(response, sizeof(response), "%s", req->response);
    lwqq_log(LOG_NOTICE, "Get response verify code: %s\n", response);

    char *c = strstr(response, "ptui_checkVC");
    char *s;
    if (!c) {
        err = LWQQ_EC_HTTP_ERROR;
        goto failed;
    }
    c = strchr(response, '\'');
    if (!c) {
        err = LWQQ_EC_HTTP_ERROR;
        goto failed;
    }
    c++;
    lc->vc = s_malloc0(sizeof(*lc->vc));
    if (*c == '0') {
        /* We got the verify code. */
        
        /* Parse uin first */
        lc->vc->uin = parse_verify_uin(response);
        if (!lc->vc->uin)
            goto failed;
        
        s = c;
        c = strstr(s, "'");
        s = c + 1;
        c = strstr(s, "'");
        s = c + 1;
        c = strstr(s, "'");
        *c = '\0';

        lc->vc->type = s_strdup("0");
        lc->vc->str = s_strdup(s);

        /* We need get the ptvfsession from the header "Set-Cookie" */
        update_cookies(lc->cookies, req, "ptvfsession", 1);
        lwqq_log(LOG_NOTICE, "Verify code: %s\n", lc->vc->str);
    } else if (*c == '1') {
        /* We need get the verify image. */

        /* Parse uin first */
        lc->vc->uin = parse_verify_uin(response);
        s = c;
        c = strstr(s, "'");
        s = c + 1;
        c = strstr(s, "'");
        s = c + 1;
        c = strstr(s, "'");
        *c = '\0';
        lc->vc->type = s_strdup("1");
        // ptui_checkVC('1','7ea19f6d3d2794eb4184c9ae860babf3b9c61441520c6df0', '\x00\x00\x00\x00\x04\x7e\x73\xb2');
        lc->vc->str = s_strdup(s);
        err = LWQQ_EC_LOGIN_NEED_VC;
        lwqq_log(LOG_NOTICE, "We need verify code image: %s\n", lc->vc->str);
    }
    
failed:
    lwqq_http_request_free(req);
    return err;
}

static LwqqAsyncEvent* get_verify_image(LwqqClient *lc)
{
    LwqqHttpRequest *req = NULL;  
    char url[512];
    char chkuin[64];
    LwqqErrorCode err;
 
    snprintf(url, sizeof(url), LWQQ_URL_VERIFY_IMG, APPID, lc->username);
    req = lwqq_http_create_default_request(lc,url, &err);
     
    snprintf(chkuin, sizeof(chkuin), "chkuin=%s", lc->username);
    req->set_header(req, "Cookie", chkuin);
    return req->do_request_async(req, 0, NULL,_C_(p_i,get_verify_image_back,req));
}
static int get_verify_image_back(LwqqHttpRequest* req)
{
    int ret;
    int err;
    char image_file[256];
    int image_length = 0;
    LwqqClient* lc = req->lc;
    if (req->http_code != 200) {
        err = LWQQ_EC_ERROR;
        goto failed;
    }
 
    const char *content_length = req->get_header(req, "Content-Length");
    if (content_length) {
        image_length = atoi(content_length);
    }
    update_cookies(lc->cookies, req, "verifysession", 1);
    snprintf(image_file, sizeof(image_file), "/tmp/%s.jpeg", lc->username);
    /* Delete old file first */
    unlink(image_file);
    int fd = creat(image_file, S_IRUSR | S_IWUSR);
    if (fd != -1) {
        ret = write(fd, req->response, image_length);
        if (ret <= 0) {
            lwqq_log(LOG_ERROR, "Saving erify image file error\n");
        }
        close(fd);
    }

    lc->vc->data = req->response;
    lc->vc->size = req->resp_len;
    req->response = NULL;
 
failed:
    lwqq_http_request_free(req);
    return err;
}
 

static void upcase_string(char *str, int len)
{
    int i;
    for (i = 0; i < len; ++i) {
        if (islower(str[i]))
            str[i]= toupper(str[i]);
    }
}

/**
 * I hacked the javascript file named comm.js, which received from tencent
 * server, and find that fuck tencent has changed encryption algorithm
 * for password in webqq3 . The new algorithm is below(descripted with javascript):
 * var M=C.p.value; // M is the qq password 
 * var I=hexchar2bin(md5(M)); // Make a md5 digest
 * var H=md5(I+pt.uin); // Make md5 with I and uin(see below)
 * var G=md5(H+C.verifycode.value.toUpperCase());
 * 
 * @param pwd User's password
 * @param vc Verify Code. e.g. "!M6C"
 * @param uin A string like "\x00\x00\x00\x00\x54\xb3\x3c\x53", NB: it
 *        must contain 8 hexadecimal number, in this example, it equaled
 *        to "0x0,0x0,0x0,0x0,0x54,0xb3,0x3c,0x53"
 * 
 * @return Encoded password on success, else NULL on failed
 */
static char *lwqq_enc_pwd(const char *pwd, const char *vc, const char *uin)
{
    int i;
    int uin_byte_length;
    char buf[128] = {0};
    char _uin[9] = {0};

    if (!pwd || !vc || !uin) {
        lwqq_log(LOG_ERROR, "Null parameterment\n");
        return NULL;
    }
    

    /* Calculate the length of uin (it must be 8?) */
    uin_byte_length = strlen(uin) / 4;

    /**
     * Ok, parse uin from string format.
     * "\x00\x00\x00\x00\x54\xb3\x3c\x53" -> {0,0,0,0,54,b3,3c,53}
     */
    for (i = 0; i < uin_byte_length ; i++) {
        char u[5] = {0};
        char tmp;
        strncpy(u, uin + i * 4 + 2, 2);

        errno = 0;
        tmp = strtol(u, NULL, 16);
        if (errno) {
            return NULL;
        }
        _uin[i] = tmp;
    }

    /* Equal to "var I=hexchar2bin(md5(M));" */
    lutil_md5_digest((unsigned char *)pwd, strlen(pwd), (char *)buf);

    /* Equal to "var H=md5(I+pt.uin);" */
    memcpy(buf + 16, _uin, uin_byte_length);
    lutil_md5_data((unsigned char *)buf, 16 + uin_byte_length, (char *)buf);
    
    /* Equal to var G=md5(H+C.verifycode.value.toUpperCase()); */
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", vc);
    upcase_string(buf, strlen(buf));

    lutil_md5_data((unsigned char *)buf, strlen(buf), (char *)buf);
    upcase_string(buf, strlen(buf));

    /* OK, seems like every is OK */
    lwqq_puts(buf);
    return s_strdup(buf);
}

static int sava_cookie(LwqqClient *lc, LwqqHttpRequest *req, LwqqErrorCode *err)
{
    update_cookies(lc->cookies, req, "ptcz", 0);
    update_cookies(lc->cookies, req, "skey",  0);
    update_cookies(lc->cookies, req, "ptwebqq", 0);
    update_cookies(lc->cookies, req, "ptuserinfo", 0);
    update_cookies(lc->cookies, req, "uin", 0);
    update_cookies(lc->cookies, req, "ptisp", 0);
    update_cookies(lc->cookies, req, "pt2gguin", 1);
    
    return 0;
}

/** 
 * Do really login
 * 
 * @param lc 
 * @param md5 The md5 calculated from calculate_password_md5() 
 * @param err 
 */
static LwqqAsyncEvent* do_login(LwqqClient *lc, const char *md5, LwqqErrorCode *err)
{
    char url[1024];
    LwqqHttpRequest *req;
    
    snprintf(url, sizeof(url), "%s/login?u=%s&p=%s&verifycode=%s&"
             "webqq_type=%d&remember_uin=1&aid=1003903&login2qq=1&"
             "u1=http%%3A%%2F%%2Fweb.qq.com%%2Floginproxy.html"
             "%%3Flogin2qq%%3D1%%26webqq_type%%3D10&h=1&ptredirect=0&"
             "ptlang=2052&from_ui=1&pttype=1&dumy=&fp=loginerroralert&"
             "action=2-11-7438&mibao_css=m_webqq&t=1&g=1", LWQQ_URL_LOGIN_HOST, lc->username, md5, lc->vc->str,lc->stat);

    req = lwqq_http_create_default_request(lc,url, err);
    /* Setup http header */
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));

    /* Send request */
    return req->do_request_async(req, 0, NULL,_C_(p_i,do_login_back,req));
}

static int do_login_back(LwqqHttpRequest* req)
{
    LwqqClient* lc = req->lc;
    int err;
    const char* response;
    if (req->http_code != 200) {
        err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }
    if (strstr(req->response,"aq.qq.com")!=NULL){
        err = LWQQ_EC_LOGIN_ABNORMAL;
        const char* beg = strstr(req->response,"http://aq.qq.com");
        sscanf(beg,"%[^']",lc->error_description);
        goto done;
    }

    response = req->response;
    char *p = strstr(response, "\'");
    if (!p) {
        err = LWQQ_EC_ERROR;
        goto done;
    }
    char buf[4] = {0};
    int status;
    strncpy(buf, p + 1, 1);
    status = atoi(buf);

    switch (status) {
    case 0:
        sava_cookie(lc, req, NULL);
        err = LWQQ_EC_OK;
        break;
        
    case 1:
        lwqq_log(LOG_WARNING, "Server busy! Please try again\n");
        lc->last_err = "Server busy! Please try again";
        err = LWQQ_EC_ERROR;
        goto done;

    case 2:
        lwqq_log(LOG_ERROR, "Out of date QQ number\n");
        lc->last_err = "Out of date QQ number";
        err = LWQQ_EC_ERROR;
        goto done;

    case 3:
        lwqq_log(LOG_ERROR, "Wrong password\n");
        err = LWQQ_EC_ERROR;
        lc->last_err = "Wrong username or password";
        goto done;

    case 4:
        lwqq_log(LOG_ERROR, "Wrong verify code\n");
        err = LWQQ_EC_ERROR;
        lc->last_err = "Wrong verify code";
        goto done;

    case 5:
        lwqq_log(LOG_ERROR, "Verify failed\n");
        lc->last_err = "Verify failed";
        err = LWQQ_EC_ERROR;
        goto done;

    case 6:
        lwqq_log(LOG_WARNING, "You may need to try login again\n");
        lc->last_err = "You may need to try login again";
        err = LWQQ_EC_ERROR;
        goto done;

    case 7:
        lwqq_log(LOG_ERROR, "Wrong input\n");
        lc->last_err = "Wrong input";
        err = LWQQ_EC_ERROR;
        goto done;

    case 8:
        lwqq_log(LOG_ERROR, "Too many logins on this IP. Please try again\n");
        lc->last_err = "Too many logins on this IP.Please try again";
        err = LWQQ_EC_ERROR;
        goto done;

    default:
        err = LWQQ_EC_ERROR;
        lc->last_err = "Unknow error";
        lwqq_log(LOG_ERROR, "Unknow error");
        goto done;
    }

done:
    lwqq_http_request_free(req);
    return err;
}

/**
 * Get WebQQ version from tencent server
 * The response is like "ptuiV(201205211530)", what we do is get "201205211530"
 * from the response and set it to lc->version.
 * 
 * @param lc 
 * @param err *err will be set LWQQ_EC_OK if everything is ok, else
 *        *err will be set LWQQ_EC_ERROR.
 */

static LwqqAsyncEvent* get_version(LwqqClient *lc, LwqqErrorCode *err)
{
    LwqqHttpRequest *req;

    req = lwqq_http_create_default_request(lc,LWQQ_URL_VERSION, err);

    /* Send request */
    lwqq_log(LOG_DEBUG, "Get webqq version from %s\n", LWQQ_URL_VERSION);
    return  req->do_request_async(req, 0, NULL,_C_(p_i,get_version_back,req));
}
static int get_version_back(LwqqHttpRequest* req)
{
    int errno;
    char* response = NULL;
    LwqqClient* lc = req->lc;
    if (req->http_code!=200) {
        errno = LWQQ_EC_NETWORK_ERROR;
        goto done;
    }
    response = req->response;
    if(response == NULL){
        errno = LWQQ_EC_NETWORK_ERROR;
        goto done;
    }
    if (strstr(response, "ptuiV")) {
        char *s, *t;
        char *v;
        s = strchr(response, '(');
        t = strchr(response, ')');
        if (!s || !t) {
            errno = LWQQ_EC_ERROR;
            goto done;
        }
        s++;
        v = alloca(t - s + 1);
        memset(v, 0, t - s + 1);
        strncpy(v, s, t - s);
        s_free(lc->version);
        lc->version = s_strdup(v);
        errno = LWQQ_EC_OK;
    }

done:
    lwqq_http_request_free(req);
    return errno;
}

static char *generate_clientid()
{
    int r;
    struct timeval tv;
    long t;
    char buf[20] = {0};
    
    srand(time(NULL));
    r = rand() % 90 + 10;
    if (gettimeofday(&tv, NULL)) {
        return NULL;
    }
    t = tv.tv_usec % 1000000;
    snprintf(buf, sizeof(buf), "%d%ld", r, t);
    return s_strdup(buf);
}

/** 
 * Set online status, this is the last step of login
 * 
 * @param err
 * @param lc 
 */
static LwqqAsyncEvent* set_online_status(LwqqClient *lc,const char *status)
{
    char msg[1024] ={0};
    char *buf;
    LwqqHttpRequest *req = NULL;  

    if (!status) {
        return NULL;
    }

    lc->clientid = generate_clientid();
    if (!lc->clientid) {
        lwqq_log(LOG_ERROR, "Generate clientid error\n");
        return NULL;
    }

    snprintf(msg, sizeof(msg), "{\"status\":\"%s\",\"ptwebqq\":\"%s\","
             "\"passwd_sig\":""\"\",\"clientid\":\"%s\""
             ", \"psessionid\":null}"
             ,status, lc->cookies->ptwebqq
             ,lc->clientid);
    buf = url_encode(msg);
    snprintf(msg, sizeof(msg), "r=%s", buf);
    s_free(buf);

    /* Create a POST request */
    req = lwqq_http_create_default_request(lc,LWQQ_URL_SET_STATUS, NULL);

    /* Set header needed by server */
    req->set_header(req, "Cookie2", "$Version=1");
    req->set_header(req, "Referer", "http://d.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    
    /* Set http cookie */
    req->set_header(req, "Cookie", lwqq_get_cookies(lc));
    
    return  req->do_request_async(req, 1, msg,_C_(p_i,set_online_status_back,req));
}

static int set_online_status_back(LwqqHttpRequest* req)
{
    int err;
    int ret;
    char* response;
    char* value;
    json_t * json = NULL;
    LwqqClient* lc = req->lc;
    if (req->http_code != 200) {
        err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }

    /**
     * Here, we got a json object like this:
     * {"retcode":0,"result":{"uin":1421032531,"cip":2013211875,"index":1060,"port":43415,"status":"online","vfwebqq":"e7ce7913336ad0d28de9cdb9b46a57e4a6127161e35b87d09486001870226ec1fca4c2ba31c025c7","psessionid":"8368046764001e636f6e6e7365727665725f77656271714031302e3133332e34312e32303200006b2900001544016e0400533cb3546d0000000a4046674d4652585136496d00000028e7ce7913336ad0d28de9cdb9b46a57e4a6127161e35b87d09486001870226ec1fca4c2ba31c025c7","user_state":0,"f":0}}
     * 
     */
    response = req->response;
    ret = json_parse_document(&json, response);
    if (ret != JSON_OK) {
        err = LWQQ_EC_ERROR;
        goto done;
    }

    if (!(value = json_parse_simple_value(json, "retcode"))) {
        err = LWQQ_EC_ERROR;
        goto done;
    }
    /**
     * Do we need parse "seskey? from kernelhcy's code, we need it,
     * but from the response we got like above, we dont need
     * 
     */
    if ((value = json_parse_simple_value(json, "seskey"))) {
        lc->seskey = s_strdup(value);
    }

    if ((value = json_parse_simple_value(json, "cip"))) {
        lc->cip = s_strdup(value);
    }

    if ((value = json_parse_simple_value(json, "index"))) {
        lc->index = s_strdup(value);
    }

    if ((value = json_parse_simple_value(json, "port"))) {
        lc->port = s_strdup(value);
    }

    if ((value = json_parse_simple_value(json, "status"))) {
        lc->status = lwqq_status_to_str(lc->stat);
    }

    if ((value = json_parse_simple_value(json, "vfwebqq"))) {
        lc->vfwebqq = s_strdup(value);
    }

    if ((value = json_parse_simple_value(json, "psessionid"))) {
        lc->psessionid = s_strdup(value);
    }

    err = LWQQ_EC_OK;
    
done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
    return err;
}

/** 
 * WebQQ login function
 * Step:
 * 1. Get webqq version
 * 2. Get verify code
 * 3. Calculate password's md5
 * 4. Do real login 
 * 5. check whether logining successfully
 * 
 * @param client Lwqq Client 
 * @param err Error code
 */
void lwqq_login(LwqqClient *client, LWQQ_STATUS status,LwqqErrorCode *err)
{
    if (!client || !status) {
        lwqq_log(LOG_ERROR, "Invalid pointer\n");
        return ;
    }

    client->stat = status;

    /* First: get webqq version */
    LwqqAsyncEvent* ev = get_version(client, err);
    lwqq_async_add_event_listener(ev,_C_(p,login_stage_2,ev));
}

static void login_stage_2(LwqqAsyncEvent* ev)
{
    if(lwqq_async_event_get_code(ev) == LWQQ_CALLBACK_FAILED) return;
    LwqqClient* lc = lwqq_async_event_get_owner(ev);
    int err = lwqq_async_event_get_result(ev);
    if (err) {
        lwqq_log(LOG_ERROR, "Get webqq version error\n");
        lc->async_opt->login_complete(lc,err);
        return ;
    }
    lwqq_log(LOG_NOTICE, "Get webqq version: %s\n", lc->version);


    /**
     * Second, we get the verify code from server.
     * If server provide us a image and let us enter code shown
     * in image number, in this situation, we just return LWQQ_EC_LOGIN_NEED_VC
     * simply, so user should call lwqq_login() again after he set correct
     * code to vc->str;
     * Else, if we can get the code directly, do login immediately.
     * 
     */
    if (!lc->vc) {
        LwqqAsyncEvent* ev = get_verify_code(lc);
        lwqq_async_add_event_listener(ev,_C_(p,login_stage_3,ev));
        return;
    }

    login_stage_4(lc);
}

static void login_stage_3(LwqqAsyncEvent* ev)
{
    if(lwqq_async_event_get_code(ev) == LWQQ_CALLBACK_FAILED) return;
    int err = lwqq_async_event_get_result(ev);
    LwqqClient* lc = lwqq_async_event_get_owner(ev);
    switch (err) {
        case LWQQ_EC_LOGIN_NEED_VC:
            lwqq_log(LOG_WARNING, "Need to enter verify code\n");
            LwqqAsyncEvent* ev = get_verify_image(lc);
            lwqq_async_add_event_listener(ev,_C_(p,lc->async_opt->login_verify,lc));
            return ;

        case LWQQ_EC_NETWORK_ERROR:
            lwqq_log(LOG_ERROR, "Network error\n");
            lc->async_opt->login_complete(lc,err);
            return ;

        case LWQQ_EC_OK:
            lwqq_log(LOG_DEBUG, "Get verify code OK\n");
            break;

        default:
            lwqq_log(LOG_ERROR, "Unknown error\n");
            lc->async_opt->login_complete(lc,err);
            return ;
    }

    login_stage_4(lc);
}

static void login_stage_4(LwqqClient* lc)
{
    /* Third: calculate the md5 */
    char *md5 = lwqq_enc_pwd(lc->password, lc->vc->str, lc->vc->uin);

    /* Last: do real login */
    LwqqAsyncEvent* ev = do_login(lc, md5, NULL);
    s_free(md5);
    lwqq_async_add_event_listener(ev,_C_(p,login_stage_5,ev));

}
static void login_stage_5(LwqqAsyncEvent* ev)
{
    if(lwqq_async_event_get_code(ev) == LWQQ_CALLBACK_FAILED) return;
    int err = lwqq_async_event_get_result(ev);
    LwqqClient* lc = lwqq_async_event_get_owner(ev);
    /* Free old value */

    if(err != LWQQ_EC_OK){
        lc->async_opt->login_complete(lc,err);
    }
    LwqqAsyncEvent* event = set_online_status(lc, lwqq_status_to_str(lc->stat));
    lwqq_async_add_event_listener(event,_C_(p,login_stage_f,event));
}
static void login_stage_f(LwqqAsyncEvent* ev)
{
    if(lwqq_async_event_get_code(ev) == LWQQ_CALLBACK_FAILED) return;
    int err = lwqq_async_event_get_result(ev);
    LwqqClient* lc = lwqq_async_event_get_owner(ev);
    lc->async_opt->login_complete(lc,err);
    lwqq_vc_free(lc->vc);
    lc->vc = NULL;
}

/** 
 * WebQQ logout function
 * 
 * @param client Lwqq Client 
 * @param err Error code
 */
void lwqq_logout(LwqqClient *client, LwqqErrorCode *err)
{
    char url[512];
    LwqqHttpRequest *req = NULL;  
    int ret;
    json_t *json = NULL;
    char *value;
    struct timeval tv;
    long int re;

    if (!client) {
        lwqq_log(LOG_ERROR, "Invalid pointer\n");
        return ;
    }

    /* Get the milliseconds of now */
    if (gettimeofday(&tv, NULL)) {
        if (err)
            *err = LWQQ_EC_ERROR;
        return ;
    }
    re = tv.tv_usec / 1000;
    re += tv.tv_sec;
    
    snprintf(url, sizeof(url), "%s/channel/logout2?clientid=%s&psessionid=%s&t=%ld",
             "http://d.web2.qq.com", client->clientid, client->psessionid, re);

    /* Create a GET request */
    req = lwqq_http_create_default_request(client,url, err);
    if (!req) {
        goto done;
    }

    /* Set header needed by server */
    req->set_header(req, "Referer", "http://ptlogin2.qq.com/proxy.html?v=20101025002");
    
    /* Set http cookie */
    req->set_header(req, "Cookie", lwqq_get_cookies(client));
    
    //lwqq_http_set_timeout(req,5);
    lwqq_http_set_option(req, LWQQ_HTTP_TIMEOUT,5L);
    ret = req->do_request(req, 0, NULL);
    if (ret) {
        lwqq_log(LOG_ERROR, "Send logout request failed\n");
        if (err)
            *err = LWQQ_EC_NETWORK_ERROR;
        goto done;
    }
    if (req->http_code != 200) {
        if (err)
            *err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }

    ret = json_parse_document(&json, req->response);
    if (ret != JSON_OK) {
        if (err)
            *err = LWQQ_EC_ERROR;
        goto done;
    }

    /* Check whether logout correctly */
    value = json_parse_simple_value(json, "retcode");
    if (!value || strcmp(value, "0")) {
        if (err)
            *err = LWQQ_EC_ERROR;
        goto done;
    }
    value = json_parse_simple_value(json, "result");
    if (!value || strcmp(value, "ok")) {
        if (err)
            *err = LWQQ_EC_ERROR;
        goto done;
    }

    /* Ok, seems like all thing is ok */
    if (err)
        *err = LWQQ_EC_OK;
    
done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);    
}

void lwqq_client_cleanup(LwqqClient* lc)
{
    lwqq_http_cleanup(lc);
}

/*void lwqq_relogin(LwqqClient* lc)
{
    char url[128];
    char post[1024];
    int ret;
    snprintf(url,sizeof(url),"%s/channel/login2","http://d.web2.qq.com");
    LwqqHttpRequest* req = lwqq_http_create_default_request(lc,url,NULL);
    if(req==NULL){
        goto done;
    }
    snprintf(post,sizeof(post),"r={"
            "\"status\":\"online\","
            "\"ptwebqq\":\"%s\","
            "\"passwd_sig\":\"\","
            "\"clientid\":\"%s\","
            "\"psessionid\":\"%s\"}"
            "&clientid=%s"
            "&psessionid=%s",
            lc->ptwebqq,lc->clientid,lc->psessionid,
            lc->clientid,lc->psessionid);
    req->set_header(req,"Origin","http://d.web2.qq.com");
    req->set_header(req,"Referer","http://d.web2.qq.com/proxy.html?v=20110331002callback=0&id=2");
    ret = req->do_request(req,1,post);
    if(ret || req->http_code!=200){
        goto done;
    }

}*/
