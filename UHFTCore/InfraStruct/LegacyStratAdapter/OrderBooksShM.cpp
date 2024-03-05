// vim:ts=2:et:sw=2
//===========================================================================//
//                           "OrderBooksShM.cpp":                            //
//         Order Books in Shared Memory: Explicit Template Instances:        //
//===========================================================================//
#include "StrategyAdaptor/OrderBooksShM.hpp"

namespace MAQUETTE
{
  template class CircBuffSimple<OrderBookSnapShot>;
  template class BooksGenShM   <OrderBookSnapShot>;
}
