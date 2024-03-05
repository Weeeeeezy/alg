// vim:ts=2:et
//===========================================================================//
//                        "Venues/BitMEX/SecDefs.cpp":                       //
//===========================================================================//
#include "Venues/BitMEX/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace BitMEX
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  // Last updated: 2021-02-07
  std::vector<SecDefS> const SecDefs
  {
    { 0, "XBTUSD", "xbtusd", "", "FFWCSX",
      "BitMEX", "", "SPOT", "",
      "BTC", "USD", 'B', 1.0, 1, 1, 0.5, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 1, "ETHUSD", "ethusd", "", "FFWCSX",
      "BitMEX", "", "SPOT", "",
      "ETH", "USD", 'A', 1.0, 1, 1, 0.05, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 2, "XRPUSD", "xrpusd", "", "FFWCSX",
      "BitMEX", "", "SPOT", "",
      "XRP", "USD", 'A', 1.0, 1, 1, 0.0001, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 3, "BCHUSD", "bchusd", "", "FFWCSX",
      "BitMEX", "", "SPOT", "",
      "BCH", "USD", 'A', 1.0, 1, 1, 0.05, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 4, "LTCUSD", "ltcusd", "", "FFWCSX",
      "BitMEX", "", "SPOT", "",
      "LTC", "USD", 'A', 1.0, 1, 1, 0.01, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 5, "XBTH21", "xbth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "BTC", "USD", 'A', 1.0, 1, 1, 0.5, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 6, "LINKUSDT", "linkusdt", "", "FFWCSX",
      "BitMEX", "", "SPOT", "",
      "LINK", "USDT", 'A', 1.0, 1, 1, 0.0005, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 7, "XBTM21", "xbtm21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "BTC", "USD", 'A', 1.0, 1, 1, 0.5, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 8, "ETHH21", "ethh21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "ETH", "XBT", 'A', 1.0, 1, 1, 1e-05, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 9, "LTCH21", "ltch21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "LTC", "XBT", 'A', 1.0, 1, 1, 5e-06, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 10, "XRPH21", "xrph21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "XRP", "XBT", 'A', 1.0, 1, 1, 1e-08, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 11, "BCHH21", "bchh21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "BCH", "XBT", 'A', 1.0, 1, 1, 1e-05, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 12, "ADAH21", "adah21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "ADA", "XBT", 'A', 1.0, 1, 1, 1e-08, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 13, "EOSH21", "eosh21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "EOS", "XBT", 'A', 1.0, 1, 1, 1e-07, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 14, "TRXH21", "trxh21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "TRX", "XBT", 'A', 1.0, 1, 1, 1e-08, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 15, "ETHUSDH21", "ethusdh21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "ETH", "USD", 'A', 1.0, 1, 1, 0.05, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 16, "ADAUSDTH21", "adausdth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "ADA", "USDT", 'A', 1.0, 1, 1, 1e-05, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 17, "EOSUSDTH21", "eosusdth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "EOS", "USDT", 'A', 1.0, 1, 1, 0.0005, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 18, "LINKUSDTH21", "linkusdth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "LINK", "USDT", 'A', 1.0, 1, 1, 0.0005, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 19, "XTZUSDTH21", "xtzusdth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "XTZ", "USDT", 'A', 1.0, 1, 1, 0.0005, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 20, "BNBUSDTH21", "bnbusdth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "BNB", "USDT", 'A', 1.0, 1, 1, 0.0005, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 21, "YFIUSDTH21", "yfiusdth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "YFI", "USDT", 'A', 1.0, 1, 1, 0.5, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 22, "DOTUSDTH21", "dotusdth21", "", "FFCCSX",
      "BitMEX", "", "SPOT", "",
      "DOT", "USDT", 'A', 1.0, 1, 1, 0.0005, 'A',1.0, 0, 0, 0.0, 0, ""
    },
    { 23, "DOGEUSDT", "dogeusdt", "", "FFWCSX",
      "BitMEX", "", "SPOT", "",
      "DOGE", "USDT", 'A', 1.0, 1, 1, 1e-05, 'A',1.0, 0, 0, 0.0, 0, ""
    },
  };
}  // End namespace BitMEX
}  // End namespace MAQUETTE
