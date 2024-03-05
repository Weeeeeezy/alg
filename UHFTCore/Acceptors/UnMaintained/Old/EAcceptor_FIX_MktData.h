// vim:ts=2:et
//===========================================================================//
//                          "EAcceptor_FIX_MktData.h":                       //
//              Generic Embedded FIX Protocol Acceptor (MktData)             //
//===========================================================================//
#pragma once

#include "FixEngine/Acceptor/EAcceptor_FIX.h"

namespace AlfaRobot
{
  //=========================================================================//
  // "EAcceptor_FIX_MktData" Class:                                          //
  //=========================================================================//
  template<FIX::DialectT::type D, typename Conn>
  class  EAcceptor_FIX_MktData:
    public EAcceptor_FIX<D, Conn, EAcceptor_FIX_MktData<D, Conn>>
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Application-Level MktData Msg Buffers:
    mutable FIX::MarketDataRequest  m_MarketDataRequest;

    //=======================================================================//
    // Ctors, Properties:                                                    //
    //=======================================================================//
    // Default Ctor is deleted:
    EAcceptor_FIX_MktData() = delete;

  public:
    // Non-Default Ctor is inherited:
    using EAcceptor_FIX<D, Conn, EAcceptor_FIX_MktData<D, Conn>>::
          EAcceptor_FIX;

    // Capabilities:
    constexpr static bool IsOrdMgmt = false;
    constexpr static bool IsMktData = true;

    //=======================================================================//
    // MktData Methods:                                                      //
    //=======================================================================//
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
    // Parsing and Processing of Application-Level Msgs (MktData):           //
    //-----------------------------------------------------------------------//
    void ParseMarketDataRequest
         (char* a_msg_body,  char const*    a_body_end,
          int   a_msg_sz,    utxx::time_val a_recv_time) const;

    void ProcessMarketDataRequest(FIXSession* /*a_session*/);

  public:
    //-----------------------------------------------------------------------//
    // ECN Core Integration API (MktData):                                   //
    //-----------------------------------------------------------------------//
    // Sends "MarketDataIncrementalRefresh" to the Client:
    void ProcessMarketDataIncrement
         (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityMarketDataIncrement>
          a_incr)
    const;

    // Sends "MarketDataSnapShot"           to the Client:
    void ProcessMarketDataSnapshot
         (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityMarketDataSnapshot>
          a_snshot)
    const;

    // Sends a "MktDataRequestReject"       to the Client:
    void ProcessMarketDataRequestReject
         (Ecn::Basis::SPtr<Ecn::FixServer::LiquidityMarketDataRequestReject>
          a_rej)
    const;

      void ProcessTradingSessionStatus(Ecn::Basis::SPtr<Ecn::FixServer::LiquidityFixSession> /*aParam*/) const;
  };
} // End namespace AlfaRobot
