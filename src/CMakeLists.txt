AUX_SOURCE_DIRECTORY(. SRC_LIST)
AUX_SOURCE_DIRECTORY(../include/liblwqq LWQQ_LIST)
LIST(REMOVE_ITEM LWQQ_LIST ../include/liblwqq/http.c)
LIST(REMOVE_ITEM LWQQ_LIST ../include/liblwqq/http_curl.c)
LIST(REMOVE_ITEM LWQQ_LIST ../include/liblwqq/async.c)
LIST(REMOVE_ITEM SRC_LIST ./http.c)
LIST(REMOVE_ITEM SRC_LIST ./test.c)

FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(PURPLE REQUIRED  purple)
PKG_CHECK_MODULES(GLIB REQUIRED  glib-2.0)
PKG_CHECK_MODULES(CURL REQUIRED libcurl)
ADD_LIBRARY (curldbg STATIC IMPORTED)
SET_PROPERTY (TARGET curldbg PROPERTY IMPORTED_LOCATION /home/xiehuc/PROJECT/pidgin-lwqq/lib/libcurl7.24.a )
#ADD_EXECUTABLE(test 
#    test.c
#    ../include/liblwqq/async.c
#    ../include/liblwqq/http_curl.c
#    ${LWQQ_LIST}
#    )
#TARGET_LINK_LIBRARIES(test
#    ${CURL_LIBRARIES}
#    ev
#    )

ADD_LIBRARY(webqq MODULE 
    ${SRC_LIST} 
    ${LWQQ_LIST}
    )
INCLUDE_DIRECTORIES(webqq
	${PURPLE_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ../include/libghttp
    ../include/liblwqq
	)
MESSAGE(state "${CURL_LIBRARIES}")
TARGET_LINK_LIBRARIES(webqq
    ${PURPLE_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${CURL_LIBRARIES}
    )

MESSAGE(state "prefix" "${CMAKE_INSTALL_PREFIX}")
ADD_DEFINITIONS(-g -O0 -Wall )
ADD_DEFINITIONS(-DINST_PREFIX="${CMAKE_INSTALL_PREFIX}")

option(NLS "Native language support" OFF)
if(NLS)
    find_package(Gettext)
endif(NLS)

IF(NOT PURPLE_MAJOR_VERSION)
	SET(PURPLE_MAJOR_VERSION "2")
ENDIF(NOT PURPLE_MAJOR_VERSION)

EXEC_PROGRAM("whereis libpurple| sed -e 's/[^\\/]*//' -e 's/\\(.*\\)lib\\/libpurple\\.so.*/\\1/'"
			OUTPUT_VARIABLE PURPLE_PREFIX)

EXEC_PROGRAM("whereis pidgin | sed -e 's/[^\\/]*//' -e 's/\\(.*\\)bin\\/pidgin.*/\\1/'"
			OUTPUT_VARIABLE PIDGIN_PREFIX)

SET(PIDGIN_PIX_INSTALL_DIR "${PIDGIN_PREFIX}share/pixmaps/pidgin/protocols")
SET(LIB_INSTALL_DIR "${PURPLE_PREFIX}lib/purple-${PURPLE_MAJOR_VERSION}")

INSTALL(DIRECTORY "../res/pidgin/" DESTINATION ${PIDGIN_PIX_INSTALL_DIR})
INSTALL(DIRECTORY "../res/empathy/" DESTINATION "/usr/share/empathy/icons/hicolor")
INSTALL(DIRECTORY "../res/qqfaces/" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/lwqq/icons/qqfaces/")
INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})

