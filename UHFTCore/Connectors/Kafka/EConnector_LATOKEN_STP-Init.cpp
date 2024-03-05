// vim:ts=2:et
//===========================================================================//
//            "Connectors/Kafka/EConnector_LATOKEN_STP-Init.cpp":            //
//     Kafka-Based STP/DropCopy for LATOKEN:  Init/Start/Stop/Recovery       //
//===========================================================================//
#include "Basis/OrdMgmtTypes.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Basis/Maths.hpp"
#include "Basis/IOUtils.hpp"
#include "Connectors/Kafka/EConnector_LATOKEN_STP.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Protocols/H2WS/LATOKEN-OMC.h"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "Venues/LATOKEN/SecDefs.h"
#include "Venues/LATOKEN/Configs_WS.h"
#include "Venues/Binance/SecDefs.h"
#include <utxx/compiler_hints.hpp>
#include <boost/algorithm/string.hpp>
#include <cstring>
#include <cassert>

using namespace std;

namespace
{
  using namespace MAQUETTE;

  //=========================================================================//
  // Lists of Kafka Brokers:                                                 //
  //=========================================================================//
  inline char const* GetKafkaBrokers(MQTEnvT a_env)
  {
    return
      (a_env == MQTEnvT::Prod)
      ? "192.168.10.24,192.168.10.25,192.168.10.26"  // Prod
      : "35.198.78.15,35.198.68.141,35.242.230.98";  // Pre-Prod = Test
      // "10.156.0.2,10.156.0.3,10.156.0.4"          // Pre-Prod iternal IPs
  }

  //=========================================================================//
  // Kafka Consumer and Producer Configs:                                    //
  //=========================================================================//
  // (Will be modified at run-time):
  //-------------------------------------------------------------------------//
  // Consumer:                                                               //
  //-------------------------------------------------------------------------//
  string const KafkaConsumerConfig[][2]
  {
    { "client.id",                    "EConnector_LATOKEN_STP" },
    { "metadata.broker.list",         ""         },  // Installed at Run-Time
    { "group.id",                     ""         },  // Installed at Run-Time
    { "enable.auto.commit",           "false"    },  // IMPORTANT!
    { "enable.auto.offset.store",     "false"    },  // IMPORTANT!
    { "socket.keepalive.enable",      "true"     },
    { "socket.timeout.ms",            "60000"    },
    { "log.connection.close",         "false"    },
    { "queue.buffering.max.messages", "500000"   },
    { "batch.num.messages",           "1000"     },
    { "max.poll.interval.ms",         "86400000" },  // 1d! Make it LONG!
    { "log_level",                    "4"        }
  };

  //-------------------------------------------------------------------------//
  // "Producer":                                                             //
  //-------------------------------------------------------------------------//
  string const KafkaProducerConfig[][2]
  {
    { "metadata.broker.list",         ""        },
    { "socket.keepalive.enable",      "true"    },
    { "socket.timeout.ms",            "60000"   },
    { "log.connection.close",         "false"   },
    { "linger.ms",                    "1000"    },
    { "acks",                         "1"       },  // XXX: Why???
    { "batch.num.messages",           "10000"   },
    { "log_level",                    "4"       }
  };

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "IsMatch(A, B, SecDefD)":                                               //
  //-------------------------------------------------------------------------//
  // Does the given pair (A, B) match the SecDefD?
  //
  inline bool IsMatch
  (
    char    const* a_a,
    char    const* a_b,
    SecDefD const& a_instr
  )
  {
    assert(a_a != nullptr && a_b != nullptr);
    return
      (strcmp(a_instr.m_AssetA  .data(), a_a) == 0) &&
      (strcmp(a_instr.m_QuoteCcy.data(), a_b) == 0);
  }

  //-------------------------------------------------------------------------//
  // "IsMatch(A,B)":                                                         //
  //-------------------------------------------------------------------------//
  // Does the given pair (A, B) matches (Asset, RFC) or (RFC, Asset)?
  //
  inline bool IsMatch
  (
    char const*   a_a,
    char const*   a_b,
    SymKey const& a_asset,
    Ccy           a_rfc
  )
  {
    assert(a_a != nullptr  && a_b != nullptr);
    return
      (strcmp(a_asset.data(), a_a) == 0 && strcmp(a_rfc.data(), a_b) == 0) ||
      (strcmp(a_asset.data(), a_b) == 0 && strcmp(a_rfc.data(), a_a) == 0);
  }

  //-------------------------------------------------------------------------//
  // "SelectUsefulInstrs":                                                   //
  //-------------------------------------------------------------------------//
  // From the given list of AllRemote_Instrs, select those which EITHER
  // (*) directly match one of OurInstrs by (A, B, SettlDate);   OR
  // (*) match (Asset/RFC) or (RFC/Asset) for one of our Assets, with a matching
  //     SettlDate:
  //
  inline void SelectUsefulInstrs
  (
    vector<SecDefS>        const& a_all_remote_instrs,
    vector<SecDefD const*> const& a_our_instrs,
    set<SymKey>            const& a_our_assets,
    Ccy                           a_rfc,
    vector<string>*               a_useful_remote_symbols
  )
  {
    assert(a_useful_remote_symbols != nullptr &&
           a_useful_remote_symbols->empty());

    for (SecDefS const& remS: a_all_remote_instrs)
    {
      // A and B Assets of "remS":
      char const* a = remS.m_AssetA  .data();
      char const* b = remS.m_QuoteCcy.data();

      // First, does "remS" directly match any of OurInstrs, so the former can
      // potentially be used for UnRPnL computation on that OurInstr (if we la-
      // ter find that the corresp SecDefD has a matching SettlDate):
      for (SecDefD const* ourDef: a_our_instrs)
      {
        assert(ourDef != nullptr);
        if (utxx::unlikely(IsMatch(a, b, *ourDef)))
        {
          a_useful_remote_symbols->push_back(string(remS.m_Symbol.data()));
          goto NextRemS;
        }
      }
      // If we got here: Check whether "remS" matches (Asset/RFC) or inverse,
      // for any of OurAssets (again, disregarding any SettlDates):
      for (SymKey const& asset: a_our_assets)
        if (utxx::unlikely(IsMatch(a, b, asset, a_rfc)))
        {
          a_useful_remote_symbols->push_back(string(remS.m_Symbol.data()));
          goto NextRemS;
        }
    NextRemS: ;
    }
    assert(a_useful_remote_symbols->size() <= a_all_remote_instrs.size());
  }

