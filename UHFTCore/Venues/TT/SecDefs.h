// vim:ts=2
//===========================================================================//
//                            "Venues/TT/SecDefs.h":                         //
//                    Statically-Configured SecDefs for TT                   //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace TT
{
  //=========================================================================//
  // "SecDef"s for TT:                                                       //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of TT SecIDs:                        //
  //=========================================================================//
  // This is because for each CcyPair, in TT there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace TT
} // End namespace MAQUETTE
