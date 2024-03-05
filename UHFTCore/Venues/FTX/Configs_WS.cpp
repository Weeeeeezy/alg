// vim:ts=2:et
//===========================================================================//
//                      "Venues/FTX/Configs_WS.cpp":                         //
//===========================================================================//
#include "Venues/FTX/Configs_WS.h"

using namespace std;

namespace MAQUETTE
{
namespace FTX
{
  //-------------------------------------------------------------------------//
  // "Configs_WS_MDC"::                                                      //
  //-------------------------------------------------------------------------//
  map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC
  {
    {
      MQTEnvT::Test,
      {
        "ftx.com", 443,
        { }
      }
    },
    {
      MQTEnvT::Prod,
      {
        "ftx.com", 443,
        { }
      }
    }
  };
  //=========================================================================//
  // "Configs_WS_OMC":                                                       //
  //=========================================================================//
  map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC = Configs_WS_MDC;
} // End namespace FTX
} // End namespace MAQUETTE
