#include <bits/stdint-uintn.h>
#include <bitset>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utxx/compiler_hints.hpp>
#include <utxx/time_val.hpp>
#include <vector>

#include "Basis/BaseTypes.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Basis/EPollReactor.h"
#include "Basis/IOUtils.h"
#include "Basis/SecDefs.h"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_OrdMgmt.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/Strategy.hpp"
#include "QuantSupport/BT/BackTest.h"
#include "QuantSupport/BT/HistMDC_MDStore.h"
#include "QuantSupport/BT/HistMDC_VSPBin.h"
#include "QuantSupport/BT/HistOMC_InstaFill.h"
#include "QuantSupport/BT/MktData.h"
#include "QuantSupport/BT/OrdMgmt.h"

#include "TradingStrats/BLS/Common/BLS_Strategies.hpp"
#include "TradingStrats/BLS/Common/BLSEngine.hpp"
#include "TradingStrats/BLS/Common/HelixSelector.hpp"

using namespace MAQUETTE;
using namespace BLS;

namespace {

template <bool BACKTEST> class HelixSelectorStrat final : public Strategy {
private:
  //=======================================================================//
  // Data Types:                                                           //
  //=======================================================================//
  constexpr static QtyTypeT QT = QtyTypeT::QtyA; // Contracts are OK as well
  using QR = long;
  using Qty_t = Qty<QT, QR>;
  using Selector = HelixSelector<QT, QR>;
  using ParallelEngine = BLSParallelEngine<QT, QR>;
  using MDRec = typename ParallelEngine::MDRec;
  using HelixTrade = typename ParallelEngine::Trade;

  EConnector_MktData *m_mdc;
  EConnector_OrdMgmt *m_omc;
  const SecDefD &m_instr;

  FILE *m_fout;

  // Helix options
  double m_spread_tolerance;
  Qty_t m_min_order_size, m_order_size;
  int m_bar_type;
  double m_bar_param;
  int m_ticks_per_point;

  // for regiming
  std::string m_md_store_root, m_venue, m_symbol;
  bool m_use_bid_for_ask;
  double m_look_back_hrs, m_look_forward_hrs;

  bool m_first_time = true;
  utxx::time_val m_next_regime_time;

  ParallelEngine m_parallel;
  Selector m_selector;

  // active strategy
  BLSEngine<Qty_t> m_helix;
  Qty_t m_current_position = Qty_t(0l);
  HelixTrade m_trade;
  int m_num_trades = 0;
  double m_cum_profit = 0.0;

  // current best bid/ask for backtest
  PriceT m_best_bid, m_best_ask;

public:
  HelixSelectorStrat(EPollReactor *a_reactor, spdlog::logger *a_logger,
                     const boost::property_tree::ptree &a_params,
                     EConnector_MktData *a_mdc, EConnector_OrdMgmt *a_omc,
                     const SecDefD &instr)
      : Strategy("BLS-Helix-Selector", a_reactor, a_logger,
                 a_params.get<int>("Strategy.DebugLevel"), {SIGINT}),
        m_mdc(a_mdc), m_omc(a_omc), m_instr(instr),
        m_spread_tolerance(a_params.get<double>("Strategy.SpreadTolerance")),
        m_min_order_size(Qty_t(a_params.get<long>("Strategy.MinOrderSize"))),
        m_order_size(Qty_t(a_params.get<long>("Strategy.OrderSize"))),
        m_bar_type(a_params.get<int>("Strategy.BarType")),
        m_bar_param(a_params.get<double>("Strategy.BarParameter")),
        m_ticks_per_point(int(1.0 / m_instr.m_PxStep)),
        m_md_store_root(a_params.get<std::string>("BacktestData.MDStoreRoot")),
        m_venue(a_params.get<std::string>("BacktestData.Venue")),
        m_symbol(a_params.get<std::string>("BacktestData.Symbol")),
        m_use_bid_for_ask(a_params.get<bool>("BacktestData.UseBidForAsk")),
        m_look_back_hrs(a_params.get<double>("BacktestData.LookBackHrs")),
        m_look_forward_hrs(a_params.get<double>("BacktestData.LookForwardHrs")),
        m_parallel(a_params.get_child("Strategy"), m_use_bid_for_ask, m_instr),
        m_selector(a_params.get_child("Selector"), m_parallel) {
    m_mdc->Subscribe(this);
    m_mdc->SubscribeMktData(this, instr, OrderBook::UpdateEffectT::L1Qty,
                            false);
    m_omc->Subscribe(this);

    m_fout = fopen("helix_selector_trades.txt", "w");
    fprintf(m_fout,
            "%3s  %12s  %25s  %18s  %7s  %10s  %10s  %10s  %10s  %10s\n", "#",
            "Type", "Date/time", "Signal", "Price", "Amount", "Profit",
            "Run-up", "Entry eff.", "Total eff.");
    fprintf(m_fout,
            "%3s  %12s  %25s  %18s  %7s  %10s  %10s  %10s  %10s  %10s\n", "",
            "", "", "", "", "Profit", "Cum profit", "Drawdown", "Exit eff.",
            "");
  }

  ~HelixSelectorStrat() noexcept override {
    // Do not allow any exceptions to propagate out of this Dtor:
    try {
      // Stop Connectors (if not stopped yet):
      m_mdc->UnSubscribe(this);
      m_mdc->Stop();

      m_omc->UnSubscribe(this);
      m_omc->Stop();

      BLS_DestroyPool();

      if (m_fout != nullptr)
        fclose(m_fout);
    } catch (...) {
    }
  }

  void Run() {
    // Start the Connectors:
    m_mdc->Start();
    m_omc->Start();

    // Enter the Inifinite Event Loop (exit on any exceptions):
    // m_reactor->Run(true);
  }

  void OnOrderBookUpdate(OrderBook const &a_ob, bool /*a_is_error*/,
                         OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
                         utxx::time_val a_ts_exch, utxx::time_val a_ts_recv,
                         utxx::time_val a_ts_strat) override {
    if (utxx::unlikely(m_first_time)) {
      m_first_time = false;

      // round down to hour
      int secs = 3600 * (int(a_ts_recv.seconds()) / 3600);
      m_next_regime_time =
          utxx::time_val(utxx::secs(secs + int(3600.0 * m_look_forward_hrs)));
      SelectNewHelix(a_ts_recv);
    }

    // Instrument of this OrderBook:
    SecDefD const &instr = a_ob.GetInstr();

    if (utxx::unlikely(&instr != &m_instr)) {
      // This may happen if a Ccy Converter OrderBook has been updated, not
      // the Main one:
      return;
    }

    // Also, for safety, we need both Bid and Ask pxs available in order to
    // proceed further (also if OMC is not active yet):
    m_best_bid = a_ob.GetBestBidPx();
    m_best_ask = a_ob.GetBestAskPx();

    if (!m_omc->IsActive() || !IsFinite(m_best_bid) || !IsFinite(m_best_ask))
      return;

    // update each strategy and process its signals, if it has any

    m_helix.OnOrderBookUpdate(
        0, instr, a_ts_exch, m_best_bid, m_best_ask, m_spread_tolerance,
        m_order_size, m_min_order_size, m_current_position, &m_trade,
        [=](bool a_is_buy, bool a_is_aggressive, PriceT a_ref_px, Qty_t a_qty) {
          this->PlaceOrder(a_is_buy, a_ref_px, a_qty, a_is_aggressive,
                           a_ts_exch, a_ts_recv, a_ts_strat);
        });

    if (a_ts_strat >= m_next_regime_time) {
      m_next_regime_time += utxx::secs(int(3600.0 * m_look_forward_hrs));
      SelectNewHelix(a_ts_recv);
    }
  }

  void OnOurTrade(Trade const &a_tr) override {
    long qty = long(a_tr.m_qty);
    auto signed_qty =
        (a_tr.m_aggressor == FIX::SideT::Buy) ? Qty_t(qty) : -Qty_t(qty);

    auto new_qty = m_current_position + signed_qty;
    bool entry = false;
    bool exit = false;

    if (QR(new_qty) * QR(m_current_position) > 0) {
      // the sign of the current position has not changed, so we are adding
      // to the current position

      // compute average entry
      double old_q = double(QR(m_current_position));
      double add_q = double(QR(signed_qty));
      m_trade.entry =
          (m_trade.entry * old_q + double(a_tr.m_px) * add_q) / (old_q + add_q);
      m_trade.quantity += signed_qty;
    } else if (QR(new_qty) * QR(m_current_position) == 0) {
      // either new_qty or m_current_position is 0, assume that not both are 0
      if (IsZero(new_qty))
        exit = true;
      else
        entry = true;
    } else {
      // the signs of new_qty and m_current_position are different, so we have
      // an exit and an entry
      exit = true;
      entry = true;
    }

    if (exit) {
      m_trade.exit = double(a_tr.m_px);
      m_trade.exit_time = a_tr.m_exchTS;
      m_trade.high = std::max(m_trade.high, double(a_tr.m_px));
      m_trade.low = std::min(m_trade.low, double(a_tr.m_px));

      m_trade.print(++m_num_trades, &m_cum_profit, m_fout);
      printf("%4i  %10.2f\n", m_num_trades, m_cum_profit);
    }

    if (entry) {
      // this is an entry
      m_trade.entry = double(a_tr.m_px);
      m_trade.entry_time = a_tr.m_exchTS;
      m_trade.high = double(a_tr.m_px);
      m_trade.low = double(a_tr.m_px);
      m_trade.quantity = new_qty;
    }

    m_current_position = new_qty;
  }

  //=======================================================================//
  // "PlaceOrder":                                                         //
  //=======================================================================//
  AOS const* PlaceOrder(bool a_is_buy, PriceT a_ref_px, Qty_t a_qty,
                  bool a_is_aggressive, utxx::time_val a_ts_exch,
                  utxx::time_val a_ts_recv, utxx::time_val a_ts_strat) {
    if (Abs(a_qty) < m_min_order_size)
      return nullptr;

    // printf("[%s] %10s %4s %10li at %.5f (current %10li)\n",
    //        a_ts_exch.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
    //        a_is_aggressive ? "aggressive" : "passive", a_is_buy ? "buy" :
    //        "sell", long(a_qty), double(a_ref_px), long(m_current_position));

    if ((a_is_buy && (m_current_position > 0)) ||
        (!a_is_buy && (m_current_position < 0))) {
      return nullptr;
    }

    //---------------------------------------------------------------------//
    // Calculate the Limit Px (Aggr or Passive):                           //
    //---------------------------------------------------------------------//
    PriceT px = a_is_aggressive
                    ? (a_is_buy ? RoundUp(1.01 * a_ref_px, m_instr.m_PxStep)
                                : RoundDown(0.99 * a_ref_px, m_instr.m_PxStep))
                    : a_ref_px;

    if constexpr (BACKTEST) {
      // if this is a backtest, we are using the InstaFill OMC, so we need to
      // set the price to the current bid/ask since we assume a limit order
      // that gets filled immediately
      if (a_is_aggressive)
        px = a_is_buy ? m_best_ask : m_best_bid;
      else
        px = a_is_buy ? std::min(a_ref_px, m_best_ask)
                      : std::max(a_ref_px, m_best_bid);

      // printf("Final price is %.5f (bid: %.5f, ask: %.5f)\n", double(px),
      //   double(m_best_bid), double(m_best_ask));
    }

    assert(a_qty > 0 && IsFinite(px));

    //---------------------------------------------------------------------//
    // Now actually place it:                                              //
    //---------------------------------------------------------------------//
    AOS const* aos = nullptr;
    try {
      aos = m_omc->NewOrder(this, m_instr, FIX::OrderTypeT::Limit, a_is_buy, px,
                            a_is_aggressive, QtyAny(a_qty), a_ts_exch,
                            a_ts_recv, a_ts_strat);
    } catch (std::exception const &exc) {
      LOG_ERROR(2, "PlaceOrder: EXCEPTION: {}", exc.what())
      return nullptr;
    }

    assert(aos != nullptr && aos->m_id > 0 && aos->m_isBuy == a_is_buy);
    Req12 *req = aos->m_lastReq;
    assert(req != nullptr && req->m_kind == Req12::KindT::New);
    int lat = req->GetInternalLatency();

    LOG_INFO(3,
             "PlaceOrder: NewOrder: AOSID={}, ReqID={}: {} {}: {} lots, Px={}: "
             "Latency={} usec",
             aos->m_id, req->m_id, a_is_aggressive ? "Aggressive" : "Passive",
             aos->m_isBuy ? "Buy" : "Sell", QR(req->m_qty), double(px),
             (lat != 0) ? std::to_string(lat) : "unknown")

    return aos;
  }

private:
  void SelectNewHelix(utxx::time_val a_now) {
    printf("Select new helix:\n");

    auto start = a_now - utxx::secs(int(3600.0 * m_look_back_hrs));
    auto end = a_now;
    auto recs =
        GetMDStoreRecs<MDRec>(m_md_store_root, m_venue, m_symbol, start, end);

    printf("    Now: %s\n", a_now.to_string(utxx::DATE_TIME_WITH_NSEC).c_str());
    printf("  Start: %s\n", start.to_string(utxx::DATE_TIME_WITH_NSEC).c_str());
    printf("    End: %s\n", end.to_string(utxx::DATE_TIME_WITH_NSEC).c_str());
    printf("   Next: %s\n",
           m_next_regime_time.to_string(utxx::DATE_TIME_WITH_NSEC).c_str());

    printf(" Recs from %s to %s\n\n",
           recs[0].ts_recv.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
           recs[recs.size() - 1]
               .ts_recv.to_string(utxx::DATE_TIME_WITH_NSEC)
               .c_str());

    auto prefix = end.to_string(utxx::DATE_TIME_WITH_NSEC) + "_";
    auto selector_log_fname = prefix + "selector.log";
    FILE *flog = fopen(selector_log_fname.c_str(), "w");

    std::vector<HelixParams> best_params;
    std::vector<double> scores;
    std::vector<OverallTradesResults<Qty_t>> trade_results;
    std::tie(best_params, scores, trade_results) =
        m_selector.GetBestParameters(recs, prefix, flog);

    fclose(flog);

    // find the parameters with the highest score
    double max_score = scores[0];
    size_t max_idx = 0;
    for (size_t i = 1; i < scores.size(); ++i) {
      if (scores[i] > max_score) {
        max_score = scores[i];
        max_idx = i;
      }
    }

    if (max_score <= 0.0) {
      // TODO: handle this properly
      throw std::runtime_error("No profitable instance found");
    }

    auto params = best_params[max_idx];
    printf("Switching to instance with score %.2f:\n", max_score);
    PrintParamsAndResults(stdout, params, trade_results[max_idx]);

    BLS_CreatePool(1);
    BLS_InitHelix(0, m_ticks_per_point, m_bar_type, m_bar_param, params);
    m_helix.m_signal.Reset();

    // prime the helix instance by dummy trading through the lookback data
    for (auto const &rec : recs) {
      auto time = rec.ts_recv;

      m_best_bid = Round(PriceT(double(rec.rec.bid)), m_instr.m_PxStep);
      m_best_ask = m_use_bid_for_ask
                       ? m_best_bid
                       : Round(PriceT(double(rec.rec.ask)), m_instr.m_PxStep);

      if (!m_use_bid_for_ask && (m_best_bid >= m_best_ask))
        m_best_ask = m_best_bid + m_instr.m_PxStep;

      // NB: some quantities are given in 100k, so they could be 0.5
      Qty_t bid_size(0L), ask_size(0L);
      if (rec.rec.bid_size > 0.0)
        bid_size = Qty_t(long(std::max(1.0, rec.rec.bid_size)));

      if (m_use_bid_for_ask) {
        ask_size = bid_size;
      } else {
        ask_size = Qty_t(long(std::max(1.0, rec.rec.ask_size)));
      }

      if (!IsFinite(m_best_bid) || IsSpecial0(bid_size) ||
          !IsFinite(m_best_ask) || IsSpecial0(ask_size))
        continue;

      Qty_t dummy_position(0L);
      HelixTrade dummy_trade;

      m_helix.OnOrderBookUpdate(0, m_instr, time, m_best_bid, m_best_ask,
                                m_spread_tolerance, m_order_size,
                                m_min_order_size, dummy_position, &dummy_trade,
                                [&](bool a_is_buy, bool /*a_is_aggressive*/,
                                    PriceT /*a_ref_px*/, Qty_t a_qty) {
                                  // just update dummy position, we don't care
                                  // about these trades, we just need to prime
                                  // Helix's internal state up to the end of the
                                  // look back data
                                  dummy_position += a_is_buy ? a_qty : -a_qty;
                                });
    }
  }
};

} // namespace

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char *argv[]) {
  try {
    //-----------------------------------------------------------------------//
    // Global Init:                                                          //
    //-----------------------------------------------------------------------//
    IO::GlobalInit({SIGINT});

    //-----------------------------------------------------------------------//
    // Get the Config:                                                       //
    //-----------------------------------------------------------------------//
    if (argc != 2) {
      std::cerr << "PARAMETER: ConfigFile.ini" << std::endl;
      return 1;
    }
    std::string iniFile(argv[1]);

    // Get the Propert Tree:
    boost::property_tree::ptree pt;

    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    //-----------------------------------------------------------------------//
    // Create the Logger and the Reactor:                                    //
    //-----------------------------------------------------------------------//
    std::shared_ptr<spdlog::logger> loggerShP =
        IO::MkLogger("Main", pt.get<std::string>("Exec.LogFile"), false);
    spdlog::logger *logger = loggerShP.get();
    assert(logger != nullptr);

    EPollReactor reactor(logger, pt.get<int>("Main.DebugLevel"));

    //-----------------------------------------------------------------------//
    // Load the historical data:                                             //
    //-----------------------------------------------------------------------//
    SecDefS eurusd{0,   "EUR/USD", "",    "",    "MRCXXX", "Hist", "", "SPOT",
                   "",  "EUR",     "USD", 'A',   1.0,      1.0,    1,  0.00001,
                   'A', 1.0,       0,     79200, 0.0,      0,      ""};

    SecDefS usdjpy{0,   "USD/JPY", "",    "",    "MRCXXX", "Hist", "", "SPOT",
                   "",  "USD",     "JPY", 'A',   1.0,      1.0,    1,  0.001,
                   'A', 1.0,       0,     79200, 0.0,      0,      ""};

    //-----------------------------------------------------------------------//
    // Create MDC, OMC, and Strategy:                                        //
    //-----------------------------------------------------------------------//
    std::vector<SecDefS> instruments{eurusd};

    HistMDC_MDStore mdc(&reactor, nullptr, &instruments, nullptr,
                        pt.get_child("HistMDC_MDStore"), nullptr);

    auto sec_def_mgr = new SecDefsMgr(true);
    HistOMC_InstaFill<long> omc(&reactor, sec_def_mgr, &instruments, nullptr,
                                pt.get_child("HistOMC_InstaFill"));

    auto eurusd_d = sec_def_mgr->GetAllSecDefs()[0];

    HelixSelectorStrat<true> strat(&reactor, logger, pt, &mdc, &omc, eurusd_d);

    //-----------------------------------------------------------------------//
    // Run backtest:                                                         //
    //-----------------------------------------------------------------------//
    strat.Run();
  } catch (std::exception const &exc) {
    std::cerr << "\nEXCEPTION: " << exc.what() << std::endl;
    return 1;
  }
  return 0;
}
