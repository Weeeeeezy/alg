// vim:ts=2:et
//===========================================================================//
//                    "QuantSupport/BT/DataProvider.cpp":                    //
//===========================================================================//

#include "QuantSupport/BT/DataProvider.h"

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <dirent.h>

namespace MAQUETTE
{

  namespace
  {

    std::string time_to_str(time_t t)
    {
      tm*  lf = gmtime(&t);
      char buf[15];
    #if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-overflow"
    #endif
      sprintf(
          buf,
          "%04d%02d%02d%02d%02d%02d",
          lf->tm_year + 1900,
          lf->tm_mon + 1,
          lf->tm_mday,
          lf->tm_hour,
          lf->tm_min,
          lf->tm_sec);
    #if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic pop
    #endif
      return std::string(buf, buf + 14);
    }

    std::vector<char> read_file(const char* fname)
    {
      std::vector<char> ret;

      int hfile = open(fname, O_RDONLY);
      if (hfile <= 0)
      {
        throw utxx::runtime_error("open file error: ", fname);
      }

      struct stat st;
      fstat(hfile, &st);
      ret.resize(uint32_t(st.st_size));
      if (!st.st_size
          || read(hfile, &ret[0], uint32_t(st.st_size)) != uint32_t(st.st_size))
      {
        throw utxx::runtime_error("read file error: ", fname);
      }
      close(hfile);
      return ret;
    }

    bool file_exist(const char* fname)
    {
      int hfile = open(fname, O_RDONLY);
      if(hfile > 0) {
          struct stat st;
          fstat(hfile, &st);
          close(hfile);
          return !!st.st_size;
      }
      return false;
    }

  } // namespace

  namespace History
  {
    // Defined in SecDefs.cpp
    std::string GetCovexityName(const std::string& ticker);

    DataProvider::DataProvider(
        std::vector<SecDefS> const& marketInstruments,
        const std::string&          tickersString,
        const std::string&          loadScript,
        const std::string&          cachePath,
        uint32_t                    marketDepth,
        TimeRange const&            period,
        spdlog::logger*             logger,
        int                         logLevel) :
      m_marketDepth(marketDepth),
      m_cachePath(std::move(cachePath)),
      m_logger(logger),
      m_debugLevel(logLevel)
    {
      for (const auto& secDef: marketInstruments)
      {
        m_ids
            [std::string(secDef.m_Symbol.data()) + "@"
             + secDef.m_Exchange.data()] = uint16_t(m_instruments.size());
        m_instruments.push_back(SecDefD(secDef, false, 0, 0));
      }

      createOutputFolder(m_cachePath);

      boost::split(m_tickers, tickersString, [](char c) { return c == ','; });
      for (auto ticker: m_tickers)
      {
        fetchTicker(ticker, loadScript, period);
      }
    }

    const SecDefD& DataProvider::GetSecDef(const std::string& ticker)
    {
      auto it = m_ids.find(ticker);
      if (it == m_ids.end())
      {
        throw utxx::runtime_error(
              "DataProvider::GetSecDef() not found: ", ticker);
      }
      return m_instruments[it->second];
    }

