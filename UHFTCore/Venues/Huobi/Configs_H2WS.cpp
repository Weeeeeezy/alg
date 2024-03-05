// vim:ts=2:et

#include "Venues/Huobi/Configs_H2WS.h"

namespace MAQUETTE
{
namespace Huobi
{
  //=========================================================================//
  // WS End-Points (both MDC and OMC):                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Configs_WS_MDC"::                                                      //
  //-------------------------------------------------------------------------//
  std::map<std::pair<AssetClassT, MQTEnvT>, H2WSConfig const> const
  Configs_WS_MDC
  {
    {
      std::make_pair(AssetClassT::Spt, MQTEnvT::Test),
      { "api.huobi.pro", 443, { } }
    },
    {
      std::make_pair(AssetClassT::Spt, MQTEnvT::Prod),
      { "api-aws.huobi.pro", 443, { } }
    },
    // NB: it seems that we cannot connect to the same IP more than once
    {
      std::make_pair(AssetClassT::Fut, MQTEnvT::Prod),
      { "api.hbdm.vn", 443, { "13.226.18.44" } }
    },
    {
      std::make_pair(AssetClassT::CSwp, MQTEnvT::Prod),
      { "api.hbdm.vn", 443, { "13.226.18.70" } }
    },
    {
      std::make_pair(AssetClassT::USwp, MQTEnvT::Prod),
      { "api.hbdm.vn", 443, { "13.226.18.75" } }
    }
  };

  //-------------------------------------------------------------------------//
  // "Configs_WS_OMC"::                                                      //
  //-------------------------------------------------------------------------//
  // Same as for WS MDC:
  std::map<std::pair<AssetClassT, MQTEnvT>, H2WSConfig const> const
  Configs_WS_OMC = Configs_WS_MDC;


  //-------------------------------------------------------------------------//
  // "Configs_H2_OMC"::                                                      //
  //-------------------------------------------------------------------------//
  // Same as for WS MDC:
  std::map<std::pair<AssetClassT, MQTEnvT>, H2WSConfig const> const
  Configs_H2_OMC = Configs_WS_MDC;

} // namespace Huobi
} // namespace MAQUETTE
