// vim:ts=2:et
//===========================================================================//
//           "Connectors/H2WS/Huobi/Processor_MDC.hpp"                       //
//===========================================================================//
#pragma once

#include <sstream>
#include <cstring>
#include <cstdlib>

#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>

#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Connectors/H2WS/Huobi/EConnector_WS_Huobi_MDC.h"
#include "Protocols/JSONParseMacros.h"

namespace MAQUETTE
{
  using namespace Huobi;

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Returns "true" to continue receiving msgs, "false" to stop:
  template <AssetClassT::type AC>
  bool EConnector_WS_Huobi_MDC<AC>::ProcessWSMsg(
      char const* a_msg_body,
      int         a_msg_len,
      bool,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl
  )
  {
    bool ok = true;

    try {
      auto c = m_zlib->decompress(a_msg_body, uint32_t(a_msg_len));

      a_msg_body = c.first;
      a_msg_len = int(c.second - c.first);

      // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
      assert(a_msg_body != nullptr && a_msg_len > 0 &&
            a_msg_body[a_msg_len] == '\0');

      // Check if the msg is complete (if not, it is a malformed msg or a proto-
      // col-level error):
      CHECK_ONLY
      (
        LOG_INFO(4, "EConnector_WS_Huobi_MDC::ProcessWSMsg: Received msg={}",
                    a_msg_body)

        this->template LogMsg<false>(a_msg_body, nullptr, 0);

        assert(a_msg_len > 0);
        const char * last = a_msg_body + (a_msg_len - 1);
        if (utxx::unlikely(*last != '}'))
        {
          LOG_ERROR(2,
            "EConnector_WS_Huobi_MDC::ProcessWSMsg: Truncated JSON Obj: {}, "
            "Len={}", a_msg_body, a_msg_len)
          return false;
        }
      )

      char const* curr = a_msg_body;

      // parse message to find out what it is
      SKP_STR("{\"")
      if (utxx::likely(strncmp(curr, "ch\":\"market.", 12) == 0)) {
        curr += 12;
        GET_STR(channel)

        // channel is of the form <symbol>.<type>
        const char* symbol = channel;
        const char* type = strchr(symbol, '.');
        *const_cast<char *>(type) = '\0';
        ++type;

        InitFixedStr(&m_mdMsg.m_symbol, symbol);
        auto ob = m_channels.at(std::string(symbol));
        if (utxx::likely(ob != nullptr))
        {
          if (utxx::likely(strncmp(type, "bbo", 3) == 0)) {
            assert(m_useBBA);
            ok = ProcessBBA(
              curr, a_msg_body, a_msg_len, ob, a_ts_recv, a_ts_handl);
          } else if (strncmp(type, IsSpt ? "mbp" : "dep", 3) == 0) {
            assert(!m_useBBA);
            ok = ProcessBook(
              curr, a_msg_body, a_msg_len, ob, a_ts_recv, a_ts_handl);
          } else if (strncmp(type, "trade.detail", 12) == 0) {
            // this is a trade
            if (utxx::likely(ob->IsInitialised()))
              ok = ProcessTrade(
                curr, a_msg_body, a_msg_len, ob, a_ts_recv, a_ts_handl);
          } else {
            LOG_ERROR(2,
              "EConnector_WS_Huobi_MDC::ProcessWSMsg: Unrecognized "
              "message in channel {}, Msg={}", symbol, a_msg_body)
            return false;
          }
        }
      } else if (strncmp(curr, "ping\":", 6) == 0) {
        // Answer to ping message with pong
        LOG_INFO(3, "EConnector_WS_Huobi_MDC::ProcessWSMsg: Received ping: {}",
                 a_msg_body)
        std::string pong(a_msg_body);
        pong[3] = 'o'; // Change ping to pong
        CHECK_ONLY(this->template LogMsg<true>(pong.c_str(), nullptr, 0);)
        LOG_INFO(3, "EConnector_WS_Huobi_MDC::ProcessWSMsg: Sending pong: {}",
                 pong.c_str())
        this->PutTxtData(pong.c_str());
        this->SendTxtFrame();
      } else if (strncmp(curr, "id\":\"", 5) == 0) {
        // a sub/unsub response
        curr = SkipNFlds<1>(curr); // skip id
        if (!IsSpt)
          curr = SkipNFlds<2>(curr); // skip subbed, ts
        SKP_STR("\"status\":\"")
        if (strncmp(curr, "ok", 2) != 0)
          throw utxx::runtime_error(
            "EConnector_WS_Huobi_MDC::ProcessWSMsg: sub/unsub error: ",
            a_msg_body);
      } else if (strncmp(curr, "status\":\"error\"", 15) == 0) {
        // got an error message
        throw utxx::runtime_error(
          "EConnector_WS_Huobi_MDC::ProcessWSMsg: error message: ",
          a_msg_body);
      } else {
        throw utxx::runtime_error(
          "EConnector_WS_Huobi_MDC::ProcessWSMsg: Unknown message type: ",
          a_msg_body);
      }
    } catch (std::exception &ex) {
      // replace \0 in a_msg_body that we introduced as part of parsing it so
      // that we can print the whole message
      for (int i = 0; i < a_msg_len; ++i)
        if (a_msg_body[i] == '\0')
          const_cast<char*>(a_msg_body)[i] = ' ';

      LOG_ERROR(2, "EConnector_WS_Huobi_MDC::ProcessWSMsg: {}, Msg={}",
                ex.what(), a_msg_body)

      // we keep going
      ok = true;
    }

    return ok;
  }

