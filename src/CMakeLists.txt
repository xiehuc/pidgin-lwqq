AUX_SOURCE_DIRECTORY(. SRC_LIST)


ADD_LIBRARY(webqq MODULE 
    ${SRC_LIST} 
    )

INCLUDE_DIRECTORIES(webqq
	${PURPLE_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${PIDGIN_INCLUDE_DIRS}
	)
TARGET_LINK_LIBRARIES(webqq
					${PURPLE_LIBRARIES}
                    ${GLIB_LIBRARIES}
                    ${PIDGIN_LIBRARIES}
					)


FIND_PACKAGE(PkgConfig REQUIRED)

PKG_CHECK_MODULES(PURPLE REQUIRED  purple)
PKG_CHECK_MODULES(GLIB REQUIRED  glib-2.0)
PKG_CHECK_MODULES(PIDGIN REQUIRED  pidgin)


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
SET(LIB_INSTALL_DIR "${PURPLE_PREFIX}lib/purple-${PURPLE_MAJOR_VERSION}")

INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})

