// vim:ts=2:et
//===========================================================================//
//                          "Tools/LatencyTest.cpp":                         //
//===========================================================================//
// #include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.h"
// #include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_MDC.h"
#include "Connectors/H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.h"
#include "Connectors/H2WS/BitMEX/EConnector_WS_BitMEX_MDC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_OMC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
// #include "Connectors/H2WS/FTX/EConnector_WS_FTX_MDC.h"
// #include "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_OMC.h"
// #include "Connectors/H2WS/Huobi/EConnector_WS_Huobi_MDC.h"
#include "Basis/ConfigUtils.hpp"
#include "Basis/Maths.hpp"
#include "Connectors/OrderBook.h"
#include "Connectors/OrderBook.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/Strategy.hpp"
#include <utxx/config.h>
#include <utxx/error.hpp>
#include <spdlog/async.h>
#include <utxx/compiler_hints.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/container/static_vector.hpp>
#include <cstdlib>
#include <cstdio>
#include "Basis/Macros.h"
#include <boost/algorithm/string.hpp>

using namespace MAQUETTE;
using namespace std;

namespace
{
  //=========================================================================//
  // "LatencyTestStrat" Class:                                               //
  //=========================================================================//
  template <class MDC, class OMC = EmptyT>
  class LatencyTestStrat final: public Strategy
  {
  private:
    constexpr static bool HasOMC = !is_same_v<OMC, EmptyT>;

    // Setup:
    SecDefsMgr*               m_secDefsMgr;
    MDC*                      m_mdc;         // Primary
    OMC*                      m_omc;

    string                    m_instrName;
    OrderBook const*          m_ob;
    bool                      m_isActive;

    // Status of Connectors:
    long                      m_updatesCount;
    int                       m_signalsCount;

    // Order-Related Params:
    AOS const*                m_aoses;        // Our Order(s) (normally just 1)
    PriceT                    m_px;           // If PxT is a NUMBER
    double                    m_sQty;         // Signed Qty
    FIX::TimeInForceT         m_timeInForce;
    bool                      m_measureRoundTrip;
    bool                      m_measureMktData;
    long                      m_delay;        // In MktData counts
    bool                      m_testBufferedOrders;
    bool                      m_testPipeLining;

    utxx::time_val m_cancelledAt = utxx::now_utc();

    // MktData:
    string                    m_mktDataFile;  // For Recording
    FILE*                     m_MDFD;         // As above, when opened...
    PriceT                    m_prevBidPx;
    PriceT                    m_prevAskPx;

    // Default Ctor is deleted:
    LatencyTestStrat() =  delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    LatencyTestStrat
    (
      EPollReactor*                a_reactor,
      spdlog::logger*              a_logger,
      boost::property_tree::ptree& a_pt     // XXX: Mutable!
    )
    :
      //---------------------------------------------------------------------//
      // Parent Class:                                                       //
      //---------------------------------------------------------------------//
      Strategy
      (
        "LatencyTest",
        a_reactor,
        a_logger,
        a_pt.get<int>("Exec.DebugLevel"),
        { SIGINT }
      ),
      //---------------------------------------------------------------------//
      // SecDefsMgr and RiskMgr:                                             //
      //---------------------------------------------------------------------//
      m_secDefsMgr        (SecDefsMgr::GetPersistInstance
        (a_pt.get<string> ("Main.Env") == "Prod", "")),

