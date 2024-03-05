// vim:ts=2:et
//===========================================================================//
//                    "QuantSupport/BT/DataProvider.cpp":                    //
//===========================================================================//

#pragma once

#include "QuantSupport/BT/BackTest.h"

#include "Basis/SecDefs.h"

#include <boost/iterator/iterator_facade.hpp>

#include <string>
#include <vector>

namespace MAQUETTE
{
  namespace History
  {
    /**
     * This class provide order book and trades data to it's consumers
     */
    class DataProvider
    {
      public:
      struct data
      {
        utxx::time_val time;
        uint16_t       id;   // index in secs
        uint16_t       type; // 1 quotes, 2 trade sell, 3 trade buy
        uint16_t       asks_size, bids_size;

        union
        {
          quote* quotes[2];
          quote  trade;
        };

        data()
        {
          memset(this, 0, sizeof(data));
        }
        data(const data& r)
        {
          memset(this, 0, sizeof(data));
          *this = r;
        }
        void operator=(const data& r)
        {
          if (type == 1)
          {
            if (quotes[0])
            {
              delete[] quotes[0];
              quotes[0] = nullptr;
            }
            if (quotes[1])
            {
              delete[] quotes[1];
              quotes[1] = nullptr;
            }
          }
          time      = r.time;
          id        = r.id;
          type      = r.type;
          asks_size = r.asks_size;
          bids_size = r.bids_size;
          if (type == 1)
          {
            quotes[0] = new quote[asks_size];
            quotes[1] = new quote[bids_size];
            std::copy(r.quotes[0], r.quotes[0] + asks_size, quotes[0]);
            std::copy(r.quotes[1], r.quotes[1] + bids_size, quotes[1]);
          }
          else
          {
            trade = r.trade;
          }
        }
        ~data()
        {
          if (type == 1)
          {
            delete[] quotes[0];
            delete[] quotes[1];
          }
        }

        bool operator<(const data& r) const
        {
          if (time == r.time)
            return r.type < type;
          return time < r.time;
        }
      };


      /**
       * Plugins iterator type (const)
       */
      class iterator
        : public boost::
              iterator_facade<iterator, data const, boost::forward_traversal_tag>
      {
        public:
            explicit iterator(
                const std::vector<data>::const_iterator& it) :
              m_iterator(it)
            {
            }

        private:
            friend class boost::iterator_core_access;

            data const& dereference() const
            {
                return *m_iterator;
            }

            void increment()
            {
                ++m_iterator;
            }

            bool equal(iterator const& other) const
            {
                return m_iterator == other.m_iterator;
            }

        private:
            std::vector<data>::const_iterator m_iterator;

      };


      public:
      DataProvider(
          std::vector<SecDefS> const& marketInstruments,
          std::string const&          tickersString,
          std::string const&          loadScript,
          std::string const&          cachePath,
          uint32_t                    marketDepth,
          TimeRange const&            period,
          spdlog::logger*             logger,
          int                         logLevel);

      /**
       * Get instrument definition by ticker string (coin\@market)
       */
      SecDefD const& GetSecDef(std::string const& ticker);

      /**
       * Load data from cache files for @a from - @a to period
       * @note After loading all trades and orders are mixed together and sorted
       * by timestamp
       */
      void Load(time_t from, time_t to);

      /**
       * Get instrument index and SecDef inside data provider by its SecID
       */
      std::pair<uint16_t, SecDefD const*> GetInstrument(SecID id);

      // Data access iterators
      iterator begin() const;
      iterator end() const;

      private:
      /**
       * Get data for ticker from database, if no data file exist
       */
      void fetchTicker(
          std::string const& ticker,
          std::string const& loadScript,
          const TimeRange&   period);

      // Load data from file for single ticker
      void loadTicker(
          const std::string& ticker,
          uint32_t           mktDepth,
          time_t             from,
          time_t             to);

      // Load trades from cache file
      void loadTrades(uint16_t id, const std::string& fname);
      // Load order book from cache file
      void loadOrderBook(
          uint16_t           id,
          uint32_t           mktDepth,
          const std::string& fname);

      struct fvalue
      {
        double value;
        char * from, *to;

        bool zero() const
        {
          return ((to - from) == 1 && *from == '0');
        }
      };

      struct node
      {
        int      bid;
        uint64_t time;
        fvalue   price;
        fvalue   count;
      };

      // Write order book snapshots
      void writeOrderBook(
          time_t                   from,
          const char*              book_fname,
          const std::vector<node>& quotes);

      // Create snapshots from syncshots and updatees
      void prepareOrderBook(time_t from, const char* book_fname, char* cov_name);

      // Create data cache folder if no exists
      void createOutputFolder(std::string const& path) const;

      private:
      // All known instruments
      std::vector<SecDefD> m_instruments;
      // Used tickers
      std::vector<std::string> m_tickers;
      // Market depth
      uint32_t m_marketDepth;
      // Map ticker name to instrument position
      std::map<std::string, uint16_t> m_ids;
      // Data cache (trades and orders sorted by timestamp)
      std::vector<data> m_data;
      // Path to store intermediate data
      std::string m_cachePath;
      // For LOG_ macros
      spdlog::logger* m_logger;
      int             m_debugLevel;
    };

  } // namespace History
} // namespace MAQUETTE
