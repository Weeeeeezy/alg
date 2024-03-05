#pragma once

#include "Protocols/FIX/Msgs.h"
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

#include <boost/algorithm/string.hpp>
#include <boost/container/detail/advanced_insert_int.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include "TradingStrats/BLS/Common/BLS_Strategies.hpp"
//#include "TradingStrats/BLS/Common/TradeStats.hpp"

namespace MAQUETTE {
namespace BLS {

struct SearchRange {
  double start, end, init_step, min_step;
};

namespace {
template <class> inline constexpr bool always_false_v = false;

constexpr static double DoubleOptimValue =
    std::numeric_limits<double>::signaling_NaN();
constexpr static int64_t IntOptimValue = -999;

template <typename T> std::vector<T> MakeList(T start, T end, T step) {
  // crude way to ensure we round up if we're very close but below an integer
  size_t num = std::max(size_t(0.001 + double(end - start) / double(step)) + 1,
                        size_t(1));

  std::vector<T> res(num);

  for (size_t i = 0; i < num; ++i)
    res[i] = start + T(i) * step;

  // make sure we don't go beyond end (but we may not reach it)
  res[num - 1] = std::min(res[num - 1], end);

  return res;
}

template <typename T>
std::pair<std::vector<T>, std::optional<SearchRange>>
MakeListOrSearchRange(const boost::property_tree::ptree &a_params,
                      const std::string &key,
                      const std::optional<double> &default_value) {
  const auto str_to_T = [](const std::string &str) {
    double val = std::stod(str);
    return T(val);
  };

  // there are 3 modes to specify parameter values
  // 1) single value, e.g. "JSize = 15"
  // 2) list of values, e.g. "JSize = [12, 14, 16]"
  // 3) range from start to end with step, e.g. "JSize = {12:2:16}"
  auto entry = a_params.get_optional<std::string>(key);
  if (!entry.has_value()) {
    // use the default value if we have one
    if (default_value.has_value())
      return {{T(default_value.value())}, std::nullopt};
    else
      throw std::runtime_error("Key '" + key +
                               "' is missing and no default value given");
  }

  auto str = entry.value();

  if ((str[0] == '[') && (str[str.size() - 1] == ']')) {
    // we have a list of values
    std::vector<std::string> vals;
    std::string list_str = str.substr(1, str.size() - 2);
    boost::split(vals, list_str, boost::is_any_of(","));

    std::vector<T> res(vals.size());
    for (size_t i = 0; i < res.size(); ++i)
      res[i] = str_to_T(vals[i]);

    return {res, std::nullopt};
  } else if ((str[0] == '{') && (str[str.size() - 1] == '}')) {
    // we have a range
    std::vector<std::string> vals;
    std::string list_str = str.substr(1, str.size() - 2);
    boost::split(vals, list_str, boost::is_any_of(":"));
    if (vals.size() != 3)
      throw std::invalid_argument("'" + str +
                                  "' cannot be parsed as {start:step:end}");

    return {MakeList(str_to_T(vals[0]), str_to_T(vals[2]), str_to_T(vals[1])),
            std::nullopt};
  } else if ((str[0] == '<') && (str[str.size() - 1] == '>')) {
    // we have a search space
    std::vector<std::string> vals;
    std::string list_str = str.substr(1, str.size() - 2);
    boost::split(vals, list_str, boost::is_any_of(","));
    if (vals.size() != 4)
      throw std::invalid_argument(
          "'" + str +
          "' cannot be parsed as <start, end, init step, min step>");

    SearchRange range;
    range.start = std::stod(vals[0]);
    range.end = std::stod(vals[1]);
    range.init_step = std::stod(vals[2]);
    range.min_step = std::stod(vals[3]);

    if constexpr (std::is_same_v<T, double>)
      return {{DoubleOptimValue}, range};
    else
      return {{IntOptimValue}, range};
  } else {
    // we must have a single value, try to read as double and then cast to T
    return {{str_to_T(str)}, std::nullopt};
  }
}

} // namespace

// Helper types and data to deal with different types of parameters
class BLSParamSpace {
private:
  template <typename... Ts> struct GenericFactory {
    using Params = std::variant<Ts...>;
    using VecParams = std::variant<std::vector<Ts>...>;
  };

  struct BLSParamDescriptor {
    const std::string name;
    const bool is_int, can_optimize;
    const std::optional<double> default_value;

