# vim:ts=2:et
#=============================================================================#
#                        "CMakeLists.txt" (Top-Level):                        #
#=============================================================================#
#=============================================================================#
# CMake Settings:                                                             #
#=============================================================================#
# Export flags for convenience when using Vim's YCM plugin:
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)
CMAKE_POLICY(          VERSION 3.14)

IF (NOT UNIX)
  MESSAGE(FATAL_ERROR "Non-UNIX platforms are not supported in MAQUETTE")
ENDIF()

#=============================================================================#
# Project Settings:                                                           #
#=============================================================================#
PROJECT(MAQUETTE CXX C)

SET (CMAKE_EXPORT_COMPILE_COMMANDS ON)
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# Generate identity string for this build:
EXECUTE_PROCESS(
  COMMAND bash -c "cd ${CMAKE_SOURCE_DIR} && git rev-parse HEAD"
  OUTPUT_VARIABLE COMMIT)
STRING(STRIP ${COMMIT} COMMIT_NAME)

EXECUTE_PROCESS(
  COMMAND bash -c "cd ${CMAKE_SOURCE_DIR} && git rev-parse --abbrev-ref HEAD"
  OUTPUT_VARIABLE BRANCH)
STRING(STRIP ${BRANCH} BRANCH_NAME)

EXECUTE_PROCESS(
  COMMAND bash -c "cd ${CMAKE_SOURCE_DIR} && git config --get remote.origin.url"
  OUTPUT_VARIABLE REMOTE)
STRING(STRIP ${REMOTE} REMOTE_NAME)

STRING(TIMESTAMP DATE "%Y-%m-%d %H-%M-%S" UTC)

ADD_COMPILE_DEFINITIONS(REMOTE_NAME="${REMOTE_NAME}"
                        BRANCH_NAME="${BRANCH_NAME}"
                        COMPILE_DATE="${DATE}"
                        COMMIT_NAME="${COMMIT_NAME}")

MESSAGE("Remote\t${REMOTE_NAME}\nBranch\t${BRANCH_NAME}\n\
Commit\t${COMMIT_NAME}\nTime\t${DATE}\n")

# NB: The following vars are normally provided via the command-line params:
#
IF (DEFINED LIB_DIR AND NOT "${LIB_DIR}" STREQUAL "")
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${LIB_DIR}")
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${LIB_DIR}")
ENDIF()

IF (DEFINED BIN_DIR AND NOT "${BIN_DIR}" STREQUAL "")
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}")
ENDIF()

IF (NOT DEFINED TOOL_CHAIN OR "${TOOL_CHAIN}" STREQUAL "")
  SET(TOOL_CHAIN "GCC")
ENDIF()

