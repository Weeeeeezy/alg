// vim:ts=2:et
//===========================================================================//
//                      "Venues/NTProgress/Accounts_FIX.cpp":                //
//                  Account Configs for NTProgress FIX Connector             //
//===========================================================================//
#include "Venues/NTProgress/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace NTProgress
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // NB: Separate accounts for MKT and ORD. We allow up to 5000 Reqs / 1 sec!
  // Non-FullAmt Orders:
  //
  map<std::string, FIXAccountConfig const> const FIXAccountConfigs
  {
    //-----------------------------------------------------------------------//
    // Test:                                                                 //
    //-----------------------------------------------------------------------//
    // OMC:
    { "ALFA1-FIX-NTProgress-TestORD",
      { "AlfaBankMM_OM",  "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },
    { "ALFA2-FIX-NTProgress-TestORD",
      { "AlfaBankMM_OM2", "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },

    // MDC:
    { "ALFA1-FIX-NTProgress-TestMKT",
      { "AlfaBankMM_MD",  "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },
    { "ALFA2-FIX-NTProgress-TestMKT",
      { "AlfaBankMM_MD2", "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },

    //-----------------------------------------------------------------------//
    // Prod:                                                                 //
    //-----------------------------------------------------------------------//
    // OMC:
    // Mapped to BISM:
    { "ALFA1-FIX-NTProgress-ProdORD",
      { "AlfaBankMM_OM",  "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },
    // Mapped to BISM:
    { "ALFA2-FIX-NTProgress-ProdORD",
      { "AlfaBankMM_OM2", "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },
    // Mapped to CAES:
    { "ALFA3-FIX-NTProgress-ProdORD",
      { "ALFN_OM",        "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },

    // MDC:
    { "ALFA1-FIX-NTProgress-ProdMKT",
      { "AlfaBankMM_MD",  "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },
    { "ALFA2-FIX-NTProgress-ProdMKT",
      { "AlfaBankMM_MD2", "", "", "", "", "", "", '\0', 0, 1, 5000 }
    },
    { "ALFA3-FIX-NTProgress-ProdMKT",
      { "ALFN_MD",        "", "", "", "", "", "", '\0', 0, 1, 5000 }
    }
  };
} // End namespace NTProgress
} // End namespace MAQUETTE