    BLSParamDescriptor(const std::string &a_name, bool a_is_int, bool a_can_optimize,
                       const std::optional<double> &a_default_value)
        : name(a_name), is_int(a_is_int), can_optimize(a_can_optimize),
          default_value(a_default_value) {}
  };

  // integer that CANNOT be optimized
  static BLSParamDescriptor Switch(const std::string &name) {
    return BLSParamDescriptor(name, true, false, std::nullopt);
  }
  static BLSParamDescriptor Switch(const std::string &name, int default_value) {
    return BLSParamDescriptor(name, true, false, default_value);
  }

  // integer that CAN be optimized
  static BLSParamDescriptor IntOpt(const std::string &name) {
    return BLSParamDescriptor(name, true, true, std::nullopt);
  }
  static BLSParamDescriptor IntOpt(const std::string &name, int default_value) {
    return BLSParamDescriptor(name, true, true, default_value);
  }

  // double that CAN be optimized
  static BLSParamDescriptor DblOpt(const std::string &name) {
    return BLSParamDescriptor(name, false, true, std::nullopt);
  }
  static BLSParamDescriptor DblOpt(const std::string &name,
                                   double default_value) {
    return BLSParamDescriptor(name, false, true, default_value);
  }

public:
  // ADD NEW PARAMS TYPES HERE
  enum StratType : int {
    HelixOSC = 0,
    RSXonRSX = 1,
    VELonVEL = 2,
    JMACCX = 3,
    JMAXover = 4,
    Trend = 5,
    GRBSystem = 6
  };

private:
  using GenericParamsTypes =
      GenericFactory<BLSParams_HelixOSC, BLSParams_RSXonRSX, BLSParams_VELonVEL,
                     BLSParams_JMACCX, BLSParams_JMAXover, BLSParams_Trend,
                     BLSParams_GRBSystem>;

  static inline const std::map<std::string, StratType> strat_type_by_name{
      {"HelixOSC", HelixOSC},  {"RSXonRSX", RSXonRSX}, {"VELonVEL", VELonVEL},
      {"JMACCX", JMACCX},      {"JMAXover", JMAXover}, {"Trend", Trend},
      {"GRBSystem", GRBSystem}};

  static inline const std::map<StratType, std::string> prefixes{
      {HelixOSC, "HXO"}, {RSXonRSX, "RoR"}, {VELonVEL, "VoV"}, {JMACCX, "CCX"},
      {JMAXover, "JMX"}, {Trend, "Trn"},    {GRBSystem, "GRB"}};

  // NB: THE ORDER OF THE PARAMTER DESCRIPTORS IN common_param_desc AND IN
  // param_desc **MUST EXACTLY MATCH** THE ORDER OF THE PARAMETERS IN THE
  // BLSParams_* structs in 3rdParty/bls/BLS_Strategies.hpp

  static inline const std::vector<BLSParamDescriptor> common_param_desc{
      IntOpt("JSize", 0),
      IntOpt("GRBForwardSize", 0),
      IntOpt("GRBReverseSize", 0),
      IntOpt("GRBGapBricks", 0),
      Switch("GRBDynamicInitMode", 0),
      Switch("TradeDirection"),
      Switch("TradingMode"),
      Switch("EntryOrder"),
      Switch("ForcedExit"),
      Switch("SwapExit"),
      IntOpt("EntryAdjust"),
      IntOpt("StopLoss"),
      DblOpt("ProfitTargetMultiple")};

  static inline const std::map<StratType, std::vector<BLSParamDescriptor>>
      param_desc{
          {HelixOSC,
           {DblOpt("CycleLength"), DblOpt("CycleTolerance"), IntOpt("Delta1"),
            IntOpt("Delta2"), DblOpt("RevThreshold"), DblOpt("Phase"),
            Switch("Smooth"), Switch("BandCriterion")}},
          {RSXonRSX,
           {
               DblOpt("RSXLength"),
               DblOpt("Blend"),
               DblOpt("Phase"),
               DblOpt("TopBand"),
               DblOpt("BottomBand"),
           }},
          {VELonVEL,
           {IntOpt("VELLength"), DblOpt("Blend"), DblOpt("LowerBand"),
            DblOpt("UpperBand")}},
          {JMACCX,
           {IntOpt("DWMALength"), DblOpt("TopLine"), DblOpt("BottomLine")}},
          {JMAXover,
           {DblOpt("JMALength"), DblOpt("JMAPhase"), Switch("DataType"),
            Switch("OtherMAType"), IntOpt("OtherMALength")}},
          {Trend, {IntOpt("Tolerance")}},
          {GRBSystem, {Switch("Polarity")}},
      };

