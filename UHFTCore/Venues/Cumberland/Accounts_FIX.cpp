// vim:ts=2:et
//===========================================================================//
//                    "Venues/Cumberland/Accounts_FIX.cpp":                  //
//                Account Configs for Cumberland FIX Connectors              //
//===========================================================================//
#include <string>
#include <map>

#include "Venues/Cumberland/Configs_FIX.hpp"

namespace MAQUETTE
{
namespace Cumberland
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // NB:  Separate accounts for MKT and ORD. Same accounts are valid across mul-
  //      tiple locations (NY6, LD4 and KVH).
  // XXX: We allow a virtually-unlimited reqs sumission rate (5000 / 1 sec),
  //      this needs to be adjusted:
  //
  std::map<std::string, FIXAccountConfig const> const FIXAccountConfigs
  {
    //-----------------------------------------------------------------------//
    { "BLS1-FIX-Cumberland-Test",
    //-----------------------------------------------------------------------//
      { "97a5fea7-49cf-49a3-8646-125c070d0509", "bl_spartan_cce", "", "", "",
        "5fee41d9-d9e1-447b-adad-ad5d52cfeb0e", "", '\0', 0, 1, 5000
    } },
    //-----------------------------------------------------------------------//
    { "BLS1-FIX-Cumberland-Prod",
    //-----------------------------------------------------------------------//
      { "ab4eb564-c133-42c8-8650-286a034fe90e", "bl_spartan_cpe", "", "", "",
        "983a5e70-f5a9-47b5-885b-e41c86ac77a0", "", '\0', 0, 1, 5000
    } }
  };
} // End namespace Cumberland
} // End namespace MAQUETTE
