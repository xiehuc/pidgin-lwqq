#ifndef WEBQQ_H_H
#define WEBQQ_H_H

#ifdef UNUSED
#elif defined(__GNUC__)
# 	define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#else
#	define UNUSED(x) x
#endif

#define DISPLAY_VERSION "0.1"
#define DBGID   "webqq"
#define QQ_ROOM_KEY_QUN_ID "gid"

#endif
