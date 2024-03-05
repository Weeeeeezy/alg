// vim:ts=2:et
//===========================================================================//
//                        "Venues/FeedOS/Configs.cpp":                       //
//    Configs for QuantHouse / S&P Capital IQ (aka FeedOS) MktData Feeds     //
//===========================================================================//
#include "Connectors/FeedOS/Configs.hpp"

using namespace std;

namespace MAQUETTE
{
namespace FeedOS
{
  //=========================================================================//
  // Actual Configs:                                                         //
  //=========================================================================//
  // Map:  Key => Config
  // where Key is like "PREFIX-FeedOS":
  //
  map<string, Config> const Configs
  {
    //-----------------------------------------------------------------------//
    { "MP-ICE-Energy-FeedOS",
    //-----------------------------------------------------------------------//
      { "172.16.112.197", 6041, "otkritie_test", "lH3sZPdf" }
    }
  };
} // End namespace FeedOS
} // End namespace MAQUETTE
