#pragma once

#include <type_traits>
#include <utxx/error.hpp>

#include "Basis/BaseTypes.hpp"
#include "Connectors/EConnector_MktData.h"
#include "Connectors/EConnector_OrdMgmt.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/Strategy.hpp"

#include "TradingStrats/BLS/Common/BLSEngine.hpp"
#include "TradingStrats/BLS/Common/Params.hpp"
#include "TradingStrats/BLS/Common/TradeStats.hpp"

namespace MAQUETTE {
namespace BLS {

// If SEP_ORD_OPN_CLOSE is true, separate orders will be made to open and close
// positions. For example, if the current position is -3 and we want to move
// into +5, with SEP_ORD_OPN_CLOSE == FALSE (default), we would just place ONE
// buy order of size 8, but with SEP_ORD_OPN_CLOSE == TRUE, we'd first place a
// buy order of size 3 and then a second buy order of size 5. This is required
// for some exchanges (e.g. TDAmeritrade that splits up orders into suchseparate
// orders anyway, but then we only know about one of them and can't match the
// fill report for the other)
template <typename MDC, typename OMC, bool USE_FRAC_QTY, bool SEP_ORD_OPN_CLOSE,
          bool BACKTEST, bool EMULATE_TS>
class BLSStrategy final : public Strategy {
private:
  constexpr static QtyTypeT QT = QtyTypeT::QtyA;
  using QR = typename std::conditional<USE_FRAC_QTY, double, long>::type;
  using Qty_t = Qty<QT, QR>;

  using BLSEngine_t = BLSEngine<Qty_t, EMULATE_TS>;
  using BLSTrade = typename BLSEngine_t::Trade;

  MDC *m_mdc;
  OMC *m_omc;
  const SecDefD &m_instr;
  const uint64_t m_bls_idx;

  const double m_spread_tolerance;
  const Qty_t m_min_order_size, m_max_order_size, m_order_size;
  const bool m_exclude_sundays;
  const double m_cost_in_points;
  const bool m_enter_at_bar;
  const std::string m_exec_mode;

  FILE *m_trades_out, *m_equity_out, *m_bar_out, *m_sig_out;

  bool m_exiting = false;

  typename BLSEngine_t::PlaceOrderFunc m_place_order_func;
  BLSEngine_t m_strat;
  Qty_t m_current_position;
  AOS const* m_active_aos = nullptr;

  // This pointer is only used if SEP_ORD_OPN_CLOSE is TRUE. In that case,
  // m_active_aos holds the AOS for the currently active position and if we open
  // a new position, the AOS goes in here. Once the previous active position
  // closes, this will be set to nullptr and the AOS will be moved to
  // m_active_aos
  AOS const* m_next_aos = nullptr;

  // current trade
  BLSTrade m_trade;
  int m_num_trades = 0;
  double m_cum_profit = 0.0;

  std::vector<BLSTrade> m_all_trades;

  // store time period
  bool m_first_time = true;
  utxx::time_val m_begin_time, m_end_time;

  // current best bid/ask for backtest
  PriceT m_best_bid, m_best_ask, m_last;

  utxx::time_val m_ts_exch, m_ts_recv, m_ts_strat;

