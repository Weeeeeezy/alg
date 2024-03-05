// vim:ts=2:et
//===========================================================================//
//                          "Tools/ManualTrading.cpp":                       //
//===========================================================================//
#include "Basis/Macros.h"
#include "Basis/ConfigUtils.hpp"
#include "Basis/Maths.hpp"
#include "Basis/IOUtils.h"
#if (!CRYPTO_ONLY)
#include "Connectors/FAST/EConnector_FAST_MICEX.hpp"
#include "Connectors/FAST/EConnector_FAST_FORTS.h"
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"
#include "Connectors/ITCH/EConnector_ITCH_HotSpotFX.h"
#include "Connectors/FIX/EConnector_FIX.h"
#endif
#if WITH_RAPIDJSON
#include "Connectors/H2WS/LATOKEN/EConnector_WS_LATOKEN_MDC.h"
#endif
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_MDC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Connectors/H2WS/BitMEX/EConnector_WS_BitMEX_MDC.h"
#include "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.h"
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_MDC.h"
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.h"
#include "Connectors/H2WS/Huobi/EConnector_WS_Huobi_MDC.h"
#include "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_OMC.h"

#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include "InfraStruct/Strategy.hpp"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <spdlog/async.h>
#include <utxx/compiler_hints.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/container/static_vector.hpp>
#include <cstdlib>
#include <cstdio>

# ifdef  __GNUC__
# pragma GCC   diagnostic push
# pragma GCC   diagnostic ignored "-Wunused-parameter"
# endif
# ifdef  __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-parameter"
# endif

using namespace MAQUETTE;
using namespace std;

namespace
{
  //=========================================================================//
  // "ManualTradingStrat" Class:                                             //
  //=========================================================================//
  class ManualTradingStrat final: public Strategy
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // NB: "m_instrs" include all configured Instrs:
    // (*) the FRONT one is the Traded Instrument;
    // (*) all others may or may not be used for Ccy->USD conversions in the
    //     RiskMgr:
    //
    constexpr static QtyTypeT QTU = QtyTypeT::UNDEFINED;

    enum class PxT
    {
      UNDEFINED      = 0,
      None           = 1, // Do not trade at all, just get MktData
      PeggedPassive  = 2,
      Passive        = 3,
      Aggressive     = 4,
      DeepAggressive = 5,
      Market         = 6,
      Number         = 7
    };

    // Setup:
    SecDefsMgr*               m_secDefsMgr;
    RiskMgr*                  m_riskMgr;
    EConnector_MktData*       m_mdc1;         // Primary
    EConnector_MktData*       m_mdc2;         // Secondary MDC (may be NULL)
    EConnector_OrdMgmt*       m_omc;
    vector<string>            m_instrNames;
    vector<SecDefD   const*>  m_instrs;       // Ptrs NOT owned -- from MDC
    vector<OrderBook const*>  m_orderBooks;   // Ditto

    // Status of Connectors:
    long                      m_updatesCount;
    int                       m_signalsCount;

    // Order-Related Params:
    boost::container::static_vector<AOS const*, 8>
                              m_aoses;        // Our Order(s) (normally just 1)
    PxT                       m_pxT;
    PriceT                    m_px;           // If PxT is a NUMBER
    double                    m_sQty;         // Signed Qty
    FIX::TimeInForceT         m_timeInForce;
    long                      m_delay;        // In MktData counts
    bool                      m_testBufferedOrders;
    bool                      m_testPipeLining;

    // MktData:
    string                    m_mktDataFile;  // For Recording
    FILE*                     m_MDFD;         // As above, when opened...
    PriceT                    m_prevBidPx;
    PriceT                    m_prevAskPx;

    // Default Ctor is deleted:
    ManualTradingStrat() =  delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    ManualTradingStrat
    (
      EPollReactor*                a_reactor,
      spdlog::logger*              a_logger,
      boost::property_tree::ptree& a_pt     // XXX: Mutable!
    )
    :
      //---------------------------------------------------------------------//
      // Parent Class:                                                       //
      //---------------------------------------------------------------------//
      Strategy
      (
        "ManualTrading",
        a_reactor,
        a_logger,
        a_pt.get<int>("Exec.DebugLevel"),
        { SIGINT }
      ),
      //---------------------------------------------------------------------//
      // SecDefsMgr and RiskMgr:                                             //
      //---------------------------------------------------------------------//
      m_secDefsMgr        (SecDefsMgr::GetPersistInstance
        (a_pt.get<string>("Main.Env") == "Prod", "")),

