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

/* URL for webqq login */
#define LWQQ_URL_LOGIN_HOST "http://ptlogin2.qq.com"
#define LWQQ_URL_CHECK_HOST "http://check.ptlogin2.qq.com"
#define LWQQ_URL_VERIFY_IMG "http://captcha.qq.com/getimage?aid=%s&uin=%s"
#define VCCHECKPATH "/check"
#define APPID "1003903"
#define LWQQ_URL_SET_STATUS "http://d.web2.qq.com/channel/login2"

/* URL for get webqq version */
#define LWQQ_URL_VERSION "http://ui.ptlogin2.qq.com/cgi-bin/ver"

static void set_online_status(LwqqClient *lc,const char *status, LwqqErrorCode *err);

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

static void get_verify_code(LwqqClient *lc, LwqqErrorCode *err)
{
    LwqqHttpRequest *req;
    char url[512];
    char response[256];
    int ret;
    char chkuin[64];

    snprintf(url, sizeof(url), "%s%s?uin=%s&appid=%s", LWQQ_URL_CHECK_HOST,
             VCCHECKPATH, lc->username, APPID);
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto failed;
    }
    
    snprintf(chkuin, sizeof(chkuin), "chkuin=%s", lc->username);
    req->set_header(req, "Cookie", chkuin);
    ret = req->do_request(req, 0, NULL);
    if (ret) {
        *err = LWQQ_EC_NETWORK_ERROR;
        goto failed;
    }
    if (req->http_code != 200) {
        *err = LWQQ_EC_HTTP_ERROR;
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
        *err = LWQQ_EC_HTTP_ERROR;
        goto failed;
    }
    c = strchr(response, '\'');
    if (!c) {
        *err = LWQQ_EC_HTTP_ERROR;
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
        *err = LWQQ_EC_LOGIN_NEED_VC;
        lwqq_log(LOG_NOTICE, "We need verify code image: %s\n", lc->vc->str);
    }
    
    lwqq_http_request_free(req);
    return ;
    
failed:
    lwqq_http_request_free(req);
}

