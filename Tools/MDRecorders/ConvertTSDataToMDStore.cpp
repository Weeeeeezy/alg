// This tool converts tick/bar data exported from TradeStation to MD Store
#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"
#include <utxx/time_val.hpp>

#include <boost/algorithm/string.hpp>
#include <cctz/civil_time.h>
#include <cctz/time_zone.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace MAQUETTE;
using namespace QuantSupport;

void ProcessFile(const std::string &md_store_root, bool tick_data,
                 int hrs_offset, const std::string &symbol,
                 const std::string &file) {
  printf("Processing %s starting\n", file.c_str());

  auto path = std::filesystem::path(file);
  if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
    throw std::invalid_argument(
        path.string() + ": File does not exist or it is not a regular file");

  // conver data
  std::string line;
  std::vector<std::string> fs;
  std::ifstream ifs(path);

  utxx::time_val last_time(utxx::secs(0));

  MDStoreWriter<MDRecL1> md_store(
      md_store_root, tick_data ? "TS_hist" : "TS_hist_1min", symbol, "ob_L1");
  MDStoreBase<MDRecL1>::MDStoreRecord rec;

  // check that first line is correct header
  std::getline(ifs, line);
  boost::trim(line);
  if (line !=
      "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Up\",\"Down\"")
    throw std::runtime_error("File '" + file + "' has unexpected header: '" +
                             line + "'");

  cctz::time_zone nyc;
  cctz::load_time_zone("America/New_York", &nyc);

  int line_num = 1;
  while (!ifs.eof()) {
    std::getline(ifs, line);
    line = line.substr(0, line.size() - 1);
    boost::trim(line);
    if (line == "")
      break;

    boost::split(fs, line, boost::is_any_of(","));
    if (fs.size() != 8)
      throw std::runtime_error(path.string() + ": Expected 8 fields but got " +
                               std::to_string(fs.size()));
    // printf("fs: %s,%s,%s,%s,%s\n", fs[0].c_str(), fs[1].c_str(),
    //        fs[2].c_str(), fs[3].c_str(), fs[4].c_str());

    // prase date and time
    uint mon = uint(std::stoul(fs[0].substr(0, 2)));
    uint day = uint(std::stoul(fs[0].substr(3, 2)));
    int year = std::stoi(fs[0].substr(6, 4));
    uint hr = uint(std::stoul(fs[1].substr(0, 2)));
    uint min = uint(std::stoul(fs[1].substr(3, 2)));
    uint sec = tick_data ? uint(std::stoul(fs[1].substr(6, 2))) : 0;

    cctz::civil_second utc;
    auto et = cctz::civil_second(year, mon, day, hr, min, sec);

    if (hrs_offset > -23) {
      et += (3600 * hrs_offset);
      utc = cctz::convert(cctz::convert(et, nyc), cctz::utc_time_zone());
    } else {
      utc = et;
    }
    utxx::time_val time(int(utc.year()), unsigned(utc.month()),
                        unsigned(utc.day()), unsigned(utc.hour()),
                        unsigned(utc.minute()), unsigned(utc.second()), 0u,
                        true);

    // ensure time is monotonic
    if (time < last_time) {
      throw std::invalid_argument("Wrong chronological order at line " +
                                  std::to_string(line_num));
    }

    if (tick_data) {
      // ensure open == high == low == close
      if ((fs[2] != fs[3]) || (fs[2] != fs[4]) || (fs[2] != fs[5]))
        throw std::invalid_argument(
            "Open, high, low, close did not match on line " +
            std::to_string(line_num));
    }

    rec.ts_exch = time;
    rec.ts_recv = time;
    rec.rec.bid = std::stod(fs[5]); // use close
    rec.rec.ask = rec.rec.bid;
    rec.rec.bid_size = 1.0;
    rec.rec.ask_size = 1.0;

    ++line_num;
    last_time = time;

    md_store.Write(rec);
  }

  ifs.close();
}

int main(int argc, char *argv[]) {
  if (argc < 6) {
    printf("Usage: %s <out md store root> <symbol> [TICK|BAR] <offset hrs from "
           "Eastern> <input file> "
           "[<input file>...]\n",
           argv[0]);
    return 1;
  }

  std::string first_file(argv[5]);
  if (first_file[first_file.size() - 1] == '*') {
    printf("Skipping %s\n", first_file.c_str());
    return 0;
  }

  std::string md_store_root(argv[1]);
  std::string symbol(argv[2]);
  std::string type(argv[3]);
  int hrs_offset = std::stoi(std::string(argv[4]));

  bool ticks;
  if (type == "TICK") {
    ticks = true;
  } else if (type == "BAR") {
    ticks = false;
  } else {
    printf("Please specify either TICK or BAR\n");
    return 1;
  }

  for (int i = 5; i < argc; ++i)
    ProcessFile(md_store_root, ticks, hrs_offset, symbol, argv[i]);

  return 0;
}
