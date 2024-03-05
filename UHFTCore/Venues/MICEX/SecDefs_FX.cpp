// vim:ts=2:et
//===========================================================================//   1,
//                      "Venues/MICEX/SecDefs_FX.cpp":                       //
//  "SecDef"s for MICEX FX, Automatically Generated with "MkSecDefs_MICEX"   //
//                            As of: 20-Mar-2017                             //
//===========================================================================//
#include "Venues/MICEX/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace MICEX
{
  //=========================================================================//
  // "SecDefs_FX":                                                           //
  //=========================================================================//
  // XXX: TenorTime is set (notionally) to 17:30 MSK = 14:30 UTC = 52200 sec
  // intra-day:   1,
  vector<SecDefS> const SecDefs_FX
  {
    { // 1
      0,
      "BYNRUBTODTOM",
      "",
      "BYN_TODTOM - SWAP BYN/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "BYN",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 2
      0,
      "BYNRUB_TOD",
      "",
      "BYN/RUB_TOD - BYN/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "BYN",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 3
      0,
      "BYNRUB_TOM",
      "",
      "BYN/RUB_TOM - BYN/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "BYN",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 4
      0,
      "CHFRUBTODTOM",
      "",
      "CHF_TODTOM - SWAP CHF/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "CHF",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 5
      0,
      "CHFRUB_TOD",
      "",
      "CHF/RUB_TOD - CHF/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "CHF",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 6
      0,
      "CHFRUB_TOM",
      "",
      "CHF/RUB_TOM - CHF/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "CHF",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 7
      0,
      "CNY000000TOD",
      "",
      "CNY/RUB_TOD - CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "CNY",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 8
      0,
      "CNYRUBTODTOM",
      "",
      "CNY_TODTOM - SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 9
      0,
      "CNYRUB_FWD",
      "",
      "CNY/RUB_LTV - CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "FWD",   // XXX: What is the difference between LTV and FWD???
      "",
      "CNY",
      "RUB",
      'A', 1.0,
      1000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 10
      0,
      "CNYRUB_SPT",
      "",
      "CNY/RUB_SPT - CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "SPOT",
      "",
      "CNY",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 11
      0,
      "CNYRUB_TOM",
      "",
      "CNY/RUB_TOM - CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "CNY",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 12
      0,
      "CNYRUB_TOM1D",
      "",
      "TOM_1D SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1D",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 13
      0,
      "CNYRUB_TOM1M",
      "",
      "TOM_1M SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1M",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 14
      0,
      "CNYRUB_TOM1W",
      "",
      "TOM_1W SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1W",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 15
      0,
      "CNYRUB_TOM2M",
      "",
      "TOM_2M SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "2M",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 16
      0,
      "CNYRUB_TOM2W",
      "",
      "TOM_2W SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "2W",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 17
      0,
      "CNYRUB_TOM3M",
      "",
      "TOM_3M SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "3M",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 18
      0,
      "CNYRUB_TOM6M",
      "",
      "TOM_6M SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "6M",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 19
      0,
      "EUR000TODTOM",
      "",
      "EUR_TODTOM - SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 20
      0,
      "EURRUB_FWD",
      "",
      "EURRUB_FWD - EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "FWD",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 21
      0,
      "EURRUB_SPT",
      "",
      "EURRUB_SPT - EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "SPOT",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 22
      0,
      "EURRUB_TOM1D",
      "",
      "TOM_1D SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1D",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 23
      0,
      "EURRUB_TOM1M",
      "",
      "TOM_1M SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1M",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 24
      0,
      "EURRUB_TOM1W",
      "",
      "TOM_1W SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1W",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 25
      0,
      "EURRUB_TOM1Y",
      "",
      "TOM_1Y SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1Y",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 26
      0,
      "EURRUB_TOM2M",
      "",
      "TOM_2M SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "2M",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 27
      0,
      "EURRUB_TOM2W",
      "",
      "TOM_2W SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "2W",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 28
      0,
      "EURRUB_TOM3M",
      "",
      "TOM_3M SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "3M",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 29
      0,
      "EURRUB_TOM6M",
      "",
      "TOM_6M SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "6M",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 30
      0,
      "EURRUB_TOM9M",
      "",
      "TOM_9M SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "9M",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 31
      0,
      "EURUSD000TOD",
      "",
      "EUR/USD_TOD - EUR/USD",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "EUR",
      "USD",
      'A', 1.0,
      1000,   1,
      5e-05,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 32
      0,
      "EURUSD000TOM",
      "",
      "EUR/USD_TOM - EUR/USD",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "EUR",
      "USD",
      'A', 1.0,
      1000,   1,
      5e-05,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 33
      0,
      "EURUSDTODTOM",
      "",
      "EUR/USD_TODTOM - SWAP EUR/USD",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "EUR",
      "USD",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 34
      0,
      "EUR_RUB__TOD",
      "",
      "EURRUB_TOD - EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 35
      0,
      "EUR_RUB__TOM",
      "",
      "EURRUB_TOM - EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 36
      0,
      "GBPRUBTODTOM",
      "",
      "GBP_TODTOM - SWAP GBP/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "GBP",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 37
      0,
      "GBPRUB_TOD",
      "",
      "GBP/RUB_TOD - GBP/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "GBP",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 38
      0,
      "GBPRUB_TOM",
      "",
      "GBP/RUB_TOM - GBP/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "GBP",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 39
      0,
      "GLDRUBTODTOM",
      "",
      "GLD_TODTOM - SWAP GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "XAU",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 40
      0,
      "GLDRUB_FWD",
      "",
      "GLDRUB_FWD - GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "FWD",
      "",
      "XAU",
      "RUB",
      'A', 1.0,
      10,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 41
      0,
      "GLDRUB_SPT",
      "",
      "GLDRUB_SPT - GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "SPOT",
      "",
      "XAU",
      "RUB",
      'A', 1.0,
      10,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 42
      0,
      "GLDRUB_TOD",
      "",
      "GLDRUB_TOD - GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "XAU",
      "RUB",
      'A', 1.0,
      10,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 43
      0,
      "GLDRUB_TOM",
      "",
      "GLDRUB_TOM - GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "XAU",
      "RUB",
      'A', 1.0,
      10,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 44
      0,
      "GLDRUB_TOM1D",
      "",
      "TOM_1D SWAP GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1D",
      "XAU",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 45
      0,
      "GLDRUB_TOM1M",
      "",
      "TOM_1M SWAP GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1M",
      "XAU",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 46
      0,
      "GLDRUB_TOM1W",
      "",
      "TOM_1W SWAP GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1W",
      "XAU",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 47
      0,
      "GLDRUB_TOM6M",
      "",
      "TOM_6M SWAP GLD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "6M",
      "XAU",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 48
      0,
      "HKDRUBTODTOM",
      "",
      "HKD_TODTOM - SWAP HKD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "HKD",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 49
      0,
      "HKDRUB_TOD",
      "",
      "HKD/RUB_TOD - HKD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "HKD",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 50
      0,
      "HKDRUB_TOM",
      "",
      "HKD/RUB_TOM - HKD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "HKD",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 51
      0,
      "KZT000000TOM",
      "",
      "KZTRUB_TOM - KZT/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "KZT",
      "RUB",
      'A', 1.0,
      10000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 52
      0,
      "SLVRUBTODTOM",
      "",
      "SLV_TODTOM - SWAP SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "XAG",
      "RUB",
      'A', 1.0,
      50000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 53
      0,
      "SLVRUB_FWD",
      "",
      "SLVRUB_FWD - SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "FWD",
      "",
      "XAG",
      "RUB",
      'A', 1.0,
      100,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 54
      0,
      "SLVRUB_SPT",
      "",
      "SLVRUB_SPT - EUR/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "SPOT",
      "",
      "XAG",
      "RUB",
      'A', 1.0,
      100,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 55
      0,
      "SLVRUB_TOD",
      "",
      "SLVRUB_TOD - SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "XAG",
      "RUB",
      'A', 1.0,
      100,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 56
      0,
      "SLVRUB_TOM",
      "",
      "SLVRUB_TOM - SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "XAG",
      "RUB",
      'A', 1.0,
      100,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 57
      0,
      "SLVRUB_TOM1D",
      "",
      "TOM_1D SWAP SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1D",
      "XAG",
      "RUB",
      'A', 1.0,
      50000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 58
      0,
      "SLVRUB_TOM1M",
      "",
      "TOM_1M SWAP SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1M",
      "XAG",
      "RUB",
      'A', 1.0,
      50000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 59
      0,
      "SLVRUB_TOM1W",
      "",
      "TOM_1W SWAP SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1W",
      "XAG",
      "RUB",
      'A', 1.0,
      50000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 60
      0,
      "SLVRUB_TOM6M",
      "",
      "TOM_6M SWAP SLV/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "6M",
      "XAG",
      "RUB",
      'A', 1.0,
      50000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 61
      0,
      "UAH000000TOM",
      "",
      "UAHRUB_TOM - UAH/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "UAH",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 62
      0,
      "USD000000TOD",
      "",
      "USDRUB_TOD - USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 63
      0,
      "USD000TODTOM",
      "",
      "USD_TODTOM - SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOD",
      "TOM",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 64
      0,
      "USD000UTSTOM",
      "",
      "USDRUB_TOM - USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 66
      0,
      "USDRUB_DIS",
      "",
      "Discrete Auction USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "DIS",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      1000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 67
      0,
      "USDRUB_FWD",
      "",
      "USDRUB_FWD - USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "FWD",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 68
      0,
      "USDRUB_SPT",
      "",
      "USDRUB_SPT - USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "SPOT",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0025,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 69
      0,
      "USDRUB_TOM1D",
      "",
      "TOM_1D SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1D",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 70
      0,
      "USDRUB_TOM1M",
      "",
      "TOM_1M SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1M",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 71
      0,
      "USDRUB_TOM1W",
      "",
      "TOM_1W SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1W",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 72
      0,
      "USDRUB_TOM1Y",
      "",
      "TOM_1Y SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "1Y",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 73
      0,
      "USDRUB_TOM2M",
      "",
      "TOM_2M SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "2M",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 74
      0,
      "USDRUB_TOM2W",
      "",
      "TOM_2W SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "2W",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 75
      0,
      "USDRUB_TOM3M",
      "",
      "TOM_3M SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "3M",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 76
      0,
      "USDRUB_TOM6M",
      "",
      "TOM_6M SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "6M",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 77
      0,
      "USDRUB_TOM9M",
      "",
      "TOM_9M SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "CETS",
      "TOM",
      "9M",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 177
      0,
      "CNYRUB_0617",
      "",
      "CNYRUB_06.17 - CNY/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "M7",
      "",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 178
      0,
      "CNYRUB_0917",
      "",
      "CNYRUB_09.17 - CNY/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "U7",
      "",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 179
      0,
      "CNYRUB_TOM",
      "",
      "CNY/RUB_TOM - CNY/RUB",
      "MRCXXX",
      "MICEX", "FUTS",        // XXX: Also available in CETS!
      "TOM",
      "",
      "CNY",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 180
      0,
      "CNYTOM_0617",
      "",
      "TOM_06.17 SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "M7",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 181
      0,
      "CNYTOM_0917",
      "",
      "TOM_09.17 SWAP CNY/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "U7",
      "CNY",
      "RUB",
      'A', 1.0,
      100000,   1,
      1e-06,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 182
      0,
      "EURRUB_0318",
      "",
      "EURRUB_03.18 - EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "H8",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 183
      0,
      "EURRUB_0617",
      "",
      "EURRUB_06.17 - EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "M7",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 184
      0,
      "EURRUB_0917",
      "",
      "EURRUB_09.17 - EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "U7",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 185
      0,
      "EURRUB_1217",
      "",
      "EURRUB_12.17 - EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "Z7",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 186
      0,
      "EURTOM_0318",
      "",
      "TOM_03.18 SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "H8",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 187
      0,
      "EURTOM_0617",
      "",
      "TOM_06.17 SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "M7",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 188
      0,
      "EURTOM_0917",
      "",
      "TOM_09.17 SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "U7",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 189
      0,
      "EURTOM_1217",
      "",
      "TOM_12.17 SWAP EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "Z7",
      "EUR",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 190
      0,
      "EUR_RUB__TOM",
      "",
      "EURRUB_TOM - EUR/RUB",
      "MRCXXX",
      "MICEX", "FUTS",       // XXX: Also available in CETS!
      "TOM",
      "",
      "EUR",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 191
      0,
      "USD000UTSTOM",
      "",
      "USDRUB_TOM - USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",       // XXX: Also available in CETS!
      "TOM",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      1000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 192
      0,
      "USDRUB_0318",
      "",
      "USDRUB_03.18 - USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "H8",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 193
      0,
      "USDRUB_0617",
      "",
      "USDRUB_06.17 - USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "M7",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 194
      0,
      "USDRUB_0917",
      "",
      "USDRUB_09.17 - USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "U7",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 195
      0,
      "USDRUB_1217",
      "",
      "USDRUB_12.17 - USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "Z7",
      "",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.01,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 196
      0,
      "USDTOM_0318",
      "",
      "TOM_03.18 SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "H8",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 197
      0,
      "USDTOM_0617",
      "",
      "TOM_06.17 SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "M7",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 198
      0,
      "USDTOM_0917",
      "",
      "TOM_09.17 SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "U7",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    },
    { // 199
      0,
      "USDTOM_1217",
      "",
      "TOM_12.17 SWAP USD/RUB",
      "MRCXXX",
      "MICEX", "FUTS",
      "TOM",
      "Z7",
      "USD",
      "RUB",
      'A', 1.0,
      100000,   1,
      0.0001,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    }
  };

  //=========================================================================//
  // "Baskets_FX":                                                           //
  //=========================================================================//
  vector<SecDefS> const Baskets_FX
  {
    { // 65
      0,
      "USDEURBASKET",
      "",
      "USDEUR/RUB basket",
      "MRIXXX",
      "MICEX", "CETS",
      "TOM",      // XXX???
      "",
      "USDEURBASKET",
      "RUB",
      'A', 1.0,
      0,   0,
      0,
      'A', 1.0, 0, 52200, 0.0, 0, ""
    }
  };
} // End namespace MICEX
} // End namespace MAQUETTE
