# Try to find RDKafka libs and includes:
# Once done this will define
#  RDKafka_FOUND
#  RDKafka_INCLUDE_DIRS
#  RDKafka_LIBRARIES

find_path(RDKafka_INCLUDE_DIR librdkafka/rdkafka.h)
find_library(RDKafka_LIBRARY NAMES rdkafka)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set RDKafka_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(RDKafka DEFAULT_MSG
                                  RDKafka_LIBRARY RDKafka_INCLUDE_DIR)

mark_as_advanced(RDKafka_INCLUDE_DIR RDKafka_LIBRARY)

set(RDKafka_LIBRARIES    ${RDKafka_LIBRARY})
set(RDKafka_INCLUDE_DIRS ${RDKafka_INCLUDE_DIR})