    void DataProvider::fetchTicker(
        const std::string& ticker,
        const std::string& loadScript,
        TimeRange const&   period)
    {
      if(!file_exist(loadScript.c_str()))
      {
        throw utxx::runtime_error("LoadData script not found: ", loadScript);
      }

      LOG_INFO(3, "BackTesting::fetching {}", ticker)

      char buf[256];
      std::copy(m_cachePath.begin(), m_cachePath.end(), buf);
      char* i = buf + m_cachePath.size();
      std::copy(ticker.begin(), ticker.end(), i);
      i += ticker.size();
      *(i++) = '_';
      i = utxx::itoa_left<uint32_t, 10>(i, m_marketDepth);
      *(i++) = '_';
      unsigned index{0};

      while (period.inside(index))
      {
        auto [from, to] = period.window(index);
        ++index;

        tm *lf = gmtime(&from);
        char* f = i
            + sprintf(
              i,
              "%04d%02d%02d%02d%02d%02d",
              lf->tm_year + 1900,
              lf->tm_mon + 1,
              lf->tm_mday,
              lf->tm_hour,
              lf->tm_min,
              lf->tm_sec);
        *(f++) = '_';
        lf = gmtime(&to);
        f += sprintf(
              f,
              "%04d%02d%02d%02d%02d%02d",
              lf->tm_year + 1900,
              lf->tm_mon + 1,
              lf->tm_mday,
              lf->tm_hour,
              lf->tm_min,
              lf->tm_sec);
        strcpy(f, "_book");

        if(file_exist(buf))
        {
          LOG_INFO(3, "File {} exists, using local copy", buf)
          continue;
        }

        LOG_INFO(3, "BackTesting::saving {}", buf)
        std::string c = History::GetCovexityName(ticker);
        char cmd[256], cov[256];
        char* p = stpcpy(cmd, "python3 ");
        p = stpncpy(p, loadScript.c_str(), loadScript.size());
        *(p++) = ' ';
        const char* o = std::find(c.c_str(), c.c_str() + c.size(), '@');
        char* cn = p;
        p = stpncpy(p, c.c_str(), uint32_t(o - c.c_str()));
        *(p++) = ' ';
        p = stpcpy(p, o + 1);
        *(p++) = ' ';
        p = stpncpy(p, i, 14);
        *(p++) = ' ';
        p = stpncpy(p, i + 15, 14);
        *p = char();

        strcpy(cov, cn);
        std::replace(cov, cov + (p - cn), ' ', '_');
        strcpy(cov + (p - cn), "_syncshots.csv");

        if (!file_exist(cov))
        {
          LOG_INFO(4, "exec: {}", cmd)
          FILE* fp = popen(cmd, "r");
          if (!fp)
            throw utxx::runtime_error("popen error: ", cmd);
          char d[256];
          size_t r = fread(d, 1, sizeof(d), fp);
          pclose(fp);
          if (r)
            throw utxx::runtime_error(
                "unexpected result: ", std::string(d, d + r));
        }
        *(cov + (p - cn)) = char();
        prepareOrderBook(from, buf, cov);

        *(cov + (p - cn)) = char();
        std::string cp = std::string("cp ") + cov + "_trades.csv "
            + m_cachePath + ticker + "_"
            + std::string(p - 29, p - 15) + "_"
            + std::string(p - 14, p) + "_trades.csv";

        LOG_INFO(4, "exec: {}", cp)
        system(cp.c_str());
      }

      LOG_INFO(3, "Fetching done")
    }

    void DataProvider::Load(time_t from, time_t to)
    {
      m_data.clear();
      LOG_INFO(
            4,
            "BackTesting::Load from {}, to {}, {} tickers",
            from,
            to,
            m_tickers.size())

      for (auto ticker: m_tickers)
      {
        loadTicker(ticker, m_marketDepth, from, to);
      }

      std::sort(m_data.begin(), m_data.end());
      LOG_INFO(4, "BackTesting::Load complete")
    }

    std::pair<uint16_t, const SecDefD *> DataProvider::GetInstrument(SecID id)
    {
      auto it = std::find_if(
          m_instruments.begin(),
          m_instruments.end(),
          [id](const SecDefD& sdef) { return sdef.m_SecID == id; });

      return {
          static_cast<uint16_t>(std::distance(m_instruments.begin(), it)),
            &(*it)};
    }

    DataProvider::iterator DataProvider::begin() const
    {
      return iterator{m_data.begin()};
    }

    DataProvider::iterator DataProvider::end() const
    {
      return iterator{m_data.end()};
    }

    void DataProvider::loadTicker(
        const std::string& ticker,
        uint32_t           mktDepth,
        time_t             from,
        time_t             to)
    {
      auto it = m_ids.find(ticker);
      if (it == m_ids.end())
      {
        throw utxx::runtime_error(
              "DataManager::load() unknown ticker: ", ticker);
      }
      std::string sf = time_to_str(from);
      std::string st = time_to_str(to);

      loadOrderBook(
            it->second,
            mktDepth,
            m_cachePath + ticker + "_" + std::to_string(mktDepth) + "_" + sf + "_"
            + st + "_book");
      loadTrades(
            it->second,
            m_cachePath + ticker + "_" + sf + "_" + st + "_trades.csv");
    }

