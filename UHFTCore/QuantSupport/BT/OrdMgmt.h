// vim:ts=2:et
//===========================================================================//
//                    "QuantSupport/BT/OrdrMgmt.h":                          //
//===========================================================================//
#pragma once

#include "BackTest.h"
#include "Connectors/EConnector_OrdMgmt.h"
#include <boost/container/flat_set.hpp>

namespace MAQUETTE
{
  struct Historical_OrdMgmt final : EConnector_OrdMgmt, History::HistoricalData
  {
    constexpr static char Name[] = "History-OMC";
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using QR                     = double;
    using QtyN                   = Qty<QT, QR>;

    Historical_OrdMgmt(
        EPollReactor*                      a_reactor,
        SecDefsMgr*                        a_sec_defs_mgr,
        std::vector<SecDefS> const*        a_expl_sec_defs,
        RiskMgr*                           a_risk_mgr,
        boost::property_tree::ptree const& a_params);

    void Start() override;
    void Stop() override;
    bool IsActive() const override;

    void EnsureAbstract() const override {}

    void Subscribe(Strategy* a_strat) override;
    void UnSubscribe(Strategy const* a_strat) override;
    bool HasPartFilledModify() const override
    {
      return true;
    }

    AOS const* NewOrder(
        Strategy*           a_strategy,
        SecDefD const&      a_instr,
        FIX::OrderTypeT     a_ord_type,
        bool                a_is_buy,
        PriceT              a_px,
        bool                a_is_aggr,
        QtyAny              a_qty,
        utxx::time_val      a_ts_md_exch    = utxx::time_val(),
        utxx::time_val      a_ts_md_conn    = utxx::time_val(),
        utxx::time_val      a_ts_md_strat   = utxx::time_val(),
        bool                a_batch_send    = false,
        FIX::TimeInForceT   a_time_in_force = FIX::TimeInForceT::UNDEFINED,
        int                 a_expire_date   = 0,
        QtyAny              a_qty_show      = QtyAny::PosInf(),
        QtyAny              a_qty_min       = QtyAny::Zero  (),
        bool                a_peg_side      = true,
        double              a_peg_offset    = NaN<double>
    ) override;

    bool CancelOrder(
        AOS const*     a_aos,
        utxx::time_val a_ts_md_exch,
        utxx::time_val a_ts_md_conn,
        utxx::time_val a_ts_md_strat,
        bool           a_batch_send
    ) override;

    bool ModifyOrder(
        AOS const*,
        PriceT,
        bool,
        QtyAny,
        utxx::time_val,
        utxx::time_val,
        utxx::time_val,
        bool,
        QtyAny,
        QtyAny
    ) override;

    void CancelAllOrders(
        unsigned long  a_strat_hash48 = 0,                     // All  Strats
        SecDefD const* a_instr        = nullptr,               // All  Instrs
        FIX::SideT     a_side         = FIX::SideT::UNDEFINED, // Both Sides
        char const*    a_segm_id      = nullptr                // All  Segms
    ) override;

    utxx::time_val FlushOrders() override;

    ~Historical_OrdMgmt() override;

    Strategy* m_strat = nullptr;

    constexpr static uint32_t quotes_size = 10;

    AOS m_aos[quotes_size];
    AOS *m_aos_f, *m_aos_t, *m_aos_c;
    Req12 m_req[quotes_size];

    struct Statistics
    {
      SecID       secID;
      std::string symbol;

      double   fee_maker  = 0;
      double   fee_taker  = 0;
      uint32_t new_orders = 0;
      uint32_t cancels    = 0;
      uint32_t modifies   = 0;
      double   volume     = 0;
      double   delta      = 0;
      double   min_pos    = 0;
      double   max_pos    = 0;
      double   cur_pos    = 0;
    };

    // To close positions correctly we are to have last price for each instrument
    std::unordered_map<SecID, double> m_lastPrices;

    void SetTrade(
        const SecDefD& a_instr,
        utxx::time_val time,
        bool           is_buy,
        PriceT         price,
        QtyUD          count) override;

    utxx::time_val last_time;

    void SetBook(
        const SecDefD&  a_instr,
        utxx::time_val  time,
        uint16_t        asks_size,
        History::quote* asks_quotes,
        uint16_t        bids_size,
        History::quote* bids_quotes) override;

    int hdeals;
    std::string name, result;

    const char* GetName() const override
      { return name.c_str(); }

    double GetPosition(SecDefD const*) const override
      { return 0.0; }

    double GetBalance(SymKey) const override
      { return 0.0; }

    private:
      std::vector<Statistics>::iterator findStatistics(
          const SecDefD &a_instr,
          Strategy * a_strat = nullptr);

    private:
      const double fee_maker;
      const double fee_taker;

      // Statistics for instrument
      std::vector<Statistics> m_statistics;
      OrderID lastReqId = 0;
  };
}