      m_riskMgr
      (
        utxx::likely(a_pt.get<bool>("RiskMgr.IsEnabled"))
        ? RiskMgr::GetPersistInstance
          (
            a_pt.get<string>("Main.Env") == "Prod",
            GetParamsSubTree(a_pt, "RiskMgr"),
            *m_secDefsMgr,
            false,          // NOT Observer!
            m_logger,
            a_pt.get<int>   ("Exec.DebugLevel")
          )
        : nullptr
      ),
      //---------------------------------------------------------------------//
      // Other Flds:                                                         //
      //---------------------------------------------------------------------//
      m_mdc1              (nullptr),
      m_mdc2              (nullptr),
      m_omc               (nullptr),
      m_instrNames        (),
      m_instrs            (),
      // Status of Connectors:
      m_updatesCount      (0),
      m_signalsCount      (0),
      // Order-Related Params:
      m_aoses             (),
      m_pxT               (PxT::UNDEFINED),
      m_px                (),       // NaN
      m_sQty              (0),
      m_timeInForce       (FIX::TimeInForceT::UNDEFINED),
      m_delay             (a_pt.get<long>  ("Main.DelayTicks")),
      m_testBufferedOrders(a_pt.get<bool>  ("OMC.TestBufferedOrders", false)),
      m_testPipeLining    (a_pt.get<bool>  ("OMC.TestPipeLining",     false)),
      //
      // MktData:
      m_mktDataFile       (a_pt.get<string>("Main.MktDataFile",       "")),
      m_MDFD              (m_mktDataFile.empty()
                          ? nullptr
                          : fopen(m_mktDataFile.data(), "a")),
      m_prevBidPx         (),       // NaN
      m_prevAskPx         ()        // NaN
    {
      //---------------------------------------------------------------------//
      // Memoise the Order Params:                                           //
      //---------------------------------------------------------------------//
      // Check the Recording:
      if (utxx::unlikely(!m_mktDataFile.empty() && m_MDFD == nullptr))
        throw utxx::badarg_error
              ("ManualTrading::Ctor: Could not open the MktData Output File: ",
               m_mktDataFile);
      // Price:
      string pxStr = a_pt.get<string>("Main.Px");
      m_pxT =
        (pxStr == "None")
        ? PxT::None           :
        (pxStr == "PeggedPassive")
        ? PxT::PeggedPassive  :
        (pxStr == "Passive")
        ? PxT::Passive        :
        (pxStr == "Aggressive")
        ? PxT::Aggressive     :
        (pxStr == "DeepAggressive")
        ? PxT::DeepAggressive :
        (pxStr == "Market")
        ? PxT::Market
          // Any other: consider it to be a Number:
        : PxT::Number;

      // Depending on the "PxT", we may or may not need the MDC and/or OMC:
      //
      // XXX: For PxT::Number, we still create the MDC as it will be used  for
      // monitiring the likelyhood of successful order execution. However, for
      // Market and PeggedPassive types, we can do without the MDC:
      bool withMDC = (m_pxT != PxT::Market && m_pxT != PxT::PeggedPassive);

      // And OMC is required in all cases except PxT::None -- the latter speci-
      // fically means that we only receive MktData and do not place any Orders:
      bool withOMC = (m_pxT != PxT::None);

      if (m_pxT == PxT::Number)
      {
        // Then get that number (otherwise, "m_px" remains 0):
        m_px = PriceT(atof(pxStr.data()));
        // In this particular case we require that the px must be positive:
        if (utxx::unlikely(double(m_px) <= 0.0))
          throw utxx::badarg_error
                ("ManualTradingStrat::Ctor: Invalid Px=", pxStr);
      }
      else
      if (utxx::unlikely(m_testPipeLining))
        // XXX: Currently, TestPipeLining is only possible with a specific Px
        // (and the Caller assumes full responsibility for the consequences;
        // in live environments, it is recommended to select that Px far away
        // from L1):
        throw utxx::badarg_error
              ("ManualTradingStrat::Ctor: TestPipeLining requires a specific "
               "Px");

      // Quantity (signed, to indicate Buy or Sell):
      m_sQty = a_pt.get<double>("Main.Qty");
      if (utxx::unlikely(m_sQty == 0))
        throw utxx::badarg_error("ManualTradingStrat::Ctor: Invalid Qty=0");

      // TimeInForce: Currently, only "Day", "IOC" and "GTC" are supported:
      string tifStr = a_pt.get<string>("Main.TimeInForce");
      m_timeInForce =
        (tifStr == "Day")
        ? FIX::TimeInForceT::Day :
        (tifStr == "IOC")
        ? FIX::TimeInForceT::ImmedOrCancel  :
        (tifStr == "GTC")
        ? FIX::TimeInForceT::GoodTillCancel :
          throw utxx::badarg_error
                ("ManualTradingStrat::Ctor: Invalid TimeInForce=", tifStr);

      //---------------------------------------------------------------------//
      // Create the Connectors (MDC and OMC):                                //
      //---------------------------------------------------------------------//
      string exchange       = a_pt.get<string>("Main.Exchange");
      string mdcProtocol    = a_pt.get<string>("Main.MDC_Protocol", "FAST");
      string omcProtocol    = a_pt.get<string>("Main.OMC_Protocol", "FIX" );
      string assetClass     = a_pt.get<string>("Main.AssetClass");
      string env            = a_pt.get<string>("Main.Env");
      string instrName      = a_pt.get<string>("Main.InstrName");

      auto const* mdcParams = GetParamsOptTree(a_pt, "MDC");
      auto const* omcParams = GetParamsOptTree(a_pt, "OMC");
#     if (!CRYPTO_ONLY)
      bool        isProd    = (env.substr(0, 4)  == "Prod");
#     endif
      withMDC &= (mdcParams != nullptr);
      withOMC &= (omcParams != nullptr);

      //---------------------------------------------------------------------//
      // Get the Symbols:                                                    //
      //---------------------------------------------------------------------//
      // The Full Designation of the Main Traded Instrument:
      m_instrNames.push_back(instrName);

      // In FAST, we are interested in the symbols used for Ccy conversion in
      // RiskMgr (if any), and obviously the Main Traded Symbol:
      if (m_riskMgr != nullptr)
      {
        vector<string> convInstrNames;
        string convStr = a_pt.get<string>("RiskMgr.Converters");
        boost::split  (convInstrNames, convStr, boost::is_any_of(";,"));

        for (string const & convInstrName: convInstrNames)
          if (utxx::likely(!convInstrName.empty()))
            // "ConvInstrName is also to be stored in "m_instrNames":
            m_instrNames.push_back(convInstrName);
      }
#     if (!CRYPTO_ONLY)
      //---------------------------------------------------------------------//
      if (exchange == "MICEX")
      //---------------------------------------------------------------------//
      {
        // NB: In Prod, we create BOTH Primary and Secondary MDCs:
        using namespace FAST::MICEX;
        // Environment:
        EnvT env1 = isProd ? EnvT::Prod1 : EnvT::TestL;
        EnvT env2 = isProd ? EnvT::Prod2 : EnvT::TestL;

        // Create the FAST MDC (depends on the AssetClass):
        if (withMDC)
        {
          if (assetClass == "FX")
          {
            m_mdc1   = new EConnector_FAST_MICEX_FX
                           (env1, m_reactor, m_secDefsMgr, &m_instrNames,
                            m_riskMgr, *mdcParams, nullptr);
            if (isProd)
              m_mdc2 = new EConnector_FAST_MICEX_FX
                           (env2, m_reactor, m_secDefsMgr, &m_instrNames,
                            m_riskMgr, *mdcParams, m_mdc1);
          }
          else
          if (assetClass == "EQ")
          {
            m_mdc1   = new EConnector_FAST_MICEX_EQ
                           (env1, m_reactor, m_secDefsMgr, &m_instrNames,
                            m_riskMgr, *mdcParams, nullptr);
            if (isProd)
              m_mdc2 = new EConnector_FAST_MICEX_EQ
                           (env2, m_reactor, m_secDefsMgr, &m_instrNames,
                            m_riskMgr, *mdcParams, m_mdc1);
          }
          else
            throw utxx::badarg_error
                  ("ManualTradingStrat::Ctor: Invalid MICEX AssetClass=",
                   assetClass);
        }
        // The MICEX FIX Connector type does not depend on the AssetClass
        // (though its internal config does):
        if (withOMC)
        {
          string omcEnv     = isProd ? "-Prod20" : "-TestL1";
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-FIX-MICEX-" + assetClass + omcEnv;
          SetAccountKey(omcParams, omcAcctKey);

          m_omc  = new EConnector_FIX_MICEX
                       (m_reactor, m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams, nullptr);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "FORTS")
      //---------------------------------------------------------------------//
      {
        using namespace FAST::FORTS;
        // Environment:
        EnvT envT = isProd ? EnvT::ProdF : EnvT::TestL;

        // Create the FAST MDC (depends on the Version and AssetClass):
        // XXX : At the moment, only the primary MDC (FAST) is created;
        // TODO: Secondary (P2CGate) MDCs:
        if (withMDC)
        {
          // FORTS FAST Protocl Version is implied by Env:
          ProtoVerT::type ver = ImpliedProtoVerT(envT);

          if (utxx::likely(mdcProtocol == "FAST" && ver == ProtoVerT::Curr))
            m_mdc1 =  new EConnector_FAST_FORTS_FOL
                          (envT,      m_reactor,  m_secDefsMgr, &m_instrNames,
                           m_riskMgr, *mdcParams, nullptr);

          if (utxx::unlikely(m_mdc1 == nullptr))
            throw utxx::badarg_error
                  ("ManualTradingStrat::Ctor: Invalid FAST FORTS Config: "
                   "ProtoVer=", ProtoVerT::to_string(ver), ", AssetClass=",
                  assetClass,  ", MDC_Protocol=",         mdcProtocol);
        }
        // For the FORTS OMC, use FIX or TWIME. In both cases, the FIX OMC
        // Connector does not depend on the AssetClass at all  (neither by
        // type, nor via the internal config):
        if (withOMC)
        {
          string acctPrefix = a_pt.get<string>("OMC.AccountPfx");

          if (omcProtocol == "FIX")
          {
            string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                                "-FIX-FORTS-" + env;
            SetAccountKey(omcParams, omcAcctKey);

            m_omc = new EConnector_FIX_FORTS
                        (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                         *omcParams, nullptr);
          }
          else
          if (omcProtocol == "TWIME")
          {
            string omcAcctKey =
              a_pt.get<string>("OMC.AccountPfx") +
              "-TWIME-FORTS-" + (isProd ? "Prod" : "TestL");
            SetAccountKey(omcParams, omcAcctKey);

            m_omc = new EConnector_TWIME_FORTS
                        (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                         *omcParams);
          }
          else
            throw utxx::badarg_error
                ("ManualTradingStrat::Ctor: Invalid FORTS OMC Protocol: ",
                 omcProtocol);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "AlfaFIX2")
      //---------------------------------------------------------------------//
      {
        // XXX: There is no Test Env for AlfaFIX2 currently:
        if (utxx::unlikely(!isProd))
          throw utxx::badarg_error("AlfaFIX2: Test Env is not supported");

        // Here both MDC and OMC are FIX Connectors. The Env must correspond to
        // the Prefix:
        if (withMDC)
        {
          string mdcAcctKey =
            a_pt.get<string>("MDC.AccountPfx") +
            "-FIX-AlfaFIX2-" + (isProd ? "ProdMKT" : "TestMKT");
          SetAccountKey(mdcParams,  mdcAcctKey);

          // NB: AlfaFIX2 Connector does not require any Symbols Filtering --
          // OrderBooks are created for explicitly-subscribe Symbols only
          // (because the underlying protocol is FIX, not FAST MultiCast):
          m_mdc1 = new EConnector_FIX_AlfaFIX2
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey =
            a_pt.get<string>("OMC.AccountPfx") + "-FIX-AlfaFIX2-" +
            (isProd ? "ProdORD" : "TestORD");
          SetAccountKey(omcParams,  omcAcctKey);

          m_omc  = new EConnector_FIX_AlfaFIX2
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams, nullptr);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "FXBA")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
        {
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-FIX-FXBA-" + env + "MKT";
          SetAccountKey(mdcParams,  mdcAcctKey);

          m_mdc1 = new EConnector_FIX_FXBA
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-FIX-FXBA-" + env + "ORD";
          SetAccountKey(omcParams,  omcAcctKey);

          m_omc  = new EConnector_FIX_FXBA
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams, nullptr);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "AlfaECN")
      //---------------------------------------------------------------------//
      {
        // This is similar to the AlfaFIX2 and FXBA cases above:
        if (withMDC)
        {
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-FIX-AlfaECN-" + env + "1MKT";
          SetAccountKey(mdcParams,  mdcAcctKey);

          m_mdc1 = new EConnector_FIX_AlfaECN
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-FIX-AlfaECN-" + env + "1ORD";
          SetAccountKey(omcParams,  omcAcctKey);

          m_omc  = new EConnector_FIX_AlfaECN
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams, nullptr);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "NTProgress")
      //---------------------------------------------------------------------//
      {
        // This is similar to AlfaFIX2, FXBA and AlfaECN cases above:
        if (withMDC)
        {
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-FIX-NTProgress-" + env + "MKT";
          SetAccountKey(mdcParams,  mdcAcctKey);

          m_mdc1 = new EConnector_FIX_NTProgress
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-FIX-NTProgress-" + env + "ORD";
          SetAccountKey(omcParams,  omcAcctKey);

          m_omc  = new EConnector_FIX_NTProgress
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams, nullptr);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "HotSpotFX_Gen")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
        {
          string protocol   = a_pt.get<string>("MDC.Protocol", "");
          if (protocol == "ITCH")
          {
            // Using ITCH (over TCP) or for MktData:
            string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                                "-TCP_ITCH-HotSpotFX-" + env;
            SetAccountKey(mdcParams,  mdcAcctKey);

            m_mdc1 = new EConnector_ITCH_HotSpotFX_Gen
                         (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *mdcParams, nullptr);
          }
          else
          if (protocol == "FIX")
          {
            // Using FIX for MktData:
            string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                                "-FIX-HotSpotFX-" + env;
            SetAccountKey(mdcParams,  mdcAcctKey);

            m_mdc1 = new EConnector_FIX_HotSpotFX_Gen
                         (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *mdcParams, nullptr);
          }
          else
            throw utxx::badarg_error
                  ("HotSpotFX_Gen MDC: Invalid Protocol=", protocol);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-FIX-HotSpotFX-" + env;
          SetAccountKey(omcParams,    omcAcctKey);

          m_omc  =   new EConnector_FIX_HotSpotFX_Gen
                         (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *omcParams, nullptr);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "HotSpotFX_Link")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
        {
          // Using ITCH (over TCP) for MktData:
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-TCP_ITCH-HotSpotFX-" + env;
          SetAccountKey(mdcParams,  mdcAcctKey);

          m_mdc1 = new EConnector_ITCH_HotSpotFX_Link
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-FIX-HotSpotFX-"    + env;
          SetAccountKey(omcParams,  omcAcctKey);

          m_omc  = new EConnector_FIX_HotSpotFX_Link
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams, nullptr);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "Currenex")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
        {
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-NY6_INet-MKT-FIX-Currenex-" + env;
          SetAccountKey(mdcParams,  mdcAcctKey);

          m_mdc1 = new EConnector_FIX_Currenex
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-NY6_INet-MKT-FIX-Currenex-" + env;
          SetAccountKey(omcParams, omcAcctKey);

          m_omc  = new EConnector_FIX_Currenex
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams, nullptr);
        }
      }
      else