    void DataProvider::loadTrades(uint16_t id, const std::string &fname)
    {
      std::vector<char> buf = read_file(fname.c_str());
      char *it = &buf[0], *ie = it + buf.size();
      for(;it != ie; ++it)
      {
        data d;
        d.id = id;
        it = const_cast<char*>(utxx::fast_atoi<uint64_t, false>(
                                 it, ie, reinterpret_cast<uint64_t&>(d.time)));
        if(*it != ',')
          throw utxx::runtime_error(
              "DataManager::load_trades() parsing error: ",
              fname);
        ++it;
        if(*it == '1' && *(it + 1) == ',') {
          it = it + 2;
          d.type = 3;
        }
        else if(*it != '-' || *(it + 1) != '1' || *(it + 2) != ',')
          throw utxx::runtime_error(
              "DataManager::load_trades() parsing error: ",
              fname);
        else
        {
          it += 3;
          d.type = 2;
        }

        d.trade.price = PriceT(strtod(it, &it));
        if(*it != ',')
          throw utxx::runtime_error(
              "DataManager::load_trades() parsing error: ",
              fname);
        ++it;
        d.trade.count = QtyUD(strtod(it, &it));

        m_data.push_back(d);
      }
    }

    void DataProvider::loadOrderBook(
        uint16_t           id,
        uint32_t           mktDepth,
        const std::string& fname)
    {
      auto skip_fixed = [fname](char*& it, char c) -> void {
        if (*it != c)
          throw utxx::runtime_error(
              "DataManager::load_book() parsing error: ",
              fname);
        ++it;
      };

      std::vector<char> buf = read_file(fname.c_str());
      char *            it = &buf[0], *ie = it + buf.size();
      for (; it != ie; ++it)
      {
        data d;
        d.id   = id;
        d.type = 1;
        it     = const_cast<char*>(utxx::fast_atoi<uint64_t, false>(
            it,
            ie,
            reinterpret_cast<uint64_t&>(d.time)));
        if (*it == ',')
        {
          ++it;
          d.quotes[0]   = new quote[mktDepth];
          d.quotes[1]   = new quote[mktDepth];
          quote*    ptr = d.quotes[1];
          uint16_t* sz  = &d.asks_size;
        repeat:
          if (*it == '1')
            ++it;
          else if (*it != '-' || *(it + 1) != '1')
            throw utxx::runtime_error(
                "DataManager::load_book() parsing error: ",
                fname);
          else
          {
            it += 2;
            ptr = d.quotes[0];
            sz  = &d.bids_size;
          }
          skip_fixed(it, ',');
          for (;;)
          {
            if (*it == '[')
            {
              ++it;
              ptr->price = PriceT(strtod(it, &it));
              skip_fixed(it, ',');
              ptr->count = QtyUD(strtod(it, &it));
              skip_fixed(it, ']');
              ++ptr;
              ++(*sz);
            }
            if (*it == ',')
              ++it;
            else if (*it == '\n')
              break;
            else if (*it == '-')
              goto repeat;
            else
              throw utxx::runtime_error(
                  "DataManager::load_book() parsing error: ",
                  fname);
          }
        }
        else if (*it != '\n')
          throw utxx::runtime_error(
              "DataManager::load_book() parsing error: ",
              fname);
        else
        {
          d.quotes[0] = nullptr;
          d.quotes[1] = nullptr;
        }

        m_data.push_back(d);
      }
    }