static void get_verify_image(LwqqClient *lc)
{
    LwqqHttpRequest *req = NULL;  
    char url[512];
    int ret;
    char chkuin[64];
    char image_file[256];
    int image_length = 0;
    LwqqErrorCode err;
 
    snprintf(url, sizeof(url), LWQQ_URL_VERIFY_IMG, APPID, lc->username);
    req = lwqq_http_create_default_request(url, &err);
    if (!req) {
        goto failed;
    }
     
    snprintf(chkuin, sizeof(chkuin), "chkuin=%s", lc->username);
    req->set_header(req, "Cookie", chkuin);
    ret = req->do_request(req, 0, NULL);
    if (ret) {
        goto failed;
    }
    if (req->http_code != 200) {
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
    puts(buf);
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
static void do_login(LwqqClient *lc, const char *md5, LwqqErrorCode *err)
{
    char url[1024];
    LwqqHttpRequest *req;
    char *response = NULL;
    char *cookies;
    int ret;
    
    snprintf(url, sizeof(url), "%s/login?u=%s&p=%s&verifycode=%s&"
             "webqq_type=%d&remember_uin=1&aid=1003903&login2qq=1&"
             "u1=http%%3A%%2F%%2Fweb.qq.com%%2Floginproxy.html"
             "%%3Flogin2qq%%3D1%%26webqq_type%%3D10&h=1&ptredirect=0&"
             "ptlang=2052&from_ui=1&pttype=1&dumy=&fp=loginerroralert&"
             "action=2-11-7438&mibao_css=m_webqq&t=1&g=1", LWQQ_URL_LOGIN_HOST, lc->username, md5, lc->vc->str,lc->stat);

    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }
    /* Setup http header */
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }

    /* Send request */
    ret = req->do_request(req, 0, NULL);
    if (ret) {
        *err = LWQQ_EC_NETWORK_ERROR;
        goto done;
    }
    if (req->http_code != 200) {
        *err = LWQQ_EC_HTTP_ERROR;
        goto done;
    }
    if (strstr(req->response,"aq.qq.com")!=NULL){
        *err = LWQQ_EC_LOGIN_ABNORMAL;
        const char* beg = strstr(req->response,"http://aq.qq.com");
        sscanf(beg,"%[^']",lc->error_description);
        goto done;
    }

    response = req->response;
    char *p = strstr(response, "\'");
    if (!p) {
        *err = LWQQ_EC_ERROR;
        goto done;
    }
    char buf[4] = {0};
    int status;
    strncpy(buf, p + 1, 1);
    status = atoi(buf);

    switch (status) {
    case 0:
        if (sava_cookie(lc, req, err)) {
            goto done;
        }
        break;
        
    case 1:
        lwqq_log(LOG_WARNING, "Server busy! Please try again\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    case 2:
        lwqq_log(LOG_ERROR, "Out of date QQ number\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    case 3:
        lwqq_log(LOG_ERROR, "Wrong password\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    case 4:
        lwqq_log(LOG_ERROR, "Wrong verify code\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    case 5:
        lwqq_log(LOG_ERROR, "Verify failed\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    case 6:
        lwqq_log(LOG_WARNING, "You may need to try login again\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    case 7:
        lwqq_log(LOG_ERROR, "Wrong input\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    case 8:
        lwqq_log(LOG_ERROR, "Too many logins on this IP. Please try again\n");
        *err = LWQQ_EC_ERROR;
        goto done;

    default:
        *err = LWQQ_EC_ERROR;
        lwqq_log(LOG_ERROR, "Unknow error");
        goto done;
    }

    set_online_status(lc, lwqq_status_to_str(lc->stat), err);
done:
    lwqq_http_request_free(req);
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

static void get_version(LwqqClient *lc, LwqqErrorCode *err)
{
    LwqqHttpRequest *req;
    char *response = NULL;
    int ret;

    req = lwqq_http_create_default_request(LWQQ_URL_VERSION, err);
    if (!req) {
        goto done;
    }

    /* Send request */
    lwqq_log(LOG_DEBUG, "Get webqq version from %s\n", LWQQ_URL_VERSION);
    ret = req->do_request(req, 0, NULL);
    if (ret || req->http_code!=200) {
        *err = LWQQ_EC_NETWORK_ERROR;
        goto done;
    }
    response = req->response;
    if(response == NULL){
        *err = LWQQ_EC_NETWORK_ERROR;
        goto done;
    }
    if (strstr(response, "ptuiV")) {
        char *s, *t;
        char *v;
        s = strchr(response, '(');
        t = strchr(response, ')');
        if (!s || !t) {
            *err = LWQQ_EC_ERROR;
            goto done;
        }
        s++;
        v = alloca(t - s + 1);
        memset(v, 0, t - s + 1);
        strncpy(v, s, t - s);
        s_free(lc->version);
        lc->version = s_strdup(v);
        *err = LWQQ_EC_OK;
    }

done:
    lwqq_http_request_free(req);
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
static void set_online_status(LwqqClient *lc,const char *status, LwqqErrorCode *err)
{
    char msg[1024] ={0};
    char *buf;
    LwqqHttpRequest *req = NULL;  
    char *response = NULL;
    char *cookies;
    int ret;
    json_t *json = NULL;
    char *value;

    if (!status || !err) {
        goto done ;
    }

    lc->clientid = generate_clientid();
    if (!lc->clientid) {
        lwqq_log(LOG_ERROR, "Generate clientid error\n");
        *err = LWQQ_EC_ERROR;
        goto done ;
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
    req = lwqq_http_create_default_request(LWQQ_URL_SET_STATUS, err);
    if (!req) {
        goto done;
    }

    /* Set header needed by server */
    req->set_header(req, "Cookie2", "$Version=1");
    req->set_header(req, "Referer", "http://d.web2.qq.com/proxy.html?v=20101025002");
    req->set_header(req, "Content-type", "application/x-www-form-urlencoded");
    
    /* Set http cookie */
    cookies = lwqq_get_cookies(lc);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    
    ret = req->do_request(req, 1, msg);
    if (ret) {
        *err = LWQQ_EC_NETWORK_ERROR;
        goto done;
    }
    if (req->http_code != 200) {
        *err = LWQQ_EC_HTTP_ERROR;
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
        *err = LWQQ_EC_ERROR;
        goto done;
    }

    if (!(value = json_parse_simple_value(json, "retcode"))) {
        *err = LWQQ_EC_ERROR;
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

    *err = LWQQ_EC_OK;
    
done:
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
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
    if (!client || !err|| !status) {
        lwqq_log(LOG_ERROR, "Invalid pointer\n");
        return ;
    }

    /* First: get webqq version */
    get_version(client, err);
    if (*err) {
        lwqq_log(LOG_ERROR, "Get webqq version error\n");
        return ;
    }
    lwqq_log(LOG_NOTICE, "Get webqq version: %s\n", client->version);

    /**
     * Second, we get the verify code from server.
     * If server provide us a image and let us enter code shown
     * in image number, in this situation, we just return LWQQ_EC_LOGIN_NEED_VC
     * simply, so user should call lwqq_login() again after he set correct
     * code to vc->str;
     * Else, if we can get the code directly, do login immediately.
     * 
     */
    if (!client->vc) {
        get_verify_code(client, err);
        switch (*err) {
        case LWQQ_EC_LOGIN_NEED_VC:
            get_verify_image(client);
            lwqq_log(LOG_WARNING, "Need to enter verify code\n");
            return ;
        
        case LWQQ_EC_NETWORK_ERROR:
            lwqq_log(LOG_ERROR, "Network error\n");
            return ;

        case LWQQ_EC_OK:
            lwqq_log(LOG_DEBUG, "Get verify code OK\n");
            break;

        default:
            lwqq_log(LOG_ERROR, "Unknown error\n");
            return ;
        }
    }
    
    /* Third: calculate the md5 */
    char *md5 = lwqq_enc_pwd(client->password, client->vc->str, client->vc->uin);

    client->stat = status;
    /* Last: do real login */
    do_login(client, md5, err);
    s_free(md5);

    /* Free old value */
    lwqq_vc_free(client->vc);
    client->vc = NULL;
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
    char *cookies;
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
    req = lwqq_http_create_default_request(url, err);
    if (!req) {
        goto done;
    }

    /* Set header needed by server */
    req->set_header(req, "Referer", "http://ptlogin2.qq.com/proxy.html?v=20101025002");
    
    /* Set http cookie */
    cookies = lwqq_get_cookies(client);
    if (cookies) {
        req->set_header(req, "Cookie", cookies);
        s_free(cookies);
    }
    
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

/*void lwqq_relogin(LwqqClient* lc)
{
    char url[128];
    char post[1024];
    int ret;
    snprintf(url,sizeof(url),"%s/channel/login2","http://d.web2.qq.com");
    LwqqHttpRequest* req = lwqq_http_create_default_request(url,NULL);
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
