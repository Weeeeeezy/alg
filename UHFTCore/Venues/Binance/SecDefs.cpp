// vim:ts=2:et
//===========================================================================//
//                       "Venues/Binance/SecDefs.cpp":                       //
//===========================================================================//
#include "Venues/Binance/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace Binance
{
  //-------------------------------------------------------------------------//
  // "SecDefs_All":                                                          //
  //-------------------------------------------------------------------------//
  // Initially NULL, created and filled in on demand. Will contain ALL Binance
  // src SecDefs:
  //
  static vector<SecDefS> SecDefs_All_Prod;    // Initially empty
  static vector<SecDefS> SecDefs_All_Test;    // ditto

  //-------------------------------------------------------------------------//
  // "GetSecDefs" (for a given Asset Class):                                 //
  //-------------------------------------------------------------------------//
  std::vector<SecDefS> const&  GetSecDefs(AssetClassT::type a_ascl)
  {
    switch (a_ascl)
    {
    case AssetClassT::FutT:
      return  SecDefs_FutT;
    case AssetClassT::FutC:
      return  SecDefs_FutC;
    case AssetClassT::Spt:
      return  SecDefs_Spt;
    default:
      // Other AssetClasses are not implemented yet:
      throw utxx::badarg_error
            ("Binance::GetSecDefs: UnSupported AssetClass=",
             AssetClassT(a_ascl).c_str());
    }
  }

  //-------------------------------------------------------------------------//
  // "GetSecDefs_All":                                                       //
  //-------------------------------------------------------------------------//
  vector<SecDefS> const& GetSecDefs_All(MQTEnvT a_cenv)
  {
    bool isProd = (a_cenv == MQTEnvT::Prod);

    vector<SecDefS> const& futSrcsT = SecDefs_FutT;
    vector<SecDefS> const& futSrcsC = SecDefs_FutC;
    vector<SecDefS> const& sptSrcs  = SecDefs_Spt;

    vector<SecDefS>& targ           = // NB: Ref!
      utxx::likely(isProd)
      ? SecDefs_All_Prod
      : SecDefs_All_Test;

    if (utxx::unlikely(targ.empty()))
    {
      // Construct and fill in this vector (XXX: non-tradable Baskets are curr-
      // ently omitted):
      targ.reserve(futSrcsT.size() + futSrcsC.size() + sptSrcs.size());

      for (SecDefS const& defs: futSrcsT)
        targ.push_back(defs);

      for (SecDefS const& defs: futSrcsC)
        targ.push_back(defs);

      for (SecDefS const& defs: sptSrcs)
        targ.push_back(defs);
    }
    return targ;
  }
} // End namespace Binance
} // End namespace MAQUETTE
