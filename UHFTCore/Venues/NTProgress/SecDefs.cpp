// vim:ts=2:et
//===========================================================================//
//                        "Venues/NTProgress/SecDefs.cpp":                   //
//                              As of: 02-Jun-2016                           //
//===========================================================================//
#include "Venues/NTProgress/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace NTProgress
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // NB: For TenorTime, we use 17:00 EST/EDT which is 21:00 or 22:00 UTC; use
  // the latter, which is 79200 sec intra-day:
  //
  vector<SecDefS> const SecDefs
  {
    { 0, "USD/RUB_TOD", "", "", "MRCXXX", "NTProgress", "", "TOD", "",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB_TOM", "", "", "MRCXXX", "NTProgress", "", "TOM", "",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB_TOD", "", "", "MRCXXX", "NTProgress", "", "TOD", "",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB_TOM", "", "", "MRCXXX", "NTProgress", "", "TOM", "",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/USD",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "EUR", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/GBP",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "EUR", "GBP", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/CHF",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "EUR", "CHF", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CHF",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "EUR", "CHF", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/USD",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "GBP", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/JPY",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "USD", "JPY", 'A', 1.0, 1.0, 1, 0.001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/JPY",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "EUR", "JPY", 'A', 1.0, 1.0, 1, 0.001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/USD",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "AUD", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/USD",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "NZD", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CNY",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "USD", "CNY", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CAD_TOD", "", "", "MRCXXX", "NTProgress", "", "TOD", "",
      "USD", "CAD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CAD",     "", "", "MRCXXX", "NTProgress", "", "TOM", "",
      "USD", "CAD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/AUD",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "EUR", "AUD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/JPY",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "AUD", "JPY", 'A', 1.0, 1.0, 1, 0.001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/JPY",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "GBP", "JPY", 'A', 1.0, 1.0, 1, 0.001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/JPY",     "", "", "MRCXXX", "NTProgress", "", "SPOT", "",
      "CHF", "JPY", 'A', 1.0, 1.0, 1, 0.001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    }
  };
} // End namespace NTProgress
} // End namespace MAQUETTE
