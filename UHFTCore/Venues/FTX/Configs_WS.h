// vim:ts=2:et
//===========================================================================//
//                      "Venues/FTX/Configs_WS.h":                           //
//===========================================================================//
// NB: Binance only supports the Prod Env. WebSockets are only for MDC and STP;
// API Keys are NOT required for Binance WebSockets per se:
//
#pragma  once
#include "Connectors/H2WS/Configs.hpp"
#include <map>

namespace MAQUETTE
{
namespace FTX
{
  //=========================================================================//
  // MDC/STP Configs:                                                        //
  //=========================================================================//
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC;
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC;

} // End namespace FTX
} // End namespace MAQUETTE