  GenericParamsTypes::VecParams m_params;

  // pair of parameter index and SearchRange
  std::vector<std::pair<size_t, SearchRange>> m_optimization_ranges;

  using ParamVal = std::variant<int64_t, double>;

  void InitVec(StratType type, size_t size) {
    // ADD NEW PARAMS TYPES HERE
    if (type == HelixOSC)
      m_params = std::vector<BLSParams_HelixOSC>(size);
    else if (type == RSXonRSX)
      m_params = std::vector<BLSParams_RSXonRSX>(size);
    else if (type == VELonVEL)
      m_params = std::vector<BLSParams_VELonVEL>(size);
    else if (type == JMACCX)
      m_params = std::vector<BLSParams_JMACCX>(size);
    else if (type == JMAXover)
      m_params = std::vector<BLSParams_JMAXover>(size);
    else if (type == Trend)
      m_params = std::vector<BLSParams_Trend>(size);
    else if (type == GRBSystem)
      m_params = std::vector<BLSParams_GRBSystem>(size);
    else
      throw std::invalid_argument("Unknown BLS strategy type " +
                                  std::to_string(type));
  }

public:
  using BLSParams = GenericParamsTypes::Params;

  BLSParamSpace(StratType a_type, size_t a_size) { InitVec(a_type, a_size); }

  BLSParamSpace(const boost::property_tree::ptree &a_params,
                bool a_optimization_ok = false) {
    auto type =
        strat_type_by_name.at(a_params.get<std::string>("StrategyType"));
    auto desc = ParamDesciptions(type);

    std::vector<std::vector<ParamVal>> values(desc.size());
    std::optional<SearchRange> optim;

    for (size_t p = 0; p < desc.size(); ++p) {
      if (desc[p].is_int) {
        auto res = MakeListOrSearchRange<int64_t>(a_params, desc[p].name,
                                                  desc[p].default_value);
        values[p].resize(res.first.size());
        for (size_t i = 0; i < res.first.size(); ++i)
          values[p][i] = res.first[i];

        optim = res.second;
      } else {
        auto res = MakeListOrSearchRange<double>(a_params, desc[p].name,
                                                 desc[p].default_value);
        values[p].resize(res.first.size());
        for (size_t i = 0; i < res.first.size(); ++i)
          values[p][i] = res.first[i];

        optim = res.second;
      }

      if (optim.has_value()) {
        if (!a_optimization_ok)
          throw std::invalid_argument("Optimization not allowed");

        if (!desc[p].can_optimize)
          throw std::invalid_argument("Cannot optimize parameter " +
                                      desc[p].name);

        m_optimization_ranges.push_back({p, optim.value()});
      }
    }

    size_t total = 1;
    std::vector<size_t> nums(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
      nums[i] = values[i].size();
      total *= nums[i];
    }

    if (total == 0)
      throw std::invalid_argument(
          "At least one list in BLSParamSpace::Init has 0 size");

    InitVec(type, total);

    for (size_t i = 0; i < total; ++i) {
      // convert the flat index i into a 15-dimensional index
      std::vector<size_t> idxs(values.size());
      size_t flat = i;
      for (size_t d = 0; d < nums.size(); ++d) {
        idxs[d] = flat % nums[d];
        flat = flat / nums[d];
      }

      std::visit(
          [&](auto &&res) {
            int64_t *int_ptr = reinterpret_cast<int64_t*>(&res[i]);
            double  *dbl_ptr = reinterpret_cast<double*> (&res[i]);

            for (size_t p = 0; p < nums.size(); ++p) {
              if (desc[p].is_int) {
                // int
                int64_t val = std::get<int64_t>(values[p][idxs[p]]);
                if (!desc[p].can_optimize && (val == IntOptimValue))
                  throw std::invalid_argument("Paramter " + desc[p].name +
                                              " cannot be optimized");

                int_ptr[p] = val;
              } else {
                // double
                double val = std::get<double>(values[p][idxs[p]]);
                if (!desc[p].can_optimize && (val == DoubleOptimValue))
                  throw std::invalid_argument("Paramter " + desc[p].name +
                                              " cannot be optimized");

                dbl_ptr[p] = val;
              }
            }
          },
          m_params);
    }
  }

