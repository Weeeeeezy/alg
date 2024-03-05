#pragma once

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <iostream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <variant>

#include <H5Cpp.h>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Basis/SecDefs.h"
#include "QuantSupport/MDStoreUtil.hpp"

#include "TradingStrats/BLS/Common/BLSEngine.hpp"
#include "TradingStrats/BLS/Common/Params.hpp"
#include <TradingStrats/BLS/Common/BLS_Strategies.hpp>
#include "TradingStrats/BLS/Common/TradeStats.hpp"

namespace MAQUETTE {

using namespace QuantSupport;

namespace BLS {

// Evaluate BLS strategy instances in parallel
template <QtyTypeT QT, typename QR> class BLSParallelEngine {
public:
  using Qty_t = Qty<QT, QR>;
  using Trade = BLSTrade<Qty_t>;
  using L1Rec = typename MDStoreReader<MDRecL1>::MDStoreRecord;
  using TradeRec = typename MDStoreReader<MDAggression>::MDStoreRecord;
  using RecsT = std::pair<std::vector<L1Rec>, std::vector<TradeRec>>;

  BLSParallelEngine(const boost::property_tree::ptree &a_params,
                    bool use_bid_for_ask, const SecDefD &a_instr,
                    bool a_verbose = false)
      : m_spread_tolerance(a_params.get<double>("SpreadTolerance")),
        m_min_order_size(Qty_t(a_params.get<long>("MinOrderSize"))),
        m_order_size(Qty_t(a_params.get<long>("OrderSize"))),
        m_exclude_sundays(a_params.get<bool>("ExcludeSundays")),
        m_cost(a_params.get<double>("Cost", 0.0)),
        m_emulate_ts(a_params.get<bool>("EmulateTradeStation", false)),
        m_enter_at_bar(a_params.get<bool>("EnterAtBar", false) || m_emulate_ts),
        m_exec_mode(a_params.get<std::string>("ExecMode")),
        m_skip_gaps_over_min(a_params.get<double>("SkipGapsOverMin", 0.0)),
        m_bar_type(a_params.get<int>("BarType")),
        m_bar_parameter(a_params.get<double>("BarParameter")),
        m_ticks_per_point(int(round(1.0 / a_instr.m_PxStep))),
        m_use_bid_for_ask(use_bid_for_ask), m_instr(a_instr),
        m_verbose(a_verbose) {}

  double Cost() const { return m_cost; }
  bool ExcludeSundays() const { return m_exclude_sundays; }

  std::vector<std::vector<Trade>> Run(const BLSParamSpace &params,
                                      const RecsT &recs) const {
    printf("Initializing %lu instances...\n", params.size());
    std::vector<std::vector<Trade>> res(params.size());
    BLS_CreatePool(int(params.size()));
    auto clean_recs =
        MakeJointRec(recs, m_exclude_sundays, m_use_bid_for_ask, m_instr);

    m_start_time = std::chrono::high_resolution_clock::now();
    m_num_instances = params.size();
    size_t current_idx = 0;

#pragma omp parallel shared(current_idx)
    {
      while (true) {
        size_t this_idx;
#pragma omp atomic capture
        {
          this_idx = current_idx;
          ++current_idx;
        }
        if (this_idx >= m_num_instances)
          break;

        if (m_emulate_ts)
          res[this_idx] = RunInstance<true>(this_idx, params.at(this_idx),
                                            clean_recs, false, "");
        else
          res[this_idx] = RunInstance<false>(this_idx, params.at(this_idx),
                                             clean_recs, false, "");
      }
    }

    BLS_DestroyPool();
    return res;
  }

  void RunAndAnalyze(const BLSParamSpace &params, const RecsT &recs,
                     bool write_trade_files = false,
                     bool write_equity_files = false, bool write_hdf5 = false,
                     bool write_bars_signals = false,
                     const std::string &file_prefix = "") const {
    printf("Initializing %lu instances...\n", params.size());
    BLS_CreatePool(int(params.size()));
    auto clean_recs =
        MakeJointRec(recs, m_exclude_sundays, m_use_bid_for_ask, m_instr);

    m_start_time = std::chrono::high_resolution_clock::now();
    m_num_instances = params.size();
    size_t current_idx = 0;

    // HDF5 file with all trades. It contains 3 datasets:
    // - num_trades: number of trades for each instance
    // - offset: offset in the data dataset of the first trade (in number of
    // records)
    // - data: actual data with all trades in a dense compact form
    std::unique_ptr<H5::H5File> h5file;
    std::unique_ptr<H5::DataSet> dataset;

    std::vector<size_t> num_trades;
    std::vector<size_t> offset;
    std::vector<size_t> ids;

    constexpr size_t rec_size = 5;
    size_t curr_offset = 0;
    hsize_t total_dims[2] = {0, rec_size};
    hsize_t h5_offset[2] = {0, 0};

    if (write_hdf5) {
      h5file = std::make_unique<H5::H5File>(file_prefix + "all_trades.h5",
                                            H5F_ACC_TRUNC);

      auto attr = h5file->createAttribute(
          "num_instances", H5::PredType::NATIVE_UINT64, H5::DataSpace());
      attr.write(H5::PredType::NATIVE_UINT64, &m_num_instances);

      num_trades.reserve(m_num_instances);
      offset.reserve(m_num_instances);
      ids.reserve(m_num_instances);

      // number of records per chunk, total chunk size: 24 * 1024 * 5 * 8 = 960
      // kB
      constexpr size_t chunk_size = 24 * 1024;
      hsize_t dims[2] = {chunk_size, rec_size};
      hsize_t max_dims[2] = {H5S_UNLIMITED, rec_size};
      hsize_t chunk_dims[2] = {chunk_size, rec_size};

      H5::DataSpace dat_space(2, dims, max_dims);

      H5::DSetCreatPropList props;
      props.setChunk(2, chunk_dims);

      dataset = std::make_unique<H5::DataSet>(h5file->createDataSet(
          "data", H5::PredType::NATIVE_DOUBLE, dat_space, props));
    }

    auto fsum_name = file_prefix + "parallel_summary.csv";
    FILE *fsum = fopen(fsum_name.c_str(), "w");
    std::vector<std::string> headers{"Id"};
    auto param_titles = MakeParamTitleCSV(params.at(0));
    auto res_headers = OverallTradesResults<Qty_t>::CSV_headers;
    headers.insert(headers.end(), param_titles.begin(), param_titles.end());
    headers.insert(headers.end(), res_headers.begin(), res_headers.end());

    auto head = "\"" + boost::algorithm::join(headers, "\",\"") + "\"";
    fprintf(fsum, "%s\n", head.c_str());

#pragma omp parallel shared(current_idx)
    {
      while (true) {
        size_t this_idx;
#pragma omp atomic capture
        {
          this_idx = current_idx;
          ++current_idx;
        }
        if (this_idx >= m_num_instances)
          break;

        std::vector<Trade> trades;
        if (m_emulate_ts)
          trades = RunInstance<true>(this_idx, params.at(this_idx), clean_recs,
                                     write_bars_signals, file_prefix);
        else
          trades = RunInstance<false>(this_idx, params.at(this_idx), clean_recs,
                                      write_bars_signals, file_prefix);

        if (write_hdf5) {
          size_t nt = trades.size();

          std::vector<double> dat(trades.size() * rec_size);
          for (size_t i = 0; i < trades.size(); ++i) {
            dat[i * rec_size + 0] = trades[i].entry;
            dat[i * rec_size + 1] = trades[i].exit;
            dat[i * rec_size + 2] = double(QR(trades[i].quantity));
            dat[i * rec_size + 3] = trades[i].entry_time.seconds();
            dat[i * rec_size + 4] = trades[i].exit_time.seconds();
          }

          // HDF5 is serial, so do this inside critical section
#pragma omp critical(write_h5)
          {
            num_trades.push_back(nt);
            offset.push_back(curr_offset);
            ids.push_back(this_idx);

            if (nt > 0) {
              curr_offset += nt;

              h5_offset[0] = total_dims[0];
              total_dims[0] += nt;
              hsize_t this_dims[2] = {nt, rec_size};

              dataset->extend(total_dims);
              auto file_space = dataset->getSpace();
              file_space.selectHyperslab(H5S_SELECT_SET, this_dims, h5_offset);

              H5::DataSpace mem_space(2, this_dims);
              dataset->write(dat.data(), H5::PredType::NATIVE_DOUBLE, mem_space,
                             file_space);
            }
          }
        }

        if (write_trade_files || write_equity_files) {
          char buf[128];
          sprintf(buf, "%s%08lu", file_prefix.c_str(), this_idx);
          auto trades_file = std::string(buf) + ".trades";
          auto equity_file = std::string(buf) + ".equity";

          FILE *ft = nullptr;
          FILE *fe = nullptr;
          if (write_trade_files) {
            ft = fopen(trades_file.c_str(), "w");
            fprintf(ft, "%8s  %12s  %25s  %7s  %10s  %10s  %10s  %10s  %10s\n",
                    "#", "Type", "Date/time", "Price", "Amount", "Profit",
                    "Run-up", "Entry eff.", "Total eff.");
            fprintf(ft, "%8s  %12s  %25s  %7s  %10s  %10s  %10s  %10s  %10s\n",
                    "", "", "", "", "Profit", "Cum profit", "Drawdown",
                    "Exit eff.", "");
          }

          if (write_equity_files) {
            fe = fopen(equity_file.c_str(), "w");
            fprintf(fe, "# %6s  %10s  %10s  %20s  %20s\n", "Number",
                    "Cum Profit", "Profit", "Entry time (UNIX)",
                    "Exit time (UNIX)");
          }

          double cum_profit = 0.0;
          for (size_t t = 0; t < trades.size(); ++t) {
            if (write_trade_files)
              trades[t].print(m_cost, int(t + 1), &cum_profit, ft);
            else
              cum_profit += trades[t].profit(m_cost);

            if (write_equity_files)
              fprintf(fe, "%8lu  %10.2f  %10.2f  %20.6f  %20.6f\n", t + 1,
                      cum_profit, trades[t].profit(m_cost),
                      trades[t].entry_time.seconds(),
                      trades[t].exit_time.seconds());
          }

          if (write_trade_files)
            fclose(ft);

          if (write_equity_files)
            fclose(fe);
        }

        OverallTradesResults<Qty_t> res(trades, m_cost, m_exclude_sundays,
                                        clean_recs[0].time,
                                        clean_recs[clean_recs.size() - 1].time);

        auto param_values = MakeParamValuesCSV(params.at(this_idx));
        auto res_values = res.MakeCSVValues();
#pragma omp critical(write_summary)
        {
          fprintf(fsum, "%08lu,%s,%s\n", this_idx, param_values.c_str(),
                  res_values.c_str());
        }
      }
    }

    if (write_hdf5) {
      assert(num_trades.size() == m_num_instances);
      assert(offset.size() == m_num_instances);
      assert(ids.size() == m_num_instances);

      // write num_trades and offset to HDF5 file
      hsize_t dims_1d[1] = {m_num_instances};
      H5::DataSpace dat_space_1d(1, dims_1d, dims_1d);
      auto ds_num_trades = h5file->createDataSet(
          "num_trades", H5::PredType::NATIVE_UINT64, dat_space_1d);
      auto ds_offset = h5file->createDataSet(
          "offset", H5::PredType::NATIVE_UINT64, dat_space_1d);
      auto ds_ids = h5file->createDataSet("ids", H5::PredType::NATIVE_UINT64,
                                          dat_space_1d);

      ds_num_trades.write(num_trades.data(), H5::PredType::NATIVE_UINT64);
      ds_offset.write(offset.data(), H5::PredType::NATIVE_UINT64);
      ds_ids.write(ids.data(), H5::PredType::NATIVE_UINT64);

      h5file->close();
    }

    fclose(fsum);

    BLS_DestroyPool();
  }

  struct L1RecT {
    L1RecT() = default;

    L1RecT(const L1Rec &l1_rec, const SecDefD &a_instr)
        : bid(Round(PriceT(double(l1_rec.rec.bid)), a_instr.m_PxStep)),
          ask(Round(PriceT(double(l1_rec.rec.ask)), a_instr.m_PxStep)),
          bid_size(l1_rec.rec.bid_size > 0.0
                       ? long(std::max(1.0, l1_rec.rec.bid_size))
                       : 0L),
          ask_size(l1_rec.rec.ask_size > 0.0
                       ? long(std::max(1.0, l1_rec.rec.ask_size))
                       : 0L) {}

    PriceT bid, ask;
    Qty_t bid_size, ask_size;
  };

  struct TradeRecT {
    TradeRecT() = default;

    TradeRecT(const TradeRec &trade_rec, const SecDefD &a_instr)
        : last(Round(PriceT(double(trade_rec.rec.m_avgPx)), a_instr.m_PxStep)),
          size(trade_rec.rec.m_totQty > 0.0
                   ? long(std::max(1.0, trade_rec.rec.m_totQty))
                   : 0L) {}

    PriceT last;
    Qty_t size;
  };

  struct RecT {
    RecT(utxx::time_val a_time, const L1RecT&    a_rec) : time(a_time), rec(a_rec) {}
    RecT(utxx::time_val a_time, const TradeRecT& a_rec)
        : time(a_time), rec(a_rec) {}

    const utxx::time_val time;
    const std::variant<L1RecT, TradeRecT> rec;
  };

  std::vector<RecT> static MakeJointRec(const RecsT &recs,
                                        bool a_exclude_sundays,
                                        bool a_use_bid_for_ask,
                                        const SecDefD &a_instr) {
    std::vector<RecT> clean_l1;
    clean_l1.reserve(recs.first.size());
    for (size_t i = 0; i < recs.first.size(); ++i) {
      auto time = recs.first[i].ts_recv; // FIXME change to ts_exch

      // January 1, 1970 was a Thursday, so add 4 to make Sunday 0 mod 7
      auto day = 4 + (time.sec() / 86400);

      if (!a_exclude_sundays || (day % 7 != 0)) {
        L1RecT l1(recs.first[i], a_instr);

        if (a_use_bid_for_ask) {
          l1.ask = l1.bid;
          l1.ask_size = l1.bid_size;
        } else if (l1.bid >= l1.ask) {
          l1.ask = l1.bid + a_instr.m_PxStep;
        }

        if (!IsFinite(l1.bid) || IsSpecial0(l1.bid_size) ||
            !IsFinite(l1.ask) || IsSpecial0(l1.ask_size))
          continue;

        clean_l1.emplace_back(time, l1);
      }
    }

    std::vector<RecT> clean_trades;
    clean_trades.reserve(recs.second.size());
    for (size_t i = 0; i < recs.second.size(); ++i) {
      auto time = recs.second[i].ts_recv; // FIXME change to ts_exch

      // January 1, 1970 was a Thursday, so add 4 to make Sunday 0 mod 7
      auto day = 4 + (time.sec() / 86400);

      if (!a_exclude_sundays || (day % 7 != 0)) {
        TradeRecT trade(recs.second[i], a_instr);
        if (!IsFinite(trade.last) || IsSpecial0(trade.size))
          continue;

        clean_trades.emplace_back(time, trade);
      }
    }

    printf("Got %lu quotes and %lu trades\n", clean_l1.size(),
           clean_trades.size());

    if (clean_l1.size() == 0) {
      // we don't have any quotes, use the last price as bid and ask
      std::vector<RecT> merged;
      merged.reserve(2 * clean_trades.size());

      for (size_t i = 0; i < clean_trades.size(); ++i) {
        const auto &t = std::get<TradeRecT>(clean_trades[i].rec);
        L1RecT l1;

        l1.bid = t.last;
        l1.ask = t.last;
        l1.bid_size = t.size;
        l1.ask_size = t.size;

        // first quote, then trade
        merged.emplace_back(clean_trades[i].time, l1);
        merged.push_back(clean_trades[i]);
      }

      return merged;
    }

    if (clean_trades.size() == 0) {
      // we don't have any trades, use mid as trades
      std::vector<RecT> merged;
      merged.reserve(2 * clean_l1.size());

      for (size_t i = 0; i < clean_l1.size(); ++i) {
        const auto &q = std::get<L1RecT>(clean_l1[i].rec);
        TradeRecT trade;

        trade.last = Round(PriceT(0.5 * (double(q.bid) + double(q.ask))),
                           a_instr.m_PxStep);
        trade.size = std::min(q.bid_size, q.ask_size);

        // first quote, then trade
        merged.push_back(clean_l1[i]);
        merged.emplace_back(clean_l1[i].time, trade);
      }

      return merged;
    }

    // if we get here, we have trades and quotes, merge them
    std::vector<RecT> merged;
    merged.reserve(clean_l1.size() + clean_trades.size());

    std::merge(clean_l1.begin(), clean_l1.end(), clean_trades.begin(),
               clean_trades.end(), std::back_inserter(merged),
               [=](const RecT &a, const RecT &b) {
                 return a.time < b.time;
                 //  if (a.time == b.time) {
                 //    // sort L1 before trades
                 //    return a.rec.index() < b.rec.index();
                 //  } else {
                 //    return a.time < b.time;
                 //  }
               });

    return merged;
  }

private:
  template <bool EMULATE_TS>
  __attribute__((always_inline)) std::vector<Trade>
  RunInstance(size_t idx, const BLSParams &params,
              const std::vector<RecT> &recs, bool write_bars_signals,
              const std::string &file_prefix) const {
    try {
      BLSInit(idx, m_ticks_per_point, m_bar_type, m_bar_parameter, params);
    } catch (std::exception &) {
      Trade bad_trade;
      bad_trade.entry = 1.0;
      bad_trade.exit = 0.5;
      bad_trade.entry_time = recs[0].time;
      bad_trade.exit_time = recs[recs.size() - 1].time;
      bad_trade.high = 1.0;
      bad_trade.low = 0.5;
      bad_trade.quantity = m_order_size;

      return {bad_trade};
    }

    Qty_t current_pos = Qty_t::Zero();
    Trade current_trade;
    std::vector<Trade> res;

    // declare these here so they can be captured by reference in the lambda
    // below
    PriceT bid, ask;
    utxx::time_val time;

    // need explicit type here, otherwise we'll get a copy in BLSEngine and the
    // references won't work
    typename BLSEngine<Qty_t, EMULATE_TS>::PlaceOrderFunc place_order =
        [this, &current_pos, &current_trade, &res, /*&bid, &ask,*/ &time]
        (bool a_is_buy, bool /*a_is_aggressive*/, PriceT a_ref_px, Qty_t a_qty)
        {
          if (Abs(a_qty) < m_min_order_size)
            return;

          if ((a_is_buy && (current_pos > 0)) ||
              (!a_is_buy && (current_pos < 0))) {
            return;
          }
          // printf("Place order: %s %li at %.2f (%s bid %.2f, ask %.2f)\n",
          //        a_is_buy ? "Buy" : "Sell", long(a_qty), double(a_ref_px),
          //        time.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
          //        double(bid), double(ask));

          PriceT px = a_ref_px;
          // if (a_is_aggressive)
          //   px = a_is_buy ? ask : bid;
          // else
          //   px = a_is_buy ? std::min(a_ref_px, ask) : std::max(a_ref_px,
          //   bid);

          // if (m_enter_at_bar) {
          //   Bar prev_bar, bar;
          //   BLS_GetLastClosedBar(idx, &prev_bar);
          //   BLS_GetCurrentBar(idx, &bar);

          //   px = a_is_buy ? PriceT(std::min(prev_bar.close, bar.open))
          //                 : PriceT(std::max(prev_bar.close, bar.open));
          // }

          // instantly fill the trade
          auto signed_qty = a_is_buy ? a_qty : -a_qty;

          auto new_qty = current_pos + signed_qty;
          bool entry = false;
          bool exit = false;

          auto &trade = current_trade;

          if (QR(new_qty) * QR(current_pos) > 0) {
            // the sign of the current position has not changed, so we are
            // adding to the current position

            // compute average entry
            double old_q = double(QR(current_pos));
            double add_q = double(QR(signed_qty));
            trade.entry =
                (trade.entry * old_q + double(px) * add_q) / (old_q + add_q);
            trade.quantity += signed_qty;
          } else if (QR(new_qty) * QR(current_pos) == 0) {
            // either new_qty or m_current_position is 0, assume that not
            // both are 0
            if (IsZero(new_qty))
              exit = true;
            else
              entry = true;
          } else {
            // the signs of new_qty and m_current_position are different, so
            // we have an exit and an entry
            exit = true;
            entry = true;
          }

          if (exit) {
            trade.exit = double(px);
            trade.exit_time = time;
            trade.high = std::max(trade.high, double(px));
            trade.low = std::min(trade.low, double(px));

            // push closed trade to list of resulting trades
            res.push_back(trade);
          }

          if (entry) {
            // this is an entry
            trade.entry = double(px);
            trade.entry_time = time;
            trade.high = double(px);
            trade.low = double(px);
            trade.quantity = new_qty;
          }

          current_pos = new_qty;
        };

    FILE *bar_out = nullptr;
    FILE *sig_out = nullptr;

    if (write_bars_signals) {
      char buf[128];
      sprintf(buf, "%s%08lu", file_prefix.c_str(), idx);
      auto bar_file = std::string(buf) + "_bars.txt";
      auto sig_file = std::string(buf) + "_signals.txt";
      bar_out = fopen(bar_file.c_str(), "w");
      sig_out = fopen(sig_file.c_str(), "w");
    }

    BLSEngine<Qty_t, EMULATE_TS> strat(
        idx, m_instr, m_spread_tolerance, m_order_size, m_min_order_size,
        m_exec_mode, place_order, &current_pos, &bar_out, &sig_out);

    auto last_time = recs[0].time;
    for (const auto &rec : recs) {
      time = rec.time;

      if ((m_skip_gaps_over_min > 0.0) &&
          ((time - last_time).seconds() > m_skip_gaps_over_min * 60)) {
        // this is a gap, discard the current open trade and reset the
        // strategy
        current_pos = Qty_t::Zero();
        current_trade = Trade();
        BLSInit(idx, m_ticks_per_point, m_bar_type, m_bar_parameter, params);

        if (idx == 0) {
          std::cerr << "WARNING: Gap detected from "
                    << last_time.to_string(utxx::DATE_TIME_WITH_NSEC) << " to "
                    << time.to_string(utxx::DATE_TIME_WITH_NSEC) << std::endl;
        }
      }
      last_time = time;

      if (rec.rec.index() == 0) {
        const auto &l1 = std::get<L1RecT>(rec.rec);
        bid = l1.bid;
        ask = l1.ask;
        strat.OnQuote(time, bid, ask);
      } else {
        const auto &trade = std::get<TradeRecT>(rec.rec);
        strat.OnTrade(time, trade.last, &current_trade);
      }
    }

    if (write_bars_signals) {
      fclose(bar_out);
      fclose(sig_out);
    }

    if (m_verbose && (idx == 0)) {
      fprintf(stderr,
              "Skipped %.2f%% of ticks (%i of %i) due to spread tolerance\n",
              100.0 * double(strat.m_num_ticks_skipped) /
                  double(strat.m_num_ticks),
              strat.m_num_ticks_skipped, strat.m_num_ticks);
    }

    if (m_verbose) {
      if ((idx + 1) % 50 == 0) {
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed_s =
            double(std::chrono::duration_cast<std::chrono::nanoseconds>(
                       now - m_start_time)
                       .count()) /
            1.0E9;
        double frac_done = double(idx + 1) / double(m_num_instances);
        double estimate_remaining = elapsed_s / frac_done - elapsed_s;
        fprintf(stderr,
                "Completed %8lu of %8lu: %.2f%% done, elapsed: %.0f s, "
                "estimated time remaining: %.0f s\n",
                idx + 1, m_num_instances, 100.0 * frac_done, elapsed_s,
                estimate_remaining);
      }
    }

    return res;
  }

  // some fixed settings for all instances
  const double m_spread_tolerance;
  const Qty_t m_min_order_size, m_order_size;
  const bool m_exclude_sundays;
  const double m_cost;
  const bool m_emulate_ts;
  const bool m_enter_at_bar;
  const std::string m_exec_mode;
  const double m_skip_gaps_over_min;

  const int m_bar_type;
  const double m_bar_parameter;
  const int m_ticks_per_point;
  const bool m_use_bid_for_ask;

  const SecDefD &m_instr;

  const bool m_verbose;

  // for progress update
  mutable std::chrono::high_resolution_clock::time_point m_start_time;
  mutable size_t m_num_instances;
};

} // namespace BLS
} // namespace MAQUETTE
