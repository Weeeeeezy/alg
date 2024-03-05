// vim:ts=2:et:sw=2
//===========================================================================//
//                              "InitMargins.h":                             //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_INITMARGINS_H
#define MAQUETTE_STRATEGYADAPTOR_INITMARGINS_H

#include "StrategyAdaptor/EventTypes.h"
#include "StrategyAdaptor/StratEnv.h"
#include <vector>
#include <utility>

namespace MAQUETTE
{
  //-------------------------------------------------------------------------//
  // "InitMargin" (Generic, for use with Reports Generator):                 //
  //-------------------------------------------------------------------------//
  double InitMargin
  (
    std::vector<std::pair<std::string, long>> const& positions,
    Events::AccCrypt                                 acc_crypt
  );

  //-------------------------------------------------------------------------//
  // "InitMargin_FORTS" (For used with "StratEnv"):                          //
  //-------------------------------------------------------------------------//
  double InitMargin_FORTS
  (
    std::map<StratEnv::QSym, long>* poss,
    StratEnv::SecDefsMap const&     defs
  );
}
#endif  // MAQUETTE_STRATEGYADAPTOR_INITMARGINS_H
