// vim:ts=2:et
//===========================================================================//
//        "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_MDC.cpp":         //
//                         WS-Based MDC for BitFinex                         //
//===========================================================================//
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_MDC.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/BitFinex/Configs_WS.h"
#include "Venues/BitFinex/SecDefs.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/convert.hpp>
#include <utxx/error.hpp>
#include <cstring>
#include <cstdlib>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "GetAllSecDefs":                                                        //
  //=========================================================================//
  std::vector<SecDefS> const* EConnector_WS_BitFinex_MDC::GetSecDefs()
  {
    return &BitFinex::SecDefs;
  }

  //=========================================================================//
  // "GetWSConfig":                                                          //
  //=========================================================================//
  H2WSConfig const& EConnector_WS_BitFinex_MDC::GetWSConfig(MQTEnvT a_env)
  {
    // XXX: For BitFinex, only "Prod" is actually supported, but for an MDC,
    // this does not matter, so we always return the "Prod" config here:
    //
    auto cit = BitFinex::Configs_WS_MDC.find(a_env);
    assert(cit != BitFinex::Configs_WS_MDC.cend());
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_BitFinex_MDC::EConnector_WS_BitFinex_MDC
  (
    EPollReactor*                      a_reactor,
    SecDefsMgr*                        a_sec_defs_mgr,
    vector<string>              const* a_only_symbols,   // NULL=UseFullList
    RiskMgr*                           a_risk_mgr,
    boost::property_tree::ptree const& a_params,
    EConnector_MktData*                a_primary_mdc     // Normally NULL
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    // NB: As usual, AccountKey is the Connector Instance Name. It may look like
    // {AccountPfx}-WS-BitFinex-MDC-{Env}:
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "BitFinex",
      0,                               // XXX: No extra ShM data as yet...
      a_reactor,
      false,                           // No BusyWait
      a_sec_defs_mgr,
      BitFinex::SecDefs,
      a_only_symbols,                  // Restricting BitFinex::SecDefs
      a_params.get<bool>("ExplSettlDatesOnly",   false),
      false,                           // Presumably no Tenors in SecIDs
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
    // "EConnector_WS_BitFinex_MDC" Itself:                                  //
    //-----------------------------------------------------------------------//
    m_mdMsg   (),
    m_channels()
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    // XXX: Currently, no more than 30 Streams can be subscribed for, incl the
    // Trades:
    int maxInstrs = EConnector_MktData::HasTrades() ? 15 : 30;

    if (utxx::unlikely(int(m_usedSecDefs.size())  > maxInstrs))
      throw utxx::runtime_error
            ("EConnector_WS_BitFinex_MDC::Ctor: Too many Instrs requested: ",
             m_usedSecDefs.size(), ", MaxAllowed=", maxInstrs, ": WithTrades=",
             EConnector_MktData::HasTrades());
  }

  //=========================================================================//
  // Dtor is Trivial:                                                        //
  //=========================================================================//
  EConnector_WS_BitFinex_MDC::~EConnector_WS_BitFinex_MDC() noexcept {}

  //=========================================================================//
  // "InitSession":                                                          //
  //=========================================================================//
  void EConnector_WS_BitFinex_MDC::InitSession() const
  {
    LOG_INFO(2,
      "EConnector_WS_BitFinex_MDC::InitSession: Running InitWSHandShake")

    // IMPORTANT: Reset the Channels, as we are going to make new Subcrs (poss-
    // ibly after a reconnect):
    m_channels.clear();

    // Proceed with the HTTP/WS HandShake using the following Path:
    ProWS::InitWSHandShake("/ws/2");
  }

  //=========================================================================//
  // "InitLogOn":                                                            //
  //=========================================================================//
  void EConnector_WS_BitFinex_MDC::InitLogOn()
  {
    // Clear the existing channel map in case of reconnect
    m_channels.clear();

    // For BitFinex MDC, there is no LogOn per se -- here we subscribe to  Mkt
    // Data. NB: "len" is the number of Orders in the initial SnapShot we will
    // get. Very roughly, it takes ~60 bytes/Order. In our WebSocket impl,  we
    // support frames up to 64k, ie we can request some 1000 Orders. But appa-
    // rently, the maximum allowed by BitFinex is 250:
    LOG_INFO(2,
      "EConnector_WS_BitFinex_MDC::InitLogOn: Subscribing to MktData Channels")

    char  buffBook[128];
    char* symPos;

    if (!RawBooks)
      symPos = stpcpy(buffBook,
             "{\"event\":\"subscribe\",\"channel\":\"book\","
             "\"precision\":\"P0\","
             "\"len\":100,\"symbol\":\"");
    else
      symPos = stpcpy(buffBook,
             "{\"event\":\"subscribe\",\"channel\":\"book\",\"prec\":\"R0\","
             "\"len\":250,\"symbol\":\"");

    char  buffTrades[128];
    char* symPosTrades =
      stpcpy(buffTrades,
             "{\"event\":\"subscribe\",\"channel\":\"trades\",\"symbol\":\"");

    for (SecDefD const* instr : m_usedSecDefs)
    {
      // Compete the Subscr JSON:
      assert(instr != nullptr);

      // Request for Order Books:
      char* curr = stpcpy(symPos, instr->m_Symbol.data());
      curr       = stpcpy(curr, "\"}");
      assert(size_t(curr - buffBook) < sizeof(buffBook));

      // Copy the data into the sending buffer, and send a WS frame:
      ProWS::PutTxtData(buffBook);
      ProWS::SendTxtFrame();

      if (EConnector_MktData::HasTrades())
      {
        // Request for Trades:
        curr = stpcpy(symPosTrades, instr->m_Symbol.data());
        curr = stpcpy(curr, "\"}");
        assert(size_t(curr - buffTrades) < sizeof(buffTrades));

        // Copy the data into the sending buffer, and send a WS frame:
        ProWS::PutTxtData  (buffTrades);
        ProWS::SendTxtFrame();
      }
    }
    MDCWS::LogOnCompleted(utxx::now_utc());
  }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Returns "true" to continue receiving msgs, "false" to stop.
  // NB: Apparently, each WebSocket frame contains just 1 JSON msg, so there is
  // no loop here:
  //
  bool EConnector_WS_BitFinex_MDC::ProcessWSMsg
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

    bool  ok     = true;
    char* msgEnd = const_cast<char*>(a_msg_body + a_msg_len);

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
        unsigned    chID = 0;
        char const* data =
            utxx::fast_atoi<unsigned, false>(a_msg_body + 1, msgEnd, chID);

        CHECK_ONLY
        (
          if (utxx::unlikely(chID == 0 || *data != ','))
          {
            LOG_ERROR(2,
              "EConnector_WS_BitFinex_MDC::ProcessWSMsg: MalFormatted ChannelID"
              ": After={} in Msg={}", data, a_msg_body)
            return false;
          }
        )
        ++data;

        // NB: Check whether it is a HeartBeat or trade update (simply ignored):
        if (utxx::unlikely(strncmp(data, "\"hb\"", 4) == 0 ||
                           strncmp(data, "\"tu\"", 4) == 0))
          return true;

        // Get the OrderBook:
        auto it = m_channels.find(chID);

        if (utxx::likely(it != m_channels.cend()))
        {
          OrderBook* ob = it->second;
          assert(ob != nullptr);

          InitFixedStr(&m_mdMsg.m_symbol, m_symbols[chID]);

          if (strncmp(data, "\"te\"", 4) == 0)
          {
            data += 5;
            // Process trade
            if (utxx::likely(ob->IsInitialised()))
              this->ProcessSingleTrade(data, msgEnd, ob, a_ts_recv, a_ts_handl);
          }
          else
          {
            if (RawBooks)
              ok =
                utxx::likely(ob->IsInitialised())
                ? // Steady Mode: It is a single Order update (FullOrdersLog).
                  // NB: "ProcessOrder" returns the after-ptr if the Order is
                  // valid, or NULL otherwise. In this case, IsSnapShot=false:
                  //
                  (ProcessOrder<false>(data, msgEnd, ob, a_ts_recv,
                                        a_ts_handl) != nullptr)

                :  // Init   Mode: It should be a SnapShot (FullOrdersBook):
                  ProcessSnapShot(data, msgEnd, ob, a_ts_recv, a_ts_handl);
            else
              ok =
                utxx::likely(ob->IsInitialised())
                ? (ProcessUpdate<false>(data, msgEnd, ob, 0, a_ts_recv,
                                 a_ts_handl) != nullptr)
                : ProcessSnapShot(data, msgEnd, ob, a_ts_recv, a_ts_handl);
          }
        }
        else
        {
          LOG_ERROR(2,
            "EConnector_WS_BitFinex_MDC::ProcessWSMsg: Invalid ChannelID={} in "
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
              "EConnector_WS_BitFinex_MDC::ProcessWSMsg: MalFormatted Event in "
              "Msg={}", a_msg_body)
            return false;
          }
        )
        // What kind of Event is it?
        if (strstr(a_msg_body, "\"event\":\"subscribed\"") != nullptr)
        {
          //-----------------------------------------------------------------//
          // Subscription confirmed:                                         //
          //-----------------------------------------------------------------//
          // Get the Symbol:
          char const* symPos = strstr(a_msg_body, "\"symbol\":\"");
          CHECK_ONLY
          (
            if (utxx::unlikely(symPos == nullptr))
            {
              LOG_ERROR(2,
                "EConnector_WS_BitFinex_MDC::ProcessWSMsg: No Symbol in Msg={}",
               a_msg_body)
              return false;
            }
          )
          symPos += 10;  // Now points to the Symbol itself

          char const* symEnd = strchr(symPos, '"');
          CHECK_ONLY
          (
            if (utxx::unlikely(symEnd == nullptr))
            {
              LOG_ERROR(2,
                "EConnector_WS_BitFinex_MDC::ProcessWSMsg: UnTerminated Symbol "
                "in Msg={}", a_msg_body)
              return false;
            }
          )
          assert(symEnd > symPos);
          string symbol(symPos, size_t(symEnd - symPos));

          // Find the OrderBook for this Symbol (UseAltSymbol=false):
          OrderBook* ob =
            EConnector_MktData::FindOrderBook<false>(symbol.data());
          CHECK_ONLY
          (
            if (utxx::unlikely(ob == nullptr))
            {
              LOG_ERROR(2,
                "EConnector_WS_BitFinex_MDC::ProcessWSMsg: OrderBook Not Found"
                ": Symbol={} in Msg={}", symbol, a_msg_body)
              return false;
            }
          )
          // Get the ChannelID:
          char const* idPos = strstr(a_msg_body, "\"chanId\":");
          CHECK_ONLY
          (
            if (utxx::unlikely(idPos == nullptr))
            {
              LOG_ERROR(2,
                "EConnector_WS_BitFinex_MDC::ProcessWSMsg: No ChannelID in Msg="
                "{}", a_msg_body)
              return false;
            }
          )
          idPos += 9; // Now points to the ChannelID itself

          // TillEOL=false:
          unsigned chID = 0;
          (void)utxx::fast_atoi<unsigned, false>(idPos, msgEnd, chID);
          CHECK_ONLY
          (
            if (utxx::unlikely(chID == 0))
            {
              LOG_ERROR(2,
                "EConnector_WS_BitFinex_MDC::ProcessWSMsg: Invalid ChannelID "
                "in Msg={}", a_msg_body)
              return false;
            }
          )
          // Finally, map ChannelID => OrderBook:
          m_channels[chID] = ob;
          m_symbols[chID]  = symbol;
          LOG_INFO(1,
            "EConnector_WS_BitFinex_MDC::ProcessWSMsg: Subscription Confirmed "
            "for {}: ChannelID={}", ob->GetInstr().m_FullName.data(), chID)
        }
        else
        if (strstr(a_msg_body, "\"event\":\"error\"") != nullptr)
        {
          //-----------------------------------------------------------------//
          // Some Error: Log it and Exit:                                    //
          //-----------------------------------------------------------------//
          LOG_ERROR(1,
            "EConnector_WS_BitFinex_MDC::ProcessWSMsg: Got {}", a_msg_body)
          ok = false;
        }
        else
          //-----------------------------------------------------------------//
          // Any other Event -- just log it:                                 //
          //-----------------------------------------------------------------//
          LOG_INFO(1,
            "EConnector_WS_BitFinex_MDC::ProcessWSMsg: {}", a_msg_body)
      }
    }
    //-----------------------------------------------------------------------//
    // Exception Handling:                                                   //
    //-----------------------------------------------------------------------//
    catch (exception const& exn)
    {
      LOG_ERROR(2, "EConnector_WS_BitFinex_MDC::ProcessWSMsg: {}, Msg={}",
                exn.what(), a_msg_body)
      ok = false;
    }
    // In any case: Restore the original trailing char and continue:
    return ok;
  }

  //=========================================================================//
  // "ProcessSingleTrade":                                                   //
  //=========================================================================//
  bool EConnector_WS_BitFinex_MDC::ProcessSingleTrade
  (
    char const*    a_data,
    char const*    a_end,
    OrderBook*     a_ob,
    utxx::time_val a_ts_recv,
    utxx::time_val UNUSED_PARAM(a_ts_handl)
  )
  {
    assert(a_data != nullptr && *a_data == '[' && a_ob != nullptr);
    char const* curr = a_data + 1;

    // Use single entry for all trades
    MDEntryST& mde = m_mdMsg.m_entries[0];

    // TrdMatchID
    curr = utxx::fast_atoi<OrderID, false>(curr, a_end, mde.m_execID);
    ++curr;

    // Timestamp
    uint64_t msTs;
    curr             = utxx::fast_atoi<uint64_t, false>(curr, a_end, msTs);
    m_mdMsg.m_exchTS = utxx::time_val(utxx::msecs(msTs));
    ++curr;

    // Qty and Side
    char*  after = nullptr;
    double qty   = strtod(curr, &after);
    assert(curr != after);

    curr         = after + 1;
    mde.m_qty    = QtyN(Abs(qty));
    // FIXME: very strange behaviour when we encounter 1e-8 qty
    if (Abs(qty) < 1e-7)
      return true;

    mde.m_entryType = FIX::MDEntryTypeT::Trade;
    FIX::SideT aggrSide = qty < 0 ? FIX::SideT::Sell : FIX::SideT::Buy;

    // Price
    double px = strtod(curr, &after);
    assert(curr != after);
    curr      = after;
    mde.m_px  = PriceT(px);

    CHECK_ONLY
    (
      LOG_INFO(4, "EConnector_WS_BitFinex_MDC::ProcessSingleTrade: "
                  "Processed {} trade: Qty={}, Px={}.",
                  (qty < 0 ? "SELL" : "BUY"), Abs(qty), px)
    )
    EConnector_MktData::ProcessTrade<false, QT, QR, OMC>
      (m_mdMsg, mde, nullptr, *a_ob, mde.m_qty, aggrSide, a_ts_recv);

    return true;
  }

  //=========================================================================//
  // "ProcessSnapShot": Initial SnapShot:                                    //
  //=========================================================================//
  bool EConnector_WS_BitFinex_MDC::ProcessSnapShot
  (
    char const*    a_data,
    char const*    a_end,
    OrderBook*     a_ob,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    assert(a_data != nullptr && *a_data == '[' && a_ob != nullptr);

    // Count number of values per order (has to be 3 values or 2 commas)
    // If !=3 - it's not an order book snapshot
    int         count  = 0;
    char const* ptrBeg = a_data;

    while (*ptrBeg != ']')
    {
      if (*ptrBeg == ',')
        ++count;
      if (count > 2)
        return true;
      ++ptrBeg;
    }

    // This if a FullOrdersLog SnapShot:
    if (RawBooks)
    {
      // NB: We assume that there are NO Bid-Ask collisions  in BitFinex Snap-
      // Shots, so the SnapShot Orders can safely be processed in any or order
      // (eg in the order of their arrival in the Msg) -- no buffering needed:
      char const* currOrd = a_data + 1;

      // Each Order is encapsulated in []s:
      while (currOrd < a_end && *currOrd == '[')
      {
        char const* curr =   // NB: IsSnapShot=true:
          ProcessOrder<true>(currOrd, a_end, a_ob, a_ts_recv, a_ts_handl);

        // Check for errors (already logged):
        if (utxx::unlikely(curr == nullptr))
          return false;

        // If OK, the next char should normally be a ',' or a ']':
        switch (*curr)
        {
        case ',':
          // To the next Order:
          currOrd = curr + 1;
          break;

        case ']':
          // SnapShot Done. The OrderBook is now Initialised:
          a_ob->SetInitialised();
          return true;

        default:
          // This should not happen:
          LOG_ERROR(1,
            "EConnector_WS_BitFinex_MDC::ProcessSnapShot: Invalid Data: {}",
            curr)
          return false;
        }
      }
    }
    else
    // This is an AggregatedOrderBook snapshot:
    {
      char const* currUpd = a_data + 1;

      int off = 0;
      // Each Order is encapsulated in []s:
      while (currUpd < a_end && *currUpd == '[')
      {
        char const* curr =
          ProcessUpdate<true>(currUpd, a_end, a_ob, off, a_ts_recv, a_ts_handl);

        ++off;
        // Check for errors (already logged):
        if (utxx::unlikely(curr == nullptr))
          return false;

        // If OK, the next char should normally be a ',' or a ']':
        switch (*curr)
        {
          case ',':
          {
            // To the next Order:
            currUpd = curr + 1;
            break;
          }
          case ']':
          {
            m_mdMsg.m_nEntrs = off;

            EConnector_MktData::UpdateOrderBooks
            <
              true,
              IsMultiCast,
              WithIncrUpdates,
              false,
              WithRelaxedOBs,
              ChangeIsPartFill,
              false,         // !NewEncdAsChange (here, that is)
              ChangeEncdAsNew,
              IsFullAmt,
              IsSparse,
              FindOrderBookBy::Symbol,
              QT,
              QR,
              OMC
            >
            (SeqNum(), m_mdMsg, false, a_ts_recv, a_ts_handl);
            return true;
          }
          default:
          // This should not happen:
          {
            LOG_ERROR(1,
              "EConnector_WS_BitFinex_MDC::ProcessSnapShot: Invalid Data: {}",
              curr)
            return false;
          }
        }
      }
    }
    // XXX: We may get here if the SnapShot was empty:
    LOG_ERROR(2,
      "EConnector_WS_BitFinex_MDC::ProcessSnapShot: Empty SnapShot(?): {}",
      a_data)
    // Is this OK or not? Probably not:
    return false;
  }

  //=========================================================================//
  // "ProcessUpdate":                                                         //
  //=========================================================================//
  // Parsing and processing of a singler PxLevel update:
  //
  template <bool IsSnapShot>
  char const* EConnector_WS_BitFinex_MDC::ProcessUpdate
  (
    char const*    a_data,
    char const*    a_end,
    OrderBook*     DEBUG_ONLY(a_ob),
    int            a_offset,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    if (utxx::likely(!IsSnapShot))
      m_mdMsg.m_nEntrs = 1;

    assert(a_data != nullptr && *a_data == '[' && a_ob != nullptr);

    double px  = NaN<double>;
    int count  = INT_MIN;
    double qty = NaN<double>;

    char const* curr = a_data + 1;
    char* after      = nullptr;

    px = strtod(curr, &after);
    assert(curr != after);

    curr = after + 1;
    curr = utxx::fast_atoi<int, false>(curr, a_end, count);
    assert(curr != nullptr);

    ++curr;
    qty = strtod(curr, &after);
    assert(curr != after);
    curr = after;

    MDEntryST& mde = m_mdMsg.m_entries[a_offset];
    mde.m_entryType =
      (qty > 0) ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer;
    mde.m_px  = PriceT(px);
    mde.m_qty = count == 0 ? QtyN(): QtyN(Abs(qty));

    if (!IsSnapShot)
    {
      // It is NOT a snapshot, but a regular update
      EConnector_MktData::UpdateOrderBooks
      <
        false,
        IsMultiCast,
        WithIncrUpdates,
        false,
        WithRelaxedOBs,
        ChangeIsPartFill,
        false,         // !NewEncdAsChange (here, that is)
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

    return curr + 1;
  }


  //=========================================================================//
  // "ProcessOrder":                                                         //
  //=========================================================================//
  // Parsing and processing of a single Order, either a stand-alone one (FOL)
  // or belonging to a SnapShot (FOB). Returns the after-ptr (or NULL in case
  // of an error):
  //
  template <bool IsSnapShot>
  char const* EConnector_WS_BitFinex_MDC::ProcessOrder
  (
    char const*    a_data,
    char const*    a_end,
    OrderBook*     a_ob,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    assert(a_data != nullptr && *a_data == '[' && a_ob != nullptr);

    //-----------------------------------------------------------------------//
    // Extract the OrderID, Px and Qty:                                      //
    //-----------------------------------------------------------------------//
    // (*) Px==0 is Cancel;
    // (*) Qty < 0 is Ask, Qty > 0 is Bid (even for Cancel):
    //
    OrderID ordID = 0;
    double  px    = NaN<double>;
    double  qty   = NaN<double>;

    // OrderID:
    // NB: TillEOL=false:
    char const* curr  =
      utxx::fast_atoi<OrderID, false>(a_data + 1, a_end, ordID);

    CHECK_ONLY
    (
      if (utxx::unlikely(curr == nullptr || *curr != ',' || ordID == 0))
      {
        LOG_ERROR(2,
          "EConnector_WS_BitFinex_MDC::ProcessOrder: Invalid OrderID in {}",
          a_data)
        return nullptr;
      }
    )
    char* after = nullptr;

    // Px:
    px   = strtod(curr + 1, &after);
    curr = after;
    CHECK_ONLY
    (
      if (utxx::unlikely(*curr != ',' || !IsFinite(px)))
      {
        LOG_ERROR(2,
          "EConnector_WS_BitFinex_MDC::ProcessOrder: Invalid Px in {}", a_data)
        return nullptr;
      }
    )
    // Qty:
    qty  = strtod(curr + 1, &after);
    curr = after;
    CHECK_ONLY
    (
      if (utxx::unlikely(*curr != ']' || !IsFinite(qty)))
      {
        LOG_ERROR(2,
          "EConnector_WS_BitFinex_MDC::ProcessOrder: Invalid Qty in {}",
          a_data)
        return nullptr;
      }
    )
    //-----------------------------------------------------------------------//
    // Apply the Order parsed:                                               //
    //-----------------------------------------------------------------------//
    assert(IsFinite(px) && IsFinite(qty));

    // If the Px is 0, it is a "Delete"; in that case, Qty must be +-1 only;
    // in all other cases, Px > 0. Note that Qty is never 0:
    //
    CHECK_ONLY
    (
      if (utxx::unlikely
         (px < 0.0 || (px == 0.0 && Abs(qty) != 1.0) || qty == 0.0))
      {
        LOG_ERROR(2,
          "EConnector_WS_BitFinex_MDC::ProcessOrder: Invalid Px={} and/or Qty="
          "{} in {}", px, qty, a_data);
        return nullptr;
      }
    )
    // XXX: "New" and "Change" actions cannot be distinhuished based on the
    // protocol info, so we pass the "Change" param  and  use "NewEncdAsChange"
    // flag:
    constexpr bool NewEncdAsChange = !IsSnapShot;
    bool           isDelete        = (px == 0.0);
    bool           isBid           = (qty > 0.0);

    FIX::MDUpdateActionT action =
      isDelete ? FIX::MDUpdateActionT::Delete : FIX::MDUpdateActionT::Change;

    // Now apply this Order Update:
    auto res =
      EConnector_MktData::ApplyOrderUpdate
      <IsSnapShot,       WithIncrUpdates, StaticInitMode,
       ChangeIsPartFill, NewEncdAsChange, ChangeEncdAsNew,
       WithRelaxedOBs,   IsFullAmt,       IsSparse,        QT,  QR>
      (
        ordID,
        action,
        a_ob,
        isBid    ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer,
        isDelete ? PriceT () : PriceT(px),
        isDelete ? QtyN(0.0) : QtyN  (Abs(qty)),
        0,  // No RptSeq
        0   // No SeqNum
      );
    OrderBook::UpdateEffectT upd   = get<0>(res);
    OrderBook::UpdatedSidesT sides = get<1>(res);

    // NB: UpdateEffect=NONE does not produce a warning (it simply means a
    // far-away Px which we cannot accommodate), but an ERROR does:
    if (utxx::unlikely(upd == OrderBook::UpdateEffectT::ERROR))
      LOG_WARN(2,
               "EConnector__MDC::ProcessSnapShot: OrderID={}, Px={}, Side={}: "
               "OrderBook UpdateEffect=ERROR",
               ordID, px, (isBid ? "Bid" : "Ask"))

    // Generic Case: NB: We do not get the ExchTS from the underlying Proto-
    // col, so set it to the RecvTS:
    (void) EConnector_MktData::AddToUpdatedList
      (a_ob, upd, sides, a_ts_recv, a_ts_recv, a_ts_handl, utxx::now_utc());

    //-----------------------------------------------------------------------//
    // Return the ptr BEYOND the closing ']':                                //
    //-----------------------------------------------------------------------//
    return curr + 1;
  }
}  // End namespace MAQUETTE
