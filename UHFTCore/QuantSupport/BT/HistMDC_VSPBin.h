// vim:ts=2:et
//===========================================================================//
//                "QuantSupport/BT/HistMDC_VSPBin.h":                        //
//          Market Data Connector that reads VSP binary data files           //
//===========================================================================//
#pragma once

#include <filesystem>
#include <memory>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Connectors/EConnector_MktData.h"
#include "Connectors/OrderBook.hpp"

namespace MAQUETTE
{
  class HistMDC_VSPBin final : public EConnector_MktData
  {
    public:
      constexpr static char Name[] = "HistMDC_VSPBin";
      constexpr static QtyTypeT QT = QtyTypeT::QtyA;
      using QR                     = long;
      using QtyN                   = Qty<QT, QR>;

      const char* GetName() const override
      {
        return Name;
      }

      HistMDC_VSPBin(
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
          // No restrictions by Symbol as we provide explicit SecDefs:
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
        m_data_file(a_params.get<std::string>("DataFile")),
        m_use_bid_for_ask(a_params.get<bool>("UseBidForAsk", false))
      {
      }

      ~HistMDC_VSPBin() override {}

      void EnsureAbstract() const override {}

      void Start() override
      {
        assert(m_strat != nullptr && m_book != nullptr);

        std::filesystem::path dat_file(m_data_file);

        if (!std::filesystem::exists(dat_file) ||
            !std::filesystem::is_regular_file(dat_file))
        {
          throw utxx::runtime_error("'" + m_data_file +
            "' does not exist or is not a regular file");
        }

        auto fsize = std::filesystem::file_size(dat_file);
        size_t num_recs = fsize / sizeof(DatRecord);
        assert(num_recs * sizeof(DatRecord) == fsize);

        // read the historical data
        std::vector<DatRecord> recs(num_recs);
        auto f = fopen(dat_file.c_str(), "rb");
        if (fread(recs.data(), sizeof(DatRecord), num_recs, f) != num_recs) {
          throw std::invalid_argument(
              "Got a different number of records than expected");
        }
        fclose(f);

        // convert to quotes
        LOG_INFO(2, "Using bid for ask: {}", m_use_bid_for_ask ? "yes" : "no")

        PriceT last_bid, last_ask;
        QtyN last_bid_size, last_ask_size;

        for (size_t i = 0; i < recs.size(); ++i) {
          auto time = utxx::msecs(size_t(recs[i].time * 1000.0));

          PriceT bid = Round(PriceT(double(recs[i].bid)),
            m_book->GetInstr().m_PxStep);
          PriceT ask = m_use_bid_for_ask ? bid :
            Round(PriceT(double(recs[i].ask)), m_book->GetInstr().m_PxStep);

          if (!m_use_bid_for_ask && (bid >= ask))
            ask = bid + m_book->GetInstr().m_PxStep;

          // since the bid and ask sizes are in 100,000, they can be 0 due to
          // rounding, so make it at least 1 * 100,000
          QtyN bid_size(std::max(1L, long(recs[i].bid_size)) * 100000L);
          QtyN ask_size = m_use_bid_for_ask ? bid_size :
            QtyN(std::max(1L, long(recs[i].ask_size)) * 100000L);

          // if ((last_bid == bid) && (last_ask == ask) &&
          //     (last_bid_size == bid_size) && (last_ask_size == ask_size))
          //   continue;

          last_bid = bid;
          last_ask = ask;
          last_bid_size = bid_size;
          last_ask_size = ask_size;

          // printf("[%lu] [%s] Update OB: bid %li @ %.5f, ask %li @ %.5f\n",
          //   i, utxx::time_val(time).to_string(utxx::DATE_TIME_WITH_USEC).c_str(),
          //   long(bid_size), double(bid), long(ask_size), double(ask));

          m_book->Invalidate();

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

          m_book->SetInitialised();

          try
          {
            m_strat->OnOrderBookUpdate
              (*m_book, false, OrderBook::UpdatedSidesT::Both,
               time, time, time);
          } catch (utxx::runtime_error const& er)
          {
            LOG_INFO(2, "Historical_MktData::SetBook failed {}", er.what())
          }
        }
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
      struct DatRecord {
      double time;
      uint8_t type = 0;
      uint8_t index = 0;
      uint8_t source = 106;   // hotspot
      uint8_t reserved = 255; // for alignment
      float bid, ask;
      uint8_t bid_size, ask_size; // in 100,000
      uint16_t reserved2 = 42;    // alignment
    };

      Strategy* m_strat = nullptr;
      std::unique_ptr<OrderBook> m_book = nullptr;

      std::string m_data_file;
      bool m_use_bid_for_ask;
  };

} // namespace MAQUETTE
