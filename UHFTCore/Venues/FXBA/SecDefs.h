// vim:ts=2:et
//===========================================================================//
//                        "Venues/FXBA/SecDefs.h":                           //
//           Statically-Configured "SecDefs" for FXBestAggregator            //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace FXBA
{
  //=========================================================================//
  // Currently available "SecDef"s for FXBA:                                 //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // In FXBA, Tenors are included into Symbols, so they are NOT explicitly used
  // in SecID computations:
  //
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace FXBA
} // End namespace MAQUETTE
