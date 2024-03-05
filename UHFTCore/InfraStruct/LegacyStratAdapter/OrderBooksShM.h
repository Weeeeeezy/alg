// vim:ts=2:et:sw=2
//===========================================================================//
//                            "OrderBooksShM.h":                             //
//                      Order Books in Shared Memory:                        //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_ORDERBOOKSSHM_H
#define MAQUETTE_STRATEGYADAPTOR_ORDERBOOKSSHM_H

#include "StrategyAdaptor/OrderBook.h"
#include "StrategyAdaptor/EventTypes.hpp"
#include <utxx/time_val.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "OrderBookSnapShot":                                                    //
  //=========================================================================//
  // As installed in Shared Memory -- unlike "low-level" "OrderBook", this obj
  // is intended for "consumption" by Clients, not for applying updates. It is
  // supposed to be completely cleaned-up, eg all Price Levels strictly monoto-
  // nic, Bid-Ask Spread always positive, etc:
  //
  struct OrderBookSnapShot
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // NB: "SeqNum" is required for Generations management -- do not remove it!
    //
    // Meta-Data:
    utxx::time_val      m_RecvTime;       // Same as OrderBook::m_recvTime
    utxx::time_val      m_eventTS;        // Same as OrderBook::m_eventTS
    Events::SeqNum      m_SeqNum;         // Same as OrderBook::m_upToDateAs
    Events::SymKey      m_symbol;         // Same as OrderBook::m_symbol

    // The Order Book itself:
    int                 m_currBids;
    OrderBook::Entry    m_bids[OrderBook::MaxDepth];
    int                 m_currAsks;
    OrderBook::Entry    m_asks[OrderBook::MaxDepth];

    // Range-estimated vols of Best Bid and Best Ask. Unit of non-uniform time:
    // a tick resulting in L1 update (ie, if Best Bid and Best Ask are not up-
    // dated, this tick is disregarded and not used in volatility adjustment):
    double              m_bidVol;
    double              m_askVol;

    // Trend Pressure: in [-1.0 .. +1.0], -1.0 corresponds to Down-Trend,
    //                                    +1.0             to Up-Trend:
    double              m_trendPressure;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    OrderBookSnapShot() { memset(this, '\0', sizeof(OrderBookSnapShot)); }

    //-----------------------------------------------------------------------//
    // Equality: Only required in the Debug Mode:                            //
    //-----------------------------------------------------------------------//
    bool operator==(OrderBookSnapShot const& right) const
    {
      // NB: "m_RecvTime" is disregarded in equality testing!
      // Common Flds:
      bool res =
        m_SeqNum   == right.m_SeqNum && m_currBids == right.m_currBids &&
        m_currAsks == right.m_currAsks;
      if (!res)
        return false;

      // Bids:
      for (int i = 0; i < m_currBids; ++i)
        if (m_bids[i] != right.m_bids[i])
          return false;

      // Asks:
      for (int i = 0; i < m_currAsks; ++i)
        if (m_asks[i] != right.m_asks[i])
          return false;

      // XXX: Vols are not used in this equality test, because they are derived
      // qtys...
      return true;
    }

    bool operator!=(OrderBookSnapShot const& right) const
      { return !(*this == right); }

    //-----------------------------------------------------------------------//
    // "GetEventTag":                                                        //
    //-----------------------------------------------------------------------//
    static constexpr Events::EventT GetEventTag()
      { return Events::EventT::OrderBookUpdate; }
  };
}
#endif  // MAQUETTE_STRATEGYADAPTOR_ORDERBOOKSSHM_H
