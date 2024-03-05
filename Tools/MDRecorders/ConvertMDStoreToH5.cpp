//===========================================================================//
//                 "Tools/MDRecorders/ConvertMDStoreToH5.cpp":               //
//                   Some tools to print MDStore market data                 //
//===========================================================================//
#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include <utxx/time_val.hpp>

#include "QuantSupport/MDStoreH5.hpp"

using namespace MAQUETTE;
using namespace QuantSupport;

utxx::time_val GetDate(const char *str) {
  int date = std::stoi(str);

  int year = date / 10000;
  int mon = (date - 10000 * year) / 100;
  int day = (date - 10000 * year - 100 * mon);

  return utxx::time_val(year, unsigned(mon), unsigned(day));
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

  // get list of symbols
  std::vector<std::string> symbols;
  auto inp_dir = std::filesystem::path(md_store_root) / venue;
  for (const auto &e : std::filesystem::directory_iterator(inp_dir)) {
    if (e.is_directory()) {
      auto sym = e.path().filename().string();
      if (strncmp(sym.c_str(), "20", 2) != 0) // NB: this will break in 2100s
        symbols.push_back(sym);
    }
  }

  std::sort(symbols.begin(), symbols.end());

  int num_days = int((to.seconds() - from.seconds()) / (24.0 * 3600.0)) + 1;

//#pragma omp parallel for
  for (int n = 0; n < num_days; ++n) {
    auto day = from + utxx::secs(n * 24 * 3600);

    {
      MDStoreH5_L1 mdh5(md_store_root, venue);
      mdh5.ConvertMDStoreFiles(day, symbols);
    }

    {
      MDStoreH5_Trade mdh5(md_store_root, venue);
      mdh5.ConvertMDStoreFiles(day, symbols);
    }
  }

  return 0;
}
