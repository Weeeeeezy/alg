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
#include "TradingStrats/BLS/Common/Params.hpp"
#include "TradingStrats/BLS/Common/BLSEngine.hpp"
#include "TradingStrats/BLS/Common/TradeStats.hpp"

using namespace MAQUETTE;
using namespace BLS;
using namespace std;

namespace {

template <bool BACKTEST, bool EMULATE_TS>
class BLSSerial final : public Strategy {
private:
  //=======================================================================//
  // Data Types:                                                           //
  //=======================================================================//
  // Qty Type used by the MDC:
  constexpr static QtyTypeT QTM = QtyTypeT::Lots;
  using QRM = long;
  using QtyM = Qty<QTM, QRM>;

  // Qty Type used by the OMC:
  constexpr static QtyTypeT QTO = QtyTypeT::QtyA; // Contracts are OK as well
  using QRO = long;
  using QtyO = Qty<QTO, QRO>;

  using BLSEngine_t = BLSEngine<QtyO, EMULATE_TS>;
  using BLSTrade = typename BLSEngine_t::Trade;

  EConnector_MktData *m_mdc;
  EConnector_OrdMgmt *m_omc;
  const SecDefD &m_instr;

  FILE *m_trades_out, *m_equity_out, *m_bar_out, *m_sig_out;

  double m_spread_tolerance;
  QtyO m_min_order_size, m_order_size;
  bool m_exclude_sundays;
  double m_cost;
  bool m_enter_at_bar;
  std::string m_exec_mode;

  std::vector<BLSEngine_t> m_strats;

  // current position
  QtyO m_current_position = QtyO(0l);

  // current trade
  BLSTrade m_trade;
  int m_num_trades = 0;
  double m_cum_profit = 0.0;

  std::vector<BLSTrade> m_all_trades;

  // store time period
  bool m_first_time = true;
  utxx::time_val m_begin_time, m_end_time;

  utxx::time_val m_ts_exch, m_ts_recv, m_ts_strat;

public:
  BLSSerial(EPollReactor *a_reactor, spdlog::logger *a_logger,
            const boost::property_tree::ptree &a_params,
            EConnector_MktData *a_mdc, EConnector_OrdMgmt *a_omc,
            const SecDefD &instr)
      : Strategy("BLS-Serial", a_reactor, a_logger,
                 a_params.get<int>("Strategy.DebugLevel"), {SIGINT}),
        m_mdc(a_mdc), m_omc(a_omc), m_instr(instr) {
    m_mdc->Subscribe(this);
    m_mdc->SubscribeMktData(this, instr, OrderBook::UpdateEffectT::L1Qty,
                            false);
    m_omc->Subscribe(this);

    auto ps = a_params.get_child("Strategy");
    m_spread_tolerance = ps.get<double>("SpreadTolerance");
    m_min_order_size = QtyO(ps.get<long>("MinOrderSize"));
    m_order_size = QtyO(ps.get<long>("OrderSize"));
    m_exclude_sundays = ps.get<bool>("ExcludeSundays");
    m_cost = ps.get<double>("Cost", 0.0);
    m_enter_at_bar = ps.get<bool>("EnterAtBar", false);
    m_exec_mode = ps.get<std::string>("ExecMode");

    if (EMULATE_TS)
      m_enter_at_bar = true;

    // create strategies
    int bar_type = ps.get<int>("BarType");
    double bar_param = ps.get<double>("BarParameter");
    int ticks_per_point = int(round(1.0 / m_instr.m_PxStep));

    auto strats = ps.get<std::string>("Strategies");
    std::vector<std::string> strategy_names;
    boost::split(strategy_names, strats, boost::is_any_of(","));

    int num_Helixes = int(strategy_names.size());
    BLS_CreatePool(num_Helixes);

    // put this outside the for loop so we have the parameters of the last
    // strategy accessible to create the output filename

    auto place_order_func =
      [this](bool   a_is_buy, bool a_is_aggressive,
             PriceT a_ref_px, QtyO a_qty)
    {
      this->PlaceOrder(a_is_buy, a_ref_px, a_qty, a_is_aggressive);
    };

    for (uint64_t i = 0; i < uint64_t(num_Helixes); ++i) {
      auto strat_ps = a_params.get_child(strategy_names[i]);

      BLSParamSpace params(strat_ps);
      if (params.size() != 1) {
        throw std::invalid_argument("Strategy '" + strategy_names[i] +
                                    "' defines more than one instance");
      }

      BLSInit(i, ticks_per_point, bar_type, bar_param, params.at(0));

      m_strats.push_back(
          BLSEngine_t(i, m_instr, m_spread_tolerance, m_order_size,
                      m_min_order_size, m_exec_mode, place_order_func,
                      &m_current_position, &m_bar_out, &m_sig_out));
    }

    m_trades_out = fopen("trades.txt", "w");

    fprintf(m_trades_out,
            "%8s  %12s  %25s  %7s  %10s  %10s  %10s  %10s  %10s\n", "#", "Type",
            "Date/time", "Price", "Amount", "Profit", "Run-up", "Entry eff.",
            "Total eff.");
    fprintf(m_trades_out,
            "%8s  %12s  %25s  %7s  %10s  %10s  %10s  %10s  %10s\n", "", "", "",
            "", "Profit", "Cum profit", "Drawdown", "Exit eff.", "");

    m_equity_out = fopen("equity.txt", "w");
    fprintf(m_equity_out, "# %6s  %10s  %10s  %20s  %20s\n", "Number",
            "Cum Profit", "Profit", "Entry time (UNIX)", "Exit time (UNIX)");

    m_bar_out = fopen("bars.txt", "w");
    m_sig_out = fopen("signals.txt", "w");
  }

