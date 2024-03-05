// vim:ts=2:et
//===========================================================================//
//                        "Venues/FeedOS/SecDefs.h":                         //
//   Statically-Configured "SecDefs": QuantHouse / S&P Capital IP / FeedOS   //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE
{
namespace FeedOS
{
  //=========================================================================//
  // XXX: At the moment, only ICE Energy Futures are provided here:          //
  //=========================================================================//
  extern std::vector<SecDefS> const SecDefs;

  // In FeedOS, all Instruments are somehow uniquely identified, so we do NOT
  // use Tenors in SecID computations:
  //
  constexpr bool UseTenorsInSecIDs = false;

} // End namespace FeedOS
} // End namespace MAQUETTE
