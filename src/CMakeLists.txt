AUX_SOURCE_DIRECTORY(. SRC_LIST)
AUX_SOURCE_DIRECTORY(../include/liblwqq LWQQ_LIST)
AUX_SOURCE_DIRECTORY(../include/libghttp GHTTP_LIST)

FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(PURPLE REQUIRED  purple)
PKG_CHECK_MODULES(GLIB REQUIRED  glib-2.0)
PKG_CHECK_MODULES(PIDGIN REQUIRED  pidgin)

ADD_LIBRARY(webqq MODULE 
    ${SRC_LIST} 
    ${LWQQ_LIST}
    ${GHTTP_LIST}
    )

INCLUDE_DIRECTORIES(webqq
	${PURPLE_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${PIDGIN_INCLUDE_DIRS}
    ../include/libghttp
    ../include/liblwqq
	)
TARGET_LINK_LIBRARIES(webqq
    ${PURPLE_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${PIDGIN_LIBRARIES}
    )


ADD_DEFINITIONS(-g -O0 -Wall )

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
INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})