  ~BLSSerial() noexcept override {
    printf("Skipped %.2f%% of ticks (%i of %i) due to spread tolerance\n\n",
           100.0 * double(m_strats[0].m_num_ticks_skipped) /
               double(m_strats[0].m_num_ticks),
           m_strats[0].m_num_ticks_skipped, m_strats[0].m_num_ticks);

    // print results
    OverallTradesResults<QtyO> res(m_all_trades, m_cost, m_exclude_sundays,
                                   m_begin_time, m_end_time);
    res.Print();

    // Do not allow any exceptions to propagate out of this Dtor:
    try {
      // Stop Connectors (if not stopped yet):
      m_mdc->UnSubscribe(this);
      m_mdc->Stop();

      m_omc->UnSubscribe(this);
      m_omc->Stop();

      BLS_DestroyPool();

      if (m_trades_out != nullptr)
        fclose(m_trades_out);

      if (m_equity_out != nullptr)
        fclose(m_equity_out);

      if (m_bar_out != nullptr)
        fclose(m_bar_out);

      if (m_sig_out != nullptr)
        fclose(m_sig_out);
    } catch (...) {
    }
  }

  void OnOrderBookUpdate(OrderBook const &a_ob, bool /*a_is_error*/,
                         OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
                         utxx::time_val a_ts_exch, utxx::time_val a_ts_recv,
                         utxx::time_val a_ts_strat) override {
    m_ts_exch = a_ts_exch;
    m_ts_recv = a_ts_recv;
    m_ts_strat = a_ts_strat;

    const auto time = m_ts_recv;

    // Instrument of this OrderBook:
    SecDefD const &instr = a_ob.GetInstr();

    if (utxx::unlikely(&instr != &m_instr)) {
      // This may happen if a Ccy Converter OrderBook has been updated, not
      // the Main one:
      return;
    }

    if (m_exclude_sundays) {
      // January 1, 1970 was a Thursday, so add 4 to make Sunday 0 mod 7
      auto day = 4 + (time.sec() / 86400);

      if (day % 7 == 0)
        return;
    }

    // Also, for safety, we need both Bid and Ask pxs available in order to
    // proceed further (also if OMC is not active yet):
    m_best_bid = a_ob.GetBestBidPx();
    m_best_ask = a_ob.GetBestAskPx();

    if (!m_omc->IsActive() || !IsFinite(m_best_bid) || !IsFinite(m_best_ask))
      return;

    if (utxx::unlikely(m_first_time)) {
      m_first_time = false;
      m_begin_time = time;
    }
    m_end_time = time;

    // update each strategy and process its signals, if it has any
    for (size_t strat_idx = 0; strat_idx < m_strats.size(); ++strat_idx) {
      m_strats[strat_idx].OnQuote(time, m_best_bid, m_best_ask);
    }
  }

