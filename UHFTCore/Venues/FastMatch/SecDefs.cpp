// vim:ts=2:et
//===========================================================================//
//                      "Venues/FastMatch/SecDefs
//===========================================================================//
#include "Venues/FastMatch/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace FastMatch
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // NB:
  // (*) Here we use Tenor=SPOT for (T+2) and Tenor=TOM for (T+1), for consist-
  //     ency with other Venues;
  // (*) In FastMatch, all Qtys are actually given in "cents" (always assuming
  //     2 decimal points), ie each LotSize is 0.01  --  but we cannot install
  //     fractional LotSzs in SecDefs, -- so use 1 instead, and apply the 0.01
  //     factor in the MktData parsing code;
  // (*) For TenorTime, we use 17:00 EST/EDT which is 21:00 or 22:00 UTC; use
  //     the latter, which is 79200 sec intra-day:
  //
  vector<SecDefS> const SecDefs
  {
    //-----------------------------------------------------------------------//
    // Ccy Pairs and Precious Metals:                                        //
    //-----------------------------------------------------------------------//
    { 0, "AUD/CAD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "CAD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/CHF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "CHF",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "HKD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/NZD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "NZD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/SGD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "SGD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "USD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "AUD/ZAR", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "AUD", "ZAR",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/CHF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "CHF",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "HKD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/MXN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "MXN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/PLN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "PLN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/SGD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "SGD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CAD/ZAR", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CAD", "ZAR",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/CZK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "CZK",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "HKD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/HUF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "HUF",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/ILS", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "ILS",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/MXN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "MXN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/PLN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "PLN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/SGD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "SGD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/TRY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "TRY",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CHF/ZAR", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CHF", "ZAR",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "CZK/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "CZK", "JPY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "DKK/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "DKK", "HKD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "DKK/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "DKK", "JPY",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "DKK/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "DKK", "NOK",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "DKK/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "DKK", "SEK",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/AUD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "AUD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/CAD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "CAD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/CHF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "CHF",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/CZK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "CZK",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/GBP", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "GBP",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "HKD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/HUF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "HUF",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/ILS", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "ILS",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/MXN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "MXN",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/NZD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "NZD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/PLN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "PLN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/RUB", "", "", "MRCXXX", "FastMatch", "", "TOM",  "", "EUR", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/SGD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "SGD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/TRY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "TRY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "USD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "EUR/ZAR", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "EUR", "ZAR",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/AUD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "AUD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/CAD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "CAD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/CHF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "CHF",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/CZK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "CZK",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "HKD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/HUF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "HUF",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/ILS", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "ILS",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/MXN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "MXN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/NZD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "NZD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/PLN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "PLN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/SGD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "SGD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/TRY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "TRY",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "USD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "GBP/ZAR", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "GBP", "ZAR",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "HKD/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "HKD", "JPY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "HUF/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "HUF", "JPY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "MXN/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "MXN", "JPY",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NOK/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NOK", "HKD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NOK/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NOK", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NOK/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NOK", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/CAD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "CAD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/CHF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "CHF",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "HKD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/PLN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "PLN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/SGD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "SGD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "USD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "NZD/ZAR", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "NZD", "ZAR",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "PLN/HUF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "PLN", "HUF",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "PLN/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "PLN", "JPY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SEK/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SEK", "HKD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SEK/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SEK", "JPY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SGD/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SGD", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SGD/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SGD", "HKD",
      'A', 1.0, 1.0, 1, 0.000001, 'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SGD/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SGD", "JPY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SGD/MXN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SGD", "MXN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SGD/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SGD", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "SGD/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "SGD", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "TRY/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "TRY", "JPY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CAD", "", "", "MRCXXX", "FastMatch", "", "TOM",  "", "USD", "CAD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CHF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "CHF",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CNH", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "CNH",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/CZK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "CZK",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/DKK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "DKK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/GHS", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "GHS",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/HKD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "HKD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/HUF", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "HUF",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/ILS", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "ILS",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/KES", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "KES",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/MXN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "MXN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/NOK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "NOK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/PLN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "PLN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/RON", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "RON",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/RUB", "", "", "MRCXXX", "FastMatch", "", "TOM",  "", "USD", "RUB",
      'A', 1.0, 1.0, 1, 0.0001,   'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/SEK", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "SEK",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/SGD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "SGD",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/TRY", "", "", "MRCXXX", "FastMatch", "", "TOM",  "", "USD", "TRY",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/ZAR", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "ZAR",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "USD/ZMW", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "USD", "ZMW",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XAG/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "XAG", "USD",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XAU/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "XAU", "USD",
      'A', 1.0, 1.0, 1, 0.01,     'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XPD/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "XPD", "USD",
      'A', 1.0, 1.0, 1, 0.01,     'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "XPT/USD", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "XPT", "USD",
      'A', 1.0, 1.0, 1, 0.01,     'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "ZAR/JPY", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "ZAR", "JPY",
      'A', 1.0, 1.0, 1, 0.001,    'A', 1.0, 0, 79200, 0.0, 0,  ""
    },
    { 0, "ZAR/MXN", "", "", "MRCXXX", "FastMatch", "", "SPOT", "", "ZAR", "MXN",
      'A', 1.0, 1.0, 1, 0.00001,  'A', 1.0, 0, 79200, 0.0, 0,  ""
    }
  };
} // End namespace FastMatch
} // End namespace MAQUETTE
