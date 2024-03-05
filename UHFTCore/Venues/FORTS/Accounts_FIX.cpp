// vim:ts=2:et
//===========================================================================//
//                       "Venues/FORTS/Accounts_FIX.cpp":                    //
//                    Account Configs for FORTS FIX Connector                //
//===========================================================================//
#include "Venues/FORTS/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace FORTS
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // Non-FullAmt Orders:
  //-------------------------------------------------------------------------//
  // Production Accounts:                                                    //
  //-------------------------------------------------------------------------//
  map<string, FIXAccountConfig const> const FIXAccountConfigs
  {
    { "MP001-FIX-FORTS-Prod",   // Ref: FMP0744; up to 300 Reqs / sec:
      { "fg22ctx2mp001", "", "", "", "00T",      "", "2226", 'C', 7, 1, 300 }
    },
    { "ALFA1-FIX-FORTS-Prod",
      { "fga0ctx2exp",   "", "", "",  "A004006", "", "",    '\0', 0, 1, 30  }
    }
  };
} // End namespace FORTS
} // End namespace MAQUETTE
