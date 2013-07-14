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
    pkg_check_modules(CURL REQUIRED libcurl)
    pkg_check_modules(SQLITE REQUIRED sqlite3)
    pkg_check_modules(ZLIB REQUIRED zlib)
    #recommand libcurl>=7.22.0
    find_package(EV REQUIRED)
    exec_program("pkg-config --variable=plugindir purple 2>/dev/null"
        OUTPUT_VARIABLE LIB_INSTALL_DIR)
    exec_program("pkg-config --variable=datadir purple 2>/dev/null"
        OUTPUT_VARIABLE PURPLE_DATADIR)
    exec_program("pkg-config --modversion purple 2>/dev/null"
        OUTPUT_VARIABLE PURPLE_VERSION)
endif(UNIX)

if(WIN32)
    set(CMAKE_PREFIX_PATH "../win32-dev/libcurl-7.18.0-win32-msvc")
    find_package(CURL REQUIRED)
    set(CMAKE_PREFIX_PATH "../win32-dev/sqlite3")
    find_package(Sqlite3 REQUIRED)
    set(CMAKE_PREFIX_PATH "../win32-dev/zlib-1.28/include" "../win32-dev/zlib-1.28/lib")
    find_package(ZLIB REQUIRED)
    set(CMAKE_PREFIX_PATH "../win32-dev/pidgin-2.10.7/libpurple" )
    find_package(LibPurple REQUIRED)
    set(CMAKE_PREFIX_PATH "../win32-dev/glib_2.28.8-1_win32/include" "../win32-dev/glib_2.28.8-1_win32/lib")
    find_package(GLIB2 REQUIRED)
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
    find_package(Gettext)
endif(NLS)

if(NLS AND GETTEXT_FOUND)
    set(ENABLE_NLS true)
    message(STATUS "Native Language Support: Yes")
else(NLS AND GETTEXT_FOUND)
    set(ENABLE_NLS false)
    message(STATUS "Native Language Support: No")
endif(NLS AND GETTEXT_FOUND)

option(SSL "Enable SSL Support " On)

option(WITHOUT_LIBEV "Disable Async " Off)

set(package pidgin-lwqq)
set(localedir ${CMAKE_INSTALL_PREFIX}/share/locale)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/src/config.h)

add_subdirectory(liblwqq)
set(STATIC_LIBRARY On CACHE BOOL "Compile Static Library")
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
