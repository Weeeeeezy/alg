#pragma once

#include <filesystem>

#include "MDStore.hpp"
#include "MDStoreH5.hpp"

namespace MAQUETTE {
namespace QuantSupport {

template <typename REC>
using MDRec_t = typename MDStoreBase<REC>::MDStoreRecord;

template <typename REC>
std::vector<MDRec_t<REC>>
GetMDRecs(const std::string &a_md_store_root, const std::string &a_venue,
          const std::string &a_symbol, utxx::time_val a_start,
          utxx::time_val a_end) {
  static constexpr bool IsL1 = std::is_same_v<REC, MDRecL1>;

  std::vector<MDRec_t<REC>> recs;
  auto read_callback = [&](const MDRec_t<REC> &rec) {
    // quit reading if we've reached the end time
    if (rec.ts_exch > a_end)
      return false;

    recs.push_back(rec);
    return true;
  };

  // determine if we have HDF5 or plain MDStore
  auto dir = std::filesystem::path(a_md_store_root) / (a_venue + "_h5");
  if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
    // we have HDF5
    if constexpr (IsL1) {
      MDStoreH5_L1 mdh5(a_md_store_root, a_venue);
      mdh5.Read(a_symbol, a_start, read_callback);
    } else {
      MDStoreH5_Trade mdh5(a_md_store_root, a_venue);
      mdh5.Read(a_symbol, a_start, read_callback);
    }
  } else {
    // using plain MDStore
    MDStoreReader<REC> mds(a_md_store_root, a_venue, a_symbol,
                           IsL1 ? "ob_L1" : "trades");
    mds.Read(a_start, read_callback);
  }

  return recs;
}

template <typename REC>
std::vector<MDRec_t<REC>> GetL1VSPRecs(const std::string a_file) {
  static_assert(std::is_same_v<REC, MDRecL1>,
                "Only L1 records are supported for VSP");
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

  std::filesystem::path dat_file(a_file);

  if (!std::filesystem::exists(dat_file) ||
      !std::filesystem::is_regular_file(dat_file)) {
    throw utxx::runtime_error("'" + a_file +
                              "' does not exist or is not a regular file");
  }

  auto fsize = std::filesystem::file_size(dat_file);
  size_t num_recs = fsize / sizeof(DatRecord);
  assert(num_recs * sizeof(DatRecord) == fsize);

  // read the historical data
  std::vector<DatRecord> dat_recs(num_recs);
  auto f = fopen(dat_file.c_str(), "rb");
  if (fread(dat_recs.data(), sizeof(DatRecord), num_recs, f) != num_recs) {
    throw std::invalid_argument(
        "Got a different number of records than expected");
  }
  fclose(f);

  // convert DatRecord to MDRec
  std::vector<MDRec_t<REC>> recs(dat_recs.size());
  for (size_t i = 0; i < recs.size(); ++i) {
    auto time = utxx::msecs(size_t(dat_recs[i].time * 1000.0));

    recs[i].ts_exch = time;
    recs[i].ts_recv = time;
    recs[i].rec.bid = double(dat_recs[i].bid);
    recs[i].rec.ask = double(dat_recs[i].ask);

    // since the bid and ask sizes are in 100,000, they can be 0 due to
    // rounding, so make it at least 1 * 100,000
    recs[i].rec.bid_size =
        std::max(1.0, double(dat_recs[i].bid_size)) * 100000.0;
    recs[i].rec.ask_size =
        std::max(1.0, double(dat_recs[i].ask_size)) * 100000.0;
  }

  return recs;
}

} // namespace QuantSupport
} // namespace MAQUETTE
