# -*- cmake -*-

# - Find ev library (libev)
# Find the ev includes and library
# This module defines
#  EV_INCLUDE_DIR, where to find db.h, etc.
#  EV_LIBRARIES, the libraries needed to use libev.
#  EV_FOUND, If false, do not try to use libev.
# also defined, but not for general use are
#  EV_LIBRARY, where to find the libev library.

FIND_PATH(EV_INCLUDE_DIR ev.h
/usr/include
/usr/include/libev
/usr/local/include
/usr/local/include/libev
)

SET(EV_NAMES ${EV_NAMES} ev)
FIND_LIBRARY(EV_LIBRARY
  NAMES ${EV_NAMES}
  PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64
  )

IF (EV_LIBRARY AND EV_INCLUDE_DIR)
    SET(EV_LIBRARIES ${EV_LIBRARY})
    SET(EV_FOUND "YES")
ELSE (EV_LIBRARY AND EV_INCLUDE_DIR)
  SET(EV_FOUND "NO")
ENDIF (EV_LIBRARY AND EV_INCLUDE_DIR)


IF (EV_FOUND)
   IF (NOT EV_FIND_QUIETLY)
      MESSAGE(STATUS "Found libev: ${EV_LIBRARIES}")
   ENDIF (NOT EV_FIND_QUIETLY)
ELSE (EV_FOUND)
   IF (EV_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find libev library")
   ENDIF (EV_FIND_REQUIRED)
ENDIF (EV_FOUND)

# Deprecated declarations.
SET (NATIVE_EV_INCLUDE_PATH ${EV_INCLUDE_DIR} )
GET_FILENAME_COMPONENT (NATIVE_EV_LIB_PATH ${EV_LIBRARY} PATH)

SET (EV_LIBRARIES ${EV_LIBRARY})
SET (EV_INCLUDE_DIRS ${EV_INCLUDE_DIR})

MARK_AS_ADVANCED(
    EV_LIBRARIES
    EV_INCLUDE_DIRS
  )
