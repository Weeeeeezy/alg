//===========================================================================//
//               "Tools/MDRecorders/VerifyH5AndRemoveDat.cpp":               //
//                  Some tools to print MDStore market data                  //
//===========================================================================//
#include "QuantSupport/MDStoreH5.hpp"
#include <algorithm>
#include <exception>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <H5Cpp.h>
#include <utxx/time_val.hpp>

#ifdef __clang__
#pragma  clang diagnostic push
#pragma  clang diagnostic ignored "-Wformat-nonliteral"
#endif
using namespace MAQUETTE;
using namespace QuantSupport;

template <typename MDRec, typename READER>
inline std::vector<MDRec> GetMDStoreRecs
  (READER &reader, utxx::time_val a_start, utxx::time_val a_end)
{
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
inline std::vector<MDRec> GetMDStoreH5Recs
(
  READER            &reader,
  std::string const &a_symbol,
  utxx::time_val    a_start,
  utxx::time_val    a_end
)
{
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

inline utxx::time_val GetDate(const char *str) {
  int date = std::stoi(str);

  int year = date / 10000;
  int mon = (date - 10000 * year) / 100;
  int day = (date - 10000 * year - 100 * mon);

  return utxx::time_val(year, unsigned(mon), unsigned(day));
}

inline bool eq(double x, double y) {
  if (x == y)
    return true;

  // if both are NaN, consider them equal
  if (isnan(x) && isnan(y))
    return true;

  return false;
}

bool VerifyRecs(const std::string &md_store_root, const std::string &venue,
                const std::string &symbol) {
  utxx::time_val start(2000, 1, 1);
  // 100 years later, can't specify any years directly after 2038 because of
  // Y2k38 problem
  auto end = start + utxx::secs(100.0 * 365.25 * 24 * 3600);

  try {
    {
      using Rec_t = MDStoreH5<MDStoreH5Type::L1>::Rec_t;
      MDStoreReader<MDRecL1> mds(md_store_root, venue, symbol, "ob_L1");
      MDStoreH5<MDStoreH5Type::L1> mh5(md_store_root, venue);

      auto recs = GetMDStoreRecs<Rec_t>(mds, start, end);
      auto recs_h5 = GetMDStoreH5Recs<Rec_t>(mh5, symbol, start, end);

      if (recs.size() != recs_h5.size())
        throw std::runtime_error(
            "Num of L1 recs mismatch: " + std::to_string(recs.size()) + " vs " +
            std::to_string(recs_h5.size()));

      for (size_t i = 0; i < recs.size(); ++i) {
        if ((recs[i].ts_exch != recs_h5[i].ts_exch) ||
            (recs[i].ts_recv != recs_h5[i].ts_recv) ||
            !eq(recs[i].rec.ask, recs_h5[i].rec.ask) ||
            !eq(recs[i].rec.bid, recs_h5[i].rec.bid) ||
            !eq(recs[i].rec.ask_size, recs_h5[i].rec.ask_size) ||
            !eq(recs[i].rec.bid_size, recs_h5[i].rec.bid_size))
          throw std::runtime_error("L1 recs mismatch at " + std::to_string(i));
      }

      printf("%10lu L1 recs OK, ", recs.size());
    }

    {
      using Rec_t = MDStoreH5<MDStoreH5Type::Trade>::Rec_t;
      MDStoreReader<MDAggression> mds(md_store_root, venue, symbol, "trades");
      MDStoreH5<MDStoreH5Type::Trade> mh5(md_store_root, venue);

      auto recs = GetMDStoreRecs<Rec_t>(mds, start, end);
      auto recs_h5 = GetMDStoreH5Recs<Rec_t>(mh5, symbol, start, end);

      if (recs.size() != recs_h5.size())
        throw std::runtime_error(
            "Num of trade recs mismatch: " + std::to_string(recs.size()) +
            " vs " + std::to_string(recs_h5.size()));

      for (size_t i = 0; i < recs.size(); ++i) {
        if ((recs[i].ts_exch != recs_h5[i].ts_exch) ||
            (recs[i].ts_exch != recs_h5[i].ts_exch) ||
            !eq(recs[i].rec.m_totQty, recs_h5[i].rec.m_totQty) ||
            !eq(recs[i].rec.m_avgPx, recs_h5[i].rec.m_avgPx) ||
            (recs[i].rec.m_bidAggr != recs_h5[i].rec.m_bidAggr))
          throw std::runtime_error("Trade recs mismatch at " +
                                   std::to_string(i));
      }

      printf("%10lu trade recs OK", recs.size());
    }
  } catch (std::exception &ex) {
    printf("ERROR: %s", ex.what());
    return false;
  }

  return true;
}

template <typename T>
inline std::string to_str(const char *fmt, T val) {
  char buf[32];
  sprintf(buf, fmt, val);
  return std::string(buf);
}

inline herr_t get_grp(hid_t, const char *name, void *dat) {
  auto names = static_cast<std::set<std::string> *>(dat);
  names->insert(name);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Usage: %s <md store root> <venue> <from YYYYMMDD> <to YYYYMMDD>\n",
           argv[0]);
    return 1;
  }

  std::string md_store_root(argv[1]);
  std::string venue(argv[2]);
  auto from = GetDate(argv[3]);
  auto to = GetDate(argv[4]);

  int num_days = int((to.seconds() - from.seconds()) / (24.0 * 3600.0)) + 1;
  auto root = std::filesystem::path(md_store_root) / venue;
  auto root_h5 = std::filesystem::path(md_store_root) / (venue + "_h5");

  for (int n = 0; n < num_days; ++n) {
    auto date = from + utxx::secs(n * 24 * 3600);

    int yr;
    unsigned mon, day;
    std::tie(yr, mon, day) = date.to_ymd();

    printf("\n\nProcessing %04i-%02u-%02u\n", yr, mon, day);

    auto ys = to_str("%04i", yr);
    auto ms = to_str("%02u", mon);
    auto ds = to_str("%02u", day);

    char buf[128];
    sprintf(buf, "%s_%04i-%02u-%02u", venue.c_str(), yr, mon, day);
    auto tmp_root = std::filesystem::path(buf);
    auto tmp_dir = tmp_root / venue;
    auto tmp_dir_h5 = tmp_root / (venue + "_h5");

    if (std::filesystem::exists(tmp_dir)) {
      printf("%s already exists\n", tmp_dir.c_str());
      continue;
    }

    // make symlinks of H5 files
    std::string l1f = "L1_" + ys + ms + ds + ".h5";
    std::string trf = "trade_" + ys + ms + ds + ".h5";

    auto l1_target = std::filesystem::absolute(root_h5 / ys / l1f);
    if (!std::filesystem::exists(l1_target)) {
      printf("No H5 file for %s %04i-%02u-%02u (%s)\n", venue.c_str(), yr, mon,
             day, l1_target.c_str());
      continue;
    }

    std::filesystem::create_directories(tmp_dir_h5 / ys);

    auto l1_link = tmp_dir_h5 / ys / l1f;
    auto tr_link = tmp_dir_h5 / ys / trf;
    std::filesystem::create_symlink(l1_target, l1_link);
    std::filesystem::create_symlink(
        std::filesystem::absolute(root_h5 / ys / trf), tr_link);

    // read symbols from L1 file
    std::set<std::string> symbols;

    {
      H5::H5File h5f(l1_link.string(), H5F_ACC_RDONLY);
      h5f.iterateElems("/", nullptr, get_grp, &symbols);
    }

    {
      H5::H5File h5f(tr_link.string(), H5F_ACC_RDONLY);
      h5f.iterateElems("/", nullptr, get_grp, &symbols);
    }

    printf("Got %lu symbols\n", symbols.size());
    for (auto &s : symbols) {
      auto dat_dir = std::filesystem::absolute(root / s / ys / ms / ds);
      auto target_parent = tmp_dir / s / ys / ms;
      std::filesystem::create_directories(target_parent);
      std::filesystem::create_directory_symlink(dat_dir, target_parent / ds);

      printf("%20s: ", s.c_str());

      if (VerifyRecs(tmp_root.string(), venue, s)) {
        printf(", deleting %s\n", dat_dir.c_str());
        std::filesystem::remove_all(dat_dir);
      } else {
        printf(", NEED TO CHECK %s\n", dat_dir.c_str());
      }
    }

    std::filesystem::remove_all(tmp_root);
  }

  return 0;
}
#ifdef __clang__
#pragma  clang diagnostic pop
#endif
