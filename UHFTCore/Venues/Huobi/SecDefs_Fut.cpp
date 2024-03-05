// vim:ts=2:et
//===========================================================================//
//                      "Venues/Huobi/SecDefs.cpp":                        //
//===========================================================================//
#include "Venues/Huobi/SecDefs.h"

namespace MAQUETTE
{
namespace Huobi
{
  //=======================================================================//
  // "SecDefs":                                                            //
  //=======================================================================//
  // Last updated: 2021-02-07
  std::vector<SecDefS> const SecDefs_Fut =
  {
    { 0, "ADA", "ADA_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "ada", "usdt", 'A', 1, 10.0, 1, 1e-06, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ADA", "ADA_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "ada", "usdt", 'A', 1, 10.0, 1, 1e-06, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ADA", "ADA_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "ada", "usdt", 'A', 1, 10.0, 1, 1e-06, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ADA", "ADA_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "ada", "usdt", 'A', 1, 10.0, 1, 1e-06, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BCH", "BCH_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "bch", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BCH", "BCH_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "bch", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BCH", "BCH_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "bch", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BCH", "BCH_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "bch", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BSV", "BSV_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "bsv", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BSV", "BSV_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "bsv", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BSV", "BSV_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "bsv", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BSV", "BSV_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "bsv", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BTC", "BTC_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "btc", "usdt", 'A', 1, 100.0, 1, 0.01, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BTC", "BTC_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "btc", "usdt", 'A', 1, 100.0, 1, 0.01, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BTC", "BTC_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "btc", "usdt", 'A', 1, 100.0, 1, 0.01, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "BTC", "BTC_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "btc", "usdt", 'A', 1, 100.0, 1, 0.01, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "DOT", "DOT_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "dot", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "DOT", "DOT_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "dot", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "DOT", "DOT_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "dot", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "DOT", "DOT_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "dot", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "EOS", "EOS_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "eos", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "EOS", "EOS_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "eos", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "EOS", "EOS_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "eos", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "EOS", "EOS_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "eos", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETC", "ETC_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "etc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETC", "ETC_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "etc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETC", "ETC_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "etc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETC", "ETC_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "etc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETH", "ETH_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "eth", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETH", "ETH_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "eth", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETH", "ETH_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "eth", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "ETH", "ETH_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "eth", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "FIL", "FIL_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "fil", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "FIL", "FIL_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "fil", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "FIL", "FIL_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "fil", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "FIL", "FIL_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "fil", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LINK", "LINK_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "link", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LINK", "LINK_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "link", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LINK", "LINK_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "link", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LINK", "LINK_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "link", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LTC", "LTC_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "ltc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LTC", "LTC_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "ltc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LTC", "LTC_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "ltc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "LTC", "LTC_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "ltc", "usdt", 'A', 1, 10.0, 1, 0.001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "TRX", "TRX_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "trx", "usdt", 'A', 1, 10.0, 1, 1e-05, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "TRX", "TRX_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "trx", "usdt", 'A', 1, 10.0, 1, 1e-05, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "TRX", "TRX_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "trx", "usdt", 'A', 1, 10.0, 1, 1e-05, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "TRX", "TRX_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "trx", "usdt", 'A', 1, 10.0, 1, 1e-05, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "XRP", "XRP_CQ", "", "MRCXXX", "Huobi", "FUT_CQ", "quarter", "",
      "xrp", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "XRP", "XRP_NW", "", "MRCXXX", "Huobi", "FUT_NW", "next_week", "",
      "xrp", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "XRP", "XRP_CW", "", "MRCXXX", "Huobi", "FUT_CW", "this_week", "",
      "xrp", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 0, "XRP", "XRP_NQ", "", "MRCXXX", "Huobi", "FUT_NQ", "next_quarter", "",
      "xrp", "usdt", 'A', 1, 10.0, 1, 0.0001, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
  };
} // namespace Huobi
} // namespace MAQUETTE
