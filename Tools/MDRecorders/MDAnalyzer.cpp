//===========================================================================//
//                    "Tools/MDRecorders/MDAnalyzer.cpp":                    //
//                 Some tools to analyze MDStore market data                 //
//===========================================================================//
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <utxx/time_val.hpp>

#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

using namespace MAQUETTE;
using namespace QuantSupport;

int main(int argc, char *argv[]) {
  if (argc != 6) {
    printf("Usage: %s <md store root> <venue> <symbol> <start> <end>\n",
           argv[0]);
    printf("  Specify <start> and <end> as: YYYY-MM-DD_HH:MM:SS\n");
    return 1;
  }

  std::string md_store_root(argv[1]);
  std::string venue(argv[2]);
  std::string symbol(argv[3]);

  std::tm start_tm, end_tm;
  std::istringstream start_iss(argv[4]);
  start_iss >> std::get_time(&start_tm, "%Y-%m-%d_%H:%M:%S");
  std::istringstream end_iss(argv[5]);
  end_iss >> std::get_time(&end_tm, "%Y-%m-%d_%H:%M:%S");

  auto tm_to_time_val_utc = [](std::tm tm) {
    return utxx::time_val(tm.tm_year + 1900, unsigned(tm.tm_mon + 1),
                          unsigned(tm.tm_mday), unsigned(tm.tm_hour),
                          unsigned(tm.tm_min), unsigned(tm.tm_sec), true);
  };

  auto start = tm_to_time_val_utc(start_tm);
  auto end = tm_to_time_val_utc(end_tm);

  printf("# Start: %s\n", start.to_string(utxx::DATE_TIME_WITH_MSEC).c_str());
  printf("#   End: %s\n", end.to_string(utxx::DATE_TIME_WITH_MSEC).c_str());
  printf("# %18s  %12s  %10s  %16s  %16s  %16s  %16s\n", "Time [frac of day]",
         "Latency [us]", "Spread", "Bid", "Ask", "Bid size", "Ask size");

  using Reader = MDStoreReader<MDRecL1>;
  Reader mds(md_store_root, venue, symbol, "ob_L1");
  Reader::MDStoreRecord last_rec;

  int num_duplicate = 0;

  mds.Read(start, [&](const Reader::MDStoreRecord &rec) {
    // quit reading if we've reached the end time
    if (rec.ts_recv > end)
      return false;

    if ((rec.rec.bid == last_rec.rec.bid) &&
        (rec.rec.ask == last_rec.rec.ask) &&
        (rec.rec.bid_size == last_rec.rec.bid_size) &&
        (rec.rec.ask_size == last_rec.rec.ask_size)) {
      ++num_duplicate;
    } else {
      auto recv_time = rec.ts_recv;
      auto latency = (recv_time - rec.ts_exch).microseconds();
      double time = double(recv_time.nanoseconds()) / (24.0 * 3600.0 * 1.0E9);

      auto spread = rec.rec.ask - rec.rec.bid;
      printf("%20.8f  %12li  %10.5f  %16.5f  %16.5f  %16.5f  %16.5f\n", time,
             latency, spread, rec.rec.bid, rec.rec.ask, rec.rec.bid_size,
             rec.rec.ask_size);
    }

    last_rec = rec;
    return true;
  });

  printf("# num duplicates: %i\n", num_duplicate);
  std::cerr << "Num duplicates: " << num_duplicate << std::endl;

  return 0;
}
