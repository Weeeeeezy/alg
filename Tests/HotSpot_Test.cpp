// vim:ts=2:et
//===========================================================================//
//                        "Tests/HotSpot_Test.cpp":                         //
//                    Test of HotSpot FIX Connectivity                      //
//===========================================================================//
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>
#include <signal.h>
#include <utxx/time_val.hpp>

#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.hpp"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/ITCH/EConnector_ITCH_HotSpotFX.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/Strategy.hpp"
#include "Protocols/FIX/Msgs.h"

# ifdef  __GNUC__
# pragma GCC   diagnostic push
# pragma GCC   diagnostic ignored "-Wunused-parameter"
# endif
# ifdef  __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-parameter"
# endif

using namespace std;
using namespace MAQUETTE;

using EConnHSFX =
  EConnector_ITCH_HotSpotFX<ITCH::DialectT::HotSpotFX_Gen>;

namespace
{
  //=========================================================================//
  // "TestStrategy_HotSpot" Class:                                           //
  //=========================================================================//
  class TestStrategy_HotSpot final: public Strategy
  {
  private:
    //=======================================================================//
    // Data Flds and Ctor:                                                   //
    //=======================================================================//
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using                     QR = double;

    mutable bool     m_exiting;
    EConnHSFX*       m_conn;
    SecDefD const&   m_instr;

  public:
    TestStrategy_HotSpot
    (
      string const&                      a_name,
      EPollReactor*                      a_reactor,
      spdlog::logger*                    a_logger,
      int                                a_debug_level,
      boost::property_tree::ptree const& a_pt,
      EConnHSFX*                         a_conn,
      SecDefD const&                     a_instr
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
      m_conn(a_conn),
      m_instr(a_instr) {}

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
      SecID             a_sec_id,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
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
        m_reactor->ExitImmediately("HotSpot_Test::InTradingEvent");
      }

      m_conn->SubscribeMktData(this, m_instr, OrderBook::UpdateEffectT::L1Qty,
          false);
    }

  public:
    //=======================================================================//
    // Mkt Data Call-Backs:                                                  //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OnOrderBookUpdate":                                                  //
    //-----------------------------------------------------------------------//
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
      //      if (utxx::unlikely(a_is_error))
      //      {
      //        m_exiting = true;
      //        m_reactor->ExitImmediately("HotSpot_Test::OnOrderBookUpdate: Error");
      //      }
      long lat_usec = (a_ts_strat - a_ts_recv).microseconds();

