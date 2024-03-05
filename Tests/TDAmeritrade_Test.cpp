// vim:ts=2:et
//===========================================================================//
//                          "Tests/TD_Test.cpp":                           //
//                      Test of LMAX FIX Connectivity                        //
//===========================================================================//
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <utxx/time_val.hpp>

#include "Basis/BaseTypes.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Basis/IOUtils.hpp"
#include "Basis/PxsQtys.h"
#include "Connectors/H2WS/TDAmeritrade/EConnector_H1WS_TDAmeritrade.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/Strategy.hpp"

using namespace std;
using namespace MAQUETTE;

namespace {
//=========================================================================//
// "TestStrategy_TDA" Class:                                               //
//=========================================================================//
class TestStrategy_TDA final : public Strategy {
private:
  //=======================================================================//
  // Data Flds and Ctor:                                                   //
  //=======================================================================//
  constexpr static QtyTypeT QT = QtyTypeT::QtyA;
  using                     QR = long;

  mutable bool m_exiting;
  EConnector_H1WS_TDAmeritrade *m_conn;
  int m_num_heartbeats = 1;
  AOS const* m_aos1;
  AOS const* m_aos2;

  const SecDefD &m_inst1, &m_inst2, &m_inst3;

public:
  TestStrategy_TDA(string const &a_name, EPollReactor *a_reactor,
                   spdlog::logger *a_logger, int a_debug_level,
                   boost::property_tree::ptree const &UNUSED_PARAM(a_pt),
                   EConnector_H1WS_TDAmeritrade *a_conn)
      : Strategy(a_name, a_reactor, a_logger, a_debug_level, {SIGINT}),
        m_exiting(false), m_conn(a_conn),
        m_inst1(m_conn->GetSecDefsMgr()->FindSecDef("SPY-TDAmeritrade-EQUITY")),
        m_inst2(
            m_conn->GetSecDefsMgr()->FindSecDef("HSBC-TDAmeritrade-EQUITY")),
        m_inst3(
            m_conn->GetSecDefsMgr()->FindSecDef("UVXY-TDAmeritrade-EQUITY")) {}

  //=======================================================================//
  // Common Call-Back used by both MktData and OrdMgmt Connectors:         //
  //=======================================================================//
  //-----------------------------------------------------------------------//
  // "OnTradingEvent":                                                     //
  //-----------------------------------------------------------------------//
  // NB: This Call-Back can be invoked from ANY "EConnector" -- both MktData
  // and OrdMgmt:
  void OnTradingEvent(EConnector const &a_conn, bool a_on,
                      SecID UNUSED_PARAM(a_sec_id),
                      utxx::time_val UNUSED_PARAM(a_ts_exch),
                      utxx::time_val UNUSED_PARAM(a_ts_recv)) override {
    cerr << "OnTradingEvent: Connector=" << a_conn.GetName() << ": Trading "
         << (a_on ? "Started" : "Stopped") << endl;
    if (!a_on && !m_exiting) {
      // NB: Throw the "ExitRun" exception only once, otherwise we can kill
      // ourselves ungracefully:
      m_exiting = true;
      m_reactor->ExitImmediately("TDAmeritrade_Test::InTradingEvent");
    }

    m_conn->SubscribeMktData(this, m_inst1, OrderBook::UpdateEffectT::L1Px,
                             false);
    m_conn->SubscribeMktData(this, m_inst2, OrderBook::UpdateEffectT::L1Px,
                             false);
    m_conn->SubscribeMktData(this, m_inst3, OrderBook::UpdateEffectT::L1Px,
                             false);

    IO::FDInfo::TimerHandler timerH(
        [this](int UNUSED_PARAM(a_fd)) -> void { this->OnHeartbeat(); });

    // ErrorHandler:
    IO::FDInfo::ErrHandler errH([](int UNUSED_PARAM(a_fd),
                                   int UNUSED_PARAM(a_err_code),
                                   uint32_t UNUSED_PARAM(a_events),
                                   char const *a_msg) -> void {
      throw std::runtime_error("Heartbeat timer error: " + std::string(a_msg));
    });

    m_reactor->AddTimer("Heartbeat timer", 0, 3000, timerH, errH);
  }

