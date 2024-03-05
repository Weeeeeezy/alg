// vim:ts=2
//===========================================================================//
//                        "Venues/HotSpotFX/SecDefs.h":                      //
//                 Statically-Configured SecDefs for HotSpotFX               //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace HotSpotFX
{
  //=========================================================================//
  // "SecDef"s for HotSpotFX:                                                //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of HotSpotFX SecIDs:                 //
  //=========================================================================//
  // This is because for each CcyPair, in HotSpotFX there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace HotSpotFX
} // End namespace MAQUETTE
