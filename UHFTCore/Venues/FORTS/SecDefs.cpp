// vim:ts=2:et
//===========================================================================//
//                        "Venues/FORTS/SecDefs.cpp":                        //
//                 Obtaining ALL FORTS "SecDefS" objs at once                //
//===========================================================================//
#include "Venues/FORTS/SecDefs.h"
#include <utxx/error.hpp>
using namespace std;

namespace MAQUETTE
{
namespace FORTS
{
  //-------------------------------------------------------------------------//
  // "SecDefs_All":                                                          //
  //-------------------------------------------------------------------------//
  // Initially NULL, created and filled in on demand. Will contain ALL FORTS
  // src SecDefs:
  //
  static vector<SecDefS> SecDefs_All_Prod;    // Initially empty
  static vector<SecDefS> SecDefs_All_Test;    // ditto

  //-------------------------------------------------------------------------//
  // "GetSecDefs" (for a given Asset Class):                                 //
  //-------------------------------------------------------------------------//
  std::vector<SecDefS> const&  GetSecDefs
    (AssetClassT::type a_ascl, MQTEnvT a_cenv)
  {
    bool isProd = IsProdEnv(a_cenv);
    switch (a_ascl)
    {
    case AssetClassT::Fut:
      return utxx::likely(isProd)
      ? SecDefs_Fut_Prod
      : SecDefs_Fut_Test;

    case AssetClassT::Opt:
      return utxx::likely(isProd)
      ? SecDefs_Opt_Prod
      : SecDefs_Opt_Test;

    case AssetClassT::Ind:
      return utxx::likely(isProd)
      ? SecDefs_Ind_Prod
      : SecDefs_Ind_Test;

    default:
      // Other AssetClasses are not implemented yet:
      throw utxx::badarg_error
            ("FORTS::GetSecDefs: UnSupported AssetClass=",
             AssetClassT(a_ascl).c_str());
    }
  }

  //-------------------------------------------------------------------------//
  // "GetSecDefs_All":                                                       //
  //-------------------------------------------------------------------------//
  vector<SecDefS> const& GetSecDefs_All(MQTEnvT a_cenv)
  {
    bool isProd = IsProdEnv(a_cenv);

    vector<SecDefS> const& futSrcs =
      utxx::likely(isProd)
      ? SecDefs_Fut_Prod
      : SecDefs_Fut_Test;

    vector<SecDefS> const& optSrcs =
      utxx::likely(isProd)
      ? SecDefs_Opt_Prod
      : SecDefs_Opt_Test;

    vector<SecDefS>& targ          = // NB: Ref!
      utxx::likely(isProd)
      ? SecDefs_All_Prod
      : SecDefs_All_Test;

    if (utxx::unlikely(targ.empty()))
    {
      // Construct and fill in this vector (XXX: non-tradable Baskets are curr-
      // ently omitted):
      targ.reserve(futSrcs.size() + optSrcs.size());

      for (SecDefS const& defs: futSrcs)
        targ.push_back(defs);

      for (SecDefS const& defs: optSrcs)
        targ.push_back(defs);
    }
    return targ;
  }
} // End namespace FORTS
} // End namespace MAQUETTE
