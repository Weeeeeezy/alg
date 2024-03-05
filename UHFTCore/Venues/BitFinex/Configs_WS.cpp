// vim:ts=2:et
//===========================================================================//
//                      "Venues/BitFinex/Configs_WS.cpp":                    //
//===========================================================================//
#include "Venues/BitFinex/Configs_WS.h"

using namespace std;

namespace MAQUETTE
{
namespace BitFinex
{
  //=========================================================================//
  // "Configs_WS_MDC"::                                                      //
  //=========================================================================//
  map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC
  {
    { MQTEnvT::Prod,
      { "api.bitfinex.com", 443,
        {
          // "10.156.112.41"
        }
      }
    },
    { MQTEnvT::Test,
      { "api.bitfinex.com", 443,
        {
        }
      }
    }
  };
  //=========================================================================//
  // "Configs_WS_OMC":                                                       //
  //=========================================================================//
  map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC = Configs_WS_MDC;
} // End namespace BitFinex
} // End namespace MAQUETTE
