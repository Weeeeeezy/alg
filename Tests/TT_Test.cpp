// vim:ts=2:et
//===========================================================================//
//                           "Tests/TT_Test.cpp":                            //
//                       Test of TT FIX Connectivity                         //
//===========================================================================//
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>
#include <signal.h>
#include <utxx/time_val.hpp>

#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.hpp"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/FIX/EConnector_FIX.h"
#include "Connectors/OrderBook.h"
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

namespace
{
  //=========================================================================//
  // "TestStrategy_TT" Class:                                                //
  //=========================================================================//
  class TestStrategy_TT final: public Strategy
  {
  private:
    //=======================================================================//
    // Data Flds and Ctor:                                                   //
    //=======================================================================//
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using                     QR = long;

    mutable bool       m_exiting;
    EConnector_FIX_TT* m_conn;
    SecDefD const&     m_instr1; //, &m_instr2, &m_instr3;

    utxx::time_val     m_start_time;

    //AOS *m_order1 = nullptr;
    //AOS *m_order2 = nullptr;

    std::string        m_sec_def_type;
    std::string        m_sec_exch;

  public:
    TestStrategy_TT
    (
      string const&                      a_name,
      EPollReactor*                      a_reactor,
      spdlog::logger*                    a_logger,
      int                                a_debug_level,
      boost::property_tree::ptree const& a_pt,
      EConnector_FIX_TT*         a_conn,
      std::string const&                 a_sec_def_type,
      std::string const&                 a_sec_exch,
      SecDefD const&                     a_instr1
      // SecDefD const&                     a_instr2,
      // SecDefD const&                     a_instr3
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
      m_instr1(a_instr1),
      // m_instr2(a_instr2),
      // m_instr3(a_instr3)
      m_sec_def_type(a_sec_def_type),
      m_sec_exch(a_sec_exch)
    {}

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
        m_reactor->ExitImmediately("TT_Test::InTradingEvent");
      }

      // auto handle_sec_def = [] (FIX::SecurityDefinition const& sec_list) {
      //   //  printf("SecurityDefinition, request id: '%s' got %i securities:\n",
      //   //     sec_list.m_SecurityReqID, sec_list.m_NoRelatedSym);

      //   // for (int i = 0; i < sec_list.m_NoRelatedSym; ++i)
      //   // {
      //   //   auto const& sec = sec_list.m_Instruments[i];
      //   //   printf("  %10s: quantity scale = %i, price scale = %i, "
      //   //     "amount scale = %i, status = %s\n", sec.m_Symbol.data(),
      //   //     sec.m_QuantityScale, sec.m_PriceScale, sec.m_AmountScale,
      //   //     sec.m_InstrumentStatus == FIX::InstrumentStatusT::Enabled ?
      //   //         "Enabled" : "Disabled");
      //   // }
      // };

      // // Get the security list
      // m_conn->GetSecurityDefinition(handle_sec_def, m_sec_def_type,
      //   m_sec_exch);

      m_conn->SubscribeMktData(this, m_instr1, OrderBook::UpdateEffectT::L1Qty,
        false);

      m_start_time = utxx::now_utc();

      // printf("starting data injection\n");
      // m_conn->InjectData("/home/jlippuner/bl_spartan/maquette/tt/Logs_trades_and_book/msgs_01");
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
      if (utxx::unlikely(a_is_error))
      {
        // m_exiting = true;
        // m_reactor->ExitImmediately("TT_Test::OnOrderBookUpdate: Error");
      }
      long lat_usec = (a_ts_strat - a_ts_recv).microseconds();

      // NB: TT Qtys are <QtyA,long>:
      cerr << "OnOrderBookUpdate: Latency="     << lat_usec << " usec: "
           << a_ob.GetInstr().m_Symbol.data()   << ": "
           << long(a_ob.GetBestBidQty<QT,QR>()) << " @ "
           << a_ob.GetBestBidPx()               << " .. "
           << long(a_ob.GetBestAskQty<QT,QR>()) << " @ "
           << a_ob.GetBestAskPx()               << endl;
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
           << long(a_trade.GetQty<QT,QR>())         << ", Aggressor="
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

      cerr << "TRADED: OrderID="       << aos->m_id  << ' ' << symbol
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
           << ((aos->m_isInactive) ? ": ORDER FAILED" : "")
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
  auto   loggerShP  = IO::MkLogger  ("TT_Test",  logOutput);
  bool   isProd     = pt.get<bool>("Main.IsProd");
  int    debugLevel = pt.get<int>("Main.DebugLevel");   // XXX: Fixed as yet

  std::string sec_def_type = pt.get<std::string>("Main.SecDefType");
  std::string sec_exch = pt.get<std::string>("Main.SecExch");

  EPollReactor reactor(loggerShP.get(), debugLevel);

  SecDefsMgr* secDefsMgr = SecDefsMgr::GetPersistInstance(isProd, "TT_Test");

  // The Main Logger (NOT the separate loggers used by Connectors) can be extr-
  // acted from the Reactor and used:

  //-------------------------------------------------------------------------//
  // Instantiate the TT FIX Connectors:                                      //
  //-------------------------------------------------------------------------//
  string sectionNameFIX("EConnector_FIX_TT");

  boost::property_tree::ptree const& paramsFIX =
    GetParamsSubTree(pt, sectionNameFIX);

  EConnector_FIX_TT* connFIX =
    new EConnector_FIX_TT(&reactor, secDefsMgr, nullptr, nullptr,
      paramsFIX,  nullptr); // No PrimaryMDC

  //-------------------------------------------------------------------------//
  // Instruments and Ccy Converters:                                         //
  //-------------------------------------------------------------------------//
  auto sec_defs = secDefsMgr->GetAllSecDefs();
  const SecDefD& cl = secDefsMgr->FindSecDef("CL-TT-FUT-20210322");

  printf("There are %lu sec defs:\n", sec_defs.size());
  for (const auto& s : sec_defs) {
    printf("  %s\n", s.m_FullName.data());
  }

  //-------------------------------------------------------------------------//
  // Instantiate the Test Strategy:                                          //
  //-------------------------------------------------------------------------//
  TestStrategy_TT
    tst("TT_Test", &reactor, loggerShP.get(), debugLevel, pt, connFIX,
      sec_def_type, sec_exch, cl);

  //-------------------------------------------------------------------------//
  // Run the Connectors and Reactor:                                         //
  //-------------------------------------------------------------------------//
  try
  {
    // FIX1: TT market data:
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
# ifdef  __GNUC__
# pragma GCC   diagnostic pop
# endif
# ifdef  __clang__
# pragma clang diagnostic pop
# endif
