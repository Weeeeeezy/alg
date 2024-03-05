// vim:ts=2:et

#include "Basis/SecDefs.h"

#include <vector>

namespace MAQUETTE
{

  namespace History
  {
    std::vector<SecDefS> const& SecDefs(const std::string& market)
    {
      static const std::map<std::string, std::vector<SecDefS>> _secDefs = {
          {"Binance",
           {{0,   "BTCUSDT",  "btcusdt", "",         "MRCXXX", "Binance",
             "",  "SPOT",     "",        "BTC",      "USDT",   'A',
             1.0, 0.00000100, 1,         0.01000000, 'A',      1.0,
             0,   0,          0.0,       0,          ""},
            {0,   "ETHUSDT",  "ethusdt", "",         "MRCXXX", "Binance",
             "",  "SPOT",     "",        "ETH",      "USDT",   'A',
             1.0, 0.00001000, 1,         0.01000000, 'A',      1.0,
             0,   0,          0.0,       0,          ""},
            {0,   "LTCUSDT",  "ltcusdt", "",         "MRCXXX", "Binance",
             "",  "SPOT",     "",        "LTC",      "USDT",   'A',
             1.0, 0.00001000, 1,         0.01000000, 'A',      1.0,
             0,   0,          0.0,       0,          ""},
            {0,   "XMRUSDT",  "xmrusdt", "",         "MRCXXX", "Binance",
             "",  "SPOT",     "",        "XMR",      "USDT",   'A',
             1.0, 0.00001000, 1,         0.01000000, 'A',      1.0,
             0,   0,          0.0,       0,          ""},
            {0,   "LTCBTC", "ltcbtc", "",  "MRCXXX", "Binance", "", "SPOT",
             "",  "LTC",    "BTC",    'A', 1.0,      0.01000,   1,  0.00000100,
             'A', 1.0,      0,        0,   0.0,      0,         ""},
            {0,   "XMRBTC", "xmrbtc", "",  "MRCXXX", "Binance", "", "SPOT",
             "",  "XMR",    "BTC",    'A', 1.0,      0.01000,   1,  0.00000100,
             'A', 1.0,      0,        0,   0.0,      0,         ""},
            {0,   "ZECBTC", "zecbtc", "",  "MRCXXX", "Binance", "", "SPOT",
             "",  "ZEC",    "BTC",    'A', 1.0,      0.01000,   1,  0.00000100,
             'A', 1.0,      0,        0,   0.0,      0,         ""},
            {0,   "ZECUSDT",  "zecusdt", "",         "MRCXXX", "Binance",
             "",  "SPOT",     "",        "ZEC",      "USDT",   'A',
             1.0, 0.00001000, 1,         0.01000000, 'A',      1.0,
             0,   0,          0.0,       0,          ""},
            {0,   "XRPUSDT",  "xrpusdt", "",          "MRCXXX", "Binance",
             "",  "SPOT",     "",        "XRP",       "USDT",   'A',
             1.0, 0.00001000, 1,         0.000010000, 'A',      1.0,
             0,   0,          0.0,       0,           ""},
            {0,   "QTUMUSDT", "qtumusdt", "",          "MRCXXX", "Binance",
             "",  "SPOT",     "",         "QTUM",      "USDT",   'A',
             1.0, 0.00001000, 1,          0.001000000, 'A',      1.0,
             0,   0,          0.0,        0,           ""},
            {0,   "QTUMBTC",  "qtumbtc", "",         "MRCXXX", "Binance",
             "",  "SPOT",     "",        "QTUM",     "BTC",    'A',
             1.0, 0.00001000, 1,         0.00000001, 'A',      1.0,
             0,   0,          0.0,       0,          ""},
            {0,   "VETUSDT",  "vetusdt", "",        "MRCXXX", "Binance",
             "",  "SPOT",     "",        "VET",     "USDT",   'A',
             1.0, 0.00001000, 1,         0.0000001, 'A',      1.0,
             0,   0,          0.0,       0,         ""},
            {0,   "VETBTC", "vetbtc", "",  "MRCXXX", "Binance",  "", "SPOT",
             "",  "VET",    "BTC",    'A', 1.0,      0.00001000, 1,  0.00000001,
             'A', 1.0,      0,        0,   0.0,      0,          ""},
            {0,   "DASHUSDT", "dashusdt", "",       "MRCXXX", "Binance",
             "",  "SPOT",     "",         "DASH",   "USDT",   'A',
             1.0, 0.00001000, 1,          0.010000, 'A',      1.0,
             0,   0,          0.0,        0,        ""},
            {0,   "DASHBTC", "dashbtc", "",  "MRCXXX", "Binance",  "", "SPOT",
             "",  "DASH",    "BTC",     'A', 1.0,      0.00001000, 1,  0.000001,
             'A', 1.0,       0,         0,   0.0,      0,          ""}}},
          {"BitFinex",
           {{0,   "tBTCUSD", "BTCUSD", "",  "MRCXXX", "BitFinex", "", "SPOT",
             "",  "BTC",     "USD",    'A', 1.0,      0.0008,     1,  1e-5,
             'A', 1.0,       0,        0,   0.0,      0,          ""},
            {0,   "tETHUSD", "ETHUSD", "",  "MRCXXX", "BitFinex", "", "SPOT",
             "",  "ETH",     "USD",    'A', 1.0,      0.04,       1,  1e-5,
             'A', 1.0,       0,        0,   0.0,      0,          ""}}},
          {"BitMEX",
           {{88,  "XBTUSD", "xbtusd", "",  "FFWCSX", "BitMEX", "", "SPOT",
             "",  "BTC",    "USD",    'B', 1.0,      1.0,      1,  0.5,
             'A', 1.0,      0,        0,   0.0,      0,        ""},
            {297, "ETHUSD", "ethusd", "",  "FFWCSX", "BitMEX", "", "SPOT",
             "",  "ETH",    "USD",    'A', 1.0,      1,        1,  0.05,
             'A', 1.0,      0,        0,   0.0,      0,        ""}}},
          {"Huobi",
           {{0,   "btcusdt", "btcusdt", "",  "",  "Huobi", "", "SPOT",
             "",  "btc",     "usdt",    'A', 1.0, 0.0001,  1,  1e-2,
             'A', 1.0,       0,         0,   0.0, 0,       ""},
            {0,   "ethusdt", "ethusdt", "",  "",  "Huobi", "", "SPOT",
             "",  "eth",     "usdt",    'A', 1.0, 0.001,   1,  1e-2,
             'A', 1.0,       0,         0,   0.0, 0,       ""}}}};

      auto it = _secDefs.find(market);
      if (it == _secDefs.end())
      {
        throw utxx::runtime_error("SecDefs(): market '{}' not found", market);
      }
      return it->second;
    }

