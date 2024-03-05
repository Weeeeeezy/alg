//===========================================================================//
//                   "Tools/MDRecorders/PrintMDStore.cpp":                   //
//                  Some tools to print MDStore market data                  //
//===========================================================================//
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <utxx/time_val.hpp>

#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

#ifdef __clang__
#pragma  clang diagnostic push
#pragma  clang diagnostic ignored "-Wformat-nonliteral"
#endif
using namespace MAQUETTE;
using namespace QuantSupport;

int main(int argc, char *argv[]) {
  if (argc != 8) {
    printf("Usage: %s <md store root> <venue> <symbol> [L1|LAST] <num decimal "
           "places> <start> <end>\n",
           argv[0]);
    printf("  Specify <start> and <end> as: YYYY-MM-DD_HH:MM:SS\n");
    return 1;
  }

  std::string md_store_root(argv[1]);
  std::string venue(argv[2]);
  std::string symbol(argv[3]);
  std::string mode(argv[4]);

  if ((mode != "L1") && (mode != "LAST"))
    throw std::invalid_argument("Mode must be L1 or LAST but got " + mode);

  std::tm start_tm, end_tm;
  std::istringstream start_iss(argv[6]);
  start_iss >> std::get_time(&start_tm, "%Y-%m-%d_%H:%M:%S");
  std::istringstream end_iss(argv[7]);
  end_iss >> std::get_time(&end_tm, "%Y-%m-%d_%H:%M:%S");

  auto tm_to_time_val_utc = [](std::tm tm) {
    return utxx::time_val(tm.tm_year + 1900, unsigned(tm.tm_mon + 1),
                          unsigned(tm.tm_mday), unsigned(tm.tm_hour),
                          unsigned(tm.tm_min), unsigned(tm.tm_sec), true);
  };

  auto start = tm_to_time_val_utc(start_tm);
  auto end = tm_to_time_val_utc(end_tm);

  printf("# Start: %s\n", start.to_string(utxx::DATE_TIME_WITH_USEC).c_str());
  printf("#   End: %s\n", end.to_string(utxx::DATE_TIME_WITH_USEC).c_str());

  if (mode == "L1") {
    printf("# %24s  %10s  %12s  %10s  %10s  %12s  %12s\n", "Date/time",
           "Spread", "Spread [bp]", "Bid", "Ask", "Bid size", "Ask size");

    using Reader = MDStoreReader<MDRecL1>;
    Reader mds(md_store_root, venue, symbol, "ob_L1");
    Reader::MDStoreRecord last_rec;

    char fmt[128];
    sprintf(fmt,
            "%%26s  %%10.%sf  %%12.3f  %%10.%sf  %%10.%sf  %%12.0f  %%12.0f\n",
            argv[5], argv[5], argv[5]);

    int num_duplicate = 0;

    mds.Read(start, [&](const Reader::MDStoreRecord &rec) {
      // quit reading if we've reached the end time
      if (rec.ts_recv > end)
        return false;

      if ((rec.rec.bid == last_rec.rec.bid) &&
          (rec.rec.ask == last_rec.rec.ask) &&
          (rec.rec.bid_size == last_rec.rec.bid_size) &&
          (rec.rec.ask_size == last_rec.rec.ask_size))
        ++num_duplicate;

      auto time = rec.ts_recv; // - utxx::secs(4 * 3600);
      auto time_str = time.to_string(utxx::DATE_TIME_WITH_USEC);

      auto spread = rec.rec.ask - rec.rec.bid;
      auto spread_bp = fabs(rec.rec.bid / rec.rec.ask - 1.0) * 1e4;
      printf(fmt, time_str.c_str(), spread, spread_bp, rec.rec.bid, rec.rec.ask,
             rec.rec.bid_size, rec.rec.ask_size);

      last_rec = rec;
      return true;
    });

    printf("# num duplicates: %i\n", num_duplicate);
    std::cerr << "Num duplicates: " << num_duplicate << std::endl;
  } else {
    printf("# %24s  %10s  %12s  %s\n", "Date/time", "Price", "Size",
           "Aggressor");

    using Reader = MDStoreReader<MDAggression>;
    Reader mds(md_store_root, venue, symbol, "trades");

    char fmt[128];
    sprintf(fmt, "%%26s  %%10.%sf  %%12.0f  %%s\n", argv[5]);

    mds.Read(start, [&](const Reader::MDStoreRecord &rec) {
      // quit reading if we've reached the end time
      if (rec.ts_recv > end)
        return false;

      auto time = rec.ts_recv; // - utxx::secs(4 * 3600);
      auto time_str = time.to_string(utxx::DATE_TIME_WITH_USEC);

      printf(fmt, time_str.c_str(), rec.rec.m_avgPx, rec.rec.m_totQty,
             rec.rec.m_bidAggr ? "P" : "G");

      return true;
    });
  }

  return 0;
}
#ifdef __clang__
#pragma  clang diagnostic pop
#endif
