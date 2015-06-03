set(SRC_LIST
    trex.c
    webqq.c
    translate.c
    qq_types.c
    ft.c
    cgroup.c
    win.c
    login.c
    )

add_definitions(-Wall)

include_directories(
    ${LIBPURPLE_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
	 ${LWQQ_INCLUDE_DIRS}
    )

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")
if(WIN32)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++" )
endif()

add_library(webqq MODULE
    ${SRC_LIST}
    )

target_link_libraries(webqq
    ${LIBPURPLE_LIBRARIES}
    ${GLIB2_LIBRARIES}
	${LWQQ_LIBRARIES}
    )
if(NLS AND WIN32)
    target_link_libraries(webqq intl iconv)
endif()
if(WIN32)
    target_link_libraries(webqq "-Wl,-Bdynamic -lpthread")
endif()

install(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})

if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS 
        ${MinGW_Path}/libgcc_s_dw2-1.dll
    )

    install(PROGRAMS 
        ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} 
        DESTINATION "${LIB_INSTALL_DIR}/../" 
        COMPONENT System)

    install(FILES 
    ### ${SQLITE_LIBRARIES} ### pidgin has sqlite already
    ${LWQQ_LIBRARIES}
    ${WIN32_LIBS_DIR}/libcurl/bin/libcurl.dll
    ${WIN32_LIBS_DIR}/libcurl/bin/libidn-11.dll
    ${WIN32_LIBS_DIR}/zlib/bin/zlib1.dll
    ${WIN32_LIBS_DIR}/single-dlls/libeay32.dll
    ${WIN32_LIBS_DIR}/single-dlls/ssleay32.dll
    ${WIN32_LIBS_DIR}/single-dlls/pthreadGC2.dll

    #${WIN32_LIBS_DIR}/js-1.8.5-mingw32/bin/libnspr4.dll #pidgin provides these dlls
    #${WIN32_LIBS_DIR}/js-1.8.5-mingw32/bin/libplc4.dll
    #${WIN32_LIBS_DIR}/js-1.8.5-mingw32/bin/libplds4.dll
    #${MinGW_Path}/pthreadGC2.dll #would introduce libgcc_sw**.dll which makes a static link invalid

    DESTINATION "${LIB_INSTALL_DIR}/../")
endif()
