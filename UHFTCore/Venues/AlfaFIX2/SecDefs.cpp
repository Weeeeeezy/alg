// vim:ts=2:et
//===========================================================================//
//                        "Venues/AlfaFIX2/SecDefs.cpp":                     //
//                             As of: 13-Oct-2015                            //
//===========================================================================//
#include "Venues/AlfaFIX2/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace AlfaFIX2
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // NB: For TenorTime, we use 17:00 EST/EDT which is 21:00 or 22:00 UTC; use
  // the latter, which is 79200 sec intra-day:
  //
  vector<SecDefS> const SecDefs
  {
    //-----------------------------------------------------------------------//
    // Spot Instruments:                                                     //
    //-----------------------------------------------------------------------//
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOD",  "",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.0005,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.0005,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOD",  "",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.0005,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.0005,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/USD",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "EUR", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/GBP",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "EUR", "GBP", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "GBP/USD",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "GBP", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/JPY",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "USD", "JPY", 'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "AUD/USD",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "AUD", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/CNY",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "USD", "CNY", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "XAU/USD",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "XAU", "USD", 'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "XAG/USD",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "XAG", "USD", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "XPD/USD",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "XPD", "USD", 'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "XPT/USD",      "", "", "MRCXXX", "AlfaFIX2", "", "SPOT", "",
      "XPT", "USD", 'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    //-----------------------------------------------------------------------//
    // Swap Instruments:                                                     //
    //-----------------------------------------------------------------------//
    // For USD/RUB:
    //
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOD",  "TOM",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "W1",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "W2",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M1",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M2",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M3",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M6",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M9",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "Y1",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "MAR",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "JUN",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "SEP",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "DEC",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    // For EUR/EUB:
    //
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOD",  "TOM",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "SPOT",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "W1",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "W2",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M1",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M2",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M3",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M6",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "M9",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB",      "", "", "MRCXXX", "AlfaFIX2", "", "TOM",  "Y1",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    }
  };
} // End namespace AlfaFIX2
} // End namespace MAQUETTE