project(libwebqq)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr")
endif(NOT CMAKE_INSTALL_PREFIX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PURPLE REQUIRED  purple>=2.8.0)
pkg_check_modules(GLIB REQUIRED  glib-2.0)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(SQLITE REQUIRED sqlite3)
#recommand libcurl>=7.22.0

add_definitions(-Wall)

EXEC_PROGRAM("pkg-config --variable=plugindir purple 2>/dev/null"
    OUTPUT_VARIABLE LIB_INSTALL_DIR)
EXEC_PROGRAM("pkg-config --variable=datadir purple 2>/dev/null"
    OUTPUT_VARIABLE PURPLE_DATADIR)

message("would install to: ${LIB_INSTALL_DIR}")
message("")

add_definitions(-DINST_PREFIX="${CMAKE_INSTALL_PREFIX}")

option(NLS "Native language support" On)
if(NLS)
  find_package(Gettext)
endif(NLS)

option(WITH_LIBEV "build in libev for async process" On)

if(WITH_LIBEV)
    find_package(EV REQUIRED)
endif(WITH_LIBEV)

if(VERBOSE)
    add_definitions(-DLWQQ_VERBOSE_LEVEL=${VERBOSE})
else(VERBOSE)
    add_definitions(-DLWQQ_VERBOSE_LEVEL=0)
endif(VERBOSE)

add_subdirectory(src)
add_subdirectory(res)

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
