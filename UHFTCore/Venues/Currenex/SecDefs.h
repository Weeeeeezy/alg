// vim:ts=2
//===========================================================================//
//                         "Venues/Currenex/SecDefs.h":                      //
//                 Statically-Configured SecDefs for Currenex                //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace Currenex
{
  //=========================================================================//
  // "SecDef"s for Currenex:                                                 //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of Currenex SecIDs:                  //
  //=========================================================================//
  // This is because for each CcyPair, in Currenex there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace Currenex
} // End namespace MAQUETTE
