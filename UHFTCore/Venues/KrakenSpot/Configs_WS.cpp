// vim:ts=2:et
//===========================================================================//
//                     "Venues/KrakenSpot/Configs_WS.cpp":                   //
//===========================================================================//
#include "Venues/KrakenSpot/Configs_WS.h"

using namespace std;

namespace MAQUETTE
{
namespace KrakenSpot
{
  //=========================================================================//
  // "Configs_WS_MDC"::                                                      //
  //=========================================================================//
  map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC
  {
    { MQTEnvT::Prod, { "ws.kraken.com", 443, {} } }
  };
  //=========================================================================//
  // "Configs_WS_OMC":                                                       //
  //=========================================================================//
  map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC
  {
    { MQTEnvT::Prod, { "ws-auth.kraken.com", 443, {} } }
  };
} // End namespace KrakenSpot
} // End namespace MAQUETTE
