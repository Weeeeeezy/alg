// vim:ts=2:et
//===========================================================================//
//                        "Venues/EBS/Accounts_FIX.cpp":                     //
//                    Account Configs for EBS FIX Connector                  //
//===========================================================================//
#include "Venues/EBS/Configs_FIX.hpp"

using namespace std;

namespace MAQUETTE
{
namespace EBS
{
  //=========================================================================//
  // "FIXAccountConfigs":                                                    //
  //=========================================================================//
  // XXX: We allow for 100 Reqs per sec, though not sure whether this is really
  // OK:
  FIXAccountConfig const accSESSION =
    { "MqtS",  "", "", "", "", "", "", '\0', 0, 1, 100 };

  map<string, FIXAccountConfig const> const FIXAccountConfigs
  {
    { "EBS01-FIX-EBS-Test1Session", accSESSION }
  };
} // End namespace EBS
} // End namespace MAQUETTE
