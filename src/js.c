#include "js.h"

#include <lwqq.h>
#include <stdint.h>
#include <jsapi.h>

struct qq_js_t {
    JSRuntime* runtime;
    JSContext* context;
    JSObject*  global;
};

static JSClass global_class = { 
    "global", 
    JSCLASS_GLOBAL_FLAGS|JSCLASS_NEW_RESOLVE, 
    JS_PropertyStub, 
    JS_PropertyStub, 
    JS_PropertyStub, 
    JS_StrictPropertyStub, 
    JS_EnumerateStub, 
    JS_ResolveStub, 
    JS_ConvertStub, 
    NULL, 
    JSCLASS_NO_OPTIONAL_MEMBERS 
 }; 

static void report_error(JSContext *cx,  const char *message, JSErrorReport *report)
{ 
    fprintf(stderr, "%s:%u:%s\n", 
            report->filename ? report->filename : "<no filename>", 
            (unsigned int) report->lineno, 
            message); 
} 

qq_js_t* qq_js_init()
{
    qq_js_t* h = s_malloc0(sizeof(*h));
    h->runtime = JS_NewRuntime(8L*1024L*1024L);
    h->context = JS_NewContext(h->runtime, 8*1024);
    JS_SetOptions(h->context, JSOPTION_VAROBJFIX);
    JS_SetErrorReporter(h->context, report_error);
    h->global = JS_NewCompartmentAndGlobalObject(h->context, &global_class, NULL);
    JS_InitStandardClasses(h->context, h->global);
    return h;
}

qq_jso_t* qq_js_load(qq_js_t* js,const char* file)
{
    JSObject* global = JS_GetGlobalObject(js->context);
    JSObject* script = JS_CompileFile(js->context, global, file);
    JS_ExecuteScript(js->context, global, script, NULL);
    return (qq_jso_t*)script;
}
void qq_js_unload(qq_js_t* js,qq_jso_t* obj)
{
    //JS_DecompileScriptObject(js->context, obj, <#const char *name#>, <#uintN indent#>);
}

char* qq_js_hash(const char* uin,const char* ptwebqq,qq_js_t* js)
{
    JSObject* global = JS_GetGlobalObject(js->context);
    jsval res;
    jsval argv[2];

    JSString* uin_ = JS_NewStringCopyZ(js->context, uin);
    JSString* ptwebqq_ = JS_NewStringCopyZ(js->context, ptwebqq);
    argv[0] = STRING_TO_JSVAL(uin_);
    argv[1] = STRING_TO_JSVAL(ptwebqq_);
    JS_CallFunctionName(js->context, global, "P", 2, argv, &res);

    JSString* res_ = JS_ValueToString(js->context, res);

    return JS_EncodeString(js->context, res_);
}

void qq_js_close(qq_js_t* js)
{
    JS_DestroyContext(js->context);
    JS_DestroyRuntime(js->runtime);
    JS_ShutDown();
    s_free(js);
}
