# Try to find PQXX libs and includes:
# Once done this will define
#  PQXX_FOUND
#  PQXX_INCLUDE_DIRS
#  PQXX_LIBRARIES

find_path(PQXX_INCLUDE_DIR pqxx/pqxx)
find_library(PQXX_LIBRARY NAMES pqxx)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set PQXX_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(PQXX DEFAULT_MSG
                                  PQXX_LIBRARY PQXX_INCLUDE_DIR)

mark_as_advanced(PQXX_INCLUDE_DIR PQXX_LIBRARY)

set(PQXX_LIBRARIES    ${PQXX_LIBRARY})
set(PQXX_INCLUDE_DIRS ${PQXX_INCLUDE_DIR})
