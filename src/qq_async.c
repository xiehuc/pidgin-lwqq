#include "qq_async.h"


typedef struct async_watch_data{
    ListenerType type;
    qq_account* ac;
    gint handle;
    LwqqErrorCode err;
}async_watch_data;

typedef struct _AsyncListener {
    ASYNC_CALLBACK login_complete;
    void* login_data;
    ASYNC_CALLBACK verify_come;
    void* verify_data;
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
    }
}


static gboolean timeout_come(void* p)
{
    async_watch_data* data = (async_watch_data*)p;
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
    }
    purple_timeout_remove(data->handle);
    return 1;
}
void qq_async_dispatch(qq_account* lc,ListenerType type,LwqqErrorCode err)
{
    async_watch_data* data = malloc(sizeof(async_watch_data));
    data->type = type;
    data->ac = lc;
    data->err = err;
    data->handle = purple_timeout_add(50,timeout_come,data);
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
