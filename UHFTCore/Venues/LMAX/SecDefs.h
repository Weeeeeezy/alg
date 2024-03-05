// vim:ts=2
//===========================================================================//
//                           "Venues/LMAX/SecDefs.h":                        //
//                   Statically-Configured SecDefs for LMAX                  //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace LMAX
{
  //=========================================================================//
  // "SecDef"s for LMAX:                                                     //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of LMAX SecIDs:                      //
  //=========================================================================//
  // This is because for each CcyPair, in LMAX there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace LMAX
} // End namespace MAQUETTE