  void OnTradeUpdate(Trade const &a_tr) override {
    m_ts_recv = a_tr.m_recvTS;
    m_ts_exch = a_tr.m_exchTS;
    m_ts_strat = utxx::now_utc();

    const auto time = m_ts_recv;

    if (utxx::unlikely(a_tr.m_instr != &m_instr)) {
      return;
    }

    if (m_exclude_sundays) {
      // January 1, 1970 was a Thursday, so add 4 to make Sunday 0 mod 7
      auto day = 4 + (time.sec() / 86400);

      if (day % 7 == 0)
        return;
    }

    if (!m_omc->IsActive() || !IsFinite(a_tr.m_px))
      return;

    if (utxx::unlikely(m_first_time)) {
      m_first_time = false;
      m_begin_time = time;
    }
    m_end_time = time;

    // update each strategy and process its signals, if it has any
    for (size_t strat_idx = 0; strat_idx < m_strats.size(); ++strat_idx) {
      m_strats[strat_idx].OnTrade(time, a_tr.m_px, &m_trade);
    }
  }

  void OnOurTrade(Trade const &a_tr) override {
    long qty = long(a_tr.GetQty<QTO,QRO>());
    auto signed_qty =
        (a_tr.m_aggressor == FIX::SideT::Buy) ? QtyO(qty) : -QtyO(qty);

    auto new_qty = m_current_position + signed_qty;
    bool entry = false;
    bool exit = false;

    if (QRO(new_qty) * QRO(m_current_position) > 0) {
      // the sign of the current position has not changed, so we are adding
      // to the current position

      // compute average entry
      double old_q = double(QRO(m_current_position));
      double add_q = double(QRO(signed_qty));
      m_trade.entry =
          (m_trade.entry * old_q + double(a_tr.m_px) * add_q) / (old_q + add_q);
      m_trade.quantity += signed_qty;
    } else if (QRO(new_qty) * QRO(m_current_position) == 0) {
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
      m_trade.exit_time = a_tr.m_recvTS;
      m_trade.high = std::max(m_trade.high, double(a_tr.m_px));
      m_trade.low = std::min(m_trade.low, double(a_tr.m_px));

      m_trade.print(m_cost, ++m_num_trades, &m_cum_profit, m_trades_out);
      fprintf(m_equity_out, "%8i  %10.2f  %10.2f  %20.6f  %20.6f\n",
              m_num_trades, m_cum_profit, m_trade.profit(m_cost),
              m_trade.entry_time.seconds(), m_trade.exit_time.seconds());
      m_all_trades.push_back(m_trade);
    }

    if (entry) {
      // this is an entry
      m_trade.entry = double(a_tr.m_px);
      m_trade.entry_time = a_tr.m_recvTS;
      m_trade.high = double(a_tr.m_px);
      m_trade.low = double(a_tr.m_px);
      m_trade.quantity = new_qty;
    }

    m_current_position = new_qty;
  }

