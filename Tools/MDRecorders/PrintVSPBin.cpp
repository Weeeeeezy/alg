//===========================================================================//
//                   "Tools/MDRecorders/PrintVSPBin.cpp":                    //
//                          Print VSP binary data                            //
//===========================================================================//
#include <iomanip>
#include <iostream>
#include <math.h>
#include <sstream>
#include <stdexcept>

#include <utxx/time_val.hpp>

#include "TradingStrats/BLS/Common/BLSParallelEngine.hpp"
#include "QuantSupport/MDStore.hpp"

using namespace MAQUETTE;
using namespace BLS;
using namespace QuantSupport;

using MDRec = typename MDStoreReader<MDRecL1>::MDStoreRecord;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <file>\n", argv[0]);
    return 1;
  }

  auto recs = GetL1VSPRecs<MDRecL1>(argv[1]);

  auto start = recs[0].ts_recv - utxx::secs(4 * 3600);
  auto end = recs[recs.size() - 1].ts_recv - utxx::secs(4 * 3600);

  printf("# Start: %s\n", start.to_string(utxx::DATE_TIME_WITH_USEC).c_str());
  printf("#   End: %s\n", end.to_string(utxx::DATE_TIME_WITH_USEC).c_str());
  printf("# %24s  %10s  %12s  %10s  %10s  %12s  %12s\n", "Date/time", "Spread",
         "Spread [bp]", "Bid", "Ask", "Bid size", "Ask size");

  int num_duplicate = 0;
  MDRec last_rec;

  for (const auto &rec : recs) {
    if ((rec.rec.bid == last_rec.rec.bid) &&
        (rec.rec.ask == last_rec.rec.ask) &&
        (rec.rec.bid_size == last_rec.rec.bid_size) &&
        (rec.rec.ask_size == last_rec.rec.ask_size))
      ++num_duplicate;

    auto time = rec.ts_recv - utxx::secs(4 * 3600);
    auto time_str = time.to_string(utxx::DATE_TIME_WITH_USEC);

    auto spread = rec.rec.ask - rec.rec.bid;
    auto spread_bp = fabs(rec.rec.bid / rec.rec.ask - 1.0) * 1e4;
    printf("%26s  %10.3f  %12.3f  %10.3f  %10.3f  %12.0f  %12.0f\n",
           time_str.c_str(), spread, spread_bp, rec.rec.bid, rec.rec.ask,
           rec.rec.bid_size, rec.rec.ask_size);

    last_rec = rec;
  }

  printf("# num duplicates: %i\n", num_duplicate);
  std::cerr << "Num duplicates: " << num_duplicate << std::endl;

  return 0;
}
