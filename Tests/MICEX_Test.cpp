// vim:ts=2:et
//===========================================================================//
//                            "Tests/MICEX_Test.cpp":                        //
//                   Test of MICEX FAST and FIX Connectivity                 //
//===========================================================================//
#include "Basis/ConfigUtils.hpp"
#include "Connectors/FAST/EConnector_FAST_MICEX.hpp"
#include "Connectors/FIX/EConnector_FIX.h"
#include "InfraStruct/SecDefsMgr.h"
#include "Connectors/EConnector_MktData.hpp"
#include "InfraStruct/Strategy.hpp"
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>
#include <signal.h>

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
using namespace FAST::MICEX;

namespace
{
  //=========================================================================//
  // "TestStrategy_MICEX" Class:                                             //
  //=========================================================================//
  class TestStrategy_MICEX final: public Strategy
  {
  private:
    //=======================================================================//
    // Data Flds and Ctor:                                                   //
    //=======================================================================//
    constexpr static QtyTypeT QT = QtyTypeT::QtyA;
    using                     QR = long;
    mutable bool m_exiting;

  public:
    TestStrategy_MICEX
    (
      string const&                      a_name,
      EPollReactor*                      a_reactor,
      spdlog::logger*                    a_logger,
      int                                a_debug_level,
      boost::property_tree::ptree const& a_pt
    )
    : Strategy
      (
        a_name,
        a_reactor,
        a_logger,
        a_debug_level,
        { SIGINT }
      ),
      m_exiting(false)
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
        m_reactor->ExitImmediately("MICEX_Test::InTradingEvent");
      }
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
      utxx::time_val            UNUSED_PARAM(a_ts_exch),
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat
    )
    override
    {
      if (utxx::unlikely(a_is_error))
      {
        m_exiting = true;
        m_reactor->ExitImmediately("MICEX_Test::OnOrderBookUpdate: Error");
      }
      long lat_usec = (a_ts_strat - a_ts_recv).microseconds();

      // NB: MICEX Qtys are <QtyA, long>:
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

      cerr << "TRADED: OrderID="       << aos->m_id           << ' ' << symbol
           << (isBuy ? " B " : " S ")  << long(a_tr.GetQty<QT,QR>()) << '@'
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

  // Instance-selecting params:
  string sectionNameFAST("EConnector_FAST_MICEX");
  ProtoVerT   ver =
  ProtoVerT::from_string(pt.get<string>(sectionNameFAST + ".ProtoVer"));

  EnvT  env =
  EnvT::from_string(pt.get<string>(sectionNameFAST + ".Env"));

  MICEX::AssetClassT ac  =
  MICEX::AssetClassT::from_string(pt.get<string>
                                 (sectionNameFAST + ".AssetClass"));
  // Symbols: Currently ALL:
  vector<string> symbolsVec;

  //-------------------------------------------------------------------------//
  // Construct the Reactor (but no Risk Manager):                            //
  //-------------------------------------------------------------------------//
  string logOutput  = pt.get<string>("Main.LogFile");
  auto   loggerShP  = IO::MkLogger  ("MICEX_Test",  logOutput);
  bool   isProd     = (env.to_string().substr(0, 4) == "Prod");
  int    debugLevel = 2;   // XXX: Fixed as yet

  EPollReactor reactor(loggerShP.get(), debugLevel);

  SecDefsMgr* secDefsMgr = SecDefsMgr::GetPersistInstance(isProd, "MICEX_Test");

  // The Main Logger (NOT the separate loggers used by Connectors) can be extr-
  // acted from the Reactor and used:

  //-------------------------------------------------------------------------//
  // Instantiate the MICEX FAST Connector:                                   //
  //-------------------------------------------------------------------------//
  // NB: We must pass the pertinent sub-tree of "pt" to the Connector Ctor:
  boost::property_tree::ptree const& paramsFAST =
    GetParamsSubTree(pt, sectionNameFAST);

  EConnector_MktData* connFAST =
    (ver ==  ProtoVerT::Curr && ac == MICEX::AssetClassT::FX)
    ? static_cast<EConnector_MktData*>
      (new EConnector_FAST_MICEX<ProtoVerT::Curr, MICEX::AssetClassT::FX>
           (env, &reactor, secDefsMgr, nullptr, nullptr, paramsFAST, nullptr))
    :
    (ver ==  ProtoVerT::Curr && ac == MICEX::AssetClassT::EQ)
    ? static_cast<EConnector_MktData*>
      (new EConnector_FAST_MICEX<ProtoVerT::Curr, MICEX::AssetClassT::EQ>
           (env, &reactor, secDefsMgr, nullptr, nullptr, paramsFAST, nullptr))
    : nullptr;

  // This should not happen:
  assert(connFAST != nullptr);

  //-------------------------------------------------------------------------//
  // Instantiate the MICEX and AlfaFIX2 FIX Connectors:                      //
  //-------------------------------------------------------------------------//
  string sectionNameFIX1 =
    "EConnector_FIX_MICEX_" + MICEX::AssetClassT::to_string(ac);

  boost::property_tree::ptree const& paramsFIX1 =
    GetParamsSubTree(pt, sectionNameFIX1);

  // OK,  we can now create the FIX Connector. XXX: At the moment, the OrdMgmt
  // methods (as opposed to SubscrMgmt) are NOT virtual for efficiency reasons,
  // so use a type-specific ptr in the LHS here:
  //
  EConnector_FIX_MICEX* connFIX1 =     // OMC!
    new EConnector_FIX_MICEX
        (&reactor, secDefsMgr, nullptr, nullptr, paramsFIX1);

  string sectionNameFIX2("EConnector_FIX_AlfaFIX2_MKT");
  string sectionNameFIX3("EConnector_FIX_AlfaFIX2_ORD");

  boost::property_tree::ptree const& paramsFIX2 =
    GetParamsSubTree(pt, sectionNameFIX2);

  boost::property_tree::ptree const& paramsFIX3 =
    GetParamsSubTree(pt, sectionNameFIX3);

  // AlfaFIX2 MDC:
  EConnector_FIX_AlfaFIX2* connFIX2 =
    new EConnector_FIX_AlfaFIX2
        (&reactor, secDefsMgr, nullptr,  nullptr, paramsFIX2,  nullptr);
        // No PrimaryMDC

  // AlfaFIX2 OMC:
  EConnector_FIX_AlfaFIX2* connFIX3 =
    new EConnector_FIX_AlfaFIX2
        (&reactor, secDefsMgr, nullptr, nullptr, paramsFIX3);

  //-------------------------------------------------------------------------//
  // Instantiate the Test Strategy:                                          //
  //-------------------------------------------------------------------------//
  TestStrategy_MICEX
    tst("MICEX_Test", &reactor, loggerShP.get(), debugLevel, pt);

  //-------------------------------------------------------------------------//
  // Instruments and Ccy Converters:                                         //
  //-------------------------------------------------------------------------//
  SecDefD const& instr1 =
    secDefsMgr->FindSecDef("USD000UTSTOM-MICEX-CETS-TOM");
  SecDefD const& instr2 =
    secDefsMgr->FindSecDef("USD/RUB-AlfaFIX2--TOM" );

  //-------------------------------------------------------------------------//
  // Run the Connectors and Reactor:                                         //
  //-------------------------------------------------------------------------//
  try
  {
    // FAST: MICEX MktData:
    connFAST->SubscribeMktData
    (
      &tst,
      instr1,
      OrderBook::UpdateEffectT::L1Qty,
      true       // RegisterInstrRisks
    );
    connFAST->Start();

    // FIX1: MICEX OrdMgmt:
    connFIX1->Subscribe(&tst);
    connFIX1->Start();

    // FIX2: AlfaFIX2 MktData -- just start it; MktData subscription can only be
    // done when it becomes active:
    connFIX2->Start();

    while (!(connFIX2->IsActive()))
      sleep(1);
    connFIX2->SubscribeMktData
    (
      &tst,
      instr2,
      OrderBook::UpdateEffectT::L1Qty,
      true       // RegisterInstrRisks
    );

    // FIX3: AlfaFIX2 OrdMgmt:
    connFIX3->Subscribe(&tst);
    connFIX3->Start();

    // Run it! Exit on any unhandled exceptions:
    bool busyWait = pt.get<bool>("Main.BusyWaitInEPoll");
    reactor.Run(true, busyWait);

    // Termination:
    connFAST->Stop();
    connFIX1->Stop();
    connFIX2->Stop();
    connFIX3->Stop();

    delete connFAST;
    delete connFIX1;
    delete connFIX2;
    delete connFIX3;

    connFAST = nullptr;
    connFIX1 = nullptr;
    connFIX2 = nullptr;
    connFIX3 = nullptr;
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
