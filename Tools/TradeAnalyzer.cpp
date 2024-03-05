#include <algorithm>
#include <chrono>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <H5Cpp.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <omp.h>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Basis/IOUtils.hpp"
#include "Basis/PxsQtys.h"
#include "TradingStrats/BLS/Common/BLSEngine.hpp"
#include "TradingStrats/BLS/Common/Params.hpp"
#include "TradingStrats/BLS/Common/TradeStats.hpp"

using namespace MAQUETTE;
using namespace BLS;

using Qty_t = Qty<QtyTypeT::QtyA, double>;
using Trade = BLSTrade<Qty_t>;

int main(int argc, char **argv) {
  IO::GlobalInit({SIGINT});

  if (argc != 2) {
    std::cerr << "PARAMETER: ConfigFile.ini" << std::endl;
    return 1;
  }
  std::string iniFile(argv[1]);

  // Get the Propert Tree:
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(iniFile, pt);

  auto analysis_ps = pt.get_child("Analysis");
  auto infile = analysis_ps.get<std::string>("Input");

  // get the start time
  std::tm start_tm;
  std::istringstream start_iss(analysis_ps.get<std::string>("StartTimeUTC"));
  start_iss >> std::get_time(&start_tm, "%Y-%m-%d %H:%M:%S");

  // get optional end time
  std::tm end_tm;
  std::istringstream end_iss(
      analysis_ps.get<std::string>("EndTimeUTC", "2100-01-01 00:00:00"));
  end_iss >> std::get_time(&end_tm, "%Y-%m-%d %H:%M:%S");

  auto tm_to_time_val_utc = [](std::tm tm) {
    return utxx::time_val(tm.tm_year + 1900, unsigned(tm.tm_mon + 1),
                          unsigned(tm.tm_mday), unsigned(tm.tm_hour),
                          unsigned(tm.tm_min), unsigned(tm.tm_sec), true);
  };

  utxx::time_val start = tm_to_time_val_utc(start_tm);
  utxx::time_val end = tm_to_time_val_utc(end_tm);

  int window_sec = analysis_ps.get<int>("WindowInSec");
  int step_sec = analysis_ps.get<int>("StepInSec");

  double cost = analysis_ps.get<double>("Cost");
  bool exclude_weekend = analysis_ps.get<bool>("ExcludeWeekend");

  auto params = BLSParamSpace(pt.get_child("Strategy"));

  std::vector<std::string> headers{"Id"};
  auto param_titles = MakeParamTitleCSV(params.at(0));
  auto res_headers = OverallTradesResults<Qty_t>::CSV_headers;
  headers.insert(headers.end(), param_titles.begin(), param_titles.end());
  headers.insert(headers.end(), res_headers.begin(), res_headers.end());
  auto head = "\"" + boost::algorithm::join(headers, "\",\"") + "\"";

  // number of windows
  auto first_start = start.sec();
  auto last_start = end.sec() - window_sec;
  auto num_windows_signed = (last_start - first_start) / step_sec + 1;
  assert(num_windows_signed > 0);
  size_t num_windows = size_t(num_windows_signed);

  // open the HDF5 file, assume there is not enough memory to keep all trades in
  // memory
  size_t num_instances = 0;

  H5::H5File h5file(infile, H5F_ACC_RDONLY);
  auto attr = h5file.openAttribute("num_instances");
  attr.read(H5::PredType::NATIVE_UINT64, &num_instances);

  // read num_trades and offset datasets
  std::vector<size_t> num_trades(num_instances, 0);
  std::vector<size_t> offsets(num_instances, 0);
  std::vector<size_t> ids(num_instances, 0);

  auto ds_data = h5file.openDataSet("data");
  auto dat_space = ds_data.getSpace();
  hsize_t dims[2];
  dat_space.getSimpleExtentDims(dims);

  constexpr size_t rec_size = 5;
  assert(dims[1] == rec_size);

  // set up buffers for reading trades
  size_t read_buf_size =
      std::min(hsize_t(128 * 1024 * 1024), dims[0]); // at most ~5 GB
  double *buffer = new double[read_buf_size * rec_size];

  {
    auto ds_num_trades = h5file.openDataSet("num_trades");
    auto ds_offset = h5file.openDataSet("offset");
    auto ds_ids = h5file.openDataSet("ids");

    hsize_t dims_1d[1];
    ds_num_trades.getSpace().getSimpleExtentDims(dims_1d);
    assert(dims_1d[0] == num_instances);

    ds_num_trades.read(num_trades.data(), H5::PredType::NATIVE_UINT64);
    ds_offset.read(offsets.data(), H5::PredType::NATIVE_UINT64);
    ds_ids.read(ids.data(), H5::PredType::NATIVE_UINT64);
  }

  // to make it easier to read trades
  offsets.push_back(offsets[num_instances - 1] + num_trades[num_instances - 1]);

  if (num_instances != params.size()) {
    printf("ERROR: Got %lu instances in HDF5 file, but parameter space has %lu "
           "instances\n",
           num_instances, params.size());
    return EXIT_FAILURE;
  }

  // open output file for each window
  std::vector<FILE *> fouts(num_windows);
  std::vector<std::mutex> mutexes(num_windows);

  printf("Processing %lu instances for %lu windows:\n", num_instances,
         num_windows);

  for (size_t i = 0; i < num_windows; ++i) {
    utxx::time_val this_start(utxx::secs(first_start + long(i) * step_sec));
    utxx::time_val this_end = this_start + utxx::secs(window_sec);
    auto start_str = this_start.to_string(utxx::DATE_TIME, '\0', '\0');
    auto end_str = this_end.to_string(utxx::DATE_TIME, '\0', '\0');
    std::string name = "results_" + start_str + "_to_" + end_str + ".csv";

    fouts[i] = fopen(name.c_str(), "w");
    fprintf(fouts[i], "%s\n", head.c_str());

    printf("  %s to %s\n", start_str.c_str(), end_str.c_str());
  }

  // main loop, we process one instance per thread (with all windows done in
  // that thread)
  size_t current_idx = 0;
  auto start_time = std::chrono::high_resolution_clock::now();

  size_t start_idx = 0;
  size_t stop_idx = 0;
  while (stop_idx < num_instances) {
    // read a chunk of the data into the buffer
    // first figure out how far we can read
    start_idx = stop_idx; // we continue after the previous stop
    auto start_itr = offsets.begin() + int64_t(start_idx);
    auto end_itr = std::upper_bound(start_itr, offsets.end(),
                                    offsets[start_idx] + read_buf_size);
    // this instance BEFORE end_itr runs past the buffer, so that one is the
    // first one we WON"T read
    --end_itr;

    auto num_instances_to_read = std::distance(start_itr, end_itr);
    if (num_instances_to_read <= 0) {
      // this happens if the number of trades of the instance at start_itr is
      // larger than our buffer size, we cannot handle this
      throw utxx::runtime_error("Too many trades compared to buffer size");
    }

    // index of first instance we are NOT reading
    stop_idx = start_idx + size_t(num_instances_to_read);
    // printf("start_idx: %lu, stop_idx: %lu\n", start_idx, stop_idx);
    // printf("offset[start_idx] = %lu, offset[stop_idx - 1] = %lu, "
    //        "offset[stop_idx] = %lu, offsets[start_idx] + read_buf_size =
    //        %lu\n", offsets[start_idx], offsets[stop_idx - 1],
    //        offsets[stop_idx], offsets[start_idx] + read_buf_size);

    hsize_t this_dims[2] = {offsets[stop_idx - 1] + num_trades[stop_idx - 1] -
                                offsets[start_idx],
                            rec_size};
    hsize_t h5_offset[2] = {offsets[start_idx], 0};
    // printf("offset: %lu, size: %lu\n", h5_offset[0], this_dims[0]);

    auto file_space = ds_data.getSpace();
    file_space.selectHyperslab(H5S_SELECT_SET, this_dims, h5_offset);
    H5::DataSpace mem_space(2, this_dims);
    ds_data.read(buffer, H5::PredType::NATIVE_DOUBLE, mem_space, file_space);

    current_idx = start_idx;

#pragma omp parallel shared(current_idx)
    {
      while (true) {
        size_t h5_idx;
#pragma omp atomic capture
        {
          h5_idx = current_idx;
          ++current_idx;
        }
        if (h5_idx >= stop_idx)
          break;

        // read trades for this instance
        std::vector<Trade> trades(num_trades[h5_idx]);
        const double *dat =
            buffer + (offsets[h5_idx] - offsets[start_idx]) * rec_size;

        for (size_t i = 0; i < trades.size(); ++i) {
          trades[i].entry = dat[i * rec_size + 0];
          trades[i].exit = dat[i * rec_size + 1];
          trades[i].quantity = Qty_t(dat[i * rec_size + 2]);
          trades[i].entry_time = utxx::secs(dat[i * rec_size + 3]);
          trades[i].exit_time = utxx::secs(dat[i * rec_size + 4]);
          trades[i].high = 0.0;
          trades[i].low = 0.0;
        }

        for (size_t w_idx = 0; w_idx < num_windows; ++w_idx) {
          auto param_values = MakeParamValuesCSV(params.at(ids[h5_idx]));

          // select the trades that OPEN in this window, even if they don't
          // close in this window
          utxx::time_val this_start(
              utxx::secs(first_start + long(w_idx) * step_sec));
          utxx::time_val this_end = this_start + utxx::secs(window_sec);

          // trades are sorted chronologically, so can use
          // std::[lower|upper]_bound
          auto start_itr1 =
              std::lower_bound(trades.begin(), trades.end(), this_start,
                               [](const Trade &trade, utxx::time_val val) {
                                 return trade.entry_time < val;
                               });

          auto end_itr1 =
              std::upper_bound(start_itr1, trades.end(), this_end,
                               [](utxx::time_val val, const Trade &trade) {
                                 return val < trade.entry_time;
                               });

          std::vector<Trade> these_trades(start_itr1, end_itr1);
          auto actual_end_time =
              these_trades.size() > 0
                  ? these_trades[these_trades.size() - 1].exit_time
                  : this_start;
          TradesGroupResults<Qty_t> res(these_trades, cost, exclude_weekend,
                                        this_start, actual_end_time);

          std::string res_values = res.MakeCSVValues();

          {
            const std::lock_guard<std::mutex> lock(mutexes[w_idx]);
            fprintf(fouts[w_idx], "%08lu,%s,%s\n", ids[h5_idx],
                    param_values.c_str(), res_values.c_str());
          }
        }

        if ((h5_idx + 1) % 50 == 0) {
          auto now = std::chrono::high_resolution_clock::now();
          double elapsed_s =
              double(std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now - start_time)
                         .count()) /
              1.0E9;
          double frac_done = double(h5_idx + 1) / double(num_instances);
          double estimate_remaining = elapsed_s / frac_done - elapsed_s;
          fprintf(stderr,
                  "Completed %8lu of %8lu: %.2f%% done, elapsed: %.0f s, "
                  "estimated time remaining: %.0f s\n",
                  h5_idx + 1, num_instances, 100.0 * frac_done, elapsed_s,
                  estimate_remaining);
        }
      }
    }
  }

  printf("All done!\n");

  for (size_t i = 0; i < num_windows; ++i)
    fclose(fouts[i]);

  delete[] buffer;
  h5file.close();

  return 0;
}
