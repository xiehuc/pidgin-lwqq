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
#recommand libcurl>=7.22.0


#SET(PURPLE_LIB_INSTALL_DIR
#  "${CMAKE_INSTALL_PREFIX}/lib/purple-${PURPLE_MAJOR_VERSION}")

EXEC_PROGRAM("pkg-config --variable=libdir purple 2>/dev/null"
			OUTPUT_VARIABLE PURPLE_LIBDIR)
SET(LIB_INSTALL_DIR "${PURPLE_LIBDIR}/purple-2")
EXEC_PROGRAM("pkg-config --variable=datadir purple 2>/dev/null"
			OUTPUT_VARIABLE PURPLE_DATADIR)


#message("prefix:\t\t" ${CMAKE_INSTALL_PREFIX})
#message("libwebqq.so:\t" ${PURPLE_LIB_INSTALL_DIR})
#message("")
#message("!!!note: please ensure libwebqq.so install path correctly.")
#message("in most of situations: it is /usr/lib/purple-2")
#message("if not.")
#message("please use cmake -DCMAKE_INSTALL_PREFIX=<path of purple-2> ..")
#message("")

message("would install to: ${LIB_INSTALL_DIR}")
message("")

add_definitions(-DINST_PREFIX="${CMAKE_INSTALL_PREFIX}")

option(NLS "Native language support" On)
if(NLS)
  find_package(Gettext)
endif(NLS)

add_subdirectory(src)
add_subdirectory(res)
