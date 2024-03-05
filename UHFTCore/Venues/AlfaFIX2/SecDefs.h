// vim:ts=2:et
//===========================================================================//
//                       "Venues/AlfaFIX2/SecDefs.h":                        //
//                Statically-Configured "SecDefs" for AlfaFIX2               //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace AlfaFIX2
{
  //=========================================================================//
  // Currently available "SecDef"s for AlfaFIX2:                             //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // NB: In AlfaFIX2, Tenors ARE in general required in SecID computation, as
  // they are NOT parts of Symbols:
  constexpr bool UseTenorsInSecIDs = true;

} // End namespace AlfaFIX2
} // End namespace MAQUETTE
