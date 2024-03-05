// vim:ts=2:et
//===========================================================================//
//                 "Connectors/ITCH/EConnector_TCP_ITCH.hpp":                //
//===========================================================================//
#pragma once
#include "Connectors/ITCH/EConnector_TCP_ITCH.h"
#include "Connectors/TCP_Connector.hpp"
#include "Connectors/EConnector_MktData.hpp"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_TCP_ITCH" Non-Default Ctor:                                 //
  //=========================================================================//
  template<ITCH::DialectT::type D, typename Derived>
  inline EConnector_TCP_ITCH<D, Derived>::EConnector_TCP_ITCH
  (
    EConnector_MktData*                 a_primary_mdc,  // May be NULL
    boost::property_tree::ptree const&  a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector_MktData":                                                 //
    //-----------------------------------------------------------------------//
    // FIXME: We should allow Trades based on Features and Config options; for
    // now, WithTrades=false always:
    //
    EConnector_MktData
    (
      true,   // Yes, MDC is Enabled!
      a_primary_mdc,
      a_params,
      ITCH::ProtocolFeatures<D>::s_hasFullAmount,
      false,  // NOT using Sparse OrderBooks here
      false,  // TCP ITCH does NOT use SeqNums (XXX: or it depends on D?)
      false,  // There are  no RptSeqs either
      false,  // And therefore there are no "continuous" RptSeqs
      0,      // TCP ITCH is OrderLog-based (XXX: always?), so MktDepth= +oo
      false,  // No explicit Trades: for ITCH-based ECNs, Trades are unreliable
      false,  // And thus no OrdersLog-derived Trades either
      false   // No DynInitMode for TCP!
    ),
    //-----------------------------------------------------------------------//
    // "TCP_Connector":                                                      //
    //-----------------------------------------------------------------------//
    // NB: At the point when this Ctor is invoked, a real "EConnector" Ctor
    // invocation would necessarily have happened, so it is OK  to refer to
    // EConnector::m_name in the following:
    TCPC
    (
      EConnector::m_name,       // Used for internal purposes
      Get_TCP_ITCH_Config<D>(EConnector::m_name).m_ServerIP,
      Get_TCP_ITCH_Config<D>(EConnector::m_name).m_ServerPort,
      8192 * 1024,              // BuffSz  = 8M (hard-wired)
      8192,                     // BuffLWM = 8k (hard-wired)
      "",                       // XXX: ClientIP not used
      a_params.get<int>     ("MaxConnAttempts", 5),
      Get_TCP_ITCH_Config<D>(EConnector::m_name).m_LogOnTimeOutSec,
      Get_TCP_ITCH_Config<D>(EConnector::m_name).m_LogOffTimeOutSec,
      Get_TCP_ITCH_Config<D>(EConnector::m_name).m_ReConnSec,
      Get_TCP_ITCH_Config<D>(EConnector::m_name).m_HeartBeatSec,
      false,                    // No TLS in ITCH, presumably?
      "",                       // Then HostName is not required either
      IO::TLS_ALPN::UNDEFINED,  // No ITCH in ALPN anyway
      false,                    // No Server Certificate verification
      "",                       // No TLS --  no Server CA File
      "",                       // No TLS --  no ClientCertFile
      "",                       // Similarly, no ClientPrivKeyFile
      a_params.get<std::string>("ProtoLogFile", "")
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_TCP_ITCH" itself:                                         //
    //-----------------------------------------------------------------------//
    m_config(&(Get_TCP_ITCH_Config<D>(EConnector::m_name)))
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // FracQtys and FullAmount cannot both be set, though it is perfectly OK if
    // both are unset:
    static_assert(!(ITCH::ProtocolFeatures<D>::s_hasFullAmount &&
                    ITCH::ProtocolFeatures<D>::s_hasFracQtys),
                  "EConnector_TCP_ITCH: FullAmount and FracQtys options are "
                  "incompatible");
  }

  //=========================================================================//
  // Starting and Stopping:                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  // It only initiates the TCP connection -- no msgs are sent at this time (the
  // latter is done by "InitLogOn"):
  //
  template<ITCH::DialectT::type   D, typename Derived>
  inline void EConnector_TCP_ITCH<D, Derived>::Start()
  {
    // In case if it is a RE-Start, do a clean-up first (incl the subscr info):
    this->ResetOrderBooksAndOrders();

    // Then start the "TCP_Connector" (which will really initiate a TCP Conn):
    TCPC::Start();
  }

  //-------------------------------------------------------------------------//
  // "IsActive":                                                             //
  //-------------------------------------------------------------------------//
  template<ITCH::DialectT::type   D, typename Derived>
  inline bool EConnector_TCP_ITCH<D, Derived>::IsActive() const
    { return TCPC::IsActive(); }

  //=========================================================================//
  // "LogOnCompleted": Wrapper around TCPC::LogOnCompleted:                  //
  //=========================================================================//
  template<ITCH::DialectT::type   D, typename Derived>
  inline void EConnector_TCP_ITCH<D, Derived>::LogOnCompleted
    (utxx::time_val a_ts_recv)
  {
    // Inform the "TCP_Connector" that the LogOn has been successfully comple-
    // ted. This in particular will cancel the Connect/SessionInit/LogOn Time-
    // Out:
    this->TCPC::LogOnCompleted();

    // Subscribe (at the ITCH Protocol level) to MktData for each Instrument:
    // There is no ReqID -- subscriptions will be identified by Symbol:
    //
    Derived* der = static_cast<Derived*>(this);
    for (SecDefD const* instr: this->m_usedSecDefs)
    {
      assert(instr != nullptr);
      der->SendMktDataSubscrReq(*instr);

      // In the FullAmount mode, we will start receiving SnapShots (and only
      // SnapShots) after that automatically; in the Generic mode,  we  also
      // need to request a single initialising SnapShot:
      if (!ITCH::ProtocolFeatures<D>::s_hasFullAmount)
        der->SendMktSnapShotReq(*instr);
    }
    // At this stage, the over-all Connector status becomes "Active":
    assert(IsActive());

    // Notify the Strategies that the Connector is now on-line. XXX:  RecvTime
    // is to be used for ExchTime as well, since we don't have the exact value
    // for the latter. It will also Re-Subscribe all MktData:
    //
    this->ProcessStartStop(true, nullptr, 0, a_ts_recv, a_ts_recv);
    LOG_INFO(1,
      "EConnector_TCP_ITCH::LogOnCompleted: Connector is now ACTIVE")
  }

  //-------------------------------------------------------------------------//
  // "Stop" (graceful, external, virtual):                                   //
  //-------------------------------------------------------------------------//
  template<ITCH::DialectT::type   D, typename Derived>
  inline void EConnector_TCP_ITCH<D, Derived>::Stop()
  {
    // To be polite to the Server, send fraceful UnSubscribe reqs for all Order
    // Books:
    Derived* der = static_cast<Derived*>(this);
    for (SecDefD const* instr: this->m_usedSecDefs)
    {
      assert(instr != nullptr);
      der->SendMktDataUnSubscrReq(*instr);
    }

    // Try to FullStop the "TCP_Connector". Normally, the Stop is graceful (not
    // immediate), but it could sometimes turn into immediate; FullStop=true:
    //
    (void) TCPC::template Stop<true>();
  }

  //-------------------------------------------------------------------------//
  // "StopNow":                                                              //
  //-------------------------------------------------------------------------//
  template<ITCH::DialectT::type   D, typename Derived>
  template<bool FullStop>
  inline void EConnector_TCP_ITCH<D, Derived>::StopNow
    (char const* a_where, utxx::time_val a_ts_recv)
  {
    assert(a_where != nullptr);

    // Perform the TCP DisConnect:
    TCPC::template DisConnect<FullStop>(a_where);

    // At this stage, the over-all Connector status becomes "Inactive":
    assert(this->IsInactive());

    // For safety, once again invalidate the OrderBooks (because "StopNow" is
    // not always called sfter "Stop"):
    this->ResetOrderBooksAndOrders();

    // Notify the Strategies (On=false):
    this->ProcessStartStop(false, nullptr, 0, a_ts_recv, a_ts_recv);
    LOG_INFO(1,
      "EConnector_TCP_ITCH: {}: Connector STOPPED{}",
      ITCH::DialectT::to_string(D), FullStop ? "." : ", but will re-start")
  }

  //-------------------------------------------------------------------------//
  // "ServerInactivityTimeOut":                                              //
  //-------------------------------------------------------------------------//
  template<ITCH::DialectT::type   D, typename Derived>
  inline void EConnector_TCP_ITCH<D, Derived>::ServerInactivityTimeOut
    (utxx::time_val a_ts_recv)
  {
    // Stop immediately, but will try to re-start (FullStop=false):
    StopNow<false>("ServerInactivityTimeOut", a_ts_recv);
  }

  //=========================================================================//
  // Application-Level Msg Processing:                                       //
  //=========================================================================//
  //=========================================================================//
  // "ProcessNewOrder":                                                      //
  //=========================================================================//
  template<ITCH::DialectT::type   D, typename Derived>
  template<QtyTypeT QT, typename QR>
  inline void EConnector_TCP_ITCH<D, Derived>::ProcessNewOrder
  (
    SymKey const&        a_symbol,
    OrderID              a_oid,
    bool                 a_is_bid,
    PriceT               a_px,
    Qty<QT,QR>           a_qty,
    utxx::time_val       a_ts_exch,
    utxx::time_val       a_ts_recv,
    utxx::time_val       a_ts_handl
  )
  {
    utxx::time_val  procTS = utxx::now_utc();

    // Checks: NB: In the FullAmount mode, there should be NO incr updates at
    // all, only SnapShots. Thus, if we got this msg (which is an IncrUpdate),
    // we should NOT be in FullAmt mode:
    assert(!ITCH::ProtocolFeatures<D>::s_hasFullAmount);

    // Get the OrderBook: Produce an err msg if not found (but no exn).  XXX:
    // here we assume that the Symbols must be UNIQUE:
    // UseAltSymbol=false:
    //
    OrderBook* obp = FindOrderBook<false>(a_symbol.data());
    CHECK_ONLY
    (
      if (utxx::unlikely(obp == nullptr))
      {
        LOG_WARN(1,
          "EConnector_TCP_ITCH::ProcessNewOrder: Symbol={}: OrderBook not "
          "found", a_symbol.data())
        return;
      }
    )
    // Apply the Order Update: This is presumably NOT a SnapShot:
    constexpr bool IsSnapShot    = false;
    auto res =
      this->template ApplyOrderUpdate
        <IsSnapShot,       WithIncrUpdates,       StaticInitMode,
         ChangeIsPartFill, NewEncdAsChange,       ChangeEncdAsNew,
         WithRelaxedOBs,   ITCH::ProtocolFeatures<D>::s_hasFullAmount,
         IsSparse,         QT,                    QR>
        (
          a_oid,     FIX::MDUpdateActionT::New,   obp,
          a_is_bid ? FIX::MDEntryTypeT::Bid   :   FIX::MDEntryTypeT::Offer,
          a_px, a_qty, 0, 0
        );
    OrderBook::UpdateEffectT upd   = std::get<0>(res);
    OrderBook::UpdatedSidesT sides = std::get<1>(res);

    // Memoise the result. Verifications / Notifications are delayed until
    // "End-of-Chunk"!
    (void) AddToUpdatedList
      (obp, upd, sides, a_ts_exch, a_ts_recv, a_ts_handl, procTS);
  }

  //=========================================================================//
  // "ProcessModifyOrder":                                                   //
  //=========================================================================//
  template<ITCH::DialectT::type   D, typename Derived>
  template<QtyTypeT QT, typename QR>
  inline void EConnector_TCP_ITCH<D, Derived>::ProcessModifyOrder
  (
    SymKey const&        a_symbol,
    OrderID              a_oid,
    Qty<QT,QR>           a_new_qty,
    utxx::time_val       a_ts_exch,
    utxx::time_val       a_ts_recv,
    utxx::time_val       a_ts_handl
  )
  {
    utxx::time_val  procTS = utxx::now_utc();

    // Checks: Again, in the FullAmount mode, there should be NO incr updates
    // at all (cf "ProcessNewOrder" above):
    assert(!ITCH::ProtocolFeatures<D>::s_hasFullAmount);

    // Get the OrderBook: Produce an err msg if not found (but no exn).  XXX:
    // here we assume that the Symbols must be UNIQUE:
    // UseAltSymbol=false:
    //
    OrderBook* obp = FindOrderBook<false>(a_symbol.data());

    CHECK_ONLY
    (
      if (utxx::unlikely(obp == nullptr))
      {
        LOG_WARN(1,
          "EConnector_TCP_ITCH::ProcessModifyOrder: Symbol={}: OrderBook not "
          "found", a_symbol.data())
        return;
      }
    )
    // Apply the Order Update (NewSide and NewPx are UNDEFINED, since it is an
    // existing order whose Px cannot be changed).   Again, this is presumably
    // NOT a SnapShot:
    constexpr bool IsSnapShot = false;
    auto res =
      this->template ApplyOrderUpdate
        <IsSnapShot,       WithIncrUpdates,       StaticInitMode,
         ChangeIsPartFill, NewEncdAsChange,       ChangeEncdAsNew,
         WithRelaxedOBs,   ITCH::ProtocolFeatures<D>::s_hasFullAmount,
         IsSparse,         QT,                    QR>
        (
          a_oid, FIX::MDUpdateActionT::Change,    obp,
          FIX::MDEntryTypeT::UNDEFINED, PriceT(), a_new_qty, 0, 0
        );
    OrderBook::UpdateEffectT upd   = std::get<0>(res);
    OrderBook::UpdatedSidesT sides = std::get<1>(res);

    // Memoise the result. Verifications / Notifications are delayed until
    // "End-of-Chunk"!
    (void) AddToUpdatedList
      (obp, upd, sides, a_ts_exch, a_ts_recv, a_ts_handl, procTS);
  }

  //=========================================================================//
  // "ProcessCancelOrder":                                                   //
  //=========================================================================//
  template<ITCH::DialectT::type   D, typename Derived>
  template<QtyTypeT QT, typename QR>
  inline void EConnector_TCP_ITCH<D, Derived>::ProcessCancelOrder
  (
    SymKey const&   a_symbol,
    OrderID         a_oid,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    utxx::time_val  procTS = utxx::now_utc();

    // Checks: Again, in the FullAmount mode, there should be NO incr updates
    // at all:
    assert(!ITCH::ProtocolFeatures<D>::s_hasFullAmount);

    // Get the OrderBook: Produce an err msg if not found (but no exn).  XXX:
    // here we assume that the Symbols must be UNIQUE:
    // UseAltSymbol=false:
    //
    OrderBook* obp = FindOrderBook<false>(a_symbol.data());

    CHECK_ONLY
    (
      if (utxx::unlikely(obp == nullptr))
      {
        LOG_WARN(1,
          "EConnector_TCP_ITCH::ProcessCancelOrder: Symbol={}: OrderBook not "
          "found", a_symbol.data())
        return;
      }
    )
    // Apply the Order Update (similar to "Modify" above, NewQty=0). So again,
    // this is presumably NOT a SnapShot:
    constexpr bool IsSnapShot    = false;

    auto res =
      this->template ApplyOrderUpdate
        <IsSnapShot,       WithIncrUpdates,       StaticInitMode,
         ChangeIsPartFill, NewEncdAsChange,       ChangeEncdAsNew,
          WithRelaxedOBs,  ITCH::ProtocolFeatures<D>::s_hasFullAmount,
         IsSparse,         QT,                    QR>
        (
          a_oid, FIX::MDUpdateActionT::Delete,    obp,
          FIX::MDEntryTypeT::UNDEFINED, PriceT(), Qty<QT,QR>(),  0, 0
        );
    OrderBook::UpdateEffectT upd   = std::get<0>(res);
    OrderBook::UpdatedSidesT sides = std::get<1>(res);

    // Memoise the result. Verifications / Notifications are delayed until
    // "End-of-Chunk"!
    (void) AddToUpdatedList
      (obp, upd, sides, a_ts_exch, a_ts_recv, a_ts_handl, procTS);
  }

  //=========================================================================//
  // "SendMsg" (for a Msg of fixed size):                                    //
  //=========================================================================//
  // NB: In ITCH, no Msg Serialization is required: the original Msg is just
  // put on the wire, so this method is quote generic:
  //
  template<ITCH::DialectT::type   D, typename Derived>
  template<typename Msg>
  inline void EConnector_TCP_ITCH<D, Derived>::SendMsg
    (Msg const& a_msg) const
  {
    char const* data = reinterpret_cast<char const*>(&a_msg);
    int         len  = int(sizeof(a_msg));

    // Invoke the SendImpl from "TCP_Connector":
    (void) (this->SendImpl(data, len));

    // Possibly log this msg (IsSend=true):
    CHECK_ONLY
    (
      if (utxx::unlikely(this->m_protoLogger != nullptr))
      {
        Derived const* der = static_cast<Derived const*>(this);
        der->template LogMsg<true>(data, len);
      }
    )
  }
} // End namespace MAQUETTE
