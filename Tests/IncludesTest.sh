#! /bin/bash
#=============================================================================#
#                            "Tests/IncludesTest.sh":                         #
#    Verifies that all UHFT Headers can be included in an arbitrary order     #
#=============================================================================#
# (Actually, it does less than that: it only verifies that each Header can be
# included on its own, ie it automatically pulls all of its dependencies)...

# Get all Headers under the UHFT Src Tree:
SrcTop=$DEVTOP/MAQUETTE/UHFT
Extern=$DEVTOP/MAQUETTE/3rdParty
cd $SrcTop

if [ "$1" != "" ]
then
  AllHdrs=$@
else
  AllHdrs=`find . -name '*.h' -o -name '*.hpp' | grep -v Acceptors`
fi

cpp=/tmp/IncludesTest.cpp
obj=/tmp/IncludesTest.o

CXX="g++ -std=gnu++17 -Wall -Wno-deprecated -Wno-class-memaccess \
  -I$SrcTop \
  -I/opt/P2CGate/Curr/include \
  -isystem /opt/P2CGate/Curr/include \
  -I$Extern \
  -isystem $Extern"

# Traverse all Headers found:
for h in $AllHdrs
do
  # Strip the leading "./" from the Header file name:
  h=${h:2}
  echo $h

  # Skip UnMaintained hdrs:
  unm=`echo $h | grep UnMaintained`
  if [ "$unm" != "" ]
  then
    continue
  fi

  # Construct a C++ program which includes this Header TWICE (this checks for
  # both undefined and doubly-defined symbols):
  echo "#include \"$h\"" >  $cpp
  echo "#include \"$h\"" >> $cpp

  # Run the g++ compiler on it:
  $CXX -c $cpp -o $obj

  if [ $? -ne 0 ]
  then
    echo "FAILED: $h"
  fi
done

# Clean-up:
rm $cpp $obj
exit 0
