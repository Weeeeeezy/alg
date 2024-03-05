#include <boost/algorithm/string/join.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <chrono>
#include <stdexcept>
#include <utility>
#include <utxx/time_val.hpp>

#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.hpp"
#include "Basis/PxsQtys.h"
#include "InfraStruct/SecDefsMgr.h"

#include "TradingStrats/BLS/Common/BLSParallelEngine.hpp"
#include "QuantSupport/MDSaver.hpp"

using namespace MAQUETTE;
using namespace BLS;

// Qty Type used by the OMC:
constexpr static QtyTypeT QTO = QtyTypeT::QtyA; // Contracts are OK as well
using QRO = long;
using QtyO = Qty<QTO, QRO>;
using ParallelEngine = BLSParallelEngine<QTO, QRO>;
using RecsT = typename ParallelEngine::RecsT;

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

  // get data
  bool use_bid_for_ask = pt.get<bool>("Data.UseBidForAsk", false);
  bool use_quotes = pt.get<bool>("Data.UseQuotes", true);
  bool use_trades = pt.get<bool>("Data.UseTrades", true);
  auto data = pt.get<std::string>("Data.Source");

  double price_step = pt.get<double>("Data.PriceStep");
  SecDefS instr{0,   "AAA/BBB", "",    "",    "MRCXXX", "Hist", "", "SPOT",
                "",  "AAA",     "BBB", 'A',   1.0,      1.0,    1,  price_step,
                'A', 1.0,       0,     79200, 0.0,      0,      ""};

  SecDefsMgr* sec_def_mgr = SecDefsMgr::GetPersistInstance(true, "");
  auto instr_d = sec_def_mgr->Add(instr, false, 21001231, 0, 0.0, 0.0);

  RecsT recs;

  if ((data == "VSP") && use_quotes) {
    recs.first = GetL1VSPRecs<MDRecL1>(pt.get<std::string>("VSP.DataFile"));
  } else if (data == "MDStore") {
    auto data_ps = pt.get_child("MDStore");

    auto tm_to_time_val_utc = [](std::tm tm) {
      return utxx::time_val(tm.tm_year + 1900, unsigned(tm.tm_mon + 1),
                            unsigned(tm.tm_mday), unsigned(tm.tm_hour),
                            unsigned(tm.tm_min), unsigned(tm.tm_sec), true);
    };

    // get the start time
    std::tm start_tm;
    std::istringstream start_iss(data_ps.get<std::string>("StartTimeUTC"));
    start_iss >> std::get_time(&start_tm, "%Y-%m-%d %H:%M:%S");

    // get optional end time
    std::tm end_tm;
    std::istringstream end_iss(
        data_ps.get<std::string>("EndTimeUTC", "2100-01-01 00:00:00"));
    end_iss >> std::get_time(&end_tm, "%Y-%m-%d %H:%M:%S");

    utxx::time_val start = tm_to_time_val_utc(start_tm);
    utxx::time_val end = tm_to_time_val_utc(end_tm);

    if (use_quotes) {
      recs.first =
          GetMDRecs<MDRecL1>(data_ps.get<std::string>("MDStoreRoot"),
                             data_ps.get<std::string>("Venue"),
                             data_ps.get<std::string>("Symbol"), start, end);
    }

    if (use_trades) {
      recs.second = GetMDRecs<MDAggression>(
          data_ps.get<std::string>("MDStoreRoot"),
          data_ps.get<std::string>("Venue"), data_ps.get<std::string>("Symbol"),
          start, end);
    }
  } else {
    throw std::invalid_argument("Data.Source must be 'VSP' or 'MDStore'");
  }

  auto params = BLSParamSpace(pt.get_child("Strategy"));

  auto parallel =
      ParallelEngine(pt.get_child("Strategy"), use_bid_for_ask, instr_d, true);

  bool write_trade_files = pt.get<bool>("Main.WriteTradeFiles", true);
  bool write_equity_files = pt.get<bool>("Main.WriteEquityFiles", true);
  bool write_hdf5 = pt.get<bool>("Main.WriteHDF5", true);
  bool write_bars_signals = pt.get<bool>("Main.WriteBarsSignals", false);

  auto start = std::chrono::high_resolution_clock::now();
  parallel.RunAndAnalyze(params, recs, write_trade_files, write_equity_files,
                         write_hdf5, write_bars_signals);
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed_s =
      double(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                 .count()) /
      1.0E9;
  printf("Running %lu strategy instances took %.6f seconds\n", params.size(),
         elapsed_s);

  printf("Writing summary files (this can take a few minutes for large "
         "parameter spaces)\n");

  return 0;
}
