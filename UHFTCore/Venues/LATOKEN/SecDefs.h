// vim:ts=2
//===========================================================================//
//                          "Venues/LATOKEN/SecDefs.h":                      //
//                 Statically-Configured SecDefs for LATOKEN                 //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>
#include <tuple>

namespace MAQUETTE
{
namespace LATOKEN
{
  //=========================================================================//
  // "SecDef"s for LATOKEN:                                                  //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // Because this is a single unified vector of "SecDef"s, we don't really need
  // a "GetSecDefs" function...

  //=========================================================================//
  // Tenors are NOT used in computation of LATOKEN SecIDs:                   //
  //=========================================================================//
  // This is because for each CcyPair, in LATOKEN there is only one (standard)
  // Tenor, that is, SPOT:
  //
  constexpr bool UseTenorsInSecIDs = false;

  //==========================================================================//
  // CcyIDs and AssetIDs:                                                     //
  //==========================================================================//
  // (XXX: The semantics is obscure):
  // This is a list of Quadruples (CcyID, AssetID, AssetName, FeeRate);
  // NB: IDs and Names may not be unique!
  //
  extern std::vector<std::tuple<unsigned, unsigned, char const*, double>>
  const  Assets;

} // End namespace LATOKEN
} // End namespace MAQUETTE
