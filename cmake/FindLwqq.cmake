# - Try to find LWQQ
# Once done this will define
#
#  LWQQ_FOUND - system has LWQQ
#  LWQQ_INCLUDE_DIRS - the LWQQ include directory
#  LWQQ_LIBRARIES - Link these to use LWQQ
#  LWQQ_DEFINITIONS - Compiler switches required for using LWQQ
#
#  Copyright (c) 2008 Tarrisse Laurent <laurent@mbdsys.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LWQQ_LIBRARIES AND LWQQ_INCLUDE_DIRS)
  # in cache already
  set(LWQQ_FOUND TRUE)
else (LWQQ_LIBRARIES AND LWQQ_INCLUDE_DIRS)
  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  #  include(UsePkgConfig)

  #  pkgconfig(LWQQ _LWQQIncDir _LWQQLinkDir _LWQQLinkFlags _LWQQCflags)

    set(LWQQ_DEFINITIONS ${_LWQQCflags})

    find_path(LWQQ_INCLUDE_DIR
        NAMES
            lwqq.h
        PATHS
            ${_LWQQIncDir}
            /usr/local/include
    )

    find_path(LWQQ_CONFIG_INCLUDE_DIR
        NAMES
            lwqq-config.h
        PATHS
            ${_LWQQIncDir}
            /usr/local/include
    )

    find_library(LWQQ_LIBRARY
        NAMES
            liblwqq.dll.a
        PATHS
            ${_LWQQLinkDir}
            /usr/local/lib/
    )

    set(LWQQ_INCLUDE_DIRS
        ${LWQQ_INCLUDE_DIR}
        ${LWQQ_CONFIG_INCLUDE_DIR}
    )

    set(LWQQ_LIBRARIES
        ${LWQQ_LIBRARY}
    )

  if (LWQQ_INCLUDE_DIRS AND LWQQ_LIBRARIES)
    set(LWQQ_FOUND TRUE)
  endif (LWQQ_INCLUDE_DIRS AND LWQQ_LIBRARIES)

  if (LWQQ_FOUND)
    if (NOT LWQQ_FIND_QUIETLY)
      message(STATUS "Found lwqq: ${LWQQ_LIBRARIES}")
    endif (NOT LWQQ_FIND_QUIETLY)
  else (LWQQ_FOUND)
    if (LWQQ_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find lwqq")
    endif (LWQQ_FIND_REQUIRED)
  endif (LWQQ_FOUND)

  # show the LWQQ_INCLUDE_DIRS and LWQQ_LIBRARIES variables only in the advanced view
  mark_as_advanced(LWQQ_INCLUDE_DIRS LWQQ_LIBRARIES)

endif (LWQQ_LIBRARIES AND LWQQ_INCLUDE_DIRS)
