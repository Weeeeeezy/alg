// vim:ts=2:et
//===========================================================================//
//                      "Venues/Binance/SecDefs_FutT.cpp":                   //
//===========================================================================//
#include "Venues/Binance/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace Binance
{
  //=========================================================================//
  // "SecDefs_FutT":                                                         //
  //=========================================================================//
  // USDT-settled Perpetual and Quarterly Futures:
  // NB: Empty Tenor is for Perpetual Futures; for them, expiration date is set
  // to 21001225:
  // Last updated: 2021-02-07
  //
  vector<SecDefS> const SecDefs_FutT
  {
    { 0, "BTCUSDT", "btcusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BTC", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ETHUSDT", "ethusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ETH", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BCHUSDT", "bchusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BCH", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "XRPUSDT", "xrpusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "XRP", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "EOSUSDT", "eosusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "EOS", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "LTCUSDT", "ltcusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "LTC", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "TRXUSDT", "trxusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "TRX", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ETCUSDT", "etcusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ETC", "USDT", 'A', 1, 0.01, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "LINKUSDT", "linkusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "LINK", "USDT", 'A', 1, 0.01, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "XLMUSDT", "xlmusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "XLM", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ADAUSDT", "adausdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ADA", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "XMRUSDT", "xmrusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "XMR", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "DASHUSDT", "dashusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "DASH", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ZECUSDT", "zecusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ZEC", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "XTZUSDT", "xtzusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "XTZ", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BNBUSDT", "bnbusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BNB", "USDT", 'A', 1, 0.01, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ATOMUSDT", "atomusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ATOM", "USDT", 'A', 1, 0.01, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ONTUSDT", "ontusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ONT", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "IOTAUSDT", "iotausdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "IOTA", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BATUSDT", "batusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BAT", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "VETUSDT", "vetusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "VET", "USDT", 'A', 1, 1, 1, 0.000001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "NEOUSDT", "neousdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "NEO", "USDT", 'A', 1, 0.01, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "QTUMUSDT", "qtumusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "QTUM", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "IOSTUSDT", "iostusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "IOST", "USDT", 'A', 1, 1, 1, 0.000001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "THETAUSDT", "thetausdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "THETA", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ALGOUSDT", "algousdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ALGO", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ZILUSDT", "zilusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ZIL", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "KNCUSDT", "kncusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "KNC", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ZRXUSDT", "zrxusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ZRX", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "COMPUSDT", "compusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "COMP", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "OMGUSDT", "omgusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "OMG", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "DOGEUSDT", "dogeusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "DOGE", "USDT", 'A', 1, 1, 1, 0.000001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "SXPUSDT", "sxpusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "SXP", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "KAVAUSDT", "kavausdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "KAVA", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BANDUSDT", "bandusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BAND", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "RLCUSDT", "rlcusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "RLC", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "WAVESUSDT", "wavesusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "WAVES", "USDT", 'A', 1, 0.1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "MKRUSDT", "mkrusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "MKR", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "SNXUSDT", "snxusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "SNX", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "DOTUSDT", "dotusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "DOT", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "DEFIUSDT", "defiusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "DEFI", "USDT", 'A', 1, 0.001, 1, 0.1, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "YFIUSDT", "yfiusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "YFI", "USDT", 'A', 1, 0.001, 1, 0.1, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BALUSDT", "balusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BAL", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "CRVUSDT", "crvusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "CRV", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "TRBUSDT", "trbusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "TRB", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "YFIIUSDT", "yfiiusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "YFII", "USDT", 'A', 1, 0.001, 1, 0.1, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "RUNEUSDT", "runeusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "RUNE", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "SUSHIUSDT", "sushiusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "SUSHI", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "SRMUSDT", "srmusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "SRM", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BZRXUSDT", "bzrxusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BZRX", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "EGLDUSDT", "egldusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "EGLD", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "SOLUSDT", "solusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "SOL", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ICXUSDT", "icxusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ICX", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "STORJUSDT", "storjusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "STORJ", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BLZUSDT", "blzusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BLZ", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "UNIUSDT", "uniusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "UNI", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "AVAXUSDT", "avaxusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "AVAX", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "FTMUSDT", "ftmusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "FTM", "USDT", 'A', 1, 1, 1, 0.000001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "HNTUSDT", "hntusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "HNT", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ENJUSDT", "enjusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ENJ", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "FLMUSDT", "flmusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "FLM", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "TOMOUSDT", "tomousdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "TOMO", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "RENUSDT", "renusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "REN", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "KSMUSDT", "ksmusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "KSM", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "NEARUSDT", "nearusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "NEAR", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "AAVEUSDT", "aaveusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "AAVE", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "FILUSDT", "filusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "FIL", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "RSRUSDT", "rsrusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "RSR", "USDT", 'A', 1, 1, 1, 0.000001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "LRCUSDT", "lrcusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "LRC", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "MATICUSDT", "maticusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "MATIC", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "OCEANUSDT", "oceanusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "OCEAN", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "CVCUSDT", "cvcusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "CVC", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BELUSDT", "belusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BEL", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "CTKUSDT", "ctkusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "CTK", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "AXSUSDT", "axsusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "AXS", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ALPHAUSDT", "alphausdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ALPHA", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ZENUSDT", "zenusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ZEN", "USDT", 'A', 1, 0.1, 1, 0.001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "SKLUSDT", "sklusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "SKL", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "GRTUSDT", "grtusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "GRT", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "1INCHUSDT", "1inchusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "1INCH", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BTCBUSD", "btcbusd", "", "MRCXXX", "Binance", "FUT", "", "",
      "BTC", "BUSD", 'A', 1, 0.001, 1, 0.1, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "AKROUSDT", "akrousdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "AKRO", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "DOTECOUSDT", "dotecousdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "DOTECO", "USDT", 'A', 1, 0.001, 1, 0.1, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "CHZUSDT", "chzusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "CHZ", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "SANDUSDT", "sandusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "SAND", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "ANKRUSDT", "ankrusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "ANKR", "USDT", 'A', 1, 1, 1, 0.000001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "LUNAUSDT", "lunausdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "LUNA", "USDT", 'A', 1, 1, 1, 0.0001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BTSUSDT", "btsusdt", "", "MRCXXX", "Binance", "FUT", "", "",
      "BTS", "USDT", 'A', 1, 1, 1, 0.00001, 'A', 1.0,
      21001225, 28000, 0.0, 0, ""
    },
    { 0, "BTCUSDT_210326", "btcusdt_210326", "", "MRCXXX", "Binance", "FUT",
      "H1", "", "BTC", "USDT", 'A', 1, 0.001, 1, 0.1, 'A', 1.0,
      20210326, 0, 0.0, 0, ""
    },
    { 0, "ETHUSDT_210326", "ethusdt_210326", "", "MRCXXX", "Binance", "FUT",
      "H1", "", "ETH", "USDT", 'A', 1, 0.001, 1, 0.01, 'A', 1.0,
      20210326, 0, 0.0, 0, ""
    }
  };
} // End namespace Binance
} // End namespace MAQUETTE
