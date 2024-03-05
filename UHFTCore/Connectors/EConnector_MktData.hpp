// vim:ts=2:et
//===========================================================================//
//                    "Connectors/EConnector_MktData.hpp":                   //
//                Templated Methods of "EConnector_MktData"                  //
//===========================================================================//
#pragma once

#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/OrderBook.hpp"
#include "InfraStruct/RiskMgr.h"
#include <utxx/convert.hpp>
#include <utxx/error.hpp>
#include <utxx/compiler_hints.hpp>
#include <string>
#include <utility>
#include <tuple>
#include <functional>
#include <algorithm>
#include <cstring>
#include <cassert>

namespace MAQUETTE
{
  //=========================================================================//
  // "OBUpdateInfo" Default Ctor:                                            //
  //=========================================================================//
  inline EConnector_MktData::OBUpdateInfo::OBUpdateInfo()
  : m_ob     (nullptr),
    m_effect (OrderBook::UpdateEffectT::NONE),
    m_sides  (OrderBook::UpdatedSidesT::NONE),
    m_exchTS (),
    m_recvTS (),
    m_handlTS(),
    m_procTS (),
    m_updtTS ()
  {}

  //=========================================================================//
  // "OBUpdateInfo" Non-Default Ctor:                                        //
  //=========================================================================//
  inline EConnector_MktData::OBUpdateInfo::OBUpdateInfo
  (
    OrderBook*                a_ob,
    OrderBook::UpdateEffectT  a_effect,
    OrderBook::UpdatedSidesT  a_sides,
    utxx::time_val            a_ts_exch,
    utxx::time_val            a_ts_recv,
    utxx::time_val            a_ts_handl,
    utxx::time_val            a_ts_proc,
    utxx::time_val            a_ts_updt
  )
  : m_ob     (a_ob),
    m_effect (a_effect),
    m_sides  (a_sides),
    m_exchTS (a_ts_exch),
    m_recvTS (a_ts_recv),
    m_handlTS(a_ts_handl),
    m_procTS (a_ts_proc),
    m_updtTS (a_ts_updt)
  {
    // XXX: We always assume that the following flds must be valid.  Time-
    // Stamps should normally be non-empty as well, but we do not formally
    // assert that:
    assert(m_ob != nullptr);
  }

  //=========================================================================//
  // "OMCTradeProcessor::ProcessTrade":                                      //
  //=========================================================================//
  template<typename OMC>
  struct  OMCTradeProcessor;

  //-------------------------------------------------------------------------//
  // First of all, the case when there is no OMC type ("void"):              //
  //-------------------------------------------------------------------------//
  template<>
  struct OMCTradeProcessor<void>
  {
    template<QtyTypeT QT, typename QR, typename MDC>
    inline static Trade const* ProcessTrade
    (
      MDC*                  UNUSED_PARAM(a_mdc),
      Req12 const*          UNUSED_PARAM(a_req),
      FIX::MDUpdateActionT  UNUSED_PARAM(a_action),
      bool                  UNUSED_PARAM(a_with_ol_trades),
      OrderID               UNUSED_PARAM(a_mde_id),
      ExchOrdID const&      UNUSED_PARAM(a_exec_id),
      PriceT                UNUSED_PARAM(a_last_px),
      Qty<QT,QR>            UNUSED_PARAM(a_trade_sz),
      FIX::SideT            UNUSED_PARAM(a_trade_aggr_side),
      int                   UNUSED_PARAM(a_settl_date),
      utxx::time_val        UNUSED_PARAM(a_ts_exch),
      utxx::time_val        UNUSED_PARAM(a_ts_recv)
    )
    { return nullptr; }
  };

  //-------------------------------------------------------------------------//
  // Generic Case:                                                           //
  //-------------------------------------------------------------------------//
  template<typename OMC>
  struct OMCTradeProcessor
  {
    template<QtyTypeT QT, typename QR, typename MDC>
    inline static Trade const* ProcessTrade
    (
      MDC*                  a_mdc,
      Req12 const*          a_req,
      FIX::MDUpdateActionT  a_action,
      bool                  a_with_ol_trades,
      OrderID               a_mde_id,
      ExchOrdID const&      a_exec_id,
      PriceT                a_last_px,
      Qty<QT,QR>            a_trade_sz,
      FIX::SideT            a_trade_aggr_side,
      int                   a_settl_date,
      utxx::time_val        a_ts_exch,
      utxx::time_val        a_ts_recv
    )
    {
      // Get the OMC:
      OMC* omc =
        (a_req != nullptr)
        ? static_cast<OMC*>(a_req->m_aos->m_omc)
        : nullptr;

      if (omc == nullptr)
        return nullptr;

      // Otherwise: Really use this OMC to Process the Trade:
      assert(a_req != nullptr);

      // Is it is Complete or Partial Fill? -- We only know that for certain if
      // it is FullOrdersLog:
      FuzzyBool isCompleteFill =
        a_with_ol_trades
        ? (a_action == FIX::MDUpdateActionT::Change
          ? FuzzyBool::False          // Modify -> PartFill
          : FuzzyBool::True)          // Delete -> True (no other cases!)
        : FuzzyBool::UNDEFINED;

      // Also, because we got a Trade from an MDC, we don't know directly which
      // Side of this order was from OUR perspective -- use UNDEFINED here,  it
      // will be inferred from the AOS:
      //
      // Invoke the OMC, it will take care of possibly multiple invocations
      // with the same ExecID:
      Trade const* trPtr =
        omc->template OrderTraded<QT, QR, QT, QR>
        (                               // NOT from Cancel/Modify Failure
          omc,                          // ProtoEng
          omc,                          // SessMgr
          a_mdc,                        // MDC
          a_req->m_id,                  // Repeated for consistency
          0,
          const_cast<Req12*>(a_req),    // Req already known -- optimisation!
          FIX::SideT::UNDEFINED,        // Because it is MktData
          a_trade_aggr_side,            // Bid is Aggr?
          a_req->m_exchOrdID,           // Repeated for consistency
          ExchOrdID(a_mde_id),          // Will check Req12 consistency!
          a_exec_id,                    // Used for Trades  Uniqueness
          PriceT(),                     // The OrigPx is not available
          a_last_px,
          a_trade_sz,
          Qty<QT,QR>::Invalid(),        // Unknown LeavesQty: Invalid (< 0)
          Qty<QT,QR>::Invalid(),        // Unknown CumQty:    Invalid
          Qty<QT,QR>(),                 // Unknown Fee, hence 0
          a_settl_date,
          isCompleteFill,
          a_ts_exch,
          a_ts_recv
        );
      // NB: Sometimes, "trPtr" may be NULL (if OMC refused to process this
      // Trade)...
      return trPtr;
    }
  };

