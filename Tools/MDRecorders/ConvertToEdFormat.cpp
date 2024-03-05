//===========================================================================//
//                 "Tools/MDRecorders/ConvertToEdFormat.cpp":                //
//       Convert market data in MDStore to Ed's format for comparison        //
//===========================================================================//
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <utxx/time_val.hpp>

#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

using namespace MAQUETTE;
using namespace QuantSupport;

int main(int argc, char *argv[]) {
  if (argc != 7) {
    printf("Usage: %s [TRADES|L1] <md store root> <venue> <symbol> <start> "
           "<end>\n",
           argv[0]);
    printf("  Specify <start> and <end> as: YYYY-MM-DD_HH:MM:SS\n");
    return 1;
  }

  bool read_l1 = false;
  if (std::string(argv[1]) == "TRADES")
    read_l1 = false;
  else if (std::string(argv[1]) == "L1")
    read_l1 = true;
  else
    throw std::invalid_argument("Need to specify TRADES or L1");

  std::string md_store_root(argv[2]);
  std::string venue(argv[3]);
  std::string symbol(argv[4]);

  std::tm start_tm, end_tm;
  std::istringstream start_iss(argv[5]);
  start_iss >> std::get_time(&start_tm, "%Y-%m-%d_%H:%M:%S");
  std::istringstream end_iss(argv[6]);
  end_iss >> std::get_time(&end_tm, "%Y-%m-%d_%H:%M:%S");

  auto tm_to_time_val_utc = [](std::tm tm) {
    return utxx::time_val(tm.tm_year + 1900, unsigned(tm.tm_mon + 1),
                          unsigned(tm.tm_mday), unsigned(tm.tm_hour),
                          unsigned(tm.tm_min), unsigned(tm.tm_sec), true);
  };

  auto start = tm_to_time_val_utc(start_tm);
  auto end = tm_to_time_val_utc(end_tm);

  printf("Start: %s\n", start.to_string(utxx::DATE_TIME_WITH_MSEC).c_str());
  printf("  End: %s\n", end.to_string(utxx::DATE_TIME_WITH_MSEC).c_str());

  // get start of the day
  start_tm.tm_hour = 0;
  start_tm.tm_min = 0;
  start_tm.tm_sec = 0;
  auto start_of_day = tm_to_time_val_utc(start_tm);

  if (read_l1) {
    using Reader = MDStoreReader<MDRecL1>;
    Reader mds(md_store_root, venue, symbol, "ob_L1");
    Reader::MDStoreRecord last_rec;

    mds.Read(start, [&](const Reader::MDStoreRecord &rec) {
      auto time_exch = (rec.ts_exch - start_of_day).nanoseconds() / 100L;
      auto time_recv = (rec.ts_recv - start_of_day).nanoseconds() / 100L;

      std::cout << time_exch << "\t" << time_recv << "\t";
      // std::cout << rec.time.to_string(utxx::DATE_TIME_WITH_NSEC) << "\t";
      rec.rec.bid == last_rec.rec.bid ? std::cout << "s"
                                      : std::cout << rec.rec.bid;
      std::cout << "\t";
      rec.rec.ask == last_rec.rec.ask ? std::cout << "s"
                                      : std::cout << rec.rec.ask;
      std::cout << "\t";
      rec.rec.bid_size == last_rec.rec.bid_size ? std::cout << "s"
                                                : std::cout << rec.rec.bid_size;
      std::cout << "\t";
      rec.rec.ask_size == last_rec.rec.ask_size ? std::cout << "s"
                                                : std::cout << rec.rec.ask_size;
      std::cout << std::endl;
      last_rec = rec;

      return (rec.ts_recv <= end); // keep going if we're not at the end yet
    });
  } else {
    using Reader = MDStoreReader<MDAggression>;
    Reader mds(md_store_root, venue, symbol, "trades");

    mds.Read(start, [&](const Reader::MDStoreRecord &rec) {
      auto time_exch = (rec.ts_exch - start_of_day).nanoseconds() / 100L;
      auto time_recv = (rec.ts_recv - start_of_day).nanoseconds() / 100L;

      std::cout << time_exch << "\t" << time_recv << "\t" << rec.rec.m_avgPx
                << "\t" << rec.rec.m_totQty << "\t"
                << (rec.rec.m_bidAggr ? "p" : "g") << std::endl;

      return (rec.ts_recv <= end); // keep going if we're not at the end yet
    });
  }

  return 0;
}