  size_t size() const {
    return std::visit([=](auto &&p) { return p.size(); }, m_params);
  }

  BLSParams at(size_t idx) const {
    return std::visit([=](auto &&p) { return BLSParams(p[idx]); }, m_params);
  }

  void set(size_t idx, const BLSParams &params) {
    if (params.index() != m_params.index())
      throw std::invalid_argument("BLSParamSpace::set: type mismatch");

    std::visit(
        [=](auto &&p) {
          using T = typename std::decay_t<decltype(p)>::value_type;
          p[idx] = std::get<T>(params);
        },
        m_params);
  }

  StratType GetType() const { return StratType(m_params.index()); }

  std::vector<SearchRange> GetOptimizationSearchSpace() const {
    std::vector<SearchRange> res;
    for (auto const &itm : m_optimization_ranges)
      res.push_back(itm.second);

    return res;
  }

  std::vector<std::string> GetOptimizedParamNames() const {
    auto desc = ParamDesciptions(GetType());
    std::vector<std::string> res;
    for (auto const &itm : m_optimization_ranges)
      res.push_back(desc[itm.first].name);

    return res;
  }

  BLSParams GetConcreteParams(size_t base_idx,
                              const std::vector<double> &optim_values) const {
    if (optim_values.size() != m_optimization_ranges.size())
      throw std::invalid_argument(
          "BLSParamSpace::GetConcreteParams: Wrong number of optimized values");

    auto desc = ParamDesciptions(GetType());
    auto params = at(base_idx);

    std::visit(
        [&](auto &&res) {
          int64_t *int_ptr = reinterpret_cast<int64_t*>(&res);
          double  *dbl_ptr = reinterpret_cast<double*> (&res);

          for (size_t i = 0; i < m_optimization_ranges.size(); ++i) {
            size_t p_idx = m_optimization_ranges[i].first;

            if (!desc[p_idx].can_optimize)
              throw std::invalid_argument("Paramter " + desc[p_idx].name +
                                          " cannot be optimized");

            if (desc[p_idx].is_int) {
              // int
              int_ptr[p_idx] = int(optim_values[i]);
            } else {
              // double
              dbl_ptr[p_idx] = optim_values[i];
            }
          }
        },
        params);

    return params;
  }

  static std::string GetPrefix(const BLSParams &params) {
    return prefixes.at(StratType(params.index()));
  }

