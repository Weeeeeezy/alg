//===========================================================================//
//                   "Tools/MDRecorders/PrintMDStoreH5.cpp":                 //
//       Convert market data in MDStore to Ed's format for comparison        //
//===========================================================================//
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <utxx/time_val.hpp>

#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"
#include "QuantSupport/MDStoreH5.hpp"

#ifdef __clang__
#pragma  clang diagnostic push
#pragma  clang diagnostic ignored "-Wformat-nonliteral"
#endif

using namespace MAQUETTE;
using namespace QuantSupport;

template <typename MDRec, typename READER>
std::vector<MDRec> GetMDStoreH5Recs(READER &reader, const std::string &a_symbol,
                                    utxx::time_val a_start,
                                    utxx::time_val a_end) {
  std::vector<MDRec> recs;

  reader.Read(a_symbol, a_start, [&](const MDRec &rec) {
    // quit reading if we've reached the end time
    if (rec.ts_exch >= a_end)
      return false;

    recs.push_back(rec);

    return true;
  });

  return recs;
}

int main(int argc, char *argv[]) {
  if (argc != 8) {
    printf("Usage: %s [TRADES|L1] <num digits> <md store root> <venue> "
           "<symbol> <start> <end>\n",
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

  const char *num_digits = argv[2];
  std::string md_store_root(argv[3]);
  std::string venue(argv[4]);
  std::string symbol(argv[5]);

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

  printf("Start: %s\n", start.to_string(utxx::DATE_TIME_WITH_MSEC).c_str());
  printf("  End: %s\n", end.to_string(utxx::DATE_TIME_WITH_MSEC).c_str());

  if (read_l1) {
    using Rec_t = MDStoreH5<MDStoreH5Type::L1>::Rec_t;
    MDStoreH5<MDStoreH5Type::L1> mh5(md_store_root, venue);
    auto recs_h5 = GetMDStoreH5Recs<Rec_t>(mh5, symbol, start, end);

    printf("# %24s  %10s  %12s  %10s  %10s  %12s  %12s\n", "Date/time",
           "Spread", "Spread [bp]", "Bid", "Ask", "Bid size", "Ask size");

    char fmt[128];
    sprintf(fmt,
            "%%26s  %%10.%sf  %%12.3f  %%10.%sf  %%10.%sf  %%12.0f  %%12.0f\n",
            num_digits, num_digits, num_digits);

    for (size_t i = 0; i < recs_h5.size(); ++i) {
      const auto &rec = recs_h5[i];
      auto time = rec.ts_recv; // - utxx::secs(4 * 3600);
      auto time_str = time.to_string(utxx::DATE_TIME_WITH_USEC);

      auto spread = rec.rec.ask - rec.rec.bid;
      auto spread_bp = fabs(rec.rec.bid / rec.rec.ask - 1.0) * 1e4;
      printf(fmt, time_str.c_str(), spread, spread_bp, rec.rec.bid, rec.rec.ask,
             rec.rec.bid_size, rec.rec.ask_size);
    }
  } else {
    using Rec_t = MDStoreH5<MDStoreH5Type::Trade>::Rec_t;
    MDStoreH5<MDStoreH5Type::Trade> mh5(md_store_root, venue);
    auto recs_h5 = GetMDStoreH5Recs<Rec_t>(mh5, symbol, start, end);

    printf("# %24s  %10s  %12s  %s\n", "Date/time", "Price", "Size",
           "Aggressor");

    char fmt[128];
    sprintf(fmt, "%%26s  %%10.%sf  %%12.0f  %%s\n", num_digits);

    for (size_t i = 0; i < recs_h5.size(); ++i) {
      const auto &rec = recs_h5[i];
      auto time = rec.ts_recv; // - utxx::secs(4 * 3600);
      auto time_str = time.to_string(utxx::DATE_TIME_WITH_USEC);

      printf(fmt, time_str.c_str(), rec.rec.m_avgPx, rec.rec.m_totQty,
             rec.rec.m_bidAggr ? "P" : "G");
    }
  }

  return 0;
}
#ifdef __clang__
#pragma  clang diagnostic pop
#endif

