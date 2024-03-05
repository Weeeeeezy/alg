// vim:ts=2:et
//===========================================================================//
//            "Connectors/H2WS/BitMEX/EConnector_WS_BitMEX_MDC.cpp":         //
//                          WS-Based MDC for BitMEX                          //
//===========================================================================//
#include "Connectors/H2WS/BitMEX/EConnector_WS_BitMEX_MDC.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/BitMEX/Configs_H2WS.h"
#include "Venues/BitMEX/SecDefs.h"
#include <cstring>
#include <sstream>
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>

using namespace std;

//===========================================================================//
// JSON Parsing Macros:                                                      //
//===========================================================================//
//---------------------------------------------------------------------------//
// "SKP_STR": Skip a fixed (known) string:                                   //
//---------------------------------------------------------------------------//
#ifdef  SKP_STR
#undef  SKP_STR
#endif
#define SKP_STR(Str) \
  { \
    constexpr size_t strLen = sizeof(Str)-1; \
    assert(strncmp(Str, curr, strLen) == 0); \
    curr += strLen; \
  }

namespace MAQUETTE
{
  //=========================================================================//
  // "GetPxLevel": Only for L2 OrderBooks:                                   //
  //=========================================================================//
  // Returns the Px Level embedded in PxLevelID:
  //
  inline PriceT EConnector_WS_BitMEX_MDC::GetPxLevel
  (
    const SymKey& a_symbol,
    unsigned long a_id
  )
  {
    unsigned long secID  = m_secID[a_symbol];
    double        pxStep = m_pxStp[a_symbol];
    double        res    = double(100'000'000 * secID - a_id) * pxStep;
    LOG_INFO(4, "GetPxLevel: Symb={}, SymbID={}, PxStep={}, id={}, resPx={}",
                a_symbol.data(), secID, pxStep, a_id, res)
    return PriceT(res);
  }

  //=========================================================================//
  // "GetSecDefs":                                                           //
  //=========================================================================//
  std::vector<SecDefS> const* EConnector_WS_BitMEX_MDC::GetSecDefs()
  { return &BitMEX::SecDefs; }

