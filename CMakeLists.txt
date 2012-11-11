project(libwebqq)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr")
endif(NOT CMAKE_INSTALL_PREFIX)
if(NOT PURPLE_MAJOR_VERSION)
  set(PURPLE_MAJOR_VERSION "2")
endif(NOT PURPLE_MAJOR_VERSION)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PURPLE REQUIRED  purple>=2.8.0)
pkg_check_modules(GLIB REQUIRED  glib-2.0)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(GTK gtk+-3.0)
#recommand libcurl>=7.22.0


#SET(PURPLE_LIB_INSTALL_DIR
#  "${CMAKE_INSTALL_PREFIX}/lib/purple-${PURPLE_MAJOR_VERSION}")

EXEC_PROGRAM("pkg-config --variable=plugindir purple 2>/dev/null"
    OUTPUT_VARIABLE LIB_INSTALL_DIR)
EXEC_PROGRAM("pkg-config --variable=datadir purple 2>/dev/null"
    OUTPUT_VARIABLE PURPLE_DATADIR)
EXEC_PROGRAM("pkg-config --variable=provider_plugindir account-plugin 2>/dev/null"
    OUTPUT_VARIABLE PLUGIN_INSTALL_DIR)

message("would install to: ${LIB_INSTALL_DIR}")
message("")

add_definitions(-DINST_PREFIX="${CMAKE_INSTALL_PREFIX}")

option(NLS "Native language support" On)
if(NLS)
  find_package(Gettext)
endif(NLS)

#set the rpath for account-plugin.
#since it works so we don't care about detail.
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 
SET(CMAKE_INSTALL_RPATH ${PLUGIN_INSTALL_DIR})
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


add_subdirectory(src)
add_subdirectory(res)
add_subdirectory(ubuntu-online-account)