  static std::vector<BLSParamDescriptor> ParamDesciptions(StratType type) {
    auto desc = common_param_desc;
    for (const auto &d : param_desc.at(type))
      desc.emplace_back(d);

    return desc;
  }
};

using BLSParams = BLSParamSpace::BLSParams;
using StratType = BLSParamSpace::StratType;

void PrintParams(const BLSParams &params, const char *prefix = "",
                 FILE *fout = stdout) {
  std::visit(
      [=](auto &&p) {
        using T = std::decay_t<decltype(p)>;

        // 4 columns with with 8-char name ": " and 6-char value
        fprintf(
            fout,
            "%s %8s: %-6li  %8s: %-6li  %8s: %-6li  %8s: %-+6li  %8s: %-6li\n",
            prefix, "Trd dir", p.trade_direction, "Trd mode", p.trading_mode,
            "Frcd ext", p.force_market_exit, "Entry", p.entry_order_type,
            "Swap ext", p.swap_exit_params);
        fprintf(fout, "%s %8s: %-6li  %8s: %-6li  %8s: %-6li  %8s: %-6.2f\n",
                prefix, "JTR size", p.JTR_size, "Entr adj",
                p.entry_adjust_ticks, "Stp loss", p.stop_loss_ticks, "Prft mul",
                p.profit_target_multiple_of_stop_loss);
        fprintf(fout, "%s %8s: %-6li  %8s: %-6li  %8s: %-6li  %8s: %-6li\n",
                prefix, "GRB fwd", p.GRB_forward_size, "GRB rev",
                p.GRB_reverse_size, "GRB gap", p.GRB_gap_bricks, "GRB dyn",
                p.GRB_dynamic_shift_mode);

        // ADD NEW PARAMS TYPES HERE
        if constexpr (std::is_same_v<T, BLSParams_HelixOSC>) {
          fprintf(fout, "%s %8s: %-6.2f  %8s: %-6.2f  %8s: %-6li  %8s: %-6li\n",
                  prefix, "Cyc len", p.cycle_length, "Cyc tol",
                  p.cycle_tolerance, "Delta 1", p.delta_1, "Delta 2",
                  p.delta_2);

          fprintf(fout, "%s %8s: %-6.2f  %8s: %-6.2f  %8s: %-6li  %8s: %-6li\n",
                  prefix, "Rev thr", p.reversal_threshold, "Phase", p.phase,
                  "Smooth", p.smooth, "Bnd crit", p.band_criterion);
        } else if constexpr (std::is_same_v<T, BLSParams_RSXonRSX>) {
          fprintf(fout, "%s %8s: %-6.2f  %8s: %-6.2f  %8s: %-6.2f\n", prefix,
                  "RSX len", p.RSX_length, "Blend", p.blend, "Phase", p.phase);

          fprintf(fout, "%s %8s: %-6.2f  %8s: %-6.2f\n", prefix, "Top bnd",
                  p.top_band, "Bot bnd", p.bottom_band);
        } else if constexpr (std::is_same_v<T, BLSParams_VELonVEL>) {
          fprintf(fout,
                  "%s %8s: %-6li  %8s: %-6.2f  %8s: %-6.2f  %8s: %-6.2f\n",
                  prefix, "VEL len", p.VEL_length, "Blend", p.blend, "Lowr bnd",
                  p.lower_band, "Upr bnd", p.upper_band);
        } else if constexpr (std::is_same_v<T, BLSParams_JMACCX>) {
          fprintf(fout, "%s %8s: %-6li  %8s: %-6.2f  %8s: %-6.2f\n", prefix,
                  "DWMA len", p.DWMA_length, "Top line", p.top_line, "Bot line",
                  p.bottom_line);
        } else if constexpr (std::is_same_v<T, BLSParams_JMAXover>) {
          fprintf(fout, "%s %8s: %s  %8s: %s\n", prefix, "Dat type",
                  p.data_type == 0 ? "Close" : "MyPrice", "MA type",
                  p.other_MA_type == 0 ? "DWMA"
                                       : p.other_MA_type == 1 ? "WMA" : "EMA");

          fprintf(fout, "%s %8s: %-6.2f  %8s: %-6.2f  %8s: %-6li\n", prefix,
                  "JMA len", p.JMA_length, "JMA Phas", p.JMA_phase, "MA len",
                  p.other_MA_length);
        } else if constexpr (std::is_same_v<T, BLSParams_Trend>) {
          fprintf(fout, "%s %8s: %-6li\n", prefix, "Tol", p.tolerance);
        } else if constexpr (std::is_same_v<T, BLSParams_GRBSystem>) {
          fprintf(fout, "%s %8s: %-6li\n", prefix, "Polarity", p.polarity);
        } else {
          static_assert(always_false_v<T>,
                        "Non-exhaustive visitor in PrintParams");
        }
      },
      params);
}

std::string MakeParamString(const BLSParams &params) {
  char buf_common[128];
  std::visit(
      [&](auto &&ps) {
        sprintf(
            buf_common,
            "_%03li_%03li_%03li_%li_%li_%+li_%li_%+li_%li_%li_%03li_%03li_%"
            "07.3f__",
            ps.JTR_size, ps.GRB_forward_size, ps.GRB_reverse_size,
            ps.GRB_gap_bricks, ps.GRB_dynamic_shift_mode, ps.trade_direction,
            ps.trading_mode, ps.entry_order_type, ps.force_market_exit,
            ps.swap_exit_params, ps.entry_adjust_ticks, ps.stop_loss_ticks,
            ps.profit_target_multiple_of_stop_loss);
      },
      params);

  char buffer[128];
  std::visit(
      [&](auto &&ps) {
        using T = std::decay_t<decltype(ps)>;

        // ADD NEW PARAMS TYPES HERE
        if constexpr (std::is_same_v<T, BLSParams_HelixOSC>) {
          sprintf(buffer, "%07.3f_%07.3f_%li_%li_%07.3f_%07.3f_%li_%li",
                  ps.cycle_length, ps.cycle_tolerance, ps.delta_1, ps.delta_2,
                  ps.reversal_threshold, ps.phase, ps.smooth,
                  ps.band_criterion);
        } else if constexpr (std::is_same_v<T, BLSParams_RSXonRSX>) {
          sprintf(buffer, "%07.3f_%07.3f_%07.3f_%07.3f_%07.3f", ps.RSX_length,
                  ps.blend, ps.phase, ps.top_band, ps.bottom_band);
        } else if constexpr (std::is_same_v<T, BLSParams_VELonVEL>) {
          sprintf(buffer, "%03li_%07.3f_%07.3f_%07.3f", ps.VEL_length, ps.blend,
                  ps.lower_band, ps.upper_band);
        } else if constexpr (std::is_same_v<T, BLSParams_JMACCX>) {
          sprintf(buffer, "%03li_%07.3f_%07.3f", ps.DWMA_length, ps.top_line,
                  ps.bottom_line);
        } else if constexpr (std::is_same_v<T, BLSParams_JMAXover>) {
          sprintf(buffer, "%07.3f_%07.3f_%li_%li_%03li", ps.JMA_length,
                  ps.JMA_phase, ps.data_type, ps.other_MA_type,
                  ps.other_MA_length);
        } else if constexpr (std::is_same_v<T, BLSParams_Trend>) {
          sprintf(buffer, "%03li", ps.tolerance);
        } else if constexpr (std::is_same_v<T, BLSParams_GRBSystem>) {
          sprintf(buffer, "%+li", ps.polarity);
        } else {
          static_assert(always_false_v<T>,
                        "Non-exhaustive visitor in MakeParamString");
        }
      },
      params);

  return BLSParamSpace::GetPrefix(params) + std::string(buf_common) +
         std::string(buffer);
}

std::vector<std::string> MakeParamTitleCSV(const BLSParams &params) {
  std::vector<std::string> res{"J size",
                               "GRB forward size",
                               "GRB reverse size",
                               "GRB gap bricks",
                               "GRB dynamic shift mode",
                               "Trd dir",
                               "Trd mode",
                               "Entry type",
                               "Forced exit",
                               "Swap exit",
                               "Entry adjust",
                               "Stop loss",
                               "Profit target multiple of stop loss"};

  std::visit(
      [&](auto &&ps) {
        using T = std::decay_t<decltype(ps)>;

        // ADD NEW PARAMS TYPES HERE
        if constexpr (std::is_same_v<T, BLSParams_HelixOSC>) {
          res.push_back("Cycle length");
          res.push_back("Cycle tolerance");
          res.push_back("Delta 1");
          res.push_back("Delta 2");
          res.push_back("Reversal threshold");
          res.push_back("Phase");
          res.push_back("Smooth");
          res.push_back("Band criterion");
        } else if constexpr (std::is_same_v<T, BLSParams_RSXonRSX>) {
          res.push_back("RSX length");
          res.push_back("Blend");
          res.push_back("Phase");
          res.push_back("Top band");
          res.push_back("Bottom band");
        } else if constexpr (std::is_same_v<T, BLSParams_VELonVEL>) {
          res.push_back("VEL length");
          res.push_back("Blend");
          res.push_back("Lower band");
          res.push_back("Upper band");
        } else if constexpr (std::is_same_v<T, BLSParams_JMACCX>) {
          res.push_back("DWMA length");
          res.push_back("Top line");
          res.push_back("Bottom line");
        } else if constexpr (std::is_same_v<T, BLSParams_JMAXover>) {
          res.push_back("JMA length");
          res.push_back("JMA phase");
          res.push_back("Data type");
          res.push_back("Other MA type");
          res.push_back("Other MA length");
        } else if constexpr (std::is_same_v<T, BLSParams_Trend>) {
          res.push_back("Tolerance");
        } else if constexpr (std::is_same_v<T, BLSParams_GRBSystem>) {
          res.push_back("Polarity");
        } else {
          static_assert(always_false_v<T>,
                        "Non-exhaustive visitor in MakeParamTitleCSV");
        }
      },
      params);

  return res;
}

std::string MakeParamValuesCSV(const BLSParams &params) {
  char buf_common[128];
  std::visit(
      [&](auto &&ps) {
        sprintf(buf_common,
                "%li,%li,%li,%li,%li,%li,%li,%li,%li,%li,%li,%li,%.3f,",
                ps.JTR_size, ps.GRB_forward_size, ps.GRB_reverse_size,
                ps.GRB_gap_bricks, ps.GRB_dynamic_shift_mode,
                ps.trade_direction, ps.trading_mode, ps.entry_order_type,
                ps.force_market_exit, ps.swap_exit_params,
                ps.entry_adjust_ticks, ps.stop_loss_ticks,
                ps.profit_target_multiple_of_stop_loss);
      },
      params);

  char buffer[128];
  std::visit(
      [&](auto &&ps) {
        using T = std::decay_t<decltype(ps)>;

        // ADD NEW PARAMS TYPES HERE
        if constexpr (std::is_same_v<T, BLSParams_HelixOSC>) {
          sprintf(buffer, "%.3f,%.3f,%li,%li,%.3f,%.3f,%li,%li",
                  ps.cycle_length, ps.cycle_tolerance, ps.delta_1, ps.delta_2,
                  ps.reversal_threshold, ps.phase, ps.smooth,
                  ps.band_criterion);
        } else if constexpr (std::is_same_v<T, BLSParams_RSXonRSX>) {
          sprintf(buffer, "%.3f,%.3f,%.3f,%.3f,%.3f", ps.RSX_length, ps.blend,
                  ps.phase, ps.top_band, ps.bottom_band);
        } else if constexpr (std::is_same_v<T, BLSParams_VELonVEL>) {
          sprintf(buffer, "%li,%.3f,%.3f,%.3f", ps.VEL_length, ps.blend,
                  ps.lower_band, ps.upper_band);
        } else if constexpr (std::is_same_v<T, BLSParams_JMACCX>) {
          sprintf(buffer, "%li,%.3f,%.3f", ps.DWMA_length, ps.top_line,
                  ps.bottom_line);
        } else if constexpr (std::is_same_v<T, BLSParams_JMAXover>) {
          sprintf(buffer, "%.3f,%.3f,%li,%li,%li", ps.JMA_length, ps.JMA_phase,
                  ps.data_type, ps.other_MA_type, ps.other_MA_length);
        } else if constexpr (std::is_same_v<T, BLSParams_Trend>) {
          sprintf(buffer, "%li", ps.tolerance);
        } else if constexpr (std::is_same_v<T, BLSParams_GRBSystem>) {
          sprintf(buffer, "%li", ps.polarity);
        } else {
          static_assert(always_false_v<T>,
                        "Non-exhaustive visitor in MakeParamString");
        }
      },
      params);

  return std::string(buf_common) + std::string(buffer);
}

void BLSInit(uint64_t index, int ticks_per_point, int bar_type,
             double bar_parameter, const BLSParams &params) {
  // printf("BLSInit: ticks_per_point: %i, bar_type: %i, bar_parameter:
  // %.2f\n",
  //        ticks_per_point, bar_type, bar_parameter);
  std::visit(
      [&](auto &&ps) {
        using T = std::decay_t<decltype(ps)>;

        // ADD NEW PARAMS TYPES HERE
        if constexpr (std::is_same_v<T, BLSParams_HelixOSC>) {
          BLS_InitHelix(index, ticks_per_point, bar_type, bar_parameter,
                        std::get<BLSParams_HelixOSC>(params));
        } else if constexpr (std::is_same_v<T, BLSParams_RSXonRSX>) {
          BLS_InitRSXonRSX(index, ticks_per_point, bar_type, bar_parameter,
                           std::get<BLSParams_RSXonRSX>(params));
        } else if constexpr (std::is_same_v<T, BLSParams_VELonVEL>) {
          BLS_InitVELonVEL(index, ticks_per_point, bar_type, bar_parameter,
                           std::get<BLSParams_VELonVEL>(params));
        } else if constexpr (std::is_same_v<T, BLSParams_JMACCX>) {
          BLS_InitJMACCX(index, ticks_per_point, bar_type, bar_parameter,
                         std::get<BLSParams_JMACCX>(params));
        } else if constexpr (std::is_same_v<T, BLSParams_JMAXover>) {
          BLS_InitJMAXover(index, ticks_per_point, bar_type, bar_parameter,
                           std::get<BLSParams_JMAXover>(params));
        } else if constexpr (std::is_same_v<T, BLSParams_Trend>) {
          BLS_InitTrend(index, ticks_per_point, bar_type, bar_parameter,
                        std::get<BLSParams_Trend>(params));
        } else if constexpr (std::is_same_v<T, BLSParams_GRBSystem>) {
          BLS_InitGRBSystem(index, ticks_per_point, bar_type, bar_parameter,
                            std::get<BLSParams_GRBSystem>(params));
        } else {
          static_assert(always_false_v<T>, "Non-exhaustive visitor in BLSInit");
        }
      },
      params);
}

} // namespace BLS
} // namespace MAQUETTE
