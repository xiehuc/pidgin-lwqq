project(pidgin-lwqq)
cmake_minimum_required(VERSION 2.6)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "3")
set(VERSION_PATCH "0")

set(CMAKE_MODULE_PATH 
    ${CMAKE_SOURCE_DIR}/cmake 
    ${CMAKE_MODULE_PATH}
    )
    
set(CPACK_GENERATOR "DEB")
set(ACK_PACKAGE_DESCRIPTION_SUMMARY "pidgin lwqq library")
set(CPACK_PACKAGE_VENDOR "edhelas")
set(CPACK_PACKAGE_DESCRIPTION "pidgin-lwqq provide a Pidgin plugin to connect to the QQ network using the lwqq library")
set(CPACK_PACKAGE_CONTACT "edhelas@movim.eu")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpurple0,libglib2.0-0,libwebqq")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(ACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${VERSION_MAJOR}.${VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jaussoin Timothée") #required

include(CPack)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	set(LINUX 1)
endif()

if(LINUX)
    set(SHARE_DIR "${CMAKE_INSTALL_PREFIX}/share")
    
    include_directories(/usr/include/lwqq)

    find_package(PkgConfig REQUIRED)
    find_package(lwqq)
    pkg_check_modules(LIBPURPLE REQUIRED  purple)
    pkg_check_modules(GLIB REQUIRED  glib-2.0)
	pkg_check_modules(LWQQ REQUIRED lwqq>=${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
	
    #recommand libcurl>=7.22.0
    exec_program("pkg-config --variable=plugindir purple 2>/dev/null"
        OUTPUT_VARIABLE LIB_INSTALL_DIR)
    exec_program("pkg-config --variable=datadir purple 2>/dev/null"
        OUTPUT_VARIABLE PURPLE_DATADIR)
    set(PURPLE_DATADIR "${PURPLE_DATADIR}/")
    exec_program("pkg-config --modversion purple 2>/dev/null"
        OUTPUT_VARIABLE PURPLE_VERSION)
	set(OPEN_PROG "xdg-open")
endif()

if(APPLE)
    set(CMAKE_PREFIX_PATH 
		"${PROJECT_SOURCE_DIR}/../adium/Frameworks/" )
	find_library(LIBPURPLE NAMES "libpurple" PATHS "${CMAKE_PERFIX_PATH}")
	find_library(GLIB NAMES "libglib" PATHS "${CMAKE_PERFIX_PATH}")
	set(OPEN_PROG "open")
endif()

if(WIN32)
    set(LIB_INSTALL_DIR "plugins")
    set(PURPLE_DATADIR  "")
    set(EV_FOUND Off)
    set(SHARE_DIR "share")

    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/../pidgin/libpurple" )
    find_package(LibPurple REQUIRED)

    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/glib_2.28.8-1_win32/include" 
        "${WIN32_LIBS_DIR}/glib_2.28.8-1_win32/lib"
        "${WIN32_LIBS_DIR}/gtk_2_0-2.14/include/glib-2.0"
        "${WIN32_LIBS_DIR}/gtk_2_0-2.14/lib"
        )
    find_package(GLIB2 REQUIRED)

    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/../lwqq/lib" 
        "${WIN32_LIBS_DIR}/../lwqq/build"
        "${WIN32_LIBS_DIR}/../lwqq/build/lib"
        )
    find_package(Lwqq REQUIRED)
    
    get_filename_component( MinGW_Path ${CMAKE_CXX_COMPILER} PATH )
	set(OPEN_PROG "start")
endif(WIN32)

add_definitions(-Wall)

message("libpurple version:${PURPLE_VERSION}")

if(PURPLE_VERSION VERSION_LESS "2.8")
    message("libpurple version outdate")
    add_definitions(-DPURPLE_OUTDATE=1)
endif(PURPLE_VERSION VERSION_LESS "2.8")

add_definitions(-DINST_PREFIX="${CMAKE_INSTALL_PREFIX}")

option(NLS "Native language support" On)
if(NLS)
    #find_package(Gettext)
    include(cmake/Gettext.cmake)
endif(NLS)

if(NLS AND GETTEXT_FOUND)
    set(ENABLE_NLS true)
else()
    set(ENABLE_NLS false)
endif()

set(package pidgin-lwqq)
set(localedir ${SHARE_DIR}/locale)
set(datadir ${SHARE_DIR}/lwqq)
set(version "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/src/config.h)

add_subdirectory(src)
add_subdirectory(res)
add_subdirectory(po)

message( "===============pidgin-lwqq flags===============")
message(STATUS "Native Language Support : ${ENABLE_NLS}")
message(STATUS "Install Path            : ${LIB_INSTALL_DIR}")
message( "===============================================")

option(UOA "ubuntu online account support" Off)
if(UOA)
    pkg_check_modules(AP REQUIRED account-plugin)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    EXEC_PROGRAM("pkg-config --variable=provider_plugindir account-plugin 2>/dev/null"
        OUTPUT_VARIABLE PLUGIN_INSTALL_DIR)
    #set the rpath for account-plugin.
    #since it works so we don't care about detail.
    SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 
    SET(CMAKE_INSTALL_RPATH ${PLUGIN_INSTALL_DIR})
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    add_subdirectory(ubuntu-online-account)
endif(UOA)

if(WIN32)
option(INST_ZIP "Make Zip Archive" Off)
if(INST_ZIP)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
    set(CPACK_PACKAGE_NAME "pidgin-lwqq")
    include(CPACK)
    include(InstallRequiredSystemLibraries)
endif(INST_ZIP)
endif()
