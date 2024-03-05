// vim:ts=2:et
//===========================================================================//
//                     "Venues/BitFinex/Configs_WS.h":                       //
//===========================================================================//
#pragma  once
#include "Connectors/H2WS/Configs.hpp"
#include <map>

namespace MAQUETTE
{
namespace BitFinex
{
  //=========================================================================//
  // MDC/STP Configs:                                                        //
  //=========================================================================//
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC;
  extern std::map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC;

} // End namespace BitFinex
} // End namespace MAQUETTE
