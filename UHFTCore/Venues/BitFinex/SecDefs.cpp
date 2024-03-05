// vim:ts=2:et
//===========================================================================//
//                      "Venues/BitFinex/SecDefs.cpp":                       //
//===========================================================================//
#include "Venues/BitFinex/SecDefs.h"

namespace MAQUETTE
{
namespace BitFinex
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  std::vector<SecDefS> const SecDefs
  {
    { 0, "tBTCUSD", "BTCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "USD", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLTCUSD", "LTCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LTC", "USD", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLTCBTC", "LTCBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LTC", "BTC", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETHUSD", "ETHUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETH", "USD", 'A', 1.0, 0.04, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETHBTC", "ETHBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETH", "BTC", 'A', 1.0, 0.04, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETCBTC", "ETCBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETC", "BTC", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETCUSD", "ETCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETC", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRRTUSD", "RRTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RRT", "USD", 'A', 1.0, 186.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRRTBTC", "RRTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RRT", "BTC", 'A', 1.0, 186.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZECUSD", "ZECUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZEC", "USD", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZECBTC", "ZECBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZEC", "BTC", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXMRUSD", "XMRUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XMR", "USD", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXMRBTC", "XMRBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XMR", "BTC", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDSHUSD", "DSHUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DSH", "USD", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDSHBTC", "DSHBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DSH", "BTC", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTCEUR", "BTCEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "EUR", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTCJPY", "BTCJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "JPY", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXRPUSD", "XRPUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XRP", "USD", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXRPBTC", "XRPBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XRP", "BTC", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOTUSD", "IOTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOT", "USD", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOTBTC", "IOTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOT", "BTC", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOTETH", "IOTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOT", "ETH", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEOSUSD", "EOSUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EOS", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEOSBTC", "EOSBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EOS", "BTC", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEOSETH", "EOSETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EOS", "ETH", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSANUSD", "SANUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SAN", "USD", 'A', 1.0, 26.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSANBTC", "SANBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SAN", "BTC", 'A', 1.0, 26.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSANETH", "SANETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SAN", "ETH", 'A', 1.0, 26.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOMGUSD", "OMGUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OMG", "USD", 'A', 1.0, 8.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOMGBTC", "OMGBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OMG", "BTC", 'A', 1.0, 8.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOMGETH", "OMGETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OMG", "ETH", 'A', 1.0, 8.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNEOUSD", "NEOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEO", "USD", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNEOBTC", "NEOBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEO", "BTC", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNEOETH", "NEOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEO", "ETH", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETPUSD", "ETPUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETP", "USD", 'A', 1.0, 16.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETPBTC", "ETPBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETP", "BTC", 'A', 1.0, 16.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETPETH", "ETPETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETP", "ETH", 'A', 1.0, 16.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tQTMUSD", "QTMUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "QTM", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tQTMBTC", "QTMBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "QTM", "BTC", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tQTMETH", "QTMETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "QTM", "ETH", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAVTUSD", "AVTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AVT", "USD", 'A', 1.0, 52.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAVTBTC", "AVTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AVT", "BTC", 'A', 1.0, 52.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAVTETH", "AVTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AVT", "ETH", 'A', 1.0, 52.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEDOUSD", "EDOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EDO", "USD", 'A', 1.0, 24.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEDOBTC", "EDOBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EDO", "BTC", 'A', 1.0, 24.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEDOETH", "EDOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EDO", "ETH", 'A', 1.0, 24.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTGUSD", "BTGUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTG", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTGBTC", "BTGBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTG", "BTC", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDATUSD", "DATUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAT", "USD", 'A', 1.0, 368.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDATBTC", "DATBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAT", "BTC", 'A', 1.0, 368.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDATETH", "DATETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAT", "ETH", 'A', 1.0, 368.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tQSHUSD", "QSHUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "QSH", "USD", 'A', 1.0, 110.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tQSHBTC", "QSHBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "QSH", "BTC", 'A', 1.0, 110.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tQSHETH", "QSHETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "QSH", "ETH", 'A', 1.0, 110.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tYYWUSD", "YYWUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "YYW", "USD", 'A', 1.0, 534.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tYYWBTC", "YYWBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "YYW", "BTC", 'A', 1.0, 534.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tYYWETH", "YYWETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "YYW", "ETH", 'A', 1.0, 534.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGNTUSD", "GNTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GNT", "USD", 'A', 1.0, 150.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGNTBTC", "GNTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GNT", "BTC", 'A', 1.0, 150.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGNTETH", "GNTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GNT", "ETH", 'A', 1.0, 150.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSNTUSD", "SNTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SNT", "USD", 'A', 1.0, 564.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSNTBTC", "SNTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SNT", "BTC", 'A', 1.0, 564.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSNTETH", "SNTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SNT", "ETH", 'A', 1.0, 564.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOTEUR", "IOTEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOT", "EUR", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBATUSD", "BATUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BAT", "USD", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBATBTC", "BATBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BAT", "BTC", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBATETH", "BATETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BAT", "ETH", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMNAUSD", "MNAUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MNA", "USD", 'A', 1.0, 206.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMNABTC", "MNABTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MNA", "BTC", 'A', 1.0, 206.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMNAETH", "MNAETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MNA", "ETH", 'A', 1.0, 206.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFUNUSD", "FUNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FUN", "USD", 'A', 1.0, 1748.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFUNBTC", "FUNBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FUN", "BTC", 'A', 1.0, 1748.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFUNETH", "FUNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FUN", "ETH", 'A', 1.0, 1748.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZRXUSD", "ZRXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZRX", "USD", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZRXBTC", "ZRXBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZRX", "BTC", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZRXETH", "ZRXETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZRX", "ETH", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTNBUSD", "TNBUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TNB", "USD", 'A', 1.0, 2980.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTNBBTC", "TNBBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TNB", "BTC", 'A', 1.0, 2980.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTNBETH", "TNBETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TNB", "ETH", 'A', 1.0, 2980.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSPKUSD", "SPKUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SPK", "USD", 'A', 1.0, 1266.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSPKBTC", "SPKBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SPK", "BTC", 'A', 1.0, 1266.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSPKETH", "SPKETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SPK", "ETH", 'A', 1.0, 1266.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRXUSD", "TRXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRX", "USD", 'A', 1.0, 400.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRXBTC", "TRXBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRX", "BTC", 'A', 1.0, 400.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRXETH", "TRXETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRX", "ETH", 'A', 1.0, 400.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRCNUSD", "RCNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RCN", "USD", 'A', 1.0, 130.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRCNBTC", "RCNBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RCN", "BTC", 'A', 1.0, 130.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRCNETH", "RCNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RCN", "ETH", 'A', 1.0, 130.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRLCUSD", "RLCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RLC", "USD", 'A', 1.0, 14.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRLCBTC", "RLCBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RLC", "BTC", 'A', 1.0, 14.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRLCETH", "RLCETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RLC", "ETH", 'A', 1.0, 14.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAIDUSD", "AIDUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AID", "USD", 'A', 1.0, 572.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAIDBTC", "AIDBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AID", "BTC", 'A', 1.0, 572.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAIDETH", "AIDETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AID", "ETH", 'A', 1.0, 572.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSNGUSD", "SNGUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SNG", "USD", 'A', 1.0, 854.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSNGBTC", "SNGBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SNG", "BTC", 'A', 1.0, 854.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSNGETH", "SNGETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SNG", "ETH", 'A', 1.0, 854.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tREPUSD", "REPUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "REP", "USD", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tREPBTC", "REPBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "REP", "BTC", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tREPETH", "REPETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "REP", "ETH", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tELFUSD", "ELFUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ELF", "USD", 'A', 1.0, 100.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tELFBTC", "ELFBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ELF", "BTC", 'A', 1.0, 100.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tELFETH", "ELFETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ELF", "ETH", 'A', 1.0, 100.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNECUSD", "NECUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEC", "USD", 'A', 1.0, 84.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNECBTC", "NECBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEC", "BTC", 'A', 1.0, 84.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNECETH", "NECETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEC", "ETH", 'A', 1.0, 84.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTCGBP", "BTCGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "GBP", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETHEUR", "ETHEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETH", "EUR", 'A', 1.0, 0.04, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETHJPY", "ETHJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETH", "JPY", 'A', 1.0, 0.04, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETHGBP", "ETHGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETH", "GBP", 'A', 1.0, 0.04, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNEOEUR", "NEOEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEO", "EUR", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNEOJPY", "NEOJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEO", "JPY", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNEOGBP", "NEOGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NEO", "GBP", 'A', 1.0, 0.6, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEOSEUR", "EOSEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EOS", "EUR", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEOSJPY", "EOSJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EOS", "JPY", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEOSGBP", "EOSGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EOS", "GBP", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOTJPY", "IOTJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOT", "JPY", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOTGBP", "IOTGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOT", "GBP", 'A', 1.0, 32.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOSUSD", "IOSUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOS", "USD", 'A', 1.0, 952.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOSBTC", "IOSBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOS", "BTC", 'A', 1.0, 952.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIOSETH", "IOSETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IOS", "ETH", 'A', 1.0, 952.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAIOUSD", "AIOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AIO", "USD", 'A', 1.0, 100.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAIOBTC", "AIOBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AIO", "BTC", 'A', 1.0, 100.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAIOETH", "AIOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AIO", "ETH", 'A', 1.0, 100.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tREQUSD", "REQUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "REQ", "USD", 'A', 1.0, 366.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tREQBTC", "REQBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "REQ", "BTC", 'A', 1.0, 366.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tREQETH", "REQETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "REQ", "ETH", 'A', 1.0, 366.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRDNUSD", "RDNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RDN", "USD", 'A', 1.0, 44.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRDNBTC", "RDNBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RDN", "BTC", 'A', 1.0, 44.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRDNETH", "RDNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RDN", "ETH", 'A', 1.0, 44.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLRCUSD", "LRCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LRC", "USD", 'A', 1.0, 234.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLRCBTC", "LRCBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LRC", "BTC", 'A', 1.0, 234.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLRCETH", "LRCETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LRC", "ETH", 'A', 1.0, 234.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWAXUSD", "WAXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WAX", "USD", 'A', 1.0, 186.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWAXBTC", "WAXBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WAX", "BTC", 'A', 1.0, 186.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWAXETH", "WAXETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WAX", "ETH", 'A', 1.0, 186.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDAIUSD", "DAIUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAI", "USD", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDAIBTC", "DAIBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAI", "BTC", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDAIETH", "DAIETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAI", "ETH", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAGIUSD", "AGIUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AGI", "USD", 'A', 1.0, 296.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAGIBTC", "AGIBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AGI", "BTC", 'A', 1.0, 296.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAGIETH", "AGIETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AGI", "ETH", 'A', 1.0, 296.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBFTUSD", "BFTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BFT", "USD", 'A', 1.0, 374.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBFTBTC", "BFTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BFT", "BTC", 'A', 1.0, 374.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBFTETH", "BFTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BFT", "ETH", 'A', 1.0, 374.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMTNUSD", "MTNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MTN", "USD", 'A', 1.0, 1810.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMTNBTC", "MTNBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MTN", "BTC", 'A', 1.0, 1810.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMTNETH", "MTNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MTN", "ETH", 'A', 1.0, 1810.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tODEUSD", "ODEUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ODE", "USD", 'A', 1.0, 112.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tODEBTC", "ODEBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ODE", "BTC", 'A', 1.0, 112.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tODEETH", "ODEETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ODE", "ETH", 'A', 1.0, 112.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tANTUSD", "ANTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ANT", "USD", 'A', 1.0, 12.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tANTBTC", "ANTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ANT", "BTC", 'A', 1.0, 12.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tANTETH", "ANTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ANT", "ETH", 'A', 1.0, 12.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTHUSD", "DTHUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTH", "USD", 'A', 1.0, 2118.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTHBTC", "DTHBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTH", "BTC", 'A', 1.0, 2118.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTHETH", "DTHETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTH", "ETH", 'A', 1.0, 2118.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMITUSD", "MITUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MIT", "USD", 'A', 1.0, 622.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMITBTC", "MITBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MIT", "BTC", 'A', 1.0, 622.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMITETH", "MITETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MIT", "ETH", 'A', 1.0, 622.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSTJUSD", "STJUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "STJ", "USD", 'A', 1.0, 60.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSTJBTC", "STJBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "STJ", "BTC", 'A', 1.0, 60.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSTJETH", "STJETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "STJ", "ETH", 'A', 1.0, 60.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXLMUSD", "XLMUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XLM", "USD", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXLMEUR", "XLMEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XLM", "EUR", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXLMJPY", "XLMJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XLM", "JPY", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXLMGBP", "XLMGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XLM", "GBP", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXLMBTC", "XLMBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XLM", "BTC", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXLMETH", "XLMETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XLM", "ETH", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXVGUSD", "XVGUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XVG", "USD", 'A', 1.0, 1298.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXVGEUR", "XVGEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XVG", "EUR", 'A', 1.0, 1298.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXVGJPY", "XVGJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XVG", "JPY", 'A', 1.0, 1298.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXVGGBP", "XVGGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XVG", "GBP", 'A', 1.0, 1298.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXVGBTC", "XVGBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XVG", "BTC", 'A', 1.0, 1298.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXVGETH", "XVGETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XVG", "ETH", 'A', 1.0, 1298.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBCIUSD", "BCIUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BCI", "USD", 'A', 1.0, 180.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBCIBTC", "BCIBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BCI", "BTC", 'A', 1.0, 180.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMKRUSD", "MKRUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MKR", "USD", 'A', 1.0, 0.02, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMKRBTC", "MKRBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MKR", "BTC", 'A', 1.0, 0.02, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMKRETH", "MKRETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MKR", "ETH", 'A', 1.0, 0.02, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tKNCUSD", "KNCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "KNC", "USD", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tKNCBTC", "KNCBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "KNC", "BTC", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tKNCETH", "KNCETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "KNC", "ETH", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPOAUSD", "POAUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "POA", "USD", 'A', 1.0, 330.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPOABTC", "POABTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "POA", "BTC", 'A', 1.0, 330.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPOAETH", "POAETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "POA", "ETH", 'A', 1.0, 330.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEVTUSD", "EVTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EVT", "USD", 'A', 1.0, 16656.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLYMUSD", "LYMUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LYM", "USD", 'A', 1.0, 1538.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLYMBTC", "LYMBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LYM", "BTC", 'A', 1.0, 1538.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLYMETH", "LYMETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LYM", "ETH", 'A', 1.0, 1538.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUTKUSD", "UTKUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UTK", "USD", 'A', 1.0, 248.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUTKBTC", "UTKBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UTK", "BTC", 'A', 1.0, 248.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUTKETH", "UTKETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UTK", "ETH", 'A', 1.0, 248.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVEEUSD", "VEEUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VEE", "USD", 'A', 1.0, 3548.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVEEBTC", "VEEBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VEE", "BTC", 'A', 1.0, 3548.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVEEETH", "VEEETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VEE", "ETH", 'A', 1.0, 3548.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDADUSD", "DADUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAD", "USD", 'A', 1.0, 154.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDADBTC", "DADBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAD", "BTC", 'A', 1.0, 154.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDADETH", "DADETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DAD", "ETH", 'A', 1.0, 154.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tORSUSD", "ORSUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ORS", "USD", 'A', 1.0, 154.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tORSBTC", "ORSBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ORS", "BTC", 'A', 1.0, 154.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tORSETH", "ORSETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ORS", "ETH", 'A', 1.0, 154.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAUCUSD", "AUCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AUC", "USD", 'A', 1.0, 1852.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAUCBTC", "AUCBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AUC", "BTC", 'A', 1.0, 1852.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAUCETH", "AUCETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AUC", "ETH", 'A', 1.0, 1852.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPOYUSD", "POYUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "POY", "USD", 'A', 1.0, 226.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPOYBTC", "POYBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "POY", "BTC", 'A', 1.0, 226.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPOYETH", "POYETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "POY", "ETH", 'A', 1.0, 226.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFSNUSD", "FSNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FSN", "USD", 'A', 1.0, 22.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFSNBTC", "FSNBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FSN", "BTC", 'A', 1.0, 22.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFSNETH", "FSNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FSN", "ETH", 'A', 1.0, 22.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCBTUSD", "CBTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CBT", "USD", 'A', 1.0, 164.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCBTBTC", "CBTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CBT", "BTC", 'A', 1.0, 164.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCBTETH", "CBTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CBT", "ETH", 'A', 1.0, 164.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZCNUSD", "ZCNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZCN", "USD", 'A', 1.0, 134.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZCNBTC", "ZCNBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZCN", "BTC", 'A', 1.0, 134.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZCNETH", "ZCNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZCN", "ETH", 'A', 1.0, 134.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSENUSD", "SENUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SEN", "USD", 'A', 1.0, 10088.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSENBTC", "SENBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SEN", "BTC", 'A', 1.0, 10088.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSENETH", "SENETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SEN", "ETH", 'A', 1.0, 10088.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNCAUSD", "NCAUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NCA", "USD", 'A', 1.0, 6666.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNCABTC", "NCABTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NCA", "BTC", 'A', 1.0, 6666.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNCAETH", "NCAETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NCA", "ETH", 'A', 1.0, 6666.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCNDUSD", "CNDUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CND", "USD", 'A', 1.0, 618.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCNDBTC", "CNDBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CND", "BTC", 'A', 1.0, 618.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCNDETH", "CNDETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CND", "ETH", 'A', 1.0, 618.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCTXUSD", "CTXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CTX", "USD", 'A', 1.0, 50.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCTXBTC", "CTXBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CTX", "BTC", 'A', 1.0, 50.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCTXETH", "CTXETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CTX", "ETH", 'A', 1.0, 50.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPAIUSD", "PAIUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PAI", "USD", 'A', 1.0, 312.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPAIBTC", "PAIBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PAI", "BTC", 'A', 1.0, 312.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSEEUSD", "SEEUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SEE", "USD", 'A', 1.0, 8312.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSEEBTC", "SEEBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SEE", "BTC", 'A', 1.0, 8312.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSEEETH", "SEEETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SEE", "ETH", 'A', 1.0, 8312.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tESSUSD", "ESSUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ESS", "USD", 'A', 1.0, 12326.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tESSBTC", "ESSBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ESS", "BTC", 'A', 1.0, 12326.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tESSETH", "ESSETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ESS", "ETH", 'A', 1.0, 12326.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tATMUSD", "ATMUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ATM", "USD", 'A', 1.0, 26310.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tATMBTC", "ATMBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ATM", "BTC", 'A', 1.0, 26310.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tATMETH", "ATMETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ATM", "ETH", 'A', 1.0, 26310.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tHOTUSD", "HOTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "HOT", "USD", 'A', 1.0, 1276.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tHOTBTC", "HOTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "HOT", "BTC", 'A', 1.0, 1276.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tHOTETH", "HOTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "HOT", "ETH", 'A', 1.0, 1276.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTAUSD", "DTAUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTA", "USD", 'A', 1.0, 16756.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTABTC", "DTABTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTA", "BTC", 'A', 1.0, 16756.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTAETH", "DTAETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTA", "ETH", 'A', 1.0, 16756.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIQXUSD", "IQXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IQX", "USD", 'A', 1.0, 3878.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIQXBTC", "IQXBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IQX", "BTC", 'A', 1.0, 3878.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIQXEOS", "IQXEOS", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IQX", "EOS", 'A', 1.0, 3878.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWPRUSD", "WPRUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WPR", "USD", 'A', 1.0, 854.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWPRBTC", "WPRBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WPR", "BTC", 'A', 1.0, 854.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWPRETH", "WPRETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WPR", "ETH", 'A', 1.0, 854.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZILUSD", "ZILUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZIL", "USD", 'A', 1.0, 708.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZILBTC", "ZILBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZIL", "BTC", 'A', 1.0, 708.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZILETH", "ZILETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZIL", "ETH", 'A', 1.0, 708.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBNTUSD", "BNTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BNT", "USD", 'A', 1.0, 18.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBNTBTC", "BNTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BNT", "BTC", 'A', 1.0, 18.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBNTETH", "BNTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BNT", "ETH", 'A', 1.0, 18.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tABSUSD", "ABSUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ABS", "USD", 'A', 1.0, 688.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tABSETH", "ABSETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ABS", "ETH", 'A', 1.0, 688.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXRAUSD", "XRAUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XRA", "USD", 'A', 1.0, 1236.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXRAETH", "XRAETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XRA", "ETH", 'A', 1.0, 1236.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMANUSD", "MANUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MAN", "USD", 'A', 1.0, 56.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMANETH", "MANETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MAN", "ETH", 'A', 1.0, 56.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBBNUSD", "BBNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BBN", "USD", 'A', 1.0, 17596.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBBNETH", "BBNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BBN", "ETH", 'A', 1.0, 17596.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNIOUSD", "NIOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NIO", "USD", 'A', 1.0, 2438.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tNIOETH", "NIOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "NIO", "ETH", 'A', 1.0, 2438.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDGXUSD", "DGXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DGX", "USD", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDGXETH", "DGXETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DGX", "ETH", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVETUSD", "VETUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VET", "USD", 'A', 1.0, 1006.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVETBTC", "VETBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VET", "BTC", 'A', 1.0, 1006.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVETETH", "VETETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VET", "ETH", 'A', 1.0, 1006.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUTNUSD", "UTNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UTN", "USD", 'A', 1.0, 2472.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUTNETH", "UTNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UTN", "ETH", 'A', 1.0, 2472.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTKNUSD", "TKNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TKN", "USD", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTKNETH", "TKNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TKN", "ETH", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGOTUSD", "GOTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GOT", "USD", 'A', 1.0, 38.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGOTEUR", "GOTEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GOT", "EUR", 'A', 1.0, 38.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGOTETH", "GOTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GOT", "ETH", 'A', 1.0, 38.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXTZUSD", "XTZUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XTZ", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXTZBTC", "XTZBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XTZ", "BTC", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCNNUSD", "CNNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CNN", "USD", 'A', 1.0, 124798.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCNNETH", "CNNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CNN", "ETH", 'A', 1.0, 124798.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBOXUSD", "BOXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BOX", "USD", 'A', 1.0, 1830.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBOXETH", "BOXETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BOX", "ETH", 'A', 1.0, 1830.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRXEUR", "TRXEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRX", "EUR", 'A', 1.0, 400.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRXGBP", "TRXGBP", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRX", "GBP", 'A', 1.0, 400.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRXJPY", "TRXJPY", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRX", "JPY", 'A', 1.0, 400.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMGOUSD", "MGOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MGO", "USD", 'A', 1.0, 476.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMGOETH", "MGOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MGO", "ETH", 'A', 1.0, 476.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRTEUSD", "RTEUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RTE", "USD", 'A', 1.0, 6210.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRTEETH", "RTEETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RTE", "ETH", 'A', 1.0, 6210.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tYGGUSD", "YGGUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "YGG", "USD", 'A', 1.0, 23192.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tYGGETH", "YGGETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "YGG", "ETH", 'A', 1.0, 23192.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMLNUSD", "MLNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MLN", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMLNETH", "MLNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MLN", "ETH", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWTCUSD", "WTCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WTC", "USD", 'A', 1.0, 12.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWTCETH", "WTCETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WTC", "ETH", 'A', 1.0, 12.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCSXUSD", "CSXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CSX", "USD", 'A', 1.0, 62.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCSXETH", "CSXETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CSX", "ETH", 'A', 1.0, 62.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOMNUSD", "OMNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OMN", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOMNBTC", "OMNBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OMN", "BTC", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tINTUSD", "INTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "INT", "USD", 'A', 1.0, 234.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tINTETH", "INTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "INT", "ETH", 'A', 1.0, 234.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDRNUSD", "DRNUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DRN", "USD", 'A', 1.0, 224.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDRNETH", "DRNETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DRN", "ETH", 'A', 1.0, 224.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPNKUSD", "PNKUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PNK", "USD", 'A', 1.0, 726.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPNKETH", "PNKETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PNK", "ETH", 'A', 1.0, 726.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDGBUSD", "DGBUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DGB", "USD", 'A', 1.0, 774.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDGBBTC", "DGBBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DGB", "BTC", 'A', 1.0, 774.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBSVUSD", "BSVUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BSV", "USD", 'A', 1.0, 0.06, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBSVBTC", "BSVBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BSV", "BTC", 'A', 1.0, 0.06, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBABUSD", "BABUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BAB", "USD", 'A', 1.0, 0.02, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBABBTC", "BABBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BAB", "BTC", 'A', 1.0, 0.02, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWLOUSD", "WLOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WLO", "USD", 'A', 1.0, 946.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWLOXLM", "WLOXLM", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WLO", "XLM", 'A', 1.0, 946.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVLDUSD", "VLDUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VLD", "USD", 'A', 1.0, 1840.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVLDETH", "VLDETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VLD", "ETH", 'A', 1.0, 1840.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tENJUSD", "ENJUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ENJ", "USD", 'A', 1.0, 82.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tENJETH", "ENJETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ENJ", "ETH", 'A', 1.0, 82.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tONLUSD", "ONLUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ONL", "USD", 'A', 1.0, 264.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tONLETH", "ONLETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ONL", "ETH", 'A', 1.0, 264.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRBTUSD", "RBTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RBT", "USD", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRBTBTC", "RBTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RBT", "BTC", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUSTUSD", "USTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "USDT", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEUTEUR", "EUTEUR", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EUT", "EUR", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEUTUSD", "EUTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EUT", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGSDUSD", "GSDUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GSD", "USD", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUDCUSD", "UDCUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UDC", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTSDUSD", "TSDUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TSD", "USD", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPAXUSD", "PAXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PAX", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRIFUSD", "RIFUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RIF", "USD", 'A', 1.0, 134.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRIFBTC", "RIFBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RIF", "BTC", 'A', 1.0, 134.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPASUSD", "PASUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PAS", "USD", 'A', 1.0, 2850.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPASETH", "PASETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PAS", "ETH", 'A', 1.0, 2850.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVSYUSD", "VSYUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VSY", "USD", 'A', 1.0, 126.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tVSYBTC", "VSYBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "VSY", "BTC", 'A', 1.0, 126.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZRXDAI", "ZRXDAI", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZRX", "DAI", 'A', 1.0, 28.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tMKRDAI", "MKRDAI", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "MKR", "DAI", 'A', 1.0, 0.02, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOMGDAI", "OMGDAI", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OMG", "DAI", 'A', 1.0, 8.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTTUSD", "BTTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTT", "USD", 'A', 1.0, 17772.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTTBTC", "BTTBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTT", "BTC", 'A', 1.0, 17772.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTCUST", "BTCUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "USDT", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETHUST", "ETHUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETH", "USDT", 'A', 1.0, 0.04, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCLOUSD", "CLOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CLO", "USD", 'A', 1.0, 8064.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCLOBTC", "CLOBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CLO", "BTC", 'A', 1.0, 8064.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIMPUSD", "IMPUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IMP", "USD", 'A', 1.0, 1272.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tIMPETH", "IMPETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "IMP", "ETH", 'A', 1.0, 1272.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLTCUST", "LTCUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LTC", "USDT", 'A', 1.0, 0.2, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEOSUST", "EOSUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EOS", "USDT", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBABUST", "BABUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BAB", "USDT", 'A', 1.0, 0.02, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSCRUSD", "SCRUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SCR", "USD", 'A', 1.0, 2564.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSCRETH", "SCRETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SCR", "ETH", 'A', 1.0, 2564.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGNOUSD", "GNOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GNO", "USD", 'A', 1.0, 0.4, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGNOETH", "GNOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GNO", "ETH", 'A', 1.0, 0.4, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGENUSD", "GENUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GEN", "USD", 'A', 1.0, 66.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGENETH", "GENETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GEN", "ETH", 'A', 1.0, 66.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tATOUSD", "ATOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ATO", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tATOBTC", "ATOBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ATO", "BTC", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tATOETH", "ATOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ATO", "ETH", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWBTUSD", "WBTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WBT", "USD", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXCHUSD", "XCHUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XCH", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEUSUSD", "EUSUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EUS", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tWBTETH", "WBTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "WBT", "ETH", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tXCHETH", "XCHETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "XCH", "ETH", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tEUSETH", "EUSETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "EUS", "ETH", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLEOUSD", "LEOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LEO", "USD", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLEOBTC", "LEOBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LEO", "BTC", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLEOUST", "LEOUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LEO", "USDT", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLEOEOS", "LEOEOS", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LEO", "EOS", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLEOETH", "LEOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LEO", "ETH", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tASTUSD", "ASTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AST", "USD", 'A', 1.0, 184.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tASTETH", "ASTETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AST", "ETH", 'A', 1.0, 184.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFOAUSD", "FOAUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FOA", "USD", 'A', 1.0, 246.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFOAETH", "FOAETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FOA", "ETH", 'A', 1.0, 246.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUFRUSD", "UFRUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UFR", "USD", 'A', 1.0, 260.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUFRETH", "UFRETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UFR", "ETH", 'A', 1.0, 260.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZBTUSD", "ZBTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZBT", "USD", 'A', 1.0, 26.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tZBTUST", "ZBTUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ZBT", "USDT", 'A', 1.0, 26.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOKBUSD", "OKBUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OKB", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUSKUSD", "USKUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "USK", "USD", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGTXUSD", "GTXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GTX", "USD", 'A', 1.0, 8.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tKANUSD", "KANUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "KAN", "USD", 'A', 1.0, 2870.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOKBUST", "OKBUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OKB", "USDT", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOKBETH", "OKBETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OKB", "ETH", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tOKBBTC", "OKBBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "OKB", "BTC", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUSKUST", "USKUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "USK", "USDT", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUSKETH", "USKETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "USK", "ETH", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUSKBTC", "USKBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "USK", "BTC", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUSKEOS", "USKEOS", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "USK", "EOS", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tGTXUST", "GTXUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "GTX", "USDT", 'A', 1.0, 8.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tKANUST", "KANUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "KAN", "USDT", 'A', 1.0, 2870.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAMPUSD", "AMPUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AMP", "USD", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tALGUSD", "ALGUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ALG", "USD", 'A', 1.0, 22.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tALGBTC", "ALGBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ALG", "BTC", 'A', 1.0, 22.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tALGUST", "ALGUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ALG", "USDT", 'A', 1.0, 22.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTCXCH", "BTCXCH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "XCH", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSWMUSD", "SWMUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SWM", "USD", 'A', 1.0, 196.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tSWMETH", "SWMETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "SWM", "ETH", 'A', 1.0, 196.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRIUSD", "TRIUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRI", "USD", 'A', 1.0, 1892.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTRIETH", "TRIETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TRI", "ETH", 'A', 1.0, 1892.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLOOUSD", "LOOUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LOO", "USD", 'A', 1.0, 340.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tLOOETH", "LOOETH", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "LOO", "ETH", 'A', 1.0, 340.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAMPUST", "AMPUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AMP", "USDT", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDUSK:USD", "DUSK:USD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DUSK", "USD", 'A', 1.0, 148.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDUSK:BTC", "DUSK:BTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DUSK", "BTC", 'A', 1.0, 148.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUOSUSD", "UOSUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UOS", "USD", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUOSBTC", "UOSBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UOS", "BTC", 'A', 1.0, 116.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRRBUSD", "RRBUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RRB", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tRRBUST", "RRBUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "RRB", "USDT", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTXUSD", "DTXUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTX", "USD", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tDTXUST", "DTXUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "DTX", "USDT", 'A', 1.0, 2.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tAMPBTC", "AMPBTC", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "AMP", "BTC", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFTTUSD", "FTTUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FTT", "USD", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tFTTUST", "FTTUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "FTT", "USDT", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tPAXUST", "PAXUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "PAX", "USDT", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUDCUST", "UDCUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "UDC", "USDT", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tTSDUST", "TSDUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "TSD", "USDT", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTC:CNHT", "BTC:CNHT", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "CNHT", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tUST:CNHT", "UST:CNHT", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "USDT", "CNHT", 'A', 1.0, 4.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCNH:CNHT", "CNH:CNHT", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CNH", "CNHT", 'A', 1.0, 6.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCHZUSD", "CHZUSD", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CHZ", "USD", 'A', 1.0, 646.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tCHZUST", "CHZUST", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "CHZ", "USDT", 'A', 1.0, 646.0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tBTCF0:USTF0", "BTCF0:USTF0", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "BTC", "USDT", 'A', 1.0, 0.0008, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 0, "tETHF0:USTF0", "ETHF0:USTF0", "", "MRCXXX", "BitFinex", "", "SPOT", "",
      "ETH", "USDT", 'A', 1.0, 0.04, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    }
  };
} // End namespace BitFinex
} // End namespace MAQUETTE