  //=========================================================================//
  // Parsers                                                                 //
  //=========================================================================//
  //=========================================================================//
  // "ProcessTrade":                                                         //
  //=========================================================================//
  template <AssetClassT::type AC>
  bool EConnector_WS_Huobi_MDC<AC>::ProcessTrade
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
    char const* end = a_msg_body + a_msg_len;

    SKP_STR(",\"ts\":")
    curr = SkipNFlds<1>(curr);
    SKP_STR("\"tick\":{\"")

    curr = SkipNFlds<2>(curr);
    SKP_STR("\"data\":[")
    curr = strchr(curr, '{');

    m_mdMsg.m_nEntrs = 0;
    MDEntryST& mde = m_mdMsg.m_entries[0];
    mde.m_entryType = FIX::MDEntryTypeT::Trade;

    while (curr != nullptr) {
      if constexpr (IsSpt) {
        SKP_STR("{\"id\":")
        uint64_t id = 0;
        curr = utxx::fast_atoi<uint64_t, false>(curr, end, id);
        mde.m_execID = id;

        SKP_STR(",\"ts\":")
        uint64_t ms = 0;
        curr = utxx::fast_atoi<uint64_t, false>(curr, end, ms);
        m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(ms));

        SKP_STR(",\"tradeId\":")
        curr = SkipNFlds<1>(curr);
      }
      else
        SKP_STR("{")

      SKP_STR("\"amount\":")
      char*  after = nullptr;
      mde.m_qty    = QtyN  (strtod(curr, &after));
      assert(after != nullptr);
      curr         = after;

      if (IsZero(mde.m_qty)) {
        // this quantity is too small, ignore it
        curr = strchr(curr, '{');
        continue;
      }

      if (!IsSpt) {
        SKP_STR(",\"quantity\":")
        curr = SkipNFlds<1>(curr);

        if (strncmp(curr, "\"trade_turnover\":", 12) == 0) {
          SKP_STR("\"trade_turnover\":")
          curr = SkipNFlds<1>(curr);
        }

        SKP_STR("\"ts\":")
        uint64_t ms = 0;
        curr = utxx::fast_atoi<uint64_t, false>(curr, end, ms);
        m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(ms));

