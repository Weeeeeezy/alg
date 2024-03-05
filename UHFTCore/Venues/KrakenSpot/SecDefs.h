// vim:ts=2
//===========================================================================//
//                       "Venues/KrakenSpot/SecDefs.h":                      //
//                Statically-Configured SecDefs for KrakenSpot               //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace KrakenSpot
{
  //=========================================================================//
  // "SecDef"s for KrakenSpot:                                               //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of KrakenSpot SecIDs:                //
  //=========================================================================//
  // This is because for each CcyPair, in KrakenSpot there is only one (standard)
  // Tenor:
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace KrakenSpot
} // End namespace MAQUETTE
