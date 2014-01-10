set(SRC_LIST
    trex.c
    webqq.c
    translate.c
    qq_types.c
    ft.c
    cgroup.c
    win.c
    )

add_definitions(-Wall)

add_library(webqq MODULE
    ${SRC_LIST}
    )

include_directories(
    ${LIBPURPLE_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/liblwqq
    ../liblwqq/lib
    )

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DUSE_DEBUG=1")
if(WIN32)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static --enable-auto-import" 
    )
endif()

target_link_libraries(webqq
    ${LIBPURPLE_LIBRARIES}
    ${GLIB2_LIBRARIES}
	lwqq-static
    -static-libgcc
    )
if(NLS AND WIN32)
    target_link_libraries(webqq intl iconv)
endif()

INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS 
        ${MinGW_Path}/libintl-8.dll 
        ${MinGW_Path}/libiconv-2.dll
        ${MinGW_Path}/pthreadGC2.dll
    )
    install(PROGRAMS 
        ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} 
        DESTINATION "${LIB_INSTALL_DIR}/../" 
        COMPONENT System)
    install(FILES 
    ### ${SQLITE_LIBRARIES} ### pidgin has sqlite already
    ${WIN32_LIBS_DIR}/libcurl/bin/libcurl.dll
    ${WIN32_LIBS_DIR}/libcurl/bin/libidn-11.dll
    ${WIN32_LIBS_DIR}/zlib/bin/zlib1.dll
    ${WIN32_LIBS_DIR}/js-1.8.5-mingw32/bin/libnspr4.dll
    ${WIN32_LIBS_DIR}/js-1.8.5-mingw32/bin/libplc4.dll
    ${WIN32_LIBS_DIR}/js-1.8.5-mingw32/bin/libplds4.dll
    ${WIN32_LIBS_DIR}/single-dlls/libeay32.dll
    ${WIN32_LIBS_DIR}/single-dlls/ssleay32.dll
    
    DESTINATION "${LIB_INSTALL_DIR}/../")
endif()
