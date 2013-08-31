# - Find mozjs
# Find the SpiderMonkey library
#
# This module defines
# MOZJS_LIBRARY
# MOZJS_FOUND, if false, do not try to link to nvtt
# MOZJS_INCLUDE_DIR, where to find the headers
#

FIND_PATH(MOZJS_INCLUDE_DIR jsapi.h
  PATHS
  /usr/local
  /usr
  $ENV{MOZJS_DIR}
  PATH_SUFFIXES include include-unix
)

FIND_LIBRARY(MOZJS_LIBRARY
  NAMES mozjs185 
  PATHS
  /usr/local
  /usr
  $ENV{MOZJS_DIR}
  PATH_SUFFIXES lib64 lib lib/shared lib/static lib64/static
)

FIND_LIBRARY(NSPR_LIBRARY
  NAMES nspr4
  PATHS
  /usr/local
  /usr
  $ENV{MOZJS_DIR}
  PATH_SUFFIXES lib64 lib lib/shared lib/static lib64/static
)

FIND_LIBRARY(PLC_LIBRARY
  NAMES plc4
  PATHS
  /usr/local
  /usr
  $ENV{MOZJS_DIR}
  PATH_SUFFIXES lib64 lib lib/shared lib/static lib64/static
)

FIND_LIBRARY(PLDS_LIBRARY
  NAMES plds4
  PATHS
  /usr/local
  /usr
  $ENV{MOZJS_DIR}
  PATH_SUFFIXES lib64 lib lib/shared lib/static lib64/static
)

SET(MOZJS_FOUND "NO")
IF(MOZJS_LIBRARY AND MOZJS_INCLUDE_DIR)
  MESSAGE(STATUS "MOZJS found")
  SET(MOZJS_FOUND "YES")
  
  SET (MOZJS_LIBRARIES ${PLC_LIBRARY} ${PLDS_LIBRARY} ${MOZJS_LIBRARY} ${NSPR_LIBRARY} stdc++ winmm)
  SET (MOZJS_INCLUDE_DIRS ${MOZJS_INCLUDE_DIR})
  MARK_AS_ADVANCED(MOZJS_LIBRARIES    MOZJS_INCLUDE_DIRS)
ENDIF(MOZJS_LIBRARY AND MOZJS_INCLUDE_DIR)