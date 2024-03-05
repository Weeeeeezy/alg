#!/bin/bash

wget https://api.huobi.pro/v1/common/symbols
cat symbols | sed 's/{\"status\":\"ok\",\"data\":\[//g' |
 sed 's/{\"base-currency\"://g' |
 sed 's/,\"quote-currency\":/ /g' |
 sed 's/,\"price-precision\":/ /g' |
 sed 's/,\"amount-precision\":/ /g' |
 sed 's/,\"symbol-partition\":/ /g' |
 sed 's/,\"symbol\":/ /g' |
 sed 's/,\"state\":/ /g' |
 sed 's/,\"value-precision\":/ /g' |
 sed 's/,\"min-order-amt\":/ /g' |
 sed 's/,\"max-order-amt\":/ /g' |
 sed 's/,\"min-order-value\":/ /g' |
 sed 's/,\"leverage-ratio\":[0-9.]*/ /g' |
 sed 's/,\"super-margin-leverage-ratio\":[0-9.]*/ /g' |
 sed 's/}]}/\n/g' |
 sed 's/},/ ,\n/g' |
awk 'BEGIN{print "\
// vim:ts=2:et \n\
//===========================================================================//\n\
//                      \"Venues/Huobi/SecDefs.cpp\":                        //\n\
//===========================================================================//\n\
#include \"Venues/Huobi/SecDefs.h\"\n\
using namespace std;\n\
\n\
namespace MAQUETTE\n\
{\n\
namespace Huobi\n\
{\n\
  //=========================================================================//\n\
  // \"SecDefs\":                                                              //\n\
  //=========================================================================//\n\
  vector<SecDefS> const SecDefs\n\
  {\
"}{print "    { 0, " $6 ", " $6 ", \"\", \"\", \"Huobi\", \"\", \"SPOT\", \"\", " $1 ", " $2 ", \x27A\x27, 1.0, " $9 ", 1, 1e-" $3 ", \x27A\x27, 1.0, 0, 0, 0.0, 0, \"\"}" $12}\
END{print "  };\n}\n}"} \
' > SecDefs.cpp

rm symbols
