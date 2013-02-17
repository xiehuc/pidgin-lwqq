set(SRC_LIST
  trex.c
  webqq.c
  translate.c
  qq_types.c
  ft.c
  )

set(LWQQ_LIST
  liblwqq/url.c
  liblwqq/md5.c
  liblwqq/unicode.c
  liblwqq/logger.c
  liblwqq/login.c
  liblwqq/json.c
  liblwqq/msg.c
  liblwqq/type.c
  liblwqq/smemory.c
  liblwqq/info.c
  liblwqq/lwdb.c
  liblwqq/swsqlite.c
  liblwqq/vplist.c
  liblwqq/util.c
  liblwqq/internal.c
  liblwqq/async.c
  liblwqq/http.c
  )

add_definitions(-Wall)

ADD_LIBRARY(webqq MODULE
    ${LWQQ_LIST}
    ${SRC_LIST}
    )
include_directories(
  ${PURPLE_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  liblwqq)

#if(WITH_LIBEV)
add_definitions(-DUSE_LIBEV)
include_directories(${EV_INCLUDE_DIRS})
target_link_libraries(webqq ${EV_LIBRARIES})
#else(WITH_LIBEV)
#add_definitions(-DUSE_LIBPURPLE)
#endif(WITH_LIBEV)

if(DB_NOSYNC)
    add_definitions(-DNOSYNC)
endif(DB_NOSYNC)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DUSE_DEBUG=1 -Wno-unused-function")

target_link_libraries(webqq
  ${SQLITE_LIBRARIES}
  ${PURPLE_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${CURL_LIBRARIES}
  )


INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})