  //-------------------------------------------------------------------------//
  // "KafkaLogger":                                                          //
  //-------------------------------------------------------------------------//
  void KafkaLogger
  (
    rd_kafka_t const* a_kafka,
    int               a_level,
    char const*       a_hmm,
    char const*       a_msg
  )
  {
    // NB: In "EConnector_LATOKEN_STP", the Logger is made Thread-Safe, because
    // it may be invoked from the context of any Kafka thread:
    spdlog::logger* logger = EConnector_LATOKEN_STP::GetLogger();

    if (utxx::unlikely(logger == nullptr))
      return;

    // OK, got the Logger. XXX: For now, we treat all Kafka msgs at level < 7
    // Warnings (though there may be Errors as well -- never mind), and at 7+,
    // as Info. Our own DebugLevel is disregarded at this point:
    //
    char const* kafkaName = (a_kafka != nullptr) ? rd_kafka_name(a_kafka) : "";
    assert(a_hmm != nullptr && a_msg != nullptr);

    if (a_level < 7)
      logger->warn("Kafka: {}|{}| {}", a_hmm, kafkaName, a_msg);
    else
      logger->info("Kafka: {}|{}| {}", a_hmm, kafkaName, a_msg);
  }
}

namespace MAQUETTE
{
  //=========================================================================//
  // Static Ptr to Singleton Instance of "EConnector_LATOKEN_STP":           //
  //=========================================================================//
  // XXX: It does not really implement a thread-safe Singleton pattern:
  //
  EConnector_LATOKEN_STP const* EConnector_LATOKEN_STP::s_Singleton =
    nullptr;

  //=========================================================================//
  // Kafka Consumer and Producer Topics: Prod:                               //
  //=========================================================================//
  char const* const EConnector_LATOKEN_STP::s_kafkaConsumerInstrTopicsProd
                   [EConnector_LATOKEN_STP::NConsTopics]
  {
    // "Instr" info is obtained from "Trade" Topics.
    // NB: The order is important here!
    "prd.cex.gz2.trd.ug.10",
    "prd.cex.gz2.trd.ug.11",
    "prd.cex.gz2.trd.ug.12",
    "prd.cex.gz2.trd.ug.13"
  };

  char const* const EConnector_LATOKEN_STP::s_kafkaConsumerAssetTopicsProd
                   [EConnector_LATOKEN_STP::NConsTopics]
  {
    // "Asset" info is obtained from "Balance" Topics.
    // NB: The order is important here!
    "prd.cex.gz2.bal.ug.10",
    "prd.cex.gz2.bal.ug.11",
    "prd.cex.gz2.bal.ug.12",
    "prd.cex.gz2.bal.ug.13"
  };

  char const* const EConnector_LATOKEN_STP::s_kafkaProducerInstrTopicsProd
                   [EConnector_LATOKEN_STP::NProdTopics]
  {
    // NB: The order is important here!
    "prd.cex.dz1.pnl.ins.ug.10",
    "prd.cex.dz1.pnl.ins.ug.11",
    "prd.cex.dz1.pnl.ins.ug.12",
    "prd.cex.dz1.pnl.ins.ug.13"
  };

  char const* const EConnector_LATOKEN_STP::s_kafkaProducerAssetTopicsProd
                   [EConnector_LATOKEN_STP::NProdTopics]
  {
    // NB: The order is important here!
    "prd.cex.dz1.pnl.ast.ug.10",
    "prd.cex.dz1.pnl.ast.ug.11",
    "prd.cex.dz1.pnl.ast.ug.12",
    "prd.cex.dz1.pnl.ast.ug.13"
  };

  //=========================================================================//
  // Kafka Consumer and Producer Topics: Test (aka Pre-Prod):                //
  //=========================================================================//
  char const* const EConnector_LATOKEN_STP::s_kafkaConsumerInstrTopicsTest
                   [EConnector_LATOKEN_STP::NConsTopics]
  {
    // "Instr" info is obtained from "Trade" Topics.
    // NB: The order is important here!
    "pre-prd.cex.gz2.trd.ug.10",
    "pre-prd.cex.gz2.trd.ug.11",
    "pre-prd.cex.gz2.trd.ug.12",
    "pre-prd.cex.gz2.trd.ug.13"
  };

  char const* const EConnector_LATOKEN_STP::s_kafkaConsumerAssetTopicsTest
                   [EConnector_LATOKEN_STP::NConsTopics]
  {
    // "Asset" info is obtained from "Balance" Topics.
    // NB: The order is important here!
    "pre-prd.cex.gz2.bal.ug.10",
    "pre-prd.cex.gz2.bal.ug.11",
    "pre-prd.cex.gz2.bal.ug.12",
    "pre-prd.cex.gz2.bal.ug.13"
  };

  char const* const EConnector_LATOKEN_STP::s_kafkaProducerInstrTopicsTest
                   [EConnector_LATOKEN_STP::NProdTopics]
  {
    // NB: The order is important here!
    "pre-prd.cex.dz1.pnl.ins.ug.10",
    "pre-prd.cex.dz1.pnl.ins.ug.11",
    "pre-prd.cex.dz1.pnl.ins.ug.12",
    "pre-prd.cex.dz1.pnl.ins.ug.13"
  };

  char const* const EConnector_LATOKEN_STP::s_kafkaProducerAssetTopicsTest
                   [EConnector_LATOKEN_STP::NProdTopics]
  {
    // NB: The order is important here!
    "pre-prd.cex.dz1.pnl.ast.ug.10",
    "pre-prd.cex.dz1.pnl.ast.ug.11",
    "pre-prd.cex.dz1.pnl.ast.ug.12",
    "pre-prd.cex.dz1.pnl.ast.ug.13"
  };

