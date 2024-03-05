// vim:ts=2:et
//===========================================================================//
//                        "Venues/Huobi/SecDefs.cpp":                        //
//===========================================================================//
#include "Venues/Huobi/SecDefs.h"

namespace MAQUETTE
{
namespace Huobi
{
  //-------------------------------------------------------------------------//
  // "GetSecDefs" (for a given Asset Class):                                 //
  //-------------------------------------------------------------------------//
  std::vector<SecDefS> const&  GetSecDefs(AssetClassT::type a_ascl)
  {
    switch (a_ascl) {
    case AssetClassT::Spt:
      return SecDefs_Spt;
    case AssetClassT::Fut:
      return SecDefs_Fut;
    case AssetClassT::CSwp:
      return SecDefs_CSwp;
    case AssetClassT::USwp:
      return SecDefs_USwp;
    default:
      // Other AssetClasses are not implemented yet:
      throw utxx::badarg_error
            ("Huobi::GetSecDefs: UnSupported AssetClass=",
             AssetClassT(a_ascl).c_str());
    }
  }

} // End namespace Huobi
} // End namespace MAQUETTE
