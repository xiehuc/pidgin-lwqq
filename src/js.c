#include <jsapi.h>

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

JSRuntime* runtime = NULL;
JSContext* context = NULL;

static void report_error(JSContext *cx,  const char *message, JSErrorReport *report)
{ 
    fprintf(stderr, "%s:%u:%s\n", 
    		 report->filename ? report->filename : "<no filename>", 
            (unsigned int) report->lineno, 
            message); 
 } 
void qq_js_init()
{
    JSObject* global = NULL;
    runtime = JS_NewRuntime(8L*1024L*1024L);
    context = JS_NewContext(runtime, 8*1024);
    JS_SetOptions(context, JSOPTION_VAROBJFIX);
    JS_SetErrorReporter(context, report_error);
    global = JS_NewCompartmentAndGlobalObject(context, &global_class, NULL);
    JS_InitStandardClasses(context, global);
}

void qq_js_load(const char* file)
{
    JSObject* global = JS_GetGlobalObject(context);
    JSObject* script = JS_CompileFile(context, global, file);
    JS_ExecuteScript(context, global, script, NULL);
}

char* qq_js_hash(const char* uin,const char* ptwebqq)
{
    JSObject* global = JS_GetGlobalObject(context);
    jsval res;
    jsval argv[2];

    JSString* uin_ = JS_NewStringCopyZ(context, uin);
    JSString* ptwebqq_ = JS_NewStringCopyZ(context, ptwebqq);
    argv[0] = STRING_TO_JSVAL(uin_);
    argv[1] = STRING_TO_JSVAL(ptwebqq_);
    JS_CallFunctionName(context, global, "P", 2, argv, &res);

    JSString* res_ = JS_ValueToString(context, res);

    return JS_EncodeString(context, res_);
}

void qq_js_close()
{
    JS_DestroyContext(context);
    JS_DestroyRuntime(runtime);
    JS_ShutDown();
}
