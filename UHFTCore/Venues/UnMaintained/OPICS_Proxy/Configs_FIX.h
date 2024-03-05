// vim:ts=2:et
//===========================================================================//
//                    "Venues/OPICS_Proxy/Configs_FIX.h":                   //
//       Server and Account Configs for AlfaBank OPICS Server Proxies        //
//===========================================================================//
// NB: OPICS Proxies are to be accessed by "InfraStruct/OPICS_Client":
//
#pragma  once
#include "Connectors/FIX/Configs.hpp"
#include <map>

namespace MAQUETTE
{
namespace OPICS_Proxy
{
  //========================================================================//
  // FIX Session and Account Configs for OPICS Proxies:                     //
  //========================================================================//
  // Key of each Map: "IsProdEnv" Flag:
  //
  extern std::map<bool, FIXSessionConfig> const FIXSessionConfigs;
  extern std::map<bool, FIXAccountConfig> const FIXAccountConfigs;

} // End namespace OPICS_Proxy
} // End namespace MAQUETTE
