# Try to find P2CGate libs and includes:
# Once done this will define
#  P2CGate_FOUND
#  P2CGate_INCLUDE_DIRS
#  P2CGate_LIBRARIES

find_path(P2CGate_INCLUDE_DIR       cgate.h
          PATHS ${ENV_PREFIX}/P2CGate/Curr/include)
find_library(P2CGate_LIBRARY  NAMES cgate
          PATHS ${ENV_PREFIX}/P2CGate/Curr/lib)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set P2CGate_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(P2CGate DEFAULT_MSG
                                  P2CGate_LIBRARY P2CGate_INCLUDE_DIR)

mark_as_advanced(P2CGate_INCLUDE_DIR P2CGate_LIBRARY)

set(P2CGate_LIBRARIES    ${P2CGate_LIBRARY})
set(P2CGate_INCLUDE_DIRS ${P2CGate_INCLUDE_DIR})
