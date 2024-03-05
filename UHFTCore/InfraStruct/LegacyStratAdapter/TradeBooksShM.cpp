// vim:ts=2:et:sw=2
//===========================================================================//
//                           "TradeBooksShM.cpp":                            //
//         Trade Books in Shared Memory: Explicit Template Instances:        //
//===========================================================================//
#include "StrategyAdaptor/TradeBooksShM.hpp"

namespace MAQUETTE
{
  template class CircBuffSimple<TradeEntry>;
  template class BooksGenShM   <TradeEntry>;
}
