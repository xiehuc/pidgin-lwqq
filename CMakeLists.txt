project(libwebqq)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

find_package(PkgConfig REQUIRED)
pkg_check_modules(PURPLE REQUIRED  purple)
pkg_check_modules(GLIB REQUIRED  glib-2.0)
pkg_check_modules(CURL REQUIRED libcurl)

if(NOT PURPLE_MAJOR_VERSION)
  set(PURPLE_MAJOR_VERSION "2")
endif(NOT PURPLE_MAJOR_VERSION)

SET(PURPLE_LIB_INSTALL_DIR
  "${CMAKE_INSTALL_PREFIX}/lib/purple-${PURPLE_MAJOR_VERSION}")

add_definitions(-DINST_PREFIX="${CMAKE_INSTALL_PREFIX}")

option(NLS "Native language support" On)
if(NLS)
  find_package(Gettext)
endif(NLS)

add_subdirectory(src)
add_subdirectory(res)
