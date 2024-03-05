// vim:ts=2:et
//===========================================================================//
//        "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_MDC.cpp":     //
//                           WS-Based MDC for KrakenSpot                     //
//===========================================================================//
#include "Connectors/H2WS/KrakenSpot/EConnector_WS_KrakenSpot_MDC.h"
#include "Connectors/H2WS/KrakenSpot/Processor_MDC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/KrakenSpot/SecDefs.h"
#include "Venues/KrakenSpot/Configs_WS.h"

using namespace std;

//===========================================================================//
// Macros:                                                                   //
//===========================================================================//
// Short-cuts for accessing the Flds and Methods of the "Derived" class:
// Non-Const:
#ifdef DER
#undef DER
#endif
#define DER static_cast<Derived *>(this)->Derived

// Const:
#ifdef CDER
#undef CDER
#endif
#define CDER static_cast<Derived const *>(this)->Derived

namespace MAQUETTE
{
  using namespace std;
  //=========================================================================//
  // "GetAllSecDefs":                                                        //
  //=========================================================================//
  std::vector<SecDefS> const* EConnector_WS_KrakenSpot_MDC::GetSecDefs()
    { return &KrakenSpot::SecDefs; }

  //=========================================================================//
  // "GetWSConfig":                                                          //
  //=========================================================================//
  H2WSConfig const& EConnector_WS_KrakenSpot_MDC::GetWSConfig
  (
    MQTEnvT a_env
  )
  {
    // XXX: For KrakenSpot, only "Prod" is actually supported, but for an MDC,
    // this does not matter, so we always return the "Prod" config here:
    //
    auto   cit  = KrakenSpot::Configs_WS_MDC.find(a_env);
    if (cit == KrakenSpot::Configs_WS_MDC.cend())
      throw utxx::runtime_error("KrakenSpot MDC: WS Config not found.");
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_KrakenSpot_MDC::EConnector_WS_KrakenSpot_MDC
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string>              const*  a_only_symbols,  // May be NULL
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params,
    EConnector_MktData*                 a_primary_mdc    // Normally NULL
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    // NB: As usual, AccountKey is the Connector Instance Name.
    // It may look like {AccountPfx}-WS-KrakenSpot-MDC-{Env}:
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "KrakenSpot",
      0,                            // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                        // No need fora BusyWait
      a_sec_defs_mgr,
      KrakenSpot::SecDefs,
      a_only_symbols,               // Restricting KrakenSpot::SecDefs
      a_params.get<bool>("ExplSettlDatesOnly", false),
      false,                        // No Tenors
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_MDC":                                                  //
    //-----------------------------------------------------------------------//
    MDCWS
    (
      a_primary_mdc,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_KrakenSpot_MDC" Itself:                                 //
    //-----------------------------------------------------------------------//
    m_useBBA            (a_params.get<bool>("UseBBA", false)),
    m_mdMsg             ()
  {}

  //=========================================================================//
  // Dtor, Start and Stop                                                    //
  //=========================================================================//
  EConnector_WS_KrakenSpot_MDC::~EConnector_WS_KrakenSpot_MDC() noexcept
  {
    // TODO unsubscribe
  }

  void EConnector_WS_KrakenSpot_MDC::Start()
  {
    // We only start the H2 leg at the moment; the WS leg will be started when
    // the ListenKey becomes available (via the H2 leg):
    LOG_INFO(2, "EConnector_WS_KrakenSpot_MDC::Start: Starting Up")
    MDCWS::Start();
  }

  void EConnector_WS_KrakenSpot_MDC::Stop()
  {
    LOG_INFO(2, "EConnector_WS_KrakenSpot_MDC::Stop: Stopping")
    MDCWS::Stop();
  }

  //=========================================================================//
  // "ChannelMapReset":                                                      //
  //=========================================================================//
  void EConnector_WS_KrakenSpot_MDC::ChannelMapReset()
  {
    m_channels.clear();
    m_symbols.clear();
  }

  //=========================================================================//
  // "InitWSSession":                                                        //
  //=========================================================================//
  void EConnector_WS_KrakenSpot_MDC::InitWSSession()
  {
    LOG_INFO(2,
      "EConnector_WS_KrakenSpot_MDC::InitSession: Running InitWSHandShake")

    // IMPORTANT: Reset the Channels, as we are going to make new Subcrs (poss-
    // ibly after a reconnect):
    ChannelMapReset();

    // Proceed with the HTTP/WS HandShake using the following Path:
    ProWS::InitWSHandShake("/ws");
  }

  //-------------------------------------------------------------------------//
  // "InitWSLogOn":                                                          //
  //-------------------------------------------------------------------------//
  void EConnector_WS_KrakenSpot_MDC::InitWSLogOn()
  {
    // Clear the existing channel map in case of reconnect
    ChannelMapReset();

    // There is no explicit logon, just subscribe to market data
    LOG_INFO(2,
      "EConnector_WS_KrakenSpot_MDC::InitWSSession: Subscribing to MktData "
      "Channels")

    std::string symbols = "";
    bool first = true;
    for (SecDefD const* instr : m_usedSecDefs) {
      assert(instr != nullptr);
      if (first)
        first = false;
      else
        symbols += ",";

      symbols += "\"" + std::string(instr->m_Symbol.data()) + "\"";
    }

    // TODO DEBUG
    // symbols = "\"XBT/USD\",\"ETH/EUR\"";

    // subscribe to market data (ticker or book)
    std::string md_sub =
      "{\"event\":\"subscribe\",\"subscription\":{\"name\":\"";
    md_sub += (m_useBBA ? "ticker" : "book");
    md_sub += "\"},\"pair\":[" + symbols + "]}";

    CHECK_ONLY(
      LogMsg<true>(md_sub.c_str(), nullptr, 0);
    )

    ProWS::PutTxtData(md_sub.c_str());
    ProWS::SendTxtFrame();

    if (EConnector_MktData::HasTrades()) {
      std::string trade_sub = "{\"event\":\"subscribe\",\"subscription\":"
        "{\"name\":\"trade\"},\"pair\":[" + symbols + "]}";

      CHECK_ONLY(
        LogMsg<true>(trade_sub.c_str(), nullptr, 0);
      )

      ProWS::PutTxtData(trade_sub.c_str());
      ProWS::SendTxtFrame();
    }

    // Signal completion immediately:
    MDCWS::LogOnCompleted(utxx::now_utc());
  }

}
// End namespace MAQUETTE
