// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/FTX/EConnector_WS_FTX_MDC.cpp":                 //
//                          WS-Based MDC for FTX                             //
//===========================================================================//
#include "Connectors/H2WS/FTX/EConnector_WS_FTX_MDC.h"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Venues/FTX/SecDefs.h"
#include "Venues/FTX/Configs_WS.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <boost/current_function.hpp>
#include <sstream>
#include <cstring>
#include <cstdlib>

using namespace std;

//---------------------------------------------------------------------------//
// "CHECK": check EXP and throw error if false                               //
//---------------------------------------------------------------------------//
#ifdef CHECK
#undef CHECK
#endif

#define CHECK(EXP) \
  { \
    if (utxx::likely(!(EXP))) \
      throw utxx::runtime_error( \
        __FILE__ ":" STRINGIFY(__LINE__) ": ", \
        BOOST_CURRENT_FUNCTION, \
        ": Check '" STRINGIFY(EXP) "' failed" \
      ); \
  }

//===========================================================================//
// Parsing Macros:                                                           //
//===========================================================================//
//===========================================================================//
// Parsing Macros:                                                           //
//===========================================================================//
//---------------------------------------------------------------------------//
// "CMP_STR": Compare with fixed (known) string and shift "Msg" pointer      //
//  if true                                                                  //
//---------------------------------------------------------------------------//
#ifdef  CMP_STR
#undef  CMP_STR
#endif
#define CMP_STR(Msg, Str) \
  ( \
    strncmp(Msg, Str, sizeof(Str) - 1) == 0 && (Msg += sizeof(Str) - 1, true) \
  )

//---------------------------------------------------------------------------//
// "SKP_STR": Skip a fixed (known) string:                                   //
//---------------------------------------------------------------------------//
#ifdef  SKP_STR
#undef  SKP_STR
#endif
#define SKP_STR(Curr, End, Str) \
  { \
    constexpr size_t strLen = sizeof(Str)-1; \
    CHECK(strncmp(Str, Curr, strLen) == 0) \
    Curr += strLen; \
    assert(Curr < End); \
  }

