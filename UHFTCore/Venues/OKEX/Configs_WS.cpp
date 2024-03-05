// vim:ts=2:et
//===========================================================================//
//                        "Venues/OKEX/Configs_WS.cpp":                      //
//===========================================================================//
#include "Venues/OKEX/Configs_WS.h"

using namespace std;

namespace MAQUETTE
{
namespace OKEX
{
  //=========================================================================//
  // "Configs_WS_MDC"::                                                      //
  //=========================================================================//
  // NB: For OKEX, only 1 IP address is currently known:
  //
  map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC
  {
    { MQTEnvT::Prod, { "real.okex.com", 8443, { "149.129.81.70" } } }
  };
} // End namespace OKEX
} // End namespace MAQUETTE
