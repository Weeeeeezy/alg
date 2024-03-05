// vim:ts=2:et
//===========================================================================//
//                      "Venues/Binance/Configs_WS.cpp":                     //
//===========================================================================//
#include "Venues/Binance/Configs_H2WS.h"
#include "Venues/Binance/SecDefs.h"

using namespace std;

namespace MAQUETTE
{
namespace Binance
{
  using AssetClassT = ::MAQUETTE::Binance::AssetClassT;

  //=========================================================================//
  // WS End-Points (both MDC and OMC):                                       //
  //=========================================================================//
  // XXX: Located in AWS NorthEast-1? Here explicit IP addrs could be appropri-
  // ate, but still, the list appears to be dependent on the Client IP addr, so
  // we better use purely-dynamic resolution:
  //-------------------------------------------------------------------------//
  // "Configs_WS_MDC"::                                                      //
  //-------------------------------------------------------------------------//
  map<AssetClassT, H2WSConfig const> const Configs_WS_MDC
  {
    { AssetClassT::Spt,  { "stream.binance.com",  9443, { } }},
    { AssetClassT::FutT, { "fstream3.binance.com", 443, { } }},
    { AssetClassT::FutC, { "dstream.binance.com",  443, { } }}
  };

  //-------------------------------------------------------------------------//
  // "Configs_WS_OMC"::                                                      //
  //-------------------------------------------------------------------------//
  // Same as for WS MDC:
  map<AssetClassT, H2WSConfig const> const Configs_WS_OMC = Configs_WS_MDC;

  //=========================================================================//
  // HTTP/2 (REST) End-Points (both MDC and OMC):                            //
  //=========================================================================//
  // For them (api.binance.com), Binance used CloudFront.net IPs, so we do not
  // provide any explicit IP addrs here:
  //
  //=========================================================================//
  // "Configs_H2_OMC":                                                       //
  //=========================================================================//
  map<pair<AssetClassT, MQTEnvT>, H2WSConfig const> const Configs_H2_OMC
  {
    { make_pair(AssetClassT::Spt,  MQTEnvT::Test),
      { "api.binance.com", 443, { } }
    },
    { make_pair(AssetClassT::Spt,  MQTEnvT::Prod),
      { "api.binance.com", 443, { } }
    },
    { make_pair(AssetClassT::FutT, MQTEnvT::Test),
      { "testnet.binancefuture.com", 443, { } }
    },
    { make_pair(AssetClassT::FutT, MQTEnvT::Prod),
      { "fapi.binance.com", 443, { } }
    },
    { make_pair(AssetClassT::FutC, MQTEnvT::Test),
      { "testnet.binancefuture.com", 443, { } }
    },
    { make_pair(AssetClassT::FutC, MQTEnvT::Prod),
      { "dapi.binance.com", 443, { } }
    }
  };

  //=========================================================================//
  // "Configs_H2_MDC":                                                       //
  //=========================================================================//
  map<pair<AssetClassT, MQTEnvT>, H2WSConfig const> const
    Configs_H2_MDC = Configs_H2_OMC;

} // End namespace Binance
} // End namespace MAQUETTE
