#pragma once

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <limits>
#include <numeric>
#include <vector>

#include "TradingStrats/BLS/Common/BLSParallelEngine.hpp"
#include "TradingStrats/BLS/Common/Params.hpp"
#include "TradingStrats/BLS/Common/TradeStats.hpp"
#include "TradingStrats/BLS/Common/selector/Histogram.hpp"
#include "TradingStrats/BLS/Common/selector/Statistics.hpp"

namespace MAQUETTE {
namespace BLS {

template <QtyTypeT QT, typename QR> class Selector {
public:
  using ParallelEngine = BLSParallelEngine<QT, QR>;
  using Qty_t = typename ParallelEngine::Qty_t;
  using Trade = typename ParallelEngine::Trade;
  using RecsT = typename ParallelEngine::RecsT;

  struct SelectorResult {
    BLSParams params;
    double score;
    OverallTradesResults<Qty_t> trade_results;
  };

  Selector(const boost::property_tree::ptree &a_params,
           const ParallelEngine &a_parallel_engine)
      : m_max_avg_secs_per_trade(a_params.get<double>("MaxAvgSecsPerTrade")),
        m_min_num_trades(a_params.get<int>("MinNumTrades")),
        m_top_score_count(a_params.get<int>("TopScoreCount")),
        m_max_zooms(a_params.get<int>("MaxZooms")),
        m_num_zoom_points(a_params.get<int>("NumZoomPoints")),
        m_max_cross_sections(a_params.get<int>("MaxCrossSections")),
        m_num_cross_section_points(a_params.get<int>("NumCrossSectionPoints")),
        m_parallel_engine(a_parallel_engine) {
    assert(m_max_avg_secs_per_trade >= 1.0);
    assert(m_min_num_trades > 0);
    assert(m_top_score_count > 0);
    assert(m_max_zooms > 0);
    assert(m_num_zoom_points > 0);
    assert(m_max_cross_sections > 0);
    assert(m_num_cross_section_points > 0);
  }

  std::vector<double> ComputeScores(
      const std::vector<OverallTradesResults<Qty_t>> &a_results) const {
    std::vector<double> scores(a_results.size());
    for (size_t i = 0; i < scores.size(); ++i) {
      const auto &stats = a_results[i].all;
      double N = double(stats.all.num_trades);
      double score =
          std::sqrt(stats.profit_factor) * N * stats.return_on_account;

      if (stats.all.duration.avg > m_max_avg_secs_per_trade)
        score = 0.0;

      if (stats.all.num_trades < m_min_num_trades)
        score = 0.0;

      score = std::max(score, 0.0);
      scores[i] = score;
    }

    return scores;
  }

  SelectorResult FindBestParams(size_t a_meta_idx,
                                const BLSParamSpace &a_param_space,
                                const RecsT &a_recs,
                                bool a_write_instance_files,
                                const std::string &a_file_prefix, FILE *a_log) {
    assert(a_log != nullptr);
    m_meta_idx = a_meta_idx;
    m_param_space = &a_param_space;
    m_param_names = m_param_space->GetOptimizedParamNames();
    m_write_instance_files = a_write_instance_files;
    m_step = 0;
    m_file_prefix = a_file_prefix;
    m_fout = a_log;

    auto search_space = m_param_space->GetOptimizationSearchSpace();

    unsigned  NP = unsigned(search_space.size());
    m_num_params = int(NP);
    m_lower_bounds.resize(NP);
    m_upper_bounds.resize(NP);
    m_min_steps   .resize(NP);
    SearchSpace current_space(NP);

    for (unsigned p = 0; p < NP; ++p) {
      assert(search_space[p].end > search_space[p].start);
      assert(search_space[p].init_step > 0.0);
      assert(search_space[p].min_step > 0.0);

      // convert start, end, step to integers
      m_min_steps[p] = search_space[p].min_step;
      m_lower_bounds[p] = int(round(search_space[p].start / m_min_steps[p]));
      m_upper_bounds[p] = int(round(search_space[p].end / m_min_steps[p]));

      current_space[p].start = m_lower_bounds[p];
      current_space[p].end = m_upper_bounds[p];
      // includes endpoints, so +1
      current_space[p].num =
          1 + int(ceil((search_space[p].end - search_space[p].start) /
                       search_space[p].init_step));
    }

    for (int z = 0; z < m_max_zooms; ++z)
      current_space = DoZoom(current_space, a_recs);

    // we're done with zooming, prepare for cross section analysis
    fprintf(m_fout, "Region of interest:\n");
    for (unsigned p = 0; p < NP; ++p)
      fprintf(m_fout, "  %5.2f .. %5.2f\n",
              double(current_space[p].start) * m_min_steps[p],
              double(current_space[p].end) * m_min_steps[p]);

    // keep peak_values at the point with the max score, set the search space by
    // extending by one step in both directions
    for (unsigned p = 0; p < NP; ++p) {
      int step = current_space[p].step();

      current_space[p].start =
          std::max(current_space[p].start - step, m_lower_bounds[p]);
      current_space[p].end =
          std::min(current_space[p].end + step, m_upper_bounds[p]);
      current_space[p].num = m_num_cross_section_points;

      if ((current_space[p].end - current_space[p].start) <
          m_num_cross_section_points) {
        // extend the range in both directions, as possible
        current_space[p].start -= m_num_cross_section_points / 2;
        current_space[p].end += m_num_cross_section_points / 2;

        current_space[p].start =
            std::min(std::max(current_space[p].start, m_lower_bounds[p]),
                     m_upper_bounds[p]);
        current_space[p].end =
            std::min(std::max(current_space[p].end, m_lower_bounds[p]),
                     m_upper_bounds[p]);
      }
    }

    // now do cross section analysis
    bool cross_section_done = true;
    for (int c = 0; c < m_max_cross_sections; ++c) {
      cross_section_done = true;

      // optimize one parameter at a time while keeping the others at their peak
      // values
      for (unsigned p = 0; p < NP; ++p)
        current_space[p] =
            DoCrossSection(current_space, p, a_recs, &cross_section_done);

      if (cross_section_done) {
        // all parameters converged
        break;
      }
    }

    if (!cross_section_done)
      fprintf(m_fout, "WARNING: Cross section analysis did NOT converge\n");

    SelectorResult res;
    auto param_values = ConvertIntParamsToRealParams(m_peak_values);
    res.params = m_param_space->GetConcreteParams(m_meta_idx, param_values);
    res.score = m_peak_score;
    res.trade_results = m_peak_trade_results;

    return res;
  }

private:
  // helper types

  // an integer parameter range
  struct IntSearchRange {
    // start and end are integer parameter values corresponding to the start and
    // end of the range, num is the number of points that the range should be
    // divided into
    int start, end, num;

    int step() const {
      return std::max(1, (end - start) / std::max(1, num - 1));
    }
  };

  using SearchSpace = std::vector<IntSearchRange>;

  struct NoProfitablePoints : public std::exception {
    const char *what() const noexcept override {
      return "*** SELECTOR FAILED *** No profitable points";
    }
  };

  struct ParamSpace {
    std::vector<std::vector<int>> int_params;
    std::vector<std::vector<double>> real_params;

    ParamSpace(size_t a_space_size, size_t a_num_params)
        : int_params(a_space_size, std::vector<int>(a_num_params)),
          real_params(a_space_size, std::vector<double>(a_num_params)) {}
  };

  ParamSpace MakeParamSpace(const SearchSpace &a_space, bool a_print = false) {
    auto np = a_space.size(); // number of parameters
    std::vector<size_t> nums(np);
    std::vector<std::vector<int>> int_param_values(np);

    size_t n_total = 1;
    for (size_t p = 0; p < np; ++p) {
      // we pick parameters in real rather than int space to ensure uniform
      // distribution, they will be rounded to int space
      double start = double(a_space[p].start) * m_min_steps[p];
      double end = double(a_space[p].end) * m_min_steps[p];

      int max_num = 1 + int(ceil((end - start) / m_min_steps[p]));
      int num = std::max(1, std::min(max_num, a_space[p].num));
      nums[p] = size_t(num);
      int_param_values[p].resize(nums[p]);

      double step = 0.0;
      if (num > 1) {
        step = (end - start) / double(nums[p] - 1);
        for (size_t i = 0; i < nums[p]; ++i) {
          double val = start + step * double(i);
          int_param_values[p][i] = int(round(val / m_min_steps[p]));
        }
      } else {
        int_param_values[p][0] = a_space[p].start;
      }
      n_total *= nums[p];

      if (a_print) {
        fprintf(m_fout, "%20s: %6.2f to %6.2f by %6.2f:  [ ",
                m_param_names[p].c_str(), start, end, step);
        fprintf(m_fout, "%6.2f",
                double(int_param_values[p][0]) * m_min_steps[p]);
        for (size_t k = 1; k < nums[p]; ++k)
          fprintf(m_fout, ", %6.2f",
                  double(int_param_values[p][k]) * m_min_steps[p]);
        fprintf(m_fout, " ]\n");
      }
    }

    if (a_print)
      fprintf(m_fout, "\n");

    ParamSpace res(n_total, np);

    std::vector<size_t> idxs(np);
    for (size_t i = 0; i < n_total; ++i) {
      // convert the flat index i into a NUM_PARAMS-dimensional index
      size_t flat = i;
      for (size_t d = 0; d < np; ++d) {
        idxs[d] = flat % nums[d];
        flat = flat / nums[d];
      }

      for (size_t p = 0; p < np; ++p)
        res.int_params[i][p] = int_param_values[p][idxs[p]];

      res.real_params[i] = ConvertIntParamsToRealParams(res.int_params[i]);
    }

    return res;
  }

  ParamSpace MakeCrossSectionParamSpace(const SearchSpace &a_full_space,
                                        unsigned a_param_idx) {
    // set other parameters to their current peak values
    auto search_space = a_full_space;
    unsigned NP       = unsigned(m_num_params);
    for (unsigned k = 0; k < NP; ++k) {
      if (k != a_param_idx) {
        search_space[k].start = m_peak_values[k];
        search_space[k].end = m_peak_values[k];
      }
    }

    fprintf(m_fout, "Current peak: %6.2f", m_peak_values[0] * m_min_steps[0]);
    for (unsigned k = 1; k < NP; ++k)
      fprintf(m_fout, ", %6.2f", m_peak_values[k] * m_min_steps[k]);

    fprintf(m_fout, " (score %10.2f), searching %20s from %6.2f to %6.2f\n",
            m_peak_score, m_param_names[a_param_idx].c_str(),
            search_space[a_param_idx].start * m_min_steps[a_param_idx],
            search_space[a_param_idx].end * m_min_steps[a_param_idx]);

    return MakeParamSpace(search_space);
  }

  std::pair<std::vector<OverallTradesResults<Qty_t>>, std::vector<double>>
  EvaluateSpace(const ParamSpace &a_space, const RecsT &a_recs,
                bool a_print_timing = false) {
    if (a_print_timing) {
      fprintf(m_fout, "Running %lu Helix instances... ",
              a_space.int_params.size());
      fflush(stdout);
    }

    auto start = std::chrono::high_resolution_clock::now();

    BLSParamSpace param_space(m_param_space->GetType(),
                              a_space.real_params.size());
    for (size_t i = 0; i < param_space.size(); ++i)
      param_space.set(i, m_param_space->GetConcreteParams(
                             m_meta_idx, a_space.real_params[i]));

    char buf[128];
    sprintf(buf, "sel_%08lu/step_%08i/", m_meta_idx, m_step);
    ++m_step;

    std::string out_dir(buf);
    std::filesystem::create_directories(out_dir);

    auto res = m_parallel_engine.RunAndAnalyze(
        param_space, a_recs, m_write_instance_files, m_write_instance_files,
        false, false, out_dir);
    auto scores = ComputeScores(res);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_s =
        double(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                   .count()) /
        1.0E9;

    if (m_write_instance_files) {
      std::string fname = out_dir + "parallel_results.txt";
      FILE *fres = fopen(fname.c_str(), "w");
      PrintParamsAndResults(fres, std::make_pair(param_space, res));
      fclose(fres);

      fname = out_dir + "parallel_summary.csv";
      FILE *fsum = fopen(fname.c_str(), "w");
      std::vector<std::string> headers{"Id"};
      auto param_titles = MakeParamTitleCSV(param_space.at(0));
      auto res_headers = OverallTradesResults<Qty_t>::CSV_headers;
      headers.insert(headers.end(), param_titles.begin(), param_titles.end());
      headers.push_back("Score");
      headers.insert(headers.end(), res_headers.begin(), res_headers.end());

      auto head = "\"" + boost::algorithm::join(headers, "\",\"") + "\"";
      fprintf(fsum, "%s\n", head.c_str());

      for (size_t i = 0; i < res.size(); ++i) {
        auto param_values = MakeParamValuesCSV(param_space.at(i));
        auto res_values = res[i].MakeCSVValues();
        fprintf(fsum, "%08lu,%s,%.5f,%s\n", i, param_values.c_str(), scores[i],
                res_values.c_str());
      }

      fclose(fsum);
    }

    if (a_print_timing)
      fprintf(m_fout, "took %.3f seconds\n\n", elapsed_s);

    return {res, scores};
  }

  // return new narrower search space after zooming into the current search
  // space
  SearchSpace DoZoom(const SearchSpace &a_space, const RecsT &a_recs) {
    auto param_space = MakeParamSpace(a_space, true);
    auto eval_res = EvaluateSpace(param_space, a_recs, true);
    auto trade_stats = eval_res.first;
    auto scores = eval_res.second;
    unsigned NP = unsigned(m_num_params);

    // do an index sort
    std::vector<size_t> idx(scores.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(),
                     [&](size_t a, size_t b) { return scores[a] > scores[b]; });

    // make sure the max score is > 0
    if (scores[idx[0]] <= 0.0) {
      // we have no positive scores at all, abort
      throw NoProfitablePoints();
    }

    // minus 1 because this is 0-indexed
    size_t threshold_idx =
        std::min(idx[size_t(m_top_score_count - 1)], scores.size() - 1);
    auto score_threshold = scores[threshold_idx];

    if (score_threshold <= 0.0) {
      fprintf(m_fout, "WARNING: Score threshold is <= 0, picking smallest "
                      "positive score as threshold\n");
      for (size_t i = 0; i < scores.size(); ++i) {
        if (scores[idx[i]] > 0.0)
          score_threshold = scores[idx[i]];
        else
          break;
      }
    }

    std::vector<std::vector<int>> best_params;
    for (size_t i = 0; i < scores.size(); ++i) {
      if (scores[i] >= score_threshold)
        best_params.push_back(param_space.int_params[i]);
    }

    fprintf(m_fout,
            "Max score: %.2f, score threshold: %.2f, num above threshold: %lu, "
            "5 best points:\n",
            scores[idx[0]], score_threshold, best_params.size());
    for (size_t i = 0; i < std::min(size_t(5), scores.size()); ++i) {
      auto param_values = param_space.real_params[idx[i]];

      fprintf(m_fout, "%10.2f:  %6.2f", scores[idx[i]], param_values[0]);
      for (unsigned k = 1; k < NP; ++k)
        fprintf(m_fout, ", %6.2f", param_values[k]);
      fprintf(m_fout, "\n");
    }
    fprintf(m_fout, "\n");

    m_peak_values = param_space.int_params[idx[0]];
    m_peak_score = scores[idx[0]];
    m_peak_trade_results = trade_stats[idx[0]];

    // for each parameter, get list of values in best_params
    std::vector<std::vector<int>> param_values(
        NP, std::vector<int>(best_params.size()));
    for (unsigned p = 0; p < NP; ++p) {
      for (unsigned i = 0; i < best_params.size(); ++i)
        param_values[p][i] = best_params[i][p];
    }

    SearchSpace new_space(NP);
    for (unsigned p = 0; p < NP; ++p) {
      Histogram hist(param_values[unsigned(p)]);
      auto strong_subrange_idxs = FindStrongSubrange(hist.counts(), true);

      // set parameter space for next zoom
      int start = hist.lower_edges()[strong_subrange_idxs[0]];
      int end = hist.lower_edges()[strong_subrange_idxs[1]] + hist.bin_width();

      new_space[p].start = std::max(m_lower_bounds[p], start);
      new_space[p].end = std::min(m_upper_bounds[p], end);
      new_space[p].num = m_num_zoom_points;

      if ((new_space[p].end - new_space[p].start) < m_num_zoom_points) {
        // extend the range in both directions, as possible
        new_space[p].start -= m_num_zoom_points / 2;
        new_space[p].end += m_num_zoom_points / 2;

        new_space[p].start = std::min(
            std::max(new_space[p].start, m_lower_bounds[p]), m_upper_bounds[p]);
        new_space[p].end = std::min(
            std::max(new_space[p].end, m_lower_bounds[p]), m_upper_bounds[p]);
      }
    }
    return new_space;
  }

  // return the narrower parameter range after optimizing just one parameter
  // given by a_param_idx
  IntSearchRange DoCrossSection(const SearchSpace &a_full_space,
                                unsigned a_param_idx, const RecsT &a_recs,
                                bool *a_converged) {
    auto param_space = MakeCrossSectionParamSpace(a_full_space, a_param_idx);
    auto eval_res = EvaluateSpace(param_space, a_recs);
    auto raw_trade_stats = eval_res.first;
    auto raw_scores = eval_res.second;

    // do an index sort by the parameter value
    std::vector<size_t> idx(raw_scores.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
      return param_space.int_params[a][a_param_idx] <
             param_space.int_params[b][a_param_idx];
    });

    std::vector<double> scores(raw_scores.size());
    std::vector<int> param_values(raw_scores.size());
    std::vector<OverallTradesResults<Qty_t>> trade_stats(raw_scores.size());

    double max_score = raw_scores[idx[0]];
    size_t max_score_idx = 0;
    for (size_t i = 0; i < scores.size(); ++i) {
      scores[i] = raw_scores[idx[i]];
      trade_stats[i] = raw_trade_stats[idx[i]];
      param_values[i] = param_space.int_params[idx[i]][a_param_idx];

      if (scores[i] > max_score) {
        max_score = scores[i];
        max_score_idx = i;
      }
    }

    // smooth out scores
    auto weighted_median_3 = WeightedMedian3(scores);
    auto weighted_median_5 = WeightedMedian5(scores);
    std::vector<double> smoothed_scores(scores.size());
    for (size_t i = 0; i < scores.size(); ++i) {
      smoothed_scores[i] = 0.5 * (weighted_median_3[i] + weighted_median_5[i]);
    }

    auto xrw = XRW(smoothed_scores, double(scores.size()) / 8.0);
    double max_xrw = 0.0;
    for (size_t i = 0; i < xrw.size(); ++i)
      max_xrw = std::max(max_xrw, xrw[i]);

    for (size_t i = 0; i < xrw.size(); ++i)
      xrw[i] -= 0.5 * max_xrw;

    auto new_range = FindStrongSubrange(xrw, false);

    // find peak index and value
    size_t peak_idx = new_range[0];
    for (size_t i = new_range[0]; i <= new_range[1]; ++i) {
      if (xrw[i] > xrw[peak_idx])
        peak_idx = i;
    }

    // check that peak score is not much less than max score
    if (scores[peak_idx] < 0.75 * max_score) {
      // fprintf(m_fout,
      //         "WARNING: Peak score (%.2f) < 75%% of max score (%.2f), picking
      //         " "max param value (%.2f) instead of peak value (%.2f)\n",
      //         scores[peak_idx], max_score,
      //         param_values[max_score_idx] * m_min_steps[a_param_idx],
      //         param_values[peak_idx] * m_min_steps[a_param_idx]);
      // peak_idx = max_score_idx;

      fprintf(m_fout,
              "WARNING: Peak score (%.2f, param value %.2f) < 75%% of max "
              "score (%.2f, param value %.2f)\n",
              scores[peak_idx],
              param_values[peak_idx] * m_min_steps[a_param_idx], max_score,
              param_values[max_score_idx] * m_min_steps[a_param_idx]);
      // peak_idx = max_score_idx;
    }

    int old_peak_value = m_peak_values[a_param_idx];
    m_peak_values[a_param_idx] = param_values[peak_idx];
    m_peak_score = scores[peak_idx];
    m_peak_trade_results = trade_stats[peak_idx];

    if (old_peak_value != m_peak_values[a_param_idx]) {
      // peak value moved, keep iterating
      *a_converged = false;
    }

    // set the new search range and extend it if the peak is near the edge
    int start = param_values[new_range[0]];
    int end = param_values[new_range[1]];
    int step = a_full_space[a_param_idx].step();

    // printf("start: %i, end: %i, step: %i, peak_idx = %i, n = %lu, new_range:
    // "
    //        "%i..%i\n",
    //        start, end, step, peak_idx, param_values.size(), new_range[0],
    //        new_range[1]);

    if (peak_idx == 0) {
      start -= 6 * step;
    } else if (new_range[0] == 0) {
      start -= 3 * step;
    } else {
      start -= 1 * step;
    }

    if (peak_idx == (scores.size() - 1)) {
      end += 6 * step;
    } else if (new_range[1] == (scores.size() - 1)) {
      end += 3 * step;
    } else {
      end += 1 * step;
    }

    start = std::min(std::max(start, m_lower_bounds[a_param_idx]),
                     m_upper_bounds[a_param_idx]);
    end = std::min(std::max(end, m_lower_bounds[a_param_idx]),
                   m_upper_bounds[a_param_idx]);

    if ((end - start) < m_num_cross_section_points) {
      // extend the range in both directions, as possible
      start -= m_num_cross_section_points / 2;
      end += m_num_cross_section_points / 2;

      start = std::min(std::max(start, m_lower_bounds[a_param_idx]),
                       m_upper_bounds[a_param_idx]);
      end = std::min(std::max(end, m_lower_bounds[a_param_idx]),
                     m_upper_bounds[a_param_idx]);
    }

    IntSearchRange res;
    res.start = start;
    res.end = end;
    res.num = m_num_cross_section_points;

    return res;
  }

