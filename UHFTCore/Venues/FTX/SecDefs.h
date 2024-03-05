// vim:ts=2
//===========================================================================//
//                          "Venues/FTX/SecDefs.h":                          //
//                 Statically-Configured SecDefs for FTX                     //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace FTX
{
  //=========================================================================//
  // "SecDef"s for FTX:                                                      //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of FTX SecIDs:                       //
  //=========================================================================//
  // This is because for each CcyPair, in Binance there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace FTX
} // End namespace MAQUETTE