      //---------------------------------------------------------------------//
      // Other Flds:                                                         //
      //---------------------------------------------------------------------//
      m_mdc               (nullptr),
      m_omc               (nullptr),
      m_isActive          (false),
      // Status of Connectors:
      m_updatesCount      (0),
      m_signalsCount      (0),
      // Order-Related Params:
      m_aoses             (nullptr),
      m_px                (),       // NaN
      m_sQty              (0),
      m_timeInForce       (FIX::TimeInForceT::UNDEFINED),
      m_measureRoundTrip  (a_pt.get<bool>  ("Main.RoundTrip", false)),
      m_measureMktData    (a_pt.get<bool>  ("Main.MktData", false)),
      m_delay             (a_pt.get<long>  ("Main.DelayTicks")),
      m_testBufferedOrders(a_pt.get<bool>  ("OMC.TestBufferedOrders", false)),
      m_testPipeLining    (a_pt.get<bool>  ("OMC.TestPipeLining",     false)),
      //
      // MktData:
      m_mktDataFile       (a_pt.get<string>("Main.MktDataFile",       "")),
      m_MDFD              (m_mktDataFile.empty()
                          ? nullptr
                          : fopen(m_mktDataFile.data(), "a")),
      m_prevBidPx         (),       // NaN
      m_prevAskPx         ()        // NaN
    {
      //---------------------------------------------------------------------//
      // Memoise the Order Params:                                           //
      //---------------------------------------------------------------------//
      // Check the Recording:
      if (utxx::unlikely(!m_mktDataFile.empty() && m_MDFD == nullptr))
        throw utxx::badarg_error
              ("LatencyTest::Ctor: Could not open the MktData Output File: ",
               m_mktDataFile);

      // Quantity (signed, to indicate Buy or Sell):
      m_sQty = a_pt.get<double>("Main.Qty");
      if (utxx::unlikely(m_sQty == 0))
        throw utxx::badarg_error("LatencyTestStrat::Ctor: Invalid Qty=0");

      // TimeInForce: Currently, only "Day", "IOC" and "GTC" are supported:
      string tifStr = a_pt.get<string>("Main.TimeInForce");
      m_timeInForce =
        (tifStr == "Day")
        ? FIX::TimeInForceT::Day :
        (tifStr == "IOC")
        ? FIX::TimeInForceT::ImmedOrCancel  :
        (tifStr == "GTC")
        ? FIX::TimeInForceT::GoodTillCancel :
          throw utxx::badarg_error
                ("LatencyTestStrat::Ctor: Invalid TimeInForce=", tifStr);

      //---------------------------------------------------------------------//
      // Create the Connectors (MDC and OMC):                                //
      //---------------------------------------------------------------------//
      string exchange       = a_pt.get<string>("Main.Exchange");
      string env            = a_pt.get<string>("Main.Env");
      m_instrName      = a_pt.get<string>("Main.InstrName");

      bool        isProd    = (env.substr(0, 4)  == "Prod");

      //---------------------------------------------------------------------//
      // Get the Symbols:                                                    //
      //---------------------------------------------------------------------//
      // The Full Designation of the Main Traded Instrument:

      auto const& mdcParams = GetParamsSubTree(a_pt, MDC::Name);
      std::string acctKeyMDC   = mdcParams.template
         get<std::string>("AccountPfx") + "-" + MDC::Name + "-" +
         (isProd ? "Prod" : "Test");
      SetAccountKey(&mdcParams, acctKeyMDC);

      m_mdc = new MDC(m_reactor, m_secDefsMgr, nullptr,
                      nullptr, mdcParams,    nullptr);
      if constexpr (HasOMC)
      {
        auto const& omcParams = GetParamsSubTree(a_pt, OMC::Name);
        std::string acctKeyOMC = omcParams.template
          get<std::string>("AccountPfx") + "-" + OMC::Name + "-" +
          (isProd ? "Prod" : "Test");

        SetAccountKey(&omcParams, acctKeyOMC);
        m_omc = new OMC(m_reactor, m_secDefsMgr, nullptr,
                        nullptr, omcParams);
      }

      //---------------------------------------------------------------------//
      // Subscribe to receive notifications from both MDC & OMC Connectrors: //
      //---------------------------------------------------------------------//
      // (This should be done immediately upon the Connectors construction):
      assert(m_mdc != nullptr);
      m_mdc->Subscribe(this);

      if constexpr (HasOMC)
      {
        assert(m_omc != nullptr);

        m_omc->Subscribe(this);
        m_omc->LinkMDC(m_mdc);
      }

      // But note that the actual MktData subscription is delayed until   the
      // MktData connector is activated (this does not matter much for a FAST
      // MDC, but important for a FIX MDC because in the latter case, the ac-
      // tual subscription msg needs to be sent out!)
    }

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~LatencyTestStrat() noexcept override
    {
      try
      {
        //-------------------------------------------------------------------//
        // Stop and Remove the Connectors:                                   //
        //-------------------------------------------------------------------//
        if (m_mdc != nullptr)
        {
          m_mdc->UnSubscribe(this);
          m_mdc->Stop();
          delete m_mdc;
          m_mdc = nullptr;
        }
        if constexpr (HasOMC)
        {
          if (m_omc != nullptr)
          {
            m_omc->UnSubscribe(this);
            m_omc->Stop();
            delete m_omc;
            m_omc = nullptr;
          }
        }
        //-------------------------------------------------------------------//
        // Reset the SecDefsMgr and the RiskMgr ptrs:                        //
        //-------------------------------------------------------------------//
        // NB: The corresp objs themselves are  allocated in ShM, don't delete
        // them!
        m_secDefsMgr = nullptr;

        // Also close the MktData Output File:
        if (m_MDFD  != nullptr)
        {
          fclose(m_MDFD);
          m_MDFD = nullptr;
        }
        // Other components are finalised automatically...
      }
      catch (exception& exn)
      {
        cerr << "EXCEPTION in ~LatencyTestStart(): " << exn.what() << endl;
      }
    }