  void OnHeartbeat() {
    // ++m_num_heartbeats;

    // if (m_num_heartbeats == 3) {
    //   Qty<QT,QR> qty(1L);
    //   m_aos1 = m_conn->NewOrder(this, m_inst3, FIX::OrderTypeT::Limit, true,
    //     PriceT(16.61), true, QtyAny(qty));
    // }

    if (m_num_heartbeats == 3) {
      Qty<QT,QR> qty(3L);
      m_aos1 = m_conn->NewOrder(this, m_inst1, FIX::OrderTypeT::Limit, false,
                                PriceT(371.13), false, QtyAny(qty));
    }

    if (m_num_heartbeats == 4) {
      Qty<QT,QR> qty(3L);
      m_conn->ModifyOrder(m_aos1, PriceT(373), false, QtyAny(qty));
    }

    if (m_num_heartbeats == 5) {
      Qty<QT,QR> qty(6L);
      m_conn->ModifyOrder(m_aos1, PriceT(373), false, QtyAny(qty));
    }

    if (m_num_heartbeats == 6) {
      Qty<QT,QR> qty(1L);
      m_aos2 = m_conn->NewOrder(this, m_inst2, FIX::OrderTypeT::Stop, true,
                                PriceT(25), false, QtyAny(qty));

      m_conn->ModifyOrder(m_aos1, PriceT(371), false, QtyAny(qty));
    }

    if (m_num_heartbeats == 7) {
      bool res = m_conn->CancelOrder(m_aos1);
      std::cerr << "Cancel: " << (res ? "successful" : "FAILED") << std::endl;

      Qty<QT,QR> qty(1L);
      m_conn->ModifyOrder(m_aos2, PriceT(26), false, QtyAny(qty));
    }

    if (m_num_heartbeats == 8) {
      m_conn->CancelOrder(m_aos2);
    }
  }

public:
  //=======================================================================//
  // Mkt Data Call-Backs:                                                  //
  //=======================================================================//
  //-----------------------------------------------------------------------//
  // "OnOrderBookUpdate":                                                  //
  //-----------------------------------------------------------------------//
  void OnOrderBookUpdate(OrderBook const &a_ob,
                         bool UNUSED_PARAM(a_is_error),
                         OrderBook::UpdatedSidesT  UNUSED_PARAM(a_sides),
                         utxx::time_val UNUSED_PARAM(a_ts_exch),
                         utxx::time_val a_ts_recv,
                         utxx::time_val a_ts_strat) override
  {
    long lat_usec = (a_ts_strat - a_ts_recv).microseconds();

    // NB: TDAmeritrade Qtys are <QtyA,long>:
    cerr << "OnOrderBookUpdate: Latency="                << lat_usec
         << " usec: " << a_ob.GetInstr().m_Symbol.data() << ": "
         << long(a_ob.GetBestBidQty<QT,QR>())            << " @ "
         << a_ob.GetBestBidPx()                          << " .. "
         << long(a_ob.GetBestAskQty<QT,QR>())            << " @ "
         << a_ob.GetBestAskPx()                          << endl;
  }

  //-----------------------------------------------------------------------//
  // "OnTradeUpdate":                                                      //
  //-----------------------------------------------------------------------//
  void OnTradeUpdate(Trade const &a_trade) override {
    long lat_usec = utxx::time_val::now_diff_usec(a_trade.m_recvTS);

    cerr << "OnTradeUpdate: Latency="     << lat_usec
         << " usec: " << a_trade.m_instr->m_Symbol.data()
         << ": Px="   << a_trade.m_px     << ", Qty="
         << long(a_trade.GetQty<QT,QR>()) << ", Aggressor="
         << char(a_trade.m_aggressor)     << endl;
  }

  //=======================================================================//
  // Order Mgmt Call-Backs:                                                //
  //=======================================================================//
  //-----------------------------------------------------------------------//
  // "OnOurTrade":                                                         //
  //-----------------------------------------------------------------------//
  void OnOurTrade(Trade const &a_tr) override {
    Req12 const *req = a_tr.m_ourReq;
    assert(req != nullptr);
    AOS const *aos = req->m_aos;
    assert(aos != nullptr);

    char const *symbol = aos->m_instr->m_Symbol.data();
    bool isBuy = aos->m_isBuy;

    cerr << "TRADED: OrderID=" << aos->m_id << ' ' << symbol
         << (isBuy ? " B " : " S ")  << long(a_tr.GetQty<QT,QR>()) << '@'
         << double(a_tr.m_px) << ' ' << a_tr.m_execID.ToString()   << endl;
  }

