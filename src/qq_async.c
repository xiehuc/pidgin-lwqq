#include "qq_async.h"
#include <ghttp.h>
#include <type.h>

typedef struct async_dispatch_data{
    ListenerType type;
    qq_account* ac;
    gint handle;
    int err;
}async_dispatch_data;
typedef struct async_watch_data{
    CALLBACK callback;
    void* data;
    LwqqHttpRequest* request;
    gint handle;
    qq_account* ac;
}async_watch_data;

typedef struct _AsyncListener {
    ASYNC_CALLBACK login_complete;
    void* login_data;
    ASYNC_CALLBACK verify_come;
    void* verify_data;
    ASYNC_CALLBACK friends_complete;
    void* friends_data;
    ASYNC_CALLBACK groups_complete;
    void* groups_data;
    ASYNC_CALLBACK msg_come;
    void* msg_data;
    ASYNC_CALLBACK online_come;
    void* online_data;
} _AsyncListener;


void qq_async_add_listener(qq_account* ac,ListenerType type,ASYNC_CALLBACK callback,void*data)
{
    AsyncListener* async = ac->async;
    if(!qq_async_enabled(ac))return;
    switch(type){
        case LOGIN_COMPLETE:
            async->login_complete = callback;
            async->login_data = data;
            break;
        case VERIFY_COME:
            async->verify_come = callback;
            async->verify_data = data;
            break;
        case FRIENDS_COMPLETE:
            async->friends_complete=callback;
            async->friends_data = data;
            break;
        case GROUPS_COMPLETE:
            async->groups_complete=callback;
            async->groups_data = data;
            break;
        case MSG_COME:
            async->msg_come = callback;
            async->msg_data = data;
            break;
        case ONLINE_COME:
            async->online_come = callback;
            async->online_data = data;
            break;
    }
}


static gboolean timeout_come(void* p)
{
    async_dispatch_data* data = (async_dispatch_data*)p;
    qq_account* lc = data->ac;
    AsyncListener* async = lc->async;
    ListenerType type = data->type;
    switch(type){
        case LOGIN_COMPLETE:
            if(async->login_complete!=NULL)
            async->login_complete(lc,data->err,async->login_data);
            break;
        case VERIFY_COME:
            if(async->verify_come!=NULL)
            async->verify_come(lc,data->err,async->verify_data);
            break;
        case FRIENDS_COMPLETE:
            if(async->friends_complete!=NULL)
                async->friends_complete(lc,data->err,async->verify_data);
            break;
        case GROUPS_COMPLETE:
            if(async->groups_complete!=NULL)
                async->groups_complete(lc,data->err,async->groups_data);
            break;
        case MSG_COME:
            if(async->msg_come!=NULL)
                async->msg_come(lc,data->err,async->msg_data);
            break;
        case ONLINE_COME:
            if(async->online_come!=NULL)
                async->online_come(lc,data->err,async->online_data);
            break;
    }
    if(data->handle>0)
        purple_timeout_remove(data->handle);
    free(data);
    return 1;
}
void qq_async_dispatch(qq_account* lc,ListenerType type,LwqqErrorCode err)
{
    async_dispatch_data* data = malloc(sizeof(async_dispatch_data));
    data->type = type;
    data->ac = lc;
    data->err = err;
    data->handle = purple_timeout_add(50,timeout_come,data);
}
void qq_async_dispatch_2(qq_account* lc,ListenerType type,void* d)
{
    async_dispatch_data* data = malloc(sizeof(async_dispatch_data));
    data->type = type;
    data->ac = lc;
    data->err = (int)d;
    data->handle = 0;
    timeout_come(data);
}

void qq_async_set(qq_account* client,int enabled)
{
    if(enabled&&!qq_async_enabled(client)){
        client->async = g_malloc0(sizeof(AsyncListener));
    }else if(!enabled&&qq_async_enabled(client)){
        g_free(client->async);
        client->async=NULL;
    }
}

static void input_come(gpointer p,gint i,PurpleInputCondition cond)
{
    async_watch_data* data = (async_watch_data*)p;
    ghttp_status status;
    LwqqHttpRequest* request = data->request;
    status = ghttp_process(request->req);

    if(status!=ghttp_done) return ;

    if(data->callback!=NULL)data->callback(data->ac,data->request,data->data);

    purple_input_remove(data->handle);
    free(data);
}

void qq_async_watch(qq_account* ac,LwqqHttpRequest* request,CALLBACK callback,void* d)
{
    ghttp_request* req = (ghttp_request*)request->req;
    async_watch_data* data = malloc(sizeof(async_watch_data));
    data->request = request;
    data->callback = callback;
    data->data = d;
    data->ac = ac;
    data->handle = purple_input_add(ghttp_get_socket(req),PURPLE_INPUT_READ,
            input_come,data);
}