    //=======================================================================//
    // "Run":                                                                //
    //=======================================================================//
    void Run()
    {
      // Start the Connectors:
      if (m_mdc != nullptr)
        m_mdc->Start();

      if constexpr (HasOMC)
        if (m_omc != nullptr)
          m_omc->Start();

      // Enter the Inifinite Event Loop, IFF at least one of the Connectors
      // requires the BusyWait Mode:
      bool exitOnExc = true;
      bool busyWait  =         (m_mdc != nullptr && m_mdc->UseBusyWait());
      if constexpr (HasOMC)
        busyWait = busyWait || (m_omc != nullptr && m_omc->UseBusyWait());

      m_reactor->Run(exitOnExc, busyWait);
    }
    //=======================================================================//
    // "OnTradingEvent" Call-Back:                                           //
    //=======================================================================//
    void OnTradingEvent
    (
      EConnector const& a_conn,
      bool              a_on,
      SecID             UNUSED_PARAM(a_sec_id),
      utxx::time_val    UNUSED_PARAM(a_ts_exch),
      utxx::time_val    a_ts_recv
    )
    override
    {
      utxx::time_val stratTime = utxx::now_utc ();
      // TradingEvent latency:
      long md_lat = (stratTime - a_ts_recv).microseconds();

      //---------------------------------------------------------------------//
      if (&a_conn == m_mdc) // If it is an MDC, it must be MDC1
      //---------------------------------------------------------------------//
      {
        assert(m_mdc != nullptr);
        if (m_mdc->IsActive())  // Do it only once
        {
          // Subscribe to MktData now:
          // empty OrderBook, but in general it is OK):
          //
          SecDefD const& instr = m_secDefsMgr->FindSecDef(m_instrName.data());

          // Subscribe MktData for this "instr". Normnally, we just need L1Px
          // data, but if MktData Recording is requested, it would be L2:

          OrderBook::UpdateEffectT level =
            (m_MDFD == nullptr)
            ? OrderBook::UpdateEffectT::L1Px
            : OrderBook::UpdateEffectT::L2;

          // XXX: RegisterInstrRisks=true (though for some Venues, a better
          // RiskMgmt policy would be preferred):
          m_mdc->SubscribeMktData(this, instr, level, true);

          // Get the corresp OrderBook from the Connector -- after Subscrip-
          // tion, it must exist:
          m_ob = &(m_mdc->GetOrderBook(instr));
        }
      }
      //---------------------------------------------------------------------//
      // In any case, output a msg:                                          //
      //---------------------------------------------------------------------//
      cerr << "\nOnTradingEvent: Connector=" << a_conn.GetName() << ": Latency="
           << md_lat << " usec: Trading "    << (a_on ? "Started" : "Stopped")
           << ": MDCActive=" << (m_mdc != nullptr && m_mdc->IsActive());

      if constexpr (HasOMC)
        cerr << ", OMCActive=" << (m_omc  != nullptr && m_omc ->IsActive());

      cerr << endl;

      if constexpr  (HasOMC)
      {
        //-------------------------------------------------------------------//
        // Place an Order Now?                                               //
        //-------------------------------------------------------------------//
        // With some Px Types, we can place an order immediately when the OMC be-
        // comes Active:
        //
        if (utxx::unlikely
            (&a_conn == m_omc  && a_on && m_omc->IsActive()))
        {
        } else
          //-----------------------------------------------------------------//
          // Should we stop now?                                             //
          //-----------------------------------------------------------------//
          // Do so if both Connectors are now inactive (either due to a previo-
          // usly-received termination signal, or because of an error):
          if (utxx::unlikely
              ((m_mdc == nullptr || !(m_mdc->IsActive())) &&
               (m_omc == nullptr || !(m_omc->IsActive())) ))
            m_reactor->ExitImmediately("LatencyTestStrat::OnTradingEvent");
      } else
          if (utxx::unlikely(
                m_mdc == nullptr || !(m_mdc->IsActive())) )
            m_reactor->ExitImmediately("LatencyTestStrat::OnTradingEvent");
    }

