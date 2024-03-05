// vim:ts=2:et
//===========================================================================//
//                        "Venues/EBS/SecDefs.cpp":                          //
//                           As of: 15-Jan-2016                              //
//===========================================================================//
#include "Venues/EBS/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace EBS
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // NB: For TenorTime, we use 17:00 EST/EDT which is 21:00 or 22:00 UTC; use
  // the latter, which is 79200 sec intra-day:
  //
  vector<SecDefS> const SecDefs
  {
    { 0, "EUR/USD", "", "", "RCSXXX", "EBS", "", "SPOT", "", "EUR", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    }
  };
} // End namespace EBS
} // End namespace MAQUETTE