  // options
  const double
      m_max_avg_secs_per_trade; // maximum average duration per trade in seconds
  const int m_min_num_trades;   // minimum number of trades for the point to be
                                // considered
  const int m_top_score_count; // number of top scoring combinations for cluster
                               // analysis
  const int m_max_zooms;       // maximum number of multi-D optimizations
  const int m_num_zoom_points; // number of points for zooming
  const int m_max_cross_sections; // maximum number of cross-section analyses
  const int
      m_num_cross_section_points; // number of points for cross-section analysis

  const ParallelEngine &m_parallel_engine;

  // state
  size_t m_meta_idx;
  const BLSParamSpace *m_param_space;
  std::vector<std::string> m_param_names;

  bool m_write_instance_files;
  int m_step;
  std::string m_file_prefix;
  FILE *m_fout;

  int m_num_params;

  // Internally, we use integers to represent the parameter values. We search on
  // a grid of m_min_steps. The integers range from m_lower_bounds to
  // m_upper_bounds
  std::vector<double> m_min_steps;
  std::vector<int> m_lower_bounds, m_upper_bounds;

  std::vector<double>
  ConvertIntParamsToRealParams(const std::vector<int> &params) {
    std::vector<double> res(params.size());

    unsigned NP = unsigned(m_num_params);
    for (unsigned p = 0; p < NP; ++p)
      res[p] = params[p] * m_min_steps[p];

    return res;
  }

  // current best param values, their score, and their trade results
  std::vector<int> m_peak_values;
  double m_peak_score;
  OverallTradesResults<Qty_t> m_peak_trade_results;
};

} // namespace BLS
} // namespace MAQUETTE
