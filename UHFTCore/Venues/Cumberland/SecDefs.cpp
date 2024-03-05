// vim:ts=2:et
//===========================================================================//
//                     "Venues/Cumberland/SecDefs.cpp":                      //
//===========================================================================//
#include "Venues/Cumberland/SecDefs.h"

namespace MAQUETTE
{
namespace Cumberland
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // Last updated: 2020-12-03
  std::vector<SecDefS> const SecDefs
  {
    {  1, "BCH_USD", "bchusd", "", "MRCXXX", "Cumberland", "", "SPOT", "",
      "BCH",  "USD", 'B', 1.0, 1.0, 1, 1e-2, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    {  2, "BTC_USD", "btcusd", "", "MRCXXX", "Cumberland", "", "SPOT", "",
      "BTC",  "USD", 'B', 1.0, 1.0, 1, 1e-2, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    {  3, "EOS_USD", "eosusd", "", "MRCXXX", "Cumberland", "", "SPOT", "",
      "EOS",  "USD", 'B', 1.0, 1.0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    {  4, "ETH_USD", "ethusd", "", "MRCXXX", "Cumberland", "", "SPOT", "",
      "ETH",  "USD", 'B', 1.0, 1.0, 1, 1e-2, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    {  5, "LTC_USD", "ltcusd", "", "MRCXXX", "Cumberland", "", "SPOT", "",
      "LTC",  "USD", 'B', 1.0, 1.0, 1, 1e-2, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    {  7, "USDC_USD", "usdcusd", "", "MRCXXX", "Cumberland", "", "SPOT", "",
      "USDC", "USD", 'B', 1.0, 1.0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    // {  8, "XRP_USD", "xrpusd", "", "MRCXXX", "Cumberland", "", "SPOT", "",
    //   "XRP",  "USD", 'B', 1.0, 1.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    // },
    {  9, "ETH_BTC", "ethbtc", "", "MRCXXX", "Cumberland", "", "SPOT", "",
      "ETH",  "BTC", 'B', 1.0, 1.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    }

    // for certification only
    // {  10, "BTC_USD", "btcusd2", "", "MRCXXX", "Cumberland", "", "SPOT2", "",
    //   "BTC", "USD", 'B', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    // },
    // {  11, "BTC_USD", "btcusd3", "", "MRCXXX", "Cumberland", "", "SPOT3", "",
    //   "BTC", "USD", 'B', 1.0, 1.0, 1, 0.01, 'A', 1.0, 0, 0, 0.0, 0, ""
    // },
  };

} // End namespace Cumberland
} // End namespace MAQUETTE
