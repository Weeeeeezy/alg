//===========================================================================//
//                               "MDStore.hpp":                              //
//   File management for storing different types of recorded market data     //
//===========================================================================//
//
// MDStore provides a unified stream-like interface for storing time-based
// market data. Everything is templated on the market data record type, so the
// format of the market data is not fixed. Market data is organized as follows:
//
// <root>/<Venue>/<symbol>/<year>/<month>/<day>/<prefix>_<hour>.dat
// where <root> is some root path, <Venue> is the name of the Venue where the
// data comes from, <symbol> is the name of the symbol whose data is stored,
// <year> is the 4-digit year, and <month>, <day>, <hour> are all 2-digit
// numbers with leading zeros. <prefix> can be "trades" or "ob" or something
// like that to distinguish different types of data for the same symbol.
//
// All the the times and dates are agnostic to time zones and daylight savings
// time. They are the same times as the ts_exch timestamps provided to the
// strategy in the order book update function by EConnector_MktData.
//

#pragma once

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <string>

#include <H5Cpp.h>
#include <type_traits>
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "H5File.h"
#include "MDStore.hpp"

namespace MAQUETTE {
namespace QuantSupport {

namespace 
{
  inline bool ends_with(const std::string &str, const std::string &suffix)
  {
    if (str.length() > suffix.length())
      return (0 == str.compare(str.length() - suffix.length(), suffix.length(),
                               suffix));
    else
      return false;
  }
} // namespace

enum class MDStoreH5Type { L1, Trade };

template <MDStoreH5Type TYPE> class MDStoreH5 {
public:
  static_assert((TYPE == MDStoreH5Type::L1) || (TYPE == MDStoreH5Type::Trade));
  static constexpr bool IsL1 = (TYPE == MDStoreH5Type::L1);

  using REC = typename std::conditional<IsL1, MDRecL1, MDAggression>::type;
  using Rec_t = typename MDStoreBase<REC>::MDStoreRecord;
  static constexpr uint32_t RecSize = MDStoreBase<REC>::RecSize;

  // This is the callback function called for every record that is being read.
  // If the function returns true, reading will continue. If it returns false,
  // reading will stop.
  using CallbackT = std::function<bool(const Rec_t &)>;

  virtual ~MDStoreH5() {
    // TODO close file
  }

private:
  struct MDStoreH5File {
    struct Header {
      std::string venue;
      std::string rec_type;
      // utxx::time_val file_start_time;
      // utxx::time_val next_file_start_time;
    };

    static std::string ReadStrAttr(const H5::H5File &f,
                                   const std::string &name) {
      std::string res = "";
      if (f.attrExists(name)) {
        auto attr = f.openAttribute(name);
        attr.read(attr.getStrType(), res);
      }

      return res;
    }

    static void SetStrAttr(const H5::H5File &f, const std::string &name,
                           const std::string &value) {
      if (f.attrExists(name))
        f.removeAttr(name);

      auto type = H5::StrType(H5::PredType::C_S1, value.length() + 1);
      auto attr = f.createAttribute(name, type, H5::DataSpace(H5S_SCALAR));
      attr.write(type, value);
    }

    // static utxx::time_val ReadTimeAttr(const H5::H5File &f,
    //                                    const std::string &name) {
    //   static_assert(sizeof(utxx::time_val) == sizeof(int64_t));

    //   utxx::time_val res(utxx::secs(0));
    //   if (f.attrExists(name)) {
    //     auto attr = f.openAttribute(name);
    //     attr.read(H5::PredType::NATIVE_INT64, &res);
    //   }

    //   return res;
    // }

    // static void SetTimeAttr(const H5::H5File &f, const std::string &name,
    //                         utxx::time_val value) {
    //   static_assert(sizeof(utxx::time_val) == sizeof(int64_t));

    //   if (f.attrExists(name))
    //     f.removeAttr(name);

    //   auto type = H5::PredType::NATIVE_INT64;
    //   auto attr = f.createAttribute(name, type, H5::DataSpace(H5S_SCALAR));
    //   attr.write(type, &value);
    // }

    static Header GetHeader(const H5::H5File &f) {
      Header head;
      head.venue = ReadStrAttr(f, "venue");
      head.rec_type = ReadStrAttr(f, "rec_type");
      // head.file_start_time = ReadTimeAttr(f, "file_start_time");
      // head.next_file_start_time = ReadTimeAttr(f, "next_file_start_time");

      return head;
    }

    static void SetHeader(const H5::H5File &f, Header head) {
      SetStrAttr(f, "venue", head.venue);
      SetStrAttr(f, "rec_type", head.rec_type);
      // SetTimeAttr(f, "file_start_time", head.file_start_time);
      // SetTimeAttr(f, "next_file_start_time", head.next_file_start_time);
    }

    template <typename T> static H5::DataType GetH5DataType() {
      if (std::is_same_v<T, double>)
        return H5::DataType(H5::PredType::NATIVE_DOUBLE);
      else if (std::is_same_v<T, utxx::time_val>)
        return H5::DataType(H5::PredType::NATIVE_INT64);
      else
        throw std::invalid_argument("Unknown type");
    }

    template <typename T>
    static std::vector<T> GetData(const H5::H5Location &loc,
                                  const std::string &name) {
      static_assert(sizeof(utxx::time_val) == sizeof(int64_t));

      auto dataset = loc.openDataSet(name);
      auto dataSpace = dataset.getSpace();

      hsize_t dim[1];
      if (1 != dataSpace.getSimpleExtentDims(dim))
        throw std::runtime_error(
            "Unexpected number of dimensions in data set " + name);

      auto datatype = GetH5DataType<T>();
      if (!(dataset.getDataType() == datatype))
        throw std::runtime_error("Wrong data type in data set " + name);

      // set size of data vector
      std::vector<T> data(dim[0]);
      dataset.read(data.data(), datatype);
      dataset.close();

      return data;
    }

    template <typename T>
    static void SetData(H5::H5Location &loc, const std::string &name,
                        const std::vector<T> &data) {
      hsize_t dim[1];
      dim[0] = data.size();
      H5::DataSpace dataSpace(1, dim, dim);

      H5::DSetCreatPropList props;

      // no timestamps so we can compute hash of file
      H5Pset_obj_track_times(props.getId(), false);

      props.setDeflate(5);
      hsize_t chunkDim[1];
      chunkDim[0] = 128 * 1024;
      chunkDim[0] = std::min(chunkDim[0], dim[0]);
      props.setChunk(1, chunkDim);

      auto datatype = GetH5DataType<T>();
      auto dataset = loc.createDataSet(name, datatype, dataSpace, props);

      dataset.write(data.data(), datatype);
      dataset.close();
    }
  };

  std::string m_venue;
  const char *m_rec_type = IsL1 ? "L1" : "trade";

  // our root path (including Venue)
  std::filesystem::path m_root_path;

  // all the files that existed in m_root_path / m_venue when the instance was
  // constructed
  std::vector<std::filesystem::path> m_all_files;

  // for efficiency, we read an entire file at once and store its records here
  std::vector<Rec_t> m_recs;

public:
  MDStoreH5(const std::string &a_root_path, const std::string &a_venue)
      : m_venue(a_venue), m_root_path(a_root_path) {
    std::filesystem::create_directories(m_root_path / (m_venue + "_h5"));

    // printf("iterating over %s\n", (m_root_path / (m_venue + "_h5")).c_str());

    for (auto const &p : std::filesystem::recursive_directory_iterator(
             m_root_path / (m_venue + "_h5"),
             std::filesystem::directory_options::follow_directory_symlink)) {
      // printf("%s regular: %s\n", p.path().c_str(),
      //        p.is_regular_file() ? "yes" : "no");
      if (p.is_regular_file() &&
          (strncmp(p.path().filename().c_str(), m_rec_type,
                   strlen(m_rec_type)) == 0) &&
          ends_with(p.path().filename().string(), ".h5"))
        m_all_files.push_back(p.path());
    }

    // now sort the file paths
    std::sort(m_all_files.begin(), m_all_files.end());

    // printf("Found files:\n");
    // for (auto &f : m_all_files)
    //   printf("  %s\n", f.c_str());
  }

private:
  std::filesystem::path
  MakePath(int yr, unsigned mon, unsigned day,
           utxx::time_val *a_file_start_time = nullptr) const {
    if (a_file_start_time != nullptr)
      *a_file_start_time = utxx::time_val(yr, mon, day, 0, 0, 0, 0);

    char buf[64];
    sprintf(buf, "%s_%04i%02i%02i.h5", m_rec_type, yr, mon, day);
    return m_root_path / (m_venue + "_h5") / std::to_string(yr) /
           std::string(buf);
  }

  std::filesystem::path
  MakePath(utxx::time_val a_time,
           utxx::time_val *a_file_start_time = nullptr) const {
    int yr;
    unsigned mon, day;
    std::tie(yr, mon, day) = a_time.to_ymd();

    return MakePath(yr, mon, day, a_file_start_time);
  }

  auto CheckHeader(const H5::H5File &f,
                   std::filesystem::path const &a_path) const {
    auto header = MDStoreH5File::GetHeader(f);

    if (std::string(header.venue) != m_venue)
      throw utxx::runtime_error("Unexpected Venue in " + a_path.string());

    if (header.rec_type != m_rec_type)
      throw utxx::runtime_error("Unexpected record type in " + a_path.string());

    return header;
  }

  auto ReadData(std::filesystem::path const &a_path,
                const std::string &a_symbol) {
    if (utxx::unlikely(!std::filesystem::exists(a_path)))
      throw utxx::runtime_error("File " + a_path.string() + " does not exist");

    H5::H5File f(a_path.string(), H5F_ACC_RDONLY);
    auto header = CheckHeader(f, a_path);

    // replace '/' in symbol
    auto symbol = a_symbol;
    std::replace(symbol.begin(), symbol.end(), '/', '_');

    // open group for this symbol
    if (!f.exists(symbol)) {
      // throw utxx::runtime_error("File " + a_path.string() +
      //                           " does not contain symbol " + symbol);
      m_recs.resize(0);
      return header;
    }

    auto grp = f.openGroup(symbol);

    // get times
    auto ts_exch =
        MDStoreH5File::template GetData<utxx::time_val>(grp, "ts_exch");
    auto ts_recv =
        MDStoreH5File::template GetData<utxx::time_val>(grp, "ts_recv");

    m_recs.resize(ts_exch.size());

    if constexpr (IsL1) {
      auto bid = MDStoreH5File::template GetData<double>(grp, "bid");
      auto ask = MDStoreH5File::template GetData<double>(grp, "ask");
      auto bid_size = MDStoreH5File::template GetData<double>(grp, "bid_size");
      auto ask_size = MDStoreH5File::template GetData<double>(grp, "ask_size");

      for (size_t i = 0; i < m_recs.size(); ++i) {
        m_recs[i].ts_exch = ts_exch[i];
        m_recs[i].ts_recv = ts_recv[i];
        m_recs[i].rec.bid = bid[i];
        m_recs[i].rec.ask = ask[i];
        m_recs[i].rec.bid_size = bid_size[i];
        m_recs[i].rec.ask_size = ask_size[i];
      }
    } else {
      auto qty = MDStoreH5File::template GetData<double>(grp, "qty");
      auto price = MDStoreH5File::template GetData<double>(grp, "price");

      for (size_t i = 0; i < m_recs.size(); ++i) {
        m_recs[i].ts_exch = ts_exch[i];
        m_recs[i].ts_recv = ts_recv[i];
        m_recs[i].rec.m_totQty = fabs(qty[i]);
        m_recs[i].rec.m_avgPx = price[i];
        m_recs[i].rec.m_bidAggr = (qty[i] >= 0.0);
      }
    }

    return header;
  }

  H5::H5File CreateFile(int yr, int mon, int day) {
    assert(mon > 0);
    assert(day > 0);

    auto path = MakePath(yr, unsigned(mon), unsigned(day));
    if (std::filesystem::exists(path))
      throw utxx::runtime_error("File already exits: " + path.string());

    if (!std::filesystem::exists(path.parent_path()))
      std::filesystem::create_directories(path.parent_path());

    H5::H5File f(path.string(), H5F_ACC_TRUNC);
    typename MDStoreH5File::Header head;
    head.venue = m_venue;
    head.rec_type = m_rec_type;
    MDStoreH5File::SetHeader(f, head);

    return f;
  }

public:
  void Read(const std::string &a_symbol, utxx::time_val a_starting_time,
            CallbackT a_callback) {
    // First: We find the file that contains a_starting_time, or if that file
    // does not exist, the next file. We do this by comparing the file paths,
    // which is kind of crude, but it saves us from having to open each file
    // or construct the file start time from the file path
    auto target_file = this->MakePath(a_starting_time);
    auto file_itr = std::lower_bound(this->m_all_files.begin(),
                                     this->m_all_files.end(), target_file);

    if (file_itr == this->m_all_files.end()) {
      // we couldn't find the file with a_starting_time or any later file, that
      // means we don't have any data
      printf("File %s not found\n", target_file.c_str());
      return;
    }

    // Now open the file, and seek to the first record with
    // time >= a_starting_time
    auto header = ReadData(*file_itr, a_symbol);
    auto rec_itr = m_recs.begin();

    // we need to find the record the first record with
    // time >= a_starting_time
    rec_itr = std::lower_bound(m_recs.begin(), m_recs.end(), a_starting_time,
                               [&](const Rec_t &rec, utxx::time_val time) {
                                 return rec.ts_recv < time;
                               });

    // now read record by record until the call backreturns false or we reach
    // the end of the available records
    while (true) {
      if (utxx::unlikely(rec_itr == m_recs.end())) {
        // we've reached the end of the records in the current file
        ++file_itr;
        if (file_itr != this->m_all_files.end()) {
          // open the next file
          ReadData(*file_itr, a_symbol);
          rec_itr = m_recs.begin();

          if (rec_itr == m_recs.end())
            continue; // open next file
        } else {
          // we reached the end of the last file. This is it
          break;
        }
      }

      if (!a_callback(*rec_itr))
        break;

      ++rec_itr;
    }
  }

  void ConvertMDStoreFiles(utxx::time_val a_day,
                           const std::vector<std::string> &a_symbols) {
    int year;
    unsigned mon, day;
    std::tie(year, mon, day) = a_day.to_ymd();
    auto fout = CreateFile(int(year), int(mon), int(day));

    for (auto &s : a_symbols) {
      std::string prefix = IsL1 ? "ob_L1" : "trades";
      MDStoreReader<REC> mds(m_root_path, m_venue, s, prefix);

      // read files
      auto s_dir = mds.MakeDir(year, mon, day);

      if (!std::filesystem::exists(s_dir)) {
        printf("%s does not exist (symbol %s)\n", s_dir.c_str(), s.c_str());
        continue;
      }

      std::vector<std::filesystem::path> files;
      files.reserve(24);
      size_t total_size = 0;

      for (const auto &e : std::filesystem::directory_iterator(s_dir)) {
        auto file = e.path();

        if ((0 ==
             strncmp(file.filename().c_str(), prefix.c_str(), prefix.size())) &&
            (file.filename().string().find("corrupt") == std::string::npos)) {
          files.push_back(file);
          total_size += std::filesystem::file_size(file);
        }
      }

      std::sort(files.begin(), files.end());

      // this is a bit too much because of the file headers
      m_recs.clear();
      m_recs.reserve(total_size / RecSize);

      for (const auto &f : files) {
        mds.OpenInputFile(f);
        m_recs.insert(m_recs.end(), mds.Recs().begin(), mds.Recs().end());
      }

      printf("%20s %6s: found %3lu files, %10lu recs for %04i-%02u-%02i %s\n", s.c_str(),
             IsL1 ? "L1" : "trades", files.size(), m_recs.size(), year, mon, day,
             files.size() != 24 ? " WARNING" : "");

      if (m_recs.size() == 0) {
        printf("ERROR: %s has no records (symbol %s)\n", s_dir.c_str(), s.c_str());
        continue;
      }

      std::vector<utxx::time_val> ts_exch(m_recs.size());
      std::vector<utxx::time_val> ts_recv(m_recs.size());

      auto grp = fout.createGroup(s);

      if constexpr (IsL1) {
        std::vector<double> bid(m_recs.size());
        std::vector<double> ask(m_recs.size());
        std::vector<double> bid_size(m_recs.size());
        std::vector<double> ask_size(m_recs.size());

        for (size_t i = 0; i < m_recs.size(); ++i) {
          ts_exch[i] = m_recs[i].ts_exch;
          ts_recv[i] = m_recs[i].ts_recv;
          bid[i] = m_recs[i].rec.bid;
          ask[i] = m_recs[i].rec.ask;
          bid_size[i] = m_recs[i].rec.bid_size;
          ask_size[i] = m_recs[i].rec.ask_size;
        }

        MDStoreH5File::template SetData<double>(grp, "bid", bid);
        MDStoreH5File::template SetData<double>(grp, "ask", ask);
        MDStoreH5File::template SetData<double>(grp, "bid_size", bid_size);
        MDStoreH5File::template SetData<double>(grp, "ask_size", ask_size);
      } else {
        std::vector<double> qty(m_recs.size());
        std::vector<double> price(m_recs.size());

        for (size_t i = 0; i < m_recs.size(); ++i) {
          ts_exch[i] = m_recs[i].ts_exch;
          ts_recv[i] = m_recs[i].ts_recv;
          qty[i] = m_recs[i].rec.m_totQty;
          if (!m_recs[i].rec.m_bidAggr)
            qty[i] = -qty[i];
          price[i] = m_recs[i].rec.m_avgPx;
        }

        MDStoreH5File::template SetData<double>(grp, "qty", qty);
        MDStoreH5File::template SetData<double>(grp, "price", price);
      }

      MDStoreH5File::template SetData<utxx::time_val>(grp, "ts_exch", ts_exch);
      MDStoreH5File::template SetData<utxx::time_val>(grp, "ts_recv", ts_recv);
    }
  }
};

using MDStoreH5_L1 = MDStoreH5<MDStoreH5Type::L1>;
using MDStoreH5_Trade = MDStoreH5<MDStoreH5Type::Trade>;

} // namespace QuantSupport
} // namespace MAQUETTE
