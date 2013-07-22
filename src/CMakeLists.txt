set(SRC_LIST
    trex.c
    webqq.c
    translate.c
    qq_types.c
    ft.c
    cgroup.c
    )

add_definitions(-Wall)

add_library(webqq MODULE
    ${SRC_LIST}
    )

include_directories(
    ${LIBPURPLE_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ../liblwqq/lib
    )

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DUSE_DEBUG=1 -Wno-unused-function")

target_link_libraries(webqq
    ${LIBPURPLE_LIBRARIES}
    ${GLIB2_LIBRARIES}
    pthread
	lwqq
    )
if(NLS AND WIN32)
    target_link_libraries(webqq intl)
endif()

INSTALL(TARGETS webqq DESTINATION ${LIB_INSTALL_DIR})
if(WIN32)
install(FILES 
    ${SQLITE_LIBRARIES} 
    ${WIN32_LIBS_DIR}/libcurl/bin/libcurl.dll
    ${WIN32_LIBS_DIR}/libcurl/bin/libidn-11.dll
    ${WIN32_LIBS_DIR}/zlib/zlib1.dll
    DESTINATION "${LIB_INSTALL_DIR}/../")
endif()
