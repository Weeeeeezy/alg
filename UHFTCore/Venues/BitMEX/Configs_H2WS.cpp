// vim:ts=2:et
//===========================================================================//
//                     "Venues/BitMEX/Configs_H2WS.cpp":                     //
//===========================================================================//
#include "Venues/BitMEX/Configs_H2WS.h"

using namespace std;

namespace MAQUETTE
{
namespace BitMEX
{
  //=========================================================================//
  // "Configs_WS_MDC":                                                       //
  //=========================================================================//
  // XXX: BEWARE: Those IPs are changing very frequently!!!
  //
  map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC
  {
    {
      MQTEnvT::Prod,
      {
        "www.bitmex.com", 443,
        {
        }
      }
    },
    {
      MQTEnvT::Test,
      {
        "www.bitmex.com", 443,
        {
        }
      }
    }
  };
  //=========================================================================//
  // "Configs_H2WS_OMC" (Ref):                                               //
  //=========================================================================//
  // Same "www.bitmex.com" and "testnet.bitmex.com":
  //
  map<MQTEnvT, H2WSConfig const> const& Configs_H2WS_OMC = Configs_WS_MDC;

} // End namespace BitMEX
} // End namespace MAQUETTE
