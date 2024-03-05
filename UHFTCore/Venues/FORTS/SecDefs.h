// vim:ts=2:et
//===========================================================================//
//                         "Venues/FORTS/SecDefs.h":                         //
//   Statically-Configured "SecDefs" for FORTS Futures and Options Sections  //
//===========================================================================//
#pragma once

#include "Basis/SecDefs.h"
#include <utxx/enum.hpp>
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <vector>
#include <cstring>
#include <cassert>

namespace MAQUETTE
{
namespace FORTS
{
  //=========================================================================//
  // FORTS Asset Classes (for all Protocols -- FAST, FIX, Native):           //
  //=========================================================================//
  UTXX_ENUM(
  AssetClassT, int,
    FOL,                      // Futures + Options (FullOrdersLog)
    Fut,                      // Futures
    Opt,                      // Options
    Ind,                      // Indices (MktData only)
    News,                     // TODO: Not fully implemented yet
    NewsSKRIN                 // ditto
  );

  //=========================================================================//
  // Currently available "SecDef"s for FORTS:                                //
  //=========================================================================//
  // The following are lists of "normal" Tradable securities:
  //
  extern std::vector<SecDefS> const SecDefs_Fut_Prod;
  extern std::vector<SecDefS> const SecDefs_Fut_Test;

  extern std::vector<SecDefS> const SecDefs_Opt_Prod;
  extern std::vector<SecDefS> const SecDefs_Opt_Test;

  // TODO: The following are Non-Tradable securities:
  //
  extern std::vector<SecDefS> const SecDefs_Ind_Prod;
  extern std::vector<SecDefS> const SecDefs_Ind_Test;

  //=========================================================================//
  // Functions returning FORTS "SecDefS" vectors:                            //
  //=========================================================================//
  // "GetSecDefs":
  // For a Given AssetClass and ConnEnv:
  //
  std::vector<SecDefS> const&  GetSecDefs
    (AssetClassT::type a_ascl, MQTEnvT a_cenv);

  // "GetSecDefs_All":
  // For All Tradable Securities (Fut + Opt) and a Given ConnEnv:
  //
  std::vector<SecDefS> const& GetSecDefs_All(MQTEnvT a_cenv);

  //=========================================================================//
  // Tenors are NOT used in FORTS SecIDs:                                    //
  //=========================================================================//
  // This is because:
  // (*) Maturities are already encoded in Futs or Opts Symbols;
  // (*) All SecIDs in FORTS are static:
  //
  constexpr bool UseTenorsInSecIDs = false;

  //=========================================================================//
  // "IsProdEnv":                                                            //
  //=========================================================================//
  // XXX: Only "Prod" and "Test" ConnEnvs are currently allowed; "Test" is not
  // supported here:
  inline bool IsProdEnv(MQTEnvT a_cenv)
  {
    bool isProd =  (a_cenv == MQTEnvT::Prod);
    if (utxx::unlikely
       (!isProd && (a_cenv != MQTEnvT::Test)))
      throw utxx::badarg_error
            ("FORTS::GetSecDefs_All: UnSupported ConnEnv: ",
             MQTEnvT::to_string(a_cenv));
    return isProd;
  }
} // End namespace FORTS
} // End namespace MAQUETTE
