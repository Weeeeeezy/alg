// vim:ts=2:et:sw=2
//===========================================================================//
//                             "SecDefsShM.h":                               //
//            Security (Instrument) Definitions in Shared Memory:            //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_SECDEFSSHM_H
#define MAQUETTE_STRATEGYADAPTOR_SECDEFSSHM_H

#ifndef ODB_COMPILER
#include <Infrastructure/SecDefTypes.h>
#include <StrategyAdaptor/CircBuffSimple.hpp>
#include <StrategyAdaptor/BooksGenShM.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // Security Definitions in Shared Memory or Local Memory:                  //
  //=========================================================================//
  // When SecDefs are stored in the Connector's ShM:
  //
  typedef BooksGenShM<SecDef> SecDefsShM;

  // "GetBooksName" (by template specialisation):
  template<>
  inline char const* GetBooksName<SecDef>() { return "SecDefs"; }

  inline char const* ToString(TradingStatus trst)
  {
    switch (trst)
    {
    case TradingStatus::NoTrading:   return "NoTrading";
    case TradingStatus::CancelsOnly: return "CancelsOnly";
    case TradingStatus::FullTrading: return "FullTrading";
    default:                         return "UnDefined";
    }
  }
}

#endif // !ODB_COMPILER

#endif // MAQUETTE_STRATEGYADAPTOR_SECDEFSSHM_H
