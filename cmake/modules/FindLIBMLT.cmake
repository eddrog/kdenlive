#
# Find the MLT includes and libraries.
#

include(FindPkgConfig)
pkg_check_modules(MLT mlt++)
add_definitions(-DMLT_PREFIX=\\\"\"${MLT_PREFIX}\"\\\")

find_path(LIBMLT_INCLUDE_DIR
  NAMES framework/mlt.h
  PATHS
    ${MLT_INCLUDEDIR}/mlt
    ${MLT_PREFIX}/include/mlt
    /usr/local/include/mlt
    /usr/include/mlt
  NO_DEFAULT_PATH
)

find_library(LIBMLT_LIBRARY
  NAMES mlt
  PATHS
    ${MLT_LIBDIR}
    ${MLT_PREFIX}/lib
    /usr/local/lib
    /usr/lib
  NO_DEFAULT_PATH
)

find_path(LIBMLTPLUS_INCLUDE_DIR
  NAMES mlt++/Mlt.h
  PATHS
    ${MLT_INCLUDEDIR}
    ${MLT_PREFIX}/include
    /usr/local/include
    /usr/include
  NO_DEFAULT_PATH
)

find_library(LIBMLTPLUS_LIBRARY
  NAMES mlt++
  PATHS
    ${MLT_LIBDIR}
    ${MLT_PREFIX}/lib
    /usr/local/lib
    /usr/lib
  NO_DEFAULT_PATH
)

if(LIBMLT_LIBRARY AND LIBMLT_INCLUDE_DIR)
  set(LIBMLT_FOUND 1)
  set(LIBMLT_LIBRARIES ${LIBMLT_LIBRARY})
else(LIBMLT_LIBRARY AND LIBMLT_INCLUDE_DIR)
  set(LIBMLT_FOUND 0)
endif(LIBMLT_LIBRARY AND LIBMLT_INCLUDE_DIR)

if(LIBMLT_FOUND AND LIBMLTPLUS_LIBRARY AND LIBMLTPLUS_INCLUDE_DIR)
  set(LIBMLT_FOUND 1)
  set(LIBMLTPLUS_LIBRARIES ${LIBMLTPLUS_LIBRARY})
else(LIBMLT_FOUND AND LIBMLTPLUS_LIBRARY AND LIBMLTPLUS_INCLUDE_DIR)
  set(LIBMLT_FOUND 0)
endif(LIBMLT_FOUND AND LIBMLTPLUS_LIBRARY AND LIBMLTPLUS_INCLUDE_DIR)

if(LIBMLT_FOUND)
  set(LIBMLT_VERSION ${MLT_VERSION})
  if(NOT LIBMLT_FIND_QUIETLY)
    message(STATUS "Found MLT++: ${LIBMLTPLUS_LIBRARY}")
  endif(NOT LIBMLT_FIND_QUIETLY)
else(LIBMLT_FOUND)
  if(LIBMLT_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find MLT++")
  endif(LIBMLT_FIND_REQUIRED)
endif(LIBMLT_FOUND)
