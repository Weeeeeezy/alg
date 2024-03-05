// vim:ts=2:et
//===========================================================================//
//                "Tools/MDRecorders/MDRecorder_HotSpot.cpp":                //
//                    Market Data Recorder for HotSpot                       //
//===========================================================================//
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>
#include <memory>
#include <signal.h>
#include <string>
#include <unordered_map>
#include <utxx/time_val.hpp>

#include "Basis/BaseTypes.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.hpp"
#include "Connectors/FIX/EConnector_FIX.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/Strategy.hpp"
#include "QuantSupport/MDSaver.hpp"
#include "QuantSupport/MDStore.hpp"

using namespace std;
using namespace MAQUETTE;
using namespace MAQUETTE::QuantSupport;

namespace
{
  //=========================================================================//
  // "MDRecorder_HotSpot" Class:                                             //
  //=========================================================================//
  class MDRecorder_HotSpotFIX final: public Strategy
  {
  private:
    //=======================================================================//
    // Data Flds and Ctor:                                                   //
    //=======================================================================//
    mutable bool m_exiting;
    EConnector_FIX_HotSpotFX_Gen* m_conn;

    // so that we can quickly get the MDStoreWriter given a sec def ID, we use
    // a vector of unique_ptrs
    vector<unique_ptr<MDStoreWriter<MDRecL1>>> m_md_stores_L1;
    vector<unique_ptr<MDStoreWriter<MDAggression>>> m_md_stores_trades;

    unordered_map<SecID, size_t> m_secID_to_idx;

  public:
    MDRecorder_HotSpotFIX
    (
      string const&                       a_name,
      EPollReactor*                       a_reactor,
      spdlog::logger*                     a_logger,
      int                                 a_debug_level,
      boost::property_tree::ptree const&  /*a_pt*/,
      EConnector_FIX_HotSpotFX_Gen*       a_conn,
      string                              a_md_store_root
    )
    : Strategy
      (
        a_name,
        a_reactor,
        a_logger,
        a_debug_level,
        { SIGINT }
      ),
      m_exiting(false),
      m_conn(a_conn)
    {
      // init the MD Stores
      auto const& sec_defs = m_conn->GetSecDefsMgr()->GetAllSecDefs();

      m_md_stores_L1.resize(sec_defs.size());
      m_md_stores_trades.resize(sec_defs.size());

      for (size_t i = 0; i < sec_defs.size(); ++i) {
        auto const &s = sec_defs[i];
        auto symbol = string(s.m_Symbol.data());
        m_md_stores_L1[i] = make_unique<MDStoreWriter<MDRecL1>>(
          a_md_store_root, "HotSpotFIX", symbol, "ob_L1");
        m_md_stores_trades[i] = make_unique<MDStoreWriter<MDAggression>>(
          a_md_store_root, "HotSpotFIX", symbol, "trades");
        m_secID_to_idx[s.m_SecID] = i;
      }
    }

    //=======================================================================//
    // Common Call-Back used by both MktData and OrdMgmt Connectors:         //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OnTradingEvent":                                                     //
    //-----------------------------------------------------------------------//
    // NB: This Call-Back can be invoked from ANY "EConnector" -- both MktData
    // and OrdMgmt:
    void OnTradingEvent
    (
      EConnector const& a_conn,
      bool              a_on,
      SecID             /*a_sec_id*/,
      utxx::time_val    /*a_ts_exch*/,
      utxx::time_val    /*a_ts_recv*/
    )
    override
    {
      cerr << "OnTradingEvent: Connector="       << a_conn.GetName()
           << ": Trading " << (a_on ? "Started" : "Stopped") << endl;
      if (!a_on && !m_exiting)
      {
        // NB: Throw the "ExitRun" exception only once, otherwise we can kill
        // ourselves ungracefully:
        m_exiting = true;
        m_reactor->ExitImmediately("MDRecorder_HotSpot::InTradingEvent");
      }

      // subscribe to quotes for each sec def
      auto const& sec_defs = m_conn->GetSecDefsMgr()->GetAllSecDefs();
      cerr << "Subscribing to " << sec_defs.size() << " instruments" << endl;
      for (auto const& s : sec_defs) {
        cerr << "    Subscribing to " << s.m_Symbol.data() << endl;
        m_conn->SubscribeMktData(this, s, OrderBook::UpdateEffectT::L1Qty,
          false);
      }
    }

  private:
    //=======================================================================//
    // Mkt Data Call-Backs:                                                  //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OnOrderBookUpdate":                                                  //
    //-----------------------------------------------------------------------//
    void OnOrderBookUpdate
    (
      OrderBook const&          a_ob,
      bool                      UNUSED_PARAM(a_is_error),
      OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat
    )
    override
    {
      long lat_usec = (a_ts_strat - a_ts_recv).microseconds();

      auto const &instr = a_ob.GetInstr();
      auto idx = m_secID_to_idx.at(instr.m_SecID);

      // m_current_rec[idx].time = a_ts_exch;
      // SaveAggrOrderBook(a_ob, &m_current_rec[idx].rec);

      // m_md_stores[idx]->Write(m_current_rec[idx]);

      // // reset the aggression
      // m_current_rec[idx].rec.m_aggr = MDAggression();

      MDStoreBase<MDRecL1>::MDStoreRecord rec;
      rec.ts_exch      = a_ts_exch;
      rec.ts_recv      = a_ts_recv;
      rec.rec.bid      = double(a_ob.GetBestBidPx());
      rec.rec.ask      = double(a_ob.GetBestAskPx());
      // NB: HotSpotX Qtys are <QtyA,double>:
      rec.rec.bid_size = double(a_ob.GetBestBidQty<QtyTypeT::QtyA,double>());
      rec.rec.ask_size = double(a_ob.GetBestAskQty<QtyTypeT::QtyA,double>());

       m_md_stores_L1[idx]->Write(rec);

      auto date = a_ts_exch.to_string(utxx::DATE_TIME_WITH_NSEC);

      // NB: HotSpot Qtys are "doubles"s:
      cerr << "[" << date << "]: Latency="   << lat_usec << " usec: "
           << instr.m_Symbol.data()         << ": "
           << double(a_ob.GetBestBidQty())  << " @ "
           << double(a_ob.GetBestBidPx())   << " .. "
           << double(a_ob.GetBestAskQty())  << " @ "
           << double(a_ob.GetBestAskPx())   << endl;
    }