        SKP_STR(",\"id\":")
        uint64_t id = 0;
        curr = utxx::fast_atoi<uint64_t, false>(curr, end, id);
        mde.m_execID = id;
      }

      SKP_STR(",\"price\":")
      mde.m_px     = PriceT(strtod(curr, &after));
      assert(after != nullptr);
      curr         = after;

      SKP_STR(",\"direction\":\"")
      assert((*curr == 'b') || (*curr == 's'));
      FIX::SideT aggrSide =
        (*curr == 's') ? FIX::SideT::Sell : FIX::SideT::Buy;

      EConnector_MktData::ProcessTrade<false, QT, QR, OMC>(
        m_mdMsg, mde, nullptr, *a_ob, mde.m_qty, aggrSide, a_ts_recv);

      // move on to next trade (if any)
      curr = strchr(curr, '{');
    }

    return true;
  }

  //=========================================================================//
  // "ProcessBBA":                                                           //
  //=========================================================================//
  template <AssetClassT::type AC>
  bool EConnector_WS_Huobi_MDC<AC>::ProcessBBA
  (
    char const*     a_curr,
    char const*     a_msg_body,
    int             a_msg_len,
    OrderBook*      a_ob,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    assert(a_ob != nullptr);

    m_mdMsg.m_nEntrs = 2;
    char const* curr = a_curr;
    char const* end = a_msg_body + a_msg_len;
    char* after = nullptr;

    SKP_STR(",\"ts\":")
    uint64_t ms;
    curr = utxx::fast_atoi<uint64_t, false>(curr, end, ms);
    m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(ms));

    SKP_STR(",\"tick\":{")

    if (IsSpt) {
      SKP_STR("\"seqId\":")
      curr = SkipNFlds<1>(curr);
    }

    if (!IsSpt) {
      SKP_STR("\"mrid\":")
      curr = SkipNFlds<1>(curr);
      SKP_STR("\"id\":")
      curr = SkipNFlds<1>(curr);
    }

    {
      if (IsSpt)
        SKP_STR("\"ask\":")
      else
        SKP_STR("\"bid\":[")

      MDEntryST& mde = m_mdMsg.m_entries[0];
      mde.m_entryType = IsSpt ? FIX::MDEntryTypeT::Offer
                              : FIX::MDEntryTypeT::Bid;
      mde.m_px = PriceT(strtod(curr, &after));
      assert(after != nullptr);
      curr = after;

      if (IsSpt)
        SKP_STR(",\"askSize\":")
      else
        SKP_STR(",")

      mde.m_qty = QtyN(strtod(curr, &after));
      assert(after != nullptr);
      curr = after;
    }

    {
      if (IsSpt)
        SKP_STR(",\"bid\":")
      else
        SKP_STR("],\"ask\":[")

      MDEntryST& mde = m_mdMsg.m_entries[1];
      mde.m_entryType = IsSpt ? FIX::MDEntryTypeT::Bid
                              : FIX::MDEntryTypeT::Offer;
      mde.m_px = PriceT(strtod(curr, &after));
      assert(after != nullptr);
      curr = after;

      if (IsSpt)
        SKP_STR(",\"bidSize\":")
      else
        SKP_STR(",")

      mde.m_qty = QtyN(strtod(curr, &after));
      assert(after != nullptr);
      curr = after;
    }

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
      EConnector_MktData::FindOrderBookBy::AltSymbol,
      QT,
      QR,
      OMC
    >
    (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl, nullptr, a_ob);

    CHECK_ONLY
    (
      if (utxx::unlikely(!ok))
      {
        LOG_WARN(2,
          "EConnector_WS_Huobi_MDC::ProcessBBA: {}: OrderBook Update "
          "Failed", ToCStr(m_mdMsg.m_symbol))
      }
    )

    return true;
  }

  //=========================================================================//
  // "ProcessBook":                                                          //
  //=========================================================================//
  template <AssetClassT::type AC>
  bool EConnector_WS_Huobi_MDC<AC>::ProcessBook
  (
    char const*     a_curr,
    char const*     a_msg_body,
    int             a_msg_len,
    OrderBook*      a_ob,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    assert(a_ob != nullptr);

    m_mdMsg.m_nEntrs = m_mdMsg.MaxMDEs;   // NB: Bids + Asks!

    char const* curr = a_curr;
    char const* end  = a_msg_body + a_msg_len;

    if (IsSpt) {
      SKP_STR(",\"ts\":")
      uint64_t ms;
      curr = utxx::fast_atoi<uint64_t, false>(curr, end, ms);
      m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(ms));
    }

    SKP_STR(",\"tick\":{")

    if (IsSpt) {
      SKP_STR("\"seqNum\":")
      curr = SkipNFlds<1>(curr);
      SKP_STR("\"prevSeqNum\":")
      curr = SkipNFlds<1>(curr);
    }

    // read bids and asks (could be any order and only one)
    int off = 0;
    for (char b = 0; b < 2; ++b)
    {
      bool ask;
      if (strncmp(curr, "\"asks\":[", 8) == 0) {
        ask = true;
        curr += 8;
      } else if (strncmp(curr, "\"bids\":[", 8) == 0) {
        ask = false;
        curr += 8;
      } else {
        break; // we're done
      }

      if (*curr == ']') {
        curr += 2; // skip ] and , (or })
        continue; // empty array
      }

      char* after = nullptr;
      for (int i = 0; i < m_mdMsg.MaxMDEs; ++i)
      {
        SKP_STR("[")
        double px  = NaN<double>;
        px = strtod(curr, &after);
        assert(after != nullptr);
        curr = after;
        SKP_STR(",")

        double qty = NaN<double>;
        qty = strtod(curr, &after);
        assert(after != nullptr);
        curr = after;

        // Check what we got:
        CHECK_ONLY
        (
          if (utxx::unlikely(!((px > 0.0) && (qty >= 0.0))))
          {
           LOG_WARN(2,
              "EConnector_WS_KrakenSpot_MDC::ProcessSnapShot: {}: Invalid Px={}"
              " or Qty={} skipped", a_curr, px, qty)
            // Skip the entire msg (as the SnapShot is invalid), but still
            // continue reading:
            return true;
          }
        )

        // Save the "px" and "qty":
        MDEntryST&  mde = m_mdMsg.m_entries[off]; // REF!
        mde.m_entryType =
          ask ? FIX::MDEntryTypeT::Offer : FIX::MDEntryTypeT::Bid;
        mde.m_px        = PriceT(px);
        mde.m_qty       = QtyN  (qty);
        ++off;

        // Next: It could be either next Entry, or Side Done:
        if (utxx::unlikely(strncmp(curr, "]]", 2) == 0))
        {
          curr += 3; // skip ]] and , (or })
          break;  // Yes, Side Done
        }
        // If not done yet:
        SKP_STR("],")
      }
    }

    bool is_snapshot = false;
    if (!IsSpt) {
      SKP_STR("\"ch\":")
      curr = SkipNFlds<1>(curr);
      SKP_STR("\"event\":\"")
      is_snapshot = (*curr == 's');
      curr = SkipNFlds<3>(curr); // skip event, id, mrid
      SKP_STR("\"ts\":")
      uint64_t ms;
      curr = utxx::fast_atoi<uint64_t, false>(curr, end, ms);
      m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(ms));
    }
    // At the end:
    m_mdMsg.m_nEntrs = off;

    //-------------------------------------------------------------------//
    // Snapshot/Update done, process it in "EConnector_MktData":         //
    //-------------------------------------------------------------------//
    // NB: "UpdateOrderBooks" takes care of notifying the Strategies:
    //
    CHECK_ONLY(bool ok);
    if (is_snapshot)
      CHECK_ONLY(ok =) EConnector_MktData::UpdateOrderBooks
      <
        true,
        IsMultiCast,
        WithIncrUpdates,
        WithOrdersLog,
        WithRelaxedOBs,
        ChangeIsPartFill,
        NewEncdAsChange,
        ChangeEncdAsNew,
        IsFullAmt,
        IsSparse,
        EConnector_MktData::FindOrderBookBy::AltSymbol,
        QT,
        QR,
        OMC
      >
      // DynInitMode=false
      (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl, nullptr, a_ob);
    else
      CHECK_ONLY(ok =) EConnector_MktData::UpdateOrderBooks
      <
        false,
        IsMultiCast,
        WithIncrUpdates,
        WithOrdersLog,
        WithRelaxedOBs,
        ChangeIsPartFill,
        NewEncdAsChange,
        ChangeEncdAsNew,
        IsFullAmt,
        IsSparse,
        EConnector_MktData::FindOrderBookBy::AltSymbol,
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
          ToCStr(m_mdMsg.m_symbol), is_snapshot ? "Snapshot" : "Update")
        // Nothing else to do here -- Strategy mgmt already done!
      }
    )

    return true;
  }
} // namespace MAQUETTE