IF (NOT DEFINED CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
  SET(CMAKE_BUILD_TYPE "RelWithDebInfo")
ENDIF()

IF (NOT DEFINED ENV_PREFIX OR "${ENV_PREFIX}" STREQUAL "")
  SET(ENV_PREFIX "/opt")
ENDIF()

IF (NOT DEFINED CMAKE_INSTALL_PREFIX OR "${CMAKE_INSTALL_PREFIX}" STREQUAL "")
  SET(CMAKE_INSTALL_PREFIX
      "${ENV_PREFIX}/${PROJECT_NAME}-${TOOL_CHAIN}-${CMAKE_BUILD_TYPE}")
ENDIF()

#=============================================================================#
# Check for Dependencies:                                                     #
#=============================================================================#
#-----------------------------------------------------------------------------#
# Mandatory Dependencies: Boost, GNUTLS and NGHTTP2:                          #
#-----------------------------------------------------------------------------#
# Extend the PkgConfig search path (for *.pc files) -- useful for those modules
# which can be found via PkgConfig:
SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${ENV_PREFIX}/lib/pkgconfig")

# Boost (no specific version checks):
FIND_PACKAGE(Boost  REQUIRED)

# GNUTLS:
SET(CMAKE_FIND_LIBRARY_PREFIXES "lib")
SET(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")
FIND_PACKAGE(GnuTLS REQUIRED)

# We need to figure out whether a Static or Shared version of GNUTLS will  be
# used. Normally, a Static one would be found under ${ENV_PREFIX} and then used
# in priority of the generic Shared one, for the sake of extra optimisation.
# But that would require several other libs to be explicitly linked in. So:
#
FIND_LIBRARY(LIBGNUTLS_A   "libgnutls.a"
             PATHS "/usr/lib64" "/usr/lib"
             "${ENV_PREFIX}/lib64" "${ENV_PREFIX}/lib")
FIND_LIBRARY(LIBGNUTLS_SO  "lingnutls.so"
             PATHS "/usr/lib64" "/usr/lib"
             "${ENV_PREFIX}/lib64" "${ENV_PREFIX}/lib")

IF ((NOT "${LIBGNUTLS_A}"  STREQUAL "LIBGNUTLS_A-NOTFOUND") AND
    (    "${LIBGNUTLS_SO}" STREQUAL "LIBGNUTLS_SO-NOTFOUND"))
  SET(GNUTLS_STATIC 1)
  MESSAGE(STATUS "GNUTLS      : Static")
ELSE()
  MESSAGE(STATUS "GNUTLS      : Shared")
ENDIF()

# NGHTTP2 for HTTP2 compressions:
FIND_PACKAGE(NGHTTP2 REQUIRED)

#-----------------------------------------------------------------------------#
# Optional Dependencies: Wt, RapidJSON, RDKafka, PQXX, P2CGate:               #
#-----------------------------------------------------------------------------#
# If the corresp package is found, append its EXT_LIBS (NB: RapidJSON is header-
# only):
SET(EXT_LIBS)

# HDF5 (CXX components).
# FIXME: It is currently marked as "REQUIRED", because the "find" script would
# wronhly set the global link flags otherwise. This script also performs some
# compilations, so remove the left-over garbage:
#
SET(WITH_HDF5 0)
FIND_PACKAGE(HDF5_CPP)
IF (HDF5_CPP_FOUND)
  SET(WITH_HDF5 1)
  LIST(APPEND EXT_LIBS "hdf5_cpp" "hdf5")
ENDIF()

# Wt: For monitoring tools:
SET(WITH_WT 0)
FIND_PACKAGE(Wt)
IF (Wt_FOUND)
  SET (WITH_WT 1)
  LIST(APPEND EXT_LIBS "wt" "wthttp")
ENDIF()

# RapidJSON: Not critical, as we usually perform JSON parsing manually:
SET(WITH_RAPIDJSON 0)
FIND_PACKAGE(RapidJSON)
IF (RapidJSON_FOUND)
  SET(WITH_RAPIDJSON 1)
ENDIF()

# RDKafka: Currently only for LATOKEN stuff:
SET(WITH_RDKAFKA 0)
FIND_PACKAGE(RDKafka)
IF (RDKafka_FOUND)
  SET (WITH_RDKAFKA 1)
  LIST(APPEND EXT_LIBS "rdkafka")
ENDIF()

# PQXX: Currently for RiskMgr PgSQL back-end only:
SET(WITH_PQXX   0)
FIND_PACKAGE(PQXX)
IF (PQXX_FOUND)
  SET (WITH_PQXX 1)
  LIST(APPEND EXT_LIBS "pqxx")
ENDIF()  

# PlazaII/CGate: only for MOEX/FORTS/P2CGate connectors:
SET(WITH_P2CGATE 0)
FIND_PACKAGE(P2CGate)
IF (P2CGATE_FOUND)
  SET (WITH_P2CGATE 1)
  LIST(APPEND EXT_LIBS "cgate")
  INCLUDE_DIRECTORIES("${ENV_PREFIX}/P2CGate/Curr/include")
  LINK_DIRECTORIES   ("${ENV_PREFIX}/P2CGate/Curr/lib")
ENDIF()

# OpenMP: We need it for run-time support libraries:
SET(WITH_OPENMP 0)
FIND_PACKAGE(OpenMP)
IF (OpenMP_FOUND)
  SET (WITH_OPENMP 1)
  LIST(APPEND EXT_LIBS "omp")
ENDIF()

# VMime for sending emails -- normally installed under ${ENV_PREFIX} and
# contains configs which can be used by CMake:
SET(WITH_VMIME 0)
FIND_PACKAGE(VMime)
IF (VMime_FOUND)
  SET (WITH_VMIME 1)   
  LIST(APPEND EXT_LIBS "vmime")
ENDIF()

# CURL for HTTP/1.1:
SET(WITH_CURL 0)
FIND_PACKAGE(CURL)
IF (CURL_FOUND)
  SET (WITH_CURL 1)
  LIST(APPEND EXT_LIBS "curl")
ENDIF()

# XXX: CCTZ: Will later be replaced by a C++20 standard library:
SET(WITH_CCTZ 0)
FIND_PACKAGE(CCTZ)
IF (CCTZ_FOUND)
  SET (WITH_CCTZ 1)
  LIST(APPEND EXT_LIBS "cctz")
ENDIF()

#-----------------------------------------------------------------------------#
# Other options:                                                              #
#-----------------------------------------------------------------------------#
# Crypto-Only mode (ignoring connectors to "Fiat" exchanges):
IF (NOT DEFINED CRYPTO_ONLY OR "${CRYPTO_ONLY}" STREQUAL "OFF")
  MESSAGE(STATUS "Crypto-Only : OFF")
  SET(CRYPTO_ONLY 0)
ELSE()
  MESSAGE(STATUS "Crypto-Only : ON")
  SET(CRYPTO_ONLY 1)
ENDIF()

OPTION(USE_CCACHE "Use ccache" OFF)
IF (USE_CCACHE)
  # Configure CCache if available
  FIND_PROGRAM(CCACHE_FOUND ccache)
  IF (CCACHE_FOUND)
    SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  ENDIF()
ENDIF()

#=============================================================================#
# Compiler Settings:                                                          #
#=============================================================================#
MESSAGE(STATUS "Configuring for the ${TOOL_CHAIN} ToolChain...")

#-----------------------------------------------------------------------------#
IF("${TOOL_CHAIN}" STREQUAL "GCC")
#-----------------------------------------------------------------------------#
  # Optimisation and debugging flags:
  IF ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    # NB: Do NOT use:
    # "-ffast-math" option -- it will result in incorrect treatment of "NaN"s!
    # -flto"               -- it intereacts in a strange way with header-only
    #                         code!
    SET(CFLAGS "-O3 -DNDEBUG -funroll-loops -fomit-frame-pointer")
  ELSEIF ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    SET(CFLAGS "-O2 -g -funroll-loops")
  ELSEIF ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    SET(CFLAGS "-O0 -g")
  ELSE()
    MESSAGE(WARNING "UnSupported BuildType: ${CMAKE_BUILD_TYPE}")
  ENDIF()

  # Other flags (common C++/C, and C++-only):
  SET(CFLAGS   "${CFLAGS} -march=native -mtune=native \
                -Wall                   -Wextra          -Wcast-qual \
                -Winit-self             -Wwrite-strings  -Wmissing-braces \
                -Wdisabled-optimization -Wconversion  \
                -Wno-stringop-truncation")

  SET(CXXFLAGS "${CFLAGS} -std=gnu++20 \
                -Wctor-dtor-privacy  -Wnon-virtual-dtor -Woverloaded-virtual \
                -Wno-class-memaccess -Wno-double-promotion")
#-----------------------------------------------------------------------------#
ELSEIF("${TOOL_CHAIN}" STREQUAL "CLang")
#-----------------------------------------------------------------------------#
  # Optimisation and debugging flags:
  IF ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    SET(CFLAGS "-O3 -DNDEBUG -funroll-loops -fomit-frame-pointer")
  ELSEIF ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    SET(CFLAGS "-O2 -g -funroll-loops")
  ELSEIF ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug"  )
    SET(CFLAGS "-O0 -g")
  ELSE()
    MESSAGE(WARNING "UnSupported BuildType: ${CMAKE_BUILD_TYPE}")
  ENDIF()

  # Other flags (common C++/C, and C++-only):
  SET(CFLAGS   "${CFLAGS} -march=native  -mtune=native \
                -Weverything             -Wno-covered-switch-default  \
                -Wno-switch-enum         -Wno-exit-time-destructors   \
                -Wno-global-constructors -Wno-reserved-id-macro       \
                -Wno-padded              -Wno-unused-macros           \
                -Wno-gnu-zero-variadic-macro-arguments \
                -Wno-disabled-macro-expansion \
                -Wno-float-equal         -Wno-missing-prototypes \
                -Wno-vla                 -Wno-vla-extension      \
                -Wno-c99-extensions      -Wno-zero-length-array  \
                -Wno-packed              -Wno-undefined-func-template \
                -Wno-gnu-statement-expression")

  SET(CXXFLAGS "${CFLAGS} -std=gnu++20 \
                -Wno-c++98-compat        -Wno-c++98-compat-pedantic    \
                -Wno-weak-vtables        -Wno-double-promotion         \
                -Wno-weak-template-vtables")
#-----------------------------------------------------------------------------#
ELSE()
  MESSAGE(FATAL_ERROR  "UnSupported UNIX ToolChain: ${TOOL_CHAIN}")
ENDIF()

#=============================================================================#
# Compiler-Independent Compilation Env:                                       #
#=============================================================================#
# Reset the standard flags for the selected mode, to prevent a mix-up:
SET(CMAKE_C_FLAGS_RELEASE          "")
SET(CMAKE_C_FLAGS_RELWITHDEBINFO   "")
SET(CMAKE_C_FLAGS_DEBUG            "")

SET(CMAKE_CXX_FLAGS_RELEASE        "")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
SET(CMAKE_CXX_FLAGS_DEBUG          "")

#-----------------------------------------------------------------------------#
# Special defs (mostly for Boost) and libs:                                   #
#-----------------------------------------------------------------------------#
# UNCOMMENT THIS to see and verify how Inlining works:
# SET(CXXFLAGS "${CXXFLAGS} -Winline")

SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${CFLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXXFLAGS}")

# NB: There is an annoying "note" in <variant> in GCC 8.1.1  and maybe other
# compilers, so disable use of <variant> in Boost:
ADD_DEFINITIONS(-DBOOST_SYSTEM_NO_DEPRECATED -UBOOST_CB_ENABLE_DEBUG
                -DBOOST_CB_DISABLE_DEBUG     -DBOOST_TEST_DYN_LINK
                -DBOOST_LOG_DYN_LINK         -DBOOST_HASH_HAS_VARIANT=0
                -DWITH_WT=${WITH_WT}
                -DWITH_RAPIDJSON=${WITH_RAPIDJSON}
                -DWITH_RDKAFKA=${WITH_RDKAFKA}
                -DWITH_PQXX=${WITH_PQXX}
                -DWITH_P2CGATE=${WITH_P2CGATE}
                -DWITH_OPENMP=${WITH_OPENMP}
                -DWITH_VMIME=${WITH_VMIME}
                -DWITH_CURL=${WITH_CURL}
                -DWITH_HDF5=${WITH_HDF5}
                -DWITH_CCTZ=${WITH_CCTZ}
                -DCRYPTO_ONLY=${CRYPTO_ONLY}
                -DUNCHECKED_MODE=${UNCHECKED_MODE})

# NB: DON'T link with Mellanox LibVMA directly -- it's much more flexible to
# use LD_PRELOAD via the "RunVMA.sh" script!
#
# GNUTLS and Boost lobs are always present:
LIST(APPEND EXT_LIBS "stdc++fs" "boost_system" "gnutls" "nghttp2")

# If we use static (perhaps custom-compiled) GNUTLS, we need the following exp-
# licit dependencies:
IF (${GNUTLS_STATIC})
  MESSAGE(STATUS "GNUTLS: Using a static library: ${LIBGNUTLS}")
  LIST(APPEND EXT_LIBS
    "hogweed" "nettle" "tasn1" "gmp" "idn2" "icui18n" "unistring" "p11-kit")
ENDIF()

# System libs:
LIST(APPEND EXT_LIBS "rt" "pthread" "dl" "z")

# XXX: Searching for a better place: BLS-related libs:
SET(BLS_STRAT_LIB
  ${CMAKE_SOURCE_DIR}/TradingStrats/BLS/Common/libBLS_Strategies.so)
SET(BLS_SELECTOR_LIB
  ${CMAKE_SOURCE_DIR}/TradingStrats/BLS/Common/libSelector.so)

#-----------------------------------------------------------------------------#
# Common Configs:                                                             #
#-----------------------------------------------------------------------------#
# During build, link executables with build rpath, not install rpath; they are
# then automatically re-linked on install. This is the default behaviour, any-
# way:
SET(CMAKE_SKIP_BUILD_RPATH            FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH    FALSE)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE )

INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/UHFTCore"
                    "${PROJECT_SOURCE_DIR}/3rdParty"
                    "${ENV_PREFIX}/include")
