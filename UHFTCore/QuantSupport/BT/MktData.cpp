// vim:ts=2:et
//===========================================================================//
//                    "QuantSupport/BT/MktData.h":                           //
//===========================================================================//
#include "QuantSupport/BT/MktData.h"

#include "QuantSupport/BT/SecDefs.h"
#include "Connectors/OrderBook.hpp"

#include <boost/algorithm/string.hpp>

namespace MAQUETTE
{
  namespace History
  {
    SecDefD const& FindSecDef(const std::string& ticker);
  }

  Historical_MktData::Historical_MktData(
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      std::vector<SecDefS> const*        a_expl_sec_defs,
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params,
      EConnector_MktData*                a_primary_mdc)
  :
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      a_params.get<std::string>("Exchange"),
      0,        // No extra ShM data
      a_reactor,
      false,    // No BusyWait
      a_sec_defs_mgr,
      a_expl_sec_defs != nullptr ?
        *a_expl_sec_defs :
        History::SecDefs(a_params.get<std::string>("Exchange")),
      nullptr,  // No restrictions by Symbol here
      a_params.get<bool>("ExplSettlDatesOnly", false),
      false,    // Presumably no Tenors in SecIDs
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    EConnector_MktData(
        true,  // Yes, IsEnabled!
        a_primary_mdc,
        a_params,
        false, // No FullAmt, presumably!
        true,  // Use sparse order book
        false, // No sequence numbers in history data
        false, // And no RptSeqs  either
        false, // So  no "continuous" RptSeqs
        0,     // We supports seeral markets, so MktDepth is unlimited
        true,  // Alwways use trades
        false, // Trades are NOT from FOL normally
        false  // No  DynInitMode
    )
  {
  }

  void Historical_MktData::Start()
  {
  }

  void Historical_MktData::Stop()
  {
  }

  bool Historical_MktData::IsActive() const
  {
    return true;
  }

  void Historical_MktData::Subscribe(Strategy* a_strat)
  {
    m_strat = a_strat;
    for(auto v: m_books)
    {
      History::SubscribeMktData(m_strat, this, v.second->GetInstr(), false);
    }
  }

  void Historical_MktData::UnSubscribe(Strategy const*)
  {
  }

  void Historical_MktData::SetTrade(
      const SecDefD& a_instr,
      utxx::time_val time,
      bool is_buy,
      PriceT price,
      QtyUD count)
  {
      Trade trade;
      const_cast<SecDefD const*&>(trade.m_instr)     = &a_instr;
      const_cast<PriceT&>        (trade.m_px)        = price;
      const_cast<QtyU&>          (trade.m_qty)       = QtyU(count);
      const_cast<FIX::SideT&>    (trade.m_aggressor) =
        is_buy ? FIX::SideT::Buy : FIX::SideT::Sell;
      const_cast<utxx::time_val&>(trade.m_recvTS)    = time;
      const_cast<utxx::time_val&>(trade.m_exchTS)    = time;
      m_strat->OnTradeUpdate(trade);
  }

  void Historical_MktData::SetBook(
      const SecDefD& a_instr,
      utxx::time_val time,
      uint16_t asks_size,
      History::quote* asks_quotes,
      uint16_t bids_size,
      History::quote* bids_quotes)
  {
    if(utxx::unlikely(!asks_size && !bids_size))
      return;

    OrderBook& ob = FindOrderBook(a_instr);
    ob.Invalidate();
    for (uint16_t i = 0; i != asks_size; ++i)
    {
        Qty<QtyTypeT::QtyA, double> c(double(asks_quotes[i].count));
        if (c != 0)
            ob.Update<
                false,
                false,
                false,
                true,
                false,
                false,
                true,
                QtyTypeT::QtyA,
                double>(
                FIX::MDUpdateActionT::New,
                asks_quotes[i].price,
                c,
                0,
                0,
                nullptr);
    }
    for (uint16_t i = 0; i != bids_size; ++i)
    {
        Qty<QtyTypeT::QtyA, double> c(double(bids_quotes[i].count));
        if (c != 0)
            ob.Update<
                true,
                false,
                false,
                true,
                false,
                false,
                true,
                QtyTypeT::QtyA,
                double>(
                FIX::MDUpdateActionT::New,
                bids_quotes[i].price,
                c,
                0,
                0,
                nullptr);
    }
    ob.SetInitialised();

    try
    {
      m_strat->OnOrderBookUpdate
        (ob, false, OrderBook::UpdatedSidesT::Both,
         time, time, time);
    } catch (utxx::runtime_error const& er)
    {
      LOG_INFO(2, "Historical_MktData::SetBook failed {}", er.what())
    }
  }

  OrderBook &Historical_MktData::FindOrderBook(const SecDefD &a_instr) const
  {
    auto it = m_books.find(a_instr.m_SecID);
    if (it == m_books.end())
    {
        auto orderBook = new OrderBook(
            this,
            &a_instr,
            false,
            true,
            QtyTypeT::QtyA,
            true,
            false,
            false,
            false,
            0,
            0,
            0);
        it = m_books
                 .insert(a_instr.m_SecID, std::unique_ptr<OrderBook>(orderBook))
                 .first;
    }
    return *(it->second);
  }

  void Historical_MktData::SubscribeMktData(
      Strategy* a_strat, SecDefD const& a_instr, OrderBook::UpdateEffectT, bool)
  {
      History::HistoricalData* hd =
          dynamic_cast<History::HistoricalData*>(a_strat);
      if (!hd) // for grid testing using History::HistoricalData interface much
               // faster
          m_strat = a_strat;
      History::SubscribeMktData(a_strat, hd ? hd : this, a_instr, false);
  }

  SecDefD const& Historical_MktData::FindSecDef(std::string const& instr) const
  {
    std::vector<std::string> parts;
    boost::split(parts, instr, [](char c) { return c == '-'; });
    if (parts.size() != 4 || !parts[2].empty())
      throw utxx::runtime_error(
          "Historical_MktData::FindSecDef() unsupported instrument: ",
          instr);
    return History::FindSecDef(parts[0]);
  }

  OrderBook const& Historical_MktData::GetOrderBook(SecDefD const& a_instr) const
  {
    return FindOrderBook(a_instr);
  }

  Historical_MktData::~Historical_MktData()
  {
  }
}

