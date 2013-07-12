set(SRC_LIST
    trex.c
    webqq.c
    translate.c
    qq_types.c
    ft.c
    cgroup.c
    )

set(LWQQ_LIST
    ../liblwqq/lib/url.c
    ../liblwqq/lib/md5.c
    ../liblwqq/lib/unicode.c
    ../liblwqq/lib/logger.c
    ../liblwqq/lib/login.c
    ../liblwqq/lib/json.c
    ../liblwqq/lib/msg.c
    ../liblwqq/lib/type.c
    ../liblwqq/lib/smemory.c
    ../liblwqq/lib/info.c
    ../liblwqq/lib/lwdb.c
    ../liblwqq/lib/swsqlite.c
    ../liblwqq/lib/vplist.c
    ../liblwqq/lib/utility.c
    ../liblwqq/lib/internal.c
    ../liblwqq/lib/async.c
    ../liblwqq/lib/http.c
    )

add_definitions(-Wall)

if(WIN32)
    set(LWQQ_LIST ${LWQQ_LIST} ../liblwqq/lib/win32.c 
        ../liblwqq/libev/ev.c 
        ../liblwqq/libev/event.c
        )
    set(LWQQ_OTHER_LIBS ${LWQQ_OTHER_LIBS} ws2_32)
    add_definitions(-DEV_STANDALONE)
    include_directories(../liblwqq/libev)
endif(WIN32)

ADD_LIBRARY(webqq MODULE
    ${SRC_LIST}
    ${LWQQ_LIST}
    )
include_directories(
    ${SQLITE_INCLUDE_DIRS}
    ${LIBPURPLE_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ../liblwqq/lib
    )

if(SSL)
    add_definitions(-DLWQQ_ENABLE_SSL=1)
endif(SSL)
if(WITHOUT_LIBEV)
    add_definitions(-DWITHOUT_LIBEV=1)
endif(WITHOUT_LIBEV)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DUSE_DEBUG=1 -Wno-unused-function")

target_link_libraries(webqq
    ${SQLITE_LIBRARIES}
    ${LIBPURPLE_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${CURL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LWQQ_OTHER_LIBS}
    ${EV_LIBRARIES}
    pthread
    )

if(UNIX)
    INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})
endif(UNIX)
