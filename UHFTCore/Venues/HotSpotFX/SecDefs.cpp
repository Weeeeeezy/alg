// vim:ts=2:et
//===========================================================================//
//                      "Venues/HotSpotFX/SecDefs.cpp":                      //
//===========================================================================//
#include "Venues/HotSpotFX/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace HotSpotFX
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // NB:
  // (*) Here we use Tenor=SPOT for (T+2) and Tenor=TOM for (T+1), for consist-
  //     ency with other Venues;
  // (*) For TenorTime, we use 17:00 EST/EDT which is 21:00 or 22:00 UTC; use
  //     the latter, which is 79200 sec intra-day;
  // (*) List last updated on 2020-06-24:
  //
  vector<SecDefS> const SecDefs
  {
    //-----------------------------------------------------------------------//
    // CLS Ccy Pairs:                                                        //
    //-----------------------------------------------------------------------//
    // AUD
    { 0, "AUD/CAD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "AUD", "CAD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "AUD/CHF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "AUD", "CHF",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "AUD/HKD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "AUD", "HKD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "AUD/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "AUD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "AUD/NZD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "AUD", "NZD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "AUD/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "AUD", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // CAD
    { 0, "CAD/CHF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "CAD", "CHF",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "CAD/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "CAD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "CAD/MXN", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "CAD", "MXN",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // CHF
    { 0, "CHF/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "CHF", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "CHF/NOK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "CHF", "NOK",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "CHF/SEK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "CHF", "SEK",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // EUR
    { 0, "EUR/AUD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "AUD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/CAD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "CAD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/CHF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "CHF",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/DKK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "DKK",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/GBP", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "GBP",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/HKD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "HKD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/MXN", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "MXN",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/NOK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "NOK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/NZD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "NZD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/SEK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "SEK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/ZAR", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "ZAR",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // GBP
    { 0, "GBP/AUD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "AUD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/CAD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "CAD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/CHF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "CHF",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/MXN", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "MXN",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/NOK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "NOK",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/NZD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "NZD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/SEK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "SEK",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // HKD
    { 0, "HKD/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "HKD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // NOK
    { 0, "NOK/SEK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "NOK", "SEK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // NZD
    { 0, "NZD/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "NZD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "NZD/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "NZD", "USD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // USD
    { 0, "USD/CAD", "", "", "MRCXXX", "HotSpotFX", "", "TOM",  "", "USD", "CAD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/CHF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "CHF",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/DKK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "DKK",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/HKD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "HKD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/ILS", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "ILS",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/MXN", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "MXN",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/NOK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "NOK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/SEK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "SEK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/SGD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "SGD",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/ZAR", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "ZAR",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // ZAR
    { 0, "ZAR/JPY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "ZAR", "JPY",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    //-----------------------------------------------------------------------//
    // Precious Metals:                                                      //
    //-----------------------------------------------------------------------//
    { 0, "LPD/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "LPD", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "LPT/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "LPT", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "XAG/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "XAG", "USD",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "XAU/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "XAU", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "XPD/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "XPD", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "XPT/USD", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "XPT", "USD",
      'A', 1.0, 1.0, 1, 0.01,    'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    //-----------------------------------------------------------------------//
    // Non-CLS Ccy Pairs:                                                    //
    //-----------------------------------------------------------------------//
    // EUR
    { 0, "EUR/CNH", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "CNH",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/CZK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "CZK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/HUF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "HUF",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/PLN", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "PLN",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/RUB", "", "", "MRCXXX", "HotSpotFX", "", "TOM",  "", "EUR", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "EUR/TRY", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "EUR", "TRY",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // GBP
    { 0, "GBP/CZK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "CZK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/HUF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "HUF",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "GBP/PLN", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "GBP", "PLN",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    // USD
    { 0, "USD/CNH", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "CNH",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/CZK", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "CZK",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/HUF", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "HUF",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/PLN", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "PLN",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/RUB", "", "", "MRCXXX", "HotSpotFX", "", "TOM",  "", "USD", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,  'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/THB", "", "", "MRCXXX", "HotSpotFX", "", "SPOT", "", "USD", "THB",
      'A', 1.0, 1.0, 1, 0.001,   'A', 1.0, 0, 79200, 0.0, 0,   ""
    },
    { 0, "USD/TRY", "", "", "MRCXXX", "HotSpotFX", "", "TOM",  "", "USD", "TRY",
      'A', 1.0, 1.0, 1, 0.00001, 'A', 1.0, 0, 79200, 0.0, 0,   ""
    }
  };
} // End namespace HotSpotFX
} // End namespace MAQUETTE