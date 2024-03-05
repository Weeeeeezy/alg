#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>

#include "TradingStrats/BLS/Common/Params.hpp"
#include "TradingStrats/BLS/Common/BLSEngine.hpp"
#include "TradingStrats/BLS/Common/BLSParallelEngine.hpp"
#include "TradingStrats/BLS/Common/selector/Selector.hpp"

namespace MAQUETTE {
namespace BLS {

template <QtyTypeT QT, typename QR> class MetaSelector {
public:
  using Qty_t = Qty<QT, QR>;
  using ParallelEngine = BLSParallelEngine<QT, QR>;
  using Trade = typename ParallelEngine::Trade;
  using RecsT = typename ParallelEngine::RecsT;

  using Selector_t = Selector<QT, QR>;
  using SelectorResult = typename Selector<QT, QR>::SelectorResult;

  MetaSelector(const boost::property_tree::ptree &a_selector_params,
               const boost::property_tree::ptree &a_strategy_params,
               const ParallelEngine &a_parallel_engine)
      : m_meta_params(a_strategy_params, true),
        m_parallel_engine(a_parallel_engine),
        m_selector(a_selector_params, a_parallel_engine) {}

  std::tuple<BLSParamSpace, std::vector<double>,
             std::vector<OverallTradesResults<Qty<QT, QR>>>>
  GetBestParameters(const RecsT &a_recs, bool a_write_instance_files = false,
                    const std::string &file_prefix = "", FILE *a_log = stdout) {
    auto res = GetBestParameters(a_recs, {}, a_write_instance_files,
                                 file_prefix, a_log);
    return {std::get<0>(res), std::get<1>(res), std::get<2>(res)};
  }

  // Tuple is: <params, in-sample scores, in-sample results, out-of-sample
  // scores, out-of-sample results
  std::tuple<BLSParamSpace, std::vector<double>,
             std::vector<OverallTradesResults<Qty<QT, QR>>>,
             std::vector<double>,
             std::vector<OverallTradesResults<Qty<QT, QR>>>>
  GetBestParameters(const RecsT &a_recs, const RecsT &a_out_sample_recs,
                    bool a_write_instance_files = false,
                    const std::string &file_prefix = "", FILE *a_log = stdout) {
    BLSParamSpace best_params(m_meta_params.GetType(), m_meta_params.size());
    std::vector<double> in_scores(m_meta_params.size());
    std::vector<double> out_scores(m_meta_params.size());

    // step through the meta space to collect parameters and scores (in and out
    // of sample, if available)
    for (size_t meta_idx = 0; meta_idx < m_meta_params.size(); ++meta_idx) {
      auto best = GetBestParameters(meta_idx, a_recs, a_out_sample_recs,
                                    a_write_instance_files, file_prefix, a_log);
      best_params.set(meta_idx, best.first.params);
      in_scores[meta_idx] = best.first.score;

      if (best.second.has_value())
        out_scores[meta_idx] = best.second.value().score;
    }

    // get the trade results for all best params, we run the parameter space
    // again so we can write output files for all instances to the output dirs
    std::string out_dir = "meta_in_sample/";
    std::filesystem::create_directories(out_dir);
    auto in_samp_results = m_parallel_engine.RunAndAnalyze(
        best_params, a_recs, true, true, false, false, out_dir);

    bool have_out = ((a_out_sample_recs.first.size() +
                      a_out_sample_recs.second.size()) > 0);
    std::vector<OverallTradesResults<Qty<QT, QR>>> out_samp_results;

    if (have_out) {
      std::string out_dir1 = "meta_out_of_sample/";
      std::filesystem::create_directories(out_dir1);
      out_samp_results = m_parallel_engine.RunAndAnalyze(
          best_params, a_out_sample_recs, true, true, false, false, out_dir1);
    }

    // write meta results to CSV file
    FILE *fres = fopen("selector_meta_results_in_sample.txt", "w");
    PrintParamsAndResults(fres, std::make_pair(best_params, in_samp_results));
    fclose(fres);

    fres = fopen("selector_meta_results_out_of_sample.txt", "w");
    PrintParamsAndResults(fres, std::make_pair(best_params, out_samp_results));
    fclose(fres);

    FILE *fsum = fopen("selector_meta_summary.csv", "w");
    std::vector<std::string> headers{"Id"};
    auto param_titles = MakeParamTitleCSV(best_params.at(0));
    auto res_headers = OverallTradesResults<Qty_t>::CSV_headers;

    headers.push_back("Score (in sample)");
    for (const auto &h : res_headers)
      headers.push_back(h + " (in sample)");

    if (have_out) {
      headers.push_back("Score (out of sample)");
      for (const auto &h : res_headers)
        headers.push_back(h + " (out of sample)");
    }

    auto head = "\"" + boost::algorithm::join(headers, "\",\"") + "\"";
    fprintf(fsum, "%s\n", head.c_str());

    for (size_t i = 0; i < in_samp_results.size(); ++i) {
      auto param_values = MakeParamValuesCSV(best_params.at(i));
      auto in_values = in_samp_results[i].MakeCSVValues();
      fprintf(fsum, "%08lu,%s,%.5f,%s", i, param_values.c_str(), in_scores[i],
              in_values.c_str());

      if (have_out) {
        auto out_values = in_samp_results[i].MakeCSVValues();
        fprintf(fsum, ",%.5f,%s", out_scores[i], out_values.c_str());
      }
      fprintf(fsum, "\n");
    }

    fclose(fsum);

    return {best_params, in_scores, in_samp_results, out_scores,
            out_samp_results};
  }

private:
  // in-sample and out-of sample results
  std::pair<SelectorResult, std::optional<SelectorResult>>
  GetBestParameters(size_t meta_idx, const RecsT &a_recs,
                    const RecsT &a_out_sample_recs, bool a_write_instance_files,
                    const std::string &file_prefix, FILE *a_log) {
    fprintf(a_log, "==========================================================="
                   "=============================\n\n");
    fprintf(a_log, "Starting new search:\n\n");
    PrintParams(m_meta_params.at(meta_idx), "  ", a_log);
    fprintf(a_log, "\n");

    auto res =
        m_selector.FindBestParams(meta_idx, m_meta_params, a_recs,
                                  a_write_instance_files, file_prefix, a_log);

    fprintf(a_log, "\n>>>> SELECTED PARAMETERS <<<<\n\n");
    PrintParams(res.params, "  ", a_log);
    fprintf(a_log, "\n");
    res.trade_results.Print("  ", a_log);

    std::optional<SelectorResult> out_samp_res = std::nullopt;

    if ((a_out_sample_recs.first.size() + a_out_sample_recs.second.size()) >
        0) {
      out_samp_res = SelectorResult();
      out_samp_res.value().params = res.params;

      BLSParamSpace space(m_meta_params.GetType(), 1);
      space.set(0, res.params);

      auto out_sample_trades = m_parallel_engine.Run(space, a_out_sample_recs);
      fprintf(a_log, "\nOUT OF SAMPLE PERFORMANCE\n\n");

      auto start_time = std::min(a_out_sample_recs.first.size() > 0
                                     ? a_out_sample_recs.first[0].ts_exch
                                     : utxx::secs(1e300),
                                 a_out_sample_recs.second.size() > 0
                                     ? a_out_sample_recs.second[0].ts_exch
                                     : utxx::secs(1e300));

      auto end_time = std::max(
          a_out_sample_recs.first.size() > 0
              ? a_out_sample_recs.first[a_out_sample_recs.first.size() - 1]
                    .ts_exch
              : utxx::secs(0),
          a_out_sample_recs.second.size() > 0
              ? a_out_sample_recs.second[a_out_sample_recs.second.size() - 1]
                    .ts_exch
              : utxx::secs(0));

      OverallTradesResults<Qty<QT, QR>> out_samp_trades_res(
          out_sample_trades[0], m_parallel_engine.Cost(),
          m_parallel_engine.ExcludeSundays(), start_time, end_time);
      out_samp_trades_res.Print("  ", a_log);

      out_samp_res.value().trade_results = out_samp_trades_res;
    }
    fprintf(a_log, "\n\n");

    return {res, out_samp_res};
  }

  BLSParamSpace m_meta_params;

  const ParallelEngine &m_parallel_engine;
  Selector_t m_selector;
};

} // namespace BLS
} // namespace MAQUETTE
