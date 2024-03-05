#pragma once

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Basis/PxsQtys.h"
#include "Basis/SecDefs.h"
#include "TradingStrats/BLS/Common/BLS_Strategies.hpp"

namespace MAQUETTE {
namespace BLS {

template <typename Qty> struct BLSTrade {
  double entry, exit;
  Qty quantity;
  utxx::time_val entry_time, exit_time;
  double high, low;

  // cost: positive means cost in points, negative means cost in percent
  double profit(double cost) const {
    auto qty = static_cast<double>(quantity);
    // 2x since cost is per side
    double this_cost =
        fabs(qty) * (cost >= 0.0 ? 2.0 * cost : (-cost) * (exit + entry));
    double pnl = qty * (exit - entry);
    return pnl - this_cost;
  }

  double duration_in_sec() const {
    return exit_time.seconds() - entry_time.seconds();
  }

  void print(double cost, int num, double *cum_profit, FILE *fout) const {
    bool lng = quantity > 0; // true if long
    double qty = double(quantity);
    double profit = this->profit(cost);
    double max_adverse = qty * ((lng ? low : high) - entry);
    double max_favorable = qty * ((lng ? high : low) - entry);
    double entry_eff = (lng ? (high - entry) : (entry - low)) / (high - low);
    double exit_eff = (lng ? (exit - low) : (high - exit)) / (high - low);
    double efficiency = (lng ? 1.0 : -1.0) * (exit - entry) / (high - low);

    *cum_profit += profit;

    std::string qty_str = std::to_string(abs(int(qty)));
    auto l = qty_str.length();
    if (l > 3)
      qty_str.insert(l - 3, ",");
    if (l > 6)
      qty_str.insert(l - 6, ",");

    auto entry_str = entry_time.to_string(utxx::DATE_TIME_WITH_USEC, '-');
    auto exit_str = exit_time.to_string(utxx::DATE_TIME_WITH_USEC, '-');
    entry_str[10] = ' ';
    exit_str[10] = ' ';
    entry_str = entry_str.substr(0, 25);
    exit_str = exit_str.substr(0, 25);

    fprintf(fout,
            "%8i  %12s  %25s  %7.5f  %10s  %10.2f  %10.2f  %9.2f%%  %10s\n",
            num, lng ? "Buy" : "Sell Short", entry_str.c_str(), entry,
            qty_str.c_str(), profit, max_favorable, entry_eff * 100.0, "");
    fprintf(fout,
            "%8s  %12s  %25s  %7.5f  %10.2f  %10.2f  %10.2f  %9.2f%%  "
            "%9.2f%%\n",
            "", lng ? "Sell" : "Buy to Cover", exit_str.c_str(), exit, profit,
            round(*cum_profit), max_adverse, exit_eff * 100.0,
            efficiency * 100.0);
  }
};

template <typename Qty, bool EMULATE_TS> class BLSEngine {
public:
  using Trade = BLSTrade<Qty>;
  using PlaceOrderFunc = std::function<void(bool a_is_buy, bool a_is_aggressive,
                                            PriceT a_ref_px, Qty a_qty)>;

  int m_num_ticks = 0;
  int m_num_ticks_skipped = 0;

private:
  const uint64_t m_helix_idx;
  const SecDefD &m_instr;
  const double m_spread_tolerance;
  const Qty m_order_size;
  const Qty m_min_order_size;
  const PlaceOrderFunc &m_place_order_func;
  Qty *const m_current_position;
  FILE **const m_bar_out;
  FILE **const m_sig_out;

  enum class ExecMode { Bid, Last, Passive };
  ExecMode m_exec_mode;

  BLSSignal m_signal;
  PriceT m_bid, m_ask, m_last;

public:
  BLSEngine(uint64_t a_helix_id, const SecDefD &a_instr,
            double a_spread_tolerance, Qty a_order_size, Qty a_min_order_size,
            const std::string &a_exec_mode,
            const PlaceOrderFunc &a_place_order_func, Qty *a_current_position,
            FILE **a_bar_out, FILE **a_sig_out)
      : m_helix_idx(a_helix_id), m_instr(a_instr),
        m_spread_tolerance(a_spread_tolerance), m_order_size(a_order_size),
        m_min_order_size(a_min_order_size),
        m_place_order_func(a_place_order_func),
        m_current_position(a_current_position), m_bar_out(a_bar_out),
        m_sig_out(a_sig_out) {
    m_signal.Reset();

    if (a_exec_mode == "Bid") {
      m_exec_mode = ExecMode::Bid;
    } else if (a_exec_mode == "Last") {
      m_exec_mode = ExecMode::Last;
    } else if (a_exec_mode == "Passive") {
      m_exec_mode = ExecMode::Passive;
    } else {
      throw std::invalid_argument("Unknown exec mode " + a_exec_mode);
    }
  }

  __attribute__((always_inline)) void ProcessSignals() {
    // process price signals
    process_signal(true, m_signal.stop, m_signal.long_pos,
                   m_signal.entry_price);

    if (*m_current_position > 0) {
      // check long exit orders
      process_signal(false, true, true, m_signal.stop_loss_long);
      process_signal(false, false, true, m_signal.profit_target_long);
    } else if (*m_current_position < 0) {
      // check short exit orders
      process_signal(false, true, false, m_signal.stop_loss_short);
      process_signal(false, false, false, m_signal.profit_target_short);
    }
  }

  __attribute__((always_inline)) void OnQuote(utxx::time_val /*a_time*/,
                                              PriceT a_bid, PriceT a_ask) {
    if (IsFinite(a_bid))
      m_bid = a_bid;

    if (IsFinite(a_ask))
      m_ask = a_ask;

    // printf("[%s] Quote: %.2f  %.2f\n",
    //        a_time.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
    //        double(m_bid), double(m_ask));

    ProcessSignals();
  }

  template <bool FLUSH = false>
  __attribute__((always_inline)) void OnTrade(utxx::time_val a_time,
                                              PriceT a_last, Trade *a_trade) {
    if (!IsFinite(m_bid) || !IsFinite(m_ask) || !IsFinite(a_last))
      return;

    ++m_num_ticks;
    if (utxx::unlikely(abs(double(m_bid) / double(m_ask) - 1.0) >=
                       m_spread_tolerance)) {
      ++m_num_ticks_skipped;
      return;
    }

    m_last = a_last;

    // printf("[%s] Trade: %.2f  %.2f  (last %.2f)\n",
    //        a_time.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
    //        double(m_bid), double(m_ask), double(a_last));

    // auto str = a_time.to_string(utxx::DATE_TIME_WITH_USEC);
    // printf("%s %.5f  %.5f  %.7f\n", str.c_str(), double(a_bid),
    // double(a_ask),
    //        fabs(double(a_bid) / double(a_ask) - 1.0));

    a_trade->high = std::max(a_trade->high, double(a_last));
    a_trade->low = std::min(a_trade->low, double(a_last));

    // get current market position: 1 long, 0 flat, -1 short
    int current_position_flag =
        (*m_current_position > 0) ? 1 : ((*m_current_position < 0) ? -1 : 0);

    bool new_signal = BLS_Update(m_helix_idx, double(a_last), 0, a_time,
                                 current_position_flag, &m_signal);

    if (new_signal) {
      if (*m_bar_out != nullptr) {
        Bar bar;
        BLS_GetLastClosedBar(0, &bar);
        uint y;
        int m, d, hr, min, sec;
        auto time = bar.close_time;
        // time -= utxx::secs((time.sec() > 1604102400 ? 5 : 4) * 3600);
        std::tie(y, m, d, hr, min, sec) = time.to_ymdhms();
        fprintf(*m_bar_out,
                "%02i/%02i/%04u,%02i:%02i:%02i,%.2f,%.2f,%.2f,%.2f\n", m, d, y,
                hr, min, sec, bar.open, bar.high, bar.low, bar.close);
        if (FLUSH)
          fflush(*m_bar_out);
      }

      if (*m_sig_out != nullptr) {
        bool s = m_signal.stop;
        bool l = m_signal.long_pos;

        auto esl = (s && l) ? m_signal.entry_price : 0.0;
        auto ess = (s && !l) ? m_signal.entry_price : 0.0;
        auto ell = (!s && l) ? m_signal.entry_price : 0.0;
        auto els = (!s && !l) ? m_signal.entry_price : 0.0;
        auto xsl = m_signal.stop_loss_long;
        auto xss = m_signal.stop_loss_short;
        auto xll = m_signal.profit_target_long;
        auto xls = m_signal.profit_target_short;

        fprintf(*m_sig_out,
                "%s,%i,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%+i,%i\n",
                a_time.to_string(utxx::TIME).c_str(), current_position_flag,
                esl, ess, ell, els, xsl, xss, xll, xls,
                m_signal.market_entry ? (l ? 1 : -1) : 0,
                m_signal.market_exit ? 1 : 0);
        if (FLUSH)
          fflush(*m_sig_out);
      }
    }

    // if current position is flat, market exit makes no sense
    if (*m_current_position == 0)
      m_signal.market_exit = false;

    // process market signals
    if (m_signal.market_entry || m_signal.market_exit) {
      bool proceed = true;
      if (m_signal.market_entry && m_signal.market_exit) {
        // NB: because of the statement on lines 136 and 137, a_current_position
        // != 0 if we are here if we are in the opposing position of the desired
        // one, this is valid
        if (m_signal.long_pos == (*m_current_position > 0)) {
          // we are already in the desired position, nothing to do
          proceed = false;
        } else {
          // we are in the opposite position than the one we want to be in, we
          // will proceed
          proceed = true;
        }
      }

      if (proceed) {
        // if this is a market entry, the long_pos determines whether we buy or
        // sell, if this is an exit, the current pos determines if we buy or
        // sell
        bool buy = m_signal.market_entry ? m_signal.long_pos
                                         : (*m_current_position < 0);

        auto new_pos = m_signal.market_entry
                           ? (buy ? m_order_size : -m_order_size)
                           : Qty::Zero();

        // this is the delta in position we need to get the the new desired
        // position
        auto delta_qty = new_pos - *m_current_position;

        if (Abs(delta_qty) >= m_min_order_size) {
          auto price = buy ? m_ask : m_bid;

          if (utxx::unlikely((delta_qty > 0) != buy)) {
            bool s = m_signal.stop;
            bool l = m_signal.long_pos;

            auto esl = (s && l) ? m_signal.entry_price : 0.0;
            auto ess = (s && !l) ? m_signal.entry_price : 0.0;
            auto ell = (!s && l) ? m_signal.entry_price : 0.0;
            auto els = (!s && !l) ? m_signal.entry_price : 0.0;
            auto xsl = m_signal.stop_loss_long;
            auto xss = m_signal.stop_loss_short;
            auto xll = m_signal.profit_target_long;
            auto xls = m_signal.profit_target_short;
            printf(
                "%s  %.5f  %.5f  %.5f  %.5f  %.5f  %.5f  %.5f  %.5f  %i  %i\n",
                a_time.to_string(utxx::TIME).c_str(), esl, ess, ell, els, xsl,
                xss, xll, xls, m_signal.market_entry ? 1 : 0,
                m_signal.market_exit ? 1 : 0);

            throw utxx::runtime_error("Mismatch between delta_qty and buy");
          }

          m_place_order_func(buy, true, price, Abs(delta_qty));

          m_signal.market_entry = false;
          m_signal.market_exit = false;
        }
      }
    }

    ProcessSignals();
  }

private:
  __attribute__((always_inline)) void process_signal(bool a_enter, bool a_stop,
                                                     bool a_long_pos,
                                                     double &a_signal_price) {
    if (a_signal_price == 0.0)
      return;

    PriceT signal(a_signal_price);
    signal.Round(m_instr.m_PxStep);

    // we buy if we enter a long position or exit a short position
    bool buy = (a_enter == a_long_pos);

    bool aggressive;
    PriceT price;
    if (!a_stop && (m_exec_mode == ExecMode::Passive)) {
      aggressive = false;
      price = signal;
    } else {
      // check if the order triggers unless this is an entry order and we always
      // enter at market
      auto ref = (m_exec_mode == ExecMode::Bid) ? m_bid : m_last;
      if (buy) {
        // If we buy (enter long or exit short), we need ask >= signal for a
        // stop order and signal >= ask for a limit order
        if (!((a_stop ? ref : signal) >= (a_stop ? signal : ref)))
          return;
      } else {
        // If we sell (enter short or exit long), we need bid <= signal for a
        // stop order and signal <= bid for a limit order
        if (!((a_stop ? ref : signal) <= (a_stop ? signal : ref)))
          return;
      }

      aggressive = a_stop;
      price = ref;
    }

    // at this point we have a triggered order

    // this is the new desired position we want to have
    auto new_pos = a_enter ? (buy ? m_order_size : -m_order_size) : Qty::Zero();

    // this is the delta in position we need to get the the new desired
    // position
    auto delta_qty = new_pos - *m_current_position;

    // printf("Triggered entry order to %s %i at %.3f\n", buy ? "buy" :
    // "sell",
    //        long(delta_qty), double(signal));

    if (Abs(delta_qty) >= m_min_order_size) {
      // NB: Assume we're submitting a limit order (also for stop, since we
      // synthesize the stop order)
      // For a stop, the bid/ask is the worse price and we're willing to take
      // it For a limit, the signal is the worse price and we're willing to
      // take it For market entry, just use bid/ask, it will be flagged as
      // agressive
      // auto price = a_stop ? (buy ? m_ask : m_bid) : signal;

      // if we're emulating TradeStation, we fill at the signal price
      if constexpr (EMULATE_TS)
        price = signal;

      // printf("[%6li] %5s %5s %5s %6li @ %.5f (bid: %.5f, ask: %.5f, signal:
      // "
      //        "%.5f)\n",
      //        long(current_position), enter ? "enter" : "exit",
      //        long_pos ? "long" : "short", stop ? "stop" : "limit",
      //        long(delta_qty), double(price), double(a_bid), double(a_ask),
      //        double(signal));

      if (utxx::unlikely((delta_qty > 0) != buy))
        throw utxx::runtime_error("Mismatch between delta_qty and buy");

      m_place_order_func(buy, aggressive, price, Abs(delta_qty));

      // set this signal to 0 to disable it, because we've fired off the order
      a_signal_price = 0.0;
    }
  }
};

} // namespace BLS
} // namespace MAQUETTE
