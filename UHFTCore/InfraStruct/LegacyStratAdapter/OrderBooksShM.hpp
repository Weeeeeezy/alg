// vim:ts=2:et:sw=2
//===========================================================================//
//                           "OrderBooksShM.hpp":                            //
//                       Order Books in Shared Memory:                       //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_ORDERBOOKSSHM_HPP
#define MAQUETTE_STRATEGYADAPTOR_ORDERBOOKSSHM_HPP

#include "StrategyAdaptor/CircBuffSimple.hpp"
#include "StrategyAdaptor/BooksGenShM.hpp"
#include "StrategyAdaptor/OrderBooksShM.h"

namespace MAQUETTE
{
  //=========================================================================//
  // Order Books in Shared Memory:                                           //
  //=========================================================================//
  typedef BooksGenShM<OrderBookSnapShot> OrderBooksShM;

  // "GetBooksName" (by template specialisation):
  template<>
  inline char const* GetBooksName<OrderBookSnapShot>()
    { return "OrderBooks"; }
}
#endif  // MAQUETTE_STRATEGYADAPTOR_ORDERBOOKSSHM_HPP
