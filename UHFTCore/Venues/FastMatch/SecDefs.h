// vim:ts=2
//===========================================================================//
//                        "Venues/FastMatch/SecDefs.h":                      //
//                 Statically-Configured SecDefs for FastMatch               //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace FastMatch
{
  //=========================================================================//
  // "SecDef"s for FastMatch:                                                //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of FastMatch SecIDs:                 //
  //=========================================================================//
  // This is because for each CcyPair, in FastMatch there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace FastMatch
} // End namespace MAQUETTE
