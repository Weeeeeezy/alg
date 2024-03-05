// vim:ts=2
//===========================================================================//
//                     "EConnector_LATOKEN_STP-DB.cpp":                      //
//                          DB-Related Operations                            //
//===========================================================================//
#include "Connectors/Kafka/EConnector_LATOKEN_STP.h"
#include "Venues/LATOKEN/Configs_WS.h"
#include "Protocols/H2WS/LATOKEN-OMC.hpp"
#include <exception>

using namespace std;

namespace
{
  //=========================================================================//
  // Temporal Params:                                                        //
  //=========================================================================//
  // If an Order is Filled or Cancelled within this time from Acceptance, it
  // will be removed from "LiqStats":  It is either an Aggressive one evaded
  // the initial detection, or a High-Freq MM-type order which is not of int-
  // erest for us either: Say 1 sec:
  //
  utxx::secs const AggrSkipInt(1);

  // How long do we wait until the Order is discarded from the Stats as UnFil-
  // led (eg a Deeply-Passive one), to keep the Stats small? Say 1 hour:
  //
  utxx::secs const HistoryInt (3600);
}

namespace MAQUETTE
{
  //=========================================================================//
  // "LiqStats" Non-Default Ctor:                                            //
  //=========================================================================//
  inline EConnector_LATOKEN_STP::LiqStats::LiqStats
  (
    ObjName const& a_ord_id,
    UserID         a_user_id,
    bool           a_is_bid,
    PriceT         a_px,
    RMQtyA         a_qty
  )
  : m_ordID        (a_ord_id),
    m_userID       (a_user_id),
    m_isBid        (a_is_bid),
    m_px           (a_px),
    m_qty          (a_qty),
    m_filledQty    (0.0),     // Not yet
    m_bmFilledAt   (),        // Not yet either
    m_bmPx         ()
  {
    if (utxx::unlikely
       (IsEmpty(m_ordID) || !(IsPos(m_px) && IsPos(m_qty))))
      throw utxx::badarg_error
            ("LiqStats::Ctor: Invalid arg(s): OrderID=", a_ord_id.data(),
             ", Px=", double(a_px), ", Qty=", double(a_qty));
  }

  //=========================================================================//
  // "LiqStats::WouldBeFilled":                                              //
  //=========================================================================//
  inline bool EConnector_LATOKEN_STP::LiqStats::WouldBeFilled() const
  {
    bool   res =  !m_bmFilledAt.empty();
    assert(res == IsPos(m_bmPx));
    return res;
  }

  //=========================================================================//
  // "UpdateLiqStats" (on a Trade or Order):                                 //
  //=========================================================================//
  void EConnector_LATOKEN_STP::UpdateLiqStats
  (
    SecDefD const&               a_instr,
    H2WS::LATOKEN::MsgOMC const& a_msg,
    utxx::time_val               a_ts_exch   // Latest Event Time
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(a_ts_exch.empty()))
      return;

    // Get or create the inner map corresp to this Instr:
    auto it = m_liqStatsMap.find(&a_instr);
    if (utxx::unlikely(it == m_liqStatsMap.end()))
      it = m_liqStatsMap.insert
           (make_pair(&a_instr, map<utxx::time_val, LiqStats>())).first;

    assert(it != m_liqStatsMap.end() && it->first == &a_instr);
    map<utxx::time_val, LiqStats>&   mapI = it->second; // Ref!

    //-----------------------------------------------------------------------//
    // Process the existing Orders (Oldest -> Newest):                       //
    //-----------------------------------------------------------------------//
    utxx::time_val oldest = a_ts_exch - HistoryInt;

    while(!mapI.empty())
    {
      // Always examine the currently-earliest Order:
      auto            itI = mapI.begin();
      LiqStats const& ls  = itI->second;

      if (itI->first > oldest)
        // This Order is still within  the History Interval, and so are all
        // other ones:
        break;

      // Otherwise: This Order goes out of History Interval. FIXME: We use a
      // Log at the moment -- should use a dedicated DB.
      // We are NOT Interested in Orders which would not be filled on EITHER
      // the BencMark Exg or LATOKEN (they will just be expelled).   For all
      // others:
      bool laTraded =  IsPos(ls.m_filledQty);
      if  (laTraded || ls.WouldBeFilled())
        OutputLiqStats(a_instr, itI->first, a_ts_exch, ls, "Time");

      // In any case, remove the "outdated" Order/LiqStats:
      mapI.erase(itI);
    }
    //-----------------------------------------------------------------------//
    // Process this Msg (if applicable):                                     //
    //-----------------------------------------------------------------------//
    // In "LiqStats", we consider Msgs from Non-MM accounts only; ie both Int-
    // ernal and External MM accounts are excluded:
    bool isIntMM =
      (LATOKEN::APIKeysIntMM.find(a_msg.m_userID) !=
       LATOKEN::APIKeysIntMM.end());

