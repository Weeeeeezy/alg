// vim:ts=2:et:sw=2
//===========================================================================//
//                            "TradeBooksShM.hpp":                           //
//                       Trade Books in Shared Memory:                       //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_TRADEBOOKSSHM_HPP
#define MAQUETTE_STRATEGYADAPTOR_TRADEBOOKSSHM_HPP

#include "StrategyAdaptor/CircBuffSimple.hpp"
#include "StrategyAdaptor/BooksGenShM.hpp"
#include "StrategyAdaptor/TradeBooksShM.h"
#include <functional>

namespace MAQUETTE
{
  //=========================================================================//
  // Trade Books in Shared Memory:                                           //
  //=========================================================================//
  typedef BooksGenShM<TradeEntry> TradeBooksShM;

  // "GetBooksName" (by template specialisation):
  template<>
  inline char const* GetBooksName<TradeEntry>()
    { return "Trades";  }
}
#endif  // MAQUETTE_STRATEGYADAPTOR_TRADEBOOKSSHM_HPP
