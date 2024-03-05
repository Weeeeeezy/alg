# Try to find NGHTTP2 libs and includes:
# Once done this will define:
#  NGHTTP2_FOUND
#  NGHTTP2_INCLUDE_DIRS
#  NGHTTP2_LIBRARIES

find_path   (NGHTTP2_INCLUDE_DIR   nghttp2/nghttp2.h)
find_library(NGHTTP2_LIBRARY NAMES nghttp2)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set NGHTTP2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(NGHTTP2 DEFAULT_MSG
                                  NGHTTP2_LIBRARY NGHTTP2_INCLUDE_DIR)

mark_as_advanced(NGHTTP2_INCLUDE_DIR NGHTTP2_LIBRARY)

set(NGHTTP2_LIBRARIES    ${NGHTTP2_LIBRARY})
set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
