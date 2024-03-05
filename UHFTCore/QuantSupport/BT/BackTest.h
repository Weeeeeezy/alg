#pragma once

#include "Basis/SecDefs.h"
#include "Basis/PxsQtys.h"
#include "Basis/EPollReactor.h"
#include "InfraStruct/Strategy.hpp"

#include <boost/property_tree/ptree_fwd.hpp>


namespace MAQUETTE
{
  struct StrategyBT
  {
    virtual void Run() = 0;
    virtual ~StrategyBT()
    {
    }
  };

  using boost::property_tree::ptree;

  typedef std::function<StrategyBT* (ptree& pt, EPollReactor& reactor, spdlog::logger* logger)> InitBT;
  void SetBackTestingEnvironment(ptree& pt, EPollReactor& reactor, spdlog::logger* logger, InitBT init);

  namespace History
  {
    /**
     * @brief The TimeRange struct
     */
    struct TimeRange
    {
      time_t   m_from;
      time_t   m_to;
      uint32_t m_window;

      TimeRange(time_t a_from, time_t a_to, uint32_t a_window) :
        m_from(a_from), m_to(a_to), m_window(a_window)
      {
        if (m_to <= m_from)
          throw std::runtime_error("TimeRange error: m_to <= m_from");
      }

      /**
       * Return @a index window inside range
       */
      std::pair<time_t, time_t> window(uint32_t index) const
      {
        auto left = m_from + m_window * index;
        assert(left < m_to);
        return {left, std::min(left + m_window, m_to)};
      }

      // Return true if window with index is inside range
      bool inside(uint32_t index) const
      {
        return m_from + m_window * index < m_to;
      }
    };


    struct quote
    {
      PriceT price;
      QtyUD count;
    };

    template<typename ... args>
    inline void unused(args& ...)
    {
    }

    /**
     * This class is an interface for data producer. One can use
     * SetTrade and SetBook methods to emulate market events
     */
    class HistoricalData
    {
      public:
        virtual void SetTrade(
            SecDefD const& a_instr,
            utxx::time_val time,
            bool           is_buy,
            PriceT         price,
            QtyUD          count)
        {
          unused(a_instr, time, is_buy, price, count);
        }

        virtual void SetBook(
            const SecDefD& a_instr,
            utxx::time_val time,
            uint16_t       asks_size,
            quote*         asks_quotes,
            uint16_t       bids_size,
            quote*         bids_quotes)
        {
          unused(a_instr, time, asks_size, asks_quotes, bids_size, bids_quotes);
        }

        virtual ~HistoricalData() = default;
    };

    void SubscribeMktData(
        Strategy*       s,
        HistoricalData* c,
        const SecDefD&  a_instr,
        bool            order_mgmt);

    } // namespace History
}

