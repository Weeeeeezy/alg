// vim:ts=2:et
//===========================================================================//
//                     "Venues/Binance/SecDefs_FutC.cpp":                    //
//===========================================================================//
#include "Venues/Binance/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace Binance
{
  //=========================================================================//
  // "SecDefs_FutC":                                                         //
  //=========================================================================//
  // Coin-Settled (i.e. Inversely-Settled) Futures (Perpetual or Quarterly):
  // Last updated: 2021-02-07
  //
  vector<SecDefS> const SecDefs_FutC
  {
    { 0, "BTCUSD_PERP", "btcusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "BTC", "USD", 'A', 100, 1, 1, 0.1, 'A', 1.0,
      21001225, 28800, 0.0, 0, "BTCUSD"
    },
    { 0, "BTCUSD_210326", "btcusd_210326", "", "MRCXXX", "Binance", "FUT",
      "H1", "",
      "BTC", "USD", 'A', 100, 1, 1, 0.1, 'A', 1.0,
      20210326, 28800, 0.0, 0, "BTCUSD"
    },
    { 0, "BTCUSD_210625", "btcusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "BTC", "USD", 'A', 100, 1, 1, 0.1, 'A', 1.0,
      20210625, 28800, 0.0, 0, "BTCUSD"
    },
    { 0, "ETHUSD_PERP", "ethusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "ETH", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      21001225, 28800, 0.0, 0, "ETHUSD"
    },
    { 0, "ETHUSD_210326", "ethusd_210326", "", "MRCXXX", "Binance", "FUT",
      "H1", "", "ETH", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      20210326, 28800, 0.0, 0, "ETHUSD"
    },
    { 0, "ETHUSD_210625", "ethusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "ETH", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      20210625, 28800, 0.0, 0, "ETHUSD"
    },
    { 0, "LINKUSD_PERP", "linkusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "LINK", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "LINKUSD"
    },
    { 0, "BNBUSD_PERP", "bnbusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "BNB", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "BNBUSD"
    },
    { 0, "TRXUSD_PERP", "trxusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "TRX", "USD", 'A', 10, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "TRXUSD"
    },
    { 0, "DOTUSD_PERP", "dotusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "DOT", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "DOTUSD"
    },
    { 0, "ADAUSD_PERP", "adausd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "ADA", "USD", 'A', 10, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "ADAUSD"
    },
    { 0, "EOSUSD_PERP", "eosusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "EOS", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "EOSUSD"
    },
    { 0, "LTCUSD_PERP", "ltcusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "LTC", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      21001225, 28800, 0.0, 0, "LTCUSD"
    },
    { 0, "BCHUSD_PERP", "bchusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "BCH", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      21001225, 28800, 0.0, 0, "BCHUSD"
    },
    { 0, "XRPUSD_PERP", "xrpusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "XRP", "USD", 'A', 10, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "XRPUSD"
    },
    { 0, "ETCUSD_PERP", "etcusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "ETC", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "ETCUSD"
    },
    { 0, "ADAUSD_210326", "adausd_210326", "", "MRCXXX", "Binance", "FUT", "H1",
      "", "ADA", "USD", 'A', 10, 1, 1, 0.00001, 'A', 1.0,
      20210326, 28800, 0.0, 0, "ADAUSD"
    },
    { 0, "LINKUSD_210326", "linkusd_210326", "", "MRCXXX", "Binance", "FUT",
      "H1", "", "LINK", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210326, 28800, 0.0, 0, "LINKUSD"
    },
    { 0, "FILUSD_PERP", "filusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "FIL", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "FILUSD"
    },
    { 0, "BNBUSD_210326", "bnbusd_210326", "", "MRCXXX", "Binance", "FUT", "H1",
      "", "BNB", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210326, 28800, 0.0, 0, "BNBUSD"
    },
    { 0, "DOTUSD_210326", "dotusd_210326", "", "MRCXXX", "Binance", "FUT", "H1",
      "", "DOT", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210326, 28800, 0.0, 0, "DOTUSD"
    },
    { 0, "XRPUSD_210326", "xrpusd_210326", "", "MRCXXX", "Binance", "FUT", "H1",
      "", "XRP", "USD", 'A', 10, 1, 1, 0.0001, 'A', 1.0,
      20210326, 28800, 0.0, 0, "XRPUSD"
    },
    { 0, "LTCUSD_210326", "ltcusd_210326", "", "MRCXXX", "Binance", "FUT", "H1",
      "", "LTC", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      20210326, 28800, 0.0, 0, "LTCUSD"
    },
    { 0, "BCHUSD_210326", "bchusd_210326", "", "MRCXXX", "Binance", "FUT", "H1",
      "", "BCH", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      20210326, 28800, 0.0, 0, "BCHUSD"
    },
    { 0, "FILUSD_210326", "filusd_210326", "", "MRCXXX", "Binance", "FUT", "H1",
      "", "FIL", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210326, 28800, 0.0, 0, "FILUSD"
    },
    { 0, "EGLDUSD_PERP", "egldusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "EGLD", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "EGLDUSD"
    },
    { 0, "ADAUSD_210625", "adausd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "ADA", "USD", 'A', 10, 1, 1, 0.00001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "ADAUSD"
    },
    { 0, "LINKUSD_210625", "linkusd_210625", "", "MRCXXX", "Binance", "FUT",
      "M1", "", "LINK", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "LINKUSD"
    },
    { 0, "BCHUSD_210625", "bchusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "BCH", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      20210625, 28800, 0.0, 0, "BCHUSD"
    },
    { 0, "ETCUSD_210625", "etcusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "ETC", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "ETCUSD"
    },
    { 0, "EOSUSD_210625", "eosusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "EOS", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "EOSUSD"
    },
    { 0, "FILUSD_210625", "filusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "FIL", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "FILUSD"
    },
    { 0, "DOTUSD_210625", "dotusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "DOT", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "DOTUSD"
    },
    { 0, "TRXUSD_210625", "trxusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "TRX", "USD", 'A', 10, 1, 1, 0.00001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "TRXUSD"
    },
    { 0, "XRPUSD_210625", "xrpusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "XRP", "USD", 'A', 10, 1, 1, 0.0001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "XRPUSD"
    },
    { 0, "LTCUSD_210625", "ltcusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "LTC", "USD", 'A', 10, 1, 1, 0.01, 'A', 1.0,
      20210625, 28800, 0.0, 0, "LTCUSD"
    },
    { 0, "BNBUSD_210625", "bnbusd_210625", "", "MRCXXX", "Binance", "FUT", "M1",
      "", "BNB", "USD", 'A', 10, 1, 1, 0.001, 'A', 1.0,
      20210625, 28800, 0.0, 0, "BNBUSD"
    },
    { 0, "DOGEUSD_PERP", "dogeusd_perp", "", "MRCXXX", "Binance", "FUT", "", "",
      "DOGE", "USD", 'A', 10, 1, 1, 0.000001, 'A', 1.0,
      21001225, 28800, 0.0, 0, "DOGEUSD"
    },
  };
} // End namespace Binance
} // End namespace MAQUETTE
