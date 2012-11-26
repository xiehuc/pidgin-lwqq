install(DIRECTORY "qqfaces/"
    DESTINATION "${PURPLE_DATADIR}/pixmaps/pidgin/emotes/webqq")
install(DIRECTORY "qqfaces_static/"
    DESTINATION "${PURPLE_DATADIR}/pixmaps/pidgin/emotes/webqq_static")
foreach(size "16" "22" "48")
    install(FILES "${size}.png" DESTINATION "${PURPLE_DATADIR}/pixmaps/pidgin/protocols/${size}" RENAME "webqq.png")
    install(FILES "${size}.png" DESTINATION "/usr/share/empathy/icons/hicolor/${size}x${size}/apps" RENAME "im-webqq.png")
endforeach(size)
foreach(size "32" "48")
    install(FILES "hi-${size}.png" DESTINATION "/usr/share/icons/hicolor/${size}x${size}/apps" RENAME "webqq.png")
endforeach(size)
install(FILES "scalable.png" DESTINATION "${PURPLE_DATADIR}/pixmaps/pidgin/protocols/scalable.png" RENAME "webqq.png")
install(FILES "scalable.png" DESTINATION "/usr/share/empathy/icons/hicolor/scalable/apps" RENAME "webqq.png")
install(FILES "hi-scalable.png" DESTINATION "/usr/share/icons/hicolor/scalable/apps" RENAME "webqq.png")
