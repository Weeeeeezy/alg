// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.cpp":       //
//                          H2WS-Based MDC for Binance                       //
//===========================================================================//
#pragma once

#include <sstream>
#include <cstring>
#include <cstdlib>

#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>

#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Protocols/JSONParseMacros.h"

//===========================================================================//
// Find and update stream ID in map                                          //
//===========================================================================//
namespace
{
  using namespace MAQUETTE;

  inline bool FindSymbolInMap
  (
    OrderID                    a_id,
    std::map<SymKey, OrderID>& a_map,
    SymKey&                    a_target
  )
  {
    auto result =
      std::find_if
      (
        a_map.begin(),   a_map.end(),
        [a_id](const auto& mo){ return mo.second == a_id; }
      );

    //RETURN VARIABLE IF FOUND
    if(result != a_map.end())
    {
      a_target = result->first;
      return true;
    }
    else
      return false;
  }
}

namespace MAQUETTE
{
  //-------------------------------------------------------------------------//
  // "OnRstStream":                                                          //
  //-------------------------------------------------------------------------//
  template <Binance::AssetClassT::type AC>
  void EConnector_H2WS_Binance_MDC<AC>::OnRstStream
  (
    H2Conn*        a_h2conn,
    uint32_t       a_stream_id,
    uint32_t       a_err_code,
    utxx::time_val a_ts_recv
  )
  {
    assert(a_h2conn != nullptr);
    LOG_WARN(2, "EConnector_H2WS_Binance_MDC::OnRstStream: "
                "Received RstStream on StreamID={}, ErrCode={}",
                a_stream_id, a_err_code)

    if (utxx::unlikely(a_err_code != 1))
    {
      // ErrCode=1 probably means that Binance resets old streams,
      // but ErrCode=7 means Stream Refused -- restart and request snap
      a_h2conn->template StopNow<false>("OnRstStream", a_ts_recv);
    }
  }

