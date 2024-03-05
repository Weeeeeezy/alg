// vim:ts=2:et
//===========================================================================//
//                        "Venues/MICEX/SecDefs.cpp":                        //
//                 Obtaining ALL MICEX "SecDefS" objs at once                //
//===========================================================================//
#include "Venues/MICEX/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace MICEX
{
  //-------------------------------------------------------------------------//
  // "SecDefs_All":                                                          //
  //-------------------------------------------------------------------------//
  // Initially NULL, created and filled in on demand. Will contain ALL MICEX
  // src SecDefs:
  //
  static vector<SecDefS> SecDefs_All;   // Initially empty

  //-------------------------------------------------------------------------//
  // "GetSecDefs_All":                                                       //
  //-------------------------------------------------------------------------//
  vector<SecDefS> const& GetSecDefs_All()
  {
    if (utxx::unlikely(SecDefs_All.empty()))
    {
      // Construct and fill in this vector (XXX: non-tradable Baskets are curr-
      // ently omitted):
      SecDefs_All.reserve(SecDefs_FX.size() + SecDefs_EQ.size());

      for (SecDefS const& defs: SecDefs_FX)
        SecDefs_All.push_back(defs);

      for (SecDefS const& defs: SecDefs_EQ)
        SecDefs_All.push_back(defs);
    }
    return SecDefs_All;
  }
} // End namespace MICEX
} // End namespace MAQUETTE
