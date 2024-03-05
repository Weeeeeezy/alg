// vim:ts=2
//===========================================================================//
//                          "Venues/BitMEX/SecDefs.h":                      //
//                 Statically-Configured SecDefs for BitMEX                 //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace BitMEX
{
  //=========================================================================//
  // "SecDef"s for BitMEX:                                                  //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of BitMEX SecIDs:                   //
  //=========================================================================//
  // This is because for each CcyPair, in BitMEX there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace BitMEX
} // End namespace MAQUETTE
