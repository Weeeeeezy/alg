// vim:ts=2:et
//===========================================================================//
//                      "Venues/MICEX/Configs_FAST.cpp":                     //
//                        Configs for MICEX FAST Feeds                       //
//                             As of: 26-Mar-2017                            //
//===========================================================================//
// XXX: This should eventually be moved into an external DB, eg Erlang Mnesia!
//
#include "Venues/MICEX/Configs_FAST.h"

using namespace std;

namespace MAQUETTE
{
namespace FAST
{
namespace MICEX
{
  using AssetClassT = ::MAQUETTE::MICEX::AssetClassT;

  //=========================================================================//
  // All Configs:                                                            //
  //=========================================================================//
  map
  < tuple<ProtoVerT::type, EnvT::type, AssetClassT::type>,
    map <DataFeedT::type,
         pair<SSM_Config, SSM_Config> >
  >
  const Configs
  {
    //=======================================================================//
    // Prod1 -- CURR only (FX):
    { make_tuple(ProtoVerT::Curr, EnvT::Prod1, AssetClassT::FX),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,        // MSR
          {
            { "239.195.1.7",      "91.203.253.225",   16007 },
            { "239.195.1.135",    "91.203.255.225",   17007 }
          }
        },
        { DataFeedT::Orders_Incr,       // OLR
          {
            { "239.195.1.1",      "91.203.253.225",   16001 },
            { "239.195.1.129",    "91.203.255.225",   17001 }
          }
        },
        { DataFeedT::Orders_Snap,       // OLS
          {
            { "239.195.1.2",      "91.203.253.225",   16002 },
            { "239.195.1.130",    "91.203.255.225",   17002 }
          }
        },
        { DataFeedT::Trades_Incr,       // TLR
          {
            { "239.195.1.5",      "91.203.253.225",   16005 },
            { "239.195.1.133",    "91.203.255.225",   17005 }
          }
        },
        { DataFeedT::Instr_Defs,        // IDF
          {
            { "239.195.1.9",      "91.203.253.225",   16009 },
            { "239.195.1.137",    "91.203.255.225",   17009 }
          }
        },
        { DataFeedT::Instr_Status,      // ISF
          {
            { "239.195.1.10",     "91.203.253.225",   16010 },
            { "239.195.1.138",    "91.203.255.225",   17010 }
          }
        }
      }
    },
    //=======================================================================//
    // Prod1 -- CURR only (EQ):
    { make_tuple(ProtoVerT::Curr, EnvT::Prod1, AssetClassT::EQ),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,        // MSR
          {
            { "239.195.1.47",     "91.203.253.227",   16047 },
            { "239.195.1.175",    "91.203.255.227",   17047 }
          }
        },
        { DataFeedT::Orders_Incr,       // OLR
          {
            { "239.195.1.41",     "91.203.253.227",   16041 },
            { "239.195.1.169",    "91.203.255.227",   17041 }
          }
        },
        { DataFeedT::Orders_Snap,       // OLS
          {
            { "239.195.1.42",     "91.203.253.227",   16042 },
            { "239.195.1.170",    "91.203.255.227",   17042 }
          }
        },
        { DataFeedT::Trades_Incr,       // TLR
          {
            { "239.195.1.45",     "91.203.253.227",   16045 },
            { "239.195.1.173",    "91.203.255.227",   17045 }
          }
        },
        { DataFeedT::Instr_Defs,        // IDF
          {
            { "239.195.1.49",     "91.203.253.227",   16049 },
            { "239.195.1.177",    "91.203.255.227",   17049 }
          }
        },
        { DataFeedT::Instr_Status,      // ISF
          {
            { "239.195.1.50",     "91.203.253.227",   16050 },
            { "239.195.1.178",    "91.203.255.227",   17050 }
          }
        }
      }
    },
    //=======================================================================//
    // Prod2 -- CURR only (FX):
    { make_tuple(ProtoVerT::Curr, EnvT::Prod2, AssetClassT::FX),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,        // MSR
          {
            { "239.195.1.7",      "91.203.253.228",   18007 },
            { "239.195.1.135",    "91.203.255.228",   19007 }
          }
        },
        { DataFeedT::Orders_Incr,       // OLR
          {
            { "239.195.1.1",      "91.203.253.228",   18001 },
            { "239.195.1.129",    "91.203.255.228",   19001 }
          }
        },
        { DataFeedT::Orders_Snap,       // OLS
          {
            { "239.195.1.2",      "91.203.253.228",   18002 },
            { "239.195.1.130",    "91.203.255.228",   19002 }
          }
        },
        { DataFeedT::Trades_Incr,       // TLR
          {
            { "239.195.1.5",      "91.203.253.228",   18005 },
            { "239.195.1.133",    "91.203.255.228",   19005 }
          }
        },
        { DataFeedT::Instr_Defs,        // IDF
          {
            { "239.195.1.9",      "91.203.253.228",   18009 },
            { "239.195.1.137",    "91.203.255.228",   19009 }
          }
        },
        { DataFeedT::Instr_Status,      // ISF
          {
            { "239.195.1.10",     "91.203.253.228",   18010 },
            { "239.195.1.138",    "91.203.255.228",   19010 }
          }
        }
      }
    },
    //=======================================================================//
    // Prod2 -- CURR only (EQ):
    { make_tuple(ProtoVerT::Curr, EnvT::Prod2, AssetClassT::EQ),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,        // MSR
          {
            { "239.195.1.47",     "91.203.253.229",   18047 },
            { "239.195.1.175",    "91.203.255.229",   19047 }
          }
        },
        { DataFeedT::Orders_Incr,       // OLR
          {
            { "239.195.1.41",     "91.203.253.229",   18041 },
            { "239.195.1.169",    "91.203.255.229",   19041 }
          }
        },
        { DataFeedT::Orders_Snap,       // OLS
          {
            { "239.195.1.42",     "91.203.253.229",   18042 },
            { "239.195.1.170",    "91.203.255.229",   19042 }
          }
        },
        { DataFeedT::Trades_Incr,       // TLR
          {
            { "239.195.1.45",     "91.203.253.229",   18045 },
            { "239.195.1.173",    "91.203.255.229",   19045 }
          }
        },
        { DataFeedT::Instr_Defs,        // IDF
          {
            { "239.195.1.49",     "91.203.253.229",   18049 },
            { "239.195.1.177",    "91.203.255.229",   19049 }
          }
        },
        { DataFeedT::Instr_Status,      // ISF
          {
            { "239.195.1.50",     "91.203.253.229",   18050 },
            { "239.195.1.178",    "91.203.255.229",   19050 }
          }
        }
      }
    },
    //=======================================================================//
    // UAT (Test for CURR) over the Internet (FX):
    { make_tuple(ProtoVerT::Curr, EnvT::TestI, AssetClassT::FX),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,        // MSR
          {
            { "239.192.114.7",    "10.50.129.200",    9127 },
            { "239.192.114.135",  "10.50.129.200",    9327 }
          }
        },
        { DataFeedT::Orders_Incr,       // OLR
          {
            { "239.192.114.1",    "10.50.129.200",    9121 },
            { "239.192.114.129",  "10.50.129.200",    9321 }
          }
        },
        { DataFeedT::Orders_Snap,       // OLS
          {
            { "239.192.114.2",    "10.50.129.200",    9122 },
            { "239.192.114.130",  "10.50.129.200",    9322 }
          }
        },
        { DataFeedT::Trades_Incr,          // TLR
          {
            { "239.192.114.5",    "10.50.129.200",    9125 },
            { "239.192.114.133",  "10.50.129.200",    9325 }
          }
        },
        { DataFeedT::Instr_Defs,           // IDF
          {
            { "239.192.114.9",    "10.50.129.200",    9129 },
            { "239.192.114.137",  "10.50.129.200",    9329 }
          }
        },
        { DataFeedT::Instr_Status,         // ISF
          {
            { "239.192.114.10",   "10.50.129.200",    9130 },
            { "239.192.114.138",  "10.50.129.200",    9330 }
          }
        }
      }
    },
    //=======================================================================//
    // UAT (Test for CURR) over the Internet (EQ):
    { make_tuple(ProtoVerT::Curr,    EnvT::TestI, AssetClassT::EQ),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,           // MSR
          {
            { "239.192.111.7",    "10.50.129.200",    9017 },
            { "239.192.111.135",  "10.50.129.200",    9217 }
          }
        },
        { DataFeedT::Orders_Incr,          // OLR
          {
            { "239.192.111.1",    "10.50.129.200",    9011 },
            { "239.192.111.129",  "10.50.129.200",    9211 }
          }
        },
        { DataFeedT::Orders_Snap,          // OLS
          {
            { "239.192.111.2",    "10.50.129.200",    9012 },
            { "239.192.111.130",  "10.50.129.200",    9212 }
          }
        },
        { DataFeedT::Trades_Incr,          // TLR
          {
            { "239.192.111.5",    "10.50.129.200",    9015 },
            { "239.192.111.133",  "10.50.129.200",    9215 }
          }
        },
        { DataFeedT::Instr_Defs,           // IDF
          {
            { "239.192.111.9",    "10.50.129.200",    9019 },
            { "239.192.111.137",  "10.50.129.200",    9219 }
          }
        },
        { DataFeedT::Instr_Status,         // ISF
          {
            { "239.192.111.10",   "10.50.129.200",    9020 },
            { "239.192.111.138",  "10.50.129.200",    9220 }
          }
        }
      }
    },
    //=======================================================================//
    // UAT (Test for CURR) in Co-Location (FX):
    { make_tuple(ProtoVerT::Curr, EnvT::TestL, AssetClassT::FX),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,            // MSR
          {
            { "239.195.1.87",     "91.203.253.238",   16087 },
            { "239.195.1.215",    "91.203.255.238",   17087 }
          }
        },
        { DataFeedT::Orders_Incr,           // OLR
          {
            { "239.195.1.81",     "91.203.253.238",   16081 },
            { "239.195.1.209",    "91.203.255.238",   17081 }
          }
        },
        { DataFeedT::Orders_Snap,           // OLS
          {
            { "239.195.1.82",     "91.203.253.238",   16082 },
            { "239.195.1.210",    "91.203.255.238",   17082 }
          }
        },
        { DataFeedT::Trades_Incr,           // TLR
          {
            { "239.195.1.85",     "91.203.253.238",   16085 },
            { "239.195.1.213",    "91.203.255.238",   17085 }
          }
        },
        { DataFeedT::Instr_Defs,            // IDF
          {
            { "239.195.1.89",     "91.203.253.238",   16089 },
            { "239.195.1.217",    "91.203.255.238",   17089 }
          }
        },
        { DataFeedT::Instr_Status,          // ISF
          {
            { "239.195.1.90",     "91.203.253.238",   16090 },
            { "239.195.1.218",    "91.203.255.238",   17090 }
          }
        }
      }
    },
    //=======================================================================//
    // UAT (Test for CURR) in Co-Location (EQ):
    { make_tuple(ProtoVerT::Curr,     EnvT::TestL, AssetClassT::EQ),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,            // MSR
          {
            { "239.195.1.119",    "91.203.253.239",   16119 },
            { "239.195.1.247",    "91.203.255.239",   17119 }
          }
        },
        { DataFeedT::Orders_Incr,           // OLR
          {
            { "239.195.1.113",    "91.203.253.239",   16113 },
            { "239.195.1.241",    "91.203.255.239",   17113 }
          }
        },
        { DataFeedT::Orders_Snap,           // OLS
          {
            { "239.195.1.114",    "91.203.253.239",   16114 },
            { "239.195.1.242",    "91.203.255.239",   17114 }
          }
        },
        { DataFeedT::Trades_Incr,           // TLR
          {
            { "239.195.1.117",    "91.203.253.239",   16117 },
            { "239.195.1.245",    "91.203.255.239",   17117 }
          }
        },
        { DataFeedT::Instr_Defs,            // IDF
          {
            { "239.195.1.121",    "91.203.253.239",   16121 },
            { "239.195.1.249",    "91.203.255.239",   17121 }
          }
        },
        { DataFeedT::Instr_Status,          // ISF
          {
            { "239.195.1.122",    "91.203.253.239",   16122 },
            { "239.195.1.250",    "91.203.255.239",   17122 }
          }
        }
      }
    },
    //=======================================================================//
    // Test for NEXT over the Internet (FX):
    { make_tuple(ProtoVerT::Next,     EnvT::TestI, AssetClassT::FX),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,            // MSR
          {
            { "239.192.112.7",    "10.50.129.200",   9027 },
            { "239.192.112.135",  "10.50.129.200",   9227 }
          }
        },
        { DataFeedT::Orders_Incr,           // OLR
          {
            { "239.192.112.1",    "10.50.129.200",   9021 },
            { "239.192.112.129",  "10.50.129.200",   9221 }
          }
        },
        { DataFeedT::Orders_Snap,           // OLS
          {
            { "239.192.112.2",    "10.50.129.200",   9022 },
            { "239.192.112.130",  "10.50.129.200",   9222 }
          }
        },
        { DataFeedT::Trades_Incr,           // TLR
          {
            { "239.192.112.5",    "10.50.129.200",   9025 },
            { "239.192.112.133",  "10.50.129.200",   9225 }
          }
        },
        { DataFeedT::Instr_Defs,            // IDF
          {
            { "239.192.112.9",    "10.50.129.200",   9029 },
            { "239.192.112.137",  "10.50.129.200",   9229 }
          }
        },
        { DataFeedT::Instr_Status,          // ISF
          {
            { "239.192.112.10",   "10.50.129.200",   9030 },
            { "239.192.112.138",  "10.50.129.200",   9230 }
          }
        }
      }
    },
    //=======================================================================//
    // Test for NEXT over the Internet (EQ):
    { make_tuple(ProtoVerT::Next,     EnvT::TestI, AssetClassT::EQ),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,            // MSR
          {
            { "239.192.113.7",    "10.50.129.200",   9117 },
            { "239.192.113.135",  "10.50.129.200",   9317 }
          }
        },
        { DataFeedT::Orders_Incr,           // OLR
          {
            { "239.192.113.1",    "10.50.129.200",   9111 },
            { "239.192.113.129",  "10.50.129.200",   9311 }
          }
        },
        { DataFeedT::Orders_Snap,           // OLS
          {
            { "239.192.113.2",    "10.50.129.200",   9112 },
            { "239.192.113.130",  "10.50.129.200",   9312 }
          }
        },
        { DataFeedT::Trades_Incr,           // TLR
          {
            { "239.192.113.5",    "10.50.129.200",   9115 },
            { "239.192.113.133",  "10.50.129.200",   9315 }
          }
        },
        { DataFeedT::Instr_Defs,            // IDF
          {
            { "239.192.113.9",    "10.50.129.200",   9119 },
            { "239.192.113.137",  "10.50.129.200",   9319 }
          }
        },
        { DataFeedT::Instr_Status,          // ISF
          {
            { "239.192.113.10",   "10.50.129.200",   9120 },
            { "239.192.113.138",  "10.50.129.200",   9320 }
          }
        }
      }
    },
    //=======================================================================//
    // Test for NEXT in Co-Location (FX):
    { make_tuple(ProtoVerT::Next,     EnvT::TestL, AssetClassT::FX),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,            // MSR
          {
            { "239.195.1.77",     "91.203.253.238",   16077 },
            { "239.195.1.205",    "91.203.255.238",   17077 }
          }
        },
        { DataFeedT::Orders_Incr,           // OLR
          {
            { "239.195.1.71",     "91.203.253.238",   16071 },
            { "239.195.1.199",    "91.203.255.238",   17071 }
          }
        },
        { DataFeedT::Orders_Snap,           // OLS
          {
            { "239.195.1.72",     "91.203.253.238",   16072 },
            { "239.195.1.200",    "91.203.255.238",   17072 }
          }
        },
        { DataFeedT::Trades_Incr,           // TLR
          {
            { "239.195.1.75",     "91.203.253.238",   16075 },
            { "239.195.1.203",    "91.203.255.238",   17075 }
          }
        },
        { DataFeedT::Instr_Defs,            // IDF
          {
            { "239.195.1.79",     "91.203.253.238",   16079 },
            { "239.195.1.207",    "91.203.255.238",   17079 }
          }
        },
        { DataFeedT::Instr_Status,          // ISF
          {
            { "239.195.1.80",     "91.203.253.238",   16080 },
            { "239.195.1.208",    "91.203.255.238",   17080 }
          }
        }
      }
    },
    //=======================================================================//
    // Test for NEXT in Co-Location (EQ):
    { make_tuple(ProtoVerT::Next,     EnvT::TestL, AssetClassT::EQ),
    //=======================================================================//
      {
        { DataFeedT::Stats_Incr,            // MSR
          {
            { "239.195.1.107",    "91.203.253.239",   16107 },
            { "239.195.1.235",    "91.203.255.239",   17107 }
          }
        },
        { DataFeedT::Orders_Incr,           // OLR
          {
            { "239.195.1.101",    "91.203.253.239",   16101 },
            { "239.195.1.229",    "91.203.255.239",   17101 }
          }
        },
        { DataFeedT::Orders_Snap,           // OLS
          {
            { "239.195.1.102",    "91.203.253.239",   16102 },
            { "239.195.1.230",    "91.203.255.239",   17102 }
          }
        },
        { DataFeedT::Trades_Incr,           // TLR
          {
            { "239.195.1.105",    "91.203.253.239",   16105 },
            { "239.195.1.233",    "91.203.255.239",   17105 }
          }
        },
        { DataFeedT::Instr_Defs,            // IDF
          {
            { "239.195.1.109",    "91.203.253.239",   16109 },
            { "239.195.1.237",    "91.203.255.239",   17109 }
          }
        },
        { DataFeedT::Instr_Status,          // ISF
          {
            { "239.195.1.110",    "91.203.253.239",   16110 },
            { "239.195.1.238",    "91.203.255.239",   17110 }
          }
        }
      }
    }
  };
} // End namespace MICEX
} // End namespace FAST
} // End namespace MAQUETTE
