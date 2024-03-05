// vim:ts=2:et
//===========================================================================//
//                      "Venues/Binance/Configs_WS.h":                       //
//===========================================================================//
// NB: Binance only supports the Prod Env. WebSockets are only for MDC and STP;
// API Keys are NOT required for Binance WebSockets per se:
//
#pragma  once
#include "Connectors/H2WS/Configs.hpp"
#include "Venues/Binance/SecDefs.h"
#include <map>

namespace MAQUETTE
{
namespace Binance
{
  using AssetClassT = ::MAQUETTE::Binance::AssetClassT;

  //=========================================================================//
  // MDC/STP Configs:                                                        //
  //=========================================================================//
  extern std::map<AssetClassT, H2WSConfig const> const Configs_WS_MDC;
  extern std::map<std::pair<AssetClassT,  MQTEnvT>,
                  H2WSConfig const> const Configs_H2_MDC;

  //=========================================================================//
  // OMC Configs (H2 and WS):                                                //
  //=========================================================================//
  extern std::map<AssetClassT, H2WSConfig const> const Configs_WS_OMC;
  extern std::map<std::pair<AssetClassT,  MQTEnvT>,
                  H2WSConfig const> const Configs_H2_OMC;

} // End namespace Binance
} // End namespace MAQUETTE
