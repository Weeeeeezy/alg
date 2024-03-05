// vim:ts=2
//===========================================================================//
//                        "Venues/BitFinex/SecDefs.h":                       //
//                 Statically-Configured SecDefs for BitFinex                //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace BitFinex
{
  //=========================================================================//
  // "SecDef"s for BitFinex:                                                 //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of BitFinex SecIDs:                  //
  //=========================================================================//
  // This is because for each CcyPair, in BitFinex there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace BitFinex
} // End namespace MAQUETTE
