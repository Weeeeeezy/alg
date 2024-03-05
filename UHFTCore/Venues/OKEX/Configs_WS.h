// vim:ts=2:et
//===========================================================================//
//                       "Venues/OKEX/Configs_WS.h":                         //
//===========================================================================//
#pragma  once
#include "Connectors/H2WS/Configs.hpp"
#include <map>

namespace MAQUETTE
{
namespace OKEX
{
  //=========================================================================//
  // MDC/STP Configs:                                                        //
  //=========================================================================//
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC;

} // End namespace OKEX
} // End namespace MAQUETTE