  //=======================================================================//
  // "PlaceOrder":                                                         //
  //=======================================================================//
  AOS const* PlaceOrder(bool a_is_buy, PriceT a_ref_px, QtyO a_qty,
                  bool a_is_aggressive) {
    if (Abs(a_qty) < m_min_order_size)
      return nullptr;

    // printf("[%s] %10s %4s %10li at %.5f (current %10li)\n",
    //        a_ts_recv.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
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

    if (m_enter_at_bar) {
      Bar prev_bar, bar;
      BLS_GetLastClosedBar(0, &prev_bar);
      BLS_GetCurrentBar(0, &bar);

      px = a_is_buy ? PriceT(std::max(double(a_ref_px), bar.open))
                    : PriceT(std::min(double(a_ref_px), bar.open));
    }

    assert(a_qty > 0 && IsFinite(px));

    //---------------------------------------------------------------------//
    // Now actually place it:                                              //
    //---------------------------------------------------------------------//
    AOS const* aos = nullptr;
    try {
      aos = m_omc->NewOrder(this, m_instr, FIX::OrderTypeT::Limit, a_is_buy, px,
                            a_is_aggressive, QtyAny(a_qty), m_ts_exch,
                            m_ts_recv, m_ts_strat);
    } catch (exception const &exc) {
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
             aos->m_isBuy ? "Buy" : "Sell", QRO(req->GetQty<QTO,QRO>()),
             double(px), (lat != 0) ? to_string(lat) : "unknown")

    return aos;
  }

private:
  // current best bid/ask for backtest
  PriceT m_best_bid, m_best_ask;
};

} // namespace

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char *argv[]) {
  try {
    IO::GlobalInit({SIGINT});

    if (argc != 2) {
      cerr << "PARAMETER: ConfigFile.ini" << endl;
      return 1;
    }
    string iniFile(argv[1]);

    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    shared_ptr<spdlog::logger> loggerShP =
        IO::MkLogger("Main", pt.get<string>("Exec.LogFile"), false);
    spdlog::logger *logger = loggerShP.get();
    assert(logger != nullptr);

    EPollReactor reactor(logger, pt.get<int>("Main.DebugLevel"));

    std::shared_ptr<EConnector_MktData> mdc;
    auto data = pt.get<std::string>("Main.Data");

    auto data_node = pt.get_child(data);
    double price_step = data_node.get<double>("PriceStep");
    SecDefS instr{0,   "AAA/BBB", "",  "",         "MRCXXX", "Hist",
                  "",  "SPOT",    "",  "AAA",      "BBB",    'A',
                  1.0, 1.0,       1,   price_step, 'A',      1.0,
                  0,   79200,     0.0, 0,          ""};

    vector<SecDefS> instruments{instr};

    if (data == "HistMDC_VSPBin") {
      mdc = std::make_shared<HistMDC_VSPBin>(&reactor, nullptr, &instruments,
                                             nullptr, data_node, nullptr);
    } else if (data == "HistMDC_MDStore") {
      mdc = std::make_shared<HistMDC_MDStore>(&reactor, nullptr, &instruments,
                                              nullptr, data_node, nullptr);
    } else {
      throw std::invalid_argument(
          "Main.Data must be either 'HistMDC_VSPBin' or 'HistMDC_MDStore'");
    }

    HistOMC_InstaFill<long> omc(&reactor, nullptr, &instruments, nullptr,
                                pt.get_child("HistOMC_InstaFill"));
    // NB: SecDefsMgr is automatically created by the "EConnector" ctor:
    SecDefsMgr const* sec_def_mgr = omc.GetSecDefsMgr();

    auto instr_d = sec_def_mgr->GetAllSecDefs()[0];

    std::unique_ptr<Strategy> strat;

    if (pt.get<bool>("Strategy.EmulateTradeStation", false))
      strat = std::make_unique<BLSSerial<true, true>>(&reactor, logger, pt,
                                                      mdc.get(), &omc, instr_d);
    else
      strat = std::make_unique<BLSSerial<true, false>>(
          &reactor, logger, pt, mdc.get(), &omc, instr_d);

    mdc->Start();
    omc.Start();
  } catch (exception const &exc) {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    return 1;
  }
  return 0;
}
