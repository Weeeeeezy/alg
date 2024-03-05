// vim:ts=2:et
//===========================================================================//
//                        "Venues/FXBA/Accounts_FIX.cpp":                    //
//                Account Configs for FXBestAggregator FIX Connector         //
//===========================================================================//
#include "Venues/FXBA/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace FXBA
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // NB: Each account is suitable for both MKT and ORD.
  // We allow for 100 Reqs / sec. Non-FullAmt Orders:
  //
  FIXAccountConfig const accTest1 =
    { "AR-DEV-1", "", "", "", "", "", "", '\0', 0, 1, 100 };

  FIXAccountConfig const accTest2 =
    { "AR-DEV-2", "", "", "", "", "", "", '\0', 0, 1, 100 };

  map<string, FIXAccountConfig const> const FIXAccountConfigs
  {
    { "ALFA1-FIX-FXBA-TestORD", accTest1 },
    { "ALFA1-FIX-FXBA-TestMKT", accTest1 },

    { "ALFA2-FIX-FXBA-TestORD", accTest2 },
    { "ALFA2-FIX-FXBA-TestMKT", accTest2 }
  };
} // End namespace FXBA
} // End namespace MAQUETTE
