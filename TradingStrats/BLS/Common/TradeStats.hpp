#pragma once

#include "TradingStrats/BLS/Common/BLSEngine.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utxx/time_val.hpp>

namespace MAQUETTE {
namespace BLS {

struct Stats {
  double min = 0.0;
  double max = 0.0;
  double median = 0.0;
  double Q1 = 0.0;
  double Q3 = 0.0;
  double avg = 0.0;
  double std_dev = 0.0;
  double skew = 0.0;

  void Compute(std::vector<double> &values) {
    if (values.size() == 0)
      return;

    // sort to get median, Q1, Q3
    std::sort(values.begin(), values.end());

    auto N = values.size();
    min = values[0];
    max = values[N - 1];
    median = Quartile(values, 0.5);
    Q1 = Quartile(values, 0.25);
    Q3 = Quartile(values, 0.75);

    avg = 0.0;
    for (size_t i = 0; i < N; ++i)
      avg += values[i];

    avg /= double(N);

    std_dev = 0.0;
    skew = 0.0;

    for (size_t i = 0; i < N; ++i) {
      auto diff = values[i] - avg;
      auto sqr = diff * diff;
      std_dev += sqr;
      skew += sqr * diff;
    }

    std_dev = sqrt(std_dev / double(N));
    skew = skew / double(N) / (std_dev * std_dev * std_dev);
  }