  //=========================================================================//
  // "ProcessTrade":                                                         //
  //=========================================================================//
  // Processing just one MDE which corresponds to a Trade.
  // XXX: Unlike "UpdateOrderBooks", here do not use "HandlTS" yet, because
  // Trade processing is more simple than that of OrderBook updates:
  //
  template<bool WithOrdersLog, QtyTypeT QT, typename QR, typename OMC,
           typename Msg>
  inline void EConnector_MktData::ProcessTrade
  (
    Msg                   const& a_msg,
    typename Msg::MDEntry const& a_mde,
    OrderBook::OrderInfo  const* a_order,           // May be NULL (see Hdr)
    OrderBook             const& a_obp,
    Qty<QT,QR>                   a_trade_sz,        // Overrides the MDE data
    FIX::SideT                   a_trade_aggr_side, // ditto
    utxx::time_val               a_ts_recv
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // If we got here, Trades Processing should be enabled -- either via Expli-
    // cit Subscription, or inferred from FullOrdersLog.  However, it might be
    // that the Trades were not explicitly enabled  but just always come along
    // with OrderBook Updates -- in which case we ignore them:
    //
    if (!HasTrades())
      return;

    // Also, "m_withOLTrades" can only be enabled if WithOrdersLog is set. (In
    // that case, "ChangeIsPartFill" must also be set, but we do not check  it
    // here):
    assert(WithOrdersLog || !m_withOLTrades);

    // (*) Not an OrdersLog-inferred Trade:
    //     Sometimes, along with Trades, other data (e.g. Stats) can be transl-
    //     ated in the same Connection or Channel, so ignore them if the occur.
    //     Normally, the valid type is "Trade", but could also be a "TradeList"
    //     (eg on MICEX). The Action must always be "New";
    // (*) OrdersLog-inferred Trade: the valid EntryTypes are Bid and Offer, and
    //     the Action must be "Modify" or "Delete":
    //
    FIX::MDUpdateActionT action    = a_mde.GetUpdateAction();
    CHECK_ONLY
    (
      FIX::MDEntryTypeT  entryType = a_mde.GetEntryType();

      if (utxx::unlikely
         ((!m_withOLTrades &&
           ((entryType  != FIX::MDEntryTypeT::Trade      &&
             entryType  != FIX::MDEntryTypeT::TradeList) ||
             action     != FIX::MDUpdateActionT::New)  )
          ||
          ( m_withOLTrades &&
           ((entryType  != FIX::MDEntryTypeT::Bid        &&
             entryType  != FIX::MDEntryTypeT::Offer)     ||
            (action     != FIX::MDUpdateActionT::Change  &&
             action     != FIX::MDUpdateActionT::Delete)))
             // NB: We already know that it is a Trade, so Delete is indeed a
             // Complete Fill, and Change is a Part-Fill!
          ))
      return;
    )
    //-----------------------------------------------------------------------//
    // Get the Trade Params:                                                 //
    //-----------------------------------------------------------------------//
    // If specified, use the given "a_trade_sz" (possibly coming from the
    // FullOrdersLog);  otherwise, try to extract that info from the MDE:
    if (!IsPos(a_trade_sz) || IsSpecial0(a_trade_sz))
      a_trade_sz = a_mde.GetEntrySize();

    // Same for the Aggressor:
    if (a_trade_aggr_side == FIX::SideT::UNDEFINED)
      a_trade_aggr_side = a_mde.GetTradeAggrSide();

    // Trade Px:
    PriceT lastPx = a_mde.GetEntryPx();

    // If it is an OrdersLog-based MDC and we do not have Order-related params
    // yet, try to get them as well: NB: this normally means that the Trade is
    // NOT OrdersLog-inferred, though we do have OrdersLog (example: MICEX):
    if (WithOrdersLog && a_order == nullptr)
    {
      // XXX: The following should return the MDEntryID of one of the Orders
      // which made this Trade -- hopefully the Passive one:
      OrderID refID = a_mde.GetEntryRefID();
      a_order       = a_obp.GetOrderInfo(refID);

      // NB: "a_order" may still be NULL, eg if "refID" was 0, it's OK. But if
      // it is non-NULL, its MDEntryID must be same as RefID of course:
      assert(a_order == nullptr || a_order->m_id == refID);
    }
    // So get the OrderID (actually an MDEntryID) if available:
    OrderID    mdeID = (a_order != nullptr) ? a_order->m_id    : 0;

    // Get the Req12 ptr (if it was our previously-registered order, or NULL):
    Req12 const* req = (a_order != nullptr) ? a_order->m_req12 : nullptr;

    // If the Req12 was found, it must have a valid AOS ptr:
    assert(req == nullptr || req->m_aos != nullptr);

    // ExecID is normally available (non-empty) -- "Traded" will need it:
    ExchOrdID        execID = a_mde.GetTradeExecID();
    utxx::time_val  eventTS = a_msg.GetEventTS(a_mde);

    // If the Aggressor Side is known, it should be consistent with the Side of
    // "a_order": The latter is assumed to be on the PASSIVE side, so the 2 si-
    // des (if known) should be opposite to each other:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_order != nullptr &&
         ((a_trade_aggr_side == FIX::SideT::Buy   &&  a_order->m_isBid)  ||
          (a_trade_aggr_side == FIX::SideT::Sell  && !a_order->m_isBid)) ))
      {
        LOG_WARN(2,
          "EConnector_MktData::ProcessTrade: ExecID={}, MDEntryID={}: "
          "Inconsistency: AggrSide={}, RefOrderIsBid={}?",
          execID.ToString(), mdeID, int(a_trade_aggr_side), a_order->m_isBid)
        return;
      }
    )
    // Verify the SettlData (Protocol-provided compared to the SecDef one); if
    // a mismatch occurs, we produce an err msg, and STILL use the latter one:
    SecDefD const& instr     = a_obp.GetInstr();
    int            settlDate = a_mde.GetTradeSettlDate();
    CHECK_ONLY
    (
      if (utxx::unlikely(settlDate != 0 && settlDate != instr.m_SettlDate))
        LOG_ERROR(1,
          "EConnector_MktData::ProcessTrade: ExecID={}, MDEntryID={}: "
          "Inconsistency: Protocol SettlDate={}, Instr SettlDate={}: Will use "
          "the latter",
          execID.ToString(), mdeID, settlDate, instr.m_SettlDate)
    )
    //-----------------------------------------------------------------------//
    // If it was Our Trade, invoke the Processor on the corresp OMC:         //
    //-----------------------------------------------------------------------//
    // Do it BEFORE "OnTradeUpdate" (see below), as the latency may be critical
    // here. NB: a non-NULL OMC ptr implies a non-NULL  Req12,  ie it is indeed
    // our own Order:
    //
    Trade const* trPtr =
      OMCTradeProcessor<OMC>::ProcessTrade
      (
        this,    req,     action,     m_withOLTrades,    mdeID,
        execID,  lastPx,  a_trade_sz, a_trade_aggr_side, settlDate,
        eventTS, a_ts_recv
      );
    // At this point, "trPtr" may still be NULL

    //-----------------------------------------------------------------------//
    // Then invoke "OnTradeUpdate" on the subscribed Strategies:             //
    //-----------------------------------------------------------------------//
    if (trPtr == nullptr)
    {
      // Yes, create an on-stack tmp Trade obj, no ID:
      Trade tr
      (
        0,
        this,                         // MDC
        &instr,                       // Instr
        req,                          // May still be NULL
        0,                            // No AccountID here: It is just a Match
        execID,
        lastPx,
        a_trade_sz,
        Qty<QT,QR>(),                 // Unknown fee, hence 0
        a_trade_aggr_side,
        FIX::SideT::UNDEFINED,        // Thus no Account Holder's Side
        eventTS,
        a_ts_recv
      );

      for (OrderBook::SubscrInfo const& sj: a_obp.GetSubscrs())
      {
        Strategy* strat = sj.m_strategy;
        assert(strat != nullptr);
        SAFE_STRAT_CB(strat, OnTradeUpdate, (tr))
      }
    }
    else
      // Use the existing "Trade" obj pointed to by "trPtr":
      for (OrderBook::SubscrInfo const& sj: a_obp.GetSubscrs())
      {
        Strategy* strat = sj.m_strategy;
        assert(strat != nullptr);
        SAFE_STRAT_CB(strat, OnTradeUpdate, (*trPtr))
      }
    // All Done!
  }

  //=========================================================================//
  // "ProcessEndOfDataChunk":                                                //
  //=========================================================================//
  // Invoked when "EAgain" was incountered on the incoming MktData (either UDP
  // msgs or TCP stream):
  template
  <
    bool     IsMultiCast,
    bool     WithIncrUpdates,
    bool     WithOrdersLog,
    bool     WithRelaxedOBs,
    QtyTypeT QT,
    typename QR
  >
  inline void EConnector_MktData::ProcessEndOfDataChunk() const
  {
    // Generically, AT THIS POINT we notify the Subscribers. We can always do
    // that it TCP; but in UDP, notifications are to be delayed if "m_wasLast-
    // Fragm" is not set (ie the prev msg was not a LastFragm, hence the curr
    // OrderBook state may be inconsistent):
    //
    m_delayedNotify = IsMultiCast && !m_wasLastFragm;

    // For efficiency, also check that there are updates indeed:
    if (!m_delayedNotify)
      NotifyOrderBookUpdates
        <IsMultiCast, WithIncrUpdates, WithOrdersLog, WithRelaxedOBs, QT, QR>
        (0, "ProcessEndOfDataChunk");
  }

  //=========================================================================//
  // "UpdateOrderBooks":                                                     //
  //=========================================================================//
  // Performs all required updates to OrderBooks based on the Msg (which may
  // be eg a FAST or FIX SnapShot or IncrementalRefresh Msg);
  // Returns the boolean success flag (mostly useful for SnapShots-based init;
  // it is to be aborted and re-done if !OK):
  template
  <
    bool                                IsSnapShot,
    bool                                IsMultiCast,
    bool                                WithIncrUpdates,
    bool                                WithOrdersLog,
    bool                                WithRelaxedOBs,
    bool                                ChangeIsPartFill,
    bool                                NewEncdAsChange,
    bool                                ChangeEncdAsNew,
    bool                                IsFullAmt,
    bool                                IsSparse,
    EConnector_MktData::FindOrderBookBy FOBBy,
    QtyTypeT                            QT,
    typename                            QR,
    typename                            OMC,
    typename                            Msg
  >
  inline bool EConnector_MktData::UpdateOrderBooks
  (
    SeqNum            a_seq_num,   // NB: May not be available from Msg itself
    Msg const&        a_msg,
    bool              a_dyn_init_mode,
    utxx::time_val    a_ts_recv,
    utxx::time_val    a_ts_handl,
    OrderBook const** a_orderbook_out,
    OrderBook*        a_known_orderbook   // if given, no need to find OB
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));
    assert(IsFullAmt == m_isFullAmt && IsSparse == m_isSparse);

    utxx::time_val  procTS = utxx::now_utc();

    // First of all, the DynInitMode is managed HERE:
    // (*) It can only be RESET;
    // (*) It is only applicable to MultiCast (because in case of TCP, sequenc-
    //     ing of msgs is managed by the Protocol Layer, not here);
    // (*) If there are no IncrUpdates, then we assume that all  SnapShots we
    //     receive are "dynamically equivalent" -- so there is no DynInitMode:
    CHECK_ONLY
    (
      if (utxx::unlikely(!m_dynInitMode && a_dyn_init_mode))
        throw utxx::logic_error
              ("EConnector_MktData::UpdateOrderBooks: Cannot return back to "
               "the DynInitMode: SeqNum=", a_seq_num);
    )
    // So possibly reset DynInitMode -- it's done HERE, once we receive an up-
    // date marked as (!a_syn_init_mode) (eg, all data packets accumulated in
    // the buffer during the Static Init are now over, too):
    m_dynInitMode &= (IsMultiCast & WithIncrUpdates & a_dyn_init_mode);

    // The Orders infrastructure must be present IFF WithOrdersLog is set:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (WithOrdersLog != (m_maxOrders > 0)))
        throw utxx::logic_error
              ("EConnector_MktData::UpdateOrderBooks: ", m_name,
               ": Inconsistency: WithOrdersLog=",    WithOrdersLog,
               ", MaxOrders=", m_maxOrders);
    )
    // Obviously, if IncrUpdates are not supported, it must be a SnapShot:
    // !WithIncrUpdates => IsSnapShot,   and other way round,
    // !IsSnapShot      => WithIncrUpdates:
    static_assert(WithIncrUpdates || IsSnapShot,
                  "EConnector_MktData::UpdateOrderBooks: "
                  "!WithIncrUpdates => IsSnapShot");

    // Furthermore, it would be very strange to have Orders but no Incremental
    // Updates; ie  WithOrdersLog => WithIncrUpdates (but converse is not true):
    static_assert(!WithOrdersLog || WithIncrUpdates,
                  "EConnector_MktData::UpdateOrderBooks: "
                  "WithOrdersLog => WithIncrUpdates");

    // Furthermore, it we have an OrdersLog, it would be very strange to allow
    // Bid-Ask collisions in an OrdersLog-based OrderBook (but the converse is
    // NOT true -- disallowing Bid-Ask collisions obviously  does NOT  mean we
    // have an OrdersLog):
    // WithOrdersLogLog => !WithRelaxedOBs:
    static_assert(!WithOrdersLog || !WithRelaxedOBs,
                  "EConnector_MktData::UpdateOrderBooks: "
                  "WithOrdersLog => !WithRelaxedOBs");

    // Furthermore, if neither SeqNums nor RptSeqs are supported, we cannot re-
    // liably have IncrUpdates in a MultiCast mode (but still can in TCP):
    // IsMultiCast && WithIncrUpdates => WithSeqNums || WithRptSeqs
    // and thus
    // IsMultiCast && WithOrdersLog   => IsMultiCast && WithIncrUpdates
    //                                => WithSeqNums || WithRptSeqs :
    CHECK_ONLY
    (
      if (utxx::unlikely
         (IsMultiCast && WithIncrUpdates && !(m_withSeqNums || m_withRptSeqs)))
        throw utxx::logic_error
          ("EConnector_MktData::UpdateOrderBooks: ", m_name, ": Inconsistency:"
           " IsMultiCast=1 && WithIncrUpdates=1, but m_withSeqNums=0 && "
           "m_withRptSeqs=0");
    )
    // NB: StaticInitMode is set when we have both IncrUpdates and SnapShots,
    // and currently using a SnapShot:
    // In particular, StaticInitMode => IsSnapShot
    //           and  StaticInitMode => WithIncrUpdates:
    // XXX: However, the notion of "StaticInitMode" is only applicable to Mul-
    // tiCast; for TCP, the proper sequencing of SnapShots and IncrUpdates is
    // guaranteed by the underlying protocol, so there is no "StaticInitMode"
    // per se:
    constexpr bool StaticInitMode =
      IsMultiCast && WithIncrUpdates && IsSnapShot;
    char const*    where          =  IsSnapShot ? "SnapShots" : "Incrs";

    // NB: With MultiCast, StaticInitMode => m_dynInitMode
    // but the converse is not true: dynamic initialisation (eg processing of
    // previously-buffered IncrUpdates) may still continue when "StaticInit-
    // Mode" is already over (eg we already switched from SnapShots to Incr-
    // Updates, but still pushing in the buffered IncrUpdates):
    CHECK_ONLY
    (
      if (utxx::unlikely(StaticInitMode && !m_dynInitMode))
        throw utxx::logic_error
              ("EConnector_MktData::UpdateOrderBooks: ", m_name,
               ": Inconsistency: StaticInitMode=1 but DynInitMode=0");
    )
    // If "m_withOLTrades" is set, ie the Trades data are inferred from the Ord-
    // ersLog rather than from separate Trade msgs, we obviously need those Ord-
    // ers. Currently, we also require that ChangeIsPartFill is set in that case
    // (because the Change msgs are used to detect Part-Fills):
    CHECK_ONLY
    (
      if (utxx::unlikely
         (m_withOLTrades && !(WithOrdersLog && ChangeIsPartFill)))
        throw utxx::logic_error
              ("EConnector_MktData::UpdateOrderBooks: ", m_name,
               ": Inconsistency: UseOBTrades=1 but WithOrdersLog=",
               WithOrdersLog, " and ChangeIsPartFill=", ChangeIsPartFill);
    )
    // XXX: Usually, IsMultiCast implies FindOrderBookBy::SecID, but perhaps
    // not always (eg not in UDP ITCH?), so do not assert that...

    //-----------------------------------------------------------------------//
    // Delayed Notification from a Prev Msg:                                 //
    //-----------------------------------------------------------------------//
    // At this point, a (delayed) notification is invoked in UDP mode  if it has
    // not been invoked at the prev EndOfDataChunk (because "m_wasLastFragm" was
    // not set there yet). Then delayed notifications are carried out when Last-
    // Fragm turns from true to false, BEFORE applying the latter msg:
    //
    if constexpr (IsMultiCast)
    {
      bool isLastFragm = a_msg.IsLastFragment();

      if (m_delayedNotify && m_wasLastFragm && !isLastFragm)
      {
        NotifyOrderBookUpdates
          <IsMultiCast, WithIncrUpdates, WithOrdersLog, WithRelaxedOBs, QT, QR>
          (a_seq_num, where);

        // After we have done this, "m_delayedNotify" is to be reset:
        m_delayedNotify = false;
      }
      m_wasLastFragm = isLastFragm;
    }
    //-----------------------------------------------------------------------//
    // Go through the "MDEntries":                                           //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) The "ok" flag is for the curr method invocation;
    // (*) but "m_updated" is persistent across multiple invocations (if multi-
    //     ple Msgs belong to the I/O chunk received by the Reactor),   and is
    //     only cleared when the Last Msg in the chunk has been processed,  be-
    //     cause the "OnOrderBookUpdate" strategy call-backs are invoked then:
    //
    bool       ok    = true;
    OrderBook* obp   = a_known_orderbook;
    PriceT     prevPx;   // Initially NaN
    Qty<QT,QR> prevQty;  // Initially 0

    for (int i = 0; i < a_msg.GetNEntries(); ++i)
    {
      //---------------------------------------------------------------------//
      // Get the MDEntry, its OrderBook and SecDef:                          //
      //---------------------------------------------------------------------//
      typename Msg::MDEntry const& mde = a_msg.GetEntry(i);

      // Find the OrderBook corresponding to this MDE --  always do it for the
      // 1st time, then for the IncrUpdates only (we assume it would be common
      // for all SnapShot MDEs, so in that case, need to find it only once):
      //
      if (!IsSnapShot || obp == nullptr)
      {
        obp = FindOrderBook<FOBBy, Msg>(a_msg, mde, where);
        if (obp == nullptr)
        {
          // "FindOrderBook" has already checked that it's OK  in this case.
          // Otherwise, there would be an exception in the call above.   So
          // nothing to do at all for a SnapShot, or just skip this MDE for
          // an IncrUpdate:
          if constexpr (IsSnapShot)
            return ok;
          else
            continue;
        }
      }
      assert(obp != nullptr);

      // Get the SecDef and the Symbol:
      SecDefD const& instr     = obp->GetInstr();
      char    const* symbol    = a_msg.GetSymbol(mde);

      // Obviously,  Symbol and SecID must be same as recorded in the Book
      // (though the Symbol in the update msg may sometimes be empty) XXX:
      // Instead of  Symbol, the AltSymbol can be used; we currently  use
      // FOBBy to distinguish that case:
      //
      constexpr static bool UseAltSymbol =
        (FOBBy == FindOrderBookBy::AltSymbol);

      assert(symbol == nullptr || *symbol == '\0'  ||
            (UseAltSymbol ? strcmp(symbol, instr.m_AltSymbol.data()) == 0
                          : strcmp(symbol, instr.m_Symbol.data())    == 0));

      // In case if Symbol which came from this MDE is NULL or empty, get it
      // from the OrderBook / SecDef -- it will be useful for diagnostics:
      if (symbol == nullptr || *symbol == '\0')
        symbol =
          UseAltSymbol
          ? instr.m_AltSymbol.data()
          : instr.m_Symbol.data();
      assert(symbol != nullptr && *symbol != '\0');

      // EntryType (Side, or Trade, or EmptyBook flag):
      FIX::MDEntryTypeT entryType = mde.GetEntryType();

      //---------------------------------------------------------------------//
      // Trade MDE?                                                          //
      //---------------------------------------------------------------------//
      // XXX: First of all, this MDE could be a Trade by itself:
      // NB:
      // (*) In this case, we are talking about a "real" Trade update, NOT abo-
      //     ut Trades inferred from the OrdersLog;
      // (*) This is  currently NOT possible with a UDP MultiCast,  because in
      //     that case, OrderBook updates and Trades are always sent via diffe-
      //     rent Channels, so they are never mixed;
      // (*) But this may happen in case of TCP where all MDEtryTypes are sent
      //     in one stream:
      //
      if (entryType == FIX::MDEntryTypeT::Trade   ||
          entryType == FIX::MDEntryTypeT::TradeList)
      {
        if constexpr (IsMultiCast)
          throw utxx::logic_error
                ("EConnector_MktData::UpdateOrderBooks(", where, "): ", m_name,
                 ": Unexepected Trade");

        // If OK: Invoke the Trade Processor. XXX: Some params are not readily-
        // available here, so omitted (but "ProcessTrade" may still try to get
        // them):
        ProcessTrade<WithOrdersLog, QT, QR, OMC>
          (a_msg,  mde,  nullptr, *obp, Qty<QT,QR>::Invalid(),
           FIX::SideT::UNDEFINED, a_ts_recv);

        // To the next MDE:
        continue;
      }

      //---------------------------------------------------------------------//
      // Otherwise: It is an OrderBook-related Entry:                        //
      //---------------------------------------------------------------------//
      // But check if it is a valid one (eg it may be OTC etc). If not, it must
      // also NOT be a valid Trade update (in case if Trades are  inferred from
      // the OrderBook updates), so skip it altogether:
      //
      if (utxx::unlikely(!mde.IsValidUpdate()))
        continue;

      // Generic Case: Update result to be filled in:
      OrderBook::UpdateEffectT upd   = OrderBook::UpdateEffectT::NONE;
      OrderBook::UpdatedSidesT sides = OrderBook::UpdatedSidesT::NONE;

      // XXX: If we are still in the "m_dynInitMode", the Book cannot be Initi-
      // alised yet, because "SetInitialised" is only invoked  in "VerifyOrder-
      // BookUpdates" when "m_dynInitMode" is over:
      CHECK_ONLY
      (
        char const* instrName = instr.m_FullName.data();

        if (utxx::unlikely(m_dynInitMode && obp->IsInitialised()))
        {
          throw utxx::logic_error
                ("EConnector_MktData::UpdateOrderBooks(", where, "): ", m_name,
                 ": Instr=", instrName,
                 ": DynInitMode=1 but OrderBook.IsInitialised=1 also");
        }
      )
      // Time Stamp:
      utxx::time_val exchTS = a_msg.GetEventTS(mde);

      // If SeqNums and/or RptSeqs are not supported, then "a_seq_num" and resp
      // "rptSeq" should be invalid (and visa versa):
      SeqNum rptSeq = a_msg.GetRptSeq(mde);

      CHECK_ONLY
      (
        if (utxx::unlikely
           (m_withSeqNums != (a_seq_num > 0) || m_withRptSeqs != (rptSeq > 0)))
        {
          LOG_ERROR(2,
            "EConnector_MktData::UpdateOrderBooks({}): MDE#={}, Instr={}: "
            "Inconsistency: WithSeqNums={}, SeqNum={}, WithRptSeqs={}, RptSeq="
            "{}", where, i, instrName, m_withSeqNums, a_seq_num, m_withRptSeqs,
            rptSeq)

          // Record this Sequencing Error, so that the Subscribers will later
          // be notified:
          upd  = OrderBook::UpdateEffectT::ERROR;
          (void) AddToUpdatedList
            (obp, upd, sides, exchTS, a_ts_recv, a_ts_handl, procTS);
          ok   = false;
          continue;
        }
      )
      // IMPORTANT: HERE we check whether we should proceed with this update
      // at all, based on RptSeq and SeqNum, and skip it if we know FOR SURE
      // that the update is outdated (more precise checks are then performed
      // by the OrderBook itself):
      // NB: !IsSnapShot => WithIncrUpdates => m_withSeqNums
      //     (so we don't need to test m_withSeqNums explicitly):
      // Also, avoid false skips when "RptSeq" or "SeqNum" are not available:
      //
      if (utxx::unlikely
         (!IsSnapShot   &&
          m_withSeqNums && a_seq_num <= obp->GetLastUpdateSeqNum() &&
          m_withRptSeqs && rptSeq    <= obp->GetLastUpdateRptSeq() ))
        // This is OK (not an error):
        continue;

      // Is it a "Clear-Only" request? Then it should normally come @ i==0
      // (and would typically be the only MDEntry in the curr msg,  but we
      // will not enforce the latter property):
      //
      bool clearOnly = (entryType == FIX::MDEntryTypeT::EmptyBook);
      CHECK_ONLY
      (
        if (utxx::unlikely(clearOnly && i != 0))
          LOG_WARN(2,
            "EConnector_MktData::UpdateOrderBooks({}): MDE#={}, Instr={}: Got "
            "unexpected EmptyBook req, but will still do it...",
            where, i, instrName)
      )
      //---------------------------------------------------------------------//
      // Possibly clear the OrderBook first:                                 //
      //---------------------------------------------------------------------//
      // NB: In the following cases:-
      // (*) (we only have SnapShots (no IncrUpdates) OR we got a SnapShot in a
      //     TCP mode, either at the beginning or in the middle), AND i==0;
      // (*) we got a specific "clearOnly" request at any "i" (though it should
      //     normally be i==0 as well),
      // then we will Clear the OrderBook now.
      // (*) But we do NOT Clear the OrderBook if this Msg is a MiltiCast Snap-
      //     Shot and there are also IncrUpdates, because the OrderBook initia-
      //     lisation may come in SEVERAL SnapShots:
      //
      constexpr bool OnlySnapShots       = !WithIncrUpdates;
      constexpr bool TCPSnapShot         = !IsMultiCast  && IsSnapShot;
      constexpr bool ClearBeforeSnapShot = OnlySnapShots || TCPSnapShot;

      if ((ClearBeforeSnapShot && i == 0) || clearOnly)
        try
        {
          // NB: This "upd" may be overwritten by "Update" below, but if the
          // book was emptied and then filled in again by "Update", we still
          // get the correct "upd":
          // Using "StaticInitMode" here is OK: It only affects SeqNum check
          // (which is indeed statically-driven):
          upd = obp->Clear<StaticInitMode>(rptSeq, a_seq_num);
        }
        catch (std::exception const& exc)
        {
          // Any exception during "Clear" mean SeqNums inconsistency. Still,
          // try to recover and go ahead:
          LOG_ERROR(1, "EConnector_MktData::UpdateOrderBooks: {}", exc.what())
          upd  = OrderBook::UpdateEffectT::ERROR;

          // Record this Sequencing Error, so that the Subscribers will later
          // be notified:
          (void) AddToUpdatedList
            (obp, upd, sides, exchTS, a_ts_recv, a_ts_handl, procTS);
          ok   = false;
          continue;
        }
        // And fall through!

      //---------------------------------------------------------------------//
      // Generic Case: Need a real Update:                                   //
      //---------------------------------------------------------------------//
      if (utxx::likely(!clearOnly))
      {
        //-------------------------------------------------------------------//
        // Get further Update details:                                       //
        //-------------------------------------------------------------------//
        // From now on, "entryType" should only designate the update side:
        CHECK_ONLY
        (
          if (utxx::unlikely
             (entryType != FIX::MDEntryTypeT::Bid  &&
              entryType != FIX::MDEntryTypeT::Offer))
          {
            // This is extremely unlikely -- produce a warning and skip this
            // MDE:
            LOG_ERROR(2,
              "EConnector_MktData::UpdateOrderBooks({}): MDE#={}, Instr={}: "
              "Unrecognised MDEntryType={}",
              where, i, instrName, char(entryType))

            upd = OrderBook::UpdateEffectT::ERROR;

            // Record this EntryType Error, so that the Subscribers will later
            // be notified:
            (void) AddToUpdatedList
              (obp, upd, sides, exchTS, a_ts_recv, a_ts_handl, procTS);
            ok   = false;
            continue;
          }
        )
        // Get the Qty -- we assume that it is always >= 0:
        Qty<QT,QR> qty = mde.GetEntrySize();
        assert(!IsNeg(qty));

        // UpdateAction: For a SnapShot, it must always be "New":
        FIX::MDUpdateActionT action = mde.GetUpdateAction();

        CHECK_ONLY
        (
          if (utxx::unlikely(IsSnapShot && action != FIX::MDUpdateActionT::New))
          {
            LOG_ERROR(2,
              "EConnector_MktData::UpdateOrderBooks({}): MDE#={}, Instr={}: "
              "SnapShot: Invalid UpdateAction={}",
              where,  i, instrName, char(action))

            upd = OrderBook::UpdateEffectT::ERROR;

            // Record this ActionType Error, so that the Subscribers will later
            // be notified:
            (void) AddToUpdatedList
              (obp, upd, sides, exchTS, a_ts_recv, a_ts_handl, procTS);
            ok   = false;
            continue;
          }
        )
        // Entry Px (what it actually means, depends on the Action):
        PriceT px(mde.GetEntryPx());

        //-------------------------------------------------------------------//
        // Apply this MDE:                                                   //
        //-------------------------------------------------------------------//
        if constexpr (WithOrdersLog)
        {
          //-----------------------------------------------------------------//
          // OrdersLog-Based OrderBook:                                      //
          //-----------------------------------------------------------------//
          // NB: Then "WithIncrUpdates" is set as well (see above).
          //
          // Get the EntryID (OrderID) -- XXX: It must be numerical, >= 0:
          // (0 may indicate a non-numerical ID, but may be valid as well;
          // the former case is to be checked MANUALLY once!):
          //
          OrderID orderID = mde.GetMDEntryID();

          // If it is a "Delete", it may be a Fill or a Delete;  "Change" norm-
          // ally means PartFill, but in some rare cases may be an in-place Mo-
          // dification.  So the "wasTraded" flag tells us whether this update
          // was actually due to a Trade  (if uncertain, it should be "false"):
          // Obviously, a "New" order was never traded yet:
          //
          bool wasTraded = mde.WasTradedFOL(); // From the underlying Protocol

          // NB:
          // (*) For IncrUpdates, "WasTraded" is obviously incompatible with a
          //     "New";
          // (*) For SnapShots, all entries are "New", so "WasTraded" flag  is
          //     allowed for Part-Filled orders:
          assert(!(!IsSnapShot  &&     wasTraded      &&
                   action == FIX::MDUpdateActionT::New));

          // IMPORTANT: Update Semantics:
          // NB: "New" and "Delete" actions in the Orders mode are  straight-
          // forward, but "Change" requires special treatment:
          // (1) NORMALLY, the "ChangeIsPartFill" param would be "true",  which
          //     means that the Order Qty (bit NOT Px!) can be changed in-place
          //     after a PartFill, or, very rarely, it can also be reduced (not
          //     enlarged!) by an in-place Modify. Thus, "Change" is really for
          //     an In-Place change, while a generic "CancelReplace" would pro-
          //     duce 2 separate MDEs ("Delete" and "New").
          // (2) But it is also possible that "ChangeIsPartFill" is  "false":
          //     then "Change" does not mean in-place change; rather, it must
          //     be transalted by US into "Delete" and "New"; the "Change" MDE
          //     must then contain enough info for both ops:
          //
          if (!ChangeIsPartFill && action == FIX::MDUpdateActionT::Change)
          {
            //---------------------------------------------------------------//
            // NB: This is a RARE case (2nd of the above):                   //
            //---------------------------------------------------------------//
            // Synthetic Double Action: Delete of "origOrdID" followed by New
            // "orderID": XXX: In this process, OrderID may or may not change;
            // it is sometimes possible that both Side, Qty and Px change for
            // same or modified OrderID, so origOrdID == orderID below is OK!

            // Delete the "origOrder",  NewQty=0:
            OrderID origOrdID = mde.GetEntryRefID();
            auto r1 =
              ApplyOrderUpdate
              <IsSnapShot,       WithIncrUpdates, StaticInitMode,
               ChangeIsPartFill, NewEncdAsChange, ChangeEncdAsNew,
               WithRelaxedOBs,   IsFullAmt,       IsSparse,      QT,  QR>
               (
                 origOrdID, FIX::MDUpdateActionT::Delete,  obp,  entryType,
                 PriceT(),  Qty<QT,QR>(), rptSeq, a_seq_num
               );
            // Now install the New Order. NB: Because it is "New", it was cert-
            // ainly NOT traded yet ("false" flag):
            auto r2 =
              ApplyOrderUpdate
              <IsSnapShot,       WithIncrUpdates, StaticInitMode,
               ChangeIsPartFill, NewEncdAsChange, ChangeEncdAsNew,
               WithRelaxedOBs,   IsFullAmt,       IsSparse,       QT,  QR>
               (
                 orderID,   FIX::MDUpdateActionT::New,      obp,  entryType,
                 px,        qty,          rptSeq, a_seq_num
               );
            // Cumulative Effect:
            upd    = std::max(upd, std::max(std::get<0>(r1), std::get<0>(r2)));
            sides |= std::get<1>(r1);
            sides |= std::get<1>(r2);

            // XXX: Here we currently do NOT infer trade from the OrdersLog...
          }
          else
          {
            //---------------------------------------------------------------//
            // Generic case (1st above):                                     //
            //---------------------------------------------------------------//
            // "New", "Delete", or "Change" which is "PartFill":
            //
            auto apRes =
              ApplyOrderUpdate
              <IsSnapShot,       WithIncrUpdates, StaticInitMode,
               ChangeIsPartFill, NewEncdAsChange, ChangeEncdAsNew,
               WithRelaxedOBs,   IsFullAmt,       IsSparse,        QT,  QR>
               (orderID, action, obp, entryType,  px, qty, rptSeq, a_seq_num);

            OrderBook::UpdateEffectT    upd1     = std::get<0>(apRes);
            OrderBook::UpdatedSidesT    sides1   = std::get<1>(apRes);
            Qty<QT,QR>                  delta    = std::get<2>(apRes);
            OrderBook::OrderInfo const* order    = std::get<3>(apRes);
            Qty<QT,QR>                  absDelta = Abs(delta);

            // Cumulative effect:
            upd    = std::max(upd, upd1);
            sides |= sides1;

            //--------------------------------------------------------------//
            // Inferred Trade?                                              //
            //--------------------------------------------------------------//
            // Apart from updating the OrderBook, this MDE may contain an inf-
            // erred Trade Info. NB: We really need the "wasTraded" flag here,
            // otherwise there is no way to distinguish say a real Delete and
            // a Fill:
            if (m_withOLTrades && wasTraded)
            {
              // This can only happen if the following invars hold:
              assert((action == FIX::MDUpdateActionT::Delete  ||
                      action == FIX::MDUpdateActionT::Change) &&
                      IsNeg(delta) && IsPos(absDelta));

              // Here we apply "overrides" based  on  the extra info we got:
              // The Trade Size is given by "absDelta"; the Aggressor Side is
              // opposite to the Passive (OrderBook) Side:
              FIX::SideT aggrSide   =
                (entryType == FIX::MDEntryTypeT::Bid)
                ? FIX::SideT::Sell  // Hit  by an aggreesive Seller
                : FIX::SideT::Buy;  // Lift by an aggressive Buyer

              // Now invoke "ProcessTrade" on this Inferred Trade, in the same
              // way as we do that for Explicit Trades:
              ProcessTrade<WithOrdersLog, QT, QR, OMC>
                (a_msg, mde, order, *obp, absDelta, aggrSide, a_ts_recv);
            }
          }
        } // Done with the OrdersLog case
        else
        {
          //-----------------------------------------------------------------//
          // No Orders (SnapShots or Aggregated OrderBook):                  //
          //-----------------------------------------------------------------//
          // In this case, invoke "obp->Update" directly: no need for "Check-
          // ApplyUpdate". Obviously, WithOrdersLog=false here;  NB: "qty" is
          // a real target qty, not a delta:
          //
          if (utxx::unlikely(IsSnapShot && !WithOrdersLog && px == prevPx))
          {
            // IMPORTANT: Handle the following cond  which may sometimes  occur
            // in the Aggregated (Banded) SnapShots mode: 2 or more consecutive
            // bands may get same px; unless the following action is taken, each
            // next band's qty would erronoeusly override the prev one. So:
            // (*) In the normal mode, add up the qtys;
            // (*) In the FullAmount (Non-Sweepable) mode, take the max :
            // (However, this fix only works for CONSECUTIVE equal pxs!):
            //
            assert(i >= 1);  // Otherwise, "prevPx" would be NaN
            if constexpr (IsFullAmt)
              qty.MaxWith(prevQty);
            else
              qty +=  prevQty;
          }

          if (entryType == FIX::MDEntryTypeT::Bid)
          {
            OrderBook::UpdateEffectT upd1 =
              obp->Update
                <true,  StaticInitMode, false, !WithIncrUpdates,
                 WithRelaxedOBs,  IsFullAmt, IsSparse,  QT, QR>
                (action, px, qty, rptSeq,    a_seq_num, nullptr);

            upd    = std::max(upd, upd1);
            sides |= OrderBook::UpdatedSidesT::Bid;
          }
          else
          if (entryType == FIX::MDEntryTypeT::Offer)
          {
            OrderBook::UpdateEffectT upd1 =
              obp->Update
                <false, StaticInitMode, false, !WithIncrUpdates,
                 WithRelaxedOBs,  IsFullAmt, IsSparse,  QT, QR>
                (action, px, qty, rptSeq,    a_seq_num, nullptr);

            upd    = std::max(upd, upd1);
            sides |= OrderBook::UpdatedSidesT::Ask;
          }
          else
            // Neither Bid nor Ask: Invalid "entryType": Not allowed here (but
            // it may be allowed eg for "Delete" or "Change" in "ApplyOrderUp-
            // date" above):
            upd = OrderBook::UpdateEffectT::ERROR;
        }
        // For the next "MDEntry" (after an Update):
        prevPx  = px;
        prevQty = qty;
      }
      // End of "Update" case

      //---------------------------------------------------------------------//
      // "MDEntry" application is complete -- Manage the "Updated" list:     //
      //---------------------------------------------------------------------//
      if (utxx::unlikely
         (!AddToUpdatedList
         (obp, upd, sides, exchTS, a_ts_recv, a_ts_handl, procTS)))
        ok = false;
    }
    // End of "MDEntries" loop

    // if a_orderbook_out is not nullptr, fill it with the pointer of the order
    // book
    if (a_orderbook_out != nullptr)
      *a_orderbook_out = obp;

    // NB: NO notifications here: For TCP, they happen at EndOfDataChunk;  and
    // for UDP, at EndOfDataChunk (if the LastFragm is set) or at a subsequent
    // msg (after LastFragm was encountered).
    // All Done:
    return ok;
  }

  //=========================================================================//
  // "ApplyOrderUpdate":                                                     //
  //=========================================================================//
  // NB: We may (very rarely) have !WinthIncrUpdates here, and thus,
  // StaticInitMode != IsSnapShot in general:
  template
  <
    bool     IsSnapShot,
    bool     WithIncrUpdates,
    bool     StaticInitMode,
    bool     ChangeIsPartFill,
    bool     NewEncdAsChange,
    bool     ChangeEncdAsNew,
    bool     WithRelaxedOBs,
    bool     IsFullAmt,
    bool     IsSparse,
    QtyTypeT QT,
    typename QR
  >
  inline
  std::tuple
  <OrderBook::UpdateEffectT, OrderBook::UpdatedSidesT,
   Qty<QT,QR>,               OrderBook::OrderInfo const*>

  EConnector_MktData::ApplyOrderUpdate
  (
    OrderID              a_order_id,
    FIX::MDUpdateActionT a_action,
    OrderBook*           a_ob,       // Non-NULL
    FIX::MDEntryTypeT    a_new_side, // Bid, Offer or maybe UNDEFINED (if !New)
    PriceT               a_new_px,
    Qty<QT,QR>           a_new_qty,
    SeqNum               a_rpt_seq,
    SeqNum               a_seq_num
  )
  const
  {
    //----------------------------------------------------------------------//
    // Check the args:                                                      //
    //----------------------------------------------------------------------//
    assert((IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));
    assert(IsFullAmt == m_isFullAmt && IsSparse == m_isSparse);

    // Obviously, if we are in "StaticInitMode", then it "IsSnapShot" (and in
    // addition,  "IsMultiCast" is set -- but it is not used here):
    static_assert(!StaticInitMode   || IsSnapShot,
                  "StaticInitMode   => IsSnapShot");

    // If there are no IncrUpdates, then it must be a SnapShot:
    static_assert(WithIncrUpdates   || IsSnapShot,
                  "!InsrUpdates     => IsSnapShot");

    // "NewEncdAsChange" makes sense only if it is NOT a SnapShot; otherwise we
    // assume that "New" is really encoded as "New":
    static_assert(!NewEncdAsChange  || !IsSnapShot,
                  "NewEncdAsChange  => !IsSnapShot");

    // Also, "NewEncdAsChange" is clearly inconsistent with "ChangeIsPartFill":
    static_assert(!(NewEncdAsChange && ChangeIsPartFill),
                   "NewEncdAsChange is incompatible with ChangeIsPartFill");

    // "ChangeEncdAsNew" is obviously incompatible with "NewEncdAsChange" and
    // probbaly with "ChangeIsPartFill" (that would look very odd):
    static_assert(!(ChangeEncdAsNew  && (NewEncdAsChange  || ChangeIsPartFill)),
                   "ChangeEncdAsNew  is incompatible with NewEncdAsChange and "
                   "ChangeIsPartFill");

    // In the SnapShot mode, there are no real Changes or PartFilles (XXX: but
    // Change msgs can theoretically appear if they encode New msgs!),  so the
    // following flags are reset in that case. XXX:   We use reser rather than
    // error so that the Caller can use the same set of flags in both SnapShot
    // and !SnapShot modes if both may be present:
    //
    constexpr bool ChangeIsPartFillAct = !IsSnapShot && ChangeIsPartFill;
    constexpr bool ChangeEncdAsNewAct  = !IsSnapShot && ChangeEncdAsNew;

    // Trivial:
    assert(a_ob != nullptr && !IsNeg(a_new_qty));
    char const* instrName  =  a_ob->GetInstr().m_FullName.data();

    //-----------------------------------------------------------------------//
    // Get the OrderInfo and perform Integrity Checks:                       //
    //-----------------------------------------------------------------------//
    // NB: In case of any error, the returned DeltaQty is 0 (NOT Invalid)!
    //
    OrderBook::OrderInfo* order =
      const_cast<OrderBook::OrderInfo*>(a_ob->GetOrderInfo(a_order_id));
    CHECK_ONLY
    (
      if (utxx::unlikely(order == nullptr))
      {
        LOG_ERROR(2,
          "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: "
          "OrderInfo slot not found", instrName, a_order_id)
        return
          std::make_tuple
            (OrderBook::UpdateEffectT::ERROR,
             OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);
      }
    )
    // If OK: NB: "order" may still be empty, actually:
    assert(order != nullptr && !IsNeg(order->m_qty));

    // The "order" must be either completely empty, or valid. (This is an int-
    // ernal invariant, so it is asserted):
    //
    bool orderExists   =  IsPos(order->m_qty);
    assert(orderExists == IsFinite(order->m_px));

    //-----------------------------------------------------------------------//
    // Top-Level Checks:                                                     //
    //-----------------------------------------------------------------------//
    // If we receive ONLY SnapShots (no IncrUpdates),   then the Order must be
    // cleared first (because the OrderID could be RE-USED between SnapShots!),
    // and the Action can only be "New". This is a VERY RARE situation:
    if constexpr (!WithIncrUpdates)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely(a_action != FIX::MDUpdateActionT::New))
        {
          LOG_ERROR(2,
            "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: "
            "Inconsistency: WithIncrUpdates=0 and Action={}",
            instrName, a_order_id, char(a_action))

          // Reject this update but continue:
          return
            std::make_tuple
              (OrderBook::UpdateEffectT::ERROR,
               OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);
        }
      )
      // If OK: Clean-up the slot:
      *order = OrderBook::OrderInfo();
      orderExists = false;
    }

    // If "NewEncdAsChange" flag is set, then there must be no Action=New;
    // "Change" is used instead, and it may mean either "Change" or "New":
    assert(!NewEncdAsChange || a_action != FIX::MDUpdateActionT::New);

    // So: Modify Action in case of NewEncdAsChange:
    if (NewEncdAsChange     && a_action == FIX::MDUpdateActionT::Change &&
        !orderExists)
      a_action = FIX::MDUpdateActionT::New;
    else
    // Similarly, modify Action is case of ChangeEncdAsNewAct:
    if (ChangeEncdAsNewAct  && a_action == FIX::MDUpdateActionT::New    &&
        orderExists)
      a_action = FIX::MDUpdateActionT::Change;

    CHECK_ONLY
    (
      // If the Action is "New", the "order" must not exist yet. The converse is
      // not always true -- eg in the Non-Strict RepSeq mode, some OrderIDs  may
      // be filtered out by the Protocol Layer for irrelevance:
      //
      if (utxx::unlikely(orderExists && a_action == FIX::MDUpdateActionT::New))
      {
        LOG_ERROR(2,
          "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: "
          "Inconsistency: Action=New but OldPx={}, OldQty={}", instrName,
          a_order_id, double(order->m_px), QR(order->GetQty<QT,QR>()))

        // Reject this update but continue:
        return
          std::make_tuple
            (OrderBook::UpdateEffectT::ERROR,
             OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);
      }

      if (utxx::unlikely(!orderExists && a_action != FIX::MDUpdateActionT::New))
      {
        // Attempting to Modify or Delete an non-existing Order?
        if (!m_contRptSeqs)
          // This is quite OK then -- just nothing to do:
          return
            std::make_tuple
              (OrderBook::UpdateEffectT::NONE,
               OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);

        // Otherewise, it is still an error:
        LOG_ERROR(2,
           "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: ",
           "Inconsistency: Action={}, OldPx={}, OldQty={}",
           instrName, a_order_id, char(a_action), double(order->m_px),
           QR(order->GetQty<QT,QR>()))
        return
          std::make_tuple
            (OrderBook::UpdateEffectT::ERROR,
             OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);
      }

      // Also, Actions on existing orders are rejected if they are requested on
      // a wrong side (but it is OK if "a_new_side" is UNDEFINED):
      //
      if (utxx::unlikely
         (orderExists      &&
         ((!order->m_isBid && a_new_side == FIX::MDEntryTypeT::Bid) ||
          ( order->m_isBid && a_new_side == FIX::MDEntryTypeT::Offer)) ))
      {
        LOG_ERROR(2,
          "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: "
          "Inconsistency: Action={}, Side={}, OldIsBid={}",   instrName,
          a_order_id, char(a_action), char(a_new_side), order->m_isBid)

        return
          std::make_tuple
            (OrderBook::UpdateEffectT::ERROR,
             OrderBook::UpdatedSidesT::NONE,  Qty<QT,QR>(), order);
      }
    )
    //-----------------------------------------------------------------------//
    // Action-Specific Processing:                                           //
    //-----------------------------------------------------------------------//
    // Affected OrderBook Sides:
    OrderBook::UpdatedSidesT sides = OrderBook::UpdatedSidesT::NONE;

    bool nonTrivNewPx  =
      IsFinite(a_new_px) && !IsZero(a_new_px) && a_new_px != order->m_px;

    switch (a_action)
    {
    case FIX::MDUpdateActionT::Delete:
    {
      //---------------------------------------------------------------------//
      // "Delete" Action:                                                    //
      //---------------------------------------------------------------------//
      // Normally, NewPx and NewQty must be Trivial (though not necessarily em-
      // pty -- they may also be unchanged). If not, produce a warning (not an
      // error), and still proceed with "Delete":
      CHECK_ONLY
      (
        bool nonTrivNewQty =
          !IsZero(a_new_qty) && (a_new_qty != order->GetQty<QT,QR>());

        if (utxx::unlikely(nonTrivNewQty || nonTrivNewPx))
          LOG_WARN(2,
            "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: "
            "Inconsistency: Action=Delete by NewPx={}, NewQty={}, OldPx={}, "
            "OldQty={}",
            instrName, a_order_id, double(a_new_px),   QR(a_new_qty),
            double(order->m_px),   QR(order->GetQty<QT,QR>()))
      )
      // In any case, for a "Delete", normalise the NewPx and NewQty:
      a_new_px  = PriceT    ();  // 0
      a_new_qty = Qty<QT,QR>();  // 0
      break;
    }
    case FIX::MDUpdateActionT::New:
      //---------------------------------------------------------------------//
      // "New" Action:                                                       //
      //---------------------------------------------------------------------//
      // Both NewSide, NewPx and NewQty must be valid:
      //
      CHECK_ONLY
      (
        if (utxx::unlikely
           (!IsPos(a_new_qty) || !IsFinite(a_new_px)   ||
              (a_new_side  != FIX::MDEntryTypeT::Bid   &&
               a_new_side  != FIX::MDEntryTypeT::Offer)))
        {
          // This is again an error -- reject this update but continue:
          LOG_ERROR(2,
            "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: "
            "Inconsistency: Action=New but NewPx={}, NewQty={}, NewSide={}",
            instrName, a_order_id, double(a_new_px), QR(a_new_qty),
            char(a_new_side))
          return
            std::make_tuple
              (OrderBook::UpdateEffectT::ERROR,
               OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);
        }
      )
      break;

    default:
      //--------------------------------------------------------------------//
      // Finally, "Change" Action:                                          //
      //--------------------------------------------------------------------//
      // NB:
      // (*) If "ChangeIsPartFillAct", then a "Change" must only DECREMENT the
      //     Qty; it cannot change the Px (so NewPx must be Trivial),   and it
      //     cannot increment the Qty. (Unchanged Qty is also not normal -- but
      //     tolerated);
      // (*) But if "ChangeIsPartFillAct" is NOT set, then "Change" msgs can in
      //     general do anything (apart from changing the Order Side, as yet),
      //     so we will in most cases have to emulate them  via  "Delete" and
      //     "New":
      assert(a_action   == FIX::MDUpdateActionT::Change);
      Qty<QT,QR> prevQty = order->GetQty<QT,QR>();

      if (utxx::unlikely(prevQty == a_new_qty && !nonTrivNewPx))
        // Both Qty and Px are unchanged, so really nothing to do: XXX: Return
        // "NONE" even without a warning:
        return
          std::make_tuple
            (OrderBook::UpdateEffectT::NONE,
             OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);

      // Otherwise: Something has changed. We assume it is a PartFill-induced
      // "Change" if the Px is unchanged (ie new px is eother missing or same
      // as existing one) and the Qty decreases, but not to 0:
      bool isPartFill =
        !nonTrivNewPx && IsPos(a_new_qty) && a_new_qty < prevQty;

      if  (isPartFill)
        // This is always OK: The Px is unchanged:
        a_new_px = order->m_px;
      else
      if constexpr (ChangeIsPartFillAct)
      {
        // It  *must* be a PartFill (because Changes are *only* PartFills in
        // this case), but it actually is NOT -- the dynamic "isPartFill" flag
        // is not set. Log and return an error:
        LOG_WARN(2,
          "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}: "
          "Inconsistency: Action=Change but NewPx={}, NewQty={}, OldPx={}, "
          "OldQty={}",
          instrName, a_order_id, double(a_new_px),  QR(a_new_qty),
          double(order->m_px),   QR(order->GetQty<QT,QR>()))
          return
            std::make_tuple
              (OrderBook::UpdateEffectT::ERROR,
               OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);
      }
      else
      {
        // This is not a PartFill, so arbitrary Changes  (of Qtys and/or Pxs)
        // are allowed. Then the Order must be removed first (and then re-ins-
        // erted below):
        Qty<QT,QR> deltaRem = - order->GetQty<QT,QR>();
        order->m_qty        =   QtyU();

        // Remove it from the Aggregated OrderBook:
        // WithOrdersLog=true, SnapShotsOnly=!WithIncrUpdates=false:
        OrderBook::UpdateEffectT remRes =
          order->m_isBid
          ? a_ob->Update
                  <true,   StaticInitMode,  true, false, WithRelaxedOBs,
                   IsFullAmt, IsSparse, QT, QR>
                  (FIX::MDUpdateActionT::Delete, order->m_px, deltaRem,
                   a_rpt_seq, a_seq_num, order)
          : a_ob->Update
                  <false,  StaticInitMode,  true, false, WithRelaxedOBs,
                   IsFullAmt, IsSparse, QT, QR>
                  (FIX::MDUpdateActionT::Delete, order->m_px, deltaRem,
                   a_rpt_seq, a_seq_num, order);

        if (utxx::unlikely(remRes == OrderBook::UpdateEffectT::ERROR))
          // Presumably, the error has already been logged by "Update". Return
          // the "ERROR" immediately:
          return
            std::make_tuple
              (OrderBook::UpdateEffectT::ERROR,
               OrderBook::UpdatedSidesT::NONE, Qty<QT,QR>(), order);
        else
        if (utxx::likely(remRes != OrderBook::UpdateEffectT::NONE))
          sides |=
            (order->m_isBid)
            ? OrderBook::UpdatedSidesT::Bid
            : OrderBook::UpdatedSidesT::Ask;

        // If removed successfully: The second-phase Action will be "New":
        a_action = FIX::MDUpdateActionT::New;
      }
    }
    //-----------------------------------------------------------------------//
    // Compute the QtyDelta, then update Order's Px and Qty:                 //
    //-----------------------------------------------------------------------//
    // QtyDelta for the Aggregated Order Book update:
    Qty<QT,QR> delta = a_new_qty - order->GetQty<QT,QR>();

    // Px for the Aggregated Order Book update. IMPORTANT: For "Delete", it is
    // the OldPx, NOT the new one (which is NaN):
    PriceT effPx =
      (a_action == FIX::MDUpdateActionT::Delete) ? order->m_px : a_new_px;

    // Now update the Order:
    if (a_action == FIX::MDUpdateActionT::New)
      // Here the OrderSide is actually initialised:
      order->m_isBid = (a_new_side == FIX::MDEntryTypeT::Bid);

    // In any case, all other flds are updated except "m_req12" (which may be
    // set via the OMC before this MktData update was received):
    order->m_px  = a_new_px;
    order->m_qty = QtyU(a_new_qty);

    //-----------------------------------------------------------------------//
    // Now try to update the Aggregated OrderBook:                           //
    //-----------------------------------------------------------------------//
    OrderBook::UpdateEffectT upd = OrderBook::UpdateEffectT::NONE;
    try
    {
      // NB: Using "StaticInitMode" here is OK  (it is concerned with SeqNum
      // checks only); WithOrdersLog=true, SnapShotsOnly=!WithIncrUpdates=false:
      upd =
        order->m_isBid
        ? a_ob->Update
                <true,   StaticInitMode,  true, false, WithRelaxedOBs,
                 IsFullAmt, IsSparse, QT, QR>
                (a_action,  effPx, delta, a_rpt_seq,   a_seq_num,  order)
        : a_ob->Update
                <false,  StaticInitMode,  true, false, WithRelaxedOBs,
                 IsFullAmt, IsSparse, QT, QR>
                (a_action,  effPx, delta, a_rpt_seq,   a_seq_num,  order);

      // If the OrderBook has really been updated (on an error has occurred),
      // record the affected side:
      if (upd != OrderBook::UpdateEffectT::NONE)
        sides |=
          (order->m_isBid)
          ? OrderBook::UpdatedSidesT::Bid
          : OrderBook::UpdatedSidesT::Ask;
    }
    catch (std::exception const& exc)
    {
      // OrderBook update by this MDE has failed:
      LOG_ERROR(1,
        "EConnector_MktData::ApplyOrderUpdate: Instr={}, OrderID={}, "
        "IsBid={}, Px={}, Action={}, Delta={}, RptSeq={}, SeqNum={}: {}",
         instrName,      a_order_id, order->m_isBid, double(effPx),
         char(a_action), QR(delta),  a_rpt_seq, a_seq_num,  exc.what())

      upd = OrderBook::UpdateEffectT::ERROR;
    }

    // Finally:
    return std::make_tuple(upd, sides, delta, order);
  }

  //=========================================================================//
  // "VerifyOrderBookUpdates":                                               //
  //=========================================================================//
  // The Post-Processing Stage. Returns its own "ok" flag:
  template
  <
    bool     IsMultiCast,
    bool     WithIncrUpdates,
    bool     WithOrdersLog,
    bool     WithRelaxedOBs,
    QtyTypeT QT,
    typename QR
  >
  inline bool EConnector_MktData::VerifyOrderBookUpdates
  (
    OBUpdateInfo* a_obui,
    SeqNum        CHECK_ONLY(a_seq_num),
    char const*   CHECK_ONLY(a_where)
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Verify the updated OrderBook:                                         //
    //-----------------------------------------------------------------------//
    assert(a_obui != nullptr && a_where != nullptr     &&
           (IsValidQtyRep<QT,QR>(m_qt, m_withFracQtys)));

    OrderBook* ob  = a_obui->m_ob;
    assert    (ob != nullptr);
    OrderBook::UpdateEffectT& upd   = a_obui->m_effect;  // XXX: Ref!
    OrderBook::UpdatedSidesT& sides = a_obui->m_sides;   // XXX: Ref!

    // If, for any reason (although this should NOT be the case), the Book was
    // not really touched , do not post-process it. XXX: Still return OK?
    //
    if (utxx::unlikely(upd == OrderBook::UpdateEffectT::NONE))
      return true;

    //-----------------------------------------------------------------------//
    // The Book is now Initialised?                                          //
    //-----------------------------------------------------------------------//
    // (*) If there are no IncrUpdates at all, then the Book is considered to be
    //     Initialised after the first SnapShot installed -- so set this
    //     property HERE;
    // (*) The Book is considered to be initialised after each and every update
    //     in TCP Mode:
    // NB: The flag set on the OrderBook by "SetInitialised" will trigger an er-
    //     ror next time if this function is ever invoked in the Dynamic InitMo-
    //     de -- but we know it is NOT:
    //
    if constexpr (!WithIncrUpdates || !IsMultiCast)
      ob->SetInitialised();

    // Do not proceed any further if the Book is still not Initialised:
    if (utxx::unlikely(ob->IsInitialised()))
      return true;  // Still OK!

    //-----------------------------------------------------------------------//
    // Is there a Bid-Ask collision?                                         //
    //-----------------------------------------------------------------------//
    // After all, that may happen, in spite of all our preventive logic. In that
    // case, we need  to clear the problem as quickly as possible --  do not al-
    // low wrong liquidity to "stick".
    // NB: If our OrderBooks are "relaxed" (typically for Banded Quotes), then
    // do NOT check them for consistency:
    //
    if (utxx::unlikely(!(WithRelaxedOBs || ob->IsConsistent())))
    {
#     if !UNCHECKED_MODE
      // Get the inconsistency details:
      bool       updBid = ob->m_lastUpdatedBid;
      PriceT     bidPx  = ob->GetBestBidPx ();
      PriceT     askPx  = ob->GetBestAskPx ();
      Qty<QT,QR> bidQty = ob->GetBestBidQty<QT,QR>();
      Qty<QT,QR> askQty = ob->GetBestAskQty<QT,QR>();

      assert((!IsFinite(bidPx) || IsPos(bidQty))  &&
             (!IsFinite(askPx) || IsPos(askQty)));
#     endif
      // Correct the problem, the Book should become consistent again. Record
      // the affected sides (normally only one side is affected):
      OrderBook::UpdatedSidesT correctedSides =
        ob->CorrectBook<WithOrdersLog, QT, QR>();

      assert(ob->IsConsistent());

      // We have "fixed" (as much as we could) this error primarily  to stop
      // its further propagation, but for now, we still record the error cond:
      upd    = OrderBook::UpdateEffectT::ERROR;
      sides |= correctedSides;

      CHECK_ONLY
      (
        // Log a warning (it would not harm):
        LOG_WARN(3,
          "EConnector_MktData::VerifyOrderBookUpdates({}): Instr={}, SeqNum={}"
          ": FIXED Bid-Ask Collision: Bid={} @ {}, Ask={} @ {}, LastUpdateWas="
          "{}",
          a_where,    ob->GetInstr().m_FullName.data(), a_seq_num,
          QR(bidQty), double(bidPx), QR(askQty),    double(askPx),
          updBid ? "Bid" : "Ask")
      )
      // The result is NOT OK:
      return false;
    }
    // All Done:
    return true;
  }

  //=========================================================================//
  // "NotifyOrderBookUpdates":                                               //
  //=========================================================================//
  template
  <
    bool        IsMultiCast,
    bool        WithIncrUpdates,
    bool        WithOrdersLog,
    bool        WithRelaxedOBs,
    QtyTypeT    QT,
    typename    QR
  >
  inline void EConnector_MktData::NotifyOrderBookUpdates
  (
    SeqNum      a_seq_num,
    char const* a_where
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // For efficiency:
    if (m_updated.empty())
      return;

    // In UDP mode, notifications only happen if it was a LastFragm (whether ot
    // not we are at the EndOfDataChunk):
    assert(a_where != nullptr);
    assert(!IsMultiCast || m_wasLastFragm);

    //-----------------------------------------------------------------------//
    // Process the Accumulated Updates:                                      //
    //-----------------------------------------------------------------------//
    for (OBUpdateInfo& obui: m_updated)  // NB: Ref!
    {
      OrderBook* ob  = obui.m_ob;
      assert    (ob != nullptr);
      OrderBook::UpdateEffectT upd = obui.m_effect;

      bool ok = (upd != OrderBook::UpdateEffectT::ERROR);

      //---------------------------------------------------------------------//
      // Before notifying the Strategies:                                    //
      //---------------------------------------------------------------------//
      // Check for Bid-Ask Collisions (and fix any of them):
      ok &= VerifyOrderBookUpdates
            <IsMultiCast, WithIncrUpdates, WithOrdersLog, WithRelaxedOBs,
             QT, QR>
            (&obui, a_seq_num, a_where);

      // XXX: As the above call may modify "obui", re-take "ups", and "sides":
      upd                            = obui.m_effect;
      OrderBook::UpdatedSidesT sides = obui.m_sides;

      // Notify the RiskMgr if the L1Px has changed:
      if (utxx::unlikely
         (upd == OrderBook::UpdateEffectT::L1Px && m_riskMgr != nullptr))
        m_riskMgr->OnMktDataUpdate(*ob, obui.m_updtTS);

      //---------------------------------------------------------------------//
      // Fianally, notify the Subscribed Strategies:                         //
      //---------------------------------------------------------------------//
      // NB: Extra checks before Strategy Call-Backs can be invoked:
      if (utxx::likely
         (ob->IsInitialised()   && upd != OrderBook::UpdateEffectT::NONE))
        for (OrderBook::SubscrInfo const& sj: ob->GetSubscrs())
        {
          // Get the curr TimeStamp -- it will also be passed as "stratTS" to
          // the Strategy (if notified at all):
          utxx::time_val stratTS = utxx::now_utc();

          // Update the Stats -- no matter whether actual notification will
          // happen or not:
          UpdateLatencyStats(obui, stratTS);

          // We only notify the Strategy if "upd" we got is >= than the confi-
          // gured Min Notification Level for that Strategy.  Because the lat-
          // ter Notification Levels are sorted in the ASCENDING order,  once
          // we get to a level which is greater than "upd",  there is nothing
          // more to do:
          if (sj.m_minCBLevel > upd)
            break;

          // So: invoke the Strategy Call-Back on THIS book:
          // NB: "ERROR" level is the highest, so in that case the Strategy is
          // always notified, but with the ErrorFlag set; the  Book  cannot be
          // trusted in that case:
          //
          Strategy* strat = sj.m_strategy;
          assert(strat != nullptr);

          // Possible exceptions are safely caught:
          SAFE_STRAT_CB(strat, OnOrderBookUpdate,
                       (*ob, !ok, sides, obui.m_exchTS, obui.m_recvTS, stratTS))
        }
    }
    //-----------------------------------------------------------------------//
    // After the Call-Backs have been invoked, "m_updated" can be reset:     //
    //-----------------------------------------------------------------------//
    m_updated.clear();
  }

  //=========================================================================//
  // "FindOrderBook" (Generic):                                              //
  //=========================================================================//
  // XXX: The following logic works in the generic case, but use it with care:
  // (*) If "IsMultiCast" is set, ie the Msg has arrived by UDP, we  calculate
  //     its SecID and search for the OrderBook by SecID.  It is OK  if it was
  //     not found (because there may be multiple unsubscribed instrs arriving
  //     through the same MultiCast), so in that case, just return NULL;
  // (*) Otherwise, it is a TCP-based subscription which  TYPICALLY  does  not
  //     carry enough info to calculate the SecID, but it has a SubscrID instd,
  //     so we search by that SubscrID. (It works for FIX, but NOT for TCP ITCH,
  //     which requires a separate search logic):
  //
  template<EConnector_MktData::FindOrderBookBy  FOBBy, typename Msg>
  inline OrderBook* EConnector_MktData::FindOrderBook
  (
    Msg const&                   a_msg,
    typename Msg::MDEntry const& a_mde,
    char const*                  CHECK_ONLY(a_where)
  )
  {
    assert(a_where != nullptr);
    switch(FOBBy)
    {
      //---------------------------------------------------------------------//
      case FindOrderBookBy::SecID:
      //---------------------------------------------------------------------//
      {
        // Then the SecID should be available -- will use it to find the Book:
        SecID sid = a_msg.GetSecID(a_mde);

        CHECK_ONLY
        (
          if (utxx::unlikely(sid == 0))
            throw utxx::runtime_error
              ("EConnector_MktData::FindOrderBook(", a_where, "): ", m_name,
              ": No SecID");
        )
        // Then scan the "m_orderBooks" vector, and return whatever we got,
        // even NULL:
        return FindOrderBook(sid);
      }
      //---------------------------------------------------------------------//
      case FindOrderBookBy::Symbol:
      //---------------------------------------------------------------------//
      {
        char const* symbol = a_msg.GetSymbol(a_mde);
        assert(symbol != nullptr);

        CHECK_ONLY
        (
          if (utxx::unlikely(*symbol == '\0'))
            throw utxx::runtime_error
            ("EConnector_MktData::FindOrderBook(", a_where, "): ", m_name,
             ": No Symbol");
        )
        // Again, we can still get NULL, of course: UseAltSymbol=false:
        return FindOrderBook<false>(symbol);
      }
      //---------------------------------------------------------------------//
      case FindOrderBookBy::AltSymbol:
      //---------------------------------------------------------------------//
      {
        // Similar to FindOrderBookBy::Symbol above, but UseAltSymbol=true.
        // Thus, it is assumed that the value returned by GetSymbol() is
        // actually an AltSymbol:
        char const* altSymbol = a_msg.GetSymbol(a_mde);
        assert(altSymbol != nullptr);

        CHECK_ONLY
        (
          if (utxx::unlikely(*altSymbol == '\0'))
            throw utxx::runtime_error
              ("EConnector_MktData::FindOrderBook(", a_where, "): ", m_name,
               ": No (Alt)Symbol");
        )
        // Again, we can still get NULL, of course: UseAltSymbol=true:
        return FindOrderBook<true>(altSymbol);
      }

      //---------------------------------------------------------------------//
      case FindOrderBookBy::SubscrReqID:
      //---------------------------------------------------------------------//
      {
        // SecID or Symbol are not available from Msg or MDE, but we can get
        // hold of the SubscrReqID used to subscribe to this data.   This is
        // currently used for FIX only. XXX: There is  NO corresponding non-
        // generic "FindOrderBook" function:
        //
        OrderID rid = a_msg.GetMDSubscrID();

        CHECK_ONLY
        (
          if (utxx::unlikely(rid == 0))
            // This must not be the case:
            throw utxx::runtime_error
              ("EConnector_MktData::FindOrderBook(", a_where, "): ", m_name,
               ": No SubscrReqID");
        )
        // Traverse the "{OB,Trd}SubscrID"s and find the Idx of this SubscrID.
        // XXX: We do not know whether "rid" is an OB or Trd SubscrID, so try
        // both, beginning with OB as it is a more frequent case:
        //
        int idx = -1;

        // First, try OBSubscrIDs:
        assert(m_obSubscrIDs != nullptr &&
               m_obSubscrIDs->size() == m_orderBooks->size());

        auto it = boost::container::find_if
                  (m_obSubscrIDs->begin(),    m_obSubscrIDs->end(),
                  [rid](OrderID a_id)->bool { return (a_id == rid); });

        if (utxx::likely(it != m_obSubscrIDs->end()))
        {
          // Get the positional idx -- it will be same for the OrderBook:
          idx = int(it - m_obSubscrIDs->begin());
          assert(idx >= 0);
        }
        else
        if (m_trdSubscrIDs != nullptr)
        {
          // Try TrdSubscrIDs:
          assert(m_trdSubscrIDs->size() == m_orderBooks->size());

          it = boost::container::find_if
               (m_trdSubscrIDs->begin(),  m_trdSubscrIDs->end(),
               [rid](OrderID a_id)->bool { return (a_id == rid); });

          if (utxx::likely(it != m_trdSubscrIDs->end()))
          {
            // Get the positional idx -- again, it will be same for the Book:
            idx = int(it - m_trdSubscrIDs->begin());
            assert(idx >= 0);
          }
        }

        // So: have we found it after all?
        if (utxx::unlikely(idx < 0))
        {
          LOG_WARN(3,
            "EConnector_MktData::FindOrderBook: SubscrReqID={}: Not Found", rid)
          return nullptr;
        }
        // If OK:
        assert(0 <= idx  &&  idx < int(m_orderBooks->size()));
        return m_orderBooks->data() + idx;
      }

      //---------------------------------------------------------------------//
      default:
      //---------------------------------------------------------------------//
      // This point is not reached (if the FOBBy param is valid):
      __builtin_unreachable();
    }
    // This point is not reached:
    __builtin_unreachable();
  }
} // End namespace MAQUETTE
