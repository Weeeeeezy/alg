// vim:ts=2
//===========================================================================//
//                          "Venues/Huobi/SecDefs.h":                        //
//                 Statically-Configured SecDefs for HUOBI                   //
//===========================================================================//
#pragma once

#include "Basis/SecDefs.h"

#include <vector>

namespace MAQUETTE
{
namespace Huobi
{
  // Huobi supports Spot,Futures and two types of Swap markets
  UTXX_ENUM(
  AssetClassT, int,
    Spt,  // Spot
    Fut,  // Futures
    CSwp, // COIN Swaps
    USwp  // USDT Swaps
  );

  extern std::vector<SecDefS> const SecDefs_Spt;
  extern std::vector<SecDefS> const SecDefs_Fut;
  extern std::vector<SecDefS> const SecDefs_CSwp;
  extern std::vector<SecDefS> const SecDefs_USwp;

  std::vector<SecDefS> const&  GetSecDefs(AssetClassT::type a_ascl);

} // namespace Huobi
} // namespace MAQUETTE
