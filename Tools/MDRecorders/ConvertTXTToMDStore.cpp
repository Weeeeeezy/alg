
// This tool converts *.txt data recorded by Ed to the MDStore format

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <string>
#include <utxx/time_val.hpp>

#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

using namespace MAQUETTE;
using namespace QuantSupport;

void ProcessFile(const std::string &md_store_root, const std::string &file) {
  printf("Processing %s starting\n", file.c_str());

  // decode file name: [Venue]_[0|1]_[Symbol]_YYYYMMDD.txt
  auto path = std::filesystem::path(file);
  if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
    throw std::invalid_argument(
        path.string() + ": File does not exist or it is not a regular file");

  if (path.extension().string() != ".txt")
    throw std::invalid_argument(path.string() +
                                ": Can only process .txt files");

  std::vector<std::string> vals;
  auto stem = path.stem().string();
  boost::split(vals, stem, boost::is_any_of("_"));

  auto venue = vals[0];

  // vals[0] must be "0" (for L1 data) or "1" (for trades data)
  bool L1_data;
  if (vals[1] == "0")
    L1_data = true;
  else if (vals[1] == "1")
    L1_data = false;
  else
    throw std::invalid_argument(
        path.string() +
        ": Cannot determine whether input file contains L1 data or trades");

  bool cme_contract_with_expiration_date =
      ((venue == "CME") && (vals.size() == 5));
  auto symbol = vals[2] + (cme_contract_with_expiration_date ? vals[3] : "");

  auto date = cme_contract_with_expiration_date ? vals[4] : vals[3];
  // date contains the date in the format YYYYMMDD
  if (date.size() != 8)
    throw std::invalid_argument(path.string() + ": Invalid date " + vals[3]);

  int year = std::stoi(date.substr(0, 4));
  int month = std::stoi(date.substr(4, 2));
  int day = std::stoi(date.substr(6, 2));

  // the time stamp in the file is already in UTC
  auto start_time = utxx::time_val::universal_time(year, month, day, 0, 0, 0);

  // conver data
  std::string line;
  std::vector<std::string> fs;
  std::ifstream ifs(path);

  if (L1_data) {
    MDStoreWriter<MDRecL1> md_store(md_store_root, venue, symbol, "ob_L1");
    MDStoreBase<MDRecL1>::MDStoreRecord rec, last_rec;

    last_rec.rec.bid = -1.0;
    last_rec.rec.ask = -1.0;
    last_rec.rec.bid_size = -1.0;
    last_rec.rec.ask_size = -1.0;

    while (!ifs.eof()) {
      std::getline(ifs, line);
      boost::trim(line);
      if (line == "")
        break;

      boost::split(fs, line, boost::is_any_of("\t"));
      if (fs.size() != 5) {
        printf("WARNING: [%s] skipping line '%s'", file.c_str(), line.c_str());
        continue;
      }
      // printf("fs: %s,%s,%s,%s,%s\n", fs[0].c_str(), fs[1].c_str(),
      //        fs[2].c_str(), fs[3].c_str(), fs[4].c_str());

      for (size_t i = 1; i < 5; ++i) {
        if ((fs[i] == "0") || (fs[i] == "s")) {
          fs[i] = "s";
        }
      }

      // skip records that are all the same as before (includes records that are
      // all 0s, which can happen on weekends and after hours)
      if ((fs[1] == "s") && (fs[2] == "s") && (fs[3] == "s") && (fs[4] == "s"))
        continue;

      utxx::time_val time = start_time + utxx::nsecs(std::stol(fs[0]) * 100L);

      // ensure time is monotonic
      time = std::max(time, last_rec.ts_recv);

      rec.ts_exch = time;
      rec.ts_recv = time;
      rec.rec.bid = fs[1] == "s" ? last_rec.rec.bid : std::stod(fs[1]);
      rec.rec.ask = fs[2] == "s" ? last_rec.rec.ask : std::stod(fs[2]);
      rec.rec.bid_size =
          fs[3] == "s" ? last_rec.rec.bid_size : std::stod(fs[3]);
      rec.rec.ask_size =
          fs[4] == "s" ? last_rec.rec.ask_size : std::stod(fs[4]);

      last_rec = rec;

      // if any fields of the record are -1, that means we used the
      // uninitialized last_rec, just skip this (but save it as last rec) and
      // wait until we have a full record
      if ((rec.rec.bid == -1.0) || (rec.rec.ask == -1.0) ||
          (rec.rec.bid_size == -1.0) || (rec.rec.ask_size == -1.0))
        continue;

      md_store.Write(rec);
    }
  } else {
    MDStoreWriter<MDAggression> md_store(md_store_root, venue, symbol,
                                         "trades");
    MDStoreBase<MDAggression>::MDStoreRecord rec;
    utxx::time_val last_time(utxx::secs(0));

    while (!ifs.eof()) {
      std::getline(ifs, line);
      boost::trim(line);
      if (line == "")
        break;

      boost::split(fs, line, boost::is_any_of("\t"));
      if (fs.size() != 4) {
        printf("WARNING: [%s] skipping line '%s'", file.c_str(), line.c_str());
        continue;
      }

      utxx::time_val time = start_time + utxx::nsecs(std::stol(fs[0]) * 100L);

      // ensure time is monotonic
      time = std::max(time, last_time);

      rec.ts_exch = time;
      rec.ts_recv = time;
      rec.rec.m_avgPx = std::stod(fs[1]);
      rec.rec.m_totQty = std::stod(fs[2]);
      rec.rec.m_bidAggr = (fs[3] == "p");

      if (rec.rec.m_avgPx == 0.0)
        // there is no data here (probably a Saturday)
        continue;

      md_store.Write(rec);
      last_time = time;
    }
  }

  ifs.close();
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <out md store root> <input .txt file> [<more input "
           "files>]\n",
           argv[0]);
    return 1;
  }

  std::string first_file(argv[2]);
  if (first_file[first_file.size() - 1] == '*') {
    printf("Skipping %s\n", first_file.c_str());
    return 0;
  }

  std::string md_store_root(argv[1]);

#pragma omp parallel for
  for (int i = 2; i < argc; ++i)
    ProcessFile(md_store_root, argv[i]);

  return 0;
}