      std::string GetCovexityName(const std::string& ticker)
      {
        static std::map<std::string, std::string> conv = {
            {"btcusdt@Huobi", "BTC-USDT@HUOBI"},
            {"ethusdt@Huobi", "ETH-USDT@HUOBI"},

            {"BTCUSDT@Binance", "BTC-USDT@BINANCE"},
            {"ETHUSDT@Binance", "ETH-USDT@BINANCE"},
            {"LTCUSDT@Binance", "LTC-USDT@BINANCE"},
            {"ZECUSDT@Binance", "ZEC-USDT@BINANCE"},
            {"XRPUSDT@Binance", "XRP-USDT@BINANCE"},
            {"LTCBTC@Binance", "LTC-BTC@BINANCE"},
            {"XMRBTC@Binance", "XMR-BTC@BINANCE"},
            {"XMRUSDT@Binance", "XMR-USDT@BINANCE"},
            {"ZECBTC@Binance", "ZEC-BTC@BINANCE"},
            {"DASHBTC@Binance", "DASH-BTC@BINANCE"},
            {"DASHUSDT@Binance", "DASH-USDT@BINANCE"},
            {"QTUMUSDT@Binance", "QTUM-USDT@BINANCE"},
            {"QTUMBTC@Binance", "QTUM-BTC@BINANCE"},
            {"VETUSDT@Binance", "VET-USDT@BINANCE"},
            {"VETBTC@Binance", "VET-BTC@BINANCE"},

            {"tBTCUSD@BitFinex", "BTC-USD@BITFINEX"},
            {"tETHUSD@BitFinex", "ETH-USD@BITFINEX"},

            {"XBTUSD@BitMEX", "BTC-USD@BITMEX"},
            {"ETHUSD@BitMEX", "ETH-USD@BITMEX"}};

        auto it = conv.find(ticker);
        if (it == conv.end())
          throw utxx::runtime_error(
              "GetCovexityName() unsupported ticker: ",
              ticker);
        return it->second;
    }

  } // namespace History
} // namespace MAQUETTE
