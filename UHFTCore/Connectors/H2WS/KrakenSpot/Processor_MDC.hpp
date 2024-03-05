// vim:ts=2:et
//===========================================================================//
//        "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_MDC.cpp":     //
//                          WS-Based MDC for KrakenSpot                      //
//===========================================================================//
#pragma once

#include <sstream>
#include <cstring>
#include <cstdlib>

#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>

#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_MDC.h"
#include "Protocols/JSONParseMacros.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Returns "true" to continue receiving msgs, "false" to stop:
  bool EConnector_WS_KrakenSpot_MDC::ProcessWSMsg
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
      LOG_INFO(4, "EConnector_WS_KrakenSpot_OMC::ProcessWSMsg: Received msg={}",
                  a_msg_body)

      LogMsg<false>(a_msg_body, nullptr, 0);

      assert(a_msg_len > 0);
      const char * last = a_msg_body + (a_msg_len - 1);
      if (utxx::unlikely((*last != '}') && (*last != ']')))
      {
        LOG_ERROR(2,
          "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: Truncated JSON Obj: {}, "
          "Len={}", a_msg_body, a_msg_len)
        return false;
      }
    )

    //---------------------------------------------------------------------//
    // Parse the JSON Text:                                                //
    //---------------------------------------------------------------------//
    // XXX: Using a hand-written JSON parser here, which relies on a fixed
    // ordering of elemets:
    // We will have to install 0-terminators of sub-strings in "a_msg_body"
    // during parsing, so:
    char const* curr = a_msg_body;
    char* end = const_cast<char*>(a_msg_body + a_msg_len);
    bool ok = true;

    try
    {
      //---------------------------------------------------------------------//
      // Check if it is MktData or some one-off Event:                       //
      //---------------------------------------------------------------------//
      if (utxx::likely(a_msg_body[0] == '[' && a_msg_body[a_msg_len-1] == ']'))
      {
        //-------------------------------------------------------------------//
        // MKtData:                                                          //
        //-------------------------------------------------------------------//
        // First of all, get the ChannelID (TillEOL=false):
        int chID = 0;
        curr = utxx::fast_atoi<int, false>(a_msg_body + 1, end, chID);

        CHECK_ONLY
        (
          if (utxx::unlikely(chID < 0 || *curr != ','))
          {
            LOG_ERROR(2,
              "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: MalFormatted "
              "ChannelID: After={} in Msg={}", curr, a_msg_body)
            return false;
          }
        )
        ++curr;

        // Get the OrderBook:
        auto ob = m_channels.at(chID);
        if (utxx::likely(ob != nullptr))
        {
          InitFixedStr(&m_mdMsg.m_symbol, m_symbols.at(chID));

          if (utxx::unlikely(*curr == '[')) {
            // this is a trade
            if (utxx::likely(ob->IsInitialised()))
              ok = this->ProcessTrade(
                curr, a_msg_body, a_msg_len, ob, a_ts_recv, a_ts_handl);
          } else if (*curr == '{') {
            if (m_useBBA) {
              ok = this->ProcessBBA(
                curr, a_msg_body, a_msg_len, ob, a_ts_recv, a_ts_handl);
            } else {
              ok = utxx::likely(ob->IsInitialised())
                   ? this->ProcessBook<false>(
                      curr, a_msg_body, a_msg_len, ob, a_ts_recv, a_ts_handl)
                   : this->ProcessBook<true>(
                      curr, a_msg_body, a_msg_len, ob, a_ts_recv, a_ts_handl);
            }
          } else {
            LOG_ERROR(2,
              "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: Unrecognized "
              "message in channel {}, Msg={}", chID, a_msg_body)
            return false;
          }
        }
        else
        {
          LOG_ERROR(2,
            "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: Invalid ChannelID={} in "
            "Msg={}", chID, a_msg_body)
          return false;
        }
      }
      else
      {
        //-------------------------------------------------------------------//
        // Some Event:                                                       //
        //-------------------------------------------------------------------//
        CHECK_ONLY
        (
          if (utxx::unlikely
             (a_msg_body[0] != '{' || a_msg_body[a_msg_len-1] != '}'))
          {
            LOG_ERROR(2,
              "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: MalFormatted Event in "
              "Msg={}", a_msg_body)
            return false;
          }
        )
        // What kind of Event is it?
        if (strncmp(a_msg_body, "{\"event\":\"heartbeat\"}", 21) == 0)
        {
          // nothing to do
        }
        else
        if (strncmp(a_msg_body, "{\"channelID\":", 12) == 0)
        {
          //-----------------------------------------------------------------//
          // Subscription confirmed:                                         //
          //-----------------------------------------------------------------//
          // Get channel id
          SKP_STR("{\"channelID\":")

          // TillEOL=false:
          int chID = 0;
          curr = utxx::fast_atoi<int, false>(curr, end, chID);
          CHECK_ONLY
          (
            if (utxx::unlikely(chID < 0))
            {
              LOG_ERROR(2,
                "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: Invalid ChannelID "
                "in Msg={}", a_msg_body)
              return false;
            }
          )

          SKP_STR(",\"channelName\":")
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"event\":\"subscriptionStatus\",\"pair\":\"")
          GET_STR(symbol)

          // Find the OrderBook for this Symbol (UseAltSymbol=false):
          OrderBook* ob =
            EConnector_MktData::FindOrderBook<false>(symbol);
          CHECK_ONLY
          (
            if (utxx::unlikely(ob == nullptr))
            {
              LOG_ERROR(2,
                "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: OrderBook Not Found"
                ": Symbol={} in Msg={}", symbol, a_msg_body)
              return false;
            }
          )

          // Finally, map ChannelID => OrderBook:
          m_channels.insert({chID, ob});
          m_symbols.insert({chID, MkSymKey(symbol)});
          LOG_INFO(1,
            "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: Subscription Confirmed "
            "for {}: ChannelID={}", ob->GetInstr().m_FullName.data(), chID)
        }
        else
        if (strstr(a_msg_body, "\"event\":\"error\"") != nullptr)
        {
          //-----------------------------------------------------------------//
          // Some Error: Log it and Exit:                                    //
          //-----------------------------------------------------------------//
          LOG_ERROR(1,
            "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: Got {}", a_msg_body)
          ok = false;
        }
        else
          //-----------------------------------------------------------------//
          // Any other Event -- just log it:                                 //
          //-----------------------------------------------------------------//
          LOG_INFO(3,
            "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: {}", a_msg_body)
      }
    }
    //-----------------------------------------------------------------------//
    // Exception Handling:                                                   //
    //-----------------------------------------------------------------------//
    catch (std::exception const& exn)
    {
      LOG_ERROR(2, "EConnector_WS_KrakenSpot_MDC::ProcessWSMsg: {}, Msg={}",
                exn.what(), a_msg_body)
      ok = false;
    }
    // In any case: Restore the original trailing char and continue:
    return ok;
  }

  //=========================================================================//
  // Parsers                                                                 //
  //=========================================================================//
  //=========================================================================//
  // "ProcessTrade":                                                         //
  //=========================================================================//
  bool EConnector_WS_KrakenSpot_MDC::ProcessTrade
  (
    char const*     a_curr,
    char const*     a_msg_body,
    int             a_msg_len,
    OrderBook*      a_ob,
    utxx::time_val  a_ts_recv,
    utxx::time_val  UNUSED_PARAM(a_ts_handl)
  )
  {
    char const* curr = a_curr;

    assert(*curr == '[');
    ++curr;

    m_mdMsg.m_nEntrs = 0;
    MDEntryST& mde = m_mdMsg.m_entries[0];
    mde.m_entryType = FIX::MDEntryTypeT::Trade;

    while (curr != nullptr) {
      SKP_STR("[\"")
      char*  after = nullptr;
      mde.m_px     = PriceT(strtod(curr, &after));
      assert(after != nullptr);
      curr         = after;

      SKP_STR("\",\"")
      mde.m_qty    = QtyN  (strtod(curr, &after));
      assert(after != nullptr);
      curr         = after;

      SKP_STR("\",\"")
      m_mdMsg.m_exchTS = utxx::secs(strtod(curr, &after));
      assert(after != nullptr);
      curr         = after;

      SKP_STR("\",\"")
      assert((*curr == 'b') || (*curr == 's'));
      FIX::SideT aggrSide =
        (*curr == 's') ? FIX::SideT::Sell : FIX::SideT::Buy;

      EConnector_MktData::ProcessTrade<false, QT, QR, OMC>(
        m_mdMsg, mde, nullptr, *a_ob, mde.m_qty, aggrSide, a_ts_recv);

      // move on to next trade (if any)
      curr = strchr(curr, '[');
    }

    return true;
  }

  //=========================================================================//
  // "ProcessBBA":                                                           //
  //=========================================================================//
  bool EConnector_WS_KrakenSpot_MDC::ProcessBBA
  (
    char const*     UNUSED_PARAM(a_curr),
    char const*     UNUSED_PARAM(a_msg_body),
    int             UNUSED_PARAM(a_msg_len),
    OrderBook*      UNUSED_PARAM(a_ob),
    utxx::time_val  UNUSED_PARAM(a_ts_recv),
    utxx::time_val  UNUSED_PARAM(a_ts_handl)
  )
  {
    // NB: Looks like Kraken does not send complete data over ticker,
    // don't implement it
    throw utxx::runtime_error("Don't use BBA for Kraken, incomplete data");

    // assert(a_ob != nullptr);

    // m_mdMsg.m_nEntrs = 2;
    // char const* curr = a_curr;
    // char* after = nullptr;

    // SKP_STR("{\"a\":[\"")
    // MDEntryST& ask = m_mdMsg.m_entries[0];
    // ask.m_entryType = FIX::MDEntryTypeT::Offer;
    // ask.m_px = PriceT(strtod(curr, &after));
    // assert(after != nullptr);
    // curr = after;

    // SKP_STR("\",")
    // curr = SkipNFlds<1>(curr);
    // SKP_STR("\"")
    // ask.m_qty = QtyN(strtod(curr, &after));
    // assert(after != nullptr);
    // curr = after;

    // SKP_STR("\"],\"b\":[\"")
    // MDEntryST& bid = m_mdMsg.m_entries[1];
    // bid.m_entryType = FIX::MDEntryTypeT::Bid;
    // bid.m_px = PriceT(strtod(curr, &after));
    // assert(after != nullptr);
    // curr = after;

    // SKP_STR("\",")
    // curr = SkipNFlds<1>(curr);
    // SKP_STR("\"")
    // bid.m_qty = QtyN(strtod(curr, &after));
    // assert(after != nullptr);

    // CHECK_ONLY(bool ok =) EConnector_MktData::UpdateOrderBooks
    // <
    //   true,
    //   IsMultiCast,
    //   false,
    //   WithOrdersLog,
    //   WithRelaxedOBs,
    //   ChangeIsPartFill,
    //   NewEncdAsChange,
    //   ChangeEncdAsNew,
    //   IsFullAmt,
    //   IsSparse,
    //   EConnector_MktData::FindOrderBookBy::Symbol,
    //   QT,
    //   QR,
    //   OMC
    // >
    // (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl, nullptr, a_ob);

    // CHECK_ONLY
    // (
    //   if (utxx::unlikely(!ok))
    //   {
    //     LOG_WARN(2,
    //       "EConnector_WS_KrakenSpot_MDC::ProcessBBA: {}: OrderBook Update "
    //       "Failed", ToCStr(m_mdMsg.m_symbol))
    //   }
    // )

    return true;
  }

  //=========================================================================//
  // "ProcessSnapShot":                                                      //
  //=========================================================================//
  template<bool IsSnapShot>
  bool EConnector_WS_KrakenSpot_MDC::ProcessBook
  (
    char const*     a_curr,
    char const*     a_msg_body,
    int             a_msg_len,
    OrderBook*      a_ob,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    m_mdMsg.m_nEntrs = m_mdMsg.MaxMDEs;   // NB: Bids + Asks!
    m_mdMsg.m_exchTS = utxx::secs(0);

    char const* curr = a_curr;
    char const* end  = a_msg_body + a_msg_len;

    SKP_STR("{\"")

    // If this is a snapshot, we have asks and bids, otherwise we may have
    // both asks and bids, only asks, or only bids
    // 0: ask, 1: bid
    char start = IsSnapShot ? 0 : ((*curr == 'a') ? 0 : 1);

    // skip [a|b]s for snapshot or [a|b] for update
    if constexpr (IsSnapShot)
      curr += 2;
    else
      curr += 1;

    SKP_STR("\":[[\"")

    // Bids (b==1) and Asks (b==0) Arrays:
    int off = 0;
    for (char b = start; b < 2; ++b)
    {
      for (int i = 0; i < m_mdMsg.MaxMDEs; ++i)
      {
        double px  = NaN<double>;
        curr = utxx::atof(curr, end, px);
        SKP_STR("\",\"")

        double qty = NaN<double>;
        curr = utxx::atof(curr, end, qty);
        SKP_STR("\",\"")

        double timestamp = 0.0;
        curr = utxx::atof(curr, end, timestamp);
        utxx::time_val this_time = utxx::secs(timestamp);
        m_mdMsg.m_exchTS = std::max(m_mdMsg.m_exchTS, this_time);

        // skip potential republished update flag
        curr = strchr(curr, ']');
        assert(curr != nullptr);

        // Check what we got:
        CHECK_ONLY
        (
          if (utxx::unlikely
             (!(px > 0.0 && (qty > 0 || (!IsSnapShot && qty >= 0)))))
          {
           LOG_WARN(2,
              "EConnector_WS_KrakenSpot_MDC::ProcessSnapShot: {}: Invalid Px={}"
              " or Qty={}: {} skipped", a_curr, px, qty,
              IsSnapShot ? "Snapshot" : "Update")
            // Skip the entire msg (as the SnapShot is invalid), but still
            // continue reading:
            return true;
          }
        )
        // Save the "px" and "qty":
        MDEntryST&  mde = m_mdMsg.m_entries[off]; // REF!
        mde.m_entryType =
          (b == 1) ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer;
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
        SKP_STR("],[\"")
      }

      if constexpr (IsSnapShot) {
        if (b == 0)
          SKP_STR(",\"bs\":[[\"")
      } else {
        bool done = (b == 1);
        if (!done) {
          // processing update and we just finish processing asks,
          // check if there are any bids
          if (utxx::unlikely(strncmp(curr, "},{\"b\":[[\"", 9) == 0))
            // there are also bids
            curr += 10;
          else
            // we're done
            done = true;
        }

        if (done) {
          SKP_STR(",\"c\":\"")
          // could now read checksum if we want
          curr = strchr(curr, '}');
          break;
        }
      }
    }
    // At the end:
    m_mdMsg.m_nEntrs = off;
    SKP_STR("},\"book")

    //-------------------------------------------------------------------//
    // Snapshot/Update done, process it in "EConnector_MktData":         //
    //-------------------------------------------------------------------//
    // NB: "UpdateOrderBooks" takes care of notifying the Strategies:
    //
    CHECK_ONLY(bool ok =) EConnector_MktData::UpdateOrderBooks
    <
      IsSnapShot,
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
    // DynInitMode=false
    (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl, nullptr, a_ob);

    CHECK_ONLY
    (
      if (utxx::unlikely(!ok))
      {
        LOG_WARN(2,
          "EConnector_WS_KrakenSpot_MDC::ProcessBook: {}: OrderBook {} Failed",
          ToCStr(m_mdMsg.m_symbol), IsSnapShot ? "Snapshot" : "Update")
        // Nothing else to do here -- Strategy mgmt already done!
      }
    )

    return true;
  }

} // End namespace MAQUETTE
