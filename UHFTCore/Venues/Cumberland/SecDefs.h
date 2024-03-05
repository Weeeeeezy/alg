// vim:ts=2
//===========================================================================//
//                       "Venues/Cumberland/SecDefs.h":                      //
//               Statically-Configured SecDefs for Cumberland                //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace Cumberland
{
  //=========================================================================//
  // "SecDef"s for Cumberland:                                               //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of Cumberland SecIDs:                //
  //=========================================================================//
  // This is because for each CcyPair, in Cumberland there is only one
  // (standard) Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace Cumberland
} // End namespace MAQUETTE
