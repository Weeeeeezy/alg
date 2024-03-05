// vim:ts=2:et
//===========================================================================//
//     "Connectors/H2WS/TDAmeritrade/EConnector_H1WS_TDAmeritrade.cpp":      //
//             WS and HTTP/1.1 Based MDC/OMC for TDAmeritrade                //
//===========================================================================//
#include "Connectors/H2WS/TDAmeritrade/EConnector_H1WS_TDAmeritrade.h"

#include "Basis/BaseTypes.hpp"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "Protocols/FIX/Msgs.h"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/TDAmeritrade/Configs_H2WS.h"
#include "Venues/TDAmeritrade/SecDefs.h"
#include <cstring>
#include <stdexcept>
#include <utxx/error.hpp>
#include <type_traits>

namespace MAQUETTE {
//=========================================================================//
// Config and Account Mgmt:                                                //
//=========================================================================//
H2WSConfig const &EConnector_H1WS_TDAmeritrade::GetWSConfig(MQTEnvT a_env) {
  // NB: For TDAmeritrade, only "Prod" is available:
  if (a_env != MQTEnvT::Prod)
    throw utxx::badarg_error(
        "EConnector_H1WS_TDAmeritrade::GetWSConfig: UnSupported Env=",
        a_env.to_string());

  return TDAmeritrade::Config_WS;
}

H2WSConfig const &EConnector_H1WS_TDAmeritrade::GetHTTPConfig(MQTEnvT a_env) {
  // NB: For TDAmeritrade, only "Prod" is available:
  if (a_env != MQTEnvT::Prod)
    throw utxx::badarg_error(
        "EConnector_H1WS_TDAmeritrade::GetHTTPConfig: UnSupported Env=",
        a_env.to_string());

  return TDAmeritrade::Config_HTTP;
}

//=========================================================================//
// "GetSecDefs":                                                           //
//=========================================================================//
std::vector<SecDefS> const *EConnector_H1WS_TDAmeritrade::GetSecDefs() {
  return &TDAmeritrade::SecDefs;
}

//=========================================================================//
// Non-Default Ctor:                                                       //
//=========================================================================//
EConnector_H1WS_TDAmeritrade::EConnector_H1WS_TDAmeritrade
(
  EPollReactor*                       a_reactor,
  SecDefsMgr*                         a_sec_defs_mgr,
  std::vector<std::string> const*     a_only_symbols, // NULL=UseFullList
  RiskMgr*                            a_risk_mgr,
  boost::property_tree::ptree const&  a_params,
  EConnector_MktData*                 a_primary_mdc // Normally NULL
)
: //-----------------------------------------------------------------------//
  // "EConnector": Virtual Base:                                           //
  //-----------------------------------------------------------------------//
  EConnector
  (
    a_params.get<std::string>("AccountKey"),
    "TDAmeritrade",
    0,              // XXX: No extra ShM data as yet...
    a_reactor,
    false,          // No BusyWait
    a_sec_defs_mgr,
    TDAmeritrade::SecDefs,
    a_only_symbols, // Restricting TDAmeritrade::SecDefs
    a_params.get<bool>("ExplSettlDatesOnly", false),
    false,          // Presumably no Tenors in SecIDs
    a_risk_mgr,
    a_params,
    QT,
    std::is_floating_point_v<QR>
  ),
  //-----------------------------------------------------------------------//
  // "EConnector_MKtData":                                                 //
  //-----------------------------------------------------------------------//
  EConnector_MktData
  (
    true, // Yes, this MDCC is Enabled!
    a_primary_mdc, a_params,
    false,    // No FullAmt, presumably!
    IsSparse, // Use Sparse OrderBooks?
    HasSeqNums,
    false, // And no RptSeqs  either
    false, // So  no "continuous" RptSeqs
    MktDepth,
    // Trades must be enabled both statically and dynamically:
    HasTrades && a_params.get<bool>("WithTrades"),
    false, // Trades are NOT from FOL normally
    false  // No  DynInitMode for TCP
  ),
  //-----------------------------------------------------------------------//
  // "EConnector_OrdMgmt":                                                 //
  //-----------------------------------------------------------------------//
  // NB: For PipeLinedReqs, we have a conjunction of Static and Dynamic conf
  // params:
  EConnector_OrdMgmt(true, // Yes, OMC is enabled!
                     a_params, ThrottlingPeriodSec, MaxReqsPerPeriod,
                     PipeLineMode, SendExchOrdIDs, UseExchOrdIDsMap,
                     HasAtomicModify, HasPartFilledModify, HasExecIDs,
                     HasMktOrders),
  //-----------------------------------------------------------------------//
  // "TD_H1WS_Connector":                                                  //
  //-----------------------------------------------------------------------//
      TDH1WS("TDAmeritrade-MDC-OMC", GetHTTPConfig(m_cenv).m_HostName,
             GetHTTPConfig(m_cenv).m_Port, a_params),
  //-----------------------------------------------------------------------//
  // "EConnector_H1WS_TDAmeritrade" Itself:                                //
  //-----------------------------------------------------------------------//
      m_useBBA(a_params.get<bool>("UseBBA", false)),
      m_trades_only(a_params.get<bool>("TradesOnly", false)), m_mdMsg() {
  if (m_useBBA)
    throw std::runtime_error("UseBBA for EConnector_H1WS_TDAmeritrade is "
                             "not working yet");

  for (SecDefD const *instr : EConnector::m_usedSecDefs) {
    assert(instr != nullptr);
    this->m_positions.insert({instr, 0.0});
  }
}

//-------------------------------------------------------------------------//
// "OnTDWSLogOn":                                                          //
//-------------------------------------------------------------------------//
void EConnector_H1WS_TDAmeritrade::OnTDWSLogOn() {
  // MDC: subscribe to market data
  {
    std::stringstream params;
    params << "\"keys\":\"";

    bool frst = true;
    for (SecDefD const *instr : EConnector::m_usedSecDefs) {
      assert(instr != nullptr);
      if (frst)
        frst = false;
      else
        params << ',';

      params << instr->m_Symbol.data();

      this->m_positions.insert({instr, 0.0});
    }
    params << "\",\"fields\":\"";

    if (!m_trades_only) {
      auto quote_params = params.str();
      if (m_useBBA) {
        // only get best bid/ask
        quote_params += "0,1,2,4,5\"";
      } else {
        // we're subscribing to Level-2
        quote_params += "0,1,2,3\"";
      }

      SendWSReq(m_useBBA ? "QUOTE" : "LISTED_BOOK", "SUBS",
                quote_params.c_str());
      // SendWSReq(m_useBBA ? "LEVELONE_FUTURES" : "LISTED_BOOK", "SUBS",
      // quote_params.c_str());
    }

    auto trade_params = params.str();
    trade_params += "0,1,2,3,4\"";
    SendWSReq("TIMESALE_EQUITY", "SUBS", trade_params.c_str());
  }

  // OMC: subscribe to account activity and get current positions
  {
    char buf[128];
    auto pos = stpcpy(buf, "\"keys\":\"");
    pos = stpcpy(pos, TDH1WS::m_subscription_key.c_str());
    pos = stpcpy(pos, "\",\"fields\":\"0,1,2,3\"");
    SendWSReq("ACCT_ACTIVITY", "SUBS", buf);

    std::string positions_url =
        "/v1/accounts/" + m_account_id + "?fields=positions";
    if (!GET(positions_url.c_str())) {
      throw utxx::runtime_error("EConnector_H1WS_TDAmeritrade::OnTDWSLogOn: "
                                "Failed to get positions, Msg=",
                                std::string(GetResponse()));
    } else {
      // not performance critical, so use proper JSON parser
      try {
        rapidjson::Document json;
        json.Parse(GetResponse());

        if (json["securitiesAccount"].HasMember("positions")) {
          for (const auto &pos1 :
               json["securitiesAccount"]["positions"].GetArray()) {
            double short_qty = pos1["shortQuantity"].GetDouble();
            double long_qty = pos1["longQuantity"].GetDouble();

            double qty = 0.0;
            if ((short_qty == 0) && (long_qty > 0))
              qty = long_qty;
            else if ((short_qty > 0) && (long_qty == 0))
              qty = -short_qty;
            else
              throw utxx::runtime_error("Unexpected quantities: long ",
                                        long_qty, ", short ", short_qty);

            auto symbol = pos1["instrument"]["symbol"].GetString();
            auto ob = this->FindOrderBook<false>(symbol);

            if (ob != nullptr) {
              this->m_positions[&ob->GetInstr()] = qty;
              LOG_INFO(2,
                       "EConnector_H1WS_TDAmeritrade::OnTDWSLogOn: "
                       "{} position of {} units",
                       symbol, qty)
            } else {
              LOG_WARN(2,
                       "EConnector_H1WS_TDAmeritrade::OnTDWSLogOn: "
                       "Unrecognized {} position of {} units",
                       symbol, qty)
            }
          }
        }
      } catch (std::exception const &ex) {
        throw utxx::runtime_error("EConnector_H1WS_TDAmeritrade::OnTDWSLogOn: "
                                  "Failed to get positions, "
                                  "Exception=",
                                  std::string(ex.what()),
                                  ", Msg=", std::string(GetResponse()));
      }
    }
  }
}

//-------------------------------------------------------------------------//
// "ProcessWSData":                                                        //
//-------------------------------------------------------------------------//
bool EConnector_H1WS_TDAmeritrade::ProcessWSData(const char *a_service,
                                                 const char *a_command,
                                                 const char *a_content,
                                                 utxx::time_val a_ts_exch,
                                                 utxx::time_val a_ts_recv,
                                                 utxx::time_val a_ts_handl) {
  if (strcmp(a_command, "SUBS") != 0)
    // not interested in this, skip
    return true;

  int len = int(strlen(a_content));
  m_mdMsg.m_exchTS = a_ts_exch; // for market data

  if (strcmp(a_service, "ACCT_ACTIVITY") == 0)
    return ProcessAcctActivity(a_content, len, a_ts_recv, a_ts_exch);

  if (strncmp(a_service, "TIMESALE", 8) == 0)
    return ProcessTrade(a_content, len, a_ts_recv, a_ts_exch);

  if (m_useBBA && (strcmp(a_service, "QUOTE") == 0))
    return ProcessLevel1(a_content, len, a_ts_recv, a_ts_handl);

  if (!m_useBBA && (strcmp(a_service, "LISTED_BOOK") == 0))
    return ProcessLevel2(a_content, len, a_ts_recv, a_ts_handl);

  return true;
}

// We only process fills. We also get notified about new and cancelled
// orders, but sometimes there is a long delay (several minutes) and
// since TD doesn't support client order ids, it's hard to link back
// to the Req12 from here (especially for new orders where we don't)
// have an ExchOrdId. So handling order confirmation, cancellations,
// and replacements is done from the HTTP response
bool EConnector_H1WS_TDAmeritrade::ProcessAcctActivity(
    char const *a_msg_body, int a_msg_len, utxx::time_val a_ts_recv,
    utxx::time_val a_ts_exch) {
  // content can contain multiple {...},{...}
  try {
    auto curr = a_msg_body;
    char *end = const_cast<char *>(a_msg_body + a_msg_len);

    char *after = nullptr;
    long long_val;

    assert(*curr == '{');

    while (curr != nullptr) {
      SKP_STR("{\"seq\":")
      curr = SkipNFlds<1>(curr); // sequence number
      SKP_STR("\"key\":")
      curr = SkipNFlds<1>(curr); // streamer key

      SKP_STR("\"1\":")
      curr = SkipNFlds<1>(curr); // account number (except first message)
      SKP_STR("\"2\":\"")
      GET_STR(msg_type)

      // handle OrderFill and OrderPartialFill
      if ((strcmp(msg_type, "OrderFill") == 0) ||
          (strcmp(msg_type, "OrderPartialFill") == 0)) {
        SKP_STR(",\"3\":\"<?xml")

        curr = strstr(curr, "<OrderKey>");
        SKP_STR("<OrderKey>")
        long_val = 0;
        curr = utxx::fast_atoi<long, false>(curr, end, long_val);
        auto exch_id = ExchOrdID(OrderID(long_val));

        curr = strstr(curr, "<ExecutionInformation>");
        SKP_STR("<ExecutionInformation>")

        curr = strstr(curr, "<Quantity>");
        SKP_STR("<Quantity>")
        long_val = -1;
        curr = utxx::fast_atoi<long, false>(curr, end, long_val);
        QtyN last_qty(long_val);

        SKP_STR("</Quantity><ExecutionPrice>")
        auto last_px = PriceT(strtod(curr, &after));
        curr = after;

        SKP_STR("</ExecutionPrice><AveragePriceIndicator>")
        if (*curr == 'f')
          SKP_STR("false</AveragePriceIndicator><LeavesQuantity>")
        else
          SKP_STR("true</AveragePriceIndicator><LeavesQuantity>")

        long_val = -1;
        curr = utxx::fast_atoi<long, false>(curr, end, long_val);
        QtyN leaves_qty(long_val);

        SKP_STR("</LeavesQuantity><ID>")
        long_val = 0;
        curr = utxx::fast_atoi<long, false>(curr, end, long_val);
        auto exec_id = ExchOrdID(OrderID(long_val));

        constexpr unsigned Mask = unsigned(Req12::KindT::New) |
                                  unsigned(Req12::KindT::Modify) |
                                  unsigned(Req12::KindT::ModLegN);

        Req12 *req = GetReq12ByExchID<Mask>(exch_id);
        if (utxx::unlikely(req == nullptr)) {
          LOG_WARN(2,
                   "EConnector_H1WS_TDAmeritrade::ProcessAcctActivity: "
                   "Got fill for unknown order {}",
                   exch_id.GetOrderID())
        } else {
          auto fully_filled = (strcmp(msg_type, "OrderFill") == 0)
                                  ? FuzzyBool::True
                                  : FuzzyBool::False;

          EConnector_OrdMgmt::OrderTraded(
              this, this, nullptr, req->m_id, req->m_aos->m_id, req,
              FIX::SideT::UNDEFINED, FIX::SideT::UNDEFINED, exch_id,
              ExchOrdID(0), exec_id, PriceT(), PriceT(last_px), last_qty,
              leaves_qty, QtyN::Invalid(), QtyN::Invalid(), 0, fully_filled,
              a_ts_exch, a_ts_recv);

          // update positions
          auto instr = req->m_aos->m_instr;
          double delta =
              req->m_aos->m_isBuy ? double(last_qty) : -double(last_qty);
          assert(this->m_positions.count(instr) == 1);
          this->m_positions[instr] += delta;
        }
      }

      // move on to next message (if any)
      curr = strchr(curr, '{');
    }
  } catch (EPollReactor::ExitRun const &) {
    // This exception is propagated: It is not an error:
    throw;
  } catch (std::exception const &exn) {
    // Log the exception and stop:
    LOG_ERROR(1, "EConnector_H1WS_TDAmeritrade::ProcessWSData: {}", exn.what())
    return false;
  }

  return true;
}

bool EConnector_H1WS_TDAmeritrade::ProcessTrade(char const *a_msg_body,
                                                int a_msg_len,
                                                utxx::time_val a_ts_recv,
                                                utxx::time_val /*a_ts_handl*/) {
  auto curr = a_msg_body;
  char *end = const_cast<char *>(a_msg_body + a_msg_len);

  char *after = nullptr;
  long qty_value = 0;
  size_t timestamp = 0;

  assert(*curr == '{');
  while (curr != nullptr) {
    SKP_STR("{\"seq\":")
    curr = SkipNFlds<1>(curr); // sequence number

    SKP_STR("\"key\":\"")
    auto end_symbol = strchr(curr, '"');
    InitFixedStr(&m_mdMsg.m_Symbol, curr, size_t(end_symbol - curr));
    curr = end_symbol;

    SKP_STR("\",\"1\":")
    curr = utxx::fast_atoi<size_t, false>(curr, end, timestamp);
    m_mdMsg.m_exchTS = utxx::msecs(timestamp);

    m_mdMsg.m_nEntrs = 0;
    SKP_STR(",\"2\":")

    auto mde = &m_mdMsg.m_entries[m_mdMsg.m_nEntrs++];
    mde->m_entryType = FIX::MDEntryTypeT::Trade;
    mde->m_px = PriceT(strtod(curr, &after));
    curr = after;

    SKP_STR(",\"3\":")
    curr = utxx::fast_atoi<long, false>(curr, end, qty_value);
    mde->m_qty = QtyN(qty_value);

    //-----------------------------------------------------------------------//
    // Now Process this Trade:                                               //
    //-----------------------------------------------------------------------//
    // Get the OrderBook for this AltSymbol -- it will be needed for sending
    // notifications to the subscribed Strategies:
    //
    OrderBook const *ob =
        EConnector_MktData::FindOrderBook<false>(ToCStr(m_mdMsg.m_Symbol));
    CHECK_ONLY(if (utxx::unlikely(ob == nullptr)) {
      LOG_WARN(
          2, "EConnector_H1WS_TDAmeritrade::ProcessTrade: No OrderBook for {}",
          ToCStr(m_mdMsg.m_Symbol))
    } else)
    // Generic Case: NB: WithOrdersLog=false:
    EConnector_MktData::ProcessTrade<false, QT, QR,
                                     EConnector_H1WS_TDAmeritrade>(
        m_mdMsg, *mde, nullptr, *ob, mde->m_qty, FIX::SideT::UNDEFINED,
        a_ts_recv);

    // move on to next symbol (if any)
    curr = strchr(curr, '{');
  }

  return true;
}

// TD sends quantity updates without prices, we ignore these for
// now because we'd have to fetch the order book and get the
// best bid/ask price to supply it to the order book update.
// This may be costly and we don't care about quantity only
// updates at this point any way.
//
// NB: Unfortunately, we can get snapshots for bid or ask only,
// this means we need to retrieve or memoize the last best bid/ask.
// That is not implemented yet, it's recommended to just subscribe
// to depth updates (m_useBBA = false)
bool EConnector_H1WS_TDAmeritrade::ProcessLevel1(char const *a_msg_body,
                                                 int a_msg_len,
                                                 utxx::time_val a_ts_recv,
                                                 utxx::time_val a_ts_handl) {
  auto curr = a_msg_body;
  char *end = const_cast<char *>(a_msg_body + a_msg_len);

  char *after = nullptr;
  long qty_value = 0;

  assert(*curr == '{');
  while (curr != nullptr) {
    SKP_STR("{\"key\":\"")
    auto end_symbol = strchr(curr, '"');
    InitFixedStr(&m_mdMsg.m_Symbol, curr, size_t(end_symbol - curr));
    curr = end_symbol + 3; // skip ","

    m_mdMsg.m_nEntrs = 0;

    MDEntryST *bid = nullptr;
    MDEntryST *ask = nullptr;

    CHECK_ONLY(
        // very first message is difference, handle it for debug, but don't
        // bother in production
        while (isalpha(*curr)) {
          curr = SkipNFlds<1>(curr);
          SKP_STR("\"")
        })

    if (*curr == '1') {
      // we have a Bid price (and also Bid size in 4)
      SKP_STR("1\":")
      bid = &m_mdMsg.m_entries[m_mdMsg.m_nEntrs++];
      bid->m_entryType = FIX::MDEntryTypeT::Bid;
      bid->m_px = PriceT(strtod(curr, &after));
      bid->m_qty = QtyN(1L);
      curr = after;
      SKP_STR(",\"")
    }

    if (*curr == '2') {
      // we have a Ask price (and also Ask size in 5)
      SKP_STR("2\":")
      ask = &m_mdMsg.m_entries[m_mdMsg.m_nEntrs++];
      ask->m_entryType = FIX::MDEntryTypeT::Offer;
      ask->m_px = PriceT(strtod(curr, &after));
      ask->m_qty = QtyN(1L);
      curr = after;
      SKP_STR(",\"")
    }

    if (m_mdMsg.m_nEntrs == 0) {
      // we didn't get any new prices, move to next group
      curr = strchr(curr, '{');
      continue;
    }

    // currently not subscribing to 3 (Last Price)

    // if we have a bid, we must have a bid size
    // assert((bid == nullptr) || (*curr == '4'));
    if ((*curr == '4') && (bid != nullptr)) {
      SKP_STR("4\":")
      curr = utxx::fast_atoi<long, false>(curr, end, qty_value);
      bid->m_qty = QtyN(qty_value * 100);
      SKP_STR(",\"")
    }

    // if we have an ask, we must have an ask size
    // assert((ask == nullptr) || (*curr == '5'));
    if ((*curr == '5') && (ask != nullptr)) {
      SKP_STR("5\":")
      curr = utxx::fast_atoi<long, false>(curr, end, qty_value);
      ask->m_qty = QtyN(qty_value * 100);
    }

    CHECK_ONLY(bool ok =)
    EConnector_MktData::UpdateOrderBooks<
        true, // IsSnapshot
        IsMultiCast,
        false, // WithIncrUpdates
        WithOrdersLog, WithRelaxedOBs, ChangeIsPartFill, NewEncdAsChange,
        ChangeEncdAsNew, IsFullAmt, IsSparse,
        EConnector_MktData::FindOrderBookBy::Symbol, QT, QR,
        EConnector_H1WS_TDAmeritrade>(SeqNum(), m_mdMsg, false, a_ts_recv,
                                      a_ts_handl);

    CHECK_ONLY(if (utxx::unlikely(!ok)){LOG_WARN(
        2,
        "EConnector_H1WS_TDAmeritrade::ProcessLevel1: {}: OrderBook Update "
        "Failed",
        ToCStr(m_mdMsg.m_Symbol))})

    // move on to next symbol (if any)
    curr = strchr(curr, '{');
  }

  return true;
}

bool EConnector_H1WS_TDAmeritrade::ProcessLevel2(char const *a_msg_body,
                                                 int a_msg_len,
                                                 utxx::time_val a_ts_recv,
                                                 utxx::time_val a_ts_handl) {
  auto curr = a_msg_body;
  char *end = const_cast<char *>(a_msg_body + a_msg_len);

  size_t timestamp = 0;

  assert(*curr == '{');

  while (curr != nullptr) {
    SKP_STR("{\"key\":\"")
    auto end_symbol = strchr(curr, '"');
    InitFixedStr(&m_mdMsg.m_Symbol, curr, size_t(end_symbol - curr));
    curr = end_symbol;

    SKP_STR("\",\"1\":")
    curr = utxx::fast_atoi<size_t, false>(curr, end, timestamp);
    m_mdMsg.m_exchTS = utxx::msecs(timestamp);

    m_mdMsg.m_nEntrs = 0;
    SKP_STR(",\"2\":[")
    curr = ProcessLevels(curr, a_msg_len, FIX::MDEntryTypeT::Bid);

    SKP_STR(",\"3\":[")
    curr = ProcessLevels(curr, a_msg_len, FIX::MDEntryTypeT::Offer);

    CHECK_ONLY(bool ok =)
    EConnector_MktData::UpdateOrderBooks<
        true, // IsSnapshot
        IsMultiCast,
        false, // WithIncrUpdates
        WithOrdersLog, WithRelaxedOBs, ChangeIsPartFill, NewEncdAsChange,
        ChangeEncdAsNew, IsFullAmt, IsSparse,
        EConnector_MktData::FindOrderBookBy::Symbol, QT, QR,
        EConnector_H1WS_TDAmeritrade>(SeqNum(), m_mdMsg, false, a_ts_recv,
                                      a_ts_handl);

    CHECK_ONLY(if (utxx::unlikely(!ok)){LOG_WARN(
        2,
        "EConnector_H1WS_TDAmeritrade::ProcessLevel1: {}: OrderBook Update "
        "Failed",
        ToCStr(m_mdMsg.m_Symbol))})

    // move on to next symbol (if any)
    curr = strchr(curr, '{');
  }

  return true;
}

char const *EConnector_H1WS_TDAmeritrade::ProcessLevels(
    char const *a_msg_body, int a_msg_len, FIX::MDEntryTypeT a_type) {
  auto curr = a_msg_body;
  char *end = const_cast<char *>(a_msg_body + a_msg_len);

  char *after = nullptr;
  long qty_value = 0;

  assert(*curr == '{');

  while (true) {
    SKP_STR("{\"0\":")
    auto mde = &m_mdMsg.m_entries[m_mdMsg.m_nEntrs++];
    mde->m_entryType = a_type;
    mde->m_px = PriceT(strtod(curr, &after));
    curr = after;
    SKP_STR(",\"1\":")
    curr = utxx::fast_atoi<long, false>(curr, end, qty_value);
    mde->m_qty = QtyN(qty_value);

    // we skip "2" (number of market makers) and "3" (array of market makers)
    curr = strchr(curr, ']');
    SKP_STR("]}")

    if (*curr == ',') {
      // we have more levels, keep going
      ++curr;
    } else {
      // we're done with these levels
      assert(*curr == ']');
      return curr + 1;
    }
  }
  // if we get here, something went wrong:
  __builtin_unreachable();
}

} // End namespace MAQUETTE