  //=========================================================================//
  // "ProcessH2Msg":                                                         //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_MDC<AC>::ProcessH2Msg
  (
    H2Conn*        a_h2conn,
    uint32_t       a_stream_id,
    char const*    a_msg_body,
    int            a_msg_len,
    bool           UNUSED_PARAM(a_last_msg),
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    assert(a_h2conn != nullptr);

    // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
    assert(a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');
    // Check if the msg is complete (if not, it is a malformed msg or a proto-
    // col-level error):
    CHECK_ONLY
    (
      LOG_INFO(4, "EConnector_H2WS_Binance_OMC::ProcessH2Msg: Received msg={}",
                  a_msg_body)

      assert(a_msg_len > 0);
      if (utxx::unlikely(a_msg_body[a_msg_len-1] != '}'))
      {
        LOG_ERROR(2,
          "EConnector_H2WS_Binance_MDC::ProcessH2Msg: Truncated JSON Obj: {}, "
          "Len={}", a_msg_body, a_msg_len)
        return true;
      }
    )
    // The H2 response should contain a SnapShot. Process it:
    bool ok = FindSymbolInMap
              (a_stream_id, a_h2conn->m_snapStreamID, m_mdMsg.m_symbol);
    if (utxx::likely(ok))
    {
      LOG_INFO(3, "EConnector_H2WS_Binance_MDC::ProcessH2Msg: "
                  "SnapShot received for StreamID={}", a_stream_id)
      ProcessSnapShot(a_msg_body, a_msg_body, a_msg_len, a_ts_recv, a_ts_handl);
    }
    else
      LOG_WARN(3, "EConnector_H2WS_Binance_MDC::ProcessH2Msg: UnExpected "
                  "StreamID={}: Msg={}",   a_stream_id, a_msg_body)
    return true;
  }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Returns "true" to continue receiving msgs, "false" to stop:
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_MDC<AC>::ProcessWSMsg
  (
    char const*     a_msg_body,
    int             a_msg_len,
    bool            UNUSED_PARAM(a_last_msg),
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
    assert(a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    // Check if the msg is complete (if not, it is a malformed msg or a proto-
    // col-level error):
    CHECK_ONLY
    (
      LOG_INFO(4, "EConnector_H2WS_Binance_OMC::ProcessWSMsg: Received msg={}",
                  a_msg_body)

      assert(a_msg_len > 0);
      if (utxx::unlikely(a_msg_body[a_msg_len-1] != '}'))
      {
        LOG_ERROR(2,
          "EConnector_H2WS_Binance_MDC::ProcessWSMsg: Truncated JSON Obj: {}, "
          "Len={}", a_msg_body, a_msg_len)
        return false;
      }
    )

    try
    {
      //---------------------------------------------------------------------//
      // Parse the JSON Text:                                                //
      //---------------------------------------------------------------------//
      // XXX: Using a hand-written JSON parser here, which relies on a fixed
      // ordering of elemets:
      // We will have to install 0-terminators of sub-strings in "a_msg_body"
      // during parsing, so:
      char const* curr = a_msg_body;

      if (utxx::unlikely(strncmp(curr, "{\"result\":", 10)) == 0) {
        curr += 10;
        if (strncmp(curr, "null", 4) != 0)
          throw utxx::runtime_error("Got non-null result");
      } else if (utxx::unlikely(strncmp(curr, "{\"code\":", 8)) == 0) {
        throw utxx::runtime_error("Got error message");
      } else {
        if ((strncmp(curr, "{\"e\":\"bookTicker\"", 17) == 0) ||
            (strncmp(curr, "{\"u\"", 4) == 0))
          return
            ProcessBBA(curr, a_msg_body, a_msg_len, a_ts_recv, a_ts_handl);

        if (strncmp(curr, "{\"e\":\"depthUpdate\"", 18) == 0)
          return
            ProcessUpdate(curr, a_msg_body, a_msg_len, a_ts_recv, a_ts_handl);

        if (strncmp(curr, "{\"e\":\"aggTrade\",", 16) == 0)
          return
            ProcessAggrTrade(curr, a_msg_body, a_msg_len, a_ts_recv, a_ts_handl);

        if (strncmp(curr, "{\"e\":\"trade\",", 13) == 0)
          return
            ProcessTrade(curr, a_msg_body, a_msg_len, a_ts_recv, a_ts_handl);

        UTXX_THROW_RUNTIME_ERROR("Unknown feed.");
      }
    }
    catch (std::exception const& exn)
    {
      //---------------------------------------------------------------------//
      // Any Exception:                                                      //
      //---------------------------------------------------------------------//
      // Log it but continue reading and processing (because all SnapShots and
      // Trades are independent from each other -- there would be no inconsist-
      // encies):
      LOG_ERROR(1,
        "EConnector_MktData_Binance_MDC::ProcessWSMsg: {}: EXCEPTION: {}",
        a_msg_body, exn.what())
    }
    //-----------------------------------------------------------------------//
    // In any case: Continue:                                                //
    //-----------------------------------------------------------------------//
    return true;
  }

  //=========================================================================//
  // Parsers                                                                 //
  //=========================================================================//
  //=========================================================================//
  // "ProcessUpdate":                                                        //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_MDC<AC>::ProcessUpdate
  (
    char const*    a_curr,
    char const*    a_msg_body,
    int            a_msg_len,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    m_mdMsg.m_nEntrs = m_mdMsg.MaxMDEs;   // NB: Bids + Asks!
    char const* curr = a_curr;
    char const* end  = a_msg_body + a_msg_len;
    SKP_STR("{\"e\":\"depthUpdate\",\"E\":")

    unsigned long lastUpdateID = m_lastUpdateID[m_mdMsg.m_symbol];

    // E: Event time
    unsigned long eventTime = 0;
    curr = utxx::fast_atoi<unsigned long, false>(curr, end, eventTime);

    // s: Symbol
    curr = strstr(curr, "\"s\":\"") + 5;
    GET_STR(symbol)
    InitFixedStr(&m_mdMsg.m_symbol, symbol);

    // U: First updID of the pack
    curr = strstr(curr, "\"U\":") + 4;
    unsigned long firstID = 0;
    curr = utxx::fast_atoi<unsigned long, false>(curr, end, firstID);

    // u: Last updID of the pack
    curr = strstr(curr, "\"u\":") + 4;
    unsigned long lastID = 0;
    curr = utxx::fast_atoi<unsigned long, false>(curr, end, lastID);

    // CHECKS
    if (lastID <= lastUpdateID)
      // This update is from the past
      return true;
    else
    if (firstID <= lastUpdateID + 1 && lastID >= lastUpdateID + 1)
      // This is the correct FIRST upd for a snapshot
      m_updateMissedNotify = false;
    else
    if (firstID > lastUpdateID + 1  &&
        lastUpdateID > 0            &&
        m_updateMissedNotify)
    {
      // FIXME: missed first update
      // For now, I guess it's ok
      m_updateMissedNotify = false;
      LOG_ERROR(1,
        "EConnector_H2WS_Binance_MDC::Process Update: "
        "Missed an update: LastUpdateID={}, U={}, u={}",
        lastUpdateID, firstID, lastID)
        m_updateMissedNotify = false;
        // Request another snapshot
        // RequestSnapShots();
        // return true;
    }

    // In case of Futures there are extra fields to be skipped
    if constexpr (IsFut)
      curr = strstr(curr, ",\"b\"");

    // Bids (b==1) and Asks (b==0) Arrays:
    int off = 0;
    for (int  b = 1; b >= 0; --b)
    {
      // Key: "bids" or "asks":
      bool isBid(b);
      if  (isBid)
      {
        SKP_STR(",\"b\":[")
      }
      else
      {
        SKP_STR(",\"a\":[")
      }

      if (utxx::unlikely(*curr == ']'))
      {
        ++curr;
        continue;
      }
      ++curr;

      for (int i = 0; i < m_mdMsg.MaxMDEs; ++i)
      {
        // The weird processing is due to Binance missing an occasional
        // quotation mark:
        if (*curr == '\"')
          ++curr;

        double px  = NaN<double>;
        curr = utxx::atof(curr, end, px);
        if (*curr == '\"')
          ++curr;
        assert(*curr == ',');
        ++curr;
        if (*curr == '\"')
          ++curr;

        double qty = NaN<double>;
        curr = utxx::atof(curr, end, qty);
        if (*curr == '\"')
          ++curr;
        assert(*curr == ']');

        // Check what we got:
        CHECK_ONLY
        (
          if (utxx::unlikely(!(px > 0.0 && qty >= 0.0)))
          {
            LOG_WARN(2,
              "EConnector_H2WS_Binance_MDC::ProcessUpdate: {}: Invalid Px={} "
              "or Qty={}: SnapShot skipped", a_msg_body, px, qty)
            // Skip the entire msg (as the SnapShot is invalid), but still
            // continue reading:
            return true;
          }
        )
        // Save the "px" and "qty":
        MDEntryST&  mde = m_mdMsg.m_entries[off]; // REF!
        mde.m_entryType =
          isBid ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer;
        mde.m_px        = PriceT(px);
        mde.m_qty       = QtyN  (qty);
        ++off;

        // Next: It could be either next Entry, or Side Done:
        if (utxx::unlikely(strncmp(curr, "]]", 2) == 0))
        {
          curr += 2;
          break;  // Yes, Side Done
        }

        // If not done yet:
        SKP_STR("],[")
      }
    }
    // At the end:
    m_mdMsg.m_nEntrs = off;
    SKP_STR ("}")
    assert(curr == end);

    //-------------------------------------------------------------------//
    // SnapShot done, process it in "EConnector_MktData":                //
    //-------------------------------------------------------------------//
    // NB: "UpdateOrderBooks" takes care of notifying the Strategies:
    //
    CHECK_ONLY(bool ok =) EConnector_MktData::UpdateOrderBooks
    <
      false,    // !IsSnapShot
      IsMultiCast,
      true,
      WithOrdersLog,
      WithRelaxedOBs,
      ChangeIsPartFill,
      NewEncdAsChange,
      ChangeEncdAsNew,
      IsFullAmt,
      IsSparse,
      EConnector_MktData::FindOrderBookBy::Symbol,
      QT,
      QR,
      OMC
    >
    (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl);

    CHECK_ONLY
    (
      if (utxx::unlikely(!ok))
      {
        LOG_WARN(2,
          "EConnector_H2WS_Binance_MDC::ProcessUpdate: {}: OrderBook Update "
          "Failed", ToCStr(m_mdMsg.m_symbol))
        // Nothing else to do here -- Strategy mgmt already done!
      }
    )

    a_curr = curr;
    return true;
  }

  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_MDC<AC>::ProcessBBA
  (
    char const*    a_curr,
    char const*    a_msg_body,
    int            a_msg_len,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    m_mdMsg.m_nEntrs = 2;
    char const* curr = a_curr;
    char const* end  = a_msg_body + a_msg_len;

    SKP_STR("{")

    if constexpr (IsFut && !IsFutC)
      SKP_STR("\"e\":\"bookTicker\",")

    SKP_STR("\"u\":")
    curr = std::find(curr, end, ',');

    if constexpr (IsFutC)
      SKP_STR(",\"e\":\"bookTicker\"")

    SKP_STR(",\"s\":\"")
    GET_STR(symbol)
    InitFixedStr(&m_mdMsg.m_symbol, symbol);

    if constexpr (IsFutC)
    {
      SKP_STR(",\"ps\":\"")
      curr = std::find(curr, end, ',');
    }

    char* after = nullptr;
    {
      SKP_STR(",\"b\":\"")
      MDEntryST& bid = m_mdMsg.m_entries[0];
      bid.m_entryType = FIX::MDEntryTypeT::Bid;
      bid.m_px = PriceT(strtod(curr, &after));
      curr = after;
      SKP_STR("\",\"B\":\"")
      bid.m_qty = QtyN(strtod(curr, &after));
      curr = after;
    }
    {
      SKP_STR("\",\"a\":\"")
      MDEntryST& ask = m_mdMsg.m_entries[1];
      ask.m_entryType = FIX::MDEntryTypeT::Offer;
      ask.m_px = PriceT(strtod(curr, &after));
      curr = after;
      SKP_STR("\",\"A\":\"")
      ask.m_qty = QtyN(strtod(curr, &after));
      curr = after;
    }

    if constexpr (IsFut) {
      curr = strchr(curr, '}');
      ++curr;
    }
    else
      SKP_STR("\"}")

    assert(curr == end);

    CHECK_ONLY(bool ok =) EConnector_MktData::UpdateOrderBooks
    <
      true,
      IsMultiCast,
      false,
      WithOrdersLog,
      WithRelaxedOBs,
      ChangeIsPartFill,
      NewEncdAsChange,
      ChangeEncdAsNew,
      IsFullAmt,
      IsSparse,
      EConnector_MktData::FindOrderBookBy::Symbol,
      QT,
      QR,
      OMC
    >
    (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl);

    CHECK_ONLY
    (
      if (utxx::unlikely(!ok))
      {
        LOG_WARN(2,
          "EConnector_H2WS_Binance_MDC::ProcessBBA: {}: OrderBook Update "
          "Failed", ToCStr(m_mdMsg.m_symbol))
      }
    )
    a_curr = curr;
    return true;
  }

  //=========================================================================//
  // "ProcessTrade":                                                         //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_MDC<AC>::ProcessTrade
  (
    char const*    a_curr,
    char const*    a_msg_body,
    int            a_msg_len,
    utxx::time_val a_ts_recv,
    utxx::time_val UNUSED_PARAM(a_ts_handl)
  )
  {
    char const* curr = a_curr;
    char const* end  = a_msg_body + a_msg_len;
    m_mdMsg.m_nEntrs = 1;
    MDEntryST&   mde = m_mdMsg.m_entries[0];

    SKP_STR("{\"e\":\"trade\",\"E\":")

    // "E":EventTime, in msec from the Epoch; not very useful, as we also
    // receive the Trade (Aggression) time below; so skip it:
    unsigned long eventTime = 0;
    curr = utxx::fast_atoi<unsigned long, false>(curr, end, eventTime);

    // "s": Symbol
    SKP_STR(",\"s\":\"")
    GET_STR(symbol)
    InitFixedStr(&m_mdMsg.m_symbol, symbol);

    // "t": tradeID
    SKP_STR(",\"t\":")
    curr = utxx::fast_atoi<OrderID, false>(curr, end, mde.m_execID);

    // "p":Px, a floating-point number in ""s:
    SKP_STR(",\"p\":\"")
    char*  after = nullptr;
    mde.m_px     = PriceT(strtod(curr, &after));
    curr         = after;

    // "q":Qty (QtyA in this case), again in ""s:
    SKP_STR("\",\"q\":\"")
    mde.m_qty    = QtyN  (strtod(curr, &after));
    curr         = after;

    // "b":Buyer OrderID: Currently ignored:
    SKP_STR("\",\"b\":")
    OrderID b  = 0;
    curr       = utxx::fast_atoi<OrderID, false>(curr, end, b);

    // "a":Seller OrderID: Currently ignored:
    SKP_STR(",\"a\":")
    OrderID a  = 0;
    curr       = utxx::fast_atoi<OrderID, false>(curr, end, a);

    // "T":AggressionTime (in msec from the Epoch):
    SKP_STR(",\"T\":")
    unsigned long  T = 0;
    curr             = utxx::fast_atoi<unsigned long, false>(curr, end, T);
    m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(T));

    // "m":BuyerIsMarketMaker:
    // If true, it means that PassiveSide=Buyer, ie Aggressor=Seller(Offer).
    // Otherwise, Aggressor=Buyer(Bid):
    SKP_STR (",\"m\":")
    GET_BOOL(sellAggr)

    mde.m_entryType     = FIX::MDEntryTypeT::Trade;
    FIX::SideT aggrSide =
       sellAggr ? FIX::SideT::Sell         : FIX::SideT::Buy;

    // "M":ReservedBoolean:
    SKP_STR (",\"M\":")
    GET_BOOL(reserved)
    SKP_STR ("}")
    assert(curr == end);

    //-----------------------------------------------------------------------//
    // Now Process this Trade:                                               //
    //-----------------------------------------------------------------------//
    // Get the OrderBook for this AltSymbol -- it will be needed for sending
    // notifications to the subscribed Strategies:
    //
    OrderBook const* ob =
      EConnector_MktData::FindOrderBook<false>(ToCStr(m_mdMsg.m_symbol));
    CHECK_ONLY
    (
      if (utxx::unlikely(ob == nullptr))
      {
        LOG_WARN(2,
          "EConnector_H2WS_Binance_MDC::ProcessTrade: No OrderBook for {}",
          ToCStr(m_mdMsg.m_symbol))
      }
    else
    )
    // Generic Case: NB: WithOrdersLog=false:
    EConnector_MktData::ProcessTrade<false, QT, QR, OMC>
      (m_mdMsg, mde, nullptr, *ob, mde.m_qty, aggrSide, a_ts_recv);

    a_curr = curr;
    return true;
  }

  //=========================================================================//
  // "ProcessAggrTrade":                                                     //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_MDC<AC>::ProcessAggrTrade
  (
    char const*    a_curr,
    char const*    a_msg_body,
    int            a_msg_len,
    utxx::time_val a_ts_recv,
    utxx::time_val UNUSED_PARAM(a_ts_handl)
  )
  {
    char const* curr = a_curr;
    char const* end  = a_msg_body + a_msg_len;
    m_mdMsg.m_nEntrs = 1;
    MDEntryST&   mde = m_mdMsg.m_entries[0];

    SKP_STR("{\"e\":\"aggTrade\",\"E\":")

    // "E":EventTime, in msec from the Epoch; not very useful, as we also
    // receive the Trade (Aggression) time below; so skip it:
    unsigned long eventTime = 0;
    curr = utxx::fast_atoi<unsigned long, false>(curr, end, eventTime);

    //-----------------------------------------------------------------------//
    // DIFFERENT PROCESSING FOR FUTURES AND SPOT                             //
    //-----------------------------------------------------------------------//
    // For Margin and Futures "a" and "s" are in reverse order
    if constexpr (!IsFut)
    {
      // "s":Symbol
      SKP_STR(",\"s\":\"")
      GET_STR(symbol)
      InitFixedStr(&m_mdMsg.m_symbol, symbol);

      // "a":AggrTradeID:
      SKP_STR(",\"a\":")
      curr = utxx::fast_atoi<OrderID, false>(curr, end, mde.m_execID);
    }
    else
    {
      // "a":AggrTradeID:
      SKP_STR(",\"a\":")
      curr = utxx::fast_atoi<OrderID, false>(curr, end, mde.m_execID);

      // "s":Symbol; should be the upper-case equivalent of "altSymbol":
      SKP_STR(",\"s\":\"")
      GET_STR(symbol)
      InitFixedStr(&m_mdMsg.m_symbol, symbol);
    }

    // "p":Px
    // (XXX: Since it is an Aggregate Trade resulting from a single Aggress-
    // sion, the Px, presumably, is a VWAP of individual matches). NB: It is
    // a floating-point number in ""s:
    SKP_STR(",\"p\":\"")
    char* after = nullptr;
    mde.m_px    = PriceT(strtod(curr, &after));
    curr        = after;

    // "q":Qty (QtyA in this case), again in ""s:
    SKP_STR("\",\"q\":\"")
    mde.m_qty   = QtyN  (strtod(curr, &after));
    curr        = after;

    // "f":FirstMatchID: Currently ignored:
    SKP_STR("\",\"f\":")
    OrderID f   = 0;
    curr        = utxx::fast_atoi<OrderID, false>(curr, end, f);

    // "l":LastMatchID:  Currently ignored (only checked against "fID"):
    SKP_STR(",\"l\":")
    OrderID l   = 0;
    curr        = utxx::fast_atoi<OrderID, false>(curr, end, l);
    assert(f <= l);

    // "T":AggressionTime (in msec from the Epoch):
    SKP_STR(",\"T\":")
    unsigned long  T = 0;
    curr             = utxx::fast_atoi<unsigned long, false>(curr, end, T);
    m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(T));

    // "m":BuyerIsMarketMaker:
    // If true, it means that PassiveSide=Buyer, ie Aggressor=Seller(Offer).
    // Otherwise, Aggressor=Buyer(Bid):
    SKP_STR (",\"m\":")
    GET_BOOL(sellAggr)

    mde.m_entryType = FIX::MDEntryTypeT::Trade;
    FIX::SideT aggrSide = sellAggr ? FIX::SideT::Sell : FIX::SideT::Buy;

    if constexpr (!IsFut)
    {
      // "M":ReservedBoolean:
      SKP_STR (",\"M\":")
      GET_BOOL(reserved)
    }

    SKP_STR ("}")
    assert(curr == end);

    //-----------------------------------------------------------------------//
    // Now Process this Trade:                                               //
    //-----------------------------------------------------------------------//
    // Get the OrderBook for this AltSymbol -- it will be needed for sending
    // notifications to the subscribed Strategies:
    //
    OrderBook const* ob =
     EConnector_MktData::FindOrderBook<false>(ToCStr(m_mdMsg.m_symbol));
    CHECK_ONLY
    (
      if (utxx::unlikely(ob == nullptr))
      {
        LOG_WARN(2,
          "EConnector_H2WS_Binance_MDC::ProcessAggrTrade: No OrderBook for {}",
          ToCStr(m_mdMsg.m_symbol))
      }
    else
    )
    // Generic Case: NB: WithOrdersLog=false:
    EConnector_MktData::ProcessTrade<false, QT, QR, OMC>
        (m_mdMsg, mde, nullptr, *ob, mde.m_qty, aggrSide, a_ts_recv);

    a_curr = curr;
    return true;
  }

