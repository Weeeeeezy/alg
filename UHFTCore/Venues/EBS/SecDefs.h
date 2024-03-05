// vim:ts=2:et
//===========================================================================//
//                         "Venues/EBS/SecDefs.h":                           //
//                    Statically-Configured "SecDefs" for EBS                //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace EBS
{
  //=========================================================================//
  // Currently available "SecDef"s for EBS:                                  //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // In EBS, there is normally only 1 Tenor for each Spot Instrument, but there
  // are Swaps and Fwds as well.    However, they are not yet supported, so for
  // the moment, Tenors are NOT used in EBS SecIDs:
  //
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace EBS
} // End namespace MAQUETTE