LINK_DIRECTORIES   ("${PROJECT_BINARY_DIR}"
                    "${LIB_DIR}"
                    "${ENV_PREFIX}/lib")

# Ignore warnings in 3rd-party headers (incl UTXX!):
# XXX: Using ADD_DEFINITIONS to that end is not a very good idea, but we do it
# anyway, in order not to change CMAKE{C|CXX}_FLAGS anymore:
#
ADD_DEFINITIONS  (-isystem "${PROJECT_SOURCE_DIR}/3rdParty"
                  -isystem "${ENV_PREFIX}/include")
IF (WITH_P2CGATE)
  ADD_DEFINITIONS(-isystem "${ENV_PREFIX}/P2CGate/Curr/include")
ENDIF()

#=============================================================================#
# Srcs and Targets:                                                           #
#=============================================================================#
ADD_SUBDIRECTORY(3rdParty)
ADD_SUBDIRECTORY(UHFTCore)

ADD_LIBRARY(MAQUETTE INTERFACE)

# Unfortunately, order MATTERS here:
TARGET_LINK_LIBRARIES(MAQUETTE INTERFACE
  MQTQuantSupport
  MQTConnectors
  MQTInfraStruct)

IF (NOT CRYPTO_ONLY)
TARGET_LINK_LIBRARIES(MAQUETTE INTERFACE
  MQTProtocols)
ENDIF()

TARGET_LINK_LIBRARIES(MAQUETTE INTERFACE
  MQTVenues
  MQTBasis
  MQT3rdParty
  ${EXT_LIBS}
  OpenMP::OpenMP_CXX
  ${VMime_LIBRARIES}
  CURL::libcurl
  ${HDF5_CXX_LIBRARIES}
)

ADD_SUBDIRECTORY(TradingStrats)
ADD_SUBDIRECTORY(Tests)
ADD_SUBDIRECTORY(Tools)
