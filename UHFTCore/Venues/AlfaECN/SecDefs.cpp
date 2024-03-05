// vim:ts=2:et
//===========================================================================//
//                        "Venues/AlfaECN/SecDefs.cpp":                      //
//                             As of: 15-Jan-2016                            //
//===========================================================================//
#include "Venues/AlfaECN/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace AlfaECN
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // NB: For TenorTime, we use 17:00 EST/EDT which is 21:00 or 22:00 UTC; use
  // the latter, which is 79200 sec intra-day:
  //
  vector<SecDefS> const SecDefs
  {
    { 0, "USD/RUB_TOD", "", "",  "MRCXXX", "AlfaECN", "", "TOD",  "",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/RUB_TOM", "", "",  "MRCXXX", "AlfaECN", "", "TOM",  "",
      "USD", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB_TOD", "", "",  "MRCXXX", "AlfaECN", "", "TOD",  "",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/RUB_TOM", "", "",  "MRCXXX", "AlfaECN", "", "TOM",  "",
      "EUR", "RUB", 'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/USD",     "", "",  "MRCXXX", "AlfaECN", "", "SPOT", "",
      "EUR", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "EUR/GBP",     "", "",  "MRCXXX", "AlfaECN", "", "SPOT", "",
      "EUR", "GBP", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "GBP/USD",     "", "",  "MRCXXX", "AlfaECN", "", "SPOT", "",
      "GBP", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/JPY",     "", "",  "MRCXXX", "AlfaECN", "", "SPOT", "",
      "USD", "JPY", 'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "AUD/USD",     "", "",  "MRCXXX", "AlfaECN", "", "SPOT", "",
      "AUD", "USD", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    },
    { 0, "USD/CNY",     "", "",  "MRCXXX", "AlfaECN", "", "SPOT", "",
      "USD", "CNY", 'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0, ""
    }
  };
} // End namespace AlfaECN
} // End namespace MAQUETTE