  private:
    //=======================================================================//
    // "OnOrderBookUpdate" Call-Back:                                        //
    //=======================================================================//
    void OnOrderBookUpdate
    (
      OrderBook const&,
      bool,
      OrderBook::UpdatedSidesT,
      utxx::time_val,
      utxx::time_val,
      utxx::time_val a_ts_strat
    )
    override
    {
      if (m_measureRoundTrip && !m_cancelledAt.empty() &&
          a_ts_strat.diff_msec(m_cancelledAt) > 1000)
        PlaceOrder();
    }

    //=======================================================================//
    // "OnTradeUpdate" Call-Back:                                            //
    //=======================================================================//
    // NB: This method is currently provided because it is required by virtual
    // interface, but  is only used for data recording:
    //
    void OnTradeUpdate(Trade const& a_tr) override
    {
      if (m_measureMktData)
      {
        cerr << "Trades:" << a_tr.m_recvTS.diff_msec(a_tr.m_exchTS);
        cerr << "ms" << endl;
      }
    }

  public:
    //=======================================================================//
    // "OnOurTrade" Call-Back:                                               //
    //=======================================================================//
    void OnOurTrade(Trade const&) override
    {
    }

    //=======================================================================//
    // "OnConfirm" Call-Back:                                                //
    //=======================================================================//
#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#   endif
    void OnConfirm(Req12 const& a_req) override
    {
      if (m_measureRoundTrip)
      {
        cerr << "Order Confirmed, ";
        AOS const* aos = a_req.m_aos;
        assert(aos != nullptr);
        cerr << "Roundtrip: ";
        cerr << setw(10) <<
                        (a_req.m_ts_conf_conn - a_req.m_ts_sent).microseconds();
        cerr << "us" << endl;

        if constexpr (HasOMC)
        {
          utxx::time_val ts_exch = a_req.m_ts_md_exch;
          utxx::time_val ts_recv = a_req.m_ts_md_conn;
          utxx::time_val ts_stra = a_req.m_ts_md_strat;

          m_omc->CancelOrder(aos, ts_exch, ts_recv, ts_stra);
        }
      }
    }
#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#   endif

    //=======================================================================//
    // "OnCancel" Call-Back:                                                 //
    //=======================================================================//
    void OnCancel
    (
      AOS const&      DEBUG_ONLY(a_aos),
      utxx::time_val  UNUSED_PARAM(a_ts_exch),
      utxx::time_val  a_ts_recv
    )
    override
    {
      cerr << "Order Cancelled, Placing" << endl;
      assert(a_aos.m_isInactive);
      m_cancelledAt = a_ts_recv;
      // PlaceOrder(a_ts_exch, a_ts_recv, utxx::now_utc());
    }

    //=======================================================================//
    // "OnOrderError" Call-Back:                                             //
    //=======================================================================//
    void OnOrderError
    (
      Req12 const&    a_req,
      int             a_err_code,
      char  const*    a_err_text,
      bool            a_prob_filled,
      utxx::time_val,
      utxx::time_val
    )
    override
    {
      AOS const* aos  = a_req.m_aos;
      assert(aos != nullptr);

      cerr << "\nERROR: OrderID="  << aos->m_id  << ", ReqID=" << a_req.m_id
           << ": ErrCode="         << a_err_code << ": "       << a_err_text
           << ((aos->m_isInactive) ? ": ORDER FAILED" : "")
           << (a_prob_filled       ? ": Probably Filled" : "") << endl;

      // XXX: Again, Do NOT reset "m_aoses" back to empty -- this may trigger
      // yet another automatic order placement, which is NOT what we want!
    }

