// vim:ts=2:et
//===========================================================================//
//                    "Venues/KrakenSpot/Configs_WS.h":                      //
//===========================================================================//
#pragma  once

#include <map>
#include <string>

#include "Connectors/H2WS/Configs.hpp"

namespace MAQUETTE
{
namespace KrakenSpot
{
  //=========================================================================//
  // MDC/STP Configs:                                                        //
  //=========================================================================//
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC;
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC;

} // End namespace KrakenSpot
} // End namespace MAQUETTE
