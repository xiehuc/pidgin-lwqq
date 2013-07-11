# - Try to find libpurple
# Once done this will define
#
#  LIBPURPLE_FOUND - system has libpurple
#  LIBPURPLE_INCLUDE_DIRS - the libpurple include directory
#  LIBPURPLE_LIBRARIES - Link these to use libpurple
#  LIBPURPLE_DEFINITIONS - Compiler switches required for using libpurple
#
#  Copyright (c) 2008 Tarrisse Laurent <laurent@mbdsys.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LIBPURPLE_LIBRARIES AND LIBPURPLE_INCLUDE_DIRS)
  # in cache already
  set(LIBPURPLE_FOUND TRUE)
else (LIBPURPLE_LIBRARIES AND LIBPURPLE_INCLUDE_DIRS)
  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
    include(UsePkgConfig)

    pkgconfig(libpurple _LIBPURPLEIncDir _LIBPURPLELinkDir _LIBPURPLELinkFlags _LIBPURPLECflags)

    set(LIBPURPLE_DEFINITIONS ${_LIBPURPLECflags})

    find_path(LIBPURPLE_INCLUDE_DIR
        NAMES
            libpurple/purple.h
            account.h
        PATHS
            ${_LIBPURPLEIncDir}
            /usr/local/include
    )

    find_library(LIBPURPLE_LIBRARY
        NAMES
            purple
        PATHS
            ${_LIBPURPLELinkDir}
            /usr/local/lib/
    )

    set(LIBPURPLE_INCLUDE_DIRS
        ${LIBPURPLE_INCLUDE_DIR}
        ${LIBPURPLE_INCLUDE_DIR}/libpurple
    )

    set(LIBPURPLE_LIBRARIES
        ${LIBPURPLE_LIBRARY}
    )

  if (LIBPURPLE_INCLUDE_DIRS AND LIBPURPLE_LIBRARIES)
    set(LIBPURPLE_FOUND TRUE)
  endif (LIBPURPLE_INCLUDE_DIRS AND LIBPURPLE_LIBRARIES)

  if (LIBPURPLE_FOUND)
    if (NOT LIBPURPLE_FIND_QUIETLY)
      message(STATUS "Found libpurple: ${LIBPURPLE_LIBRARIES}")
    endif (NOT LIBPURPLE_FIND_QUIETLY)
  else (LIBPURPLE_FOUND)
    if (LIBPURPLE_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find libpurple")
    endif (LIBPURPLE_FIND_REQUIRED)
  endif (LIBPURPLE_FOUND)

  # show the LIBPURPLE_INCLUDE_DIRS and LIBPURPLE_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBPURPLE_INCLUDE_DIRS LIBPURPLE_LIBRARIES)

endif (LIBPURPLE_LIBRARIES AND LIBPURPLE_INCLUDE_DIRS)