    //=======================================================================//
    // "OnSignal" Call-Back:                                                 //
    //=======================================================================//
    void OnSignal(signalfd_siginfo const& a_si) override
    {
      // Cancel all Active Orders:
      // bool cancelled = false;
      AOS const* aos = m_aoses;
      assert(aos != nullptr);
      if (utxx::likely(!(aos->m_isInactive) && aos->m_isCxlPending == 0))
      {
        bool done = false;

        if constexpr (HasOMC)
        {
          assert(m_omc != nullptr); // Of course -- we got an AOS!
          utxx::time_val now = utxx::now_utc();
          done = m_omc->CancelOrder(aos, now, now, now);
        }

        cerr << "\nGot a Signal (" << a_si.ssi_signo
             << "), Cancelling the OrderID=" << aos->m_id
             << (utxx::unlikely(!done) ? ": FAILED???" : "") << endl;
        // cancelled = done;
      }
      // Done for now. XXX: Here we do NOT increment "m_signalsCount" yet:
      if (m_mdc != nullptr)
        m_mdc->Stop();

      if constexpr (HasOMC)
        if (m_omc  != nullptr)
          m_omc->Stop();

      // Otherwise: Increment the counter and proceed with Graceful or Immedia-
      // te Shut-Down:

      cerr << "\nGot a Signal (" << a_si.ssi_signo << "), Count="
           << m_signalsCount     << ", Exiting "
           << ((m_signalsCount == 1) ? "Gracefully" : "Immediately") << "..."
           << endl;

        // Throw the "ExitRun" exception to exit the Main Event Loop (if we are
        // not in that loop, it will be handled safely anyway):
        m_reactor->ExitImmediately("LatencyTestStrat::OnSignal");
    }

