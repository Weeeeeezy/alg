#include <boost/property_tree/ini_parser.hpp>
#include <chrono>
#include <stdexcept>

#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.hpp"
#include "Basis/PxsQtys.h"
#include "InfraStruct/SecDefsMgr.h"

#include "TradingStrats/BLS/Common/MetaSelector.hpp"
#include "QuantSupport/MDSaver.hpp"

using namespace MAQUETTE;
using namespace BLS;

// Qty Type used by the OMC:
constexpr static QtyTypeT QTO = QtyTypeT::QtyA; // Contracts are OK as well
using QRO = long;
using QtyO = Qty<QTO, QRO>;
using ParallelEngine = BLSParallelEngine<QTO, QRO>;
using RecsT = typename ParallelEngine::RecsT;
using MetaSelector_t = MetaSelector<QTO, QRO>;

auto GetRecs(const boost::property_tree::ptree &pt) {
  RecsT recs;
  auto type = pt.get<std::string>("Type");

  if (type == "VSP") {
    recs.first = GetL1VSPRecs<MDRecL1>(pt.get<std::string>("DataFile"));
  } else if (type == "MDStore") {
    auto tm_to_time_val_utc = [](std::tm tm) {
      return utxx::time_val(tm.tm_year + 1900, unsigned(tm.tm_mon + 1),
                            unsigned(tm.tm_mday), unsigned(tm.tm_hour),
                            unsigned(tm.tm_min), unsigned(tm.tm_sec), true);
    };

    // get the start time
    std::tm start_tm;
    std::istringstream start_iss(pt.get<std::string>("StartTimeUTC"));
    start_iss >> std::get_time(&start_tm, "%Y-%m-%d %H:%M:%S");

    // get optional end time
    std::tm end_tm;
    std::istringstream end_iss(
        pt.get<std::string>("EndTimeUTC", "2100-01-01 00:00:00"));
    end_iss >> std::get_time(&end_tm, "%Y-%m-%d %H:%M:%S");

    utxx::time_val start = tm_to_time_val_utc(start_tm);
    utxx::time_val end = tm_to_time_val_utc(end_tm);

    recs.first = GetMDRecs<MDRecL1>(pt.get<std::string>("MDStoreRoot"),
                                    pt.get<std::string>("Venue"),
                                    pt.get<std::string>("Symbol"), start, end);
    recs.second = GetMDRecs<MDAggression>(
        pt.get<std::string>("MDStoreRoot"), pt.get<std::string>("Venue"),
        pt.get<std::string>("Symbol"), start, end);
  } else {
    throw std::invalid_argument("Data type must be 'VSP' or 'MDStore'");
  }

  return recs;
}

int main(int argc, char *argv[]) {
  IO::GlobalInit({SIGINT});

  if (argc != 2) {
    std::cerr << "PARAMETER: ConfigFile.ini" << std::endl;
    return 1;
  }
  std::string iniFile(argv[1]);

  // Get the Propert Tree:
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(iniFile, pt);

  // make secdef
  double price_step = pt.get<double>("Data.PriceStep");
  SecDefS instr{0,   "AAA/BBB", "",    "",    "MRCXXX", "Hist", "", "SPOT",
                "",  "AAA",     "BBB", 'A',   1.0,      1.0,    1,  price_step,
                'A', 1.0,       0,     79200, 0.0,      0,      ""};

  SecDefsMgr sec_def_mgr(true);
  auto instr_d = sec_def_mgr.Add(instr, false, 21001231);

  // get data
  bool use_bid_for_ask = pt.get<bool>("Data.UseBidForAsk", false);

  auto in_samp_block = pt.get<std::string>("Data.InSample");
  auto in_sample_recs = GetRecs(pt.get_child(in_samp_block));

  auto out_samp_block = pt.get<std::string>("Data.OutOfSample");
  auto out_sample_recs = GetRecs(pt.get_child(out_samp_block));

  auto parallel =
      ParallelEngine(pt.get_child("Strategy"), use_bid_for_ask, instr_d);

  // create selector
  auto sel_params = pt.get_child("Selector");
  auto strat_params = pt.get_child("Strategy");
  MetaSelector_t selector(sel_params, strat_params, parallel);

  // std::vector<double> in_samp_scores, out_samp_scores;
  // std::vector<OverallTradesResults<QtyO>> in_samp_res, out_samp_res;
  // std::tie(best_params, in_samp_scores, in_samp_res, out_samp_scores,
  //          out_samp_res) =
  //     selector.GetBestParameters(in_sample_recs, out_sample_recs);

  selector.GetBestParameters(in_sample_recs, out_sample_recs, true);

  return 0;
}
