project(libwebqq)
cmake_minimum_required(VERSION 2.6)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "2")
set(VERSION_PATCH "c")

set(CMAKE_MODULE_PATH 
    ${CMAKE_SOURCE_DIR}/cmake 
    ${CMAKE_SOURCE_DIR}/liblwqq/cmake
    ${CMAKE_MODULE_PATH}
    )

if(UNIX)
    set(SHARE_DIR "${CMAKE_INSTALL_PREFIX}/share")

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBPURPLE REQUIRED  purple)
    pkg_check_modules(GLIB REQUIRED  glib-2.0)
    pkg_check_modules(MOZJS mozjs185)
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
    set(LIB_INSTALL_DIR "plugins")
    set(PURPLE_DATADIR  "")
    set(EV_FOUND Off)
    set(SHARE_DIR "share")

    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/../pidgin-2.10.7/libpurple" )
    find_package(LibPurple REQUIRED)

    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/glib_2.28.8-1_win32/include" 
        "${WIN32_LIBS_DIR}/glib_2.28.8-1_win32/lib"
        "${WIN32_LIBS_DIR}/gtk_2_0-2.14/include/glib-2.0"
        "${WIN32_LIBS_DIR}/gtk_2_0-2.14/lib"
        )
    find_package(GLIB2 REQUIRED)
    
    set(CMAKE_PREFIX_PATH "${WIN32_LIBS_DIR}/sqlite3")
    find_package(Sqlite3 REQUIRED)

    set(CMAKE_PREFIX_PATH 
        "${WIN32_LIBS_DIR}/js-1.8.5-mingw32/include/js" 
        "${WIN32_LIBS_DIR}/js-1.8.5-mingw32/lib"
        )
    find_package(MozJS REQUIRED)
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

option(WITH_MOZJS "Use Mozjs To Provide hash.js support" ${MOZJS_FOUND})
if(WITH_MOZJS AND NOT MOZJS_FOUND)
	message(FATAL_ERROR "Reuired mozjs185 package not found")
endif()

set(WITH_LIBUV Off CACHE BOOL "Use Libuv To Provide Async ")
set(STATIC_LIBRARY On CACHE BOOL "Compile Static Library")
set(LWQQ_INSTALL Off CACHE BOOL "Install Lwqq Library")

set(package pidgin-lwqq)
set(localedir ${SHARE_DIR}/locale)
set(datadir ${SHARE_DIR}/lwqq)
set(version "${VERSION_MAJOR}.${VERSION_MINOR}${VERSION_PATCH}")
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/src/config.h)

add_subdirectory(liblwqq)
add_subdirectory(src)
add_subdirectory(res)
add_subdirectory(po)

message( "===============pidgin-lwqq flags===============")
message(STATUS "With Mozjs (Option)     : ${WITH_MOZJS}")
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
endif(INST_ZIP)
endif()
