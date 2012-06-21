
#include <ghttp.h>
#include <logger.h>
#include <json.h>
#include <smemory.h>
#include <string.h>
#include <http.h>
#include <zlib.h>

#include "qq_async.h"
static void get_friend_qqnumber_callback(qq_account* ac,LwqqHttpRequest* request,void* data);

static char *unzlib(const char *source, int len, int *total, int isgzip)
{
#define CHUNK 16 * 1024
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char out[CHUNK];
    int totalsize = 0;
    char *dest = NULL;

    if (!source || len <= 0 || !total)
        return NULL;

/* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    if(isgzip) {
        /**
         * 47 enable zlib and gzip decoding with automatic header detection,
         * So if the format of compress data is gzip, we need passed it to
         * inflateInit2
         */
        ret = inflateInit2(&strm, 47);
    } else {
        ret = inflateInit(&strm);
    }

    if (ret != Z_OK) {
        lwqq_log(LOG_ERROR, "Init zlib error\n");
        return NULL;
    }

    strm.avail_in = len;
    strm.next_in = (Bytef *)source;

    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        switch (ret) {
        case Z_STREAM_END:
            break;
        case Z_BUF_ERROR:
            lwqq_log(LOG_ERROR, "Unzlib error\n");
            break;
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR; /* and fall through */
            break;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_STREAM_ERROR:
            lwqq_log(LOG_ERROR, "Ungzip stream error!", strm.msg);
            inflateEnd(&strm);
            goto failed;
        }
        have = CHUNK - strm.avail_out;
        totalsize += have;
        dest = s_realloc(dest, totalsize);
        memcpy(dest + totalsize - have, out, have);
    } while (strm.avail_out == 0);

/* clean up and return */
    (void)inflateEnd(&strm);
    if (ret != Z_STREAM_END) {
        goto failed;
    }
    *total = totalsize;
    return dest;

failed:
    if (dest) {
        s_free(dest);
    }
    lwqq_log(LOG_ERROR, "Unzip error\n");
    return NULL;
}

static char *ungzip(const char *source, int len, int *total)
{
    return unzlib(source, len, total, 1);
}
static char *lwqq_http_get_header(LwqqHttpRequest *request, const char *name)
{
    if (!name) {
        lwqq_log(LOG_ERROR, "Invalid parameter\n");
        return NULL; 
    }

    const char *h = ghttp_get_header(request->req, name);
    if (!h) {
        lwqq_log(LOG_WARNING, "Cant get http header: %s\n", name);
        return NULL;
    }

    return s_strdup(h);
}


static int fake_do_request(LwqqHttpRequest *request, int method, char *body)
{
    if (!request->req)
        return -1;


    ghttp_status status;
    ghttp_type m;
    char **resp = &request->response;

    /* Clear off last response */
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }

    /* Set http method */
    if (method == 0) {
        m = ghttp_type_get;
    } else if (method == 1) {
        m = ghttp_type_post;
    } else {
        lwqq_log(LOG_WARNING, "Wrong http method\n");
        goto failed;
    }
    if (ghttp_set_type(request->req, m) == -1) {
        lwqq_log(LOG_WARNING, "Set request type error\n");
        goto failed;
    }
    ghttp_set_sync(request->req,ghttp_async);

    /* For POST method, set http body */
    if (m == ghttp_type_post && body) {
        ghttp_set_body(request->req, body, strlen(body));
    }
    
    if (ghttp_prepare(request->req)) {
        goto failed;
    }

    status = ghttp_process(request->req);
    if(status == ghttp_error) {
        lwqq_log(LOG_ERROR, "Http request failed: %s\n",
                ghttp_get_error(request->req));
        goto failed;
    }
    if(status == ghttp_not_done){
        return 0;
    }

failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return -1;
}

static int fake_finish_request(LwqqHttpRequest* request)
{

    char **resp = &request->response;
    char *buf;
    int len = 0;
    int have_read_bytes = 0;
    /* NOTE: buf may NULL, notice it */
    buf = ghttp_get_body(request->req);
    if (buf) {
        len = ghttp_get_body_len(request->req);
        *resp = s_realloc(*resp, have_read_bytes + len);
        memcpy(*resp + have_read_bytes, buf, len);
        have_read_bytes += len;
    }

    /* NB: *response may null */
    if (*resp == NULL) {
        goto failed;
    }

    /* Uncompress data here if we have a Content-Encoding header */
    char *enc_type = NULL;
    enc_type = lwqq_http_get_header(request, "Content-Encoding");
    if (enc_type && strstr(enc_type, "gzip")) {
        char *outdata;
        int total = 0;
        
        outdata = ungzip(*resp, have_read_bytes, &total);
        if (!outdata) {
            s_free(enc_type);
            goto failed;
        }

        s_free(*resp);
        /* Update response data to uncompress data */
        *resp = s_strdup(outdata);
        s_free(outdata);
        have_read_bytes = total;
    }
    s_free(enc_type);

    /* OK, done */
    if ((*resp)[have_read_bytes -1] != '\0') {
        /* Realloc a byte, cause *resp hasn't end with char '\0' */
        *resp = s_realloc(*resp, have_read_bytes + 1);
        (*resp)[have_read_bytes] = '\0';
    }
    request->http_code = ghttp_status_code(request->req);
    return 0;
failed:
    if (*resp) {
        s_free(*resp);
        *resp = NULL;
    }
    return -1;
}

static char *get_friend_qqnumber(qq_account* ac,LwqqBuddy* buddy )
{
    LwqqClient* lc = ac->qq;
    const char* uin=buddy->uin;
    if (!lc || !uin) {
        return NULL;
    }

    char url[512];
    LwqqHttpRequest *req = NULL;  
    int ret;
    json_t *json = NULL;
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
    //ret = req->do_request(req, 0, NULL);
    fake_do_request(req,0,NULL);
    qq_async_watch(ac,req,get_friend_qqnumber_callback,buddy);
done:
    /* Free temporary string */
    if (json)
        json_free_value(&json);
    lwqq_http_request_free(req);
}

static void get_friend_qqnumber_callback(qq_account* ac,LwqqHttpRequest* req,void* data){
    int ret;
    json_t *json = NULL;
    char *qqnumber = NULL;
    LwqqBuddy* buddy=(LwqqBuddy*)data;
    fake_finish_request(req);

    if (/*ret ||*/ req->http_code != 200) {
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
    buddy->qqnumber = qqnumber;
    qq_async_dispatch_2(ac,FRIEND_COME,(int)buddy);
}

/** 
 * a fake version of Get all friends qqnumbers
 * 
 * @param lc 
 * @param err 
 */
void qq_fake_get_all_friend_qqnumbers(qq_account* ac, LwqqErrorCode *err)
{
    if(ac==NULL) return;
    LwqqClient* lc=ac->qq;
    LwqqBuddy *buddy;

    if (!lc)
        return ;
    
    LIST_FOREACH(buddy, &lc->friends, entries) {
        if (!buddy->qqnumber) {
            /** If qqnumber hasnt been fetched(NB: lc->myself has qqnumber),
             * fetch it
             */
            get_friend_qqnumber(ac, buddy);
            //lwqq_log(LOG_DEBUG, "Get buddy qqnumber: %s\n", buddy->qqnumber);
        }
    }

    if (err) {
        *err = LWQQ_EC_OK;
    }
}
void qq_fake_kill_thread(qq_account* ac);
