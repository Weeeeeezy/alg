// vim:ts=2
//===========================================================================//
//                       "Venues/TDAmeritrade/SecDefs.h":                    //
//              Statically-Configured SecDefs for TDAmeritrade               //
//===========================================================================//
#pragma once
#include "Basis/SecDefs.h"
#include <vector>

namespace MAQUETTE {
namespace TDAmeritrade {
UTXX_ENUM(AssetClassT, int,
          Eqt // Equities
          // Fut     // Futures
);

//=========================================================================//
// "SecDef"s for TDAmeritrade: //
//=========================================================================//
extern std::vector<SecDefS> const SecDefs;

// "GetSecDefs":
// For a Given AssetClass and ConnEnv:
//
std::vector<SecDefS> const &GetSecDefs(AssetClassT::type a_ascl);

// "GetSecDefs_All":
// For All Tradable Securities (Fut + Opt) and a Given ConnEnv:
//
std::vector<SecDefS> const &GetSecDefs_All(MQTEnvT a_cenv);
} // End namespace TDAmeritrade
} // End namespace MAQUETTE
