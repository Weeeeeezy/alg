// This tool converts tick/bar data exported from TradeStation to VSP Dat
#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

#include <utxx/time_val.hpp>
#include <boost/algorithm/string.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cctz/civil_time.h>
#include <cctz/time_zone.h>

using namespace MAQUETTE;
using namespace QuantSupport;

struct DatRecord {
  double time;
  uint8_t type = 0;
  uint8_t index = 0;
  uint8_t source = 106;   // hotspot
  uint8_t reserved = 255; // for alignment
  float bid, ask;
  uint8_t bid_size, ask_size; // in 100,000
  uint16_t reserved2 = 42;    // alignment
};

int main(int argc, char *argv[]) {
  if (argc != 6) {
    printf("Usage: %s <symbol> [TICK|BAR] <offset hrs from Eastern> <input file> <output file>\n",
           argv[0]);
    return 1;
  }

  std::string symbol(argv[1]);
  std::string type(argv[2]);
  int hrs_offset = std::stoi(std::string(argv[3]));

  bool tick_data;
  if (type == "TICK") {
    tick_data = true;
  } else if (type == "BAR") {
    tick_data = false;
  } else {
    printf("Please specify either TICK or BAR\n");
    return 1;
  }

  std::string file(argv[4]);
  std::string out_file(argv[5]);

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

  // check that first line is correct header
  std::getline(ifs, line);
  boost::trim(line);
  if (line !=
      "\"Date\",\"Time\",\"Open\",\"High\",\"Low\",\"Close\",\"Up\",\"Down\"")
    throw std::runtime_error("File '" + file + "' has unexpected header: '" +
                             line + "'");

  cctz::time_zone nyc;
  cctz::load_time_zone("America/New_York", &nyc);

  DatRecord rec;
  std::vector<DatRecord> dat_recs;

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

    auto et = cctz::civil_second(year, mon, day, hr, min, sec);
    et += (3600 * hrs_offset);
    auto utc = cctz::convert(cctz::convert(et, nyc), cctz::utc_time_zone());
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

    rec.time = time.seconds();
    rec.bid = std::stof(fs[5]); // use close
    rec.ask = rec.bid;
    rec.bid_size = 1.0;
    rec.ask_size = 1.0;

    dat_recs.push_back(rec);

    ++line_num;
    last_time = time;
  }

  ifs.close();

  // write output file
  FILE * fout = fopen(out_file.c_str(), "wb");
  fwrite(dat_recs.data(), sizeof(DatRecord), dat_recs.size(), fout);
  fclose(fout);

  return 0;
}
