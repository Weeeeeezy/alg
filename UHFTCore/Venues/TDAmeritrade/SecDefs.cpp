// vim:ts=2:et
//===========================================================================//
//                    "Venues/TDAmeritrade/SecDefs.cpp":                     //
//===========================================================================//
#include "Venues/TDAmeritrade/SecDefs.h"
using namespace std;

namespace MAQUETTE {
namespace TDAmeritrade {
vector<SecDefS> const SecDefs{
    { 0, "UVXY",  "uvxy", "", "ESXXXX", "TDAmeritrade", "EQUITY", "", "",
         "UVXY",  "USD", 'A', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    {0,  "SPY",   "spy",  "", "ESXXXX", "TDAmeritrade", "EQUITY", "", "",
         "SPY",   "USD", 'A', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "HSBC",  "hsbc",   "", "ESXXXX", "TDAmeritrade", "EQUITY", "", "",
         "HSBC",  "USD", 'A', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "WFC",   "wfc",    "", "ESXXXX", "TDAmeritrade", "EQUITY", "", "",
         "WFC",   "USD", 'A', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "BAC",   "bac",    "", "ESXXXX", "TDAmeritrade", "EQUITY", "", "",
         "BAC",   "USD", 'A', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "SAN",   "san",    "", "ESXXXX", "TDAmeritrade", "EQUITY", "", "",
         "SAN",   "USD", 'A', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    }
};

//-------------------------------------------------------------------------//
// "SecDefs_All":                                                          //
//-------------------------------------------------------------------------//
// Initially NULL, created and filled in on demand. Will contain ALL FORTS
// src SecDefs:
//
// static vector<SecDefS> SecDefs_All_Prod;    // Initially empty
// static vector<SecDefS> SecDefs_All_Test;    // ditto

//-------------------------------------------------------------------------//
// "GetSecDefs" (for a given Asset Class):                                 //
//-------------------------------------------------------------------------//
// std::vector<SecDefS> const&  GetSecDefs(AssetClassT::type a_ascl)
// {
//   switch (a_ascl)
//   {
//   case AssetClassT::Eqt:
//     return SecDefs_Eqt;
//   default:
//     // Other AssetClasses are not implemented yet:
//     throw utxx::badarg_error
//           ("TDAmeritrade::GetSecDefs: UnSupported AssetClass=",
//            AssetClassT(a_ascl).c_str());
//   }
// }

//-------------------------------------------------------------------------//
// "GetSecDefs_All":                                                       //
//-------------------------------------------------------------------------//
// vector<SecDefS> const& GetSecDefs_All(MQTEnvT a_cenv)
// {
//   bool isProd = (a_cenv == MQTEnvT::Prod);

//   vector<SecDefS> const& eqtSrcs = SecDefs_Eqt;

//   vector<SecDefS>& targ          = // NB: Ref!
//     utxx::likely(isProd)
//     ? SecDefs_All_Prod
//     : SecDefs_All_Test;

//   if (utxx::unlikely(targ.empty()))
//   {
//     // Construct and fill in this vector (XXX: non-tradable Baskets are curr-
//     // ently omitted):
//     targ.reserve(eqtSrcs.size());

//     for (SecDefS const& defs: eqtSrcs)
//       targ.push_back(defs);

//   }
//   return targ;
// }

} // End namespace TDAmeritrade
} // End namespace MAQUETTE
