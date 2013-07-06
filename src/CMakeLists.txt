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

ADD_LIBRARY(webqq MODULE
    ${SRC_LIST}
    ${LWQQ_LIST}
    )
include_directories(
  ${PURPLE_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
  ../liblwqq/lib
  )

#if(WITH_LIBEV)
add_definitions(-DUSE_LIBEV)
include_directories(${EV_INCLUDE_DIRS})
target_link_libraries(webqq ${EV_LIBRARIES})
#else(WITH_LIBEV)
#add_definitions(-DUSE_LIBPURPLE)
#endif(WITH_LIBEV)

if(SSL)
    add_definitions(-DLWQQ_ENABLE_SSL=1)
endif(SSL)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DUSE_DEBUG=1 -Wno-unused-function")

target_link_libraries(webqq
  ${SQLITE_LIBRARIES}
  ${PURPLE_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${CURL_LIBRARIES}
  ${ZLIB_LIBRARIES}
  )


INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})
