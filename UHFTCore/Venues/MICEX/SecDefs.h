// vim:ts=2:et
//===========================================================================//
//                         "Venues/MICEX/SecDefs.h":                         //
//       Statically-Configured "SecDef"s for MICEX FX and EQ Sections        //
//===========================================================================//
#pragma once

#include "Basis/SecDefs.h"
#include <utxx/enum.hpp>
#include <vector>
#include <cstring>
#include <cassert>

namespace MAQUETTE
{
namespace MICEX
{
  //=========================================================================//
  // MICEX Asset Classes (for all Protocols -- FAST, FIX, Native):           //
  //=========================================================================//
  UTXX_ENUM(
  AssetClassT, int,
    FX,                       // FX (Currencies)
    EQ                        // Equities
  );

  //=========================================================================//
  // Currently available "SecDef"s for FX and EQ:                            //
  //=========================================================================//
  // The following are lists of "normal" Tradable securities.
  // XXX: For the moment, unlike FORTS, we do NOT maintain separate "Prod" and
  // "Test" lists here:
  //
  extern std::vector<SecDefS> const SecDefs_FX;
  extern std::vector<SecDefS> const SecDefs_EQ;

  // The following are Non-Tradable securities: FX Baskets and EQ Indices:
  //
  extern std::vector<SecDefS> const Baskets_FX;
  extern std::vector<SecDefS> const Indices_EQ;

  //=========================================================================//
  // Function returning MICEX "SecDefS" vectors:                             //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // (*) By AssetClass and Tradable Status:                                  //
  //-------------------------------------------------------------------------//
  inline std::vector<SecDefS> const& GetSecDefs
  (
    AssetClassT::type a_ascl,
    bool              a_is_tradable
  )
  {
    return
      (a_ascl == AssetClassT::FX)
      ? (utxx::likely(a_is_tradable) ? SecDefs_FX : Baskets_FX)
      : (utxx::likely(a_is_tradable) ? SecDefs_EQ : Indices_EQ);
  }

  //-------------------------------------------------------------------------//
  // (*) "All" (Tradable Securities only (NOT including Baskets or Indices): //
  //-------------------------------------------------------------------------//
  std::vector<SecDefS> const& GetSecDefs_All();

  //=========================================================================//
  // Never use Tenors in MICEX SecIDs:                                       //
  //=========================================================================//
  // (Tenors are already encoded in MICEX Symbols, so no further disambiguation
  // is required):
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace MICEX
} // End namespace MAQUETTE
