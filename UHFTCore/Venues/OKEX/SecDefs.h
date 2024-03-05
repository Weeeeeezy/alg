// vim:ts=2
//===========================================================================//
//                           "Venues/OKEX/SecDefs.h":                        //
//                  Statically-Configured SecDefs for OKEX                   //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace OKEX
{
  //=========================================================================//
  // "SecDef"s for OKEX:                                                  //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of OKEX SecIDs:                   //
  //=========================================================================//
  // This is because for each CcyPair, in OKEX there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace OKEX
} // End namespace MAQUETTE
