project(libwebqq)
cmake_minimum_required(VERSION 2.6)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_PATCH "f")

set(CMAKE_MODULE_PATH 
    ${CMAKE_SOURCE_DIR}/cmake 
    ${CMAKE_SOURCE_DIR}/liblwqq/cmake
    ${CMAKE_MODULE_PATH}
    )
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr")
endif(NOT CMAKE_INSTALL_PREFIX)

if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBPURPLE REQUIRED  purple)
    pkg_check_modules(GLIB REQUIRED  glib-2.0)
    #recommand libcurl>=7.22.0
    exec_program("pkg-config --variable=plugindir purple 2>/dev/null"
        OUTPUT_VARIABLE LIB_INSTALL_DIR)
    exec_program("pkg-config --variable=datadir purple 2>/dev/null"
        OUTPUT_VARIABLE PURPLE_DATADIR)
    set(PURPLE_DATADIR "${PURPLE_DATADIR}/")
    exec_program("pkg-config --modversion purple 2>/dev/null"
        OUTPUT_VARIABLE PURPLE_VERSION)
endif(UNIX)

if(WIN32)
    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/../pidgin-2.10.7/libpurple" )
    find_package(LibPurple REQUIRED)
    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/glib_2.28.8-1_win32/include" 
        "${WIN32_LIBS_DIR}/glib_2.28.8-1_win32/lib")
    find_package(GLIB2 REQUIRED)
    set(LIB_INSTALL_DIR "plugins")
    set(PURPLE_DATADIR  "")
    set(CMAKE_PREFIX_PATH 
    "${WIN32_LIBS_DIR}/libcurl" 
        "${WIN32_LIBS_DIR}/libcurl/include" 
        "${WIN32_LIBS_DIR}/libcurl/lib"
        )
    find_package(CURL REQUIRED)
    set(CMAKE_PREFIX_PATH "${WIN32_LIBS_DIR}/sqlite3")
    find_package(Sqlite3 REQUIRED)
    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/zlib/lib"
        "${WIN32_LIBS_DIR}/zlib/include" 
        "${WIN32_LIBS_DIR}/zlib/lib"
        )
    find_library(ZLIB NAMES zlib1 zlib PATHS
  "$ENV{LIB_DIR}/lib" CMAKE_PREFIX_PATH
  NO_DEFAULT_PATH)
    MESSAGE("${ZLIB}")
endif(WIN32)

add_definitions(-Wall)

message("libpurple version:${PURPLE_VERSION}")
message("would install to: ${LIB_INSTALL_DIR}")
message("")

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
    message(STATUS "Native Language Support: Yes")
else(NLS AND GETTEXT_FOUND)
    set(ENABLE_NLS false)
    message(STATUS "Native Language Support: No")
endif(NLS AND GETTEXT_FOUND)

set(package pidgin-lwqq)
set(localedir ${CMAKE_INSTALL_PREFIX}/share/locale)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/src/config.h)

set(WITH_LIBEV Off CACHE BOOL "Use Libev To Provide Async ")
set(WITH_LIBUV Off CACHE BOOL "Use Libuv To Provide Async ")
set(STATIC_LIBRARY On CACHE BOOL "Compile Static Library")
set(LWQQ_INSTALL Off CACHE BOOL "Install Lwqq Library")

add_subdirectory(liblwqq)
add_subdirectory(src)
add_subdirectory(res)
add_subdirectory(po)

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
    set(CPACK_PACKAGE_NAME "pidgin-lwqq")
    include(CPACK)
endif(INST_ZIP)
endif()
