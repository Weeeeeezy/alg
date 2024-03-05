# Try to find RapidJSON includes (this is a Headers-only library):
# Once done this will define
#  RapidJSON_FOUND
#  RapidJSON_INCLUDE_DIRS

find_path(RapidJSON_INCLUDE_DIR rapidjson/rapidjson.h)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set RapidJSON_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(RapidJSON DEFAULT_MSG
                                  "" RapidJSON_INCLUDE_DIR)

mark_as_advanced(RapidJSON_INCLUDE_DIR RapidJSON_LIBRARY)

set(RapidJSON_INCLUDE_DIRS ${RapidJSON_INCLUDE_DIR})
