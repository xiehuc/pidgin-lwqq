set(SRC_LIST
  background.c
  trex.c
  webqq.c
  translate.c
  qq_types.c
  ft.c
  )

set(LWQQ_LIST
  liblwqq/async.c
  liblwqq/http_curl.c
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
  )

add_definitions(-Wall)
add_definitions(-DUSE_LIBPURPLE)
ADD_LIBRARY(webqq MODULE
    ${SRC_LIST}
    ${LWQQ_LIST}
    )
include_directories(
  ${PURPLE_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  liblwqq)

target_link_libraries(webqq
  ${SQLITE_LIBRARIES}
  ${PURPLE_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${CURL_LIBRARIES}
  #  ev
  )

INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})
