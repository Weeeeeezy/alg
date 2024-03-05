//===========================================================================//
//                  "Tools/MDRecorders/VerifyH5MDStore.cpp":                 //
//       Convert market data in MDStore to Ed's format for comparison        //
//===========================================================================//
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <utxx/time_val.hpp>

#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"
#include "QuantSupport/MDStoreH5.hpp"

using namespace MAQUETTE;
using namespace QuantSupport;

template <typename MDRec, typename READER>
std::vector<MDRec> GetMDStoreRecs(READER &reader, utxx::time_val a_start,
                                  utxx::time_val a_end) {
  std::vector<MDRec> recs;

  reader.Read(a_start, [&](const MDRec &rec) {
    // quit reading if we've reached the end time
    if (rec.ts_exch >= a_end)
      return false;

    recs.push_back(rec);

    return true;
  });

  return recs;
}

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

bool eq(double x, double y) {
  if (x == y)
    return true;

  // if both are NaN, consider them equal
  if (isnan(x) && isnan(y))
    return true;

  return false;
}

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

  if (read_l1) {
    using Rec_t = MDStoreH5<MDStoreH5Type::L1>::Rec_t;
    MDStoreReader<MDRecL1> mds(md_store_root, venue, symbol, "ob_L1");
    MDStoreH5<MDStoreH5Type::L1> mh5(md_store_root, venue);

    auto recs = GetMDStoreRecs<Rec_t>(mds, start, end);
    auto recs_h5 = GetMDStoreH5Recs<Rec_t>(mh5, symbol, start, end);

    if (recs.size() != recs_h5.size())
      throw std::runtime_error(
          "Num of recs mismatch: " + std::to_string(recs.size()) + " vs " +
          std::to_string(recs_h5.size()));

    for (size_t i = 0; i < recs.size(); ++i) {
      if ((recs[i].ts_exch != recs_h5[i].ts_exch) ||
          (recs[i].ts_recv != recs_h5[i].ts_recv) ||
          !eq(recs[i].rec.ask, recs_h5[i].rec.ask) ||
          !eq(recs[i].rec.bid, recs_h5[i].rec.bid) ||
          !eq(recs[i].rec.ask_size, recs_h5[i].rec.ask_size) ||
          !eq(recs[i].rec.bid_size, recs_h5[i].rec.bid_size))
        throw std::runtime_error("Recs mismatch at " + std::to_string(i));
    }

    printf("%lu recs match\n", recs.size());
  } else {
    using Rec_t = MDStoreH5<MDStoreH5Type::Trade>::Rec_t;
    MDStoreReader<MDAggression> mds(md_store_root, venue, symbol, "trades");
    MDStoreH5<MDStoreH5Type::Trade> mh5(md_store_root, venue);

    auto recs = GetMDStoreRecs<Rec_t>(mds, start, end);
    auto recs_h5 = GetMDStoreH5Recs<Rec_t>(mh5, symbol, start, end);

    if (recs.size() != recs_h5.size())
      throw std::runtime_error(
          "Num of recs mismatch: " + std::to_string(recs.size()) + " vs " +
          std::to_string(recs_h5.size()));

    for (size_t i = 0; i < recs.size(); ++i) {
      if ((recs[i].ts_exch != recs_h5[i].ts_exch) ||
          (recs[i].ts_exch != recs_h5[i].ts_exch) ||
          !eq(recs[i].rec.m_totQty, recs_h5[i].rec.m_totQty) ||
          !eq(recs[i].rec.m_avgPx, recs_h5[i].rec.m_avgPx) ||
          (recs[i].rec.m_bidAggr != recs_h5[i].rec.m_bidAggr))
        throw std::runtime_error("Recs mismatch at " + std::to_string(i));
    }

    // if (memcmp(recs.data(), recs_h5.data(), sizeof(Rec_t) * recs.size()) !=
    // 0) {
    //   FILE *f = fopen("recs.out", "w");
    //   FILE *f5 = fopen("recs_h5.out", "w");

    //   for (size_t i = 0; i < recs.size(); ++i)
    //     fprintf(f, "%s  %s  %24.15f  %24.15f  %c\n",
    //             recs[i].ts_exch.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
    //             recs[i].ts_recv.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
    //             recs[i].rec.m_totQty, recs[i].rec.m_avgPx,
    //             recs[i].rec.m_bidAggr ? 'B' : 'S');

    //   for (size_t i = 0; i < recs.size(); ++i)
    //     fprintf(f5, "%s  %s  %24.15f  %24.15f  %c\n",
    //             recs_h5[i].ts_exch.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
    //             recs_h5[i].ts_recv.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
    //             recs_h5[i].rec.m_totQty, recs_h5[i].rec.m_avgPx,
    //             recs_h5[i].rec.m_bidAggr ? 'B' : 'S');

    //   fclose(f);
    //   fclose(f5);

    //   throw std::runtime_error("Recs mismatch");
    // }

    printf("%lu recs match\n", recs.size());
  }

  return 0;
}
