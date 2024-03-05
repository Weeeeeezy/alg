// vim:ts=2:et
//===========================================================================//
//           "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_MDC.cpp"           //
//===========================================================================//
#include "Connectors/H2WS/Huobi/EConnector_WS_Huobi_MDC.h"

#include <exception>
#include <utxx/error.hpp>

#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Connectors/H2WS/Huobi/EConnector_Huobi_Common.hpp"
#include "Connectors/H2WS/Huobi/Processor_MDC.hpp"
#include "Venues/Huobi/Configs_H2WS.h"

namespace MAQUETTE
{
  using namespace Huobi;

  template <AssetClassT::type AC>
  EConnector_WS_Huobi_MDC<AC>::EConnector_WS_Huobi_MDC(
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      std::vector<std::string>    const* a_only_symbols,  // NULL=UseFullList
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params,
      EConnector_MktData*                a_primary_mdc) :
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "Huobi",
      0,                              // ExtraShMSize
      a_reactor,
      false,                          // No BusyWait
      a_sec_defs_mgr,
      Huobi::GetSecDefs(AC),
      a_only_symbols,                 // Restricting full Huobi list
      a_params.get<bool>("ExplSettlDatesOnly",   false),
      false,                          // Presumably no Tenors in SecIDs
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    MDCWS(a_primary_mdc, a_params),
    m_useBBA(a_params.get<bool>("UseBBA", false)),
    m_zlib(new Huobi::zlib) { }

  template<AssetClassT::type AC>
  EConnector_WS_Huobi_MDC<AC>::~EConnector_WS_Huobi_MDC() noexcept = default;

  template <AssetClassT::type AC>
  H2WSConfig const& EConnector_WS_Huobi_MDC<AC>::GetWSConfig(MQTEnvT a_env)
  {
    auto cit = Huobi::Configs_WS_MDC.find(std::make_pair(AC, a_env));
    if (utxx::unlikely(cit == Huobi::Configs_WS_MDC.cend()))
      throw utxx::badarg_error
            ("EConnector_WS_Huobi_MDC::GetWSConfig: UnSupported Env=",
             a_env.to_string());
    return cit->second;
  }

  template <AssetClassT::type AC>
  void EConnector_WS_Huobi_MDC<AC>::InitSession() const
  {
    LOG_INFO(2, "EConnector_WS_Huobi_MDC::InitSession()")

    // IMPORTANT: Reset the Channels, as we are going to make new Subcrs (poss-
    // ibly after a reconnect):
    m_channels.clear();

    this->InitWSHandShake(Huobi::Traits<AC>::MdcEndpoint);
  }

  template <AssetClassT::type AC>
  void EConnector_WS_Huobi_MDC<AC>::InitLogOn()
  {
    // Clear the existing channel map in case of reconnect
    m_channels.clear();

    // There is no explicit logon, just subscribe to market data
    LOG_INFO(2,
      "EConnector_WS_Huobi_MDC::InitWSSession: Subscribing to MktData")

    for (const auto& secDef : this->m_usedSecDefs) {
      std::string symbol = secDef->m_AltSymbol.data();
      OrderBook* orderBook = this->template FindOrderBook<true>(symbol.data());

      if (utxx::unlikely(!orderBook))
        throw utxx::runtime_error(
          "EConnector_WS_Huobi_MDC::InitLogOn() OrderBook not found for ",
            symbol);

      orderBook->SetInitialised();
      m_channels.insert({symbol, orderBook});

      std::string sub = "{\"sub\":\"market." + symbol;
      if (m_useBBA)
        sub += ".bbo";
      else
        sub += (IsSpt ? ".mbp.150" : ".depth.size_150.high_freq");
      sub += "\",";
      if (!IsSpt && !m_useBBA)
        sub += "\"data_type\":\"incremental\",";
      sub += "\"id\":\"book_" + symbol + "\"}";

      LOG_INFO(3, "EConnector_WS_Huobi_MDC::InitLogOn: Subscribe {}",
        sub.c_str())
      this->PutTxtData(sub.c_str());
      CHECK_ONLY(this->template LogMsg<true>(sub.c_str(), nullptr, 0);)
      this->SendTxtFrame();

      if (EConnector_MktData::HasTrades()) {
        std::string sub1 = "{\"sub\":\"market." + symbol;
        sub1 += ".trade.detail\",\"id\":\"trades_" + symbol + "\"}";

        LOG_INFO(3, "EConnector_WS_Huobi_MDC::InitlogOn: Subscribe {}",
          sub1.c_str())
        this->PutTxtData(sub1.c_str());
        CHECK_ONLY(this->template LogMsg<true>(sub1.c_str(), nullptr, 0);)
        this->SendTxtFrame();
      }
    }

    // Signal completion immediately:
    MDCWS::LogOnCompleted(utxx::now_utc());
  }

  //-------------------------------------------------------------------------//
  // Explicit Instances:                                                     //
  //-------------------------------------------------------------------------//
  template class EConnector_WS_Huobi_MDC<AssetClassT::Spt>;
  template class EConnector_WS_Huobi_MDC<AssetClassT::Fut>;
  template class EConnector_WS_Huobi_MDC<AssetClassT::CSwp>;
  template class EConnector_WS_Huobi_MDC<AssetClassT::USwp>;
}
// namespace MAQUETTE
