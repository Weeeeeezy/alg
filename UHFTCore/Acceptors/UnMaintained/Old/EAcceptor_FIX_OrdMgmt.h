// vim:ts=2:et
//===========================================================================//
//                          "EAcceptor_FIX_OrdMgmt.h":                       //
//              Generic Embedded FIX Protocol Acceptor (OrdMgmt)             //
//===========================================================================//
#pragma once

#include "FixEngine/Acceptor/EAcceptor_FIX.h"

namespace AlfaRobot
{
  //=========================================================================//
  // "EAcceptor_FIX_OrdMgmt" Class:                                          //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  class  EAcceptor_FIX_OrdMgmt:
  public EAcceptor_FIX<D, Conn, EAcceptor_FIX_OrdMgmt<D, Conn>>
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Application-Level OrdMgmt Msg Buffers:
    mutable FIX::NewOrderSingle            m_NewOrderSingle;
    mutable FIX::OrderCancelRequest        m_OrderCancelRequest;
    mutable FIX::OrderCancelReplaceRequest m_OrderCancelReplaceRequest;

    //=======================================================================//
    // Ctors, Properties:                                                    //
    //=======================================================================//
    // Default Ctor is deleted:
    EAcceptor_FIX_OrdMgmt() = delete;

  public:
    // Non-Default Ctor is inherited:
    using EAcceptor_FIX<D, Conn, EAcceptor_FIX_OrdMgmt<D, Conn>>::
          EAcceptor_FIX;

    // Capabilities:
    constexpr static bool IsOrdMgmt = true;
    constexpr static bool IsMktData = false;

    //=======================================================================//
    // OrdMgmt Methods:                                                      //
    //=======================================================================//
  // TODO YYY
  public:
    //-----------------------------------------------------------------------//
    // "ReadHandler":                                                        //
    //-----------------------------------------------------------------------//
    // Multiplexed Call-Back for all FIX Responses. Call-Back are invoked via
    // "m_readH" which is registered with the Reactor. Returns the number  of
    // bytes actually consumed (>= 0), or a negative value to indicate immedi-
    // ate stop:
    int ReadHandler
    (
      int             a_fd,
      char const*     a_buff,       // NB: Buffer is managed by the Reactor
      int             a_size,       //
      utxx::time_val  a_recv_time
    );
  private:
    //-----------------------------------------------------------------------//
    // Parsing and Processing of Application-Level Msgs (OrdMgmt):           //
    //-----------------------------------------------------------------------//
    void ParseNewOrderSingle
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseOrderCancelRequest
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ParseOrderCancelReplaceRequest
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ProcessNewOrderSingle           (FIXSession* a_session);
    void ProcessOrderCancelRequest       (FIXSession* a_session);
    void ProcessOrderCancelReplaceRequest(FIXSession* a_session);

  public:
    //-----------------------------------------------------------------------//
    // Sending Application-Level Msgs (OrdMgmt):                             //
    //-----------------------------------------------------------------------//
    // NB: These methods are invoked on receiving internal Liquidity Events (eg
    // from a Matching Engine), so they do not have a "FIXSession*" arg  -- the
    // session is determined inside:
    //
    // Sends "ExecutionReport"      to the Client:
    void ProcessOrderExecutionReport
         (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityOrderExecutionReport>
          a_report)
    const;

    // Sends "OrderCancelReject"    to the Client:
    void ProcessOrderCancelReject
         (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityOrderCancelReject> a_crej)
    const;

    // Sends "TradingSessionStatus" to the Client:
    void ProcessTradingSessionStatus
         (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityFixSession> a_status)
    const;
  };
} // End namespace AlfaRobot