    //=======================================================================//
    // "PlaceOrder":                                                         //
    //=======================================================================//
#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#   endif
    void PlaceOrder
    (
      utxx::time_val a_ts_exch  = utxx::time_val(),
      utxx::time_val a_ts_recv  = utxx::time_val(),
      utxx::time_val a_ts_strat = utxx::time_val()
    )
    {
      if constexpr (HasOMC)
      {
        m_cancelledAt.clear();
        //-------------------------------------------------------------------//
        // Calculate the actial Px:                                          //
        //-------------------------------------------------------------------//
        PriceT px = 0.9 * m_ob->GetBestBidPx();   // Initially NaN
        if (!IsFinite(px))
          return;
        double qty = abs(m_sQty);

        assert(m_omc  != nullptr);
        AOS const* aos = nullptr;
        try
        {
          SecDefD const& instr = m_secDefsMgr->FindSecDef(m_instrName.data());
          if (IsFinite(instr.m_ABQtys[1]))
            qty *= double(px);
          if (!m_omc->HasFracQtys())
            qty = Floor(qty);

          std::cout << px << "  " << qty << std::endl;

          aos = m_omc->NewOrder
          (
            this,
            instr,
            FIX::OrderTypeT::Limit,
            true,
            px,         // NaN for Mkt and Pegged orders
            false,
            QtyAny(qty),
            a_ts_exch,
            a_ts_recv,
            a_ts_strat,
            false,  // NOT Buffered -- Flush Now!
            m_timeInForce
            // QtyShow, QtyMin and Peg params always take their defaul vals;
            // in particular, defaly Peg params mean pegging in THIS side of
            // the market with 0 offset -- ie Passive Pegging indeed!
          );
        }
        catch (exception const& exc)
        {
          cerr << "\nNewOrder: EXCEPTION: " << exc.what() << endl;
          return;
        }
        assert(aos != nullptr && aos->m_id > 0);
        m_aoses = aos;

        Req12* req  = aos->m_lastReq;
        assert(req != nullptr && req->m_kind == Req12::KindT::New);
        UNUSED_PARAM(int  lat =) req->GetInternalLatency();
      }
    }
#   if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#   endif
  };
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  try
  {
    //-----------------------------------------------------------------------//
    // Global Init:                                                          //
    //-----------------------------------------------------------------------//
    IO::GlobalInit({SIGINT});

    //-----------------------------------------------------------------------//
    // Get the Config:                                                       //
    //-----------------------------------------------------------------------//
    if (argc != 2)
    {
      cerr << "PARAMETER: ConfigFile.ini" << endl;
      return 1;
    }
    string iniFile(argv[1]);

    // Get the Propert Tree:
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    string exchange = pt.get<string>("Main.Exchange");
    boost::algorithm::to_lower(exchange);

    //-----------------------------------------------------------------------//
    // Create the Logger and the Reactor:                                    //
    //-----------------------------------------------------------------------//
    shared_ptr<spdlog::logger> loggerShP =
      IO::MkLogger
      (
        "Main",
        pt.get<string>("Exec.LogFile"),
        false,        // MT is not needed
        pt.get<long>  ("Exec.LogFileSzM") * 1048576,
        pt.get<int>   ("Exec.LogFileRotations")
      );
    spdlog::logger* logger = loggerShP.get();
    assert(logger != nullptr);

    EPollReactor reactor
    (
      logger,
      pt.get<int> ("Exec.DebugLevel"),
      pt.get<bool>("Exec.UsePoll",      false),
      pt.get<bool>("Exec.UseKernelTLS", true),
      128,        // MaxFDs
      pt.get<int> ("Exec.CPU",          -1)
    );

    //-----------------------------------------------------------------------//
    // Create the Strategy Instrance:                                        //
    //-----------------------------------------------------------------------//
    // NB: This automatically creates the RiskMgr and Connecttors:
    //
    // if (exchange == "bitfinex")
    // {
    //   LatencyTestStrat<EConnector_WS_BitFinex_MDC, EConnector_WS_BitFinex_OMC>
    //     mts(&reactor, logger, pt);
    //   mts.Run();
    // }
    // else
    if (exchange == "bitmex")
    {
      LatencyTestStrat<EConnector_WS_BitMEX_MDC, EConnector_H2WS_BitMEX_OMC>
        mts(&reactor, logger, pt);
      mts.Run();
    }
    else if (exchange == "binance_spt")
    {
      LatencyTestStrat<EConnector_H2WS_Binance_MDC_Spt,
                       EConnector_H2WS_Binance_OMC_Spt>
        mts(&reactor, logger, pt);
      mts.Run();
    }
    else if (exchange == "binance_fut")
    {
      LatencyTestStrat<EConnector_H2WS_Binance_MDC_FutT,
                       EConnector_H2WS_Binance_OMC_FutT>
        mts(&reactor, logger, pt);
      mts.Run();
    }
    // else if (exchange == "ftx")
    // {
    //   LatencyTestStrat<EConnector_WS_FTX_MDC>
    //     mts(&reactor, logger, pt);
    //   mts.Run();
    // }
    // else if (exchange == "huobi-spt")
    // {
    //   LatencyTestStrat<
    //       EConnector_H2WS_Huobi_MDC_Spt,
    //       EConnector_H2WS_Huobi_OMC_Spt>
    //       mts(&reactor, logger, pt);
    //   mts.Run();
    // }
    // else if (exchange == "huobi-fut")
    // {
    //   LatencyTestStrat<
    //       EConnector_H2WS_Huobi_MDC_Fut,
    //       EConnector_H2WS_Huobi_OMC_Fut>
    //       mts(&reactor, logger, pt);
    //   mts.Run();
    // }
    // else if (exchange == "huobi-swap")
    // {
    //   LatencyTestStrat<
    //       EConnector_H2WS_Huobi_MDC_Swap,
    //       EConnector_H2WS_Huobi_OMC_Swap>
    //       mts(&reactor, logger, pt);
    //   mts.Run();
    // }
    else
    {
      throw utxx::runtime_error("LatencyTest: Unsupported exchange");
    }
  }
  catch (exception const& exc)
  {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    // Prevent any further exceptions eg in Dtors:
    _exit(1);
  }
  return 0;
}
