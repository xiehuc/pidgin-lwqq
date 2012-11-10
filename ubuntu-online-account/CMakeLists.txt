add_library(plugin MODULE
    account-plugin.c
    )
set_target_properties(plugin PROPERTIES OUTPUT_NAME webqq)
include_directories(
    ${ACCOUNT_INCLUDE_DIRS}
    )
find_library(EMPATHY_LIBRARIES 
    empathy-uoa-account-plugin 
    empathy
    empathy-gtk
    /usr/lib/empathy)
target_link_libraries(plugin
    ${EMPATHY_LIBRARIES}
    ${ACCOUNT_LIBRARIES}
    ${MCP_LIBRARIES}
    ${SIGNON_LIBRARIES}
    )
INSTALL(TARGETS plugin DESTINATION ${PLUGIN_INSTALL_DIR})
INSTALL(FILES webqq.provider DESTINATION /usr/share/accounts/providers)
INSTALL(FILES webqq-im.service DESTINATION /usr/share/accounts/services)
