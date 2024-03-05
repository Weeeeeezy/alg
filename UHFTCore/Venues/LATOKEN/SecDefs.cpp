// vim:ts=2:et
//===========================================================================//
//                       "Venues/LATOKEN/SecDefs.cpp":                       //
//                             As of: 2019-08-03                             //
//===========================================================================//
#include "Venues/LATOKEN/SecDefs.h"
using namespace std;

namespace MAQUETTE
{
namespace LATOKEN
{
  //=========================================================================//
  // "SecDefs":                                                              //
  //=========================================================================//
  vector<SecDefS> const SecDefs
  {
    { 29, "LA/ETH", "", "LATOKEN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LA", "ETH", 'A', 1.0, 1e-1, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 152, "HTKN/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "HTKN", "LA", 'A', 1.0, 1e-1, 0, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 154, "PAY/ETH", "", "TenX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PAY", "ETH", 'A', 1.0, 1e-1, 20, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 155, "POWR/ETH", "", "Power Ledger / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "POWR", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 156, "AION/ETH", "", "Aion / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AION", "ETH", 'A', 1.0, 1e-2, 1000, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 157, "FUN/ETH", "", "FunFair / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FUN", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 158, "WTC/ETH", "", "Walton / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "WTC", "ETH", 'A', 1.0, 1e-2, 100, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 159, "GNO/ETH", "", "Gnosis / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GNO", "ETH", 'A', 1.0, 1e-3, 50, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 160, "RDN/ETH", "", "Raiden Network / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RDN", "ETH", 'A', 1.0, 1.0, 1, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 161, "VERI/ETH", "", "Veritaseum / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VERI", "ETH", 'A', 1.0, 1e-3, 30, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 162, "SNT/ETH", "", "StatusNetwork / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNT", "ETH", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 163, "BNT/ETH", "", "Bancor / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BNT", "ETH", 'A', 1.0, 1e-2, 100, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 165, "PPT/ETH", "", "Populous / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PPT", "ETH", 'A', 1.0, 1e-2, 100, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 167, "POWR/BTC", "", "Power Ledger / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "POWR", "BTC", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 168, "AION/BTC", "", "Aion / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AION", "BTC", 'A', 1.0, 1.0, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 169, "FUN/BTC", "", "FunFair / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FUN", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 170, "WTC/BTC", "", "Walton / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "WTC", "BTC", 'A', 1.0, 1e-2, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 174, "SNT/BTC", "", "StatusNetwork / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNT", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 177, "PPT/BTC", "", "Populous / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PPT", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 226, "TRX/USDT", "", "Tron / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRX", "USDT", 'A', 1.0, 1.0, 50, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 227, "ICX/USDT", "", "ICON / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ICX", "USDT", 'A', 1.0, 1e-2, 1000, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 229, "TRX/BTC", "", "Tron / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRX", "BTC", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 230, "ICX/BTC", "", "ICON / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ICX", "BTC", 'A', 1.0, 1e-2, 200, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 232, "TRX/ETH", "", "Tron / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRX", "ETH", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 233, "ICX/ETH", "", "ICON / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ICX", "ETH", 'A', 1.0, 1e-2, 100000000, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 254, "MTRC/ETH", "", "ModulTrade / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MTRC", "ETH", 'A', 1.0, 1e-1, 750, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 256, "CPT/LA", "", "Cryptaur / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CPT", "LA", 'A', 1.0, 1.0, 370, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 257, "CPT/ETH", "", "Cryptaur / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CPT", "ETH", 'A', 1.0, 1.0, 370, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 262, "ZCO/ETH", "", "Zebi / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ZCO", "ETH", 'A', 1.0, 1.0, 80, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 264, "NPXS/LA", "", "Pundi X / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NPXS", "LA", 'A', 1.0, 1.0, 660, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 265, "NPXS/ETH", "", "Pundi X / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NPXS", "ETH", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 266, "HOT/LA", "", "Holo / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HOT", "LA", 'A', 1.0, 1.0, 1000, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 267, "HOT/ETH", "", "Holo / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HOT", "ETH", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 269, "PCO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PCO", "ETH", 'A', 1.0, 1e-1, 300, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 271, "TEN/ETH", "", "Tokenomy / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TEN", "ETH", 'A', 1.0, 1e-1, 70, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 280, "LOOM/ETH", "", "Loom Network / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOOM", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 282, "MYB/ETH", "", "MyBit Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MYB", "ETH", 'A', 1.0, 1e-1, 550, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 285, "GOT/ETH", "", "GOeureka / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GOT", "ETH", 'A', 1.0, 1e-4, 70000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 287, "QKC/ETH", "", "Quarkchain / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QKC", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 288, "BEZ/ETH", "", "BEZOP / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BEZ", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 292, "QASH/ETH", "", "QASH / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QASH", "ETH", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 293, "SALT/ETH", "", "SALT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SALT", "ETH", 'A', 1.0, 1e-1, 20, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 294, "BAT/ETH", "", "Basic Attention Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BAT", "ETH", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 295, "SAN/ETH", "", "Santiment Network / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SAN", "ETH", 'A', 1.0, 1e-1, 20, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 302, "JOINT/ETH", "", "JOINT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JOINT", "ETH", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 324, "OLT/ETH", "", "OneLedger Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OLT", "ETH", 'A', 1.0, 1.0, 125, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 326, "DRGN/ETH", "", "Dragonchain / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DRGN", "ETH", 'A', 1.0, 1e-1, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 327, "ATMI/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ATMI", "ETH", 'A', 1.0, 1e-0, 130, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 330, "SCT/ETH", "", "Soma Community Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SCT", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 333, "SPN/ETH", "", "Sapien Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SPN", "ETH", 'A', 1.0, 1.0, 200, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 335, "XPST/ETH", "", "PokerSports Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XPST", "ETH", 'A', 1.0, 1.0, 12500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 337, "ATS/ETH", "", "Authorship / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ATS", "ETH", 'A', 1.0, 1.0, 855, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 339, "DIW/ETH", "", "DIW Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DIW", "ETH", 'A', 1.0, 1.0, 215, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 342, "SPX/ETH", "", "Sp8de / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SPX", "ETH", 'A', 1.0, 1.0, 11100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 344, "WELL/ETH", "", "WELL / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "WELL", "ETH", 'A', 1.0, 1e-1, 1000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 345, "NIM/ETH", "", "Nimiq / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NIM", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 348, "HUR/ETH", "", "Hurify Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HUR", "ETH", 'A', 1.0, 1.0, 420, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 350, "ESTS/ETH", "", "ESTS / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ESTS", "ETH", 'A', 1.0, 1.0, 130, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 352, "XPAT/ETH", "", "Pangea Arbitration Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XPAT", "ETH", 'A', 1.0, 1.0, 142850, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 354, "URUN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "URUN", "ETH", 'A', 1.0, 1e-1, 10, 1e-6, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 356, "UTK/ETH", "", "UTK / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UTK", "ETH", 'A', 1.0, 1.0, 25, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 358, "SCRL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SCRL", "ETH", 'A', 1.0, 1e-0, 15, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 360, "STQ/ETH", "", "STQ / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "STQ", "ETH", 'A', 1.0, 1.0, 555, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 362, "VIKKY/ETH", "", "Vikky Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VIKKY", "ETH", 'A', 1.0, 1.0, 26315, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 364, "MVP/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MVP", "LA", 'A', 1.0, 1e-1, 13150, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 365, "MVP/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MVP", "ETH", 'A', 1.0, 1e-1, 13150, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 366, "NRVE/ETH", "", "NRVE / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NRVE", "ETH", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 370, "ETH/BTC", "", "Ethereum / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ETH", "BTC", 'A', 1.0, 1e-3, 10, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 371, "LA/BTC", "", "LATOKEN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LA", "BTC", 'A', 1.0, 1e-1, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 372, "QURO/ETH", "", "Qurito / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QURO", "ETH", 'A', 1.0, 1.0, 45, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 373, "QURO/LA", "", "Qurito / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QURO", "LA", 'A', 1.0, 1.0, 45, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 374, "MYST/ETH", "", "MYST / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MYST", "ETH", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 376, "KON/ETH", "", "Konios Project / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KON", "ETH", 'A', 1.0, 1.0, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 396, "DAV/ETH", "", "DAV Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DAV", "ETH", 'A', 1.0, 1.0, 190, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 402, "AEN/ETH", "", "Aenco Solutions / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AEN", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 404, "MCO/ETH", "", "Crypto.com / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCO", "ETH", 'A', 1.0, 1e-2, 20, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 405, "AE/ETH", "", "Aeternity / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AE", "ETH", 'A', 1.0, 1e-2, 100, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 406, "MANA/ETH", "", "Decentraland / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MANA", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 407, "REP/ETH", "", "Augur / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "REP", "ETH", 'A', 1.0, 1e-3, 50, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 408, "ETHOS/ETH", "", "ETHOS / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ETHOS", "ETH", 'A', 1.0, 1e-2, 300, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 412, "PHM/ETH", "", "Phoneum / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PHM", "ETH", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 415, "MTC/ETH", "", "doc.com Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MTC", "ETH", 'A', 1.0, 1e-1, 800, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 416, "SGP/ETH", "", "SGPay Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SGP", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 421, "POLY/ETH", "", "Poly / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "POLY", "ETH", 'A', 1.0, 1e-1, 40, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 423, "MTC/BTC", "", "doc.com Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MTC", "BTC", 'A', 1.0, 1e-1, 800, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 424, "TKY/ETH", "", "TKY / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TKY", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 426, "DYC/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "DYC", "LA", 'A', 1.0, 1e-0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 427, "DYC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "DYC", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 430, "ABYSS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ABYSS", "ETH", 'A', 1.0, 1e-1, 1000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 433, "INCX/ETH", "", "INCX Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "INCX", "ETH", 'A', 1.0, 1.0, 180, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 435, "EFX/ETH", "", "EFX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EFX", "ETH", 'A', 1.0, 1.0, 90, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 437, "SETH/ETH", "", "Sether / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SETH", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 439, "AVINOC/LA", "", "AVINOC / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AVINOC", "LA", 'A', 1.0, 1.0, 70, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 440, "AVINOC/ETH", "", "AVINOC / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AVINOC", "ETH", 'A', 1.0, 1.0, 70, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 443, "GTA/ETH", "", "GTA / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GTA", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 446, "SETH/BTC", "", "Sether / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SETH", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 447, "ABYSS/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ABYSS", "BTC", 'A', 1.0, 1e-1, 1000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 448, "TMC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TMC", "ETH", 'A', 1.0, 1e-0, 44, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 450, "SRN/ETH", "", "SIRIN Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SRN", "ETH", 'A', 1.0, 1e-1, 80, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 452, "SRN/BTC", "", "SIRIN Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SRN", "BTC", 'A', 1.0, 1e-1, 80, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 453, "DBET/BTC", "", "DBET / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DBET", "BTC", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 455, "DBET/ETH", "", "DBET / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DBET", "ETH", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 457, "TMC/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TMC", "BTC", 'A', 1.0, 1e-1, 440, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 458, "PTOY/ETH", "", "Patientory  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PTOY", "ETH", 'A', 1.0, 1.0, 25, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 460, "PTOY/BTC", "", "Patientory  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PTOY", "BTC", 'A', 1.0, 1.0, 25, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 461, "NKN/ETH", "", "NKN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NKN", "ETH", 'A', 1.0, 1e-1, 200, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 463, "NKN/BTC", "", "NKN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NKN", "BTC", 'A', 1.0, 1e-1, 200, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 464, "C8/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "C8", "ETH", 'A', 1.0, 1e-1, 1400, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 466, "ENJ/ETH", "", "Enjin Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ENJ", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 468, "ENJ/BTC", "", "Enjin Coin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ENJ", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 469, "RLX/ETH", "", "Relex / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RLX", "ETH", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 471, "RLX/BTC", "", "Relex / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RLX", "BTC", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 472, "AMO/ETH", "", "AMO Foundation / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMO", "ETH", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 475, "DENT/ETH", "", "DENT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DENT", "ETH", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 477, "DENT/BTC", "", "DENT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DENT", "BTC", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 480, "FUEL/ETH", "", "Etherparty / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FUEL", "ETH", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 482, "FUEL/BTC", "", "Etherparty / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FUEL", "BTC", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 483, "INXT/ETH", "", "Internxt / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "INXT", "ETH", 'A', 1.0, 1e-1, 2, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 485, "INXT/BTC", "", "Internxt / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "INXT", "BTC", 'A', 1.0, 1e-1, 2, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 486, "CEEK/ETH", "", "CEEK Smart VR Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CEEK", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 488, "CEEK/BTC", "", "CEEK Smart VR Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CEEK", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 489, "KWATT/ETH", "", "KWATT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KWATT", "ETH", 'A', 1.0, 1e-1, 450, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 490, "KWATT/BTC", "", "KWATT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KWATT", "BTC", 'A', 1.0, 1e-1, 450, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 492, "AGLT/ETH", "", "Agrolot / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AGLT", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 495, "OIO/ETH", "", "ONLINE.IO  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OIO", "ETH", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 496, "OIO/BTC", "", "ONLINE.IO  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OIO", "BTC", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 499, "STQ/BTC", "", "STQ / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "STQ", "BTC", 'A', 1.0, 1.0, 555, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 502, "ETH/USDT", "", "Ethereum / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ETH", "USDT", 'A', 1.0, 1e-3, 10, 1e-2, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 503, "BTC/USDT", "", "Bitcoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BTC", "USDT", 'A', 1.0, 1e-4, 10, 1e-2, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 504, "KNW/ETH", "", "KNW Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KNW", "ETH", 'A', 1.0, 1e-1, 700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 505, "KNW/BTC", "", "KNW Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KNW", "BTC", 'A', 1.0, 1e-1, 700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 506, "MRPH/ETH", "", "Morpheus Network / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MRPH", "ETH", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 509, "AGLT/BTC", "", "Agrolot / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AGLT", "BTC", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 510, "UBC/ETH", "", "Ubcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UBC", "ETH", 'A', 1.0, 1e-1, 3500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 512, "IMT/ETH", "", "MoneyToken / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IMT", "ETH", 'A', 1.0, 1e-1, 11100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 514, "MFTU/ETH", "", "Mainstream For The Underground / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MFTU", "ETH", 'A', 1.0, 1.0, 6100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 516, "MFTU/BTC", "", "Mainstream For The Underground / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MFTU", "BTC", 'A', 1.0, 1.0, 6100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 517, "LOCI/ETH", "", "LOCIcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOCI", "ETH", 'A', 1.0, 1e-1, 450, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 518, "LOCI/BTC", "", "LOCIcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOCI", "BTC", 'A', 1.0, 1.0, 45, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 521, "CHT/ETH", "", "Countinghouse CHT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CHT", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 522, "CHT/LA", "", "Countinghouse CHT / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CHT", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 524, "OPET/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "OPET", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 525, "OPET/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "OPET", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 526, "METM/ETH", "", "MetaMorph / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "METM", "ETH", 'A', 1.0, 1.0, 65, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 528, "METM/BTC", "", "MetaMorph / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "METM", "BTC", 'A', 1.0, 1.0, 65, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 529, "OAX/ETH", "", "OAX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OAX", "ETH", 'A', 1.0, 1.0, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 530, "OAX/BTC", "", "OAX / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OAX", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 532, "WHEN/ETH", "", "WHEN Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "WHEN", "ETH", 'A', 1.0, 1e-1, 1200, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 535, "WELL/BTC", "", "WELL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "WELL", "BTC", 'A', 1.0, 1e-1, 1000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 536, "BTT/ETH", "", "Blocktrade / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BTT", "ETH", 'A', 1.0, 1.0, 15, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 538, "PAT/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PAT", "ETH", 'A', 1.0, 1e-0, 60, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 539, "PAT/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PAT", "BTC", 'A', 1.0, 1e-0, 60, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 540, "DOV/ETH", "", "DOVU / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DOV", "ETH", 'A', 1.0, 1.0, 80, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 541, "DOV/BTC", "", "DOVU / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DOV", "BTC", 'A', 1.0, 1.0, 80, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 542, "PARETO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PARETO", "ETH", 'A', 1.0, 1e-0, 500, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 543, "PARETO/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PARETO", "BTC", 'A', 1.0, 1e-0, 500, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 544, "MOD/ETH", "", "Modum / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MOD", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 545, "COUP/ETH", "", "Coupit / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "COUP", "ETH", 'A', 1.0, 1.0, 110, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 546, "COUP/BTC", "", "Coupit / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "COUP", "BTC", 'A', 1.0, 1.0, 110, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 547, "CYFM/ETH", "", "CyberFM / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CYFM", "ETH", 'A', 1.0, 1.0, 100000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 548, "CYFM/BTC", "", "CyberFM / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CYFM", "BTC", 'A', 1.0, 1.0, 100000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 549, "BTT/BTC", "", "Blocktrade / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BTT", "BTC", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 551, "MIC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MIC", "ETH", 'A', 1.0, 1e-1, 250, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 552, "MIC/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MIC", "BTC", 'A', 1.0, 1e-1, 250, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 553, "DCN/ETH", "", "Dentacoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DCN", "ETH", 'A', 1.0, 1.0, 5000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 554, "DCN/BTC", "", "Dentacoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DCN", "BTC", 'A', 1.0, 1.0, 5000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 556, "PXT/BTC", "", "Populous XBRL Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PXT", "BTC", 'A', 1.0, 1.0, 35, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 557, "ASA/ETH", "", "ASA / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ASA", "ETH", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 558, "ASA/BTC", "", "ASA / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ASA", "BTC", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 559, "PAT/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PAT", "LA", 'A', 1.0, 1e-0, 60, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 560, "LOC/ETH", "", "LockTrip / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOC", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 561, "LOC/BTC", "", "LockTrip / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOC", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 562, "KRI/ETH", "", "Krios / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KRI", "ETH", 'A', 1.0, 1e-1, 1500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 564, "JSE/ETH", "", "JSEcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JSE", "ETH", 'A', 1.0, 1e-1, 3700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 568, "FAN/ETH", "", "Fanfare Global / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FAN", "ETH", 'A', 1.0, 1.0, 152, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 571, "MGX/ETH", "", "MegaX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MGX", "ETH", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 572, "MGX/BTC", "", "MegaX / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MGX", "BTC", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 575, "AMLT/ETH", "", "AMLT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMLT", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 576, "BDT/ETH", "", "Blockonix / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BDT", "ETH", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 579, "HIVE/ETH", "", "HIVE token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HIVE", "ETH", 'A', 1.0, 1e-1, 450, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 580, "HIVE/BTC", "", "HIVE token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HIVE", "BTC", 'A', 1.0, 1e-1, 450, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 581, "SPND/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SPND", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 582, "SPND/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SPND", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 583, "MFG/ETH", "", "SyncFab / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MFG", "ETH", 'A', 1.0, 1e-1, 2700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 586, "SCOI/ETH", "", "SprinkleCoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SCOI", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 587, "SCOI/BTC", "", "SprinkleCoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SCOI", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 588, "IFT/ETH", "", "InvestFeed / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IFT", "ETH", 'A', 1.0, 1e-1, 1000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 589, "IFT/BTC", "", "InvestFeed / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IFT", "BTC", 'A', 1.0, 1e-1, 1000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 592, "X8X/ETH", "", "X8X / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "X8X", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 593, "X8X/BTC", "", "X8X / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "X8X", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 602, "DEB/ETH", "", "Debitum / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DEB", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 616, "AMLT/BTC", "", "AMLT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMLT", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 618, "ROBET/BTC", "", "RoBET / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ROBET", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 619, "MITH/ETH", "", "Mithril / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MITH", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 620, "MITH/BTC", "", "Mithril / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MITH", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 629, "SYC/ETH", "", "SynchroCoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SYC", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 634, "MIR/ETH", "", "Mircoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MIR", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 635, "NWS/ETH", "", "NWS token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NWS", "ETH", 'A', 1.0, 1e-1, 1400, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 636, "NWS/BTC", "", "NWS token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NWS", "BTC", 'A', 1.0, 1e-1, 1400, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 637, "DPP/ETH", "", "DPP token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DPP", "ETH", 'A', 1.0, 1e-1, 850, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 638, "QNT/ETH", "", "Quant Network / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QNT", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 639, "QNT/BTC", "", "Quant Network / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QNT", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 640, "LMA/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LMA", "ETH", 'A', 1.0, 1e-1, 5000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 641, "LMA/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LMA", "BTC", 'A', 1.0, 1e-1, 5000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 643, "BBT/ETH", "", "BlockBooster Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BBT", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 644, "NOIZ/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "NOIZ", "ETH", 'A', 1.0, 1e-1, 15300, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 646, "ABX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ABX", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 647, "KRC7/ETH", "", "KURECOIN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KRC7", "ETH", 'A', 1.0, 1e-1, 300, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 648, "KRC7/BTC", "", "KURECOIN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KRC7", "BTC", 'A', 1.0, 1e-1, 300, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 649, "KNT/ETH", "", "Knekted / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KNT", "ETH", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 650, "KNT/BTC", "", "Knekted / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KNT", "BTC", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 651, "CCOS/ETH", "", "CCOS / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CCOS", "ETH", 'A', 1.0, 1e-1, 2000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 652, "CCL/ETH", "", "CYCLEAN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CCL", "ETH", 'A', 1.0, 1e-1, 7150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 653, "COU/ETH", "", "Couchain / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "COU", "ETH", 'A', 1.0, 1.0, 56100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 654, "XDCE/ETH", "", "Xinfin Network / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XDCE", "ETH", 'A', 1.0, 1e-1, 4200, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 655, "CCOS/BTC", "", "CCOS / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CCOS", "BTC", 'A', 1.0, 1e-1, 2000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 656, "CCL/BTC", "", "CYCLEAN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CCL", "BTC", 'A', 1.0, 1e-1, 7150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 658, "SYC/LA", "", "SynchroCoin / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SYC", "LA", 'A', 1.0, 1e-1, 0, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 659, "DPP/LA", "", "DPP token / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DPP", "LA", 'A', 1.0, 1e-1, 850, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 660, "CPX/ETH", "", "CPX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CPX", "ETH", 'A', 1.0, 1e-1, 300, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 661, "ECOM/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ECOM", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 662, "BNTE/ETH", "", "Bountie  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BNTE", "ETH", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 663, "QXE/ETH", "", "Quixxi Connect / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QXE", "ETH", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 664, "BITTO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BITTO", "ETH", 'A', 1.0, 1e-1, 20, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 665, "CLIN/ETH", "", "Clinicoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CLIN", "ETH", 'A', 1.0, 1e-1, 7300, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 666, "KNT/LA", "", "Knekted / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KNT", "LA", 'A', 1.0, 1e-1, 10000, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 668, "MCB/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MCB", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 669, "ECOM/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ECOM", "BTC", 'A', 1.0, 1e-0, 15, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 670, "XDCE/BTC", "", "Xinfin Network / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XDCE", "BTC", 'A', 1.0, 1.0, 420, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 671, "CVN/ETH", "", "CVCOIN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CVN", "ETH", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 672, "ZLA/ETH", "", "ZILLA Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ZLA", "ETH", 'A', 1.0, 1e-1, 250, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 673, "BITTO/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BITTO", "BTC", 'A', 1.0, 1e-0, 2, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 674, "CVN/BTC", "", "CVCOIN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CVN", "BTC", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 676, "PDI/ETH", "", "Pindex / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PDI", "ETH", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 677, "CSM/ETH", "", "CSM / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CSM", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 678, "PDI/BTC", "", "Pindex / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PDI", "BTC", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 679, "AL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "AL", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 680, "AL/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "AL", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 681, "GMB/ETH", "", "GMB / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GMB", "ETH", 'A', 1.0, 1e-1, 2940, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 683, "GEMA/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GEMA", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 685, "VIBE/ETH", "", "VIBE / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VIBE", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 688, "UTS/ETH", "", "UTEMIS / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UTS", "ETH", 'A', 1.0, 1.0, 380, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 695, "PYLNT/ETH", "", "Pylon Token  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PYLNT", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 696, "VIBE/BTC", "", "VIBE / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VIBE", "BTC", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 697, "NDX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "NDX", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 698, "NDX/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "NDX", "BTC", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 699, "VIBE/LA", "", "VIBE / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VIBE", "LA", 'A', 1.0, 1e-1, 150, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 700, "SWP/BTC", "", "Swapcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SWP", "BTC", 'A', 1.0, 1.0, 6, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 703, "PYLNT/BTC", "", "Pylon Token  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PYLNT", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 705, "UTD/ETH", "", "Airdrop United / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UTD", "ETH", 'A', 1.0, 1e-1, 8000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 708, "OAX/LA", "", "OAX / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OAX", "LA", 'A', 1.0, 1e-1, 30, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 709, "GUSD/ETH", "", "Gemini Dollar / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GUSD", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 711, "FOUR/ETH", "", "The 4th Pillar / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FOUR", "ETH", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 712, "FOUR/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "FOUR", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 713, "FOOD/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "FOOD", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 714, "FOOD/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "FOOD", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 715, "GUSD/BTC", "", "Gemini Dollar / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GUSD", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 716, "USDC/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "USDC", "BTC", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 717, "SCT/BTC", "", "Soma Community Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SCT", "BTC", 'A', 1.0, 1.0, 15, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 719, "CSM/BTC", "", "CSM / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CSM", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 720, "QURO/BTC", "", "Qurito / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QURO", "BTC", 'A', 1.0, 1.0, 45, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 721, "JSE/BTC", "", "JSEcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JSE", "BTC", 'A', 1.0, 1.0, 370, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 722, "TMT/ETH", "", "Traxia Foundation / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TMT", "ETH", 'A', 1.0, 1.0, 120, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 724, "VTHO/ETH", "", "VeThor Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VTHO", "ETH", 'A', 1.0, 1e-1, 12000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 725, "IVNT/ETH", "", "IVNT Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IVNT", "ETH", 'A', 1.0, 1.0, 220, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 726, "TAB/ETH", "", "TAB / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TAB", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 727, "PLAT/ETH", "", "BitGuild PLAT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PLAT", "ETH", 'A', 1.0, 1.0, 90, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 728, "LQD/ETH", "", "LQD / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LQD", "ETH", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 729, "AMB/ETH", "", "AMB / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMB", "ETH", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 731, "ATCC/BTC", "", "ATCCOIN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ATCC", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 732, "TYPE/ETH", "", "TYPE / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TYPE", "ETH", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 733, "MIO/ETH", "", "MIO / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MIO", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 734, "TYPE/BTC", "", "TYPE / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TYPE", "BTC", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 735, "CRYP/ETH", "", "CrypticCoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CRYP", "ETH", 'A', 1.0, 1.0, 135, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 736, "CRYP/BTC", "", "CrypticCoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CRYP", "BTC", 'A', 1.0, 1.0, 135, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 737, "BETR/ETH", "", "BetterBetting / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BETR", "ETH", 'A', 1.0, 1.0, 185, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 738, "GOX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GOX", "ETH", 'A', 1.0, 1e-0, 25, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 740, "KST/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "KST", "ETH", 'A', 1.0, 1e-0, 60, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 741, "KST/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "KST", "BTC", 'A', 1.0, 1e-0, 60, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 742, "PLAT/BTC", "", "BitGuild PLAT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PLAT", "BTC", 'A', 1.0, 1.0, 1550, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 743, "IXE/ETH", "", "IXE / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IXE", "ETH", 'A', 1.0, 1.0, 120, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 744, "RAI/ETH", "", "RAI / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RAI", "ETH", 'A', 1.0, 1.0, 87250, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 746, "VGW/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "VGW", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 747, "MEDIBIT/ETH", "", "MEDIBIT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MEDIBIT", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 748, "SRH/ETH", "", "SRCOIN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SRH", "ETH", 'A', 1.0, 1.0, 140, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 749, "WES/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WES", "ETH", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 751, "NHCT/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "NHCT", "ETH", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 753, "ROTO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ROTO", "ETH", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 754, "WES/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WES", "BTC", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 755, "RAI/BTC", "", "RAI / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RAI", "BTC", 'A', 1.0, 1.0, 87250, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 756, "ABX/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ABX", "BTC", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 759, "XNY/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XNY", "ETH", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 760, "XNY/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XNY", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 761, "SRH/BTC", "", "SRCOIN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SRH", "BTC", 'A', 1.0, 1.0, 140, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 762, "MEDIBIT/BTC", "", "MEDIBIT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MEDIBIT", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 764, "SHA/ETH", "", "SHA / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SHA", "ETH", 'A', 1.0, 1.0, 3900, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 765, "SCC/ETH", "", "SCC / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SCC", "ETH", 'A', 1.0, 1e-2, 2500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 766, "UTS/BTC", "", "UTEMIS / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UTS", "BTC", 'A', 1.0, 1.0, 380, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 768, "GBT/ETH", "", "Globatalent / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GBT", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 769, "SHA/BTC", "", "SHA / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SHA", "BTC", 'A', 1.0, 1.0, 3900, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 770, "GXC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GXC", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 772, "TOS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TOS", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 774, "IXE/BTC", "", "IXE / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IXE", "BTC", 'A', 1.0, 1.0, 120, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 775, "GBA/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GBA", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 779, "OAX/USDT", "", "OAX / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OAX", "USDT", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 780, "SGP/USDT", "", "SGPay Token / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SGP", "USDT", 'A', 1.0, 1.0, 15, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 781, "XDCE/USDT", "", "Xinfin Network / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XDCE", "USDT", 'A', 1.0, 1.0, 420, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 782, "HDR/ETH", "", "Hedger Tech / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HDR", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 783, "HERC/ETH", "", "HERC / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HERC", "ETH", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 784, "HERC/BTC", "", "HERC / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HERC", "BTC", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 785, "GEMA/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GEMA", "BTC", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 786, "GBPP/ETH", "", "Populous / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GBPP", "ETH", 'A', 1.0, 1.0, 3, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 787, "GBPP/BTC", "", "Populous / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GBPP", "BTC", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 788, "MIO/USDT", "", "MIO / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MIO", "USDT", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 789, "GBPP/USDT", "", "Populous / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GBPP", "USDT", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 790, "TRAK/ETH", "", "TRAK / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRAK", "ETH", 'A', 1.0, 1.0, 60, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 791, "HERC/USDT", "", "HERC / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HERC", "USDT", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 792, "GRET/ETH", "", "Global REIT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GRET", "ETH", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 794, "XRTC/ETH", "", "XRTC / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XRTC", "ETH", 'A', 1.0, 1.0, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 795, "ROTO/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ROTO", "BTC", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 796, "KNT/USDT", "", "Knekted / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KNT", "USDT", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 797, "PSK/ETH", "", "Pool of stake / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PSK", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 798, "BDC/ETH", "", "Bitdeal Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BDC", "ETH", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 799, "BDC/BTC", "", "Bitdeal Coin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BDC", "BTC", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 800, "LTR/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LTR", "ETH", 'A', 1.0, 1e-0, 2000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 801, "RMT/ETH", "", "RMT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RMT", "ETH", 'A', 1.0, 1.0, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 802, "HDR/BTC", "", "Hedger Tech / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HDR", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 803, "NZO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "NZO", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 804, "NZO/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "NZO", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 805, "EYC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "EYC", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 806, "EYC/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "EYC", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 807, "WNC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WNC", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 808, "WNC/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WNC", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 809, "TRAK/BTC", "", "TRAK / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRAK", "BTC", 'A', 1.0, 1.0, 60, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 810, "HLL/ETH", "", "HotelLoad / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HLL", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 812, "OVC/ETH", "", "OVCODE / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OVC", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 814, "BCQR/BTC", "", "BCQR / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BCQR", "BTC", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 815, "XLM/ETH", "", "Stellar Lumens / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XLM", "ETH", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 816, "XLM/BTC", "", "Stellar Lumens / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XLM", "BTC", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 817, "RMT/BTC", "", "RMT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RMT", "BTC", 'A', 1.0, 1.0, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 818, "RMT/USDT", "", "RMT / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RMT", "USDT", 'A', 1.0, 1.0, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 819, "PSK/BTC", "", "Pool of stake / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PSK", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 820, "RMT/LA", "", "RMT / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RMT", "LA", 'A', 1.0, 1e-1, 6000, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 821, "LTR/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LTR", "BTC", 'A', 1.0, 1e-0, 2000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 822, "SREUR/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SREUR", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 824, "IDEAL/ETH", "", "IDEAL Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IDEAL", "ETH", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 827, "CRF/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CRF", "ETH", 'A', 1.0, 1e-0, 35, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 830, "REL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "REL", "ETH", 'A', 1.0, 1e-0, 30, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 831, "YPTKN/ETH", "", "YPTKN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "YPTKN", "ETH", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 832, "GIG/ETH", "", "GigEcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GIG", "ETH", 'A', 1.0, 1e-1, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 833, "CLT/ETH", "", "Coinloan / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CLT", "ETH", 'A', 1.0, 1.0, 6, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 834, "NKD/ETH", "", "Naked Technology / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NKD", "ETH", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 835, "BBRT/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BBRT", "ETH", 'A', 1.0, 1e-0, 5, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 836, "REL/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "REL", "BTC", 'A', 1.0, 1e-0, 30, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 837, "YPTKN/BTC", "", "YPTKN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "YPTKN", "BTC", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 838, "HYT/ETH", "", "HoryouToken / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HYT", "ETH", 'A', 1.0, 1.0, 33, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 839, "GZB/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GZB", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 840, "GZB/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GZB", "LA", 'A', 1.0, 1e-0, 1, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 841, "JVY/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "JVY", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 842, "JVY/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "JVY", "LA", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 843, "HYT/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "HYT", "LA", 'A', 1.0, 1e-0, 33, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 844, "CLT/USDT", "", "Coinloan / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CLT", "USDT", 'A', 1.0, 1.0, 6, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 845, "BXT/ETH", "", "Bitfxt coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BXT", "ETH", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 846, "BXT/LA", "", "Bitfxt coin / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BXT", "LA", 'A', 1.0, 1e-1, 50, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 847, "GBT/BTC", "", "Globatalent / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GBT", "BTC", 'A', 1.0, 1.0, 44, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 848, "GBT/USDT", "", "Globatalent / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GBT", "USDT", 'A', 1.0, 1.0, 44, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 849, "VC/ETH", "", "Voicecoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VC", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 850, "PLTC/ETH", "", "PLTC / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PLTC", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 851, "BBRT/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BBRT", "BTC", 'A', 1.0, 1e-0, 5, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 853, "AEN/BTC", "", "Aenco Solutions / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AEN", "BTC", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 854, "AEN/USDT", "", "Aenco Solutions / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AEN", "USDT", 'A', 1.0, 1.0, 20, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 855, "KON/BTC", "", "Konios Project / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KON", "BTC", 'A', 1.0, 1.0, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 856, "KON/USDT", "", "Konios Project / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KON", "USDT", 'A', 1.0, 1.0, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 857, "GRET/USDT", "", "Global REIT / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GRET", "USDT", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 858, "PARETO/USDT", "", "PARETO / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PARETO", "USDT", 'A', 1.0, 1.0, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 859, "TYPE/USDT", "", "TYPE / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TYPE", "USDT", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 860, "GHFGH/LA", "", "testtokenname291118 / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GHFGH", "LA", 'A', 1.0, 1e-4, 20000, 1e-2, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 861, "BOB/ETH", "", "Bobsrepair / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BOB", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 862, "KZE/ETH", "", "KZE / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KZE", "ETH", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 863, "VC/BTC", "", "Voicecoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VC", "BTC", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 864, "NAM/ETH", "", "Namacoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NAM", "ETH", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 865, "ICT/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ICT", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 866, "BOB/BTC", "", "Bobsrepair / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BOB", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 867, "BOB/USDT", "", "Bobsrepair / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BOB", "USDT", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 868, "TAB/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TAB", "BTC", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 869, "TAB/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TAB", "USDT", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 870, "GIG/BTC", "", "GigEcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GIG", "BTC", 'A', 1.0, 1e-1, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 871, "GIG/USDT", "", "GigEcoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GIG", "USDT", 'A', 1.0, 1e-1, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 872, "CYS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CYS", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 873, "VES/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "VES", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 874, "CYS/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CYS", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 875, "FOUR/BTC", "", "The 4th Pillar / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FOUR", "BTC", 'A', 1.0, 1.0, 50, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 876, "XYO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XYO", "ETH", 'A', 1.0, 1e-0, 250, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 877, "TXC/ETH", "", "TenXcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TXC", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 878, "JOYS/ETH", "", "Joys digital / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JOYS", "ETH", 'A', 1.0, 1.0, 15, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 879, "JOYS/BTC", "", "Joys digital / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JOYS", "BTC", 'A', 1.0, 1.0, 15, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 880, "JOYS/USDT", "", "Joys digital / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JOYS", "USDT", 'A', 1.0, 1.0, 15, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 881, "JOYS/LA", "", "Joys digital / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JOYS", "LA", 'A', 1.0, 1.0, 15, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 882, "XYO/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XYO", "BTC", 'A', 1.0, 1e-0, 250, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 883, "ATR/ETH", "", "AsterionSpace / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ATR", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 884, "AMC/ETH", "", "AMCCoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMC", "ETH", 'A', 1.0, 1.0, 1, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 885, "SPRTZ/ETH", "", "SpritzCoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SPRTZ", "ETH", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 886, "SNPC/ETH", "", "Snapparazzi / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNPC", "ETH", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 887, "TXC/BTC", "", "TenXcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TXC", "BTC", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 888, "TXC/USDT", "", "TenXcoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TXC", "USDT", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 889, "ATR/BTC", "", "AsterionSpace / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ATR", "BTC", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 890, "ATR/USDT", "", "AsterionSpace / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ATR", "USDT", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 891, "XYO/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XYO", "USDT", 'A', 1.0, 1e-0, 250, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 892, "KZE/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "KZE", "BTC", 'A', 1.0, 1e-4, 10000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 893, "ARTID/ETH", "", "ARTID / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARTID", "ETH", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 896, "SPRTZ/BTC", "", "SpritzCoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SPRTZ", "BTC", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 898, "INCO/USDT", "", "Incodium / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "INCO", "USDT", 'A', 1.0, 1.0, 150, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 899, "ENTR/ETH", "", "Entrade / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ENTR", "ETH", 'A', 1.0, 1.0, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 900, "GGFGF/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GGFGF", "BTC", 'A', 1.0, 1e-4, 10000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 902, "ENTR/BTC", "", "Entrade / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ENTR", "BTC", 'A', 1.0, 1.0, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 903, "ENTR/USDT", "", "Entrade / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ENTR", "USDT", 'A', 1.0, 1.0, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 906, "QXE/BTC", "", "Quixxi Connect / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QXE", "BTC", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 907, "PDRY/ETH", "", "PANDROYTY / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PDRY", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 908, "TEL/ETH", "", "Telcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TEL", "ETH", 'A', 1.0, 1.0, 3000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 909, "SCC/BTC", "", "SCC / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SCC", "BTC", 'A', 1.0, 1e-2, 2500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 910, "SCC/USDT", "", "SCC / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SCC", "USDT", 'A', 1.0, 1e-2, 2500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 911, "SNPC/BTC", "", "Snapparazzi / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNPC", "BTC", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 912, "SNPC/USDT", "", "Snapparazzi / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNPC", "USDT", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 913, "USDC/USDT", "", "USDC / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "USDC", "USDT", 'A', 1.0, 1e-2, 100, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 914, "XBASE/ETH", "", "Eterbase / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XBASE", "ETH", 'A', 1.0, 1.0, 200, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 916, "NRVE/BTC", "", "NRVE / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NRVE", "BTC", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 917, "NRVE/USDT", "", "NRVE / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NRVE", "USDT", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 918, "TYPE/LA", "", "TYPE / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TYPE", "LA", 'A', 1.0, 1e-1, 10000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 919, "PARETO/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PARETO", "LA", 'A', 1.0, 1e-1, 5000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 920, "CWEX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CWEX", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 921, "XBASE/BTC", "", "Eterbase / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XBASE", "BTC", 'A', 1.0, 1.0, 200, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 922, "HTL/BTC", "", "Hotelium / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HTL", "BTC", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 923, "GPYX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GPYX", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 927, "XGT/ETH", "", "XGT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XGT", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 931, "HYT/BTC", "", "HoryouToken / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HYT", "BTC", 'A', 1.0, 1.0, 33, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 932, "MBIT/BTC", "", "MBIT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MBIT", "BTC", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 933, "MBIT/ETH", "", "MBIT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MBIT", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 934, "BIO/ETH", "", "BioCrypt / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BIO", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 935, "FORCE/ETH", "", "FORCE / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FORCE", "ETH", 'A', 1.0, 1.0, 7, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 936, "BAX/ETH", "", "BAX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BAX", "ETH", 'A', 1.0, 1.0, 2500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 937, "PCPIE/BTC", "", "PCPIE / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PCPIE", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 938, "QNTU/ETH", "", "QNTU / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QNTU", "ETH", 'A', 1.0, 1.0, 5000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 939, "SBTC/BTC", "", "Siambitcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SBTC", "BTC", 'A', 1.0, 1e-5, 1, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 940, "SBTC/USDT", "", "Siambitcoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SBTC", "USDT", 'A', 1.0, 1e-5, 1, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 941, "SBTC/LA", "", "Siambitcoin / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SBTC", "LA", 'A', 1.0, 1e-5, 1, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 942, "S4F/ETH", "", "S4F / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "S4F", "ETH", 'A', 1.0, 1e-1, 80, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 943, "LIBER/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LIBER", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 945, "BAX/BTC", "", "BAX / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BAX", "BTC", 'A', 1.0, 1.0, 2500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 946, "BAX/USDT", "", "BAX / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BAX", "USDT", 'A', 1.0, 1.0, 2500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 947, "XGT/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XGT", "BTC", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 948, "XGT/USDT", "", "XGT / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XGT", "USDT", 'A', 1.0, 1.0, 10, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 949, "CPMS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CPMS", "ETH", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 950, "CPMS/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CPMS", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 951, "TEL/BTC", "", "Telcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TEL", "BTC", 'A', 1.0, 1.0, 3000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 952, "TEL/USDT", "", "Telcoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TEL", "USDT", 'A', 1.0, 1.0, 3000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 953, "S4F/LA", "", "S4F / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "S4F", "LA", 'A', 1.0, 1e-1, 80, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 954, "DLSD/ETH", "", "The Deal Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DLSD", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 955, "TCAT/ETH", "", "TCAT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TCAT", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 956, "SET/ETH", "", "SET / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SET", "ETH", 'A', 1.0, 1.0, 12, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 958, "EMPC/ETH", "", "Empire Cash / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EMPC", "ETH", 'A', 1.0, 1.0, 12, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 959, "SHEEP/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SHEEP", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 960, "MBIT/USDT", "", "MBIT / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MBIT", "USDT", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 961, "BRC/ETH", "", "BRC / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BRC", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 962, "UAP/ETH", "", "UAP / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UAP", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 963, "DLSD/BTC", "", "The Deal Coin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DLSD", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 964, "DLSD/USDT", "", "The Deal Coin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DLSD", "USDT", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 965, "TCAT/USDT", "", "TCAT / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TCAT", "USDT", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 966, "QNTU/BTC", "", "QNTU / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QNTU", "BTC", 'A', 1.0, 1.0, 5000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 967, "BRC/BTC", "", "BRC / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BRC", "BTC", 'A', 1.0, 1.0, 22, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 968, "BRC/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BRC", "USDT", 'A', 1.0, 1e-0, 22, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 969, "AFIN/ETH", "", "AFIN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AFIN", "ETH", 'A', 1.0, 1.0, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 970, "EDR/ETH", "", "EDR / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EDR", "ETH", 'A', 1.0, 1e-1, 130, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 971, "AMP/ETH", "", "AMP / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMP", "ETH", 'A', 1.0, 1e-1, 700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 972, "HLL/BTC", "", "HotelLoad / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HLL", "BTC", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 973, "HLL/USDT", "", "HotelLoad / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HLL", "USDT", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 974, "HALO/ETH", "", "Halo Platform / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HALO", "ETH", 'A', 1.0, 1.0, 2000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 975, "HALO/BTC", "", "Halo Platform / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HALO", "BTC", 'A', 1.0, 1.0, 2000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 976, "HALO/USDT", "", "Halo Platform / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HALO", "USDT", 'A', 1.0, 1.0, 2000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 977, "TRADE/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TRADE", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 979, "AGO/ETH", "", "AGO / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AGO", "ETH", 'A', 1.0, 1e-1, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 980, "UAP/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "UAP", "USDT", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 981, "EDR/USDT", "", "EDR / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EDR", "USDT", 'A', 1.0, 1e-1, 130, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 982, "ECTE/BTC", "", "ECTE / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ECTE", "BTC", 'A', 1.0, 1e-1, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 984, "FREE/USDT", "", "FREE / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FREE", "USDT", 'A', 1.0, 1.0, 1000000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 985, "ZLC/ETH", "", "ZLC / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ZLC", "ETH", 'A', 1.0, 1e-1, 2500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 986, "XID/ETH", "", "XID / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XID", "ETH", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 987, "VTM/ETH", "", "VTM / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VTM", "ETH", 'A', 1.0, 1e-1, 170, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 988, "AFIN/BTC", "", "AFIN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AFIN", "BTC", 'A', 1.0, 1.0, 30, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 989, "AFIN/USDT", "", "AFIN / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AFIN", "USDT", 'A', 1.0, 1e-2, 300, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 990, "WRL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WRL", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 991, "CRWD/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CRWD", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 992, "ZLC/BTC", "", "ZLC / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ZLC", "BTC", 'A', 1.0, 1e-1, 2500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 993, "ZLC/USDT", "", "ZLC / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ZLC", "USDT", 'A', 1.0, 1e-1, 2500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 994, "VTM/BTC", "", "VTM / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VTM", "BTC", 'A', 1.0, 1e-1, 170, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 995, "TRYL/BTC", "", "TRYL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRYL", "BTC", 'A', 1.0, 1e-1, 52, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 996, "TRYL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TRYL", "ETH", 'A', 1.0, 1e-1, 52, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 997, "USDL/BTC", "", "USDL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "USDL", "BTC", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 998, "USDL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "USDL", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 999, "RUPL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "RUPL", "ETH", 'A', 1.0, 1e-1, 710, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1000, "ARSL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ARSL", "ETH", 'A', 1.0, 1e-1, 370, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1001, "ARSL/BTC", "", "ARSL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARSL", "BTC", 'A', 1.0, 1e-1, 370, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1002, "GBPL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GBPL", "ETH", 'A', 1.0, 1e-1, 8, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1003, "GBPL/BTC", "", "GBPL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GBPL", "BTC", 'A', 1.0, 1e-1, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1004, "RUPL/BTC", "", "RUPL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RUPL", "BTC", 'A', 1.0, 1e-1, 710, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1006, "BRLL/BTC", "", "BRLL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BRLL", "BTC", 'A', 1.0, 1e-1, 40, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1008, "EURL/BTC", "", "EURL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EURL", "BTC", 'A', 1.0, 1e-1, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1009, "VNDL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "VNDL", "ETH", 'A', 1.0, 1e-1, 250000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1010, "VNDL/BTC", "", "VNDL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VNDL", "BTC", 'A', 1.0, 1e-1, 250000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1011, "MYN/ETH", "", "MYN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MYN", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1012, "XID/USDT", "", "XID / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XID", "USDT", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1013, "GAGA/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GAGA", "LA", 'A', 1.0, 1e-4, 1000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1014, "FFDDF/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "FFDDF", "LA", 'A', 1.0, 1e-4, 1000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1015, "ZZJK/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ZZJK", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1016, "VGW/BTC", "", "VegaWallet / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VGW", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1017, "XLM/USDT", "", "Stellar Lumens / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XLM", "USDT", 'A', 1.0, 1e-1, 50, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1018, "OVC/BTC", "", "OVCODE / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OVC", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1019, "OVC/USDT", "", "OVCODE / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "OVC", "USDT", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1020, "PTFYW/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PTFYW", "LA", 'A', 1.0, 1e-4, 10000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1021, "ZZZZ/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ZZZZ", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1022, "XNY/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XNY", "BTC", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1023, "XNY/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XNY", "USDT", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1024, "KEY/ETH", "", "Selfkey / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KEY", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1025, "KEY/BTC", "", "Selfkey / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KEY", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1026, "KEY/USDT", "", "Selfkey / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KEY", "USDT", 'A', 1.0, 1.0, 385, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1027, "PLA/ETH", "", "PlayChip / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PLA", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1028, "PLA/BTC", "", "PlayChip / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PLA", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1029, "PLA/USDT", "", "PlayChip / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PLA", "USDT", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1031, "RYO/ETH", "", "c0ban / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RYO", "ETH", 'A', 1.0, 1e-3, 100, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1032, "RYO/BTC", "", "c0ban / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RYO", "BTC", 'A', 1.0, 1e-3, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1033, "RYO/USDT", "", "c0ban / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RYO", "USDT", 'A', 1.0, 1e-3, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1034, "BEZ/BTC", "", "BEZOP / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BEZ", "BTC", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1035, "ABCDS/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ABCDS", "LA", 'A', 1.0, 1e-4, 10000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1036, "S4F/BTC", "", "S4F / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "S4F", "BTC", 'A', 1.0, 1.0, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1037, "B2G/ETH", "", "Bitcoiin2Gen / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "B2G", "ETH", 'A', 1.0, 1.0, 7, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1038, "KSKSK/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "KSKSK", "USDT", 'A', 1.0, 1e-4, 10000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1039, "KSKSK/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "KSKSK", "LA", 'A', 1.0, 1e-4, 10000, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1040, "URUN/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "URUN", "BTC", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1041, "URUN/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "URUN", "USDT", 'A', 1.0, 1e-0, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1042, "GOLDL/BTC", "", "GOLDL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GOLDL", "BTC", 'A', 1.0, 1e-3, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1043, "CHFL/BTC", "", "CHFL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CHFL", "BTC", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1045, "AAPLL/BTC", "", "AAPLL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AAPLL", "BTC", 'A', 1.0, 1e-3, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1046, "BABAL/BTC", "", "BABAL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BABAL", "BTC", 'A', 1.0, 1e-3, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1047, "WWW/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WWW", "BTC", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1048, "WWW/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WWW", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1049, "KTO/ETH", "", "KTO / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KTO", "ETH", 'A', 1.0, 1e-2, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1050, "GGHHH/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GGHHH", "LA", 'A', 1.0, 1e-8, 999, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1051, "ARB/ETH", "", "ARB / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARB", "ETH", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1052, "KTO/BTC", "", "KTO / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KTO", "BTC", 'A', 1.0, 1e-2, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1053, "ALDX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ALDX", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1054, "ALDX/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ALDX", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1055, "WRL/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WRL", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1056, "CRWD/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CRWD", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1057, "CCOIN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CCOIN", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1058, "CCOIN/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CCOIN", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1059, "TRADE/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TRADE", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1060, "TIIM/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TIIM", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1061, "TIIM/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TIIM", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1062, "XCEL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XCEL", "ETH", 'A', 1.0, 1e-1, 13, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1063, "XCEL/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XCEL", "LA", 'A', 1.0, 1e-1, 13, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1064, "TM/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TM", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1065, "TM/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TM", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1066, "DXG/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "DXG", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1067, "DXG/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "DXG", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1068, "ZZZ/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ZZZ", "ETH", 'A', 1.0, 1e-1, 10, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1069, "ARB/BTC", "", "ARB / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARB", "BTC", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1070, "ARB/USDT", "", "ARB / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARB", "USDT", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1071, "JOY/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "JOY", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1072, "NAM/BTC", "", "Namacoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NAM", "BTC", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1073, "PRSN/ETH", "", "Persona platform / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PRSN", "ETH", 'A', 1.0, 1e-1, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1074, "SSS/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SSS", "LA", 'A', 1.0, 1e-4, 0, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1075, "JJJJ/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "JJJJ", "USDT", 'A', 1.0, 1e-4, 1, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1076, "B2G/USDT", "", "Bitcoiin2Gen / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "B2G", "USDT", 'A', 1.0, 1.0, 7, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1077, "B2G/BTC", "", "Bitcoiin2Gen / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "B2G", "BTC", 'A', 1.0, 1.0, 7, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1078, "PRSN/BTC", "", "Persona platform / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PRSN", "BTC", 'A', 1.0, 1e-1, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1079, "PRSN/USDT", "", "Persona platform / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PRSN", "USDT", 'A', 1.0, 1e-1, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1080, "EDR/BTC", "", "EDR / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EDR", "BTC", 'A', 1.0, 1e-1, 130, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1081, "2222/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "2222", "ETH", 'A', 1.0, 1e-2, 100, 1e-6, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1082, "LATX/ETH", "", "LATX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LATX", "ETH", 'A', 1.0, 1.0, 106, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1083, "333/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "333", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1084, "111/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "111", "LA", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1085, "SPL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SPL", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1087, "DDDD/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "DDDD", "LA", 'A', 1.0, 1e-8, 999, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1088, "GFFGF/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GFFGF", "LA", 'A', 1.0, 1e-8, 799999999, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1089, "EMPC/BTC", "", "Empire Cash / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EMPC", "BTC", 'A', 1.0, 1.0, 12, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1090, "EMPC/USDT", "", "Empire Cash / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EMPC", "USDT", 'A', 1.0, 1.0, 12, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1091, "AMP/BTC", "", "AMP / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMP", "BTC", 'A', 1.0, 1e-1, 700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1092, "AMP/USDT", "", "AMP / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMP", "USDT", 'A', 1.0, 1e-1, 700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1093, "EVY/ETH", "", "EVY / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EVY", "ETH", 'A', 1.0, 1.0, 750, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1094, "USAT/BTC", "", "USAT  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "USAT", "BTC", 'A', 1.0, 1.0, 4, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1095, "ARAW/ETH", "", "ARAW / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARAW", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1096, "ACAD/ETH", "", "ACAD / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ACAD", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1097, "ARA/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ARA", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1098, "EVY/BTC", "", "EVY / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EVY", "BTC", 'A', 1.0, 1.0, 750, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1099, "EVY/USDT", "", "EVY / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EVY", "USDT", 'A', 1.0, 1.0, 750, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1100, "LATX/BTC", "", "LATX / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LATX", "BTC", 'A', 1.0, 1.0, 106, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1101, "LATX/USDT", "", "LATX / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LATX", "USDT", 'A', 1.0, 1.0, 106, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1102, "EVY/LA", "", "EVY / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "EVY", "LA", 'A', 1.0, 1.0, 750, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1103, "ARAW/BTC", "", "ARAW / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARAW", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1104, "ARAW/USDT", "", "ARAW / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ARAW", "USDT", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1105, "MODEX/ETH", "", "MODEX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MODEX", "ETH", 'A', 1.0, 1.0, 6, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1106, "MODEX/BTC", "", "MODEX / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MODEX", "BTC", 'A', 1.0, 1.0, 6, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1107, "XKN/ETH", "", "KOIN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XKN", "ETH", 'A', 1.0, 1e-3, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1108, "KGSL/ETH", "", "KYRGYZSOMcrncyLATOKEN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KGSL", "ETH", 'A', 1.0, 1.0, 70, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1109, "KGSL/BTC", "", "KYRGYZSOMcrncyLATOKEN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KGSL", "BTC", 'A', 1.0, 1.0, 70, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1110, "WWZ/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WWZ", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1111, "XKN/BTC", "", "KOIN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XKN", "BTC", 'A', 1.0, 1e-3, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1112, "XKN/USDT", "", "KOIN / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XKN", "USDT", 'A', 1.0, 1e-3, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1113, "ACAD/BTC", "", "ACAD / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ACAD", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1114, "ACAD/USDT", "", "ACAD / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ACAD", "USDT", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1115, "CAL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CAL", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1117, "POLY/BTC", "", "Poly / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "POLY", "BTC", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1118, "QUROZ/ETH", "", "QFORA / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QUROZ", "ETH", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1119, "QCX/ETH", "", "Quick X / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QCX", "ETH", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1120, "AID/ETH", "", "AIDUS PROJECT  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AID", "ETH", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1121, "MPAY/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MPAY", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1122, "AZS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "AZS", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1123, "TRP/ETH", "", "Tronipay / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRP", "ETH", 'A', 1.0, 1e-1, 20, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1124, "QCX/BTC", "", "Quick X / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QCX", "BTC", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1125, "QCX/USDT", "", "Quick X / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QCX", "USDT", 'A', 1.0, 1.0, 20, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1126, "QUROZ/BTC", "", "QFORA / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QUROZ", "BTC", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1127, "AID/BTC", "", "AIDUS PROJECT  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AID", "BTC", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1128, "AID/USDT", "", "AIDUS PROJECT  / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AID", "USDT", 'A', 1.0, 1e-1, 150, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1129, "BUD/ETH", "", "BUD / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BUD", "ETH", 'A', 1.0, 1e-1, 400, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1130, "BUD/BTC", "", "BUD / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BUD", "BTC", 'A', 1.0, 1e-1, 400, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1131, "BUD/USDT", "", "BUD / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BUD", "USDT", 'A', 1.0, 1e-1, 400, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1132, "XRM/ETH", "", "Aerum / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XRM", "ETH", 'A', 1.0, 1e-1, 830, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1131, "BUD/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BUD", "USDT", 'A', 1.0, 1e-1, 400, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1133, "PLAN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PLAN", "ETH", 'A', 1.0, 1e-0, 136, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1134, "KIN/ETH", "", "KIN blockchain / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KIN", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1135, "GOT/BTC", "", "GOeureka / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GOT", "BTC", 'A', 1.0, 1e-4, 70000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1136, "XGOTX/BTC", "", "GoToken / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XGOTX", "BTC", 'A', 1.0, 1e-4, 150000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1137, "KIN/BTC", "", "KIN blockchain / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KIN", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1138, "KIN/USDT", "", "KIN blockchain / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "KIN", "USDT", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1139, "UTD/BTC", "", "Airdrop United / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UTD", "BTC", 'A', 1.0, 1.0, 800, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1140, "UTD/USDT", "", "Airdrop United / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "UTD", "USDT", 'A', 1.0, 1.0, 800, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1141, "MBMT/ETH", "", "MBMT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MBMT", "ETH", 'A', 1.0, 1e-3, 20, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1143, "AMC/BTC", "", "AMCCoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AMC", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1144, "PLAN/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PLAN", "LA", 'A', 1.0, 1e-0, 164, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1145, "JCT/ETH", "", "JCT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JCT", "ETH", 'A', 1.0, 1e-2, 3300, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1146, "OREO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "OREO", "ETH", 'A', 1.0, 1e-0, 1, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1147, "JCT/BTC", "", "JCT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JCT", "BTC", 'A', 1.0, 1e-2, 3300, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1148, "JCT/USDT", "", "JCT / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "JCT", "USDT", 'A', 1.0, 1e-2, 3300, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1149, "WHN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WHN", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1150, "TGN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TGN", "ETH", 'A', 1.0, 1e-3, 1000, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1151, "TGN/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TGN", "LA", 'A', 1.0, 1e-3, 1000, 1e-5, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1153, "BCEO/ETH", "", "BitCEO / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BCEO", "ETH", 'A', 1.0, 1e-2, 8300, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1156, "TMNK/ETH", "", "Trademonk / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TMNK", "ETH", 'A', 1.0, 1e-1, 25, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1157, "BCEO/BTC", "", "BitCEO / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BCEO", "BTC", 'A', 1.0, 1e-2, 8300, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1158, "VTEX/ETH", "", "Vertex Market / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VTEX", "ETH", 'A', 1.0, 1e-2, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1159, "IPUX/ETH", "", "IPUX / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IPUX", "ETH", 'A', 1.0, 1e-2, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1160, "TC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TC", "ETH", 'A', 1.0, 1e-1, 1000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1161, "GMB/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GMB", "BTC", 'A', 1.0, 1e-2, 29400, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1162, "VTEX/USDT", "", "Vertex Market / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VTEX", "USDT", 'A', 1.0, 1e-2, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1163, "ROC2/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ROC2", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1164, "CIA/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CIA", "ETH", 'A', 1.0, 1e-1, 1000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1165, "MAC/ETH", "", "Matrexcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MAC", "ETH", 'A', 1.0, 1e-2, 800, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1166, "MNC/ETH", "", "Maincoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MNC", "ETH", 'A', 1.0, 1e-2, 6300, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1167, "MAC/BTC", "", "Matrexcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MAC", "BTC", 'A', 1.0, 1e-2, 800, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1168, "MAC/USDT", "", "Matrexcoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MAC", "USDT", 'A', 1.0, 1e-2, 800, 1e-4, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1169, "GOX/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GOX", "BTC", 'A', 1.0, 1e-1, 250, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1170, "GOX/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GOX", "USDT", 'A', 1.0, 1e-1, 250, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1171, "DIO/ETH", "", "DECIMATED / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DIO", "ETH", 'A', 1.0, 1e-2, 1700, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1172, "DIO/BTC", "", "DECIMATED / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DIO", "BTC", 'A', 1.0, 1e-2, 1700, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1173, "DIO/USDT", "", "DECIMATED / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DIO", "USDT", 'A', 1.0, 1e-2, 1700, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1174, "REB/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "REB", "ETH", 'A', 1.0, 1e-2, 100, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1175, "GFC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GFC", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1176, "LEDU/ETH", "", " LEDU / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LEDU", "ETH", 'A', 1.0, 1e-2, 100000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1177, "VIDT/ETH", "", "V-ID / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VIDT", "ETH", 'A', 1.0, 1e-1, 170, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1178, "VIDT/BTC", "", "V-ID / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VIDT", "BTC", 'A', 1.0, 1e-1, 170, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1179, "VIDT/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "VIDT", "USDT", 'A', 1.0, 1e-1, 170, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1180, "7E/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "7E", "ETH", 'A', 1.0, 1e-1, 50000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1181, "LEDU/BTC", "", " LEDU / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LEDU", "BTC", 'A', 1.0, 1e-2, 100000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1182, "MCAN/ETH", "", "Medican coin  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCAN", "ETH", 'A', 1.0, 1e-2, 9400, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1183, "XSAT/BTC", "", "Saturn Black  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XSAT", "BTC", 'A', 1.0, 1e-2, 700, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1184, "MCAN/BTC", "", "Medican coin  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCAN", "BTC", 'A', 1.0, 1e-2, 9400, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1185, "MCAN/USDT", "", "Medican coin  / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCAN", "USDT", 'A', 1.0, 1e-2, 9400, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1186, "XSAT/USDT", "", "Saturn Black  / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XSAT", "USDT", 'A', 1.0, 1e-2, 700, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1187, "SG/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SG", "ETH", 'A', 1.0, 1e-4, 10000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1188, "LEDU/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LEDU", "USDT", 'A', 1.0, 1e-0, 1000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1189, "AZP/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "AZP", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1190, "ZAW/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ZAW", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1191, "QQQ/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "QQQ", "LA", 'A', 1.0, 1e-1, 10, 1e-4, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1192, "7E/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "7E", "LA", 'A', 1.0, 1e-1, 50000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1193, "WBY/ETH", "", "WeBuy / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "WBY", "ETH", 'A', 1.0, 1e-1, 40, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1194, "MNC/BTC", "", "Maincoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MNC", "BTC", 'A', 1.0, 1e-2, 6300, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1195, "AMS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "AMS", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1196, "AGRO/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "AGRO", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1197, "BOTX/ETH", "", "BotXcoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BOTX", "ETH", 'A', 1.0, 1e-1, 700, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1198, "PHM/BTC", "", "Phoneum / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PHM", "BTC", 'A', 1.0, 1e-2, 5000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1199, "PHM/USDT", "", "Phoneum / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PHM", "USDT", 'A', 1.0, 1e-1, 500, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1200, "WBY/BTC", "", "WeBuy / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "WBY", "BTC", 'A', 1.0, 1e-1, 40, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1201, "BOTX/BTC", "", "BotXcoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BOTX", "BTC", 'A', 1.0, 1e-1, 700, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1202, "BOTX/USDT", "", "BotXcoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BOTX", "USDT", 'A', 1.0, 1e-1, 700, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1203, "BAT/BTC", "", "Basic Attention Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BAT", "BTC", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1205, "REP/BTC", "", "Augur / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "REP", "BTC", 'A', 1.0, 1e-3, 100, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1206, "ERA/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ERA", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1207, "HOT/BTC", "", "Holo / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HOT", "BTC", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1208, "AE/BTC", "", "Aeternity / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "AE", "BTC", 'A', 1.0, 1e-2, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1209, "NPXS/BTC", "", "Pundi X / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NPXS", "BTC", 'A', 1.0, 1.0, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1210, "LOOM/BTC", "", "Loom Network / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOOM", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1211, "MCO/BTC", "", "Crypto.com / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCO", "BTC", 'A', 1.0, 1e-2, 10, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1212, "DPT/ETH", "", "DPT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DPT", "ETH", 'A', 1.0, 1e-1, 2, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1213, "USAT/LA", "", "USAT  / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "USAT", "LA", 'A', 1.0, 1.0, 4, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1215, "DPT/BTC", "", "DPT / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DPT", "BTC", 'A', 1.0, 1e-1, 2, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1216, "DPT/USDT", "", "DPT / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DPT", "USDT", 'A', 1.0, 1e-1, 2, 1e-3, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1217, "QKC/BTC", "", "Quarkchain / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "QKC", "BTC", 'A', 1.0, 1e-1, 1000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1218, "UBE/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "UBE", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1219, "MCA/ETH", "", "MECA Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCA", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1220, "RALLY/ETH", "", "Rally / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RALLY", "ETH", 'A', 1.0, 1e-1, 5500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1221, "GTP/ETH", "", "Tron Game Global Pay / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GTP", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1222, "SWM/ETH", "", "SWARM / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SWM", "ETH", 'A', 1.0, 1e-1, 110, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1223, "CDAG/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CDAG", "ETH", 'A', 1.0, 1e-1, 0, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1224, "SEA/ETH", "", "Galleon Quest / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SEA", "ETH", 'A', 1.0, 1e-1, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1225, "MCA/ICX", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MCA", "ICX", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1226, "SEA/BTC", "", "Galleon Quest / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SEA", "BTC", 'A', 1.0, 1e-1, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1227, "SEA/USDT", "", "Galleon Quest / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SEA", "USDT", 'A', 1.0, 1e-1, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1228, "RALLY/BTC", "", "Rally / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RALLY", "BTC", 'A', 1.0, 1e-1, 5500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1229, "SWM/BTC", "", "SWARM / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SWM", "BTC", 'A', 1.0, 1e-1, 110, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1230, "RALLY/USDT", "", "Rally / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "RALLY", "USDT", 'A', 1.0, 1e-1, 5500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1231, "IVNT/BTC", "", "IVNT Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IVNT", "BTC", 'A', 1.0, 1.0, 220, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1232, "IVNT/USDT", "", "IVNT Token / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IVNT", "USDT", 'A', 1.0, 1.0, 220, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1233, "ETM/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ETM", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1234, "CDAG/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CDAG", "BTC", 'A', 1.0, 1e-1, 0, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1235, "CDAG/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CDAG", "USDT", 'A', 1.0, 1e-1, 0, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1236, "HETA/ETH", "", "HetaChain / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HETA", "ETH", 'A', 1.0, 1e-1, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1237, "HETA/BTC", "", "HetaChain / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HETA", "BTC", 'A', 1.0, 1e-1, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1238, "HETA/USDT", "", "HetaChain / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "HETA", "USDT", 'A', 1.0, 1e-1, 600, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1239, "CVT/ETH", "", "concertVR / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CVT", "ETH", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1240, "SNTVT/ETH", "", "Sentivate / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNTVT", "ETH", 'A', 1.0, 1e-1, 8330, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1241, "CVT/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CVT", "BTC", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1242, "CVT/USDT", "", "concertVR / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CVT", "USDT", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1243, "CVT/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CVT", "LA", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1244, "XRM/BTC", "", "Aerum / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XRM", "BTC", 'A', 1.0, 1e-1, 830, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1245, "CXG/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CXG", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1246, "CHC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CHC", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1247, "FSHN/ETH", "", "Fashion Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FSHN", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1248, "INF/ETH", "", "Infinitus Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "INF", "ETH", 'A', 1.0, 1e-1, 8, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1249, "TMNK/BTC", "", "Trademonk / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TMNK", "BTC", 'A', 1.0, 1e-1, 25, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1250, "SNTVT/BTC", "", "Sentivate / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNTVT", "BTC", 'A', 1.0, 1e-1, 8330, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1251, "SNTVT/USDT", "", "Sentivate / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SNTVT", "USDT", 'A', 1.0, 1e-1, 8330, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1252, "IPUX/BTC", "", "IPUX / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IPUX", "BTC", 'A', 1.0, 1e-2, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1253, "IPUX/USDT", "", "IPUX / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IPUX", "USDT", 'A', 1.0, 1e-2, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1256, "LOCUS/ETH", "", "LOCUS / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOCUS", "ETH", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1257, "LOCUS/BTC", "", "LOCUS / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOCUS", "BTC", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1258, "LOCUS/USDT", "", "LOCUS / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LOCUS", "USDT", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1259, "RPZX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "RPZX", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1260, "MANA/BTC", "", "Decentraland / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MANA", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1261, "VLM/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "VLM", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1262, "BNTE/BTC", "", "Bountie  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BNTE", "BTC", 'A', 1.0, 1e-1, 500, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1263, "NOIZ/BTC", "", "NOIZchain / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "NOIZ", "BTC", 'A', 1.0, 1e-1, 70, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1264, "IPN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "IPN", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1265, "FR8/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "FR8", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1269, "EIS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "EIS", "ETH", 'A', 1.0, 1e-1, 4000, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1270, "ETHOS/BTC", "", "ETHOS / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ETHOS", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1271, "DRGN/BTC", "", "Dragonchain / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "DRGN", "BTC", 'A', 1.0, 1e-1, 50, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1272, "DROID/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "DROID", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1273, "SST/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "SST", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1274, "TRP/BTC", "", "Tronipay / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRP", "BTC", 'A', 1.0, 1e-1, 20, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1275, "TRP/USDT", "", "Tronipay / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TRP", "USDT", 'A', 1.0, 1e-1, 20, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1276, "LOL/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LOL", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1277, "IDEAL/BTC", "", "IDEAL Coin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IDEAL", "BTC", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1278, "PGF7T/ETH", "", "PGF500 / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PGF7T", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1279, "FR8/BTC", "", "Fr8 Network / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FR8", "BTC", 'A', 1.0, 1e-1, 110, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1280, "IGG/ETH", "", "Intergalactic Gaming  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IGG", "ETH", 'A', 1.0, 1.0, 6342, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1281, "VET/ETH", "", "VeСhain / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VET", "ETH", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1282, "VET/BTC", "", "VeСhain / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VET", "BTC", 'A', 1.0, 1.0, 100, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1283, "VET/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "VET", "LA", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1284, "VET/USDT", "", "VeСhain / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VET", "USDT", 'A', 1.0, 1.0, 100, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1285, "OT/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "OT", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1286, "PGF7T/USDT", "", "PGF500 / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PGF7T", "USDT", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1287, "AGLC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "AGLC", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1288, "VTHO/BTC", "", "VeThor Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VTHO", "BTC", 'A', 1.0, 1e-1, 12000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1289, "VTHO/USDT", "", "VeThor Token / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VTHO", "USDT", 'A', 1.0, 1e-1, 12000, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1290, "SKY/ETH", "", "Skycoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SKY", "ETH", 'A', 1.0, 1e-2, 100, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1291, "SKY/BTC", "", "Skycoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SKY", "BTC", 'A', 1.0, 1e-2, 100, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1292, "SKY/USDT", "", "Skycoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SKY", "USDT", 'A', 1.0, 1.0, 0, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1293, "SKY/LA", "", "Skycoin / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SKY", "LA", 'A', 1.0, 1.0, 0, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1295, "PXB/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PXB", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1296, "IGG/BTC", "", "Intergalactic Gaming  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IGG", "BTC", 'A', 1.0, 1.0, 6342, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1297, "IGG/USDT", "", "Intergalactic Gaming  / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IGG", "USDT", 'A', 1.0, 1e-1, 63420, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1298, "IGG/LA", "", "Intergalactic Gaming  / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "IGG", "LA", 'A', 1.0, 1e-1, 63420, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
   { 1299, "ZUC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ZUC", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1300, "BTR/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BTR", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1301, "Z/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "Z", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1302, "MT/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MT", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1303, "ACU/ETH", "", "Aitheon / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ACU", "ETH", 'A', 1.0, 1.0, 3, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1304, "OT/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "OT", "BTC", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1305, "ZUC/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "ZUC", "BTC", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1306, "XLAB/BTC", "", "XcelToken Plus / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XLAB", "BTC", 'A', 1.0, 1e-1, 200, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1307, "XLAB/USDT", "", "XcelToken Plus / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XLAB", "USDT", 'A', 1.0, 1e-1, 200, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1308, "TOC/ETH", "", "Touch Coin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TOC", "ETH", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1309, "TOC/BTC", "", "Touch Coin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TOC", "BTC", 'A', 1.0, 1e-1, 100, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1310, "ACU/BTC", "", "Aitheon / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ACU", "BTC", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1311, "ACU/USDT", "", "Aitheon / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ACU", "USDT", 'A', 1.0, 1e-1, 30, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1312, "GEC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GEC", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1313, "TSL/ETH", "", "TSLCoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TSL", "ETH", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1314, "TSL/BTC", "", "TSLCoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TSL", "BTC", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1315, "TSL/USDT", "", "TSLCoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "TSL", "USDT", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1316, "BNRT/ETH", "", "BNRT / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "BNRT", "ETH", 'A', 1.0, 1.0, 2, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1317, "MCA/BTC", "", "MECA Coin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCA", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1318, "MCA/USDT", "", "MECA Coin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCA", "USDT", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1319, "MCA/LA", "", "MECA Coin / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "MCA", "LA", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1320, "ABBC/BTC", "", "ABBCCoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "ABBC", "BTC", 'A', 1.0, 1e-8, 100000000, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1321, "GNY/BTC", "", "GNY  / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GNY", "BTC", 'A', 1.0, 1.0, 5, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1322, "PROPEL/ETH", "", "PROPEL / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PROPEL", "ETH", 'A', 1.0, 1.0, 833, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1323, "LIN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LIN", "ETH", 'A', 1.0, 1e-1, 1500, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1324, "TCJ/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "TCJ", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1325, "CPC/ETH", "", "Cyber Physical Chain / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CPC", "ETH", 'A', 1.0, 1e-1, 700, 1e-7, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1326, "CPC/BTC", "", "Cyber Physical Chain / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CPC", "BTC", 'A', 1.0, 1.0, 70, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1327, "CPC/USDT", "", "Cyber Physical Chain / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "CPC", "USDT", 'A', 1.0, 1.0, 70, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1328, "PROPEL/BTC", "", "PROPEL / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PROPEL", "BTC", 'A', 1.0, 1.0, 833, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1329, "PROPEL/USDT", "", "PROPEL / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "PROPEL", "USDT", 'A', 1.0, 1.0, 833, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1330, "CNTX/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CNTX", "ETH", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1331, "CNTX/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "CNTX", "BTC", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1332, "LTC/ETH", "", "LITECOIN / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LTC", "ETH", 'A', 1.0, 1e-3, 10, 1e-5, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1333, "LTC/BTC", "", "LITECOIN / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LTC", "BTC", 'A', 1.0, 1e-2, 1, 1e-6, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1334, "LTC/USDT", "", "LITECOIN / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "LTC", "USDT", 'A', 1.0, 1e-5, 1000, 1e-2, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1335, "VAN/ETH", "", "VOCEAN  / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "VAN", "ETH", 'A', 1.0, 1.0, 16, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1336, "SUN/ETH", "", "SunnyCoin / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SUN", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1337, "SUN/BTC", "", "SunnyCoin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SUN", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1338, "SUN/USDT", "", "SunnyCoin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SUN", "USDT", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1339, "SUN/LA", "", "SunnyCoin / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SUN", "LA", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1340, "FSHN/BTC", "", "Fashion Coin / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FSHN", "BTC", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1341, "IOWN/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "IOWN", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1342, "WAY/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "WAY", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1343, "DAYTA/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "DAYTA", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1344, "SMPT/ETH", "", "Smart Pharma Token / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SMPT", "ETH", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1345, "SMPT/BTC", "", "Smart Pharma Token / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "SMPT", "BTC", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1346, "PXB/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PXB", "USDT", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1347, "FSHN/LA", "", "Fashion Coin / LATOKEN", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FSHN", "LA", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1348, "FSHN/USDT", "", "Fashion Coin / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "FSHN", "USDT", 'A', 1.0, 1.0, 1, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1349, "GFCS/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GFCS", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1350, "GTP/BTC", "", "Tron Game Global Pay / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GTP", "BTC", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1351, "GTP/USDT", "", "Tron Game Global Pay / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "GTP", "USDT", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1352, "GFCS/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GFCS", "USDT", 'A', 1.0, 1e-1, 10, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1353, "XBM/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "XBM", "ETH", 'A', 1.0, 1e-1, 10, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1354, "XCT/ETH", "", "xCrypt / Ethereum", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XCT", "ETH", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1355, "XCT/BTC", "", "xCrypt / Bitcoin", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XCT", "BTC", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1356, "XCT/USDT", "", "xCrypt / Tether USD ", "MRCXXX", "LATOKEN", "",
      "SPOT", "", "XCT", "USDT", 'A', 1.0, 1.0, 10, 1e-8, 'A', 1.0,
      0, 0, 0.0, 0, ""
    },
    { 1357, "LIN/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "LIN", "BTC", 'A', 1.0, 1e-1, 1500, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1360, "HOT/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "HOT", "USDT", 'A', 1.0, 1e-3, 1000000, 1e-7, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1361, "BTNY/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BTNY", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1362, "MZG/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MZG", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1363, "MT/TRX", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MT", "TRX", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1364, "MACH/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "MACH", "ETH", 'A', 1.0, 1e-0, 1, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1365, "PATH/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PATH", "ETH", 'A', 1.0, 1e-0, 2, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1366, "PATH/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PATH", "BTC", 'A', 1.0, 1e-0, 2, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1368, "BNRT/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BNRT", "BTC", 'A', 1.0, 1e-0, 2, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1369, "GENE/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GENE", "ETH", 'A', 1.0, 1e-0, 410, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1370, "GENE/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "GENE", "BTC", 'A', 1.0, 1e-0, 410, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1371, "RPZX/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "RPZX", "BTC", 'A', 1.0, 1e-1, 60, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1372, "RPZX/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "RPZX", "USDT", 'A', 1.0, 1e-1, 60, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1373, "BFC/LA", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BFC", "LA", 'A', 1.0, 1e-0, 150, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1374, "BFC/ETH", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BFC", "ETH", 'A', 1.0, 1e-0, 150, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1375, "BFC/BTC", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BFC", "BTC", 'A', 1.0, 1e-0, 150, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1376, "BFC/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "BFC", "USDT", 'A', 1.0, 1e-0, 150, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    },
    { 1377, "PATH/USDT", "", "", "MRCXXX", "LATOKEN", "", "SPOT", "",
      "PATH", "USDT", 'A', 1.0, 1e-0, 2, 1e-8, 'A', 1.0, 0, 0, 0.0, 0, ""
    }
  };

  //=========================================================================//
  // LATOKEN Assets:                                                         //
  //=========================================================================//
  // NB: This is a list of Quadruples (CcyID, AssetID, AssetName, FeeRate):
  //
  vector<tuple<unsigned, unsigned, char const*, double>> const Assets
  {
  // CcyID,  AssetID,  AssetName, FeeRate:
    {   1,       960, "LA", 0.0005 },
    {   2,       961, "ETH", 0.001 },
    {  19,        91, "Apple", 0.001 },
    {  20,       111, "AMZLA", 0.001 },
    {  21,       115, "Facebook", 0.001 },
    {  22,       116, "NVIDIA", 0.001 },
    {  23,       117, "Tesla", 0.001 },
    {  24,       118, "Alphabet", 0.001 },
    {  25,       119, "Microsoft", 0.001 },
    {  26,       120, "Netflix", 0.001 },
    {  27,       121, "WTI", 0.001 },
    {  28,       122, "Gold", 0.001 },
    {  29,       261, "Real Estate ETF LAT", 0.001 },
    {  31,       283, "Nikkei", 0.001 },
    {  32,       284, "S&P 500", 0.001 },
    {  33,       285, "Apple ETH", 0.001 },
    {  34,       962, "MTRC", 0.001 },
    {  35,       239, "LATNEPRA", 0.001 },
    {  36,       963, "PPT", 0.001 },
    {  38,       549, "FIN", 0.001 },         // Finom (FIN)
    {  39,       964, "BTC", 0.001 },         // Aka XBT!!!
    {  40,       965, "VERI", 0.001 },
    {  41,       966, "SALT", 0.001 },
    {  42,       967, "REP", 0.001 },
    {  43,       968, "MANA", 0.001 },
    {  44,       969, "SAN", 0.001 },
    {  45,       970, "BAT", 0.001 },
    {  46,       971, "PAY", 0.001 },
    {  47,       972, "SNT", 0.001 },
    {  48,       973, "QASH", 0.001 },
    {  49,       974, "POWR", 0.001 },
    {  50,       975, "MCO", 0.001 },
    {  51,       976, "AION", 0.001 },
    {  52,       977, "FUN", 0.001 },
    {  53,       978, "WTC", 0.001 },
    {  54,       979, "GNO", 0.001 },
    {  55,       980, "AE", 0.001 },
    {  56,       981, "RDN", 0.001 },
    {  57,       982, "BNT", 0.001 },
    {  58,       983, "VEN", 0.001 },
    {  89,       637, "DEAL", 0.001 },
    {  90,       640, "JNT", 0.001 },
    {  91,       642, "CTD", 0.001 },         // COINTED (CTD)
    {  92,       643, "CUBE", 0.001 },        // CUBE (CUBE)
    {  93,       644, "FLP", 0.001 },         // Gameflip - FLIP (FLP)
    {  94,       645, "CGCOIN", 0.001 },      // Crowd Genie (CGCOIN)
    {  95,       646, "ASTR", 0.001 },        // ASTRcoin (ASTR)
    {  96,       647, "STQ", 0.001 },         // Storiqa (STQ)
    {  97,       648, "PCT", 0.001 },         // PerksCoin (PCT)
    {  98,       649, "InC", 0.001 },         // InterestCoin (InC)
    {  99,       650, "ING", 0.001 },
    { 100,       651, "CJT", 0.001 },
    { 101,       652, "MOOSE", 0.001 },       // Moosecoin (MOOSE)
    { 102,       653, "MET", 0.001 },         // Memessenger (MET)
    { 103,       654, "CLT", 0.001 },         // CoinLoan Token (CLT)
    { 104,       655, "LCD", 0.001 },
    { 105,       656, "BFY", 0.001 },         // Bank4YOU Group (BFY)
    { 106,       657, "COV", 0.001 },
    { 107,       658, "WBT", 0.001 },         // WorldBit (WBT)
    { 108,       659, "KYC", 0.001 },         // KYC token (KYC)
    { 109,       660, "BTF", 0.001 },         // Blockchain Traded Fund (BTF)
    { 110,       661, "BFY", 0.001 },         // Bank4YOU Group (BFY)
    { 111,       662, "TOK", 0.001 },         // TOKIA (TOK)
    { 112,       663, "COV", 0.001 },
    { 113,       664, "WBT", 0.001 },         // WorldBit (WBT)
    { 114,       665, "KYC", 0.001 },         // KYC token (KYC)
    { 115,       666, "BTF", 0.001 },         // Blockchain Traded Fund (BTF)
    { 116,       667, "HQX", 0.001 },
    { 117,       668, "BNK", 0.001 },         // Bankera (BNK)
    { 118,       669, "NVB", 0.001 },         // NVB Token (NVB)
    { 119,       670, "CPT", 0.001 },         // Cryptaur (CPT)
    { 120,       671, "WAT", 0.001 },         // WAT (WAT)
    { 121,       672, "RNT", 0.001 },
    { 122,       673, "BERRY", 0.001 },       // BERRY (BERRY)
    { 123,       674, "C3", 0.001 },          // CCCR (C3)
    { 124,       675, "LAN", 0.001 },         // FreelancerCoin (LAN)
    { 125,       676, "GNI", 0.001 },         // Miners At Work (GNI)
    { 126,       677, "GEA", 0.001 },         // GOLDEA (GEA)
    { 127,       678, "DIGIT", 0.001 },       // DIGIT Token (DIGIT)
    { 128,       679, "TCT", 0.001 },         // Travel Token (TCT)
    { 129,       680, "Coinhealth", 0.001 },  // Coinhealth (Coinhealth)
    { 130,       681, "LYL", 0.001 },         // LoyalCoin (LYL)
    { 131,       682, "BTR", 0.001 },         // Bitether (BTR)
    { 132,       683, "PCP", 0.001 },         // PopulCoin (PCP)
    { 133,       684, "SCR", 0.001 },
    { 134,       685, "RFR", 0.001 },
    { 135,       686, "VB", 0.001 },
    { 136,       687, "CRC", 0.001 },         // CRYCASH (CRC)
    { 137,       688, "MTN", 0.001 },         // MyTrackNet (MTN)
    { 138,       689, "GMIT", 0.001 },        // GMIT (GMIT)
    { 139,       690, "SPC", 0.001 },         // SPC (SuperPay)
    { 140,       691, "LION", 0.001 },        // CoinLion (LION)
    { 141,       692, "BAR", 0.001 },         // Titanium Blockchain Infrastructure Services (BAR)
    { 142,       693, "LALA", 0.001 },        // LaLa Coin (LALA)
    { 143,       694, "STC", 0.001 },         // StarterCoin (STC)
    { 144,       695, "VIT", 0.001 },         // ViToken (VIT)
    { 145,       696, "THETA", 0.001 },       // Theta Token (THETA)
    { 146,       697, "ATFS", 0.001 },        // ATFS Token (ATFS)
    { 148,       705, "Boston", 0.001 },
    { 149,       717, "VIOLA", 0.001 },
    { 150,       718, "HTKN", 0.001 },
    { 151,       725, "IPS", 0.001 },
    { 152,       984, "USDT", 0.001 },
    { 153,       764, "CLN", 0.001 },
    { 154,       758, "WT", 0.001 },
    { 155,       779, "WELL", 0.001 },
    { 156,       778, "CUBE", 0.001 },
    { 157,       777, "SMT", 0.001 },
    { 158,       776, "THRT", 0.001 },
    { 159,       775, "IQN", 0.001 },
    { 160,       774, "LIF", 0.001 },
    { 161,       773, "SRNT", 0.001 },
    { 162,       771, "SCR", 0.001 },
    { 163,       770, "NCT", 0.001 },
    { 164,       767, "FSN", 0.001 },
    { 165,       766, "HIRE", 0.001 },
    { 166,       765, "REM", 0.001 },
    { 167,       763, "LNC", 0.001 },
    { 168,       761, "WPR", 0.001 },
    { 169,       760, "ROCK", 0.001 },
    { 170,       782, "thd", 0.001 },
    { 171,       781, "Test", 0.001 },
    { 172,       780, "thd", 0.001 },
    { 173,       721, "ferrytocken", 0.001 },
    { 174,       755, "WT", 0.001 },          // WeToken (WT)
    { 175,       745, "AMN", 0.001 },
    { 176,       744, "AMN", 0.001 },
    { 177,       735, "Influencer Coin", 0.001 },
    { 178,       734, "OmiX", 0.001 },
    { 180,       749, "Ashrafee", 0.001 },
    { 181,       759, "Rocket ICO", 0.001 },
    { 182,       746, "ahahahahah", 0.001 },
    { 183,       769, "Test", 0.001 },
    { 187,       985, "EOS", 0.001 },
    { 188,       986, "TRX", 0.001 },
    { 189,       987, "ICX", 0.001 },
    { 190,       840, "RQX", 0.001 },
    { 191,       841, "VST", 0.001 },
    { 192,       842, "STM", 0.001 },
    { 193,       846, "PON", 0.001 },
    { 194,       850, "SKYFT", 0.001 },
    { 195,       855, "TNS", 0.001 },
    { 196,       856, "STF", 0.001 },
    { 197,       857, "VIN", 0.001 },
    { 198,       862, "GO",  0.0 },
    { 199,       864, "ADM", 0.0 },
    { 201,       872, "SGN", 0.0 },
    { 202,       873, "AMN", 0.0 },
    { 203,       874, "GOL", 0.0 },
    { 204,       875, "UKT", 0.0 },
    { 205,       877, "LPK", 0.0 },
    { 206,       878, "LTS", 0.0 },
    { 207,       879, "IAHC", 0.0 },
    { 208,       906, "MOAT", 0.0 },
    { 209,       921, "OLXA", 0.0 },
    { 210,       959, "BERLI", 0.0 },
    { 211,       988, "CPT", 0.001 },
    { 212,       883, "ELY", 0.001 },
    { 213,      1022, "LBA", 0.001 },
    { 214,      1024, "ZCO", 0.001 },
    { 215,      1025, "NPXS", 0.001 },
    { 216,      1026, "HOT", 0.001 },
    { 217,      1033, "PCO", 0.001 },
    { 218,      1034, "TEN", 0.001 },
    { 219,      1039, "TESTF", 0.001 },
    { 220,      1041, "TESTG", 0.001 },
    { 221,      1047, "MORPH", 0.001 },
    { 222,      1055, "SPS", 0.001 },
    { 223,      1070, "LOOM", 0.001 },
    { 224,      1071, "MYB", 0.001 },
    { 225,      1060, "GOT", 0.001 },
    { 226,      1080, "QKC", 0.001 },
    { 227,      1074, "BEZ", 0.001 },
    { 228,      1076, "EDU", 0.001 },
    { 229,      1089, "EMOT", 0.001 },
    { 230,      1087, "JOINT", 0.001 },
    { 231,      1040, "UMT", 0.001 },
    { 232,      1095, "TESTZ", 0.001 },
    { 233,      1090, "MVU", 0.001 },
    { 234,      1096, "TESTZ", 0.001 },
    { 235,      1094, "TEST", 0.001 },
    { 236,      1097, "TEST", 0.001 },
    { 237,      1098, "UUU", 0.001 },
    { 238,      1104, "SAMST", 0.001 },
    { 239,      1105, "GOLD", 0.001 },
    { 240,      1106, "HSIT", 0.001 },
    { 241,      1102, "RCD", 0.001 },
    { 242,      1083, "UUNIO", 0.001 },
    { 243,      1120, "AIC", 0.001 },
    { 244,      1122, "OLT", 0.001 },
    { 245,      1123, "DRGN", 0.001 },
    { 246,      1126, "ATMI", 0.001 },
    { 247,      1132, "TOKEN", 0.001 },
    { 248,      1134, "SCT", 0.001 },
    { 249,      1145, "NIM", 0.001 },
    { 250,      1146, "SPN", 0.001 },
    { 251,      1147, "XPST", 0.001 },
    { 252,      1148, "ATS", 0.001 },
    { 253,      1149, "DIW", 0.001 },
    { 254,      1082, "MCC", 0.001 },
    { 255,      1139, "SPX", 0.001 },
    { 256,      1184, "HUR", 0.001 },
    { 257,      1186, "ESTS", 0.001 },
    { 258,      1183, "XPAT", 0.001 },
    { 259,      1180, "URUN", 0.001 },
    { 260,      1192, "UTK", 0.001 },
    { 261,      1197, "SCRL", 0.001 },
    { 262,      1202, "STQ", 0.001 },
    { 263,      1185, "VIKKY", 0.001 },
    { 264,      1224, "NEO", 0.001 },
    { 265,      1225, "GAS", 0.001 },
    { 266,      1226, "EFX", 0.001 },
    { 267,      1227, "NRVE", 0.001 },
    { 268,      1229, "MVP", 0.001 },
    { 269,      1213, "COSM", 0.001 },
    { 270,      1232, "QURO", 0.001 },
    { 271,      1191, "KON", 0.001 },
    { 290,      1238, "MYST", 0.001 },
    { 291,      1239, "ABL", 0.001 },
    { 303,      1240, "ESS", 0.001 },
    { 307,      1241, "GOT", 0.001 },
    { 308,      1242, "MFT", 0.001 },
    { 309,      1243, "EGT", 0.001 },
    { 310,      1244, "PAI", 0.001 },
    { 311,      1245, "SEELE", 0.001 },
    { 312,      1246, "OPEN", 0.001 },
    { 313,      1261, "SPRK", 0.001 },
    { 314,      1262, "DAV", 0.001 },
    { 315,      1263, "FTM", 0.001 },
    { 316,      1264, "OAK", 0.001 },
    { 317,      1255, "AEN", 0.001 },
    { 318,      1267, "ETHOS", 0.001 },
    { 319,      1291, "PHM", 0.001 },
    { 320,      1292, "PHM", 0.001 },
    { 321,      1293, "MTC", 0.001 },
    { 322,      1136, "SGP", 0.001 },
    { 323,      1297, "TKY", 0.001 },
    { 324,      1270, "PDATA", 0.001 },
    { 325,      1298, "TMC", 0.001 },
    { 326,      1141, "POLY", 0.001 },
    { 327,      1305, "DYC", 0.001 },
    { 328,      1307, "TOYOT", 0.001 },
    { 329,      1308, "ICBCT", 0.001 },
    { 330,      1306, "ABYSS", 0.001 },
    { 331,      1299, "GFDGF", 0.001 },
    { 332,      1174, "INCX", 0.001 },
    { 333,      1310, "SETH", 0.001 },
    { 334,      1312, "AVINOC", 0.001 },
    { 335,      1311, "TTC", 0.001 },
    { 336,      1313, "GTA", 0.001 },
    { 337,      1321, "SRN", 0.001 },
    { 338,      1328, "DBET", 0.001 },
    { 339,      1330, "PTOY", 0.001 },
    { 340,      1331, "NKN", 0.001 },
    { 341,      1332, "C8", 0.001 },
    { 342,      1337, "ENJ", 0.001 },
    { 343,      1333, "RLX", 0.001 },
    { 344,      1338, "AMO", 0.001 },
    { 345,      1319, "DENT", 0.001 },
    { 346,      1340, "INCO", 0.001 },
    { 347,      1350, "FUEL", 0.001 },
    { 348,      1354, "INXT", 0.001 },
    { 349,      1358, "CEEK", 0.001 },
    { 350,      1360, "KWATT", 0.001 },
    { 351,      1368, "AGLT", 0.001 },
    { 352,      1359, "OIO", 0.001 },
    { 354,      1378, "KNW", 0.001 },
    { 355,      1381, "MRPH", 0.001 },
    { 356,      1387, "UBC", 0.001 },
    { 357,      1391, "IMT", 0.001 },
    { 358,      1388, "MFTU", 0.001 },
    { 359,      1393, "LOCI", 0.001 },
    { 360,      1382, "JHGAH", 0.001 },
    { 361,      1396, "CHT", 0.001 },
    { 362,      1389, "OPET", 0.001 },
    { 363,      1399, "BGL", 0.001 },
    { 364,      1394, "METM", 0.001 },
    { 365,      1404, "OAX", 0.001 },
    { 366,      1400, "WHEN", 0.001 },
    { 368,      1410, "BTT", 0.001 },
    { 369,      1417, "PAT", 0.001 },
    { 370,      1418, "DOV", 0.001 },
    { 371,      1420, "PARETO", 0.001 },
    { 372,      1421, "MOD", 0.001 },
    { 373,      1422, "COUP", 0.001 },
    { 374,      1423, "CYFM", 0.001 },
    { 375,      1419, "MIC", 0.001 },
    { 376,      1409, "DCN", 0.001 },
    { 377,      1427, "PXT", 0.001 },
    { 378,      1428, "ASA", 0.001 },
    { 379,      1395, "LOC", 0.001 },
    { 380,      1431, "KRI", 0.001 },
    { 381,      1435, "TOYOT", 0.001 },
    { 382,      1430, "JSE", 0.001 },
    { 383,      1436, "FAN", 0.001 },
    { 384,      1205, "VBEO", 0.001 },
    { 385,      1446, "MGX", 0.001 },
    { 386,      1442, "RUSH", 0.001 },
    { 387,      1447, "AMLT", 0.001 },
    { 388,      1398, "ROBET", 0.001 },
    { 389,      1450, "BDT", 0.001 },
    { 390,      1448, "HIVE", 0.001 },
    { 391,      1355, "SPND", 0.001 },
    { 392,      1451, "MFG", 0.001 },
    { 393,      1453, "SCOI", 0.001 },
    { 394,      1454, "IFT", 0.001 },
    { 395,      1437, "TIM", 0.001 },
    { 398,      1452, "X8X", 0.001 },
    { 399,      1443, "DEB", 0.001 },
    { 400,      1459, "MITH", 0.001 },
    { 401,      1460, "SYC", 0.001 },
    { 402,      1177, "MIR", 1.0,  },
    { 407,      1468, "QNT", 0.001 },
    { 408,      1429, "NWS", 0.001 },
    { 409,      1477, "DPP", 0.001 },
    { 410,      1464, "LMA", 0.001 },
    { 411,      1480, "TASTA", 0.001 },
    { 412,      1469, "BBT", 0.001 },
    { 413,      1481, "NOIZ", 0.001 },
    { 414,      1476, "ABX", 0.001 },
    { 415,      1479, "KRC7", 0.001 },
    { 416,      1474, "KNT", 0.001 },
    { 417,      1142, "CCOS", 0.001 },
    { 418,      1486, "CCL", 0.001 },
    { 419,      1466, "COU", 0.001 },
    { 420,      1489, "XDCE", 0.001 },
    { 421,      1498, "TGOLD", 0.001 },
    { 422,      1497, "CPX", 0.001 },
    { 423,      1500, "ECOM", 0.001 },
    { 424,      1493, "BNTE", 0.001 },
    { 425,      1494, "QXE", 0.001 },
    { 426,      1506, "BITTO", 0.001 },
    { 427,      1502, "CLIN", 0.001 },
    { 428,      1490, "PAVO", 0.001 },
    { 430,      1492, "MCB", 0.001 },
    { 431,      1487, "CVN", 0.001 },
    { 432,      1511, "ZLA", 0.001 },
    { 433,      1512, "BXM", 0.001 },
    { 434,      1514, "PDI", 0.001 },
    { 435,      1515, "CSM", 0.001 },
    { 436,      1516, "AL", 0.001 },
    { 437,      1517, "GMB", 0.001 },
    { 438,      1504, "GEMA", 0.001 },
    { 439,      1520, "VIBE", 0.001 },
    { 440,      1519, "SCX", 0.001 },
    { 441,      1518, "UTS", 0.001 },
    { 442,      1522, "SWP", 0.001 },
    { 443,      1269, "BOS", 1.0,  },
    { 444,      1510, "GSL", 0.001 },
    { 445,      1526, "CMCT", 0.001 },
    { 446,      1189, "PYLNT", 0.001 },
    { 447,      1528, "NDX", 0.001 },
    { 448,      1534, "UTD", 0.001 },
    { 449,      1534, "UTD", 0.001 },
    { 450,      1534, "UTD", 0.001 },
    { 451,      1539, "GUSD", 0.001 },
    { 452,      1543, "USDC", 0.001 },
    { 453,      1544, "FOUR", 0.001 },
    { 454,      1546, "FOOD", 0.001 },
    { 455,      1557, "TMT", 0.001 },
    { 456,      1559, "VET", 1.0,  },
    { 457,      1562, "VTHO", 0.001 },
    { 458,      1564, "PLAT", 0.001 },
    { 459,      1572, "IVNT", 0.001 },        // Aka IVN
    { 460,      1503, "TAB", 0.001 },
    { 461,      1573, "LQD", 0.001 },
    { 462,      1574, "AMB", 0.001 },
    { 463,      1575, "ATCC", 0.001 },
    { 464,      1580, "CRYP", 0.001 },
    { 465,      1581, "KST", 0.001 },
    { 466,      1582, "TYPE", 0.001 },
    { 467,      1584, "MIO", 0.001 },
    { 468,      1583, "BETR", 0.001 },
    { 469,      1561, "GOX", 0.001 },
    { 470,      1586, "IXE", 0.001 },
    { 471,      1588, "RAI", 0.001 },
    { 472,      1590, "HBX", 0.001 },
    { 473,      1589, "VGW", 0.001 },
    { 474,      1595, "MEDIBIT", 0.001 },
    { 475,      1596, "SRCOIN", 0.001 },
    { 476,      1495, "WES", 0.001 },
    { 477,      1591, "NHCT", 0.001 },
    { 478,      1598, "ROTO", 0.001 },
    { 479,      1547, "XNY", 0.001 },
    { 480,      1594, "KSC", 0.001 },
    { 481,      1605, "SHA", 0.001 },
    { 482,      1606, "SCC", 0.001 },
    { 483,      1609, "EUR", 0.0 },
    { 486,      1603, "FCT", 0.001 },
    { 487,      1614, "GBT", 0.001 },
    { 488,      1604, "GXC", 0.001 },
    { 489,      1608, "TOS", 0.001 },
    { 490,      1617, "GBA", 0.001 },
    { 491,      1621, "HDR", 0.001 },
    { 492,      1625, "HERC", 0.001 },
    { 493,      1632, "GBPP", 0.001 },
    { 494,      1634, "TRAK", 0.001 },
    { 495,      1636, "GRET", 0.001 },
    { 496,      1637, "YPNG", 0.001 },
    { 497,      1638, "XRTC", 0.001 },
    { 498,      1645, "PSK", 0.001 },
    { 499,      1646, "BDC", 0.001 },
    { 500,      1649, "XLM", 0.001 },
    { 501,      1650, "LTR", 0.001 },
    { 502,      1653, "RMT", 0.001 },
    { 503,      1642, "NZO", 0.001 },
    { 504,      1635, "EYC", 0.001 },
    { 505,      1651, "WNC", 0.001 },
    { 506,      1660, "HLL", 0.001 },
    { 507,      1658, "OVC", 0.001 },
    { 508,      1661, "BCQR", 0.001 },
    { 509,      1663, "SREUR", 0.001 },
    { 510,      1640, "IDEAL", 0.001 },       // Aka IDC
    { 511,      1665, "55555", 0.001 },
    { 512,      1666, "CRF", 0.001 },
    { 513,      1671, "REL", 0.001 },
    { 514,      1672, "YPTKN", 0.001 },
    { 515,      1673, "GIG", 0.001 },
    { 516,      1674, "CLT", 0.001 },
    { 517,      1675, "NKD", 0.001 },
    { 518,      1676, "BBRT", 0.001 },
    { 519,      1662, "HYT", 0.001 },
    { 520,      1678, "GZB", 0.001 },
    { 521,      1670, "JVY", 0.001 },
    { 522,      1681, "HFDG", 0.001 },
    { 523,      1685, "BXT", 0.001 },
    { 524,      1687, "VC", 0.001 },
    { 525,      1692, "PLTC", 0.001 },
    { 526,      1694, "GG5GG", 0.001 },
    { 527,      1697, "GHFGH", 0.001 },
    { 528,      1698, "BOB", 0.001 },
    { 529,      1690, "KZE", 0.001 },
    { 530,      1688, "NAM", 0.001 },
    { 531,      1683, "ICT", 0.001 },
    { 532,      1705, "JOYS", 0.001 },
    { 533,      1693, "CYS", 0.001 },
    { 534,      1701, "VES", 0.001 },
    { 535,      1700, "SBTC", 0.001 },
    { 536,      1708, "XYO", 0.001 },
    { 537,      1709, "TXC", 0.001 },
    { 538,      1710, "ATR", 0.001 },
    { 539,      1711, "AMC", 0.001 },
    { 540,      1712, "SPRTZ", 0.001 },
    { 541,      1713, "SNPC", 0.001 },
    { 542,      1714, "BIO", 0.001 },
    { 543,      1718, "ARTID", 0.001 },
    { 544,      1719, "KKHKH", 0.001 },
    { 545,      1722, "ENTR", 0.001 },
    { 546,      1723, "GGFGF", 0.001 },
    { 547,      1702, "NAT", 0.001 },
    { 548,      1667, "TQBCD", 0.001 },
    { 549,      1726, "PDRY", 0.001 },
    { 551,      1726, "PDRY", 0.001 },
    { 552,      1729, "TEL", 0.001 },
    { 553,      1730, "XBASE", 0.001 },
    { 554,      1731, "HTL", 0.001 },
    { 555,      1725, "CWEX", 0.001 },
    { 556,      1737, "EEE", 0.001 },
    { 557,      1735, "GPYX", 0.001 },
    { 558,      1754, "XGT", 0.001 },
    { 561,      1755, "S4F", 0.001 },         // Aka SFE
    { 562,      1758, "MBIT", 0.001 },        // Aka MBI
    { 563,      1757, "FORCE", 0.001 },
    { 564,      1759, "BAX", 0.001 },
    { 565,      1761, "PCPIE", 0.001 },
    { 566,      1760, "QNTU", 0.001 },
    { 567,      1767, "LIBER", 0.001 },
    { 568,      1743, "CPMS", 0.001 },
    { 569,      1776, "EMPC", 0.001 },
    { 570,      1620, "DLSD", 0.001 },
    { 571,      1777, "TCAT", 0.001 },
    { 572,      1778, "SET", 0.001 },
    { 573,      1779, "FREE", 0.001 },
    { 574,      1780, "SHEEP", 0.001 },
    { 575,      1782, "BRC", 0.001 },
    { 576,      1783, "UAP", 0.001 },
    { 577,      1784, "RYO", 0.001 },
    { 578,      1785, "HALO", 0.001 },
    { 579,      1786, "AFIN", 0.001 },
    { 580,      1787, "EDR", 0.001 },
    { 581,      1791, "AMP", 0.001 },
    { 582,      1775, "TRADE", 0.001 },
    { 583,      1792, "ECTE", 0.001 },
    { 584,      1794, "AGO", 0.001 },
    { 585,      1796, "L", 0.001 },
    { 586,      1797, "ZLC", 0.001 },
    { 587,      1802, "XID", 0.001 },
    { 588,      1803, "VTM", 0.001 },
    { 589,      1756, "WRL", 0.001 },
    { 590,      1798, "CRWD", 0.001 },
    { 591,      1808, "TRYL", 0.001 },
    { 592,      1809, "USDL", 0.001 },
    { 593,      1810, "RUPL", 0.001 },
    { 594,      1811, "ARSL", 0.001 },
    { 595,      1812, "GBPL", 0.001 },
    { 596,      1813, "BRLL", 0.001 },
    { 597,      1814, "EURL", 0.001 },
    { 598,      1815, "VNDL", 0.001 },
    { 599,      1818, "MYN", 0.001 },
    { 600,      1821, "GAGA", 0.001 },
    { 601,      1824, "FFDDF", 0.001 },
    { 602,      1827, "ZZJK", 0.001 },
    { 603,      1828, "PLA", 0.001 },
    { 604,      1738, "PTFYW", 0.001 },
    { 605,      1836, "ZZZZ", 0.001 },
    { 606,      1837, "KEY", 0.001 },
    { 607,      1840, "ABCDS", 0.001 },
    { 608,      1842, "B2G", 0.001 },
    { 609,      1843, "KSKSK", 0.001 },
    { 610,      1849, "CHFL", 0.001 },
    { 611,      1850, "GOLDL", 0.001 },
    { 612,      1851, "WTIL", 0.001 },
    { 613,      1852, "AAPLL", 0.001 },
    { 614,      1853, "BABAL", 0.001 },
    { 615,      1854, "WWW", 0.001 },
    { 616,      1855, "KTO", 0.001 },
    { 617,      1856, "GGHHH", 0.001 },
    { 618,      1857, "ARB", 0.001 },
    { 619,      1833, "ALDX", 0.001 },
    { 620,      1788, "CCOIN", 0.001 },
    { 621,      1831, "TIIM", 0.001 },
    { 622,      1839, "XLAB", 0.001 },        // Aka XCEL
    { 623,      1834, "TM", 0.001 },
    { 624,      1847, "DXG", 0.001 },
    { 625,      1859, "ZZZ", 0.001 },
    { 626,      1844, "JOY", 0.001 },
    { 627,      1860, "PRSN", 0.001 },
    { 628,      1861, "SSS", 0.001 },
    { 629,      1864, "JJJJ", 0.001 },
    { 630,      1873, "2222", 0.001 },
    { 631,      1874, "LATX", 0.001 },
    { 632,      1876, "333", 0.001 },
    { 633,      1877, "111", 0.001 },
    { 634,      1832, "SPL", 0.001 },
    { 635,      1878, "USAT", 0.001 },
    { 636,      1880, "DDDD", 0.001 },
    { 637,      1881, "GFFGF", 0.001 },
    { 638,      1884, "EVY", 0.001 },
    { 639,      1885, "ARAW", 0.001 },
    { 640,      1886, "ACAD", 0.001 },
    { 641,      1883, "ARA", 0.001 },
    { 642,      1892, "MODEX", 0.001 },
    { 643,      1893, "XKN", 0.001 },
    { 644,      1894, "KGSL", 0.001 },
    { 645,      1896, "WWZ", 0.001 },
    { 646,      1887, "CAL", 0.001 },
    { 647,      1897, "RB", 0.001 },
    { 662,      1905, "QUROZ", 0.001 },
    { 671,      1906, "AID", 0.001 },
    { 673,      1904, "QCX", 0.001 },
    { 676,      1902, "MPAY", 0.001 },
    { 677,      1901, "TRP", 0.001 },
    { 678,      1908, "XXXX", 0.001 },
    { 679,      1895, "AZS", 0.001 },
    { 680,      1913, "BUD", 0.001 },
    { 681,      1907, "XRM", 0.001 },
    { 682,      1915, "KIN", 0.001 },
    { 683,      1914, "PLAN", 0.001 },
    { 684,      1918, "MBMT", 0.001 },
    { 685,      1920, "JCT", 0.001 },
    { 686,      1916, "OREO", 0.001 },
    { 687,      1919, "WHN", 0.001 },
    { 688,      1800, "TGN", 0.001 },
    { 689,      1926, "QWZ", 0.001 },
    { 690,      1928, "BCEO", 0.001 },
    { 691,      1927, "TMNK", 0.001 },
    { 692,      1929, "TMNK", 0.001 },
    { 693,      1932, "VTEX", 0.001 },
    { 694,      1933, "IPUX", 0.001 },
    { 695,      1931, "TC", 0.001 },
    { 696,      1917, "ROC2", 0.001 },
    { 697,      1930, "CIA", 0.001 },
    { 698,      1937, "MAC", 0.001 },
    { 699,      1938, "MNC", 0.001 },
    { 700,      1939, "DIO", 0.001 },
    { 701,      1922, "REB", 0.001 },
    { 702,      1925, "GFC", 0.001 },
    { 703,      1699, "REMCO", 0.001 },
    { 704,      1941, "LEDU", 0.001 },
    { 705,      1942, "VIDT", 0.001 },
    { 706,      1940, "7E", 0.001 },
    { 707,      1944, "MCAN", 0.001 },
    { 708,      1945, "XSAT", 0.001 },
    { 709,      1947, "SG", 0.001 },
    { 710,      1949, "AZP", 0.001 },
    { 711,      1946, "ZAW", 0.001 },
    { 712,      1950, "QQQ", 0.001 },
    { 713,      1952, "WBY", 0.001 },
    { 714,      1953, "AMS", 0.001 },
    { 715,      1951, "AGRO", 0.001 },
    { 716,      1955, "BOTX", 0.001 },
    { 717,      1956, "ERA", 0.001 },
    { 718,      1960, "DPT", 0.001 },
    { 719,      1961, "QZ", 0.001 },
    { 720,      1959, "UBE", 0.001 },
    { 721,      1964, "MCA", 0.001 },
    { 722,      1965, "RALLY", 0.001 },
    { 723,      1890, "GTP", 0.001 },
    { 724,      1966, "SWM", 0.001 },
    { 725,      1968, "CDAG", 0.001 },
    { 726,      1969, "SEA", 0.001 },
    { 727,      1972, "ETM", 0.001 },
    { 728,      1973, "TOC", 0.001 },
    { 729,      1974, "HETA", 0.001 },
    { 730,      1975, "CVT", 0.001 },
    { 731,      1976, "SNTVT", 0.001 },
    { 732,      1978, "CXG", 0.001 },
    { 733,      1980, "CHC", 0.001 },
    { 734,      1981, "FSHN", 0.001 },
    { 735,      1982, "INF", 0.001 },
    { 736,      1985, "LOL", 0.001 },
    { 737,      1986, "LOCUS", 0.001 },
    { 738,      1979, "RPZX", 0.001 },
    { 739,      1977, "VLM", 0.001 },
    { 740,      1988, "IPN", 0.001 },
    { 741,      1957, "FR8", 0.001 },
    { 742,      1235, "UUUHH", 0.001 },
    { 743,      1346, "DGFDS", 0.001 },
    { 744,      1523, "GD", 0.001 },          // Aka GGFFG
    { 745,      1990, "EIS", 0.001 },
    { 746,      1995, "DROID", 0.001 },
    { 747,      1984, "SST", 0.001 },
    { 748,      1996, "ABBC", 0.001 },
    { 749,      1997, "SKY", 0.001 },
    { 750,      1999, "PGF7T", 0.001 },      // Aka PGF
    { 751,      2000, "IGG", 0.001 },
    { 758,      2003, "OT", 0.001 },
    { 759,      1998, "AGLC", 0.001 },
    { 760,      2010, "GNY", 0.001 },
    { 761,      2012, "TSL", 0.001 },
    { 762,      2004, "PXB", 0.001 },
    { 763,      2014, "ZUC", 0.001 },
    { 764,      1992, "Z", 0.001 },
    { 771,      2013, "BTR", 0.001 },
    { 772,      2015, "MT", 0.001 },
    { 773,      2017, "ACU", 0.001 },
    { 774,      1983, "GEC", 0.001 },
    { 775,      2023, "BNRT", 0.001 },
    { 776,      2026, "PROPEL", 0.001 },
    { 777,      2019, "LIN", 0.001 },
    { 778,      2022, "TCJ", 0.001 },
    { 779,      2028, "CPC", 0.001 },
    { 780,      2029, "LTC", 0.001 },
    { 781,      2008, "CNTX", 0.001 },
    { 783,      2031, "SUN", 0.001 },
    { 786,      2034, "VAN", 0.001 },
    { 787,      2035, "IOWN", 0.001 },
    { 788,      2020, "WAY", 0.001 },
    { 789,      2009, "DAYTA", 0.001 },
    { 790,      2040, "SMPT", 0.001 },
    { 791,      2036, "GFCS", 0.001 },
    { 792,      2027, "XBM", 0.001 },
    { 793,      2042, "XCT", 0.001 },
    { 794,      2043, "BTT", 0.001 },
    { 795,      2041, "MAS", 0.001 },
    { 796,      2011, "BTNY", 0.001 },
    { 797,      2045, "MZG",  0.001 },
    { 798,      2044, "MACH", 0.001 },
    { 799,      2046, "PATH", 0.001 },
    { 800,      2047, "GENE", 0.001 },
    { 801,      2048, "BFC",  0.001 }
  };
} // End namespace LATOKEN
} // End namespace MAQUETTE