namespace MAQUETTE
{
  //=========================================================================//
  // "GetWSConfig":                                                          //
  //=========================================================================//
  H2WSConfig const& EConnector_WS_FTX_MDC::GetWSConfig(MQTEnvT a_env)
  {
    // XXX: For Binance, only "Prod" is actually supported, but for an MDC,
    // this does not matter, so we always return the "Prod" config here:
    //
    auto   cit  = FTX::Configs_WS_MDC.find(a_env);
    assert(cit != FTX::Configs_WS_MDC.cend());
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_FTX_MDC::EConnector_WS_FTX_MDC
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string>              const*  a_only_symbols,   // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params,
    EConnector_MktData*                 a_primary_mdc     // Normally NULL
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "FTX",
      0,                            // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                        // No BusyWait
      a_sec_defs_mgr,
      FTX::SecDefs,
      a_only_symbols,               // Restricting FTX::SecDefs
      a_params.get<bool>("ExplSettlDatesOnly",   false),
      false,
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_MDC":                                                  //
    //-----------------------------------------------------------------------//
    // NB: As usual, AccountKey is the Connector Instance Name. It may look like
    // {AccountPfx}-WS-FTX-MDC-{Env}:
    MDCWS
    (
      a_primary_mdc,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // This Class:                                                           //
    //-----------------------------------------------------------------------//
    m_mdMsg()
  {}

  //=========================================================================//
  // "InitSession":                                                          //
  //=========================================================================//
  void EConnector_WS_FTX_MDC::InitSession() const
  {
    LOG_INFO(2, "EConnector_WS_FTX_MDC::InitSession")
    ProWS::InitWSHandShake("/ws");
  }

  //-------------------------------------------------------------------------//
  // "InitLogOn":                                                          //
  //-------------------------------------------------------------------------//
  void EConnector_WS_FTX_MDC::InitLogOn()
  {
    // Clear the existing channel map in case of reconnect
    // TODO(ats):
    //m_channels.clear();

    LOG_INFO(2,
          "EConnector_WS_FTX_MDC::InitWSLogOn: Subscribing to MktData Channels")

    // (Optional) Authenticate with {op: 'login', 'args': {'key': <api_key>, 'sign': <signature>, 'time': <ts>}}

    char  buffBook[128];
    char* symPosBook = stpcpy(buffBook,
      R"({"op":"subscribe","channel":"orderbook","market":")");

    char  buffTrades[128];
    char* symPosTrades = stpcpy(buffTrades,
      R"({"op":"subscribe","channel":"trades","market":")");

    auto const subscribe = [this](char* msgBuff,
                                  std::size_t DEBUG_ONLY(buffSize),
                                  char* symPos,
                                  char const* symbol){
      // Request:
      char* curr = stpcpy(symPos, symbol);
      curr       = stpcpy(curr, "\"}");
      assert(size_t(curr - msgBuff) < buffSize);

      // Copy the data into the sending buffer, and send a WS frame:
      ProWS::PutTxtData(msgBuff);
      ProWS::SendTxtFrame();
    };

    for (SecDefD const* instr : m_usedSecDefs)
    {
      // Compete the Subscr JSON:
      assert(instr != nullptr);

      auto const symbol = instr->m_Symbol.data();

      // Request for Order Books:
      subscribe(buffBook, sizeof(buffBook), symPosBook, symbol);

      // Request for Trades:
      if (EConnector_MktData::HasTrades())
        subscribe(buffTrades, sizeof(buffTrades), symPosTrades, symbol);
    }

    MDCWS::LogOnCompleted(utxx::now_utc());
  }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Returns "true" to continue receiving msgs, "false" to stop:
  //
  bool EConnector_WS_FTX_MDC::ProcessWSMsg
  (
    char const*     a_msg_body,
    int             a_msg_len,
    bool            UNUSED_PARAM(a_last_msg),
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
#ifdef LOGPFX
#undef LOGPFX
#endif
#define LOGPFX "EConnector_WS_FTX_MDC::ProcessWSMsg: "

    // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
    assert(a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    bool  ok         = true;
    char* msgEnd     = const_cast<char*>(a_msg_body + a_msg_len);
    const char* curr = a_msg_body;

    try
    {
      CHECK(a_msg_body[0] == '{' && a_msg_body[a_msg_len-1] == '}')

      if (utxx::likely(CMP_STR(curr, R"({"channel": )")))
      {
        //-------------------------------------------------------------------//
        // MKtData:                                                          //
        //-------------------------------------------------------------------//
        if (CMP_STR(curr, R"("orderbook", "market": ")"))
        {
          char const* symPos = curr;
          char const* symEnd = strchr(symPos, '"');
          curr = symEnd;

          CHECK(symPos < symEnd)

          string symbol(symPos, size_t(symEnd - symPos));
          InitFixedStr(&m_mdMsg.m_symbol, symbol);

          // Find the OrderBook for this Symbol (UseAltSymbol=false):
          OrderBook* ob =
            EConnector_MktData::FindOrderBook<false>(symbol.data());

          assert(ob != nullptr);

          CHECK(CMP_STR(curr, R"(", "type": )"))

          int offset = 0;

          if (CMP_STR(curr, R"("update", "data": )"))
          {
            ProcessUpdates(curr, msgEnd, ob, &offset,  true);
            ProcessUpdates(curr, msgEnd, ob, &offset,  false);

            if (offset == 0)
              return true;

            constexpr bool isSnapshot = false;

            m_mdMsg.m_nEntrs = offset;
            EConnector_MktData::UpdateOrderBooks
              <
                isSnapshot,
                IsMultiCast,
                WithIncrUpdates,
                WithOrdersLog,
                WithRelaxedOBs,
                ChangeIsPartFill,
                NewEncdAsChange,
                ChangeEncdAsNew,
                IsFullAmt,
                IsSparse,
                FindOrderBookBy::Symbol,
                QT,
                QR,
                OMC
                >
              (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl);
          }
          else
          {
            CHECK(CMP_STR(curr, R"("partial", "data": )"))

            ProcessUpdates(curr, msgEnd, ob, &offset,  true);
            ProcessUpdates(curr, msgEnd, ob, &offset,  false);

            constexpr bool isSnapshot = true;

            m_mdMsg.m_nEntrs = offset;
            EConnector_MktData::UpdateOrderBooks
              <
                isSnapshot,
                IsMultiCast,
                WithIncrUpdates,
                WithOrdersLog,
                WithRelaxedOBs,
                ChangeIsPartFill,
                NewEncdAsChange,
                ChangeEncdAsNew,
                IsFullAmt,
                IsSparse,
                FindOrderBookBy::Symbol,
                QT,
                QR,
                OMC
                >
              (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl);
          }
        }
        else
        {
          CHECK(CMP_STR(curr, R"("trades", "market": ")"))
          char const* symPos = curr;
          char const* symEnd = strchr(symPos, '"');
          curr = symEnd;

          CHECK(symPos < symEnd)

          string symbol(symPos, size_t(symEnd - symPos));
          InitFixedStr(&m_mdMsg.m_symbol, symbol);

          // Find the OrderBook for this Symbol (UseAltSymbol=false):
          OrderBook* ob =
            EConnector_MktData::FindOrderBook<false>(symbol.data());

          assert(ob != nullptr);

          CHECK(CMP_STR(curr, R"(", "type": "update", "data": )"))

          ProcessTrades(curr, msgEnd, ob, a_ts_recv, a_ts_handl);
        }
      }
      else
      if (CMP_STR(curr, R"({"type": "subscribed", "channel": )"))
      {
        if (CMP_STR(curr, R"("orderbook", "market": )"))
        {
          //TODO
        }
        else
        if (CMP_STR(curr, R"("trades", "market": )"))
        {
          //TODO
        }
        else
        {
          LOG_ERROR(2, LOGPFX "Subscribed to the channel of unknown type, "
                              "Msg={}", a_msg_body)
          return false;
        }
      }
      else
      if (CMP_STR(curr, R"({"type": "info")") &&
          (strstr(curr, R"("code": 20001)") != nullptr ||
           strstr(curr, R"("code": "20001")") != nullptr))
      {
        //-----------------------------------------------------------------//
        // Need to reconnect                                               //
        //-----------------------------------------------------------------//
        LOG_INFO(2, LOGPFX "Code 20001 received - need to reconnect, Msg={}",
                 a_msg_body)
        return false;
      }
      else
      if (CMP_STR(curr, R"({"type": "error") )"))
      {
        //-----------------------------------------------------------------//
        // Some Error: Log it and Exit:                                    //
        //-----------------------------------------------------------------//
        LOG_ERROR(2, LOGPFX "Error found, Msg={}", a_msg_body)
        return false;
      }
      else
      {
        LOG_ERROR(2, LOGPFX "Unknown msg type, Msg={}", a_msg_body)
        return false;
      }
    }
    catch (exception const& exn)
    {
      LOG_ERROR(2, LOGPFX "{}, Msg={}", exn.what(), a_msg_body)
      ok = false;
    }

    return ok;
  }

  //-----------------------------------------------------------------------//
  // "ProcessUpdate"                                                       //
  //-----------------------------------------------------------------------//
  // Throws exception on failure:
  //
  void EConnector_WS_FTX_MDC::ProcessUpdates
  (
    char const*    a_data,
    char const*    a_end,
    OrderBook*     DEBUG_ONLY(a_ob),
    int*           a_offset,
    bool           a_is_bids
  )
  {
    assert(a_ob != nullptr);
    CHECK(a_data != nullptr && a_data < a_end && *a_data == '{')

    char const* curr = a_data;
    double px        = NaN<double>;
    double qty       = NaN<double>;

    CHECK(curr != nullptr && curr < a_end)

    char const* field = a_is_bids ? R"("bids")" : R"("asks")";

    curr = strstr(curr, field);
    CHECK(curr != nullptr && curr < a_end)
    SKP_STR(curr, a_end, a_is_bids ? R"("bids": [)" : R"("asks": [)")

    char* after = nullptr;

    while (curr < a_end && *curr == '[')
    {
      // [px, qty], ...]]
      ++curr;
      px = strtod(curr, &after);
      CHECK(curr != after)
      curr = after + 2; // skip ", "
      CHECK(curr < a_end && *(curr - 2) == ',' && *(curr - 1) == ' ')

      qty = strtod(curr, &after);
      CHECK(curr != after)
      curr = after;

      MDEntryST& mde = m_mdMsg.m_entries[*a_offset];
      mde.m_entryType = a_is_bids ? FIX::MDEntryTypeT::Bid :
                                  FIX::MDEntryTypeT::Offer;
      mde.m_px  = PriceT(px);
      mde.m_qty = QtyN(qty);
      ++*a_offset;

      if (CMP_STR(curr, "], "))
        continue;

      CHECK(CMP_STR(curr, "]]"))
    }
  }

  //=========================================================================//
  // "ProcessTrades":                                                         //
  //=========================================================================//
  void EConnector_WS_FTX_MDC::ProcessTrades
  (
    char const*    a_data,
    char const*    a_end,
    OrderBook*     a_ob,
    utxx::time_val a_ts_recv,
    utxx::time_val UNUSED_PARAM(a_ts_handl)
  )
  {
#ifdef LOGPFX
#undef LOGPFX
#endif
#define LOGPFX "EConnector_WS_FTX_MDC::ProcessTrades: "

    assert(a_ob != nullptr);
    CHECK(a_data != nullptr && a_data < a_end && *a_data == '[')

    char*  after = nullptr;
    size_t offset = 0;
    char const* curr = a_data + 1;
    FIX::SideT aggrSide;

    while (curr < a_end && *curr == '{')
    {
      MDEntryST& mde = m_mdMsg.m_entries[offset];

      // TrdMatchID
      CHECK(CMP_STR(curr, R"({"id": )"))
      curr = utxx::fast_atoi<OrderID, false>(curr, a_end, mde.m_execID);

      // Price
      CHECK(CMP_STR(curr, R"(, "price": )"))
      double px = strtod(curr, &after);
      CHECK(curr != after)
      curr      = after;
      mde.m_px  = PriceT(px);

      // Qty
      CHECK(CMP_STR(curr, R"(, "size": )"))
      double qty = strtod(curr, &after);
      curr       = after;
      mde.m_qty  = QtyN(qty);

      // Side
      CHECK(CMP_STR(curr, R"(, "side": )"))
      if (CMP_STR(curr, R"("buy")"))
      {
        aggrSide = FIX::SideT::Buy;
      }
      else
      {
        CHECK(CMP_STR(curr, R"("sell")"))
        aggrSide = FIX::SideT::Sell;
      }

      CHECK_ONLY
      (
        LOG_INFO(4, LOGPFX "Processed {} trade: Qty={}, Px={}.",
                 (aggrSide == FIX::SideT::Sell ? "SELL" : "BUY"), qty, px)
      )

      CHECK(CMP_STR(curr, R"(, "liquidation": )") &&
            (CMP_STR(curr, R"(true, )") || CMP_STR(curr, R"(false, )")))

      // Timestamp
      CHECK(CMP_STR(curr, R"("time": ")"))
      m_mdMsg.m_exchTS = DateTimeToTimeValTZhhmm(curr);

      CHECK((curr = strchr(curr, '"')) != nullptr)

      ProcessTrade<false, QT, QR, OMC>
        (m_mdMsg, mde, nullptr, *a_ob, mde.m_qty, aggrSide, a_ts_recv);

      if (CMP_STR(curr,   R"("}, )"))
        continue;

      CHECK(CMP_STR(curr, R"("}])"))
    }
  }
} // End namespace MAQUETTE
