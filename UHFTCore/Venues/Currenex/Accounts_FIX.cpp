// vim:ts=2:et
//===========================================================================//
//                      "Venues/Currenex/Accounts_FIX.cpp":                  //
//                  Account Configs for Currenex FIX Connectors              //
//===========================================================================//
#include "Venues/Currenex/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace Currenex
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // NB:  Separate accounts for MKT and ORD. Same accounts are valid across mul-
  //      tiple locations (NY6, LD4 and KVH).
  // XXX: We allow a virtually-unlimited reqs sumission rate (5000 / 1 sec),
  //      this needs to be adjusted:
  //
  map<string, FIXAccountConfig const> const FIXAccountConfigs
  {
    //-----------------------------------------------------------------------//
    { "BLS1-MKT-FIX-Currenex-Prod",
    //-----------------------------------------------------------------------//
      { "vel000576str2", "", "vel000576str2", "Welcome1", "", "", "",
        '\0', 0, 1, 5000
    } },

    //-----------------------------------------------------------------------//
    { "BLS1-ORD-FIX-Currenex-Prod",
    //-----------------------------------------------------------------------//
      { "vel000576trd2", "", "vel000576trd2", "Welcome1", "", "", "",
        '\0', 0, 1, 5000
    } }
  };
} // End namespace Currenex
} // End namespace MAQUETTE
