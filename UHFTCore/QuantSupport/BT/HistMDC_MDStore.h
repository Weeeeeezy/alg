// vim:ts=2:et
//===========================================================================//
//                "QuantSupport/BT/HistMDC_MDStore.h":                       //
//         Market Data Connector that reads from the MD store                //
//===========================================================================//
#pragma once

#include <filesystem>
#include <memory>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Connectors/EConnector_MktData.h"
#include "Connectors/OrderBook.hpp"
#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

using MAQUETTE::QuantSupport::MDRecL1;
using MAQUETTE::QuantSupport::MDStoreReader;

namespace MAQUETTE
{
  class HistMDC_MDStore final : public EConnector_MktData
  {
    public:
      constexpr static char Name[] = "HistMDC_MDStore";
      constexpr static QtyTypeT QT = QtyTypeT::QtyA;
      using QR                     = long;
      using QtyN                   = Qty<QT, QR>;

      const char* GetName() const override
      {
        return Name;
      }

      HistMDC_MDStore(
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
          0,     // No extra ShM data
          a_reactor,
          false, // No BusyWait
          a_sec_defs_mgr,
          a_expl_sec_defs != nullptr ?
            *a_expl_sec_defs :
            std::vector<SecDefS>(),
          // As we provide explicit SecDefs, we may not need further restrict-
          // ions by Symbol?
          nullptr,
          a_params.get<bool>("ExplSettlDatesOnly", false),
          false, // Presumably no Tenors in SecIDs
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
          false, // And no RptSeqs either
          false, // So no "continuous" RptSeqs
          0,     // We supports serial markets, so MktDepth is unlimited
          false, // Always use trades
          false, // Trades are NOT from FOL normally
          false  // No DynInitMode
        ),
        m_md_store_root(a_params.get<std::string>("MDStoreRoot")),
        m_venue(a_params.get<std::string>("Venue")),
        m_symbol(a_params.get<std::string>("Symbol")),
        m_use_bid_for_ask(a_params.get<bool>("UseBidForAsk", false))
      {
        auto tm_to_time_val_utc = [](std::tm tm) {
          return utxx::time_val(tm.tm_year + 1900, unsigned(tm.tm_mon + 1),
                                unsigned(tm.tm_mday), unsigned(tm.tm_hour),
                                unsigned(tm.tm_min), unsigned(tm.tm_sec), true);
        };

        // get the start time
        std::tm start_tm;
        std::istringstream start_iss(a_params.get<std::string>("StartTimeUTC"));
        start_iss >> std::get_time(&start_tm, "%Y-%m-%d %H:%M:%S");

        // get optional end time
        std::tm end_tm;
        std::istringstream end_iss(
          a_params.get<std::string>("EndTimeUTC", "2100-01-01 00:00:00"));
        end_iss >> std::get_time(&end_tm, "%Y-%m-%d %H:%M:%S");

        m_start = tm_to_time_val_utc(start_tm);
        m_end = tm_to_time_val_utc(end_tm);
      }

      ~HistMDC_MDStore() override {}

      void EnsureAbstract() const override {}

      void Start() override
      {
        assert(m_strat != nullptr && m_book != nullptr);

        using Reader = MDStoreReader<MDRecL1>;
        Reader mds(m_md_store_root, m_venue, m_symbol, "ob_L1");

        // convert to quotes
        LOG_INFO(2, "Using bid for ask: {}", m_use_bid_for_ask ? "yes" : "no")

        mds.Read(m_start, [&](const Reader::MDStoreRecord &rec) {
          // quit reading if we've reached the end time
          if (rec.ts_recv > m_end)
            return false;

          PriceT bid = Round(PriceT(double(rec.rec.bid)),
            m_book->GetInstr().m_PxStep);
          PriceT ask = m_use_bid_for_ask ? bid :
            Round(PriceT(double(rec.rec.ask)), m_book->GetInstr().m_PxStep);

          if (!m_use_bid_for_ask && (bid >= ask))
            ask = bid + m_book->GetInstr().m_PxStep;

          // NB: some quantities are given in 100k, so they could be 0.5
          QtyN bid_size(0L), ask_size(0L);
          if(rec.rec.bid_size > 0.0)
            bid_size = QtyN(long(std::max(1.0, rec.rec.bid_size)));

          if (m_use_bid_for_ask) {
            ask_size = bid_size;
          } else {
            ask_size = QtyN(long(std::max(1.0, rec.rec.ask_size)));
          }

          m_book->Invalidate();

          if (IsFinite(ask) && !IsSpecial0(ask_size))
          {
            m_book->Update<
              false,
              false,
              false,
              true,
              false,
              false,
              true,
              QtyTypeT::QtyA,
              long>(
              FIX::MDUpdateActionT::New,
              ask,
              ask_size,
              0,
              0,
              nullptr);
          }

          if (IsFinite(bid) && !IsSpecial0(bid_size))
          {
            m_book->Update<
              true,
              false,
              false,
              true,
              false,
              false,
              true,
              QtyTypeT::QtyA,
              long>(
              FIX::MDUpdateActionT::New,
              bid,
              bid_size,
              0,
              0,
              nullptr);
          }

          m_book->SetInitialised();

          try
          {
            m_strat->OnOrderBookUpdate
              (*m_book, false, OrderBook::UpdatedSidesT::Both,
               rec.ts_exch, rec.ts_recv, rec.ts_recv);
          } catch (utxx::runtime_error const& er)
          {
            LOG_INFO(2, "Historical_MktData::SetBook failed {}", er.what())
          }

          return true;
        });
      }

      void Stop() override {}
      bool IsActive() const override { return true; }

      void Subscribe(Strategy* a_strat) override
      {
        if(m_strat != nullptr)
          throw std::runtime_error(
            "HistOMC_InstaFill::Subscribe() only one subscriber supported");
        m_strat = a_strat;
      }

      void UnSubscribe(Strategy const* DEBUG_ONLY(a_strat)) override
      {
        assert(a_strat == m_strat);
        m_strat = nullptr;
      }

      void SubscribeMktData
      (
        Strategy*                 DEBUG_ONLY(a_strat),
        SecDefD const&            a_instr,
        OrderBook::UpdateEffectT  DEBUG_ONLY(a_min_cb_level),
        bool                      /*a_reg_instr_risks*/
      ) override
      {
        assert(a_strat == m_strat);
        assert(a_min_cb_level == OrderBook::UpdateEffectT::L1Qty);
        m_book = std::make_unique<OrderBook>(this, &a_instr, false, true, QT,
          false, false, false, false, 0, 0, 0);
      }

      OrderBook const& GetOrderBook(SecDefD const& DEBUG_ONLY(a_instr)) const override
      {
        assert(m_book != nullptr && &m_book->GetInstr() == &a_instr);
        return *m_book;
      }

    private:
      Strategy* m_strat = nullptr;
      std::unique_ptr<OrderBook> m_book = nullptr;

      std::string m_md_store_root;
      std::string m_venue;
      std::string m_symbol;
      utxx::time_val m_start, m_end;
      bool m_use_bid_for_ask;
  };

} // namespace MAQUETTE