  // values MUST BE SORTED
  static double Quartile(const std::vector<double> &values, double p) {
    double dk = p * double(values.size() + 1);
    int64_t k = int64_t(dk);
    double alpha = dk - double(k);

    size_t idx_k = std::min(values.size() - 1, size_t(std::max(0L, k)));
    size_t idx_kp1 = std::min(values.size() - 1, size_t(std::max(0L, k + 1)));

    return values[idx_k] + alpha * (values[idx_kp1] - values[idx_k]);
  }
};

// statistics for one group of trades (e.g. winner/loser, all)
template <typename Qty> struct TradesStatistics {
  using Trade = BLSTrade<Qty>;

  int num_trades = 0;

  Stats profit, duration, time_between_trades;

  TradesStatistics() = default;

  TradesStatistics(const std::vector<Trade> &trades, double a_cost) {
    if (trades.size() == 0)
      return;

    size_t N = trades.size();
    num_trades = int(N);
    std::vector<double> profits(N);
    std::vector<double> durations(N);
    std::vector<double> times_between(N - 1);

    profits[0] = trades[0].profit(a_cost);
    durations[0] = trades[0].duration_in_sec();

    // NB: start at i = 1
    for (size_t i = 1; i < N; ++i) {
      profits[i] = trades[i].profit(a_cost);
      durations[i] = trades[i].duration_in_sec();
      times_between[i - 1] =
          trades[i].entry_time.seconds() - trades[i - 1].exit_time.seconds();
    }

    profit.Compute(profits);
    duration.Compute(durations);
    time_between_trades.Compute(times_between);
  }
};

namespace {

// return the trading day number (number of days since 1970-01-01) in UTC, count
// Sundays as Mondays
inline int64_t GetTradingDayNum(utxx::time_val time, bool exclude_weekend)
{
  int64_t day_idx = time.sec() / (24 * 3600);

  if (exclude_weekend) {
    // check if Sunday or Saturday (+4 here because 1970-01-01 was a Thursday)
    if ((day_idx + 4) % 7 == 0) {
      // this is a Sunday, count it as next Monday
      ++day_idx;
    } else if ((day_idx + 4) % 7 == 6) {
      // this is a Saturday, count it as previous Friday
      --day_idx;
    }
  }

  return day_idx;
}
} // namespace

template <typename Qty> struct TradesGroupResults {
  constexpr static int num_trading_days_in_year = 261;
  using Trade = BLSTrade<Qty>;

  TradesStatistics<Qty> all, winners, losers;

  double gross_profit = 0.0;
  double gross_loss = 0.0;
  double net_profit = 0.0;
  double profit_factor = 0.0;

  double fraction_profitable = 0.0;

  double max_drawdown = 0.0;
  double return_on_account = 0.0;

  int64_t num_trading_days;
  double sharpe_ratio = 0.0;
  double avg_num_trades_per_trading_day = 0.0;
  double annual_return = 0.0;

  TradesGroupResults() = default;

  TradesGroupResults(const std::vector<Trade> &trades, double a_cost,
                     bool exclude_weekend, utxx::time_val a_begin_time,
                     utxx::time_val a_end_time) {
    if (trades.size() == 0)
      return;

    // double cum =0;
    // int num = 1;
    // for (const auto &t : trades) {
    //   t.print(a_cost, num, &cum, stdout);
    //   ++num;
    // }

    std::vector<Trade> win, lose;
    double equity = 0.0;
    double watermark = 0.0;

    // get range of trading days
    // auto begin =
    //     a_begin_time.has_value() ? a_begin_time.value() :
    //     trades[0].entry_time;
    // auto end = a_end_time.has_value() ? a_end_time.value()
    //                                   : trades[trades.size() - 1].exit_time;

    if (a_begin_time > a_end_time)
      throw std::runtime_error("TradesGroupResults:: Begin > End");

    auto first_trading_day = GetTradingDayNum(a_begin_time, exclude_weekend);
    auto last_trading_day = GetTradingDayNum(a_end_time, exclude_weekend);
    auto num_days = last_trading_day - first_trading_day + 1;

    if (exclude_weekend) {
      num_trading_days = 0;
      for (int64_t d = 0; d < num_days; ++d) {
        auto day_of_week = (d + first_trading_day + 4) % 7;
        if ((day_of_week != 0) && (day_of_week != 6))
          ++num_trading_days;
      }
    } else {
      num_trading_days = num_days;
    }

    std::vector<double> profit_by_trading_day(size_t(num_days), 0.0);
    // cctz::time_zone nyc;
    // cctz::load_time_zone("America/New_York", &nyc);

    for (const auto &t : trades) {
      double profit = t.profit(a_cost);

      if (profit >= 0.0) {
        win.push_back(t);
        gross_profit += profit;
      } else {
        lose.push_back(t);
        gross_loss += profit;
      }

      equity += profit;
      watermark = std::max(watermark, equity);
      max_drawdown = std::max(max_drawdown, watermark - equity);

      // // determine trading day (int as YYYYMMDD), new trading day starts at
      // // 5 pm Eastern time
      // int yr;
      // unsigned mon, day, hr, min, sec;
      // std::tie(yr, mon, day, hr, min, sec) = t.exit_time.to_ymdhms(true);
      // auto trade_close =
      //     cctz::convert(cctz::civil_second(yr, mon, day, hr, min, sec),
      //                   cctz::utc_time_zone());
      // auto trade_close_et = cctz::convert(trade_close, nyc);

      // // if it's at or after 5 pm Eastern, the trading day date is the date
      // // of the next day
      // if (trade_close_et.hour() >= 17) {
      //   trade_close_et += 24 * 3600;
      // }

      // int trading_day = int(trade_close_et.year()) * 10000 +
      //                   trade_close_et.month() * 100 + trade_close_et.day();

      // scratch that, we'll just use the UTC day and not trade on Sundays, to
      // match Ed

      if ((t.exit_time < a_begin_time) || (t.exit_time > a_end_time))
        throw utxx::runtime_error("Trade exit time outside of time window");

      auto tday_idx =
          GetTradingDayNum(t.exit_time, exclude_weekend) - first_trading_day;
      profit_by_trading_day[size_t(tday_idx)] += profit;
    }

    net_profit = gross_profit + gross_loss;
    if (gross_loss == 0.0)
      profit_factor = 10.0; // FIXME: what's a good value here?
    else
      profit_factor = fabs(gross_profit / gross_loss);

    if (max_drawdown == 0.0)
      return_on_account = 10.0; // FIXME: what's a good value here?
    else
      return_on_account = net_profit / max_drawdown;

    all = TradesStatistics<Qty>(trades, a_cost);
    winners = TradesStatistics<Qty>(win, a_cost);
    losers = TradesStatistics<Qty>(lose, a_cost);

    fraction_profitable = double(winners.num_trades) / double(all.num_trades);

    // compute avg number of trades per trading day, sharpe ratio, annual return
    double nd = double(num_trading_days);

    avg_num_trades_per_trading_day = double(all.num_trades) / nd;
    annual_return = net_profit / nd * double(num_trading_days_in_year);

    if (num_trading_days > 1) {
      double avg_daily_return = 0.0;
      if (exclude_weekend) {
        for (int64_t d = 0; d < num_days; ++d) {
          auto day_of_week = (d + first_trading_day + 4) % 7;
          if ((day_of_week != 0) && (day_of_week != 6)) {
            avg_daily_return += profit_by_trading_day[size_t(d)];
          } else {
            if (profit_by_trading_day[size_t(d)] != 0.0)
              throw std::runtime_error("Got non-zero profit on a weekend");
          }
        }
      } else {
        for (auto profit : profit_by_trading_day)
          avg_daily_return += profit;
      }

      avg_daily_return /= nd;

      double std_dev_daily_return = 0.0;
      if (exclude_weekend) {
        for (int64_t d = 0; d < num_days; ++d) {
          auto day_of_week = (d + first_trading_day + 4) % 7;
          if ((day_of_week != 0) && (day_of_week != 6)) {
            double diff = avg_daily_return - profit_by_trading_day[size_t(d)];
            std_dev_daily_return += diff * diff;
          }
        }
      } else {
        for (auto profit : profit_by_trading_day) {
          double diff = avg_daily_return - profit;
          std_dev_daily_return += diff * diff;
        }
      }

      std_dev_daily_return = sqrt(std_dev_daily_return / (nd - 1.0));
      sharpe_ratio = avg_daily_return / std_dev_daily_return;

      // annualize sharpe ratio:4
      sharpe_ratio *= sqrt(double(num_trading_days_in_year));
    }

    // printf("sharpe: %.2f\n", sharpe_ratio);
  }

  void Print(const char *prefix = "", FILE *fout = stdout) const {
    // clang-format off
    //
    // Net profit: -0,000,000.00, profit factor: 0.00 (0,000,000.00 / 0,000,000.00)
    // Num trades: 000000, profitable: 000.00% (won 000000, lost 000000)
    // Max drawdown: 000,000.00, return on account: 0000.00%
    // Avg annual: -000,000,000.00, 0000.00 trades/day, annual sharpe: 000.00
    //
    //     ALL avg duration: 00 hr 00 min 00 sec, avg between trades: 00 hr 00 min 00 sec
    // Statistic         min          Q1      median          Q3         max        mean     std dev    skewness
    // P/L / trd  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00
    //  Duration  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000
    // Time btwn  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000
    //
    // WINNERS avg duration: 00 hr 00 min 00 sec, avg between trades: 00 hr 00 min 00 sec
    // P/L / trd  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00
    //  Duration  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000
    // Time btwn  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000
    //
    //  LOSERS avg duration: 00 hr 00 min 00 sec, avg between trades: 00 hr 00 min 00 sec
    // P/L / trd  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00  -000000.00
    //  Duration  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000
    // Time btwn  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000  0000000000
    //
    // clang-format on
    auto sec_to_hms = [](double sec_d) {
      int h = 0;
      int m = 0;
      int s = 0;

      int sec = int(sec_d);
      h = sec / 3600;
      m = (sec - h * 3600) / 60;
      s = sec - h * 3600 - m * 60;

      char buffer[32];
      if (h > 0) {
        sprintf(buffer, "%2i hr %2i min %2i sec", h, m, s);
      } else if (m > 0) {
        sprintf(buffer, "%2i min %2i sec", m, s);
      } else {
        sprintf(buffer, "%2i sec", s);
      }

      return std::string(buffer);
    };

    auto print_stats = [=](const TradesStatistics<Qty> &g) {
      fprintf(fout,
              "%s%9s  %10.2f  %10.2f  %10.2f  %10.2f  %10.2f  %10.2f  %10.2f  "
              "%10.2f\n",
              prefix, "P/L / trd", g.profit.min, g.profit.Q1, g.profit.median,
              g.profit.Q3, g.profit.max, g.profit.avg, g.profit.std_dev,
              g.profit.skew);

      fprintf(fout,
              "%s%9s  %10.0f  %10.0f  %10.0f  %10.0f  %10.0f  %10.0f  %10.0f  "
              "%10.2f\n",
              prefix, "Duration", g.duration.min, g.duration.Q1,
              g.duration.median, g.duration.Q3, g.duration.max, g.duration.avg,
              g.duration.std_dev, g.duration.skew);

      fprintf(fout,
              "%s%9s  %10.0f  %10.0f  %10.0f  %10.0f  %10.0f  %10.0f  %10.0f  "
              "%10.2f\n",
              prefix, "Time btw", g.time_between_trades.min,
              g.time_between_trades.Q1, g.time_between_trades.median,
              g.time_between_trades.Q3, g.time_between_trades.max,
              g.time_between_trades.avg, g.time_between_trades.std_dev,
              g.time_between_trades.skew);
    };

    std::stringstream ss1, ss2, ss3;
    ss1.imbue(std::locale("en_US.UTF-8"));
    ss1 << std::noshowbase << std::fixed << std::setprecision(2);
    ss1 << "Net profit: " << std::put_money(net_profit * 100.0);
    ss1 << ", profit factor: " << profit_factor;
    ss1 << " (" << std::put_money(gross_profit * 100.0);
    ss1 << " / " << std::put_money(gross_loss * 100.0) << ")";
    auto profit = ss1.str();

    ss2.imbue(std::locale("en_US.UTF-8"));
    ss2 << std::noshowbase << std::fixed << std::setprecision(2);
    ss2 << "Max drawdown: " << std::put_money(max_drawdown * 100.0);
    ss2 << ", return on account: " << 100.0 * return_on_account << "%";
    auto roa = ss2.str();

    ss3.imbue(std::locale("en_US.UTF-8"));
    ss3 << std::noshowbase << std::fixed << std::setprecision(2);
    ss3 << "Avg annual: " << std::put_money(annual_return * 100.0);
    ss3 << ", trades/day: " << avg_num_trades_per_trading_day;
    ss3 << ", annual sharpe: " << sharpe_ratio;
    auto sharpe = ss3.str();

    fprintf(fout, "%s%s\n", prefix, profit.c_str());
    fprintf(fout, "%sNum trades: %i, profitable: %.2f%% (won %i, lost %i)\n",
            prefix, all.num_trades, fraction_profitable * 100.0,
            winners.num_trades, losers.num_trades);
    fprintf(fout, "%s%s\n", prefix, roa.c_str());
    fprintf(fout, "%s%s\n\n", prefix, sharpe.c_str());

    fprintf(fout, "%s%s avg duration: %s, avg between trades: %s\n", prefix,
            "ALL", sec_to_hms(all.duration.avg).c_str(),
            sec_to_hms(all.time_between_trades.avg).c_str());
    fprintf(fout, "%s%9s  %10s  %10s  %10s  %10s  %10s  %10s  %10s  %10s\n",
            prefix, "Statistic", "Min", "Q1", "Median", "Q3", "Max", "Mean",
            "Std dev", "Skewness");
    print_stats(all);
    fprintf(fout, "\n");

    fprintf(fout, "%s%s avg duration: %s, avg between trades: %s\n", prefix,
            "WINNERS", sec_to_hms(winners.duration.avg).c_str(),
            sec_to_hms(winners.time_between_trades.avg).c_str());
    print_stats(winners);
    fprintf(fout, "\n");

    fprintf(fout, "%s%s avg duration: %s, avg between trades: %s\n", prefix,
            "LOSERS", sec_to_hms(losers.duration.avg).c_str(),
            sec_to_hms(losers.time_between_trades.avg).c_str());
    print_stats(losers);
  }

# ifdef __clang__
# pragma  clang diagnostic push
# pragma  clang diagnostic ignored "-Wformat-nonliteral"
# endif
  std::string MakeCSVValues() const {
    auto print_stats =
    [](const Stats &s, bool decimal) {
      char buffer[1024];
      char const* format = decimal ? ",%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"
                                   : ",%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f";
      sprintf(buffer, format, s.min, s.Q1, s.median, s.Q3, s.max, s.avg,
              s.std_dev, s.skew);
      return std::string(buffer);
    };

    char buffer[1024];
    sprintf(buffer, "%.0f,%.0f,%i,%.2f,%.2f,%.2f,%.4f,%.2f,%.0f", net_profit,
            annual_return, all.num_trades, avg_num_trades_per_trading_day,
            sharpe_ratio, fraction_profitable * 100.0, profit_factor,
            100.0 * return_on_account, max_drawdown);

    auto res = std::string(buffer) + print_stats(all.profit, true) +
               print_stats(all.duration, false) +
               print_stats(all.time_between_trades, true);
    return res;
  }
# ifdef __clang__
# pragma  clang diagnostic pop
# endif
};

template <typename Qty> struct OverallTradesResults {
  using Trade = BLSTrade<Qty>;

  TradesGroupResults<Qty> all, longs, shorts;

  OverallTradesResults() = default;

  static inline const std::vector<std::string> CSV_headers{
      "Net profit",
      "Avg annual",
      "Num trades",
      "Trades/day",
      "Sharpe",
      "Frac win %",
      "Profit fac",
      "ROA %",
      "Max drawdown",
      "PL/trd Min",
      "PL/trd Q1",
      "PL/trd Median",
      "PL/trd Q3",
      "PL/trd Max",
      "PL/trd Mean",
      "PL/trd Std dev",
      "PL/trd Skewness",
      "Duration (sec) Min",
      "Duration (sec) Q1",
      "Duration (sec) Median",
      "Duration (sec) Q3",
      "Duration (sec) Max",
      "Duration (sec) Mean",
      "Duration (sec) Std dev",
      "Duration (sec) Skewness",
      "Time between trds (sec) Min",
      "Time between trds (sec) Q1",
      "Time between trds (sec) Median",
      "Time between trds (sec) Q3",
      "Time between trds (sec) Max",
      "Time between trds (sec) Mean",
      "Time between trds (sec) Std dev",
      "Time between trds (sec) Skewness"};

  OverallTradesResults(const std::vector<Trade> &trades, double a_cost,
                       bool exclude_weekend, utxx::time_val a_begin_time,
                       utxx::time_val a_end_time) {
    if (trades.size() == 0)
      return;

    std::vector<Trade> long_trades, short_trades;
    for (const auto &t : trades) {
      if (t.quantity > 0)
        long_trades.push_back(t);
      else
        short_trades.push_back(t);
    }

    all = TradesGroupResults<Qty>(trades, a_cost, exclude_weekend, a_begin_time,
                                  a_end_time);
    longs = TradesGroupResults<Qty>(long_trades, a_cost, exclude_weekend,
                                    a_begin_time, a_end_time);
    shorts = TradesGroupResults<Qty>(short_trades, a_cost, exclude_weekend,
                                     a_begin_time, a_end_time);
  }

  std::string MakeCSVValues() const { return all.MakeCSVValues(); }

  void Print(const char *prefix = "", FILE *fout = stdout) const {
    std::string pref = std::string(prefix) + "  ";
    fprintf(fout, "%sALL TRADES\n", prefix);
    all.Print(pref.c_str(), fout);

    fprintf(fout, "\n%sLONG TRADES\n", prefix);
    longs.Print(pref.c_str(), fout);

    fprintf(fout, "\n%sSHORT TRADES\n", prefix);
    shorts.Print(pref.c_str(), fout);
  }
};

template <typename Qty>
void PrintParamsAndResults(FILE *fout, const BLSParams &params,
                           const OverallTradesResults<Qty> &results,
                           const std::string &prefix = "") {
  std::string pref = prefix + "  ";
  PrintParams(params, pref.c_str(), fout);
  fprintf(fout, "%s\n", prefix.c_str());
  results.Print(pref.c_str(), fout);
  fprintf(fout, "%s\n", prefix.c_str());
}

template <typename Qty>
void PrintParamsAndResults(
    FILE *fout,
    const std::pair<BLSParamSpace, std::vector<OverallTradesResults<Qty>>> &inp,
    const std::string &prefix = "") {
  for (size_t i = 0; i < inp.first.size(); ++i) {
    fprintf(fout, "============================================================"
                  "============================\n");
    fprintf(fout, "  Number %08lu\n\n", i);
    PrintParamsAndResults(fout, inp.first.at(i), inp.second[i], prefix);
  }
}

} // namespace BLS
} // namespace MAQUETTE