    void DataProvider::writeOrderBook(
        time_t                                 from,
        const char*                            book_fname,
        const std::vector<DataProvider::node>& quotes)
    {
      if (quotes.empty())
        throw utxx::runtime_error("nothing for save: ", book_fname);

      uint64_t time = uint64_t(from) * 1000000000;

      auto apply = [](auto& book_a, auto& book_b, auto it) -> void {
        if (Abs(it->count.value) < 1e-12)
        {
          book_b.erase(it->price.value);
          book_a.erase(it->price.value);
        }
        else
        {
          if (it->bid == 1)
          {
            book_b[it->price.value] = it->count.value;
            book_a.erase(it->price.value);
          }
          else if (it->bid == -1)
          {
            book_a[it->price.value] = it->count.value;
            book_b.erase(it->price.value);
          }
        }
      };

      auto dump =
          [this, book_fname](uint64_t tm, char*& s, auto& book_a, auto& book_b)
          -> void {
        if (!book_a.empty() && !book_b.empty()
            && book_a.begin()->first < book_b.begin()->first)
        {
          LOG_INFO(3, "ask < bid detected for time: {}, {}", tm, book_fname)
          return;
        }

        auto db = [this](auto& book, char*& str) -> void {
          auto it = book.begin(), ie = book.end();
          for (uint32_t i = 0; i != m_marketDepth && it != ie; ++i, ++it)
          {
            str = stpncpy(str, ",[", 2);
            str += utxx::ftoa_left(it->first, str, 16, 8);
            str = stpncpy(str, ",", 1);
            str += utxx::ftoa_left(it->second, str, 16, 8);
            str = stpncpy(str, "]", 1);
          }
        };

        s = utxx::itoa_left<uint64_t, 19>(s, tm);
        if (!book_b.empty())
        {
          s = stpncpy(s, ",1", 2);
          db(book_b, s);
        }
        if (!book_a.empty())
        {
          s = stpncpy(s, ",-1", 3);
          db(book_a, s);
        }
        s = stpncpy(s, "\n", 1);
      };

      std::map<double /*price*/, double /*count*/>   book_a;
      std::map<double, double, std::greater<double>> book_b;
      auto it = quotes.begin(), ie = quotes.end();

      for (; it != ie && it->time <= time; ++it)
        apply(book_a, book_b, it);

      std::vector<char> buf(50 * 1024 * 1024);
      char *            c = &buf[0], *e = &buf[0] + buf.size();
      for (; it != ie; ++it)
      {
        if (it->time != time)
        {
          if (e - c < 1024 * 1024)
          {
            size_t sz = size_t(c - &buf[0]);
            buf.resize(buf.size() * 2);
            c = &buf[0] + sz;
            e = &buf[0] + buf.size();
          }
          dump(time, c, book_a, book_b);
        }

        apply(book_a, book_b, it);
        time = it->time;
      }
      dump(time, c, book_a, book_b);

      std::ofstream f(book_fname, std::ofstream::trunc | std::ios::binary);
      f.write(&buf[0], uint32_t(c - &buf[0]));
      LOG_INFO(3, "file {} saved", book_fname)
    }

    void DataProvider::prepareOrderBook(
        time_t      from,
        const char* book_fname,
        char*       cov_name)
    {
      char*             f = cov_name + strlen(cov_name);
      std::vector<node> quotes;

      strcpy(f, "_syncshots.csv");
      std::vector<char> sync = read_file(cov_name);
      strcpy(f, "_updates.csv");
      std::vector<char> up = read_file(cov_name);

      uint64_t time = 0;
      for (uint32_t i = 0; i != 2; ++i)
      {
        std::vector<char>& data = i ? up : sync;
        const char *       it = &data[0], *ie = it + data.size();
        while (it != ie)
        {
          node n;
          it = utxx::fast_atoi<uint64_t, false>(it, ie, n.time) + 1;
          if (!i)
          {
            if (time && time != n.time)
              break;
            time = n.time;
          }
          it            = utxx::fast_atoi<int, false>(it, ie, n.bid) + 1;
          n.price.from  = const_cast<char*>(it);
          n.price.value = strtod(it, &n.price.to);
          it            = n.price.to + 1;
          n.count.from  = const_cast<char*>(it);
          n.count.value = strtod(it, &n.count.to);
          it            = n.count.to;
          if (*it != '\n')
            throw utxx::runtime_error("parsing file error: ", cov_name);
          if (!i || time < n.time)
            quotes.push_back(n);
          ++it;
        }
      }
      writeOrderBook(from, book_fname, quotes);
    }

    void DataProvider::createOutputFolder(const std::string &path) const
    {
      DIR *d = opendir(path.c_str());
      if(!d) {
        std::string md = "mkdir " + path;
        LOG_INFO(4, "exec: {}", md)
        system(md.c_str());
        d = opendir(path.c_str());
        if(!d)
        {
          throw utxx::runtime_error(
                "Unable to create data folder: ", path);
        }
      }
      closedir(d);
    }

  } // namespace History
} // namespace MAQUETTE
