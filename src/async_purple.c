#include "async.h"
#include "async_impl.h"
#include "smemory.h"
#include <glib.h>
// never add file to build
// it would be included in webqq.c directly
#undef LWQQ__ASYNC_IMPL
#define LWQQ__ASYNC_IMPL(x) x

struct LwqqAsyncTimer_ {
   LwqqAsyncTimer super;
   guint h;
};
struct LwqqAsyncIo_ {
   LwqqAsyncIo super;
   guint h;
};
static void blank_func() {}
static void* LWQQ__ASYNC_IMPL(io_new)()
{
   return s_malloc0(sizeof(struct LwqqAsyncIo_));
}

static void LWQQ__ASYNC_IMPL(io_free)(void* h) { s_free(h); }
static void io_cb_wrap(gpointer data, gint fd, PurpleInputCondition c)
{
   struct LwqqAsyncIo_* io_ = data;
   if (io_->super.func)
      io_->super.func(data, io_->super.fd, io_->super.action, io_->super.data);
}
static void LWQQ__ASYNC_IMPL(io_start)(void* io, int fd, int action)
{
   struct LwqqAsyncIo_* io_ = (struct LwqqAsyncIo_*)io;
   io_->h = purple_input_add(fd, action, io_cb_wrap, io);
}

static void LWQQ__ASYNC_IMPL(io_stop)(void* io)
{
   struct LwqqAsyncIo_* io_ = (struct LwqqAsyncIo_*)io;
   purple_input_remove(io_->h);
}

static void* LWQQ__ASYNC_IMPL(timer_new)()
{
   return s_malloc0(sizeof(struct LwqqAsyncTimer_));
}

static void LWQQ__ASYNC_IMPL(timer_free)(void* t) { s_free(t); }

static gboolean timer_cb_wrap(gpointer data)
{
   struct LwqqAsyncTimer_* io_ = data;
   if (io_->super.func)
      io_->super.func(data, io_->super.data);
   return TRUE;
}

void LWQQ__ASYNC_IMPL(timer_start)(void* t, unsigned int ms)
{
   struct LwqqAsyncTimer_* t_ = (struct LwqqAsyncTimer_*)t;
   t_->h = purple_timeout_add(ms, timer_cb_wrap, t);
}

void LWQQ__ASYNC_IMPL(timer_stop)(void* t)
{
   struct LwqqAsyncTimer_* t_ = (struct LwqqAsyncTimer_*)t;
   purple_timeout_remove(t_->h);
}

static LwqqAsyncImpl impl_purple = {
   .name = "purple",
   .flags = NO_THREAD,
   .loop_create = blank_func,
   .loop_run = blank_func,
   .loop_stop = blank_func,
   .loop_free = blank_func,

   .io_new = io_new,
   .io_free = io_free,
   .io_start = io_start,
   .io_stop = io_stop,

   .timer_new = timer_new,
   .timer_free = timer_free,
   .timer_start = timer_start,
   .timer_stop = timer_stop,
   .timer_again = blank_func,
};