  //=========================================================================//
  // "GetWSConfig":                                                          //
  //=========================================================================//
  H2WSConfig const& EConnector_WS_BitMEX_MDC::GetWSConfig(MQTEnvT a_env)
  {
    // XXX: For BitMEX "Prod" and "TestNet" are available:
    auto cit = BitMEX::Configs_WS_MDC.find(a_env);

    if (utxx::unlikely(cit == BitMEX::Configs_WS_MDC.cend()))
      throw utxx::badarg_error
            ("EConnector_WS_BitMEX_MDC::GetWSConfig: UnSupported Env=",
             a_env.to_string());

    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_BitMEX_MDC::EConnector_WS_BitMEX_MDC
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string> const*               a_only_symbols, // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params,
    EConnector_MktData*                 a_primary_mdc   // Normally NULL
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "BitMEX",
      0,                                // XXX: No extra ShM data as yet...
      a_reactor,
      false,                            // No BusyWait
      a_sec_defs_mgr,
      BitMEX::SecDefs,
      a_only_symbols,
      a_params.get<bool>("ExplSettlDatesOnly",   false),
      false,                            // Presumably no Tenors in SecIDs
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_MDC":                                                  //
    //-----------------------------------------------------------------------//
    MDCWS
    (
      a_primary_mdc,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // This Class:                                                           //
    //-----------------------------------------------------------------------//
    m_mdMsg(),
    m_isInitialized(false)
  {}

  //=========================================================================//
  // "InitSession":                                                          //
  //=========================================================================//
  // XXX: At the moment, we subscribe to Depth-20 SnapShots rather than Full-
  // Depth Incremental Updates. The reasons are the following:
  // (*) We are not much interested in updates at far-away OrderBook levels;
  //     they may be problematic to handle, and could generate extra traffic
  //     whih reduces the savings from receiving Increments compared to Snap-
  //     Shots;
  //
  void EConnector_WS_BitMEX_MDC::InitSession()
  {
    //-----------------------------------------------------------------------//
    // Construct the Path from the Instrs:                                   //
    //-----------------------------------------------------------------------//
    // NB: The number of subscribed instrs is potentially large, so use String
    // Stream -- it is OK as this method is for starting-up only:
    stringstream sp;
    // sp << "/realtime?subscribe=";
    sp << "/realtime?subscribe=orderBook10";
    if (EConnector_MktData::HasTrades())
      sp << ",trade";

    // bool frst = true;
    for (SecDefD const* instr : m_usedSecDefs)
    {
      // assert(instr != nullptr);
      // if (frst)
      //   frst = false;
      // else
      //   sp << ',';

      // // Put orderBook10 for orderbook
      // sp << "orderBook10:" << instr->m_Symbol.data();

      // if (EConnector_MktData::HasTrades())
      //   // Subscribe to trades:
      //   sp << ",trade:" << instr->m_Symbol.data();

      // Init maps
      // m_pxStp[std::string(instr->m_Symbol.data())] = instr->m_PxStep;
      m_secID[instr->m_Symbol] = instr->m_SecID;
    }
    //-----------------------------------------------------------------------//
    // Proceed with the HTTP/WS HandShake using the Path generated:          //
    //-----------------------------------------------------------------------//
    ProWS::InitWSHandShake(sp.str().c_str());
  }

  //=========================================================================//
  // "InitLogOn":                                                            //
  //=========================================================================//
  // Nothing to do, signal immediate Completion:
  //
  void EConnector_WS_BitMEX_MDC::InitLogOn()
    { MDCWS::LogOnCompleted(utxx::now_utc()); }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Returns "true" to continue receiving msgs, "false" to stop:
  //
  bool EConnector_WS_BitMEX_MDC::ProcessWSMsg
  (
    char const*    a_msg_body,
    int            a_msg_len,
    bool           UNUSED_PARAM(a_last_msg),
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
    assert(a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    char const* curr  = a_msg_body;
    char*       after = nullptr;
    char const* end   = a_msg_body + a_msg_len;

    CHECK_ONLY(this->template LogMsg<false>(a_msg_body, nullptr, 0);)

    // Subscription failure
    if (utxx::unlikely
       (strstr(a_msg_body, "{\"success\":\"false\"") != nullptr))
    {
      LOG_ERROR(1,
        "EConnector_WS_BitMEX_MDC::ProcessWSMsg: Subscription Error: {}, "
        "EXITING...", a_msg_body)
      this->Stop();
      return false;
    }

    // Welcome message, subscr success etc, but not actual data: Log it but
    // skip:
    curr = strstr(a_msg_body, "{\"table\":\"");
    if (utxx::unlikely(curr == nullptr))
    {
      LOG_INFO(1, "EConnector_WS_BitMEX_MDC::ProcessWSMsg: {}", a_msg_body)
      return true;
    }

    // Get the Stream. It is "trade" or "orderBook10":
    curr += 10;
    bool isTrade       = (strncmp(curr, "trade", 5) == 0);
    bool isOrderBook10 = (strncmp(curr, "orderBook10", 11) == 0);
    bool isOrderBookL2 = (strncmp(curr, "orderBookL2", 11) == 0);
    assert(isTrade || isOrderBook10 || isOrderBookL2);

    // BitMEX-specific "action":
    curr = strstr(a_msg_body, "\"action\":\"");
    if (utxx::unlikely(curr == nullptr))
    {
      LOG_INFO(1,
        "EConnector_WS_BitMEX_MDC::ProcessWSMsg: Missing \"action\": {}",
        a_msg_body)
      return true;
    }
    curr += 10;

    // Data Object:
    if (isTrade)
    {
      //----------------------------------------------------------------------//
      // Trades Batch (ie Trades arrive in Batches):                          //
      //----------------------------------------------------------------------//
      // Skip "partial" for trades:
      if (utxx::unlikely(strncmp(curr, "partial", 7) == 0))
      {
        LOG_INFO(4,
          "EConnector_WS_BitMEX_MDC::ProcessWSMsg: Received PARTIAL trade: {}",
          a_msg_body)
        return true;
      }

      // Start parsing trades:
      curr = strstr(a_msg_body, "\"data\":[{");
      if (utxx::unlikely(curr == nullptr))
      {
        return true;
      }
      curr += 9;
      char const* currTrade = curr;

      //-------------------------------------------------------------------//
      // Parse Trade Batch:                                                //
      //-------------------------------------------------------------------//
      m_mdMsg.m_nEntrs = 1;
      while (true)
      {
        // Use single entry for all trades
        MDEntryST& mde = m_mdMsg.m_entries[0];

        // Exchange timestamp:
        currTrade = strstr(curr, "\"timestamp\":\"") + 13;
        //"XXXX-XX-XXTXX:XX:XX.XXXZ"
        m_mdMsg.m_exchTS = DateTimeToTimeValTZ(currTrade);

        // Symbol (not altSymbol):
        currTrade = strstr(curr, "\"symbol\":\"") + 10;
        InitFixedStr(&m_mdMsg.m_symbol, currTrade,
                     size_t(strchr(currTrade, '"') - currTrade));

        // Side:
        currTrade     = strstr(curr, "\"side\":\"") + 8;
        bool sellAggr = strncmp(currTrade, "Sell", 4) == 0;
        mde.m_entryType = FIX::MDEntryTypeT::Trade;
        FIX::SideT aggrSide = sellAggr ? FIX::SideT::Sell : FIX::SideT::Buy;

        // Quantity:
        currTrade = strstr(curr, "\"size\":") + 7;
        long qty  = -1;
        utxx::fast_atoi<long, false>(currTrade, end, qty);
        mde.m_qty = QtyN(qty);

        // Price
        currTrade = strstr(curr, "\"price\":") + 8;
        mde.m_px  = PriceT(strtod(currTrade, &after));
        currTrade = after;

        // TrdMatchID
        currTrade = strstr(curr, "\"trdMatchID\":\"") + 14;
        mde.m_execID =
            ExchOrdID(currTrade, int(strchr(currTrade, '"') - currTrade));

        //-------------------------------------------------------------------//
        // Now Process this Trade:                                           //
        //-------------------------------------------------------------------//
        // Get the OrderBook for this Symbol -- it will be needed for sending
        // notifications to the subscribed Strategies:

        OrderBook const* ob =
          EConnector_MktData::FindOrderBook<false>(m_mdMsg.m_symbol.data());
        CHECK_ONLY
        (if (utxx::unlikely(ob == nullptr))
        {
          LOG_WARN(2,
            "EConnector_WS_BitMEX_MDC::ProcessWSMsg(Trade): No OrderBook for "
            "{} in {}", m_mdMsg.m_symbol.data(), a_msg_body)
        }
        else)

        // Generic Case:
        EConnector_MktData::ProcessTrade<WithOrdersLog, QT, QR, OMC>
          (m_mdMsg, mde, nullptr, *ob, mde.m_qty, aggrSide, a_ts_recv);

        // Move to next trade or resume waiting for message
        curr = strstr(curr, "},{");
        if (utxx::unlikely(curr == nullptr))
          return true;

        curr += 3;
        currTrade = curr;
      }
    }
    else
    if (isOrderBook10)
    {
      //---------------------------------------------------------------------//
      // "OrderBook10" SnapShot:                                             //
      //---------------------------------------------------------------------//
      // Only "partial" (for the initial SnapShot) or "update" (for following-
      // on SnapShots) Msgs are considered for OrderBook10:
      if (utxx::unlikely
         (strncmp(curr, "update", 6) != 0 && strncmp(curr, "partial", 7) != 0))
      {
        LOG_INFO(1,
          "EConnector_WS_BitMEX_MDC::ProcessWSMsg: Received weird OrderBook10:"
          " {}", a_msg_body)
        return true;
      }

      // Start parsing OB:
      curr = strstr(a_msg_body, "\"data\":[{");
      if (utxx::unlikely(curr == nullptr))
        return true;

      char const* currSnap = strstr(curr, "\"symbol\":\"") + 10;
      InitFixedStr(&m_mdMsg.m_symbol, currSnap,
                   size_t(strchr(currSnap, '"') - currSnap));

      //---------------------------------------------------------------------//
      // Parse an OrderBook10 SnapShot:                                      //
      //---------------------------------------------------------------------//
      m_mdMsg.m_nEntrs = 20;  // NB: Bids + Asks!

      // Bids (b==1) and Asks (b==0) Arrays:
      int off = 0;
      for (int b = 1; b >= 0; --b)
      {
        // Key: "bids" or "asks":
        bool isBid(b);
        if (isBid)
          currSnap = strstr(curr, "\"bids\":[[") + 9;
        else
          currSnap = strstr(curr, "\"asks\":[[") + 9;

        // Bids or Asks: Array of length <= 10 of Entries: [\"Px\",\"Qty\"]:
        for (int i = 0; i < 10; ++i)
        {
          double px = strtod(currSnap, &after);
          currSnap  = strchr(after, ',') + 1;

          long qty = LONG_MIN;
          currSnap = utxx::fast_atoi<long, false>(currSnap, end, qty);
          currSnap = strstr(currSnap, "],[");
          if (currSnap != nullptr)
            currSnap += 3;

          // Save the "px" and "qty":
          MDEntryST& mde = m_mdMsg.m_entries[off];  // REF!
          mde.m_entryType =
            isBid ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer;
          mde.m_px  = PriceT(px);
          mde.m_qty = QtyN(qty);
          ++off;
        }
      }

      // Exchange timestamp:
      curr = strstr(curr, "\"timestamp\":\"") + 13;
      //"XXXX-XX-XXTXX:XX:XX.XXXZ"
      m_mdMsg.m_exchTS = DateTimeToTimeValTZ(curr);

      //-------------------------------------------------------------------//
      // SnapShot done, process it in "EConnector_MktData":                //
      //-------------------------------------------------------------------//
      constexpr  bool IsSnapShot       = true;
      constexpr  bool ChangeIsPartFill = true;   // This is normal
      constexpr  bool NewEncdAsChange  = false;  // Very rare anyway
      constexpr  bool ChangeEncdAsNew  = false;  // ditto
      constexpr  bool DynInitMode      = false;

      DEBUG_ONLY(bool ok =)
        EConnector_MktData::UpdateOrderBooks
        <IsSnapShot,
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
        (SeqNum(), m_mdMsg, DynInitMode, a_ts_recv, a_ts_handl);
      assert(ok);
    }
    else
    if (isOrderBookL2)
    {
      LOG_INFO(4, "EConnector_WS_BitMEX_MDC::ProcessWSMsg(OrderBookL2): "
                  "Msg={}", a_msg_body)

      bool isUpdate = strncmp(curr, "update", 6) == 0;
      bool isSnapSh = strncmp(curr, "partial", 7) == 0;
      bool isRemove = strncmp(curr, "delete", 6) == 0;
      bool isInsert = strncmp(curr, "insert", 6) == 0;
      if (utxx::unlikely(!(isSnapSh || isUpdate || isInsert || isRemove)))
      {
        LOG_INFO(1,
          "EConnector_WS_BitMEX_MDC::ProcessWSMsg: Received weird OrderBookL2:"
          " {}", a_msg_body)
        return true;
      }

      curr = strstr(a_msg_body, "\"data\":[");
      if (utxx::unlikely(curr == nullptr))
      {
        LOG_ERROR(1, "EConnector_WS_BitMEX_MDC::ProcessWSMsg: No data")
        return true;
      }
      curr += 8;

      if (isSnapSh)
      {
        m_isInitialized = true;
        return ProcessUpdates<true, false, false>
               (curr, end, a_ts_recv, a_ts_handl);
      }
      else if (isRemove && m_isInitialized)
        return ProcessUpdates<false, true, false>
               (curr, end, a_ts_recv, a_ts_handl);
      else if (isInsert && m_isInitialized)
        return ProcessUpdates<false, false, true>
               (curr, end, a_ts_recv, a_ts_handl);
      else if (isUpdate && m_isInitialized)
        return ProcessUpdates<false, false, false>
               (curr, end, a_ts_recv, a_ts_handl);
    }
    else
    {
      LOG_ERROR(2, "EConnector_WS_BitMEX_MDC::ProcessWSMsg: "
                   "Spurious Msg={}", a_msg_body)
    }
    return true;
  }

  template<bool IsSnapShot, bool IsDelete, bool IsInsert>
  bool EConnector_WS_BitMEX_MDC::ProcessUpdates
  (
    char const* a_curr,
    char const* a_end,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    // !IsSnapShot && !IsDelete && !IsInsert = IsUpdate
    assert(!(IsSnapShot && (IsDelete || IsInsert)) || !IsSnapShot);
    assert(!IsDelete || !IsInsert);

    char const* curr = a_curr;
    assert(*curr == '{');

    // Do this once to get the symbol
    SKP_STR("{\"symbol\":\"")

    InitFixedStr(&m_mdMsg.m_symbol, curr,
                 size_t(strchr(curr, '"') - curr));

    int entryCount = 0;
    while (true)
    {
      curr = strstr(curr, "\"id\":");
      if (curr == nullptr)
        break;
      curr += 5;

      MDEntryST& mde = m_mdMsg.m_entries[entryCount];
      ++entryCount;

      unsigned long id = 0;
      curr = utxx::fast_atoi<unsigned long, false>(curr, a_end, id);
      mde.m_px = GetPxLevel(m_mdMsg.m_symbol, id);

      SKP_STR(",\"side\":\"")
      bool isBuy = strncmp(curr, "Buy", 3) == 0;
      mde.m_entryType = isBuy ? FIX::MDEntryTypeT::Bid
                              : FIX::MDEntryTypeT::Offer;

      if (utxx::unlikely(IsDelete))
      {
        LOG_INFO(4, "EConnector_WS_BitMEX_MDC::ProcessUpdates: "
                    "DELETE: Px={}", double(mde.m_px))
        mde.m_qty = QtyN::Zero();
        continue;
      }

      curr = strstr(curr, "size\":");
      assert(curr != nullptr);
      curr += 6;

      QR qty = NaN<QR>;
      curr   = utxx::fast_atoi<QR, false>(curr, a_end, qty);
      mde.m_qty = QtyN(qty);

      if (IsSnapShot || IsInsert)
      {
        char* after = nullptr;
        SKP_STR(",\"price\":")
        double parsedPx = strtod(curr, &after);
        assert(curr < after);

        LOG_INFO(4, "EConnector_WS_BitMEX_MDC::ProcessUpdates: "
                    "INSERT: Px={}, Qty={}, CalcedPx={}",
                    parsedPx, double(qty), double(mde.m_px))

        mde.m_px = (mde.m_px == PriceT(parsedPx)) ? mde.m_px
                                                  : PriceT(parsedPx);
        curr = after;
      }
      else
      {
        LOG_INFO(4, "EConnector_WS_BitMEX_MDC::ProcessUpdates: "
                    "UPDATE: Px={}, Qty={}",
                    double(mde.m_px), double(qty))

        assert(*curr == '}');
      }
    }

    m_mdMsg.m_nEntrs = entryCount;

    constexpr  bool ChangeIsPartFill = false;  // This is normal
    constexpr  bool NewEncdAsChange  = false;  // Very rare anyway
    constexpr  bool ChangeEncdAsNew  = false;  // ditto
    constexpr  bool DynInitMode      = false;

    EConnector_MktData::UpdateOrderBooks
    <IsSnapShot,
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
    (SeqNum(), m_mdMsg, DynInitMode, a_ts_recv, a_ts_handl);

    return true;
  }
}  // End namespace MAQUETTE
