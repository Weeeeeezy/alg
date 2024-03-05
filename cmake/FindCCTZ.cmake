# Try to find CCTZ libs and includes:
# Once done this will define
#  CCTZ_FOUND
#  CCTZ_INCLUDE_DIRS
#  CCTZ_LIBRARIES

find_path(CCTZ_INCLUDE_DIR cctz/time_zone.h)
find_library(CCTZ_LIBRARY NAMES cctz)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set CCTZ_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(CCTZ DEFAULT_MSG
                                  CCTZ_LIBRARY CCTZ_INCLUDE_DIR)

mark_as_advanced(CCTZ_INCLUDE_DIR CCTZ_LIBRARY)

set(CCTZ_LIBRARIES    ${CCTZ_LIBRARY})
set(CCTZ_INCLUDE_DIRS ${CCTZ_INCLUDE_DIR})
