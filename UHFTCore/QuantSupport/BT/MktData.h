// vim:ts=2:et
//===========================================================================//
//                    "QuantSupport/BT/MktData.h":                           //
//===========================================================================//
#pragma once

#include "QuantSupport/BT/BackTest.h"
#include "Connectors/EConnector_MktData.h"

#include <boost/ptr_container/ptr_map.hpp>

namespace MAQUETTE
{
  struct Historical_MktData final : EConnector_MktData, History::HistoricalData
  {
    constexpr static char Name[] = "History-MDC";
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using QR                     = double;
    using QtyN                   = Qty<QT, QR>;

    const char* GetName() const override
    {
      return Name;
    }

    Historical_MktData(
        EPollReactor*                      a_reactor,
        SecDefsMgr*                        a_sec_defs_mgr,
        std::vector<SecDefS> const*        a_expl_sec_defs,
        RiskMgr*                           a_risk_mgr,
        boost::property_tree::ptree const& a_params,
        EConnector_MktData*                a_primary_mdc);

    void EnsureAbstract() const override {}

    void Start() override;
    void Stop() override;
    bool IsActive() const override;

    void Subscribe(Strategy* a_strat) override;
    void UnSubscribe(Strategy const* a_strat) override;
    void SubscribeMktData
    (
      Strategy*                 a_strat,
      SecDefD const&            a_instr,
      OrderBook::UpdateEffectT  a_min_cb_level,
      bool                      a_reg_instr_risks
    ) override;
    SecDefD const& FindSecDef(std::string const& instr) const;
    OrderBook const& GetOrderBook(SecDefD const& a_instr) const override;

    ~Historical_MktData() override;

    void SetTrade(
        const SecDefD& a_instr,
        utxx::time_val time,
        bool is_buy,
        PriceT price,
        QtyUD count) override;

    void SetBook(
        const SecDefD& a_instr,
        utxx::time_val time,
        uint16_t asks_size,
        History::quote* asks_quotes,
        uint16_t bids_size,
        History::quote* bids_quotes) override;

    private:
      // Find order book or create new if none exists.
      // This function is const to be called from GetOrderBook method
      OrderBook& FindOrderBook(SecDefD const& a_instr) const;

    private:
      Strategy* m_strat = nullptr;
      mutable boost::ptr_map<SecID, OrderBook> m_books;
  };

} // namespace MAQUETTE