  const SecDefD &GetSecDef(const std::string &a_name, EConnector_MktData *a_mdc,
                           EConnector_OrdMgmt *a_omc) {
    const SecDefD &sec_def_mdc =
        a_mdc->GetSecDefsMgr()->FindSecDef(a_name.c_str());
    const SecDefD &sec_def_omc =
        a_omc->GetSecDefsMgr()->FindSecDef(a_name.c_str());

    if (&sec_def_mdc != &sec_def_omc)
      throw utxx::runtime_error("Got different SecDefs from MDC and OMC");

    return sec_def_mdc;
  }

public:
  BLSStrategy(EPollReactor *a_reactor, spdlog::logger *a_logger,
              const boost::property_tree::ptree &a_params, MDC *a_mdc,
              OMC *a_omc, uint64_t a_bls_idx)
      : Strategy("BLS-" + a_params.get<std::string>("Name"), a_reactor,
                 a_logger, a_params.get<int>("DebugLevel"), {SIGINT}),
        m_mdc(a_mdc), m_omc(a_omc),
        m_instr(
            GetSecDef(a_params.get<std::string>("Instrument"), a_mdc, a_omc)),
        m_bls_idx(a_bls_idx),
        m_spread_tolerance(a_params.get<double>("SpreadTolerance")),
        m_min_order_size(Qty_t(a_params.get<QR>("MinOrderSize"))),
        m_max_order_size(Qty_t(a_params.get<QR>("MaxOrderSize"))),
        m_order_size(Qty_t(a_params.get<QR>("OrderSize"))),
        m_exclude_sundays(a_params.get<bool>("ExcludeSundays", false)),
        m_cost_in_points(a_params.get<double>("CostInPriceStep", 0.0) *
                         m_instr.m_PxStep),
        m_enter_at_bar(a_params.get<bool>("EnterAtBar", false) || EMULATE_TS),
        m_exec_mode(a_params.get<std::string>("ExecMode")),
        m_place_order_func([this](bool   a_is_buy, bool a_is_aggressive,
                                  PriceT a_ref_px, Qty_t a_qty) {
          this->PlaceOrder(a_is_buy, a_is_aggressive, a_ref_px, a_qty);
        }),
        m_strat(m_bls_idx, m_instr, m_spread_tolerance, m_order_size,
                m_min_order_size, m_exec_mode, m_place_order_func,
                &m_current_position, &m_bar_out, &m_sig_out) {
    m_mdc->Subscribe(this);
    if (static_cast<void*>(m_mdc) != static_cast<void*>(m_omc))
      m_omc->Subscribe(this);

    // create strategies
    int bar_type = a_params.get<int>("BarType");
    double bar_param = a_params.get<double>("BarParameter");
    int ticks_per_point = int(round(1.0 / m_instr.m_PxStep));

    BLSParamSpace params(a_params);
    if (params.size() != 1)
      throw std::invalid_argument("Strategy '" + m_name +
                                  "' defines more than one instance");

    BLSInit(m_bls_idx, ticks_per_point, bar_type, bar_param, params.at(0));

    m_trades_out = fopen((m_name + "_trades.txt").c_str(), "a");

    fprintf(m_trades_out,
            "%8s  %12s  %25s  %7s  %10s  %10s  %10s  %10s  %10s\n", "#", "Type",
            "Date/time", "Price", "Amount", "Profit", "Run-up", "Entry eff.",
            "Total eff.");
    fprintf(m_trades_out,
            "%8s  %12s  %25s  %7s  %10s  %10s  %10s  %10s  %10s\n", "", "", "",
            "", "Profit", "Cum profit", "Drawdown", "Exit eff.", "");

    m_equity_out = fopen((m_name + "_equity.txt").c_str(), "a");
    fprintf(m_equity_out, "# %6s  %10s  %10s  %20s  %20s\n", "Number",
            "Cum Profit", "Profit", "Entry time (UNIX)", "Exit time (UNIX)");

    m_bar_out = fopen((m_name + "_bars.txt").c_str(), "a");
    m_sig_out = fopen((m_name + "_signals.txt").c_str(), "a");

    LOG_INFO(1, "BLSStrategy {} created", m_name)
  }