  //-----------------------------------------------------------------------//
  // "OnConfirm":                                                          //
  //-----------------------------------------------------------------------//
  void OnConfirm(Req12 const &a_req) override {
    AOS const *aos = a_req.m_aos;
    assert(aos != nullptr);
    cerr << "Confirmed: OrderID=" << aos->m_id << ", ReqID=" << a_req.m_id
         << endl;
  }

  //-----------------------------------------------------------------------//
  // "OnCancel":                                                           //
  //-----------------------------------------------------------------------//
  void OnCancel(AOS const &a_aos, utxx::time_val UNUSED_PARAM(a_ts_exch),
                utxx::time_val UNUSED_PARAM(a_ts_recv)) override {
    assert(a_aos.m_isInactive);
    cerr << "CANCELLED: OrderID=" << a_aos.m_id << endl;
  }

  //-----------------------------------------------------------------------//
  // "OnOrderError":                                                       //
  //-----------------------------------------------------------------------//
  void OnOrderError(Req12 const &a_req, int a_err_code, char const *a_err_text,
                    bool a_prob_filled, utxx::time_val UNUSED_PARAM(a_ts_exch),
                    utxx::time_val UNUSED_PARAM(a_ts_recv)) override {
    AOS const *aos = a_req.m_aos;
    assert(aos != nullptr);
    cerr << "ERROR: OrderID=" << aos->m_id << ", ReqID=" << a_req.m_id
         << ": ErrCode=" << a_err_code << ": " << a_err_text
         << ((aos->m_isInactive) ? ": ORDER FAILED" : "")
         << (a_prob_filled ? ": Probably Filled" : "") << endl;
  }

  //-----------------------------------------------------------------------//
  // "OnSignal":                                                           //
  //-----------------------------------------------------------------------//
  void OnSignal(signalfd_siginfo const & /*a_si*/) override {
    m_reactor->ExitImmediately(
        "MDRecorder_Binance::OnSignal: Shutting down gracefully...");
  }
};
} // namespace

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char *argv[]) {
  //-------------------------------------------------------------------------//
  // Params:                                                                 //
  //-------------------------------------------------------------------------//
  IO::GlobalInit({SIGINT});

  if (argc != 2) {
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
  string logOutput = pt.get<string>("Main.LogFile");
  auto loggerShP = IO::MkLogger("TDAmeritrade_Test", logOutput);
  bool isProd = true;
  int debugLevel = pt.get<int>("Main.DebugLevel");

  EPollReactor reactor(loggerShP.get(), debugLevel, false, false);

  SecDefsMgr *secDefsMgr =
      SecDefsMgr::GetPersistInstance(isProd, "TDAmeritrade_Test");

  // The Main Logger (NOT the separate loggers used by Connectors) can be extr-
  // acted from the Reactor and used:

  printf("Creating TD Connector...\n");

  EConnector_H1WS_TDAmeritrade *conn = new EConnector_H1WS_TDAmeritrade(
      &reactor, secDefsMgr, nullptr, nullptr,
      GetParamsSubTree(pt, "EConnector_H1WS_TDAmeritrade"), nullptr);

  printf("Creating TD Connector... done\n");

  //-------------------------------------------------------------------------//
  // Instruments and Ccy Converters:                                         //
  //-------------------------------------------------------------------------//
  auto sec_defs = secDefsMgr->GetAllSecDefs();
  printf("There are %lu sec defs:\n", sec_defs.size());
  for (auto s : sec_defs) {
    printf("  %s\n", s.m_FullName.data());
  }

  //-------------------------------------------------------------------------//
  // Instantiate the Test Strategy:                                          //
  //-------------------------------------------------------------------------//
  TestStrategy_TDA tst("TDAmeritrade_Test", &reactor, loggerShP.get(),
                       debugLevel, pt, conn);

  //-------------------------------------------------------------------------//
  // Run the Connectors and Reactor:                                         //
  //-------------------------------------------------------------------------//
  try {
    conn->Subscribe(&tst);
    conn->Start();

    // Run it! Exit on any unhandled exceptions:
    bool busyWait = pt.get<bool>("Main.BusyWaitInEPoll");
    printf("Run\n");
    reactor.Run(true, busyWait);

    // Termination:
    printf("Stop\n");
    conn->Stop();

    delete conn;
  } catch (exception const &exc) {
    cerr << "Exception: " << exc.what() << endl;
  }
  return 0;
}
