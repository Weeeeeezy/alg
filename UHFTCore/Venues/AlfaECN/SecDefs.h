// vim:ts=2:et
//===========================================================================//
//                         "Venues/AlfaECN/SecDefs.h":                       //
//                    Statically-Configured "SecDefs" for AlfaECN            //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace AlfaECN
{
  //=========================================================================//
  // Currently available "SecDef"s for AlfaECN:                              //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // In AlfaECN, Tenors are NOT used in SecID computations (indeed, Tenors are
  // parts of Symbols there):
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace AlfaECN
} // End namespace MAQUETTE
