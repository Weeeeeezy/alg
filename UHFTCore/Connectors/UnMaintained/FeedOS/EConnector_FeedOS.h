// vim:ts=2:et
//===========================================================================//
//                 "Connectors/FeedOS/EConnector_FeedOS.h":                  //
//      MktData using the QuantHouse / S&P Capital IQ API (aka FeedOS)       //
//===========================================================================//
#pragma once

#include "Connectors/EConnector_MktData.h"
#include "Connectors/FeedOS/Configs.hpp"

#ifndef  TARGET_PLATFORM_LINUX
#define  TARGET_PLATFORM_LINUX 1
#endif
#include <feedos/api/api.h>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_FeedOS" Class:                                              //
  //=========================================================================//
  class EConnector_FeedOS final: public EConnector_MktData
  {
  private:
    //=======================================================================//
    // FeedOS API Call-Back Classes:                                         //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "HandlerL1": Call-Backs for L1 MktData Streaming:                     //
    //-----------------------------------------------------------------------//
    class HandlerL1 final:
    public ::FeedOS::API_QUOTATION_SubscribeInstrumentsL1_base
    {
    private:
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//

    public:
      //---------------------------------------------------------------------//
      // Non-Default Ctor, Dtor:                                             //
      //---------------------------------------------------------------------//

      //---------------------------------------------------------------------//
      // Call-Back Methods:                                                  //
      //---------------------------------------------------------------------//
      void response_QUOTATION_SubscribeInstrumentsL1_Started
      (
        ::FeedOS::uint32                                 a_ticket,
        ::FeedOS::Types::ListOfInstrumentStatusL1 const& a_snapShot
      )
      override;

      void response_QUOTATION_SubscribeInstrumentsL1_Failed
        (::FeedOS::ReturnCode a_rc)
      override;

      void aborted_QUOTATION_SubscribeInstrumentsL1
        (::FeedOS::ReturnCode a_rc)
      override;

      void notif_TradeEventExt
      (
        ::FeedOS::FOSInstrumentCode                      a_code,
        ::FeedOS::Timestamp const&                       a_serverUTCTimestamp,
        ::FeedOS::Timestamp const&                       a_MarketUTCTimestamp,
        ::FeedOS::Types::QuotationTradeEventExt const&   a_data
      )
      override;
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    FeedOS::Config const            m_config; // NB: *our*    FeedOS namespace
    ::FeedOS::API::SimpleConnection m_conn;   // NB: *system* FeedOS namespace
    int const                       m_maxRestarts;
    int                             m_fd;     // Extracted from "m_conn"
    mutable bool                    m_errFlag;

    // Default Ctor is deleted:
    EConnector_FeedOS() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor, Properties:                                   //
    //=======================================================================//
    EConnector_FeedOS
    (
      EPollReactor*                       a_reactor,
      SecDefsMgr*                         a_sec_defs_mgr,
      RiskMgr*                            a_risk_mgr,
      boost::property_tree::ptree const&  a_params
    );

    ~EConnector_FeedOS()   noexcept;

    //=======================================================================//
    // "Start", "Stop", Properties:                                          //
    //=======================================================================//
    // XXX: Here "Start" is a synchronous (blocking) operation. Because of that,
    // it is recommended that this Connector is stared *BEFORE* any  Connectors
    // with asynchronous Start:
    //
    void Start()           override;
    void Stop ()           override;
    bool IsActive () const override;

  private:
    //=======================================================================//
    // Internal Methods:                                                     //
    //=======================================================================//
    // Handlers for "EPollReactor":
    //
    bool RawInputHandler(int a_fd);

    void ErrHandler
    (
      int         a_fd,
      int         a_err_code,
      uint32_t    a_events,
      char const* a_msg
    );

    //=======================================================================//
    // L1 MktData Streaming Call-Backs:                                      //
    //=======================================================================//
  };
} // End namespace MAQUETTE
