#! /bin/bash -e
# vim:ts=2:et
#-----------------------------------------------------------------------------#
# Paths:                                                                      #
#-----------------------------------------------------------------------------#
AbsPath0=`realpath $0`
MQTTop=`dirname $AbsPath0`
Appl=`basename $MQTTop`
DevTop=`dirname $MQTTop`

# Top-level installation prefix:
TopInstallPrefix=/opt

#-----------------------------------------------------------------------------#
# Command-Line Params:                                                        #
#-----------------------------------------------------------------------------#
ConfigMake=0
CleanUp=0
Build=0
Install=0
Verbose=0
DebugMode=0
ReleaseMode=0
UnCheckedMode=0
CryptoOnly=OFF
ToolChain="GCC"
Jobs=`nproc`

function usage
{
  echo "ERROR: Invalid option: $1"
  echo "Available options:"
  echo "-t ToolChain (GCC|CLang), default is GCC"
  echo "-c       : Configure"
  echo "-C       : as above, but with clean-up"
  echo "-b       : Build"
  echo "-i       : Install"
  echo "-j Jobs  : Number of concurrent jobs in make (default:  auto)"
  echo "-d: (use with -c|-C): Configure in the Debug     mode"
  echo "-r: (use with -c|-C): Configure in the Release   mode (default)"
  echo "-u: (use with -c|-C): Configure in the UnChecked mode"
  echo "-v: Verbose output"
  echo "-O: Only build the Crypto-related components"
  echo "-q: Quiet mode (no stdout output)"
  exit 1
}

while getopts ":t:j:cCbidruvqA" opt
do
  case $opt in
    t) ToolChain="$OPTARG";;
    c) ConfigMake=1;;
    C) ConfigMake=1;    CleanUp=1;;
    b) Build=1;;
    i) Install=1;;
    d) DebugMode=1;;
    r) ReleaseMode=1;;
    u) UnCheckedMode=1;;
    q) Verbose=0;;
    v) Verbose=1;;
    O) CryptoOnly=ON;;
    j) Jobs="$OPTARG";;
    *) usage $OPTARG;;
  esac
done

#-----------------------------------------------------------------------------#
# Set the BuildType:                                                          #
#-----------------------------------------------------------------------------#
# By default, assume BuildType=Release. Otherwise, set it from the cmdl params.
# NB:
# (*) UnCheckedMode cannot be combined with DebugMode;
# (*) UnCheckedMode can    be combined with ReleaseMode (the former is a stron-
#     ger version of the latter); only in this case BuildType=Release  is diff-
#     erent from BuildMode=UnChecked;
# (*) Release and Debug Models can be  combined:
#
BuildType="Release"

if [ $UnCheckedMode -eq 1 ]
then
  if [ $DebugMode -eq 1 ]
  then
    echo "UnCheckedMode is incompatible with DebugMode"
    exit 1
  fi
  # BuildType remains "Release" but BuildMode is "UnChecked":
  BuildMode="UnChecked"
else
  # Override BuildType:
  [ $ReleaseMode -eq 0 -a $DebugMode -eq 1 ] && BuildType="Debug"
  [ $ReleaseMode -eq 1 -a $DebugMode -eq 1 ] && BuildType="RelWithDebInfo"
  # Here BuildMode is the same as BuildType:
  BuildMode="$BuildType"
fi

#-----------------------------------------------------------------------------#
# Verify the ToolChain and set the C and C++ Compilers:                       #
#-----------------------------------------------------------------------------#
# XXX: We use the compilers of the appropriate ToolChain just as they appear in
# the current PATH:
#
case "$ToolChain" in
  "GCC")
    CXX=`which g++`
    CC=`which gcc`
    ;;
  "CLang")
    CXX=`which clang++`
    CC=`which clang`
    ;;
  *) echo "ERROR: Invalid ToolChain=$ToolChain (must be: GCC|CLang)";
     exit 1
esac

#-----------------------------------------------------------------------------#
# Go Ahead:                                                                   #
#-----------------------------------------------------------------------------#
# XXX: "bin", "build" and "lib" are currently under the "MQTTop":
#
BldSub="$Appl/$ToolChain-$BuildMode"
BldTop="$DevTop/__BUILD__/$BldSub"
BldDir="$BldTop/build"
BinDir="$BldTop/bin"
LibDir="$BldTop/lib"
InstallPrefix="$TopInstallPrefix/$BldSub"

# Create dirs if they don't exist:
mkdir -p $BldDir
mkdir -p $BinDir
mkdir -p $LibDir

#-----------------------------------------------------------------------------#
# Configure:                                                                  #
#-----------------------------------------------------------------------------#
# Generate Makefiles if requested or build directory is empty:
if [ $ConfigMake -eq 1 ] || [ ! "$(ls -A $BldDir)" ]
then
  # Remove all files in {Bld,Bin,Lib}Dir if requested:
  [ $CleanUp -eq 1 ] && rm -fr $BldDir/*
  [ $CleanUp -eq 1 ] && rm -fr $BinDir/*
  [ $CleanUp -eq 1 ] && rm -fr $LibDir/*

  echo "Generating files in $BldDir..."

  # Run CMake:
  # "$BinDir" (the current dir from which CMake is invoked) will become
  # PROJECT_BINARY_DIR,  and "$SrcDir" (passed explicitly as arg) will
  # become PROJECTS_SOURCE_DIR (some other variables are automatically
  # set as well):
  cmake \
    -G "Unix Makefiles" \
    -D CMAKE_CXX_COMPILER="$CXX" \
    -D CMAKE_C_COMPILER="$CC"  \
    -D TOOL_CHAIN="$ToolChain" \
    -D CMAKE_BUILD_TYPE="$BuildType" \
    -D UNCHECKED_MODE="$UnCheckedMode"       \
    -D CMAKE_INSTALL_PREFIX="$InstallPrefix" \
    -D CMAKE_EXPORT_COMPILE_COMMANDS="ON" \
    -D CRYPTO_ONLY="$CryptoOnly" \
    -D ENV_PREFIX="$EnvPrefix" \
    -D PROJECT_NAME="$Appl" \
    -D LIB_DIR="$LibDir" \
    -D BIN_DIR="$BinDir" \
    -S "$MQTTop" \
    -B "$BldDir"
fi

#-----------------------------------------------------------------------------#
# Build:                                                                      #
#-----------------------------------------------------------------------------#
# NB: Only Makefile-based build can be done in batch mode, so the postfix after
# "BinBase" is always empty:
#
if [ $Build -eq 1 ]
then
  if [ $Verbose -eq 1 ]; then MVerbose="VERBOSE=1"; else MVerbose=""; fi

  cmake --build $BldDir -- -j $Jobs $MVerbose
fi

#-----------------------------------------------------------------------------#
# Install:                                                                    #
#-----------------------------------------------------------------------------#
if [ $Install -eq 1 ]
then
  # Create that dir, and make it accessible for the current user:
  sudo mkdir -p $InstallPrefix
  sudo chown `id -nu`:`id -ng` $InstallPrefix

  if [ $Verbose -eq 1 ]; then MVerbose="-- VERBOSE=1"; else MVerbose=""; fi
  cmake --build $BinDir --target install $MVerbose
fi
exit 0
