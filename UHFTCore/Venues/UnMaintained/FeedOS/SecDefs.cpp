// vim:ts=2:et
//===========================================================================//
//                        "Venues/FeedOS/SecDefs.cpp":                       //
//  Statically-Configured Instruments: QuantHouse / S&P Capital IP / FeedOS  //
//                            As of: 26-Nov-2015                             //
//===========================================================================//
#include "Venues/FeedOS/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace FeedOS
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // XXX: This is sample sub-set only, of ICE Energy Futures provided by FeedOS:
  // Clearing time is (not very precisely) 23:00 LDN (UTC+0h or UTC+1h), so use
  // 23:00 UTC = 82800 sec) as the latest:
  //
  vector<SecDefS> const SecDefs
  {
    {
      432761356,
      "220837",
      "BRN",
      "Brent Crude Futures - North Sea - Cal 16",
      "FCECXX",
      "ICE", "IFEU",
      "", "", "",
      "USD",
      'A', 1.0, 1000.0, 1, 0.01, 'A', 1.0,
      20151216, 82800,
      0.0,   0, ""
    },
    {
      432761353,
      "220840",
      "BRN",
      "Brent Crude Futures - North Sea - Jan16",
      "FCECXX",
      "ICE", "IFEU",
      "", "", "",
      "USD",
      'A', 1.0, 1000.0, 1, 0.01, 'A', 1.0,
      20151216, 82800,
      0.0,   0, ""
    },
    {
      432761346,
      "220847",
      "BRN",
      "Brent Crude Futures - North Sea - Q1 16",
      "FCECXX",
      "ICE", "IFEU",
      "", "", "",
      "USD",
      'A', 1.0, 1000.0, 1, 0.01, 'A', 1.0,
      20151216, 82800,
      0.0,   0, ""
    }
  };
} // End namespace FeedOS
} // End namespace MAQUETTE
