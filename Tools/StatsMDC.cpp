// vim:ts=2:et
//===========================================================================//
//                             "Tools/StatsMDC.cpp":                         //
//===========================================================================//
#include "InfraStruct/PersistMgr.h"
#include "Connectors/EConnector_MktData.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <iostream>
#include <cstring>

using namespace std;
using namespace MAQUETTE;

namespace
{
  //=========================================================================//
  // "DisplayStatsGen":                                                      //
  //=========================================================================//
  // ST: FixedShM or FixedMMF
  //
  template<typename ST>
  inline void DisplayStatsGen
  (
    char const*   a_seg_name,
    unsigned long a_base_addr
  )
  {
    // Map the segment in:
    PersistMgr<ST> PM(a_seg_name, a_base_addr, 0, nullptr);

    // Get ptrs to all Stats:
    using ECMD = EConnector_MktData;
    constexpr char const* statNames[5]
    {
      ECMD::SocketStatsON (),
      ECMD::ProcStatsON   (),
      ECMD::UpdtStatsON   (),
      ECMD::StratStatsON  (),
      ECMD::OverAllStatsON()
    };

    for (unsigned i = 0; i < 5; ++i)
    {
      // Get the "LatencyStats" object ptr:
      auto   res =
        PM.GetSegm()->template find<ECMD::LatencyStats>(statNames[i]);
      assert(res.second == 1);

      ECMD::LatencyStats const* ls = res.first;
      assert(ls != nullptr);

      // Output the object:
      cout << (*ls) << endl;
    }
  }
}
//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  //-------------------------------------------------------------------------//
  // Get the Command-Line Args:                                              //
  //-------------------------------------------------------------------------//
  if (utxx::unlikely
     ((argc != 2  && argc   != 3) ||
      strcmp(argv[1], "-h") == 0  || strcmp(argv[1], "--help") == 0))
  {
    cerr << "PARAMETERS: {shm|mmf}:Name [BaseAddr]" << endl;
    return 1;
  }

  char const*   segName  = argv[1];
  unsigned long baseAddr =
    (argc == 3)
    ? strtoul(argv[2], nullptr, 16)
    : 0;        // Will use the default one

  // Is it a ShM Segment Name or a File?
  bool isShM =  (strncmp(segName, "shm:", 4) == 0);
  bool isMMF =  (strncmp(segName, "mmf:", 4) == 0);

  if (utxx::unlikely(isShM != !isMMF))
  {
    cerr << "ERROR: URL prefix must be 'shm:' or 'mmf:'" << endl;
    return 1;
  }
  // Remove the URL prefix from "segName":
  segName += 4;

  //-------------------------------------------------------------------------//
  // Now Do It:                                                              //
  //-------------------------------------------------------------------------//
  // NB: "segName" will be automatically converted to "string":
  try
  {
    if (isShM)
      DisplayStatsGen<FixedShM>(segName, baseAddr);
    else
      DisplayStatsGen<FixedMMF>(segName, baseAddr);
  }
  catch (std::exception const& exc)
  {
    cerr << "EXCEPTION: " <<   exc.what() << endl;
    return 1;
  }
  return 0;
}
