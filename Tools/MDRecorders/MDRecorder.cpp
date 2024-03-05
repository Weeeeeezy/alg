// vim:ts=2:et
//===========================================================================//
//                    "Tools/MDRecorders/MDRecorder.cpp":                    //
//                        General Market Data Recorder                       //
//===========================================================================//
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <signal.h>
#include <string>
#include <unordered_map>

#include <boost/property_tree/ini_parser.hpp>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Basis/BaseTypes.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Basis/IOSendEmail.hpp"
#include "Basis/IOUtils.hpp"
#include "Basis/Macros.h"
#include "Connectors/EConnector_MktData.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/Strategy.hpp"
#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

#include "Connectors/FIX/EConnector_FIX.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/BitMEX/EConnector_WS_BitMEX_MDC.h"
#include "Connectors/H2WS/Huobi/EConnector_WS_Huobi_MDC.h"
#include "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_MDC.h"

using namespace MAQUETTE;
using namespace MAQUETTE::QuantSupport;

namespace {

static bool exited_due_to_signal = false;

//=========================================================================//
// "MDRecorder" Class:                                                     //
//=========================================================================//
template <bool FRACTIONAL_QTY> class MDRecorder final : public Strategy
{
private:
  //=======================================================================//
  // Data Flds and Ctor:                                                   //
  //=======================================================================//
  using QR = std::conditional_t<FRACTIONAL_QTY, double, long>;

  EConnector_MktData *m_mdc;

  // so that we can quickly get the MDStoreWriter given a sec def ID, we use
  // a vector of unique_ptrs
  std::vector<std::unique_ptr<MDStoreWriter<MDRecL1>>> m_md_stores_L1;
  std::vector<std::unique_ptr<MDStoreWriter<MDAggression>>> m_md_stores_trades;

  std::unordered_map<SecID, size_t> m_secID_to_idx;

  utxx::time_val m_last_quote_recv_time, m_last_trade_recv_time;
  // timer FD for periodic check that we're still receiving data
  int m_check_timer_FD = -1;

  bool m_use_alt = false;

public:
  MDRecorder(std::string const &a_name, EPollReactor *a_reactor,
             spdlog::logger *a_logger, int a_debug_level,
             boost::property_tree::ptree const & /*a_pt*/,
             EConnector_MktData *a_mdc, const std::string &a_md_store_root,
             const std::string &a_exchange)
      : Strategy(a_name, a_reactor, a_logger, a_debug_level, {SIGINT}),
        m_mdc(a_mdc) {
    // init the MD Stores
    auto const &sec_defs = m_mdc->GetSecDefsMgr()->GetAllSecDefs();

    m_md_stores_L1.resize(sec_defs.size());
    m_md_stores_trades.resize(sec_defs.size());

    for (size_t i = 0; i < sec_defs.size(); ++i) {
      auto const &s = sec_defs[i];

      // For Huobi/TT we need to use AltSymbol because Symbol can be degenerate
      bool is_huobi = (strncmp(a_exchange.c_str(), "Huobi", 5) == 0);
      bool is_tt = (strncmp(a_exchange.c_str(), "TT", 2) == 0);

      m_use_alt = (is_huobi || is_tt);

      auto symbol =
          std::string(m_use_alt ? s.m_AltSymbol.data() : s.m_Symbol.data());

      m_md_stores_L1[i] = std::make_unique<MDStoreWriter<MDRecL1>>(
          a_md_store_root, a_exchange, symbol, "ob_L1");
      m_md_stores_trades[i] = std::make_unique<MDStoreWriter<MDAggression>>(
          a_md_store_root, a_exchange, symbol, "trades");
      m_secID_to_idx[s.m_SecID] = i;
    }

    // set check timer
    // TimerHandler: Calls "CheckDataFlowing" periodically:
    IO::FDInfo::TimerHandler timerH([this](int DEBUG_ONLY(a_fd)) -> void
    {
      assert(a_fd == m_check_timer_FD);
      this->CheckDataFlowing();
    });

    // ErrorHandler:
    IO::FDInfo::ErrHandler errH([this]
    (int a_fd, int a_err_code, uint32_t a_events, char const *a_msg) -> void
    {
      this->CheckTimerErrHandler(a_fd, a_err_code, a_events, a_msg);
    });

    // Period: 2 min = 120 sec = 120,000 msec:
    constexpr uint32_t CheckTimerPeriodMSec = 120 * 1000;

    // Create the TimerFD and add it to the Reactor:
    auto timerName = a_exchange + "_CheckTimer";
    m_check_timer_FD = m_reactor->AddTimer(timerName.c_str(),
                                           60000, // ms, Initial offset
                                           CheckTimerPeriodMSec, // ms, Period
                                           timerH, errH);
  }

  void CheckDataFlowing() {
    // if we haven't received any quotes in 2 minutes or trades in 10 minutes,
    // something is wrong
    if (m_last_quote_recv_time < (utxx::now_utc() - utxx::secs(120))) {
      throw utxx::runtime_error(
          "Last quote data received over 2 minutes ago at {}",
          m_last_quote_recv_time.to_string(utxx::DATE_TIME_WITH_NSEC));
    }

    if (m_last_trade_recv_time < (utxx::now_utc() - utxx::secs(600))) {
      throw utxx::runtime_error(
          "Last trade data received over 10 minutes ago at {}",
          m_last_trade_recv_time.to_string(utxx::DATE_TIME_WITH_NSEC));
    }
  }

  void CheckTimerErrHandler(int a_fd, int a_err_code, uint32_t a_events,
                            char const *a_msg) {
    // There should be NO errors associated with this Timer at all. If one occ-
    // urs, we better shut down completely:
    assert(a_fd == m_check_timer_FD);
    LOG_ERROR(1,
              "MDRecorder::CheckTimerErrHandler: TimerFD={}, ErrCode={}, "
              "Events={}, Msg={}: STOPPING",
              a_fd,  a_err_code, m_reactor->EPollEventsToStr(a_events),
              (a_msg != nullptr) ? a_msg : "")

    throw utxx::runtime_error("MDRecorder::CheckTimerErrHandler");
  }

  //=======================================================================//
  // Common Call-Back used by both MktData and OrdMgmt Connectors:         //
  //=======================================================================//
  //-----------------------------------------------------------------------//
  // "OnTradingEvent":                                                     //
  //-----------------------------------------------------------------------//
  void OnTradingEvent(EConnector const &a_conn, bool a_on, SecID /*a_sec_id*/,
                      utxx::time_val /*a_ts_exch*/,
                      utxx::time_val /*a_ts_recv*/) override {
    if (&a_conn != m_mdc)
      throw utxx::runtime_error(
          "MDRecorder::OnTradingEvent: Unexpected connector");

    LOG_INFO(1, "OnTradingEvent: Connector={}: Trading {}", a_conn.GetName(),
             (a_on ? "Started" : "Stopped"))

    if (!a_on) {
      m_reactor->ExitImmediately("MDRecorder::OnTradingEvent: Trading stopped");
    } else {
      // subscribe to quotes for each sec def
      auto const &sec_defs = m_mdc->GetSecDefsMgr()->GetAllSecDefs();
      LOG_INFO(1, "Subscribing to {} instruments", sec_defs.size())

      // FIXME: The following is Cumberland-specific:
      EConnector_FIX_Cumberland *cumberland =
          dynamic_cast<EConnector_FIX_Cumberland *>(m_mdc);
      for (auto const &s : sec_defs) {
        LOG_INFO(1, "Subscribing to {}",
                 (m_use_alt ? s.m_AltSymbol : s.m_Symbol).data())
        if (cumberland != nullptr) {
          if (strncmp(s.m_QuoteCcy.data(), "USD", 3) == 0)
            cumberland->SubscribeStreamingQuote(
                this, s, false, Qty<QtyTypeT::QtyB, double>(200000.0));
          else
            cumberland->SubscribeStreamingQuote(
                this, s, false, Qty<QtyTypeT::QtyB, double>(10.0));
        } else {
          m_mdc->SubscribeMktData(this, s, OrderBook::UpdateEffectT::L1Qty,
                                  false);
        }
      }
    }
  }

private:
  //=======================================================================//
  // Mkt Data Call-Backs:                                                  //
  //=======================================================================//
  //-----------------------------------------------------------------------//
  // "OnOrderBookUpdate":                                                  //
  //-----------------------------------------------------------------------//
  void OnOrderBookUpdate(OrderBook const &a_ob,
                         bool /*a_is_error*/, // ignored
                         OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
                         utxx::time_val a_ts_exch, utxx::time_val a_ts_recv,
                         utxx::time_val a_ts_strat) override {
    long lat_usec = (a_ts_strat - a_ts_recv).microseconds();

    auto const &instr = a_ob.GetInstr();
    auto idx = m_secID_to_idx.at(instr.m_SecID);

    MDStoreBase<MDRecL1>::MDStoreRecord rec;
    rec.ts_exch = a_ts_exch;
    rec.ts_recv = a_ts_recv;
    rec.rec.bid = double(a_ob.GetBestBidPx());
    rec.rec.ask = double(a_ob.GetBestAskPx());

    // FIXME: Can we always assume QtyA here? Does Cumberland use QtyB???
    rec.rec.bid_size = double(QR(a_ob.GetBestBidQty<QtyTypeT::QtyA,QR>()));
    rec.rec.ask_size = double(QR(a_ob.GetBestAskQty<QtyTypeT::QtyA,QR>()));

    m_md_stores_L1[idx]->Write(rec);

    auto date = a_ts_exch.to_string(utxx::DATE_TIME_WITH_NSEC);

    printf("[%s] %20s (latency %6li): %20.8f @ %16.8f .. %20.8f @ %16.8f\n",
           date.c_str(),
           (m_use_alt ? instr.m_AltSymbol : instr.m_Symbol).data(), lat_usec,
           rec.rec.bid_size, rec.rec.bid, rec.rec.ask_size, rec.rec.ask);

    m_last_quote_recv_time = a_ts_recv;
  }

  //-----------------------------------------------------------------------//
  // "OnTradeUpdate":                                                      //
  //-----------------------------------------------------------------------//
  void OnTradeUpdate(Trade const &a_trade) override {
    long lat_usec = utxx::time_val::now_diff_usec(a_trade.m_recvTS);

    MDStoreBase<MDAggression>::MDStoreRecord rec;
    rec.ts_exch = a_trade.m_exchTS;
    rec.ts_recv = a_trade.m_recvTS;
    rec.rec.m_avgPx = double(a_trade.m_px);
    rec.rec.m_totQty  = double(QR(a_trade.GetQty<QtyTypeT::QtyA,QR>()));
    rec.rec.m_bidAggr = (a_trade.m_aggressor == FIX::SideT::Buy);

    auto idx = m_secID_to_idx.at(a_trade.m_instr->m_SecID);
    m_md_stores_trades[idx]->Write(rec);

    auto date = a_trade.m_exchTS.to_string(utxx::DATE_TIME_WITH_NSEC);

    printf(
        "[%s] %20s (latency %6li): %20.8f @ %16.8f %s\n", date.c_str(),
        (m_use_alt ? a_trade.m_instr->m_AltSymbol : a_trade.m_instr->m_Symbol)
            .data(),
        lat_usec, rec.rec.m_totQty, rec.rec.m_avgPx,
        rec.rec.m_bidAggr ? "BOUGHT" : "SOLD");

    m_last_trade_recv_time = a_trade.m_recvTS;
  }

public:
  //-----------------------------------------------------------------------//
  // "OnSignal":                                                           //
  //-----------------------------------------------------------------------//
  void OnSignal(signalfd_siginfo const & /*a_si*/) override {
    exited_due_to_signal = true;
    m_reactor->ExitImmediately(
        "MDRecorder::OnSignal: Shutting down gracefully...");
  }
};
} // namespace