    //-----------------------------------------------------------------------//
    // "OnTradeUpdate":                                                      //
    //-----------------------------------------------------------------------//
    void OnTradeUpdate(Trade const& a_trade) override
    {
      long lat_usec = utxx::time_val::now_diff_usec(a_trade.m_recvTS);

      MDStoreBase<MDAggression>::MDStoreRecord rec;
      rec.ts_exch = a_trade.m_exchTS;
      rec.ts_recv = a_trade.m_recvTS;
      rec.rec.m_avgPx = double(a_trade.m_px) ;
      rec.rec.m_totQty = double(a_trade.m_qty);
      rec.rec.m_bidAggr = (a_trade.m_aggressor == FIX::SideT::Buy);

      auto idx = m_secID_to_idx.at(a_trade.m_instr->m_SecID);
      m_md_stores_trades[idx]->Write(rec);

      auto date = a_trade.m_exchTS.to_string(utxx::DATE_TIME_WITH_NSEC);

      cerr << "[" << date << "] OnTradeUpdate: Latency="
           << lat_usec                              << " usec: "
           << a_trade.m_instr->m_Symbol.data()      << ": Px="
           << double(a_trade.m_px)                  << ", Qty="
           << double(a_trade.m_qty)                 << ", Aggressor="
           << char(a_trade.m_aggressor)             << endl;
    }

  public:
    //-----------------------------------------------------------------------//
    // "OnSignal":                                                           //
    //-----------------------------------------------------------------------//
    void OnSignal(signalfd_siginfo const& /*a_si*/) override
    {
      cerr << "Got a Signal, Exiting Gracefully..." << endl;
      exit(0);
    }

    //-----------------------------------------------------------------------//
    // NB: Default Ctor is auto-generated                                    //
    //-----------------------------------------------------------------------//
  };
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  //-------------------------------------------------------------------------//
  // Params:                                                                 //
  //-------------------------------------------------------------------------//
  IO::GlobalInit({SIGINT});

  if (argc != 2)
  {
    cerr << "PARAMETER: ConfigFile.ini" << endl;
    return 1;
  }
  string iniFile(argv[1]);

  // Get the Propert Tree:
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(iniFile, pt);

  //-------------------------------------------------------------------------//
  // Construct the Reactor (but no Risk Manager):                            //
  //-------------------------------------------------------------------------//
  string logOutput  = pt.get<string>("Main.LogFile");
  auto   loggerShP  = IO::MkLogger  ("HotSpot_Test",  logOutput);
  bool   isProd     = pt.get<bool>("Main.IsProd");
  int    debugLevel = pt.get<int>("Main.DebugLevel");

  EPollReactor reactor(loggerShP.get(), debugLevel);  // Other args by default

  SecDefsMgr* secDefsMgr =
      SecDefsMgr::GetPersistInstance(isProd, "HotSpot_MDRec");

  // The Main Logger (NOT the separate loggers used by Connectors) can be extr-
  // acted from the Reactor and used:

  //-------------------------------------------------------------------------//
  // Instantiate the HotSpot FIX Connectors:                                 //
  //-------------------------------------------------------------------------//
  string sectionNameFIX("EConnector_FIX_HotSpot");

  boost::property_tree::ptree const& paramsFIX =
    GetParamsSubTree(pt, sectionNameFIX);

  EConnector_FIX_HotSpotFX_Gen* connFIX =
    new EConnector_FIX_HotSpotFX_Gen
        (&reactor, secDefsMgr, nullptr,  paramsFIX,  nullptr); // No PrimaryMDC

  //-------------------------------------------------------------------------//
  // Instantiate the Test Strategy:                                          //
  //-------------------------------------------------------------------------//
  MDRecorder_HotSpotFIX tst("HotSpot_MDRec", &reactor, loggerShP.get(),
      debugLevel, pt, connFIX, pt.get<string>("Main.MDStoreRoot"));

  //-------------------------------------------------------------------------//
  // Run the Connectors and Reactor:                                         //
  //-------------------------------------------------------------------------//
  try
  {
    // FIX1: HotSpot market data:
    connFIX->Subscribe(&tst);
    connFIX->Start();

    // Run it! Exit on any unhandled exceptions:
    bool busyWait = pt.get<bool>("Main.BusyWaitInEPoll");
    reactor.Run(true, busyWait);

    // Termination:
    connFIX->Stop();

    delete connFIX;
    connFIX = nullptr;
  }
  catch (exception const& exc)
  {
    cerr << "Exception: " << exc.what() << endl;
  }
  return 0;
}
