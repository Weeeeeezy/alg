// vim:ts=2:et
//===========================================================================//
//                         "Venues/FXBA/SecDefs.cpp":                        //
//                             As of: 15-Jan-2016                            //
//===========================================================================//
#include "Venues/FXBA/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace FXBA
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // NB: For TenorTime, we use 17:00 EST/EDT which is 21:00 or 22:00 UTC; use
  // the latter, which is 79200 sec intra-day:
  //
  vector<SecDefS> const SecDefs
  {
    { 0, "USD/RUB_TOD", "", "", "MRCXXX", "FXBA", "", "TOD",  "", "USD", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/RUB_TOM", "", "", "MRCXXX", "FXBA", "", "TOM",  "", "USD", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/RUB_TOD", "", "", "MRCXXX", "FXBA", "", "TOD",  "", "EUR", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/RUB_TOM", "", "", "MRCXXX", "FXBA", "", "TOM",  "", "EUR", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/USD",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "EUR", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/GBP",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "EUR", "GBP",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/USD",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "GBP", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/JPY",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "USD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/USD",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "AUD", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CNY",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "USD", "CNY",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XAU/USD",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "XAU", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XAG/USD",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "XAG", "USD",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XPD/USD",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "XPD", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XPT/USD",     "", "", "MRCXXX", "FXBA", "", "SPOT", "", "XPT", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    }
  };
} // End namespace FXBA
} // End namespace MAQUETTE