  ~BLSStrategy() noexcept override {
    printf("Skipped %.2f%% of ticks (%i of %i) due to spread tolerance\n\n",
           100.0 * double(m_strat.m_num_ticks_skipped) /
               double(m_strat.m_num_ticks),
           m_strat.m_num_ticks_skipped, m_strat.m_num_ticks);

    // print results
    OverallTradesResults<Qty_t> res(m_all_trades, m_cost_in_points,
                                    m_exclude_sundays, m_begin_time,
                                    m_end_time);
    res.Print();

    // Do not allow any exceptions to propagate out of this Dtor:
    try {
      // Stop Connectors (if not stopped yet):
      m_mdc->UnSubscribe(this);
      m_omc->UnSubscribe(this);

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

  void OnTradingEvent(EConnector const &a_conn, bool a_on,
                      SecID UNUSED_PARAM(a_sec_id),
                      utxx::time_val UNUSED_PARAM(a_ts_exch),
                      utxx::time_val UNUSED_PARAM(a_ts_recv)) override {
    LOG_INFO(1, "{}: BLSStrategy::OnTradingEvent: Connector={}: Trading {}",
             m_name, a_conn.GetName(), (a_on ? "Started" : "Stopped"))

    if (!a_on && !m_exiting) {
      // NB: Throw the "ExitRun" exception only once, otherwise we can kill
      // ourselves ungracefully:
      m_exiting = true;
      m_reactor->ExitImmediately("BLSStrategy::OnTradingEvent");
    }

    if (a_on && m_mdc->IsActive() && m_omc->IsActive()) {
      double init_pos = m_omc->GetPosition(&m_instr);
      printf("Init pos: %.2f\n", init_pos);
      if (isnan(init_pos))
        throw utxx::runtime_error(m_name,
                                  ": BLSStrategy got NaN initial position");
      m_current_position = Qty_t(QR(init_pos));

      LOG_INFO(1,
               "BLSStrategy::OnTradingEvent: Strategy {} subscribing to {} "
               "with initial position {}",
               m_name, m_instr.m_FullName.data(), QR(m_current_position))

      m_mdc->SubscribeMktData(this, m_instr, OrderBook::UpdateEffectT::L1Px,
                              false);
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

    if (utxx::unlikely(&instr != &m_instr))
      return;

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

    if (m_bls_idx == 0) {
      long lat_usec = (a_ts_strat - a_ts_recv).microseconds();
      auto date = a_ts_exch.to_string(utxx::DATE_TIME_WITH_NSEC);

      printf("[%s] %20s (latency %6li): %20.8f @ %16.8f .. %20.8f @ %16.8f\n",
             date.c_str(), instr.m_AltSymbol.data(), lat_usec,
             double(a_ob.GetBestBidQty<QtyTypeT::QtyA,QR>()),
             double(m_best_bid),
             double(a_ob.GetBestAskQty<QtyTypeT::QtyA,QR>()),
             double(m_best_ask));
    }
    m_strat.OnQuote(time, m_best_bid, m_best_ask);
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

    m_last = a_tr.m_px;

    if (!m_omc->IsActive() || !IsFinite(a_tr.m_px))
      return;

    if (utxx::unlikely(m_first_time)) {
      m_first_time = false;
      m_begin_time = time;
    }
    m_end_time = time;

    if (m_bls_idx == 0) {
      auto date = a_tr.m_exchTS.to_string(utxx::DATE_TIME_WITH_NSEC);
      long lat_usec = utxx::time_val::now_diff_usec(a_tr.m_recvTS);

      printf("[%s] %20s (latency %6li): %20.8f @ %16.8f %s\n", date.c_str(),
             a_tr.m_instr->m_AltSymbol.data(), lat_usec,
             double(a_tr.GetQty<QT,QR>()),     double(a_tr.m_px),
             a_tr.m_aggressor == FIX::SideT::Buy ? "BOUGHT" : "SOLD");
    }

    m_strat.template OnTrade<true>(time, a_tr.m_px, &m_trade);
  }

  void OnOurTrade(Trade const &a_tr) override {
    auto req = a_tr.m_ourReq;
    auto aos = req->m_aos;

    LOG_INFO(2, "TRADED: Strategy {}, OrderID={}, ReqID={}", m_name, aos->m_id,
             req->m_id)
    std::cerr << m_name << ": TRADED: OrderID=" << aos->m_id
              << ", ReqID=" << req->m_id << std::endl;

    // is our order fully filled now?
    if (m_active_aos->IsFilled()) {
      // set the active AOS to next AOS (if there is no next AOS it's NULL,
      // which is not a problem)
      m_active_aos = m_next_aos;
      m_next_aos = nullptr;
    }

    QR qty = QR(a_tr.GetQty<QT,QR>());
    auto signed_qty = aos->m_isBuy ? Qty_t(qty) : -Qty_t(qty);

    auto new_qty = m_current_position + signed_qty;
    printf("%s: OnOurTrade: prev position: %f, change: %f, new position: %f\n",
           m_name.c_str(),         double(QR(m_current_position)),
           double(QR(signed_qty)), double(QR(new_qty)));
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
      m_trade.exit_time = a_tr.m_recvTS;
      m_trade.high = std::max(m_trade.high, double(a_tr.m_px));
      m_trade.low = std::min(m_trade.low, double(a_tr.m_px));

      m_trade.print(m_cost_in_points, ++m_num_trades, &m_cum_profit,
                    m_trades_out);
      fprintf(m_equity_out, "%8i  %10.2f  %10.2f  %20.6f  %20.6f\n",
              m_num_trades, m_cum_profit, m_trade.profit(m_cost_in_points),
              m_trade.entry_time.seconds(), m_trade.exit_time.seconds());
      m_all_trades.push_back(m_trade);
      fflush(m_trades_out);
      fflush(m_equity_out);
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
    printf("%s: OnOurTrade: m_current_position: %f\n", m_name.c_str(),
           double(QR(m_current_position)));
  }

  //-----------------------------------------------------------------------//
  // "OnConfirm":                                                          //
  //-----------------------------------------------------------------------//
  void OnConfirm(Req12 const &a_req) override {
    AOS const *aos = a_req.m_aos;
    assert((m_active_aos == nullptr) || (aos == m_active_aos) ||
           (aos == m_next_aos));

    LOG_INFO(2, "Confirmed: Strategy {}, OrderID={}, ReqID={}", m_name,
             aos->m_id, a_req.m_id)
    std::cerr << m_name << ": Confirmed: OrderID=" << aos->m_id
              << ", ReqID=" << a_req.m_id << std::endl;
  }

  //-----------------------------------------------------------------------//
  // "OnCancel":                                                           //
  //-----------------------------------------------------------------------//
  void OnCancel(AOS const &a_aos, utxx::time_val UNUSED_PARAM(a_ts_exch),
                utxx::time_val UNUSED_PARAM(a_ts_recv)) override {
    // assert((&a_aos == m_active_aos) || (&a_aos == m_next_aos));
    assert(a_aos.m_isInactive);

    LOG_INFO(2, "Cancelled: Strategy {}, OrderID={}", m_name, a_aos.m_id)
    std::cerr << m_name << ": Cancelled: OrderID=" << a_aos.m_id << std::endl;

    if (&a_aos == m_active_aos)
      m_active_aos = nullptr;

    if (&a_aos == m_next_aos)
      m_next_aos = nullptr;
  }

  //-----------------------------------------------------------------------//
  // "OnOrderError":                                                       //
  //-----------------------------------------------------------------------//
  void OnOrderError(Req12 const &a_req, int a_err_code, char const *a_err_text,
                    bool UNUSED_PARAM(a_prob_filled),
                    utxx::time_val UNUSED_PARAM(a_ts_exch),
                    utxx::time_val UNUSED_PARAM(a_ts_recv)) override {
    AOS const *aos = a_req.m_aos;
    assert(aos != nullptr);

    LOG_ERROR(
        1, "Order Error: Strategy {}, OrderID={}, ReqID={}: ErrCode={}: {}: {}",
        m_name, aos->m_id, a_req.m_id, a_err_code, a_err_text,
        ((aos->m_isInactive) ? ": ORDER FAILED" : ""))
    std::cerr << m_name << ": ERROR: OrderID=" << aos->m_id
              << ", ReqID=" << a_req.m_id << ": ErrCode=" << a_err_code << ": "
              << a_err_text << ((aos->m_isInactive) ? ": ORDER FAILED" : "")
              << std::endl;
  }

  void OnSignal(signalfd_siginfo const &UNUSED_PARAM(a_si)) override {
    m_reactor->ExitImmediately(
        "BLSStrategy::OnSignal: Shutting down gracefully...");
  }

  //=======================================================================//
  // "PlaceOrder":                                                         //
  //=======================================================================//
  void PlaceOrder(bool a_is_buy, bool a_is_aggressive, PriceT a_ref_px,
                  Qty_t a_qty) {
    printf("%s: [%s] %10s %4s %10f at %.5f (current %10f)\n", m_name.c_str(),
           m_ts_exch.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
           a_is_aggressive ? "aggressive" : "passive",
           a_is_buy ? "buy" : "sell", double(QR(a_qty)), double(a_ref_px),
           double(QR(m_current_position)));

    if (a_qty < m_min_order_size)
      return;

    if ((a_is_buy && (m_current_position > 0)) ||
        (!a_is_buy && (m_current_position < 0))) {
      return;
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

      px = a_is_buy ? PriceT(std::min(prev_bar.close, bar.open))
                    : PriceT(std::max(prev_bar.close, bar.open));
    }

    assert(a_qty > 0 && IsFinite(px));

    if (a_qty > m_max_order_size) {
      printf("%s: WARNING: capped ordder at %f\n", m_name.c_str(),
             double(QR(m_max_order_size)));
      a_qty = m_max_order_size;
    }

    //---------------------------------------------------------------------//
    // Now actually place it:                                              //
    //---------------------------------------------------------------------//
    try {
      // if (m_active_aos != nullptr) {
      //   // we have an active order, check if we can modify it
      //   if ((m_active_aos->m_instr == &m_instr) &&
      //       (m_active_aos->m_orderType == FIX::OrderTypeT::Limit) &&
      //       (m_active_aos->m_isBuy == a_is_buy) &&
      //       (m_active_aos->m_lastReq->m_isAggr == a_is_aggressive)) {
      //     // replace existing order
      //     printf("ACTION: ModifyOrder\n");
      //     auto res = m_omc->ModifyOrder(m_active_aos, px, a_is_aggressive,
      //                                   QtyAny(a_qty), m_ts_exch,
      //                                   m_ts_recv, m_ts_strat);
      //     if (!res)
      //       throw utxx::runtime_error("ModifyOrder failed");
      //   } else {
      //     // cancel existing order
      //     printf("ACTION: CancelOrder\n");
      //     auto res = m_omc->CancelOrder(m_active_aos);
      //     if (!res)
      //       throw utxx::runtime_error("CancelOrder failed");
      //     m_active_aos = nullptr;
      //   }
      // }

      if (m_active_aos != nullptr) {
        // cancel existing order
        printf("%s: ACTION: CancelOrder\n", m_name.c_str());
        auto res = m_omc->CancelOrder(m_active_aos);
        if (!res)
          throw utxx::runtime_error("CancelOrder failed");
        m_active_aos = nullptr;
      }

      if (m_next_aos != nullptr) {
        // cancel existing order
        printf("%s: ACTION: CancelOrder (Next)\n", m_name.c_str());
        auto res = m_omc->CancelOrder(m_next_aos);
        if (!res)
          throw utxx::runtime_error("CancelOrder failed (Next)");
        m_next_aos = nullptr;
      }

      // either we didn't have an active order, or we cancelled it because
      // it couldn't be modified
      printf("%s: ACTION: NewOrder (%s %f at %.2f (bid %.2f, ask %.2f, last "
             "%.2f)\n",
             m_name.c_str(), a_is_buy ? "buy" : "sell", double(QR(a_qty)),
             double(px), double(m_best_bid), double(m_best_ask),
             double(m_last));
      auto new_pos = m_current_position + (a_is_buy ? a_qty : -a_qty);
      if (SEP_ORD_OPN_CLOSE && ((QR(m_current_position) * QR(new_pos)) < 0)) {
        // we need to split up this order into two to close the current
        // position and open the new one
        m_active_aos = m_omc->NewOrder(
            this, m_instr, FIX::OrderTypeT::Limit, a_is_buy, px,
            a_is_aggressive, QtyAny(Abs(m_current_position)), m_ts_exch,
            m_ts_recv, m_ts_strat, false, FIX::TimeInForceT::Day);
        m_next_aos = m_omc->NewOrder(this, m_instr, FIX::OrderTypeT::Limit,
                                     a_is_buy, px, a_is_aggressive,
                                     QtyAny(Abs(new_pos)), m_ts_exch, m_ts_recv,
                                     m_ts_strat, false, FIX::TimeInForceT::Day);
      } else {
        m_active_aos = m_omc->NewOrder(
            this, m_instr, FIX::OrderTypeT::Limit, a_is_buy, px,
            a_is_aggressive, QtyAny(a_qty), m_ts_exch, m_ts_recv, m_ts_strat,
            false, FIX::TimeInForceT::Day);
      }
    } catch (std::exception const &exc) {
      LOG_ERROR(1, "PlaceOrder: Strategy {}, EXCEPTION: {}", m_name, exc.what())
      return;
    }

    {
      auto aos = m_active_aos;
      assert(aos != nullptr && aos->m_id > 0 && aos->m_isBuy == a_is_buy);
      Req12 *req = aos->m_lastReq;
      assert(req != nullptr && req->m_kind == Req12::KindT::New);
      int lat = req->GetInternalLatency();

      LOG_INFO(
          1,
          "PlaceOrder: Strategy {}, AOSID={}, ReqID={}: {} {}: {} lots, Px={}: "
          "Latency={} usec",
          m_name, aos->m_id, req->m_id,
          a_is_aggressive ? "Aggressive" : "Passive",
          aos->m_isBuy ? "Buy" : "Sell", QR(req->GetQty<QT,QR>()), double(px),
          (lat != 0) ? std::to_string(lat) : "unknown")
    }

    if (m_next_aos != nullptr) {
      auto aos = m_next_aos;
      assert(aos != nullptr && aos->m_id > 0 && aos->m_isBuy == a_is_buy);
      Req12 *req = aos->m_lastReq;
      assert(req != nullptr && req->m_kind == Req12::KindT::New);
      int lat = req->GetInternalLatency();

      LOG_INFO(
          1,
          "PlaceOrder: Strategy {}, AOSID={}, ReqID={}: {} {}: {} lots, Px={}: "
          "Latency={} usec",
          m_name, aos->m_id, req->m_id,
          a_is_aggressive ? "Aggressive" : "Passive",
          aos->m_isBuy ? "Buy" : "Sell", QR(req->GetQty<QT,QR>()), double(px),
          (lat != 0) ? std::to_string(lat) : "unknown")
    }
  }
};

} // namespace BLS
} // namespace MAQUETTE