  //=========================================================================//
  // "ProcessSnapShot":                                                      //
  //=========================================================================//
  template <Binance::AssetClassT::type AC>
  bool EConnector_H2WS_Binance_MDC<AC>::ProcessSnapShot
  (
    char const*     a_curr,
    char const*     a_msg_body,
    int             a_msg_len,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    m_mdMsg.m_nEntrs = m_mdMsg.MaxMDEs;   // NB: Bids + Asks!
    char const* curr = a_curr;
    char const* end  = a_msg_body + a_msg_len;

    if (strncmp(curr, ",\"data", 6) == 0)
      // WS stream
      SKP_STR(",\"data\":{\"lastUpdateId\":")
    else
      // H2 snapshot
      SKP_STR("{\"lastUpdateId\":")

    // SeqNum (LastUpdateID):
    unsigned long seqNum = 0;
    curr = utxx::fast_atoi<unsigned long, false>(curr, end, seqNum);

    // Update lastUpdateID for symbol
    m_lastUpdateID[m_mdMsg.m_symbol] = seqNum;
    m_updateMissedNotify = true;

    if constexpr (IsFut)
      // In Futures API, there are E and T fields here
      curr = strstr(curr, ",\"bids");

    // Bids (b==1) and Asks (b==0) Arrays:
    int off = 0;
    for (int  b = 1; b >= 0; --b)
    {
      // Key: "bids" or "asks":
      bool isBid(b);
      if  (isBid)
        SKP_STR(",\"bids\":[[")
      else
        SKP_STR(",\"asks\":[[")

      for (int i = 0; i < m_mdMsg.MaxMDEs; ++i)
      {
        if (*curr == '\"')
          ++curr;

        double px  = NaN<double>;
        curr = utxx::atof(curr, end, px);
        if (*curr == '\"')
          ++curr;
        assert(*curr == ',');
        ++curr;
        if (*curr == '\"')
          ++curr;

        double qty = NaN<double>;
        curr = utxx::atof(curr, end, qty);
        if (*curr == '\"')
          ++curr;
        assert(*curr == ']');

        // Check what we got:
        CHECK_ONLY
        (
          if (utxx::unlikely(!(px > 0.0 && qty > 0.0)))
          {
           LOG_WARN(2,
              "EConnector_H2WS_Binance_MDC::ProcessSnapShot: {}: Invalid Px={}"
              " or Qty={}: SnapShot skipped", a_curr, px, qty)
            // Skip the entire msg (as the SnapShot is invalid), but still
            // continue reading:
            return true;
          }
        )
        // Save the "px" and "qty":
        MDEntryST&  mde = m_mdMsg.m_entries[off]; // REF!
        mde.m_entryType =
          isBid ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer;
        mde.m_px        = PriceT(px);
        mde.m_qty       = QtyN  (qty);
        ++off;

        // Next: It could be either next Entry, or Side Done:
        if (utxx::unlikely(strncmp(curr, "]]", 2) == 0))
        {
          curr += 2;
          break;  // Yes, Side Done
        }
        // If not done yet:
        SKP_STR("],[")
      }
    }
    // At the end:
    m_mdMsg.m_nEntrs = off;
    SKP_STR("}")
    assert(curr == end);

    //-------------------------------------------------------------------//
    // SnapShot done, process it in "EConnector_MktData":                //
    //-------------------------------------------------------------------//
    // NB: "UpdateOrderBooks" takes care of notifying the Strategies:
    //
    CHECK_ONLY(bool ok =) EConnector_MktData::UpdateOrderBooks
    <
      true,    // IsSnapShot
      IsMultiCast,
      WithIncrUpdates,
      WithOrdersLog,
      WithRelaxedOBs,
      ChangeIsPartFill,
      NewEncdAsChange,
      ChangeEncdAsNew,
      IsFullAmt,
      IsSparse,
      EConnector_MktData::FindOrderBookBy::Symbol,
      QT,
      QR,
      OMC
    >
    (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl);  // DynInitMode=false

    CHECK_ONLY
    (
      if (utxx::unlikely(!ok))
      {
        LOG_WARN(2,
          "EConnector_H2WS_Binance_MDC::ProcessSnapShot: {}: OrderBook Update "
          "Failed", ToCStr(m_mdMsg.m_symbol))
        // Nothing else to do here -- Strategy mgmt already done!
      }
    )

    a_curr = curr;
    return true;
  }
} // End namespace MAQUETTE
