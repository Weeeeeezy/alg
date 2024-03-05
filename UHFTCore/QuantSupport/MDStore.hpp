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
#include <bits/stdint-uintn.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <string>

#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Basis/Macros.h"
#include "MDSaver.hpp"

namespace MAQUETTE {
namespace QuantSupport {

struct MDRecL1 {
  double bid, ask;
  double bid_size, ask_size;
};

template <typename REC> class MDStoreBase {
public:
  struct MDStoreRecord {
    using InnerRecT = REC;

    utxx::time_val ts_exch;
    utxx::time_val ts_recv;
    REC rec;
  };

  static constexpr uint32_t RecSize = sizeof(MDStoreRecord);

  virtual ~MDStoreBase() {}

protected:
  struct MDStoreFileHeader {
    char venue[32];
    char symbol[16];
    uint32_t record_size;
    utxx::time_val file_start_time;
    utxx::time_val next_file_start_time;
  };

  std::string m_venue;
  std::string m_symbol;
  std::string m_prefix;

  // our root path (including Venue and Symbol)
  std::filesystem::path m_root_path;

  // all the files that existed in m_root_path when the instance was constructed
  std::vector<std::filesystem::path> m_all_files;

  // time of the last written record
  utxx::time_val m_last_written_time;

  MDStoreBase(const std::string &a_root_path, const std::string &a_venue,
              const std::string &a_symbol, const std::string &a_prefix,
              bool a_skip_corrupted)
      : m_venue(a_venue), m_symbol(a_symbol), m_prefix(a_prefix) {
    // replace '/' in symbol
    std::replace(m_symbol.begin(), m_symbol.end(), '/', '_');

    m_root_path = std::filesystem::path(a_root_path) / m_venue / m_symbol;
    std::filesystem::create_directories(m_root_path);

    for (auto const &p : std::filesystem::recursive_directory_iterator(
             m_root_path,
             std::filesystem::directory_options::follow_directory_symlink)) {
      // printf("%s\n", p.path().c_str());
      if (p.is_regular_file() &&
          (p.path().filename().string().find(m_prefix) == 0) &&
          (p.path().filename().string().find("corrupt") == std::string::npos))
        m_all_files.push_back(p.path());
    }

    // now sort the file paths
    std::sort(m_all_files.begin(), m_all_files.end());

    // printf("Found files:\n");
    // for (auto &f : m_all_files)
    //   printf("  %s\n", f.c_str());

    // get the time of the last written record
    m_last_written_time = utxx::time_val(utxx::secs(0));
    while (this->m_all_files.size() > 0) {
      auto path = this->m_all_files[this->m_all_files.size() - 1];
      try {
        auto size = std::filesystem::file_size(path);
        if (size <= sizeof(MDStoreFileHeader)) {
          this->m_all_files.pop_back();
          continue;
        }

        std::fstream fstm(path, std::ios_base::in | std::ios_base::binary);
        auto last_rec = this->GetLastRecordTime(&fstm, path);

        if (last_rec > utxx::now_utc())
          throw utxx::runtime_error(
              "Last written record of file {} is in the future: {}",
              path.string(), last_rec.to_string(utxx::DATE_TIME_WITH_NSEC));

        m_last_written_time = last_rec;
        break;
      } catch (std::exception &ex) {
        if (a_skip_corrupted) {
          printf(
              "WARNING: File '%s' is corrupt and was moved to '%s_corrupt'\n",
              path.string().c_str(), path.string().c_str());
          std::filesystem::rename(path, path.string() + "_corrupt");
          this->m_all_files.pop_back();
        } else {
          printf("ERROR: File '%s' is corrupt\n", path.string().c_str());
          throw ex;
        }
      }
    }
  }

  std::string Leading0(unsigned num) {
    return (num < 10 ? "0" : "") + std::to_string(num);
  }

public:
  auto MakeDir(int yr, unsigned mon, unsigned day) {
    return m_root_path / std::to_string(yr) / Leading0(mon) / Leading0(day);
  }

protected:
  std::filesystem::path MakePath(utxx::time_val a_time,
                                 utxx::time_val *a_file_start_time = nullptr) {
    int yr;
    unsigned mon, day, hr, min, sec;
    std::tie(yr, mon, day, hr, min, sec) = a_time.to_ymdhms();

    if (a_file_start_time != nullptr)
      *a_file_start_time = utxx::time_val(yr, mon, day, hr, 0, 0, 0);

    return MakeDir(yr, mon, day) / (m_prefix + "_" + Leading0(hr) + ".dat");
  }

  MDStoreFileHeader CheckHeader(std::fstream *a_fstm,
                                std::filesystem::path const &a_path) {
    MDStoreFileHeader header;
    a_fstm->seekg(0);
    a_fstm->read(reinterpret_cast<char *>(&header), sizeof(MDStoreFileHeader));

    if (std::string(header.venue) != m_venue)
      throw utxx::runtime_error("Unexpected Venue in " + a_path.string());

    if (std::string(header.symbol) != m_symbol)
      throw utxx::runtime_error("Unexpected Symbol in " + a_path.string());

    if (header.record_size != RecSize)
      throw utxx::runtime_error("Unexpected record size in " + a_path.string());

    return header;
  }

  utxx::time_val GetLastRecordTime(std::fstream *a_fstm,
                                   const std::filesystem::path &path) {
    CheckHeader(a_fstm, path);
    auto size = std::filesystem::file_size(path);

    if (size == sizeof(MDStoreFileHeader))
      throw std::invalid_argument("File " + path.string() + " has no records");

    a_fstm->seekg(long(size - RecSize), std::fstream::beg);

    MDStoreRecord last_rec;
    a_fstm->read(reinterpret_cast<char *>(&last_rec), sizeof(MDStoreRecord));

    if (!a_fstm)
      throw std::runtime_error("Failed to read last record");

    return last_rec.ts_recv;
  }
}; // namespace QuantSupport

template <typename REC> class MDStoreWriter : public MDStoreBase<REC> {
public:
  using typename MDStoreBase<REC>::MDStoreRecord;
  using MDStoreBase<REC>::RecSize;

  MDStoreWriter(const std::string &a_root_path, const std::string &a_venue,
                const std::string &a_symbol, const std::string &a_prefix)
      : MDStoreBase<REC>(a_root_path, a_venue, a_symbol, a_prefix, true) {}

  ~MDStoreWriter() override {
    if (m_of.is_open())
      m_of.close();
  }

  void Write(const MDStoreRecord &record) {
    CHECK_ONLY(if (record.ts_recv < this->m_last_written_time) {
      printf("MDStore Error: Symbol %s, record %s < %s not written\n",
             this->m_symbol.c_str(),
             record.ts_recv.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
             this->m_last_written_time.to_string(utxx::DATE_TIME_WITH_NSEC)
                 .c_str());
      throw utxx::runtime_error("Records must be written chronologically");
    })

    if (utxx::unlikely(!m_of.is_open() ||
                       (record.ts_recv >= m_next_file_start_time))) {
      // we don't have an open file for writing or this record needs to go into
      // the next file
      OpenOutputFile(record.ts_recv);
    }

    m_of.write(reinterpret_cast<const char *>(&record), RecSize);
    this->m_last_written_time = record.ts_recv;
  }

private:
  using typename MDStoreBase<REC>::MDStoreFileHeader;

  std::fstream m_of;                     // current output file stream
  utxx::time_val m_next_file_start_time; // start time of next file

  void OpenOutputFile(utxx::time_val a_time) {
    if (utxx::likely(m_of.is_open()))
      m_of.close();

    // open the file that belongs to the given time and seek to the end of that
    // file
    utxx::time_val file_start_time;
    auto path = this->MakePath(a_time, &file_start_time);
    m_next_file_start_time = file_start_time + utxx::secs(3600);

    if (utxx::unlikely(std::filesystem::exists(path))) {
      try {
        // file already exists, check it's header and time of its last record
        m_of.open(path, std::ios_base::in | std::ios_base::out |
                            std::ios_base::binary | std::ios_base::app);

        auto header = this->CheckHeader(&m_of, path);
        if (header.file_start_time != file_start_time)
          throw utxx::runtime_error("Unexpected start time in " +
                                    path.string());

        if (header.next_file_start_time != m_next_file_start_time)
          throw utxx::runtime_error("Unexpected next time in " + path.string());

        auto last_rec = this->GetLastRecordTime(&m_of, path);
        if (a_time < last_rec)
          throw utxx::runtime_error(
              "Tried to write record to file whose last record is later");

        this->m_last_written_time = last_rec;
        return;
      } catch (std::exception &) {
        printf("WARNING: File '%s' is corrupt and was moved to '%s_corrupt'\n",
               path.string().c_str(), path.string().c_str());
        std::filesystem::rename(path, path.string() + "_corrupt");
      }
    }

    // if we get here, we're creating a new file, write the header
    std::filesystem::create_directories(path.parent_path());
    m_of.open(path,
              std::ios_base::out | std::ios_base::binary | std::ios_base::app);

    MDStoreFileHeader header;
    strncpy(header.venue, this->m_venue.c_str(), 32);
    strncpy(header.symbol, this->m_symbol.c_str(), 16);
    header.record_size = this->RecSize;
    header.file_start_time = file_start_time;
    header.next_file_start_time = m_next_file_start_time;
    m_of.write(reinterpret_cast<char *>(&header), sizeof(MDStoreFileHeader));
    m_of.flush();
  }
};

template <typename REC> class MDStoreReader : public MDStoreBase<REC> {
public:
  using typename MDStoreBase<REC>::MDStoreRecord;
  using MDStoreBase<REC>::RecSize;

  // This is the callback function called for every record that is being read.
  // If the function returns true, reading will continue. If it returns false,
  // reading will stop.
  using CallbackT = std::function<bool(const MDStoreRecord &)>;

  MDStoreReader(const std::string &a_root_path, const std::string &a_venue,
                const std::string &a_symbol, const std::string &a_prefix)
      : MDStoreBase<REC>(a_root_path, a_venue, a_symbol, a_prefix, false) {}

  const auto &Recs() const { return m_recs; }

  auto OpenInputFile(std::filesystem::path const &path) {
    // printf("opening file %s\n", path.c_str());
    if (utxx::unlikely(!std::filesystem::exists(path)))
      throw utxx::runtime_error("File " + path.string() + " does not exist");

    std::fstream fstm(path, std::ios_base::in | std::ios_base::binary);
    auto header = this->CheckHeader(&fstm, path);

    // now read the file
    auto size = std::filesystem::file_size(path);
    auto num_recs = (size - sizeof(MDStoreFileHeader)) / RecSize;
    m_recs.resize(num_recs);
    fstm.read(reinterpret_cast<char *>(m_recs.data()), long(num_recs * RecSize));

    fstm.close();
    return header;
  }

  void Read(utxx::time_val a_starting_time, CallbackT a_callback) {
    if (a_starting_time > this->m_last_written_time) {
      // there are no records on or after a_starting_time
      // printf("No recs: start (%s) > last written (%s)\n",
      //        a_starting_time.to_string(utxx::DATE_TIME_WITH_NSEC).c_str(),
      //        this->m_last_written_time.to_string(utxx::DATE_TIME_WITH_NSEC)
      //            .c_str());
      return;
    }

    // Navigate to the starting position

    // First: We find the file that contains a_starting_time, or if that file
    // does not exist, the next file. We do this by comparing the file paths,
    // which is kind of crude, but it saves us from having to open each file
    // or construct the file start time from the file path
    auto target_file = this->MakePath(a_starting_time);
    auto file_itr = std::lower_bound(this->m_all_files.begin(),
                                     this->m_all_files.end(), target_file);

    if (file_itr == this->m_all_files.end()) {
      // we couldn't find the file with a_starting_time or the next later file,
      // this should not happen because we checked above that the last record we
      // have is AT or AFTER a_starting_time, so throw an exception
      throw utxx::runtime_error("Could not find file " + target_file.string() +
                                " or next later file");
    }

    // Now open the file, and seek to the first record with
    // time >= a_starting_time
    auto header = OpenInputFile(*file_itr);
    auto rec_itr = m_recs.begin();

    if (header.file_start_time == a_starting_time) {
      // the file starts at the requested start time, we don't need to do
      // anything
    } else {
      // we need to find the record the first record with
      // time >= a_starting_time
      rec_itr =
          std::lower_bound(m_recs.begin(), m_recs.end(), a_starting_time,
                           [&](const MDStoreRecord &rec, utxx::time_val time) {
                             return rec.ts_recv < time;
                           });
    }

    // now read record by record until the call backreturns false or we reach
    // the end of the available records
    while (true) {
      if (utxx::unlikely(rec_itr == m_recs.end())) {
        // we've reached the end of the records in the current file
        ++file_itr;
        if (file_itr != this->m_all_files.end()) {
          // open the next file
          OpenInputFile(*file_itr);
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

private:
  using typename MDStoreBase<REC>::MDStoreFileHeader;

  // for efficiency, we read an entire file at once and store its records here
  std::vector<MDStoreRecord> m_recs;
};

} // namespace QuantSupport
} // namespace MAQUETTE