      // NB: HotSpot Qtys are <QtyA,double>:
      cerr << "OnOrderBookUpdate: Latency="       << lat_usec << " usec: "
           << a_ob.GetInstr().m_Symbol.data()     << ": "
           << double(a_ob.GetBestBidQty<QT,QR>()) << " @ "
           << a_ob.GetBestBidPx()                 << " .. "
           << double(a_ob.GetBestAskQty<QT,QR>()) << " @ "
           << a_ob.GetBestAskPx()                 << endl;
    }

    //-----------------------------------------------------------------------//
    // "OnTradeUpdate":                                                      //
    //-----------------------------------------------------------------------//
    void OnTradeUpdate(Trade const& a_trade) override
    {
      long lat_usec = utxx::time_val::now_diff_usec(a_trade.m_recvTS);

      cerr << "OnTradeUpdate: Latency=" << lat_usec << " usec: "
           << a_trade.m_instr->m_Symbol.data()      << ": Px="
           << a_trade.m_px                          << ", Qty="
           << double(a_trade.GetQty<QT,QR>())       << ", Aggressor="
           << char(a_trade.m_aggressor)             << endl;
    }

    //=======================================================================//
    // Order Mgmt Call-Backs:                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "OnOurTrade":                                                         //
    //-----------------------------------------------------------------------//
    void OnOurTrade(Trade const& a_tr) override
    {
      Req12 const* req = a_tr.m_ourReq;
      assert(req != nullptr);
      AOS   const* aos = req->m_aos;
      assert(aos != nullptr);

      char const* symbol = aos->m_instr->m_Symbol.data();
      bool isBuy         = aos->m_isBuy;

      cerr << "TRADED: OrderID="       << aos->m_id             << ' ' << symbol
           << (isBuy ? " B " : " S ")  << double(a_tr.GetQty<QT,QR>()) << '@'
           << double(a_tr.m_px)        << ' '
           << a_tr.m_execID.ToString() << endl;
    }

    //-----------------------------------------------------------------------//
    // "OnConfirm":                                                          //
    //-----------------------------------------------------------------------//
    void OnConfirm(Req12 const& a_req) override
    {
      AOS const* aos = a_req.m_aos;
      assert(aos != nullptr);
      cerr << "Confirmed: OrderID=" << aos->m_id << ", ReqID=" << a_req.m_id
           << endl;
    }

    //-----------------------------------------------------------------------//
    // "OnCancel":                                                           //
    //-----------------------------------------------------------------------//
    void OnCancel
    (
      AOS const&      a_aos,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    )
    override
    {
      assert(a_aos.m_isInactive);
      cerr << "CANCELLED: OrderID=" << a_aos.m_id << endl;
    }

    //-----------------------------------------------------------------------//
    // "OnOrderError":                                                       //
    //-----------------------------------------------------------------------//
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
      cerr << "ERROR: OrderID=" << aos->m_id  << ", ReqID="    << a_req.m_id
           << ": ErrCode="      << a_err_code << ": "          << a_err_text
           << ((aos->m_isInactive) ? ": ORDER FAILED"    : "")
           << (a_prob_filled       ? ": Probably Filled" : "") << endl;
    }

    //-----------------------------------------------------------------------//
    // "OnSignal":                                                           //
    //-----------------------------------------------------------------------//
    void OnSignal(signalfd_siginfo const& a_si) override
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

  // Symbols: Currently ALL:
  vector<string> symbolsVec;

  //-------------------------------------------------------------------------//
  // Construct the Reactor (but no Risk Manager):                            //
  //-------------------------------------------------------------------------//
  string logOutput  = pt.get<string>("Main.LogFile");
  auto   loggerShP  = IO::MkLogger  ("HotSpot_Prod",  logOutput);
  bool   isProd     = pt.get<bool>("Main.IsProd");
  int    debugLevel = pt.get<int>("Main.DebugLevel");

  EPollReactor reactor(loggerShP.get(), debugLevel);

  SecDefsMgr* secDefsMgr = SecDefsMgr::GetPersistInstance(isProd, "HotSpot_Prod");

  // The Main Logger (NOT the separate loggers used by Connectors) can be extr-
  // acted from the Reactor and used:

  //-------------------------------------------------------------------------//
  // Instantiate the HotSpot ITCH Connector:                                 //
  //-------------------------------------------------------------------------//
  //
  auto conn_params = GetParamsSubTree(pt, "EConnector_ITCH_HotSpot");

  EConnHSFX* conn =
    new EConnHSFX(&reactor, secDefsMgr, nullptr, nullptr, conn_params, nullptr);

  //-------------------------------------------------------------------------//
  // Instruments and Ccy Converters:                                         //
  //-------------------------------------------------------------------------//
  auto sec_defs = secDefsMgr->GetAllSecDefs();
  printf("There are %lu sec defs:\n", sec_defs.size());
  for (auto s : sec_defs) {
    printf("  %s\n", s.m_FullName.data());
  }

  SecDefD const& eurusd = secDefsMgr->FindSecDef("EUR/USD-HotSpotFX--SPOT");

  //-------------------------------------------------------------------------//
  // Instantiate the Test Strategy:                                          //
  //-------------------------------------------------------------------------//
  TestStrategy_HotSpot
    tst("HotSpot_Test", &reactor, loggerShP.get(), debugLevel, pt, conn,
    eurusd);

  //-------------------------------------------------------------------------//
  // Run the Connectors and Reactor:                                         //
  //-------------------------------------------------------------------------//
  try
  {
    // FIX1: HotSpot market data:
    conn->Subscribe(&tst);
    conn->Start();

    // Run it! Exit on any unhandled exceptions:
    bool busyWait = pt.get<bool>("Main.BusyWaitInEPoll");
    reactor.Run(true, busyWait);

    // Termination:
    conn->Stop();

    delete conn;
    conn = nullptr;
  }
  catch (exception const& exc)
  {
    cerr << "Exception: " << exc.what() << endl;
  }
  return 0;
}
# ifdef  __GNUC__
# pragma GCC   diagnostic pop
# endif
# ifdef  __clang__
# pragma clang diagnostic pop
# endif
