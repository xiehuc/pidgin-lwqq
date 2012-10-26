set(SRC_LIST
  async.c
  background.c
  trex.c
  webqq.c
  translate.c
  http_curl.c
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
  liblwqq/info.c)

add_definitions(-Wall)
ADD_LIBRARY(webqq MODULE
    ${SRC_LIST}
    ${LWQQ_LIST}
    )
include_directories(
  ${PURPLE_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  liblwqq)

target_link_libraries(webqq
  ${PURPLE_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${CURL_LIBRARIES}
  )

INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})
