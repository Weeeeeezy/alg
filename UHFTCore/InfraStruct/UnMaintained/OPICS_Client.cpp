// vim:ts=2:et
//===========================================================================//
//                       "InfraStruct/OPICS_Client.cpp":                     //
//    Special FIX Client for Submitting ExecReports to OPICS via a Proxy     //
//===========================================================================//
#include "InfraStruct/OPICS_Client.h"
#include "Venues/OPICS_Proxy/Configs.h"

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "OPICS_Client" Non-Default Ctor:                                        //
  //=========================================================================//
  OPICS_Client::OPICS_Client
  (
    MQTEnvT                             a_cenv,
    EPollReactor*                       a_reactor,
    Spdlog::logger*                     a_logger,
    boost::property_tree::ptree const&  a_params
  )
  : FIX_ConnectorSessMgr<FIX::DialectT::OPICS_Proxy, OPICS_Client>
    (
      "OPICS_Client",
      a_reactor,
      this,
      a_logger,
      a_params,
      OPICS_Proxy::FIXSessionConfigs.at(a_cenv),
      OPICS_Proxy::FIXAccountConfigs.at(a_cenv),
      &m_TxSN,
      &m_RxSN
    )
  {}
}
// End namespace MAQUETTE
