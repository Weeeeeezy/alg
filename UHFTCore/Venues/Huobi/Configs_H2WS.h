// vim:ts=2:et
//===========================================================================//
//                      "Venues/Huobi/Configs_H2WS.h":                       //
//===========================================================================//

#pragma  once

#include "Connectors/H2WS/Configs.hpp"
#include "Venues/Huobi/SecDefs.h"

namespace MAQUETTE
{
namespace Huobi
{
  //=========================================================================//
  // MDC Configs:                                                            //
  //=========================================================================//
  extern std::map<std::pair<AssetClassT, MQTEnvT>, H2WSConfig const> const
  Configs_WS_MDC;

  //=========================================================================//
  // OMC Configs (H2 and WS):                                                //
  //=========================================================================//
  extern std::map<std::pair<AssetClassT, MQTEnvT>, H2WSConfig const> const
  Configs_WS_OMC;
  extern std::map<std::pair<AssetClassT, MQTEnvT>, H2WSConfig const> const
  Configs_H2_OMC;

} // namespace Huobi
} // namespace MAQUETTE
