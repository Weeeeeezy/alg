// vim:ts=2:et:sw=2
//===========================================================================//
//                             "TradeBooksShM.h":                            //
//                       Trade Books in Shared Memory:                       //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_TRADEBOOKSSHM_H
#define MAQUETTE_STRATEGYADAPTOR_TRADEBOOKSSHM_H

#include "StrategyAdaptor/EventTypes.h"
#include <utxx/time_val.hpp>
#include <sys/time.h>

namespace MAQUETTE
{
  //-------------------------------------------------------------------------//
  // "AggressorT":                                                           //
  //-------------------------------------------------------------------------//
  // Indicates which side of the Order Book has aggressed the Bid-Ask Spread
  // (typically, "Undefined"):
  //
  enum class AggressorT
  {
    Undefined   =  0,
    BidSide   =  1,
    AskSide   =  2
  };

  //=========================================================================//
  // "TradeEntry" as visible to the Clients (Startegies):                    //
  //=========================================================================//
  struct TradeEntry
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // Meta-Data:
    utxx::time_val      m_RecvTime;
    Events::SeqNum      m_SeqNum;     // Normally monotonic but not contin's
    Events::SymKey      m_symbol;
    // Trade Itself:
    double              m_price;
    long                m_qty;
    utxx::time_val      m_eventTS;    // Trade TimeStamp, from the Exchange
    AggressorT          m_aggressor;  // Usually "Unknown"

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    TradeEntry()        { memset(this, '\0', sizeof(TradeEntry)); }

    //-----------------------------------------------------------------------//
    // Equality:                                                             //
    //-----------------------------------------------------------------------//
    // XXX: For some reason, equality is not auto-generated. We only need it in
    // Debug mode.   NB: "m_RecvTime" is of course excluded from equality test:
    //
    bool operator==(TradeEntry const& right) const
    {
      return m_SeqNum    == right.m_SeqNum  && m_price   == right.m_price   &&
             m_qty       == right.m_qty     && m_eventTS == right.m_eventTS &&
             m_aggressor == right.m_aggressor;
    }

    bool operator!=(TradeEntry const& right) const
      { return !(*this == right); }

    //-----------------------------------------------------------------------//
    // "GetEventTag":                                                        //
    //-----------------------------------------------------------------------//
    static constexpr Events::EventT GetEventTag()
      { return Events::EventT::TradeBookUpdate; }
  };
}
#endif  // MAQUETTE_STRATEGYADAPTOR_TRADEBOOKSSHM_H