    bool isExtMM =
      (find(LATOKEN::UserIDsExtMM.cbegin(), LATOKEN::UserIDsExtMM.cend(),
            a_msg.m_userID)             !=  LATOKEN::UserIDsExtMM.cend());

    if (isIntMM || isExtMM)
      return;

    //-----------------------------------------------------------------------//
    // Generic Case: Non-MM UserID:                                          //
    //-----------------------------------------------------------------------//
    // Extract the TimeStamp from the OrderID. NB: This is the OriginalTS which
    // is preserved across multiple Events (ie Trades, incl Acks and Fills)  of
    // that Order. Do NOT use the TradeID -- it is NOT persistent:
    //
    utxx::time_val origTS = H2WS::LATOKEN::MkTSFromMsgID(a_msg.m_ordID.data());

    if (utxx::unlikely(origTS.empty()))
      return;

    switch (a_msg.m_ordStatus)
    {
      //---------------------------------------------------------------------//
      case FIX::OrdStatusT::New:
      //---------------------------------------------------------------------//
      {
        // Insert new "LiqStats":
        bool isBid = (a_msg.m_side == FIX::SideT::Buy);
        try
        {
          // NB: Here we use:
          // (*) "OrdID",  not "ID/TrdID", as otherwise we cannot link various
          //     Order states together;
          // (*) "ExecQty" for InitQty, NOT "Amount": the latter is for WS only!
          LiqStats ls
            (a_msg.m_ordID, a_msg.m_userID, isBid, a_msg.m_initPx,
             a_msg.m_execQty);

          // NB: The new "LiqStats" are created according to the OriginalExchTS
          // of the corresp Order:
          DEBUG_ONLY(auto insRes =) mapI.insert(make_pair(origTS, ls));
          assert(insRes.second);

          LOG_INFO(3,
            "EConnector_LATOKEN_STP::UpdateLiqStats: NewOrdID={}: Tracking",
            a_msg.m_ordID.data())
        }
        catch (exception const& exn)
        {
          LOG_ERROR(2,
            "EConnector_LATOKEN_STP::UpdateLiqStats: NewOrdID={}: Failed: {}",
            a_msg.m_ordID.data(), exn.what())
        }
        break;
      }
      //---------------------------------------------------------------------//
      case FIX::OrdStatusT::Filled:
      case FIX::OrdStatusT::Cancelled:
      //---------------------------------------------------------------------//
      {
        // This "LiqStats" are now done, successfully or otherwise:
        bool isFilled = (a_msg.m_ordStatus == FIX::OrdStatusT::Filled);
        auto itI      = mapI.find(origTS);

        if (utxx::unlikely(itI == mapI.end()))
        {
          LOG_WARN(2,
            "EConnector_LATOKEN_STP::UpdateLiqStats: {} OrdID={}, TrdID={}, "
            "OrigTS={}.{}: Not Found",
            isFilled ? "Filled" : "Cancelled",
            a_msg.m_ordID.data(), a_msg.m_id.data(), origTS.sec(),
            origTS.usec())
        }
        else
        {
          // Perform the following update ONLY if the Order  was not done too
          // soon; otherwise, it was an Aggressive or HFT Order, for which we
          // do not maintain the stats -- so we simply drop it:
          assert(itI->first == origTS);
          LiqStats& ls = itI->second;  // Ref!

          if (origTS <= a_ts_exch - AggrSkipInt)
          {
            if (isFilled)
              ls.m_filledQty = ls.m_qty;
            OutputLiqStats
              (a_instr, origTS, a_ts_exch, ls, isFilled ? "Fill" : "Cxl");
          }
          else
            LOG_INFO(3,
              "EConnector_LATOKEN_STP::UpdateLiqStats: {} OrdID={}, TrdID={}, "
              "OrigTS={}.{}: Dropped: Aggressive or HFT Order",
              isFilled ? "Filled" : "Cancelled",
              a_msg.m_ordID.data(), a_msg.m_id.data(), origTS.sec(),
              origTS.usec())

          // In any case, remove this Order/LiqStats:
          mapI.erase(itI);
        }
        break;
      }
      //---------------------------------------------------------------------//
      case FIX::OrdStatusT::PartiallyFilled:
      //---------------------------------------------------------------------//
      {
        // Again, find the "LiqStats". In this case, there are no "too-soon"
        // restrictions for an update, and no output yet:
        auto itI = mapI.find(origTS);

        if (utxx::unlikely(itI == mapI.end()))
        {
          LOG_WARN(2,
            "EConnector_LATOKEN_STP::UpdateLiqStats: PartFilled OrdID={}, "
            "TrdID={}, OrigTS={}.{}: Not Found",
            a_msg.m_ordID.data(), a_msg.m_id.data(), origTS.sec(),
            origTS.usec())
        }
        else
        {
          LiqStats& ls    = itI->second;  // Ref!
          ls.m_filledQty += a_msg.m_execQty;
        }
        break;
      }
      //---------------------------------------------------------------------//
      case FIX::OrdStatusT::Rejected:
      //---------------------------------------------------------------------//
        // It is probably OK to ignore Rejected orders here:
        break;

      //---------------------------------------------------------------------//
      default:
      //---------------------------------------------------------------------//
        // Anything else:
        LOG_WARN(2,
          "EConnector_LATOKEN_STP::UpdateLiqStats: OrderID={}, TrdID={}, "
          "OrigTS={}.{}: UnExpected OrdStatus={}",
          a_msg.m_ordID.data(), a_msg.m_id.data(), origTS.sec(),
          origTS.usec(),   char(a_msg.m_ordStatus))
    }
    // All Done!
  }

  //=========================================================================//
  // "UpdateLiqStats":                                                       //
  //=========================================================================//
  void EConnector_LATOKEN_STP::UpdateLiqStats
  (
    SecDefD const& a_instr,
    bool           a_ref_is_bid,
    PriceT         a_ref_px,
    utxx::time_val a_ts_recv
  )
  const
  {
    // Checks:
    // First of all, RefPx must be valid:
    if (utxx::unlikely(!(IsFinite(a_ref_px) && IsPos(a_ref_px))))
      return;

    // Perform the update for all relevant "LiqStats" with this Instr:
    auto it = m_liqStatsMap.find(&a_instr);
    if (utxx::unlikely(it == m_liqStatsMap.end()))
      return;

    map<utxx::time_val, LiqStats>& mapI = it->second;  // Ref!
    for (auto& p: mapI)
    {
      LiqStats& ls = p.second;  // Ref!
      if (ls.WouldBeFilled())
        continue;               // Would already have been filled

      // In the folloiwing 2 cases, we detect a "would-be-fill" on the Bench-
      // Mark Exchange:
      // (1) "ls" is for a Buy  Order, RefPx is AskPx and it is below the OrdPx;
      // (2) "ls" is for a Sell Order, RefPx is BidPx and it is above the OrdPx:
      //
      if (( ls.m_isBid && !a_ref_is_bid && a_ref_px < ls.m_px) ||
          (!ls.m_isBid &&  a_ref_is_bid && a_ref_px > ls.m_px))
      {
        ls.m_bmFilledAt = a_ts_recv;
        ls.m_bmPx       = a_ref_px;
        assert(ls.WouldBeFilled());
      }
    }
    // All Done!
  }

  //=========================================================================//
  // "OutputLiqStats":                                                       //
  //=========================================================================//
  void EConnector_LATOKEN_STP::OutputLiqStats
  (
    SecDefD  const& a_instr,
    utxx::time_val  a_ts_orig,
    utxx::time_val  a_ts_exch,
    LiqStats const& a_ls,
    char const*     a_evt
  )
  const
  {
    assert(a_evt != nullptr);
    // FIXME: For the moment, we perform output into the Log:
    // Event  (Fill|Cxl|Time)
    // ExchTS (sec.usec)
    // Instr
    // OrdID
    // Bid|Ask
    // PassivePx
    // OrigQty
    // FilledQty
    // PassiveTime (sec)
    // Would have been filled on BenchMark Exch?
    //
    double passTime = (a_ts_exch - a_ts_orig).seconds();
    bool   wbFilled = a_ls.WouldBeFilled();

    LOG_INFO(1,
      "LiqStats,{},{}.{},{},{},{},{},{},{},{},{},{}.{},{}",
      a_evt,  a_ts_exch.sec(),  a_ts_exch.usec(),
      a_instr.m_Symbol.data(),  a_ls.m_ordID.data(),
      a_ls.m_isBid  ?  "Bid" :  "Ask",
      double(a_ls.m_px),        double(a_ls.m_qty),
      double(a_ls.m_filledQty), passTime,
      int(wbFilled),            a_ls.m_bmFilledAt.sec(),
      a_ls.m_bmFilledAt.usec(), wbFilled ? double(a_ls.m_bmPx) : 0.0)
  }
}
// End namespace MAQUETTE