  //=========================================================================//
  // "EConnector_LATOKEN_STP" Non-Default Ctor:                              //
  //=========================================================================//
  EConnector_LATOKEN_STP::EConnector_LATOKEN_STP
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_all_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector":                                                         //
    //-----------------------------------------------------------------------//
    // XXX: From the "EConnector" Ctor perspective, IsOMC=false here, because
    // we do not need to allocate ShM space for AOSes, Req12s and Trades  :
    //
    // AccountKey would typically be {AccountPfx}-LATOKEN-STP-{Prod|Test} :
    //
    EConnector
    (
      a_all_params.get<string>("STP.AccountKey"),
      "LATOKEN",
      ShMSz,           // Estimated ShM size
      a_reactor,
      false,           // No BusyWait
      a_sec_defs_mgr,
      LATOKEN::SecDefs,
      nullptr,         // No constraints -- we really need ALL symbols
      false,           // Explicit SettlDates are NOT required!
      false,           // No tenors in SecIDs
      a_risk_mgr,
      GetParamsSubTree(a_all_params, "STP"),
      LATOKEN::QT,
      is_floating_point_v<LATOKEN::QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_LATOKEN_STP" ItSelf:                                      //
    //-----------------------------------------------------------------------//
    m_withXTrds     (a_all_params.get<bool>  ("STP.WithSelfTrades", false)),
    // Of course, NoDB mode if OFF by default:
    m_noDBMode      (a_all_params.get<bool>  ("STP.SkipDBOutput",   false)),
    // Normally, we use Kafka-carried L1 OrderBook updates for UnRPnL valuations
    // on LATOKEN, but if they are unavailable, use LastTrades:
    m_useLATrades   (a_all_params.get<bool>  ("STP.UseLATrades",    false)),
    m_isActive      (false),
    m_stpDynInitMode(false),
    // Kafka-related flds are initialised below. NB: "m_topicN" is normally -1;
    // if set to 0..(NConsTopics-1), then only  the corresp Topic will be used
    // (for testing only):
    m_kafkaC        (nullptr),  // Consumer
    m_kafkaP        (nullptr),  // Producer
    m_kq            (nullptr),
    m_kfd           (-1),
    m_rfd           (-1),
    m_topicN        (a_all_params.get<int>   ("STP.KafkaTopicN", -1)),
    m_batchSzC      (1),        // For Consumer only
    m_kpartsC       (nullptr),  // ditto
    m_msgCount      (0),        // ditto
    m_instrTopics   (),
    m_assetTopics   (),
    // XXX: "DRMLess" and "DRMAlloc" objs are copied into "DRMSet", so NO need
    // to create them in ShM   (and for "DRMAlloc", it would be impossible any-
    // way):
    m_drmOrdSet     (m_pm.GetSegm()->find_or_construct<DRMSet>("DRMOrdSet")
                      (FStrLessT<ObjNameSz>(),
                       DRMSetAlloc(m_pm.GetSegm()->get_segment_manager())
                    )),
    m_drmTrdSet     (m_pm.GetSegm()->find_or_construct<DRMSet>("DRMTrdSet")
                      (FStrLessT<ObjNameSz>(),
                       DRMSetAlloc(m_pm.GetSegm()->get_segment_manager())
                    )),
    m_drmBalSet     (m_pm.GetSegm()->find_or_construct<DRMSet>("DRMBalSet")
                      (FStrLessT<ObjNameSz>(),
                       DRMSetAlloc(m_pm.GetSegm()->get_segment_manager())
                    )),
    m_drmDEQ        (m_pm.GetSegm()->find_or_construct<DRMDEQ>("DRMDEQ")
                      (DRMDEQAlloc(m_pm.GetSegm()->get_segment_manager())
                    )),
    m_liqStatsMap   (),
    m_rbuff         (8192),       // LWM
    m_fdInfo        (),           // Zeroed-out
    m_reader        (),
    m_handler       (m_logger, m_debugLevel),
    m_sbuff         (),
    // MDCs are also initialised in the following:
    m_instrsBinance (),
    m_mdcBinance    (nullptr),
    m_allMDCs       (),
    m_nLABooks      (0),          // Initialised below
    m_laBooksShM    (nullptr),    // ditto
    m_ccyIDs        (),           // Filled   in below
    m_ccyNames      (),           // ditto
    m_feeRates      (),           // ditto
    m_instrThrots   (),
    m_assetThrots   (),
    m_throtLimit    (long(Ceil
                      (double(MtMThrotWinSec) /
                       a_all_params.get<double>("STP.MDOutputPeriodSec", 10.0))
                    ))
  {
    //=======================================================================//
    // Checks:                                                               //
    //=======================================================================//
    // First of all, it is a Singleton: Only 1 instance can be created within
    // the current process:
    if (s_Singleton != nullptr)
      throw utxx::badarg_error
            ("EConnector_LATOKEN_STP: Singleton: No more instances allowed");

    // If OK: Memoise the ptr to this instance:
    s_Singleton = this;

    // Obviously, RiskMgr must be present, as this is the main purpose of STP:
    if (utxx::unlikely(m_riskMgr == nullptr))
      throw utxx::badarg_error
            ("EConnector_LATOKEN_STP::Ctor: RiskMgr is compulsory");

    assert(m_secDefsMgr != nullptr && m_drmOrdSet != nullptr &&
           m_drmTrdSet  != nullptr && m_drmBalSet != nullptr &&
           m_drmDEQ     != nullptr && m_throtLimit > 0);

    //=======================================================================//
    // LATOKEN OrderBooks in ShM:                                            //
    //=======================================================================//
    // Although some Instrs may be valued by external MDCs, provide space for
    // ALL LATOKEN Instrs:
    if (utxx::unlikely(m_usedSecDefs.size() > MaxLABooks))
      throw utxx::runtime_error
            ("EConnector_LATOKEN_STP::Ctor: Too many LATOKEN Instrs: ",
             m_usedSecDefs.size(), ", MaxAllowed: ", MaxLABooks);

    // Try to get the ShM Array for LATOKEN OrderBooks:
    unsigned maxLABooks = MaxLABooks;
    m_laBooksShM        =
      m_pm.FindOrConstruct<OrderBookBase>("LAOrderBooks", &maxLABooks);

    if (utxx::unlikely(m_laBooksShM == nullptr || maxLABooks != MaxLABooks))
      throw utxx::runtime_error
            ("EConnector_LATOKEN_STP::Ctor: LAOrderBooks Alloc Error: ",
             ((m_laBooksShM == nullptr) ? " NULL, " : ""),   "MaxBooks=",
             maxLABooks, ", Expected=",     MaxLABooks);
    // OK:
    m_nLABooks = int(m_usedSecDefs.size());
    assert(maxLABooks == MaxLABooks && m_laBooksShM != nullptr &&
           m_nLABooks <= int(MaxLABooks));

    // Place "OrderBookBase" objs in ShM:
    for (int i = 0; i < m_nLABooks; ++i)
    {
      SecDefD const*  instr = m_usedSecDefs[size_t(i)];
      assert(instr != nullptr);

      // Placement "new": MDC=NULL. This creates an empty "OrderBookBase" with
      // L1 Pxs set to NaN:
      new (m_laBooksShM + i) OrderBookBase(nullptr, instr);
    }
    //=======================================================================//
    // RiskMgr and MDCs SetUp:                                               //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Construct the set of LATOKEN Assets:                                  //
    //-----------------------------------------------------------------------//
    // (And check whether all of them are Spot, ie have SettlDate=0):
    set<SymKey> laAssets;

    // First, from the Instruments:
    for (SecDefD const* laDef: m_usedSecDefs)
    {
      assert(laDef != nullptr);
      laAssets.insert(laDef->m_AssetA);
      laAssets.insert(MkSymKey(laDef->m_QuoteCcy.data()));
    }

    // Then add any other Assets from the list of LATOKEN Ccys:
    for (auto const&  asset3: LATOKEN::Assets)
      laAssets.insert(MkSymKey(get<2>(asset3)));

    LOG_INFO(1,
      "EConnector_LATOKEN_STP::Ctor: {} LATOKEN Assets", laAssets.size())

    // IMPORTANT:
    // Explicitly install all "laAssets" with SettlDate=0 and UserID=0 in the
    // RiskMgr, so they can be used as prototypes:
    //
    for (SymKey const& assetName: laAssets)
      (void) m_riskMgr->GetAssetRisks(assetName.data(), 0, 0);

    //=======================================================================//
    // Create the Binance MDC:                                               //
    //=======================================================================//
    // Construct the list of required Binance Instrs:
    // NB: We need "instrBinance" instead of the fill list "Binance::SecDefs",
    // in order not to create too many unnecessary OrderBooks:
    vector<string> instrsBinance;
    SelectUsefulInstrs
      (Binance::SecDefs_Spt, m_usedSecDefs, laAssets, m_riskMgr->GetRFC(),
       &instrsBinance);

    LOG_INFO(1,
      "EConnector_LATOKEN_STP::Ctor: Found {} Valuation Instruments at "
      "Binance, Creating Binance MDC", instrsBinance.size())

    // Make sure that "ExplSettlDates" are set to "false" in Binance params,
    // as "instrsBinance" already provide restriction on the Binance Instrs
    // to use:
    boost::property_tree::ptree& paramsBinance =  // Ref!
      const_cast<boost::property_tree::ptree&>
      (GetParamsSubTree(a_all_params, "MDC-Binance"));
    paramsBinance.put("ExplSettlDatesOnly", false);

    // Use the same Reactor, SecDefsMgr and RiskMgr as ourselves. RiskMgr  is
    // provided so that the MDC can invoke "OnMktDataUpdate" method on it, on
    // Registered and Valuator instrs:
    //
    m_mdcBinance = new EConnector_H2WS_Binance_MDC<Binance::AssetClassT::Spt>
    (
      m_reactor,
      m_secDefsMgr,
      &instrsBinance,
      m_riskMgr,
      paramsBinance,
      nullptr
    );
    assert(m_mdcBinance != nullptr);
    m_allMDCs.push_back(m_mdcBinance);

    //=======================================================================//
    // Register all LATOKEN Instruments and External MDCs with the RiskMgr:  //
    //=======================================================================//
    for (SecDefD const* laDef: m_usedSecDefs)
    {
      assert(laDef != nullptr);
      //---------------------------------------------------------------------//
      // Try to get an External BenchMark OrderBook for this Instrument:     //
      //---------------------------------------------------------------------//
      // NB: This is only required for UnRealised PnL computation, NOT for NAV,
      // and is NOT absolutely critical):
      //
      OrderBookBase const* instrOB   = nullptr;
      char          const* a         = laDef->m_AssetA  .data();
      char          const* b         = laDef->m_QuoteCcy.data();
      int                  settlDate = laDef->m_SettlDate;

      // Traverse the configured External MDCs (Binance and any others):
      for (EConnector_MktData const* mdc: m_allMDCs)
      {
        if (instrOB  != nullptr)
          break;  // Already found
        assert(mdc != nullptr);

        for (OrderBook const& ob: mdc->GetOrderBooks())
        {
          // Get the corresp Instr and check its Assets and SettlDate:
          SecDefD const& instr =  ob.GetInstr();

          if (utxx::unlikely
             (IsMatch(a, b, instr) && instr.m_SettlDate == settlDate))
          {
            instrOB = &ob;
            break;
          }
        }
      }
      //---------------------------------------------------------------------//
      // Not found? Try to find an internal LATOKEN OrderBook then:          //
      //---------------------------------------------------------------------//
      if (instrOB == nullptr)
        for (int i = 0; i < m_nLABooks; ++i)
          if (&(m_laBooksShM[i].GetInstr()) == laDef)
          {
            instrOB = m_laBooksShM + i;
            break;
          }
      // Still Not Found (Very Unlikely): Produce a warning and do without such
      // an OrderBook:
      if (instrOB == nullptr)
        LOG_WARN(1,
          "EConnector_LATOKEN_STP::Ctor: No OrderBook for Instr={}",
          laDef->m_FullName.data())

      //---------------------------------------------------------------------//
      // Finally: Register this OrderBook (or NULL) for UnrPnL Updates:      //
      //---------------------------------------------------------------------//
      m_riskMgr->Register(*laDef, instrOB);

      // Also create an MtM Msg Rate Throttler for this Instr:
      m_instrThrots.insert
        (make_pair(laDef->m_SecID, Throttler(MtMThrotWinSec)));
    }

    //=======================================================================//
    // Install Valuators for all LATOKEN Assets:                             //
    //=======================================================================//
    // NB:  This needs to be done ON TOP of registering Instrument OrderBooks
    // above. Do it for UserID=0 only (as others may or may not be created at
    // this point)m thus installing Valuators in "prototype" "AssetRisks". For
    // all other UserIDs, the Valuators will be propagated automatically:
    //
    auto const& ars = m_riskMgr->GetAllAssetRisks(0);
    for (auto const&  arp: ars)
    {
      AssetRisks const& ar = arp.second;
      //---------------------------------------------------------------------//
      // Again, search all External MDCs and their OrderBooks:               //
      //---------------------------------------------------------------------//
      // Try to find one which can be used as a Valuator for this asset (ie for
      // Asset/RFC or RFC/Asset):
      //
      OrderBookBase const* assetOB = nullptr;

      for (EConnector_MktData const* mdc: m_allMDCs)
      {
        if (assetOB  != nullptr)
          break;  // Already found
        assert(mdc != nullptr);

        for (OrderBook const&    ob: mdc->GetOrderBooks())
        {
          SecDefD const& instr = ob.GetInstr();

          // Check for "instr" applicability:
          if (utxx::unlikely
             (IsMatch
               (instr.m_AssetA.data(), instr.m_QuoteCcy.data(),
                ar.m_asset,            m_riskMgr->GetRFC())  &&
              instr.m_SettlDate  ==    ar.m_settlDate))
          {
            assetOB = &ob;
            break;
          }
        }
      }
      //---------------------------------------------------------------------//
      // If not found, serach the LATOKEN "Poor Man's" OrderBooks:           //
      //---------------------------------------------------------------------//
      if (assetOB == nullptr)
        for (int i = 0; i < m_nLABooks; ++i)
        {
          SecDefD const& instr = m_laBooksShM[i].GetInstr();

          // Again, check for "instr" applicability:
          if (utxx::unlikely
             (IsMatch
               (instr.m_AssetA.data(), instr.m_QuoteCcy.data(),
                ar.m_asset,            m_riskMgr->GetRFC())  &&
              instr.m_SettlDate  ==    ar.m_settlDate))
          {
            assetOB = m_laBooksShM + i;
            break;
          }
        }
      //---------------------------------------------------------------------//
      // Found? Install the Valuator:                                        //
      //---------------------------------------------------------------------//
      // XXX: Do NOT install it directly in "ar"; use the RiskMgr API for that,
      // as the RiskMgr needs to do some extra house-keping   (with minor over-
      // head):
      if (utxx::likely(assetOB != nullptr))
      {
        m_riskMgr->InstallValuator(ar.m_asset.data(), ar.m_settlDate, assetOB);
        assert(ar.m_evalOB1 == assetOB);
      }
      else
        // XXX: For the moment, the default (Trivial) valuator will be used:
        LOG_WARN(2,
          "EConnector_LATOKEN_STP::Ctor: No OrderBook for Asset={}: "
          "Using Trivial Valuator", ar.m_asset.data())
    }
    //=======================================================================//
    // Register our Call-Backs with the RiskMgr:                             //
    //=======================================================================//
    m_riskMgr->SetCallBacks
    (
      //---------------------------------------------------------------------//
      // "InstrRisksUpdateCB":                                               //
      //---------------------------------------------------------------------//
      [this]
      (
        InstrRisks const& a_ir,        RMQtyA          a_ppos_a,
        char const*       a_trade_id,  bool            a_is_buy,
        PriceT            a_px_ab,     RMQtyA          a_qty_a,
        RMQtyB            a_rgain_b,   double          a_b_rfc,
        double            a_rgain_rfc
      )
      ->void
      {
        this->InstrRisksUpdate
          (a_ir,          a_ppos_a,    a_trade_id,     a_is_buy,
           a_px_ab,       a_qty_a,     a_rgain_b,      a_b_rfc,
           a_rgain_rfc);
      },

      //---------------------------------------------------------------------//
      // "AssetRisksUpdate":                                                 //
      //---------------------------------------------------------------------//
      [this]
      (
        AssetRisks const& a_ar,        char const*     a_trans_id,
        AssetRisks::AssetTransT a_trans_t,
        double            a_qty,       double          a_eval_rate
      )
      ->void
      {
        this->AssetRisksUpdate
          (a_ar,          a_trans_id,  a_trans_t,      a_qty,
           a_eval_rate);
      }
    );

    //=======================================================================//
    // Kafka SetUp:                                                          //
    //=======================================================================//
    char kerr[512];  // Possible err msgs will come here

    //-----------------------------------------------------------------------//
    // Initialise the Kafka Consumer Config (similar to LATOKEN WSTGW):      //
    //-----------------------------------------------------------------------//
    rd_kafka_conf_t* kconfC = rd_kafka_conf_new();
    assert(kconfC != nullptr);

    // XXX: Do NOT allow any ptrs to local vars in the following Configs; they
    // will be shallow-copied into "m_kafkaC" and become dangling!
    // XXX: "security.protocol=ssl" would be desirable, but is unfortunately
    // NOT supported by the Producer:
    //
    string const& accountPfx =  a_all_params.get<string>("STP.AccountPfx");

    for (string const (&kv)[2]: KafkaConsumerConfig)
    {
      string const& key = kv[0];
      string&       val = const_cast<string&>(kv[1]); // Mutable!

      // Modify some vals which are not known at compile-time:
      if (key == "group.id")
        val = accountPfx + "-" + string(m_riskMgr->GetRFC().data());
      else
      if (key == "metadata.broker.list")
        val = GetKafkaBrokers(m_cenv);
      else
      if (key == "batch.num.messages")
        // Memoise it:
        m_batchSzC = atoi(val.data());

      // Set the conf line:
      if (utxx::unlikely
         (rd_kafka_conf_set(kconfC, key.data(), val.data(), kerr, sizeof(kerr))
         != 0))
        throw utxx::runtime_error
              ("EConnector_LATOKEN_STP::Ctor: Cannot set Kafka Consumer Conf: ",
               key, '=', val, ": ", kerr);
    }
    assert(m_batchSzC > 0);

    //-----------------------------------------------------------------------//
    // Initialise the Kafka Consumer:                                        //
    //-----------------------------------------------------------------------//
    // NB: "kconfC" becomes aliased in "m_kafkaC", we do not need to keep this
    // ptr -- but do NOT de-allocate the obj:
    m_kafkaC =
      rd_kafka_new(RD_KAFKA_CONSUMER, kconfC, kerr, sizeof(kerr));
    if (utxx::unlikely(m_kafkaC == nullptr))
      throw utxx::runtime_error
            ("EConnector_LATOKEN_STP::Ctor: Cannot init Kafka Consumer: ",
             kerr);

    // Get the Consumer Kafka Queue (with the Main Queue forwarded into it,
    // just in case):
    if (utxx::unlikely(rd_kafka_poll_set_consumer(m_kafkaC) != 0))
      throw utxx::runtime_error
            ("EConnector_LATOKEN_STP::Ctor: Cannot redirect MainQ->ConsQ");

    m_kq = rd_kafka_queue_get_consumer(m_kafkaC);
    assert(m_kq != nullptr);

    //-----------------------------------------------------------------------//
    // Create a Kafka Event FD and attach it to the Reactor:                 //
    //-----------------------------------------------------------------------//
    // We actually create a pipe, with one end going into Kafka and the other
    // one into our EPollReactor:
    //
    int fds[2] {-1, -1};
    if (utxx::unlikely(pipe(fds) < 0))
      IO::SystemError(-1, "EConnector_LATOKEN_STP::Ctor: Cannot create a Pipe");

    // If OK:
    m_rfd = fds[0];  // Our side, to be added to the Reactor
    m_kfd = fds[1];  // Kafka side
    assert(m_kfd >= 0 && m_rfd >= 0);

    // "m_kfd" is registered with Kafka, which will send a standard 1-byte msg
    // to notify out Reactor on the activities on the Kafka side:
    //
    rd_kafka_queue_io_event_enable(m_kq, m_kfd, "K", 1);

    // "m_rfd" is attached to our EPollReactor  (it will be made Non-Blocking
    // automatically):
    assert(m_reactor != nullptr);
    m_reactor->AddRawInput
    (
      "KafkaNotificationFD",
      m_rfd,
      true,    // Make it Edge-Triggered and thus Non-Blocking

      // Raw Input Handler:
      [this](int DEBUG_ONLY(a_fd))->void
      {
        assert(a_fd == m_rfd);
        this->GetKafkaMsgs();
      },

      // Error Handler:
      [this]
      (int DEBUG_ONLY(a_fd),  int a_err_code,  uint32_t a_events,
       char const*    a_msg)->void
      {
        assert(a_fd == m_rfd);
        this->KafkaPipeError(0, a_err_code, a_events, a_msg);
      }
    );
    // Reserve a decent capacity of "m_rbuff" (twice the LWM), just to be safe:
    m_rbuff.reserve(16384);

    //-----------------------------------------------------------------------//
    // Initialise the Kafka Producer Config:                                 //
    //-----------------------------------------------------------------------//
    rd_kafka_conf_t* kconfP = rd_kafka_conf_new();
    assert(kconfP != nullptr);

    // Again, make sure there are no dangling ptrs:
    for (string const (&kv)[2]: KafkaProducerConfig)
    {
      string const& key = kv[0];
      string&       val = const_cast<string&>(kv[1]); // Mutable!

      if (key == "metadata.broker.list")
        val = GetKafkaBrokers(m_cenv);

      // Set the conf line:
      if (utxx::unlikely
         (rd_kafka_conf_set(kconfP, key.data(), val.data(), kerr, sizeof(kerr))
         != 0))
        throw utxx::runtime_error
              ("EConnector_LATOKEN_STP::Ctor: Cannot set Kafka Producer Conf: ",
               key, '=', val, ": ", kerr);
    }
    //-----------------------------------------------------------------------//
    // Initialise the Kafka Producer:                                        //
    //-----------------------------------------------------------------------//
    // NB: "kconfP" becomes aliased in "m_kafkaP", we do not need to keep this
    // ptr -- but do NOT de-allocate the obj:
    m_kafkaP =
      rd_kafka_new(RD_KAFKA_PRODUCER, kconfP, kerr, sizeof(kerr));
    if (utxx::unlikely(m_kafkaP == nullptr))
      throw utxx::runtime_error
            ("EConnector_LATOKEN_STP::Ctor: Cannot init Kafka Producer: ",
             kerr);

    //-----------------------------------------------------------------------//
    // Construct "InstrRisks"- and "AssetRisks"-related Producer Topics:     //
    //-----------------------------------------------------------------------//
    for (int i = 0; i < 2; ++i)
    {
      char const* const (&tNames)[NProdTopics] =
        IsProdEnv()
        ? ((i == 0)
            ? s_kafkaProducerInstrTopicsProd
            : s_kafkaProducerAssetTopicsProd)
        : ((i == 0)
            ? s_kafkaProducerInstrTopicsTest
            : s_kafkaProducerAssetTopicsTest);

      vector<rd_kafka_topic_t*>& topics        =
        (i == 0) ? m_instrTopics            : m_assetTopics;

      for (char const* tName: tNames)
      {
        // Topic config will replicate the main Producer config:
        rd_kafka_topic_conf_t* tConf =
          rd_kafka_default_topic_conf_dup(m_kafkaP);
        assert(tConf != nullptr);

        // Then create and memoise the Topic itself:
        rd_kafka_topic_t* topic = rd_kafka_topic_new(m_kafkaP, tName, tConf);
        assert(topic != nullptr);

        topics.push_back (topic);

        // XXX: Is it safe now to destroy the "tConf"? The docs are obscure on
        // that. We opt NOT to do so, thus risking a tiny memory leak...
      }
    }
    //-----------------------------------------------------------------------//
    // Direct the Kafka Log Msgs into the Main Log:                          //
    //-----------------------------------------------------------------------//
    // XXX: Should we use "kconfC" or "kconfP" here?
    rd_kafka_conf_set_log_cb(kconfC, KafkaLogger);

    //=======================================================================//
    // Helpers:                                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Map AssetNames to AssetIDs (used in output msg generation):           //
    //-----------------------------------------------------------------------//
    // Repeated AssetNames are installed only once (the latest one):
    //
    for (auto const& quadr: LATOKEN::Assets)
    {
      unsigned    ccyID     = get<0>(quadr);
      char const* assetName = get<2>(quadr);
      double      feeRate   = get<3>(quadr);

      // Try to insert it into both Maps:
      auto insRes1 = m_ccyIDs.insert  (make_pair(assetName, ccyID));
      if (utxx::unlikely(!insRes1.second))
        LOG_WARN(2,
          "EConnector_LATOKEN_STP: Repeated Asset Name: {}", assetName)

      auto insRes2 = m_ccyNames.insert(make_pair(ccyID, assetName));
      if (utxx::unlikely(!insRes2.second))
        LOG_WARN(2,
          "EConnector_LATOKEN_STP: Repeated CcyID: {}",      ccyID)

      // Install the FeeRate as well:
      (void) m_feeRates.insert        (make_pair(assetName, feeRate));

      // Also, create MtM Msg Rate Throttler for this Asset (for Instrs, they
      // were already created above):
      m_assetThrots.insert
        (make_pair(assetName, Throttler(MtMThrotWinSec)));
    }
    // All Done!
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  EConnector_LATOKEN_STP::~EConnector_LATOKEN_STP()
  {
    try
    {
      //---------------------------------------------------------------------//
      // Stop and Destroy the MDCs:                                          //
      //---------------------------------------------------------------------//
      if (m_mdcBinance != nullptr)
      {
        m_mdcBinance->Stop();
        delete m_mdcBinance;
      }
      m_allMDCs.clear();

      //---------------------------------------------------------------------//
      // Stop the Connector if by any chance it is still running:            //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(m_isActive))
        Stop();
      assert(!m_isActive);

      //---------------------------------------------------------------------//
      // Kafka Consumer Queue:                                               //
      //---------------------------------------------------------------------//
      if (m_kq != nullptr)
      {
        rd_kafka_queue_destroy(m_kq);
        m_kq = nullptr;
      }

      //---------------------------------------------------------------------//
      // Kafka Consumer:                                                     //
      //---------------------------------------------------------------------//
      if (m_kafkaC != nullptr)
      {
        // IMPORTANT: Commit the last consumed offsets (in all Topics/Part0)
        // SYNCHRONOUSLY:
        if (m_kpartsC != nullptr)
        {
          // Get the NEXT offsets (after the last consumed msgs):
          auto rc = rd_kafka_position(m_kafkaC, m_kpartsC);

          if (utxx::unlikely(rc != 0))
          {
            LOG_ERROR(1,
              "EConnector_LATOKEN_STP::Dtor: Cannot get the Consumed Kafka "
              "Offsets: {}", rd_kafka_err2str(rc))
          }
          else
          {
            // Get the offsets back to the last consumed ones:
            for (int i = 0; i < m_kpartsC->cnt; ++i)
            {
              rd_kafka_topic_partition_t* part = m_kpartsC->elems + i;
              if (part->offset != RD_KAFKA_OFFSET_INVALID)
              {
                --(part->offset);
                LOG_INFO(2,
                  "EConnector_LATOKEN_STP::Dtor: {}::{}: Committing Offset={}",
                  part->topic, part->partition, part->offset)
              }
            }
            // Now really commit them (ASync=false!):
            rc = rd_kafka_commit(m_kafkaC, m_kpartsC, 0);  // ASync=false

            if (utxx::unlikely(rc != 0))
              LOG_ERROR(1,
                "EConnector_LATOKEN_STP::Dtor: Cannot Commit Kafka Offsets: "
                "{}", rd_kafka_err2str(rc))
          }
          // We can now destroy the Topic/Parts List:
          rd_kafka_topic_partition_list_destroy(m_kpartsC);
          m_kpartsC = nullptr;
        }
        // For extra safety, close the Consumer explicitly:
        auto rc = rd_kafka_consumer_close(m_kafkaC);
        if (utxx::unlikely(rc != 0))
          LOG_ERROR(1,
            "EConnector_LATOKEN_STP::Dtor: Cannot Close the Kafka Consumer: "
            "{}", rd_kafka_err2str(rc))

        // Now really destroy the Consumer:
        rd_kafka_destroy(m_kafkaC);
        m_kafkaC = nullptr;
      }
      //---------------------------------------------------------------------//
      // Kafka Producer:                                                     //
      //---------------------------------------------------------------------//
      if (m_kafkaP != nullptr)
      {
        // Flush all msgs currently being sent, with a 5sec time-out. XXX:
        // This cal is blocking, but it is OK for a Dtor:
        auto rc  = rd_kafka_flush(m_kafkaP, 5000);
        if (utxx::unlikely (rc != 0))
          LOG_ERROR(1,
            "EConnector_LATOKEN_STP::Dtor: Cannot Flush Kafka Msgs: {}",
            rd_kafka_err2str(rc))

        // Now really destroy the Producer:
        rd_kafka_destroy(m_kafkaP);
        m_kafkaP = nullptr;
      }
      //---------------------------------------------------------------------//
      // Kafka Producer Topics (InstrRisks and AssetRisks-related):          //
      //---------------------------------------------------------------------//
      for (rd_kafka_topic_t* topic: m_instrTopics)
        if (topic != nullptr)
          rd_kafka_topic_destroy(topic);
      m_instrTopics.clear();

      for (rd_kafka_topic_t* topic: m_assetTopics)
        if (topic != nullptr)
          rd_kafka_topic_destroy(topic);
      m_assetTopics.clear();

      //---------------------------------------------------------------------//
      // Close the FDs:                                                      //
      //---------------------------------------------------------------------//
      if (m_kfd != -1)
      {
        (void) close(m_kfd);
        m_kfd = -1;
      }
      if (m_rfd != -1)
      {
        // Detach it from the Reactor and close:
        if (m_reactor != nullptr)
          m_reactor->Remove(m_rfd);
        (void) close(m_rfd);
        m_rfd = -1;
      }
      //---------------------------------------------------------------------//
      // Also clear the Singleton Ptr:                                       //
      //---------------------------------------------------------------------//
      s_Singleton = nullptr;
    }
    // Do not allow any exceptions to propagate from the Dtor:
    catch (...) {}
  }

  //=========================================================================//
  // Starting / Initialisation Methods:                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Start":                                                                //
  //-------------------------------------------------------------------------//
  void EConnector_LATOKEN_STP::Start()
  {
    if (utxx::unlikely(m_isActive))
    {
      LOG_WARN(2, "EConnector_LATOKEN_STP::Start: Already Active")
      return;
    }
    // Then in MUST NOT be in DynInitMode either:
    assert(!m_stpDynInitMode);

    //-----------------------------------------------------------------------//
    // Generic Case: Subscribe to InstrRisks and AssetRisks Consumer Topics: //
    //-----------------------------------------------------------------------//
    // NB: If "m_topicN" is NOT in 0..(NConsTopics-1), then all "NConsTopics"
    // are subscribed; otherwise, only the corresp Topic is subscribed (this
    // is for testing only):
    bool allTopics = (m_topicN < 0) || (m_topicN >= NConsTopics);

    // Subscribe to both Instrument (Trade) and Asset (Balance)-related Topics:
    if (m_kpartsC == nullptr)
    {
      m_kpartsC =
        rd_kafka_topic_partition_list_new(allTopics ? (2 * NConsTopics) : 2);

      // Construct the full list of Topics:
      for (int i = 0; i < 2; ++i)
      {
        char const* const (&tNames)[NConsTopics] =
          IsProdEnv()
          ? ((i == 0)
              ? s_kafkaConsumerInstrTopicsProd
              : s_kafkaConsumerAssetTopicsProd)
          : ((i == 0)
              ? s_kafkaConsumerInstrTopicsTest
              : s_kafkaConsumerAssetTopicsTest);

        for (int j = 0; j < NConsTopics; ++j)
          if (allTopics || j == m_topicN)
          {
            DEBUG_ONLY(rd_kafka_topic_partition_t* topic =)
              // NB: PartID==0 as we have just 1 Partition per Topic:
              rd_kafka_topic_partition_list_add(m_kpartsC, tNames[j], 0);
            assert(topic != nullptr);
          }
      }
    }
    // Now actually subscribe to all Topics at once:
    auto rc  = rd_kafka_subscribe(m_kafkaC, m_kpartsC);

    if (utxx::unlikely (rc != 0))
    {
      rd_kafka_topic_partition_list_destroy(m_kpartsC);
      throw utxx::runtime_error
            ("EConnector_LATOKEN_STP::Start: Cannot subscribe to Kafka topics"
             ": ", rd_kafka_err2str(rc));
    }
    LOG_INFO(1,
      "EConnector_LATOKEN_STP: Subscribed to Kafka Topics, and is now ACTIVE")

    //-----------------------------------------------------------------------//
    // Finally, set the Flags:                                               //
    //-----------------------------------------------------------------------//
    // NB: We are now in DynInitMode (until all backlog of Kafka msgs is read
    // in):
    m_isActive       = true;
    m_stpDynInitMode = true;

    // In DynInitMode, disable MktData Updates of Risks, as the Trades being
    // read from Kafka backlog are behind the MktData!  (NB: MktData Updates
    // should not be coming anyway yet, as we do not start MDCs in  DynInit-
    // Mode, but disable them for safety still):
    //
    m_riskMgr->DisableMktDataUpdates();
  }

  //=========================================================================//
  // Stopping Methods:                                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Stop":                                                                 //
  //-------------------------------------------------------------------------//
  void EConnector_LATOKEN_STP::Stop()
  {
    //-----------------------------------------------------------------------//
    // UnSubsctibe from Kafka Topics, but do not destroy any Kafka objs yet: //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    ( if (utxx::unlikely(!m_isActive))
      {
        LOG_WARN(1, "EConnector_LATOKEN_STP::Stop: Already Inactive");
        return;
    } )
    // Generic Case:
    auto rc  = rd_kafka_unsubscribe(m_kafkaC);
    if (utxx::unlikely (rc != 0))
      // This is a weird error condition, XXX: we can only log it:
      LOG_ERROR(1,
        "EConnector_LATOKEN_STP::Stop: Cannot UnSubscribe Kafka Topics: {}",
        rd_kafka_err2str(rc))

    //-----------------------------------------------------------------------//
    // Stop the MDCs:                                                        //
    //-----------------------------------------------------------------------//
    // (No harm if they are not running):
    //
    for (EConnector_MktData* mdc: m_allMDCs)
    {
      assert(mdc != nullptr);
      mdc->Stop();
    }
    //-----------------------------------------------------------------------//
    // Finally, clear the flags in any case:                                 //
    //-----------------------------------------------------------------------//
    m_isActive       = false;
    m_stpDynInitMode = false;
  }
} // End namespace MAQUETTE
