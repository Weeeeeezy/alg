# Try to find Wt libs and includes:
# Once done this will define
#  Wt_FOUND
#  Wt_INCLUDE_DIRS
#  Wt_LIBRARIES

find_path(Wt_INCLUDE_DIR Wt/WWidget.h)
find_library(Wt_LIBRARY NAMES wt)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set Wt_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Wt DEFAULT_MSG
                                  Wt_LIBRARY Wt_INCLUDE_DIR)

mark_as_advanced(Wt_INCLUDE_DIR Wt_LIBRARY)

set(Wt_LIBRARIES    ${Wt_LIBRARY})
set(Wt_INCLUDE_DIRS ${Wt_INCLUDE_DIR})
