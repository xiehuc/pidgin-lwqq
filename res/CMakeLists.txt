install(FILES "hash.js" DESTINATION "${datadir}")
install(FILES "smiley.txt" DESTINATION "${datadir}")

#install qq faces
install(DIRECTORY "qqfaces/"
    DESTINATION "${PURPLE_DATADIR}pixmaps/pidgin/emotes/webqq"
    )
install(DIRECTORY "qqfaces_static/"
    DESTINATION "${PURPLE_DATADIR}pixmaps/pidgin/emotes/webqq_static"
    )

#install pidgin icons
foreach(size "16" "22" "48")
    install(FILES "${size}.png" 
    	DESTINATION "${PURPLE_DATADIR}pixmaps/pidgin/protocols/${size}" 
    	RENAME "webqq.png"
    	)
    #install(FILES "${size}.png" 
    #	DESTINATION "/usr/share/empathy/icons/hicolor/${size}x${size}/apps" 
    #	RENAME "im-webqq.png"
    #)
endforeach(size)
install(FILES "scalable.svg" 
    DESTINATION "${PURPLE_DATADIR}pixmaps/pidgin/protocols/scalable.png" 
    RENAME "webqq.svg"
    )

# install system icon
if(NOT WIN32)
foreach(size "32" "48")
    install(FILES "hi-${size}.png" 
    	DESTINATION "/usr/share/icons/hicolor/${size}x${size}/apps" 
    	RENAME "webqq.png"
    	)
endforeach(size)
install(FILES "hi-scalable.svg" 
    DESTINATION "/usr/share/icons/hicolor/scalable/apps" 
    RENAME "webqq.svg"
    )
endif()



#install(FILES "scalable.svg" 
#	DESTINATION "/usr/share/empathy/icons/hicolor/scalable/apps" 
#	RENAME "im-webqq.svg"
#	)