#     if WITH_RAPIDJSON
      //---------------------------------------------------------------------//
      if (exchange == "LATOKEN")
      //---------------------------------------------------------------------//
      {
        // XXX: LATOKEN support is currently incomplete, so it is also de-conf-
        // igured in the Crypto-Only mode. LATOKEN STP Connector is NOT created
        // here, as it is neither MDC nor OMC:
        //
        if (withMDC)
          // XXX: No "mdcAcctKey" here?
          m_mdc1 = new EConnector_WS_LATOKEN_MDC
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        // TODO: OMC!
      }
      else
#     endif
#     endif // !CRYPTO_ONLY
      //---------------------------------------------------------------------//
      if (exchange == "Binance")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
        {
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-H2WS-Binance-MDC-" + assetClass + "-" + env;
          SetAccountKey(mdcParams, mdcAcctKey);

          // NB: No special restrictions of "SecDefS"s to be considered (NULL):
          // We will anyway use just 1 Instrument configured by SettlDate (even
          // if it is set to 0):
          if (assetClass == "Spt")
            m_mdc1 = new EConnector_H2WS_Binance_MDC_Spt
                         (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *mdcParams, nullptr);
          else
          if (assetClass == "FutT")
            m_mdc1 = new EConnector_H2WS_Binance_MDC_FutT
                         (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *mdcParams, nullptr);
          else
          if (assetClass == "FutC")
            m_mdc1 = new EConnector_H2WS_Binance_MDC_FutC
                         (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *mdcParams, nullptr);
          else
            throw utxx::badarg_error
                  ("ManualTradingStrat::Ctor: Invalid MICEX AssetClass=",
                   assetClass);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-H2WS-Binance-OMC-" + assetClass + "-" + env;
          SetAccountKey(omcParams, omcAcctKey);

          if (assetClass == "Spt")
            m_omc  = new EConnector_H2WS_Binance_OMC_Spt
                         (m_reactor, m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *omcParams);
          else
          if (assetClass == "FutT")
            m_omc  = new EConnector_H2WS_Binance_OMC_FutT
                         (m_reactor, m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *omcParams);
          else
          if (assetClass == "FutC")
            m_omc  = new EConnector_H2WS_Binance_OMC_FutC
                         (m_reactor, m_secDefsMgr, &m_instrNames, m_riskMgr,
                          *omcParams);
          else
            throw utxx::badarg_error
                    ("ManualTradingStrat::Ctor: Invalid MICEX AssetClass=",
                     assetClass);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "BitMEX")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
        {
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-WS-BitMEX-MDC-" + env;
          SetAccountKey(mdcParams,  mdcAcctKey);

          // NB: No special restrictions of "SecDefS"s to be considered (NULL):
          // We will anyway use just 1 Instrument configured by SettlDate (even
          // if it is set to 0):
          m_mdc1 = new EConnector_WS_BitMEX_MDC
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-H2WS-BitMEX-OMC-" + env;
          SetAccountKey(omcParams,  omcAcctKey);

          m_omc  = new EConnector_H2WS_BitMEX_OMC
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams);
        }
      }
      else
      //---------------------------------------------------------------------//
      if (exchange == "BitFinex")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
        {
          string mdcAcctKey = a_pt.get<string>("MDC.AccountPfx") +
                              "-WS-BitFinex-MDC-" + env;
          SetAccountKey(mdcParams,  mdcAcctKey);

          m_mdc1 = new EConnector_WS_BitFinex_MDC
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *mdcParams, nullptr);
        }
        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-WS-BitFinex-OMC-" + env;
          SetAccountKey(omcParams,  omcAcctKey);
          m_omc  = new EConnector_WS_BitFinex_OMC
                       (m_reactor,  m_secDefsMgr, &m_instrNames, m_riskMgr,
                        *omcParams);
        }
      }
      /*
      else
      //---------------------------------------------------------------------//
      if (exchange == "Huobi")
      //---------------------------------------------------------------------//
      {
        if (withMDC)
          // XXX: No "mdcAccountKey" here???
          m_mdc1 = new EConnector_H2WS_Huobi_MDC_Spt
                       (m_reactor,  m_secDefsMgr, nullptr, m_riskMgr,
                        *mdcParams, nullptr);

        if (withOMC)
        {
          string omcAcctKey = a_pt.get<string>("OMC.AccountPfx") +
                              "-H2WS-Huobi-OMC-" + env;
          SetAccountKey(omcParams, omcAcctKey);

          m_omc = new EConnector_H2WS_Huobi_OMC_Spt
                      (m_reactor, m_secDefsMgr, nullptr, m_riskMgr,
                       *omcParams);
        }
      }
      */
      //---------------------------------------------------------------------//
      else
      //---------------------------------------------------------------------//
        throw utxx::badarg_error
              ("ManualTradingStrat::Ctor: UnSupported Exchange=", exchange,
               " and/or AssetClass=", assetClass);

      //---------------------------------------------------------------------//
      // After the Connector has been created, get all SecDefs:              //
      //---------------------------------------------------------------------//
      for (string const& in: m_instrNames)
      {
        // XXX: Depending on the Connector, SecDef is retrieved either using
        // Segment or using SettlDate; here we use the uniform Segment-based
        // version of "FindSecDef" which will always work (but is less effic-
        // ient if SettlDates are actually used):
        //
        SecDefD const& instr = m_secDefsMgr->FindSecDef(in.data());

        // For extra safety, check for duplicate Instrs:
        if (utxx::likely(find(m_instrs.cbegin(), m_instrs.cend(), &instr)
                         == m_instrs.cend()))
            // OK, install a ptr to this SecDef:
            m_instrs.push_back(&instr);

        // After the start of MDC, we will subscribe to these Instruments and
        // wait for their OrderBooks to become valid!
      }

      //---------------------------------------------------------------------//
      // Subscribe to receive notifications from both MDC & OMC Connectrors: //
      //---------------------------------------------------------------------//
      // (This should be done immediately upon the Connectors construction):
      if (withMDC)
      {
        // XXX: We only subscribe to events from the Primary MDC -- those from
        // the Secondary may be more confusing than useful:
        assert(m_mdc1 != nullptr);
        m_mdc1->Subscribe(this);
      }
      if (withOMC)
      {
        assert(m_omc != nullptr);
        m_omc->Subscribe(this);
      }

      // If we have both MDC and OMC, link them together,   so that OMC will
      // receive updates on our own orders coming from MDC. For this reason,
      // only the Primary MDC is to be used here:
      if (withOMC && withMDC)
        m_omc->LinkMDC(m_mdc1);

      // But note that the actual MktData subscription is delayed until   the
      // MktData connector is activated (this does not matter much for a FAST
      // MDC, but important for a FIX MDC because in the latter case, the ac-
      // tual subscription msg needs to be sent out!)
    }

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~ManualTradingStrat() noexcept override
    {
      try
      {
        //-------------------------------------------------------------------//
        // Stop and Remove the Connectors:                                   //
        //-------------------------------------------------------------------//
        if (m_mdc1 != nullptr)
        {
          m_mdc1->UnSubscribe(this);
          m_mdc1->Stop();
          delete m_mdc1;
          m_mdc1 = nullptr;
        }
        if (m_mdc2 != nullptr)
        {
          // No need to UnSubscribe here -- “m_mdc2” was not Subscribed for!
          m_mdc2->Stop();
          delete m_mdc2;
          m_mdc2 = nullptr;
        }
        if (m_omc != nullptr)
        {
          m_omc->UnSubscribe(this);
          m_omc->Stop();
          delete m_omc;
          m_omc = nullptr;
        }
        //-------------------------------------------------------------------//
        // Reset the SecDefsMgr and the RiskMgr ptrs:                        //
        //-------------------------------------------------------------------//
        // NB: The corresp objs themselves are  allocated in ShM, don't delete
        // them!
        m_secDefsMgr = nullptr;
        m_riskMgr    = nullptr;

        // Also close the MktData Output File:
        if (m_MDFD  != nullptr)
        {
          fclose(m_MDFD);
          m_MDFD = nullptr;
        }
        // Other components are finalised automatically...
      }
      catch (exception& exn)
      {
        cerr << "EXCEPTION in ~ManualTradingStart(): " << exn.what() << endl;
      }
    }

    //=======================================================================//
    // "Run":                                                                //
    //=======================================================================//
    void Run(bool a_use_busy_wait)
    {
      // Start the Connectors:
      if (m_mdc1 != nullptr)
        m_mdc1->Start();

      if (m_mdc2 != nullptr)
        m_mdc2->Start();

      if (m_omc != nullptr)
        m_omc->Start();

      // Enter the Inifinite Event Loop, IFF at least one of the Connectors
      // requires the BusyWait Mode:
      bool exitOnExc = true;
      bool busyWait  =
        a_use_busy_wait                              ||
        (m_mdc1 != nullptr && m_mdc1->UseBusyWait()) ||
        (m_mdc2 != nullptr && m_mdc2->UseBusyWait()) ||
        (m_omc  != nullptr && m_omc ->UseBusyWait());

      m_reactor->Run(exitOnExc, busyWait);
    }

    //=======================================================================//
    // "OnTradingEvent" Call-Back:                                           //
    //=======================================================================//
    void OnTradingEvent
    (
      EConnector const& a_conn,
      bool              a_on,
      SecID             a_sec_id,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    )
    override
    {
      utxx::time_val stratTime = utxx::now_utc ();
      // TradingEvent latency:
      long md_lat = (stratTime - a_ts_recv).microseconds();

      //---------------------------------------------------------------------//
      if (&a_conn == m_mdc1)
      //---------------------------------------------------------------------//
      {
        // If it is an MDC, it must be MDC1:
        assert(m_mdc1 != nullptr);
        if (m_mdc1->IsActive() && m_orderBooks.empty())  // Do it only once
        {
          // Subscribe to MktData now:
          // empty OrderBook, but in general it is OK):
          //
          for (SecDefD const* instr: m_instrs)
          {
            // Subscribe MktData for this "instr". Normnally, we just need L1Px
            // data, but if MktData Recording is requested, it would be L2:
            assert(instr != nullptr);

            OrderBook::UpdateEffectT level =
              (m_MDFD == nullptr)
              ? OrderBook::UpdateEffectT::L1Px
              : OrderBook::UpdateEffectT::L2;

            // XXX: RegisterInstrRisks=true (though for some Venues, a better
            // RiskMgmt policy would be preferred):
            m_mdc1->SubscribeMktData(this, *instr, level, true);

            // Get the corresp OrderBook from the Connector -- after Subscrip-
            // tion, it must exist:
            OrderBook const& ob = m_mdc1->GetOrderBook(*instr);
            m_orderBooks.push_back(&ob);
          }

          // We can now try to register the Converter OBs with the RiskMgr (even
          // though they may not be initialised yet, because no MktData are re-
          // ceived yet). NB: It is safe to do so even if the MDC is turning Ac-
          // tive more than once:
          // FIXME: This functionality must be re-implemented. For now, RiskMgr
          // cannot be used at all!
          // if (utxx::likely(m_riskMgr != nullptr)) {}
        }
      }
      //---------------------------------------------------------------------//
      // In any case, output a msg:                                          //
      //---------------------------------------------------------------------//
      cerr << "\nOnTradingEvent: Connector=" << a_conn.GetName() << ": Latency="
           << md_lat << " usec: Trading "    << (a_on ? "Started" : "Stopped")
           << ": MDCActive=" << (m_mdc1 != nullptr && m_mdc1->IsActive())
           << ", OMCActive=" << (m_omc  != nullptr && m_omc ->IsActive())
           << endl;

      //---------------------------------------------------------------------//
      // Place an Order Now?                                                 //
      //---------------------------------------------------------------------//
      // With some Px Types, we can place an order immediately when the OMC be-
      // comes Active:
      //
      if (utxx::unlikely
         (&a_conn == m_omc  && a_on && m_aoses.empty() &&
         (m_pxT == PxT::Number || m_pxT == PxT::Market ||
          m_pxT == PxT::PeggedPassive)))
        PlaceOrder();
      else
      //---------------------------------------------------------------------//
      // Should we stop now?                                                 //
      //---------------------------------------------------------------------//
      // Do so if both Connectors are now inactive (either due to a previously-
      // received termination signal, or because of an error):
      if (utxx::unlikely
         ((m_mdc1 == nullptr || !(m_mdc1->IsActive())) &&
          (m_omc  == nullptr || !(m_omc->IsActive()))  ))
        m_reactor->ExitImmediately("ManualTradingStrat::OnTradingEvent");
    }

  private:
    //=======================================================================//
    // "OnOrderBookUpdate" Call-Back:                                        //
    //=======================================================================//
    void OnOrderBookUpdate
    (
      OrderBook const&          a_ob,
      bool                      a_is_error,
      OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat
    )
    override
    {
      //---------------------------------------------------------------------//
      // Measure the internal latency of MktData delivery to the Strategy:   //
      //---------------------------------------------------------------------//
      // MKtData latency:
      long md_lat = (a_ts_strat - a_ts_recv).nanoseconds();

      // Check the error condition first. If there was an error, it is now cor-
      // rected anyway, so we only produce a warning:
      if (utxx::unlikely(a_is_error))
        cerr << "WARNING: Got OrderBook Error" << endl;

      SecDefD const& instr = a_ob.GetInstr();

      if (&instr != m_instrs.front())
        // This may happen if a Ccy Converter OrderBook has been updated, not
        // the Main one:
        return;

      //---------------------------------------------------------------------//
      // If Main Instrument: Get curr MktData:                               //
      //---------------------------------------------------------------------//
      ++m_updatesCount;

      PriceT bidPx = a_ob.GetBestBidPx();
      PriceT askPx = a_ob.GetBestAskPx();
      // NB: In some cases (eg if we receive banded quotes instead of a full
      // OrderBook), we cannot assert that bidPx < askPx!
      // But we need the Pxs to be available, in any case:
      if (utxx::unlikely(!(IsFinite(bidPx) && IsFinite(askPx))))
        return;

      //---------------------------------------------------------------------//
      // Submit the Order if we can:                                         //
      //---------------------------------------------------------------------//
      // (And if it was not submitted yet, and we are not stopping):
      //
      bool withFracQtys = a_ob.WithFracQtys();

      bool stopping =
        !m_aoses.empty()          &&
        (m_aoses[0]->m_isInactive || m_aoses[0]->m_isCxlPending != 0);

      if (utxx::likely
         (m_updatesCount   >= m_delay && m_signalsCount == 0 && !stopping &&
          m_omc != nullptr && m_omc->IsActive()))
      {
        // Once the Mkt Pxs have become known, we can submit an order of the
        // following types:
        if (utxx::unlikely
           ((m_pxT == PxT::Passive || m_pxT == PxT::Aggressive ||
             m_pxT == PxT::DeepAggressive)  && m_aoses.empty()))
        {
          // Generate the initial Order:
          PlaceOrder(bidPx, askPx, a_ts_exch, a_ts_recv, a_ts_strat);
          assert(!m_aoses.empty() && m_aoses[0] != nullptr);
        }
        else
        if (!m_aoses.empty())
        {
          // Modify the Passive Order (ONLY this type!) if MktData have changed
          // on the Ask side (sQty < 0) or on the Bid Side (sQty > 0). For Part-
          // Filled orders, only do that if such modifications are allowed:
          // Get CumQty in the "double" rep:
          QtyUD cumQty =
            withFracQtys
            ? QtyUD(m_aoses[0]->GetCumFilledQty<QTU,double>())
            : QtyUD(m_aoses[0]->GetCumFilledQty<QTU,long>  ());

          if (m_pxT == PxT::Passive &&
              (IsZero(cumQty) || m_omc->HasPartFilledModify()) &&
              ((m_sQty > 0.0  && bidPx != m_prevBidPx) ||
               (m_sQty < 0.0  && askPx != m_prevAskPx) ))
          {
            m_px = (m_sQty < 0.0) ? askPx : bidPx;
            ModifyOrder(a_ts_exch, a_ts_recv, a_ts_strat);
          }
        }
      }
      // Output the curr L1 pxs in any case:
      cerr << '\r'  << instr.m_FullName.data() << ": " << bidPx << " .. "
           << askPx << ": Lat=" << md_lat      << " nsec";

      // In any case, memoise the MktData:
      m_prevBidPx = bidPx;
      m_prevAskPx = askPx;

      //---------------------------------------------------------------------//
      // If in the Recording Mode:                                           //
      //---------------------------------------------------------------------//
      if (m_MDFD != nullptr)
      {
        // Output the Time Stamp:
        fprintf(m_MDFD, "%d %ld OrderBookUpdate\n",
                GetCurrDateInt(), (a_ts_strat - GetCurrDate()).microseconds());

        // Then Up to 20 levels of Bids and Asks:
        // XXX: Templated Lambdas are not in the language yet, so using the
        // run-time "withFraqQtys" param:
        auto action =
          [this, withFracQtys]
          (int a_level, PriceT a_px, OrderBook::OBEntry const& a_entry) ->bool
          {
            // Output the PxLevel, Px, Cumulative Qty and NOrders:
            if (withFracQtys)
              fprintf
                (this->m_MDFD, "%d %f %.2f %d\n", a_level, double(a_px),
                 double(a_entry.GetAggrQty<QTU,double>()), a_entry.m_nOrders);
            else
              fprintf
                (this->m_MDFD, "%d %f %ld %d\n",  a_level, double(a_px),
                 long  (a_entry.GetAggrQty<QTU,long> ()),  a_entry.m_nOrders);

            // Then output all Orders at that level:
            for (OrderBook::OrderInfo const*  oi = a_entry.m_frstOrder;
                 oi != nullptr; oi = oi->m_next)
              if (withFracQtys)
                fprintf
                  (this->m_MDFD, "%.2f ", double(oi->GetQty<QTU,double>()));
              else
                fprintf
                  (this->m_MDFD, "%ld ",  long  (oi->GetQty<QTU,long>  ()));

            fprintf(this->m_MDFD, "\n");
            return  true;
          };
        // Traverse Bids and Asks with this Action:
        if (withFracQtys)
        {
          // QT=UNDEFINED, QR=double, IsBid=true:
          a_ob.Traverse<true> (20, action);
          // QT=UNDEFINED, QR=double, IsBid=false:
          a_ob.Traverse<false>(20, action);
        }
        else
        {
          // QT=UNDEFINED, QR=long,   IsBid=true:
          a_ob.Traverse<true> (20, action);
          // QT=UNDEFINED, QR=long,   IsBid=false:
          a_ob.Traverse<false>(20, action);
        }
        fflush(m_MDFD);
      }
    }

    //=======================================================================//
    // "OnTradeUpdate" Call-Back:                                            //
    //=======================================================================//
    // NB: This method is currently provided because it is required by virtual
    // interface, but  is only used for data recording:
    //
    void OnTradeUpdate(Trade const& a_trade) override
    {
      if (m_MDFD == nullptr)
        return;

      // If recording is enabled: Output the Time Stamp:
      fprintf(m_MDFD, "%d %ld Trade\n", GetCurrDateInt(),
             (a_trade.m_recvTS  - GetCurrDate()).microseconds());

      // Then the Trade details: Price, Qty, Aggressor:
      char bsu =
           (a_trade.m_aggressor == FIX::SideT::Buy )
           ? 'B' :
           (a_trade.m_aggressor == FIX::SideT::Sell)
           ? 'S' : 'U';
      if (a_trade.m_withFracQtys)
        fprintf
          (m_MDFD, "%f %.2f %c\n",   double(a_trade.m_px),
           double(a_trade.GetQty<QTU,double>()), bsu);
      else
        fprintf
          (m_MDFD, "%f %ld %c\n",    double(a_trade.m_px),
           long  (a_trade.GetQty<QTU,long>  ()), bsu);
      fflush  (m_MDFD);
    }

  public:
    //=======================================================================//
    // "OnOurTrade" Call-Back:                                               //
    //=======================================================================//
    void OnOurTrade(Trade const& a_tr) override
    {
      Req12   const* req   = a_tr.m_ourReq;
      assert(req != nullptr);
      AOS     const* aos   = req->m_aos;
      assert(aos != nullptr && aos->m_instr != nullptr);

      // NB: "aos" must be in the "m_aoses" list:
      bool found =
        (find(m_aoses.cbegin(), m_aoses.cend(), aos) != m_aoses.cend());

      SecDefD const& instr = *(aos->m_instr);
      assert(&instr == m_instrs.front() && &instr == aos->m_instr);

      cerr << "\nTRADED: " << instr.m_Symbol.data()
           << (aos->m_isBuy ? ": B " : ": S ");
      if (a_tr.m_withFracQtys)
        cerr << double(a_tr.GetQty<QTU,double>());
      else
        cerr << long  (a_tr.GetQty<QTU,long>  ());

      cout << " @ "        << double(a_tr.m_px)
           << ", Fee="     << double(a_tr.GetFee<QTU,double>())
           << ", FromMDC=" << (a_tr.m_mdc != nullptr)
           << ", ExecID="  << a_tr.m_execID.ToString()
           << ", ReqID="   << ((a_tr.m_ourReq != nullptr)
                              ? a_tr.m_ourReq->m_id : 0)
           << (utxx::unlikely(!found) ? ": WARNING: AOS Not Found!" : "")
           << endl;

      if (m_riskMgr != nullptr)
      {
        Ccy ccyB = instr.m_QuoteCcy;
        AssetRisks const& arB =
          m_riskMgr->GetAssetRisks(ccyB.data(), instr.m_SettlDate, 0);
        cerr << "Ccy=" << ccyB.data() << ": CurrPos=" << arB.GetTrdPos()
             << endl;
      }
      // XXX: Even if the Order has been Filled, Do NOT reset "m_aoses" back
      // to empty -- this may trigger yet another automatic order placement,
      // which is NOT what we want!
    }

    //=======================================================================//
    // "OnConfirm" Call-Back:                                                //
    //=======================================================================//
    void OnConfirm(Req12 const& a_req) override
    {
      AOS const* aos = a_req.m_aos;
      assert(aos != nullptr);

      // Again, this "aos" must be on the "m_aoses" list:
      bool found =
        (find(m_aoses.cbegin(), m_aoses.cend(), aos) != m_aoses.cend());

      cerr << "\nOrder Confirmed by Exchange: OrderID=" << aos->m_id
           << ", ReqID="   << a_req.m_id << ", ExchID="
           << a_req.m_exchOrdID.ToString()
           << (utxx::unlikely(!found) ? ": WARNING: AOS Not Found!" : "")
           << endl;
    }

    //=======================================================================//
    // "OnCancel" Call-Back:                                                 //
    //=======================================================================//
    void OnCancel
    (
      AOS const&      a_aos,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    )
    override
    {
      assert(a_aos.m_isInactive);

      // Again, this "aos" must be on the "m_aoses" list:
      bool found =
        (find(m_aoses.cbegin(), m_aoses.cend(), &a_aos) != m_aoses.cend());

      cerr << "\nOrder CANCELLED: OrderID=" << a_aos.m_id
           << (utxx::unlikely(!found) ? ": WARNING: AOS Not Found!" : "")
           << endl;

      // XXX: Again, Do NOT reset "m_aoses" back to empty -- this may trigger
      // yet another automatic order placement, which is NOT what we want!
    }

    //=======================================================================//
    // "OnOrderError" Call-Back:                                             //
    //=======================================================================//
    void OnOrderError
    (
      Req12 const&    a_req,
      int             a_err_code,
      char  const*    a_err_text,
      bool            a_prob_filled,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    )
    override
    {
      AOS const* aos  = a_req.m_aos;
      assert(aos != nullptr);

      // Again, this "aos" must be on the "m_aoses" list:
      bool found =
        (find(m_aoses.cbegin(), m_aoses.cend(), aos) != m_aoses.cend());

      cerr << "\nERROR: OrderID=" << aos->m_id  << ", ReqID=" << a_req.m_id
           << ": ErrCode="        << a_err_code << ": "       << a_err_text
           << ((aos->m_isInactive)    ? ": ORDER FAILED"            : "")
           << (a_prob_filled          ? ": Probably Filled"         : "")
           << (utxx::unlikely(!found) ? ": WARNING: AOS Not Found!" : "")
           << endl;

      // XXX: Again, Do NOT reset "m_aoses" back to empty -- this may trigger
      // yet another automatic order placement, which is NOT what we want!
    }

    //=======================================================================//
    // "OnSignal" Call-Back:                                                 //
    //=======================================================================//
    void OnSignal(signalfd_siginfo const& a_si) override
    {
      // Cancel all Active Orders:
      bool cancelled = false;
      for (AOS const* aos: m_aoses)
      {
        assert(aos != nullptr);
        if (utxx::likely(!(aos->m_isInactive) && aos->m_isCxlPending == 0))
        {
          // Yes, can Cancel it:
          utxx::time_val now = utxx::now_utc();

          assert(m_omc != nullptr); // Of course -- we got an AOS!
          bool done     = m_omc->CancelOrder(aos, now, now, now);

          cerr << "\nGot a Signal (" << a_si.ssi_signo
               << "), Cancelling the OrderID=" << aos->m_id
               << (utxx::unlikely(!done) ? ": FAILED???" : "") << endl;
          cancelled |= done;
        }
      }
      if (utxx::likely(cancelled))
        // Done for now. XXX: Here we do NOT increment "m_signalsCount" yet:
        return;

      // Otherwise: Increment the counter and proceed with Graceful or Immedia-
      // te Shut-Down:
      ++m_signalsCount;
      assert(m_signalsCount >= 1);

      cerr << "\nGot a Signal (" << a_si.ssi_signo << "), Count="
           << m_signalsCount     << ", Exiting "
           << ((m_signalsCount == 1) ? "Gracefully" : "Immediately") << "..."
           << endl;

      if (m_signalsCount == 1)
      {
        // Try to stop all Connectors, and then exit (by TradingEvent):
        if (m_mdc1 != nullptr)
          m_mdc1->Stop();

        if (m_mdc2 != nullptr)
          m_mdc2->Stop();

        if (m_omc  != nullptr)
          m_omc->Stop();
      }
      else
        // Throw the "ExitRun" exception to exit the Main Event Loop (if we are
        // not in that loop, it will be handled safely anyway):
        m_reactor->ExitImmediately("ManualTradingStrat::OnSignal");
    }

    //=======================================================================//
    // "PlaceOrder":                                                         //
    //=======================================================================//
    void PlaceOrder
    (
      PriceT         a_bid_px   = PriceT(), // NaN
      PriceT         a_ask_px   = PriceT(), // NaN
      utxx::time_val a_ts_exch  = utxx::time_val(),
      utxx::time_val a_ts_recv  = utxx::time_val(),
      utxx::time_val a_ts_strat = utxx::time_val()
    )
    {
      //---------------------------------------------------------------------//
      // Pre-Checks:                                                         //
      //---------------------------------------------------------------------//
      // If we are in a position to place an order, must start the RiskMgr
      // first:
      SecDefD const& instr = *(m_instrs.front());
      if (m_riskMgr != nullptr)
        m_riskMgr->Start();

      // Once Again: There must be NO order placed already!
      if (utxx::unlikely(!m_aoses.empty()))
        throw utxx::runtime_error
              ("ManualTradingStrat::PlaceOrder: Already Submitted");

      //---------------------------------------------------------------------//
      // Calculate the actial Px:                                            //
      //---------------------------------------------------------------------//
      PriceT px;   // Initially NaN
      bool   isBuy = (m_sQty > 0);

      switch (m_pxT)
      {
        case PxT::Passive:
          // Here the px must be valid:
          px     = isBuy ? a_bid_px : a_ask_px;
          break;

        case PxT::Aggressive:
          px     = isBuy ? a_ask_px : a_bid_px;
          break;

        case PxT::DeepAggressive:
          px     =
            isBuy
            ? RoundUp  (1.01 * a_ask_px, instr.m_PxStep)
            : RoundDown(0.99 * a_bid_px, instr.m_PxStep);
          break;

        case PxT::Number:
          px = m_px;
          break;

        default:;
          // No Px is to be computed in the remaining cases ("None", "Market"
          // and "PeggedPassive"):
      }
      // In all cases except "None", "Market" and "PeggedPassive", the "px"
      // must at the end be valid:
      if (utxx::unlikely
         (!IsFinite(px)              && m_pxT != PxT::Market &&
          m_pxT != PxT::PeggedPassive && m_pxT != PxT::None))
            throw utxx::badarg_error("PlaceOrder: Missing Px");

      //---------------------------------------------------------------------//
      // Now actually place it:                                              //
      //---------------------------------------------------------------------//
      // XXX: The following is for "TestBufferedOrders". A similar test is act-
      // ually performed if "TestPipeLining" is requested, but then each pipe-
      // lined order comes with its full size:
      //
      double qtys[2] = { abs(m_sQty), 0.0 };
      if (utxx::unlikely(m_testBufferedOrders && !m_testPipeLining))
      {
        qtys[1] =  qtys[0] / 2;
        qtys[0] -= qtys[1];  // May become 0: this is checked in the loop hdr
      }

      FIX::OrderTypeT ordType =
        (m_pxT == PxT::Market)
        ? FIX::OrderTypeT::Market
        :
        (m_pxT == PxT::PeggedPassive)
        ? FIX::OrderTypeT::Pegged
        : FIX::OrderTypeT::Limit;   // Generic Case

      // XXX: In Test Modes, we can place multiple Orders here:
      int nOrds =
        m_testPipeLining ? int(m_aoses.size()) : m_testBufferedOrders ? 2 : 1;

      assert(m_omc != nullptr);
      for (int i = 0; i < nOrds; ++i)
      {
        AOS const* aos = nullptr;
        double qty =
          utxx::unlikely(m_testBufferedOrders && !m_testPipeLining)
          ? qtys[i]
          : qtys[0];

        // Is, after all,  our Order Aggressive?
        bool isAggr = (m_pxT >= PxT::Aggressive);
        try
        {
          // Normally, we just place a "NewOrder", unless it is "TestPipeLining"
          // in which case all subsequent Orders are modifications of the 1st
          // one:
          if (!m_testPipeLining || i == 0)
            aos = m_omc->NewOrder
            (
              this,
              instr,
              ordType,
              isBuy,
              px,         // NaN for Mkt and Pegged orders
              isAggr,
              QtyAny(qty, QtyTypeT::QtyA),
              a_ts_exch,
              a_ts_recv,
              a_ts_strat,
              (i == 0)
                ? (m_testBufferedOrders || m_testPipeLining)
                : false,  // NOT Buffered -- Flush Now!
              m_timeInForce
              // QtyShow, QtyMin and Peg params always take their defaul vals;
              // in particular, defaly Peg params mean pegging in THIS side of
              // the market with 0 offset -- ie Passive Pegging indeed!
            );
          else
            (void) m_omc->ModifyOrder
            (
              aos,
              px,
              false,     // NOT Aggressive here!
              QtyAny(qty, QtyTypeT::QtyA),
              a_ts_exch,
              a_ts_recv,
              a_ts_strat
            );

          if (m_testPipeLining)
          {
            // In this case, move the px for the next iteration:
            if (isBuy)
              px -= instr.m_PxStep;
            else
              px += instr.m_PxStep;
          }
        }
        catch (exception const& exc)
        {
          cerr << "\nNewOrder: EXCEPTION: " << exc.what() << endl;
          return;
        }
        assert(aos != nullptr && aos->m_id > 0);
        m_aoses.push_back(aos);

        Req12* req  = aos->m_lastReq;
        assert(req != nullptr && req->m_kind == Req12::KindT::New);
        int    lat  = req->GetInternalLatency();

        cerr << "\nNewOrder: OrderID=" << aos->m_id       <<  ", ReqID="
             << req->m_id    << ": "   << ((aos->m_isBuy) ? "Buy " : "Sell ");
        if (req->m_withFracQtys)
          cerr << double(req->GetQty<QTU,double>());
        else
          cerr << long  (req->GetQty<QTU,long>  ());
        cerr << ", Px="
             << (IsFinite(req->m_px) ? to_string(double(req->m_px)) : "NaN")
             << ": Latency="
             << ((lat != 0) ? to_string(lat) : "unknown") << " usec" << endl;
      }
    }

    //=======================================================================//
    // "ModifyOrder":                                                        //
    //=======================================================================//
    void ModifyOrder
    (
      utxx::time_val a_ts_exch  = utxx::time_val(),
      utxx::time_val a_ts_recv  = utxx::time_val(),
      utxx::time_val a_ts_strat = utxx::time_val()
    )
    {
      // This only works if there is EXACTLY ONE currently-active AOS:
      if (utxx::unlikely(m_aoses.size() != 1))
        throw utxx::badarg_error
              ("ManualTradingStrat::ModifyOrder: There are ", m_aoses.size(),
               ", not 1, orders available -- cannot handle!");

      AOS const* aos = m_aoses[0];
      assert(aos   != nullptr && m_sQty != 0 && IsFinite(m_px) &&
             m_omc != nullptr);
      bool   done   = false;
      double remQty = Abs(m_sQty) - double(aos->GetCumFilledQty<QTU,double>());

      if (utxx::unlikely(remQty <= 0))
      {
        cerr << "\nModifyOrder: AOS=" << aos->m_id << ": Skipped: RemQty="
             << remQty << endl;
        return;
      }
      // Generic Case:
      try
      {
        done = m_omc->ModifyOrder
        (
          aos,
          m_px,
          false,        // NOT Aggressive here!
          QtyAny(remQty, QtyTypeT::QtyA),
          a_ts_exch,
          a_ts_recv,
          a_ts_strat
        );
      }
      catch (exception const& exc)
      {
        assert(!done);
        cerr << "\nModifyOrder: EXCEPTION: " << exc.what() << endl;
        return;
      }

      if (utxx::likely(done))
      {
        Req12* req  = aos->m_lastReq;
        assert(req != nullptr &&
               (req->m_kind == Req12::KindT::Modify ||
                req->m_kind == Req12::KindT::ModLegN));
        int    lat  = req->GetInternalLatency();

        cerr << "\nModifyOrder: OrderID=" << aos->m_id   << ", ReqID="
             << req->m_id << ": NewPx="   << req->m_px   << ": Latency="
            << ((lat != 0) ? to_string(lat) : "unknown") << " usec" << endl;
      }
    }
  };
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  try
  {
    //-----------------------------------------------------------------------//
    // Global Init:                                                          //
    //-----------------------------------------------------------------------//
    IO::GlobalInit({SIGINT});

    //-----------------------------------------------------------------------//
    // Get the Config:                                                       //
    //-----------------------------------------------------------------------//
    if (argc != 2)
    {
      cerr << "PARAMETER: ConfigFile.ini" << endl;
      return 1;
    }
    string iniFile(argv[1]);

    // Get the Propert Tree:
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    //-----------------------------------------------------------------------//
    // Create the Logger and the Reactor:                                    //
    //-----------------------------------------------------------------------//
    shared_ptr<spdlog::logger> loggerShP =
      IO::MkLogger
      (
        "Main",
        pt.get<string>("Exec.LogFile"),
        false,        // MT is not needed
        pt.get<long>  ("Exec.LogFileSzM") * 1048576,
        pt.get<int>   ("Exec.LogFileRotations")
      );
    spdlog::logger* logger = loggerShP.get();
    assert(logger != nullptr);

    EPollReactor reactor
    (
      logger,
      pt.get<int> ("Exec.DebugLevel"),
      pt.get<bool>("Exec.UsePoll",      false),
      pt.get<bool>("Exec.UseKernelTLS", true),
      128,        // MaxFDs
      pt.get<int> ("Exec.CPU",          -1)
    );

    //-----------------------------------------------------------------------//
    // Create the Strategy Instrance:                                        //
    //-----------------------------------------------------------------------//
    // NB: This automatically creates the RiskMgr and Connecttors:
    //
    ManualTradingStrat mts(&reactor, logger, pt);

    //-----------------------------------------------------------------------//
    // Run the Strategy:                                                     //
    //-----------------------------------------------------------------------//
    bool useBusyWait = pt.get<bool>("Exec.UseBusyWait", 0);
    mts.Run(useBusyWait);
  }
  catch (exception const& exc)
  {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    // Prevent any further exceptions eg in Dtors:
    _exit(1);
  }
  return 0;
}

# ifdef  __GNUC__
# pragma GCC   diagnostic pop
# endif
# ifdef  __clang__
# pragma clang diagnostic pop
# endif
