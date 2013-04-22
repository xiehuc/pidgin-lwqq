#include "remote.h"
#include "async.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

static struct {
    int serv;
    int ref;
    struct sockaddr_in my_addr;
    LwqqAsyncIo watcher;
}remote_handle = {0};
#define rh remote_handle

/*
static const char html[]=
"<html><head><script type='text/javascript'>"
" var p = location.href.substring(location.href.indexOf('?')+1);"
" var u = p.substring(0,p.indexOf('&'));"
" var c = p.substring(p.indexOf('&')+1);"
" alert(u);alert(c);"
" document.cookie = c;"
" window.cookie = c;"
" document.location.href = u;"
"</script></head></html>";
*/
static const char html[]=
//"HTTP/1.1 302 Moved Temporarily\n"
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Cache-Control: no-cache\r\n"
"Set-Cookie: uin=oxxxxxxxxx; Domain=.qq.com; Path=/; Expires=Thu, 02 Jan 2020 00:00:00 GMT\r\n"
"Set-Cookie: ptui_loginuin=xxxxxxxxx; Domain=.qq.com; Path=/; Expires=Thu, 02 Jan 2020 00:00:00 GMT\r\n"
"Set-Cookie: pt2gguin=oxxxxxxxxx; Domain=.qq.com; Path=/; Expires=Thu, 02 Jan 2020 00:00:00 GMT\r\n"
"Set-Cookie: o_cookie=oxxxxxxxxx; Domain=.qq.com; Path=/; Expires=Thu, 02 Jan 2020 00:00:00 GMT\r\n"
;
static const char body[]=
"<html><head><script>window.location.href='http://mail.qq.com/'</script></head></html>";
#define PORT 8889

static void remote_connect_and_write(void* data,int fd,int action)
{
    char buf[38192];
    struct sockaddr_in cli_addr;
    socklen_t sz = sizeof(cli_addr);
    int client = accept(fd,(struct sockaddr*)&cli_addr,&sz);
    snprintf(buf,sizeof(buf),"%sContent-Length: %ld\r\n\r\n%s",html,strlen(body),body);
    send(client,buf,strlen(buf)+1,0);
    /*send(client,html,sizeof(html),0);
    snprintf(buf,sizeof(buf),"Location: %s\n\n","http://id.qq.com/");
    send(client,buf,strlen(buf)+1,0);*/
    close(client);
}
static unsigned int h;
void qq_remote_init()
{
    if(rh.serv == 0){
        rh.serv = socket(AF_INET,SOCK_STREAM,0);
        memset(&rh.my_addr,0,sizeof(rh.my_addr));
        rh.my_addr.sin_family=AF_INET;
        rh.my_addr.sin_addr.s_addr=INADDR_ANY;
        rh.my_addr.sin_port=htons(8889);
        bind(rh.serv,(struct sockaddr*)&rh.my_addr,sizeof(rh.my_addr));
        listen(rh.serv,5);
        h = purple_input_add(rh.serv, PURPLE_INPUT_READ, (PurpleInputFunction)remote_connect_and_write, &rh);
        //lwqq_async_io_watch(&rh.watcher, rh.serv, LWQQ_ASYNC_READ, remote_connect_and_write, &rh);
    }
    rh.ref++;
}
void qq_remote_call(qq_account* ac,const char* url)
{
    char buf[2048];
    snprintf(buf,sizeof(buf),"xdg-open 'http://127.0.0.1:%d/?%s&%s'",
            PORT,url,lwqq_get_cookies(ac->qq));
    printf("%s\n",lwqq_get_cookies(ac->qq));
    system(buf);
}
void qq_remove_destroy()
{
    rh.ref--;
    if(rh.ref==0){
        //lwqq_async_io_stop(&rh.watcher);
        purple_input_remove(h);
        close(rh.serv);
        rh.serv = 0;
    }
}
