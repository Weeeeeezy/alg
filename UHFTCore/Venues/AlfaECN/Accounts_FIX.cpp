// vim:ts=2:et
//===========================================================================//
//                        "Venues/AlfaECN/Accounts_FIX.cpp":                 //
//                    Account Configs for AlfaECN FIX Connector              //
//===========================================================================//
#include "Venues/AlfaECN/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace AlfaECN
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // NB: Separate accounts for MKT and ORD. We allow up to 5000 Reqs / 1 sec!
  //
  FIXAccountConfig const accMKT =
    { "MqtMD",  "", "", "", "", "", "", '\0', 0, 1, 5000 };

  FIXAccountConfig const accORD =
    { "MqtOD",  "", "", "", "", "", "", '\0', 0, 1, 5000 };

  map<string, FIXAccountConfig const> const FIXAccountConfigs
  {
    { "ALFA1-FIX-AlfaECN-Test1ORD", accORD },
    { "ALFA1-FIX-AlfaECN-Test1MKT", accMKT },

    { "ALFA1-FIX-AlfaECN-Test2ORD", accORD },
    { "ALFA1-FIX-AlfaECN-Test2MKT", accMKT },

    { "ALFA1-FIX-AlfaECN-ProdORD",  accORD },
    { "ALFA1-FIX-AlfaECN-ProdMKT",  accMKT }
  };
} // End namespace AlfaECN
} // End namespace MAQUETTE