#define MK_MDC(name)                                                           \
  new name(&reactor, secDefsMgr, nullptr, nullptr,                             \
           GetParamsSubTree(pt, #name), nullptr);                              \
  fractional_qty = std::is_same_v<double, name::QR>;

//===========================================================================//
// "main": //
//===========================================================================//
int main(int argc, char *argv[]) {
  //-------------------------------------------------------------------------//
  // Params: //
  //-------------------------------------------------------------------------//
  IO::GlobalInit({SIGINT});

  if (argc != 2) {
    printf("Usage: %s ConfigFile.ini\n", argv[0]);
    return 1;
  }

  auto now = utxx::now_utc().to_string(utxx::DATE_TIME_WITH_NSEC);

  EConnector_MktData *mdc = nullptr;
  int ret = -1;

  try {
    // make Logs directory
    if (std::filesystem::exists("Logs")) {
      std::filesystem::rename("Logs", "Logs_" + now);
      std::filesystem::create_directories("Logs");
    }

    std::string iniFile(argv[1]);

    // Get the Propert Tree:
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    //-----------------------------------------------------------------------//
    // Construct the Reactor (but no Risk Manager):                          //
    //-----------------------------------------------------------------------//
    std::string logOutput = pt.get<std::string>("Main.LogFile");

    auto loggerShP = IO::MkLogger("MDRecorder_" + now, logOutput);
    bool isProd = pt.get<bool>("Main.IsProd");
    int debugLevel = pt.get<int>("Main.DebugLevel");

    auto exchange = pt.get<std::string>("Main.Exchange");
    auto md_store_root = pt.get<std::string>("Main.MDStoreRoot");

    // Use default params in the Reactor:
    EPollReactor reactor(loggerShP.get(), debugLevel, false, false);

    SecDefsMgr *secDefsMgr =
        SecDefsMgr::GetPersistInstance(isProd, exchange + "_MDRec_" + now);

    bool fractional_qty;

    if (exchange == "Binance") {
      mdc = MK_MDC(EConnector_H2WS_Binance_MDC_FutT)
    } else if (exchange == "Binance_d") {
      mdc = MK_MDC(EConnector_H2WS_Binance_MDC_FutC)
    } else if (exchange == "BinanceSpt") {
      mdc = MK_MDC(EConnector_H2WS_Binance_MDC_Spt)
    } else if (exchange == "BitMEX") {
      mdc = MK_MDC(EConnector_WS_BitMEX_MDC)
    } else if (exchange == "Cumberland") {
      mdc = MK_MDC(EConnector_FIX_Cumberland)
    } else if (exchange == "Currenex") {
      mdc = MK_MDC(EConnector_FIX_Currenex)
    } else if (exchange == "KrakenSpot") {
      mdc = MK_MDC(EConnector_WS_KrakenSpot_MDC)
    } else if (exchange == "LMAX") {
      mdc = MK_MDC(EConnector_FIX_LMAX)
    } else if (exchange == "HuobiSpt") {
      mdc = MK_MDC(EConnector_WS_Huobi_MDC_Spt)
    } else if (exchange == "HuobiFut") {
      mdc = MK_MDC(EConnector_WS_Huobi_MDC_Fut)
    } else if (exchange == "HuobiCSwp") {
      mdc = MK_MDC(EConnector_WS_Huobi_MDC_CSwp)
    } else if (exchange == "HuobiUSwp") {
      mdc = MK_MDC(EConnector_WS_Huobi_MDC_USwp)
    } else if (exchange == "TT") {
      mdc = MK_MDC(EConnector_FIX_TT)
    } else {
      throw std::invalid_argument("Unknown exchange '" + exchange + "'");
    }

    //-----------------------------------------------------------------------//
    // Instantiate the Test Strategy:                                        //
    //-----------------------------------------------------------------------//
    std::unique_ptr<Strategy> tst;
    if (fractional_qty)
      tst = std::make_unique<MDRecorder<true>>(
          exchange + "_MDRec_" + now, &reactor, loggerShP.get(), debugLevel, pt,
          mdc, md_store_root, exchange);
    else
      tst = std::make_unique<MDRecorder<false>>(
          exchange + "_MDRec_" + now, &reactor, loggerShP.get(), debugLevel, pt,
          mdc, md_store_root, exchange);

    //-----------------------------------------------------------------------//
    // Run the Connectors and Reactor:                                       //
    //-----------------------------------------------------------------------//
    mdc->Subscribe(tst.get());
    mdc->Start();

    // Run it! Exit on any unhandled exceptions:
    bool busyWait = pt.get<bool>("Main.BusyWaitInEPoll");
    reactor.Run(true, busyWait);

    // Termination:
    mdc->Stop();

    if (exited_due_to_signal) {
      // it's all good
      ret = 0;
    }
  } catch (std::exception &ex) {
    ret = 2;
    printf("ERROR: Exception thrown: %s\n", ex.what());
  } catch (...) {
    ret = 3;
    printf("ERROR: Non-standard exception thrown\n");
  }

  if (mdc != nullptr)
    delete mdc;

  return ret;
}
