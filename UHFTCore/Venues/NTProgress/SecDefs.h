// vim:ts=2:et
//===========================================================================//
//                         "Venues/NTProgress/SecDefs.h":                    //
//                  Statically-Configured "SecDef"s for NTProgress           //
//===========================================================================//
#pragma once

#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace NTProgress
{
  //=========================================================================//
  // Currently available "SecDef"s for NTProgress:                           //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // In NTProgress, Tenors are NOT used in SecID computations (indeed, Tenors
  // are parts of Symbols there):
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace NTProgress
} // End namespace MAQUETTE
