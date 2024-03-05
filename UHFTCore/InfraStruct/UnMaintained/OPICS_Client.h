// vim:ts=2:et
//===========================================================================//
//                         "InfraStruct/OPICS_Client.h":                     //
//    Special FIX Client for Submitting ExecReports to OPICS via a Proxy     //
//===========================================================================//
#pragma  once
#include "Connectors/FIX/FIX_ConnectorSessMgr.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "OPICS_Client" Class:                                                   //
  //=========================================================================//
  class OPICS_Client:
    public FIX_ConnectorSessMgr<FIX::DialectT::OPICS_Proxy, OPICS_Client>
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // NB: Because "OPICS_Client" is NOT embedded into an "EConnector", it must
    // maintain the FIX SeqNums and OrdIDs locally:
    //
  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    OPICS_Client
    (
      MQTEnvT                             a_cenv,
      EPollReactor*                       a_reactor,  // Non-NULL
      Spdlog::logger*                     a_logger,   // Non-NULL
      boost::property_tree::ptree const&  a_params    //
    );

    //=======================================================================//
    // Processor API required by "FIX_Connector_SessMgr":                    //
    //=======================================================================//
  };

} // End namespace MAQUETTE
