// vim:ts=2:et
//===========================================================================//
//                      "Venues/FORTS/Accounts_TWIME.cpp":                   //
//                   Account Configs for FORTS TWIME Connector               //
//===========================================================================//
#include "Venues/FORTS/Configs_TWIME.hpp"

using namespace std;

namespace MAQUETTE
{
namespace FORTS
{
  //=========================================================================//
  // "TWIMEAccountConfigs":                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Over-All Map:                                                           //
  //-------------------------------------------------------------------------//
  // NB: The last param is MaxReqsPerSec:
  //
  map<string, TWIMEAccountConfig> const TWIMEAccountConfigs
  {
    { "MP001-TWIME-FORTS-TestL", { "twFZct_FZ00TLD", "FZ00TLD", 300 } },
    { "MP001-TWIME-FORTS-Prod",  { "fw22ct_mp001",   "222600T", 300 } },
    { "ALFA1-TWIME-FORTS-Prod",  { "fwa0ct_ar1",     "A004006", 300 } }
  };
} // End namespace FORTS
} // End namespace MAQUETTE
