# Try to find HDF5_CPP libs and includes:
# Once done this will define
#  HDF5_CPP_FOUND
#  HDF5_CPP_INCLUDE_DIRS
#  HDF5_CPP_LIBRARIES

find_path(HDF5_CPP_INCLUDE_DIR hdf5.h)
find_library(HDF5_CPP_LIBRARY NAMES hdf5_cpp)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set HDF5_CPP_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(HDF5_CPP DEFAULT_MSG
                                  HDF5_CPP_LIBRARY HDF5_CPP_INCLUDE_DIR)

mark_as_advanced(HDF5_CPP_INCLUDE_DIR HDF5_CPP_LIBRARY)

set(HDF5_CPP_LIBRARIES    ${HDF5_CPP_LIBRARY})
set(HDF5_CPP_INCLUDE_DIRS ${HDF5_CPP_INCLUDE_DIR})
