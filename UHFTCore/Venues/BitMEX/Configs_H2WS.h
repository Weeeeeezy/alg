// vim:ts=2:et
//===========================================================================//
//                     "Venues/BitMEX/Configs_H2WS.h":                       //
//===========================================================================//
// NB: BitMEX only supports the Prod Env. WebSockets are only for MDC and STP;
// API Keys are NOT required for BitMEX WebSockets per se:
//
#pragma  once
#include "Connectors/H2WS/Configs.hpp"
#include <map>

namespace MAQUETTE
{
namespace BitMEX
{
  //=========================================================================//
  // MDC Configs:                                                            //
  //=========================================================================//
  extern std::map<MQTEnvT, H2WSConfig const> const  Configs_WS_MDC;

  //=========================================================================//
  // OMC Configs:                                                            //
  //=========================================================================//
  // XXX: Currently, MDC/STP and OMC Configs are the same, so the latter is just
  // a ref to the former:
  extern std::map<MQTEnvT, H2WSConfig const> const& Configs_H2WS_OMC;

} // End namespace BitMEX
} // End namespace MAQUETTE
