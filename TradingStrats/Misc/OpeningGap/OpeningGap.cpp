// vim:ts=2:et
//===========================================================================//
//                 "TradingStrats/OpeningGap/OpeningGap.cpp":                //
//      Taking a Pre-Defined Directional Position at Market OpeningTime      //
//===========================================================================//
#include "Basis/Maths.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Connectors/FAST/EConnector_FAST_FORTS.h"
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"
#include "InfraStruct/SecDefsMgr.h"
#include "InfraStruct/RiskMgr.h"
#include "InfraStruct/Strategy.hpp"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <cassert>

# ifdef  __GNUC__
# pragma GCC   diagnostic push
# pragma GCC   diagnostic ignored "-Wunused-parameter"
# endif
# ifdef  __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-parameter"
# endif

using namespace MAQUETTE;
using namespace std;

namespace
{
  //=========================================================================//
  // "OpeningGapStrat" Class:                                                //
  //=========================================================================//
  // XXX: Currently, we assume it is for FORTS only:
  //
  class OpeningGapStrat final: public Strategy
  {
  private:
    //=======================================================================//
    // Data Types:                                                           //
    //=======================================================================//
    // Qty Type used by the MDC:
    constexpr static QtyTypeT QTM = QtyTypeT::Lots;
    using     QRM  = long;
    using     QtyM = Qty<QTM, QRM>;

    // Qty Type used by the OMC:
    constexpr static QtyTypeT QTO = QtyTypeT::QtyA; // Contracts are OK as well
    using     QRO  = long;
    using     QtyO = Qty<QTO, QRO>;

    //=======================================================================//
    // Flds:                                                                 //
    //=======================================================================//
    SecDefsMgr*                m_secDefsMgr;
    RiskMgr*                   m_riskMgr;
    vector<string>             m_instrNames;       // Must actually be just 1
    EConnector_FAST_FORTS_FOL  m_mdc;
    EConnector_TWIME_FORTS     m_omc;

    // Order-Related Params:
    SecDefD const&             m_instr;      // ...
    QtyO                       m_qty;        // Qty for all Orders (NOT in Lots)
    bool                       m_isBuy;      // Main Order is Long?
    double                     m_takeProfit; // Differential, > 0
    double                     m_stopLoss;   // Differential, < 0

    AOS const*                 m_aosMain;    // Main        Order
    AOS const*                 m_aosTP;      // Take-Profit Order
    AOS const*                 m_aosSL;      // Stop-Loss   Order
    PriceT                     m_fillPx;     // FillPx of the Main Order
    int                        m_signalsCount;

    // Default Ctor is deleted:
    OpeningGapStrat() =  delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    OpeningGapStrat
    (
      EPollReactor  *              a_reactor,
      spdlog::logger*              a_logger,
      boost::property_tree::ptree& a_params   // XXX: Mutable!
    )
    : Strategy
      (
        "OpeningGap",
        a_reactor,
        a_logger,
        a_params.get<int>("Exec.DebugLevel"),
        { SIGINT }
      ),
      // SecDefsMgr:
      m_secDefsMgr(SecDefsMgr::GetPersistInstance
        (a_params.get<string>("Main.Env") == "Prod", "")),

      // RiskMgr: FIXME: Not yet:
      m_riskMgr   (nullptr),

      // Instrument to be traded: Only one, but we create a vector for MDC/OMC
      // API compatibility:
      m_instrNames{a_params.get<string>("Main.Instr")},

      // [EConnector_FAST_FORTS] (MktData):
      m_mdc
      (
        (a_params.get<string>("Main.Env") == "Prod")  // Non-Prod -> Test!
          ? FAST::FORTS::EnvT::ProdF
          : FAST::FORTS::EnvT::TestL,
        m_reactor,
        m_secDefsMgr,
        &m_instrNames,
        m_riskMgr,
        GetParamsSubTree  (a_params, "EConnector_FAST_FORTS"),
        nullptr
      ),

      // [EConnector_TWIME_FORTS] (OrdMgmt):
      m_omc
      (
        m_reactor,
        m_secDefsMgr,
        &m_instrNames,
        m_riskMgr,
        SetAccountKey
        (
          &a_params,
          "EConnector_TWIME_FORTS",
          a_params.get<string>("EConnector_TWIME_FORTS.AccountPfx") +
            "-TWIME-FORTS-" +
            ((a_params.get<string>("Main.Env") == "Prod") ? "Prod" : "Test")
        )
      ),

      // Order-Related Params:
      m_instr             (m_secDefsMgr->FindSecDef
                          (m_instrNames.begin()->data())),
      m_qty               (a_params.get<long>  ("Main.Qty")),
      m_isBuy             (a_params.get<string>("Main.Direction") == "Up"),
      m_takeProfit        (a_params.get<double>("Main.TakeProfit")),
      m_stopLoss          (a_params.get<double>("Main.StopLoss")),

      // There are no AOSes yet:
      m_aosMain           (nullptr),
      m_aosTP             (nullptr),
      m_aosSL             (nullptr),
      m_fillPx            (0.0),
      m_signalsCount      (0)
    {
      //---------------------------------------------------------------------//
      // Verify the Params:                                                  //
      //---------------------------------------------------------------------//
      // Make sure Reactor and Logger are non-NULL ("Strategy" itself does not
      // require that):
      if (utxx::unlikely(m_reactor == nullptr || m_logger == nullptr))
        throw utxx::badarg_error
              ("OpeningGap::Ctor: Reactor and Logger must be non-NULL");

      if (utxx::unlikely(!IsPos(m_qty)))
        throw utxx::badarg_error
          ("OpeningGapStrat::Ctor: Invalid Qty=", QRO(m_qty));

      if (utxx::unlikely(m_takeProfit <= 0.0 || m_stopLoss <= 0.0))
        throw utxx::badarg_error("OpeningGapStrat::Ctor: Invalid TP/SL");

      if (utxx::unlikely
         (a_params.get<string>("Main.Direction") != "Up" &&
          a_params.get<string>("Main.Direction") != "Down"))
        throw utxx::badarg_error("OpeningGapStrat::Ctor: Invalid Direction");

      //---------------------------------------------------------------------//
      // Subscribe to receive notifications from both Connectrors:           //
      //---------------------------------------------------------------------//
      // NB: FAST MDC should be ready to subscribe MktData immediately (even if
      // it has not been started yet):
      m_mdc.Subscribe(this);
      m_omc.Subscribe(this);

      // NB: We currently only need L1 Px updates; RegisterInstrRisks=true:
      m_mdc.SubscribeMktData
        (this, m_instr, OrderBook::UpdateEffectT::L1Px, true);
    }

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~OpeningGapStrat() noexcept override
    {
      // Do not allow any exceptions to propagate out of this Dtor:
      try
      {
        // Stop Connectors (if not stopped yet):
        m_mdc.UnSubscribe(this);
        m_mdc.Stop();

        m_omc.UnSubscribe(this);
        m_omc.Stop();

        // Reset the SecDefsMgr abd RiskMgr ptrs. NB: the actual objs are alloc-
        // ated in ShM, don't delete them!
        m_secDefsMgr = nullptr;
        m_riskMgr    = nullptr;

        // Other components are finalised automatically...
      }
      catch(...){}
    }

    //=======================================================================//
    // "Run":                                                                //
    //=======================================================================//
    void Run()
    {
      // Start the Connectors:
      m_mdc.Start();
      m_omc.Start();

      // Enter the Inifinite Event Loop (exit on any exceptions):
      m_reactor->Run(true);
    }

    //=======================================================================//
    // "OnTradingEvent" Call-Back:                                           //
    //=======================================================================//
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
      // Output a msg:
      LOG_INFO(1,
        "OnTradingEvent: Connector={}: Trading {}: MDCActive={}, OMCActive={}",
        a_conn.GetName(), a_on ? "STARTED" : "STOPPED",
        m_mdc.IsActive(), m_omc.IsActive())

      // Should we stop now?
      // Do so if a termination signal has been received, and both Connectors
      // are now inactive:
      if (utxx::unlikely
         (!(m_mdc.IsActive() || m_omc.IsActive()) && m_signalsCount >= 1))
        m_reactor->ExitImmediately("OpeningGapStrat::OnTradingEvent");
    }

    //=======================================================================//
    // "OnOrderBookUpdate" Call-Back:                                        //
    //=======================================================================//
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
      //---------------------------------------------------------------------//
      // Measure the internal latency of MktData delivery to the Strategy:   //
      //---------------------------------------------------------------------//
      // Check the error condition first -- but do not terminate on that case:
      if (utxx::unlikely(a_is_error))
      {
        LOG_ERROR(2, "OnOrderBookUpdate: Got OrderBook Error")
        return;
      }
      // Instrument of this OrderBook:
      SecDefD const& instr = a_ob.GetInstr();

      if (&instr != &m_instr)
        // This may happen if a Ccy Converter OrderBook has been updated, not
        // the Main one:
        return;

      // Also, for safety, we need both Bid and Ask pxs availavle in order to
      // proceed further (also if OMC is not active yet):
      PriceT bidPx = a_ob.GetBestBidPx();
      PriceT askPx = a_ob.GetBestAskPx();
      if (!(IsFinite(bidPx) && IsFinite(askPx) && m_omc.IsActive()))
        return;

      //---------------------------------------------------------------------//
      // Should we place the Main Order now?                                 //
      //---------------------------------------------------------------------//
      if (m_aosMain == nullptr)
      {
        // If we are to place an Aggressive Buy order, we need at least a valid
        // px on the Ask side which we will hit;   for Aggressive Sell, a valid
        // Bid px:
        PriceT  refPx = m_isBuy ? askPx : bidPx;
        assert(IsFinite(refPx));

        // We are about to submit the 1st order -- start the RiskMgr now:
        if (m_riskMgr != nullptr)
          m_riskMgr->Start();

        // Place the order: IsAggressive=true:
        m_aosMain = PlaceOrder<true>
                    (m_isBuy, refPx, m_qty, a_ts_exch, a_ts_recv, a_ts_strat);
        assert(m_aosMain != nullptr);
        return;
      }

      //---------------------------------------------------------------------//
      // Should we place the Stop-Loss Order now?                            //
      //---------------------------------------------------------------------//
      // Only do so under the following conditions: FIXME: if the Main Order is
      // only Part-Filled (and so not Inactive), can we get stuck w/o Stop-Loss?
      //
      QtyO mainCQL = m_aosMain->GetCumFilledQty<QTO,QRO>();

      if (m_aosMain != nullptr    && mainCQL > 0        &&
          m_aosMain->m_isInactive && m_aosSL == nullptr &&
         (( m_isBuy && askPx < m_fillPx - m_stopLoss)   ||
          (!m_isBuy && bidPx > m_fillPx + m_stopLoss)))
      {
        // IsAggressive=true for a StopLoss order. It comes in the direction
        // opposite to the Main one:
        PriceT refPx = m_isBuy ? bidPx : askPx;    // Does not really matter

        m_aosSL = PlaceOrder<true>
                  (!m_isBuy,  refPx, mainCQL, a_ts_exch, a_ts_recv, a_ts_strat);
        assert(m_aosSL != nullptr);
      }
      // All Done!
    }

    //=======================================================================//
    // "OnTradeUpdate" Call-Back:                                            //
    //=======================================================================//
    // NB: This method is currently provided because it is required by virtual
    // interface, but  it is not actually used:
    //
    void OnTradeUpdate(Trade const& a_trade) override {}

    //=======================================================================//
    // "OnOurTrade" Call-Back:                                               //
    //=======================================================================//
    void OnOurTrade(Trade const& a_tr) override
    {
      utxx::time_val stratTime = utxx::now_utc ();

      // Output a msg:
      Req12 const* req = a_tr.m_ourReq;
      assert(req != nullptr);
      AOS   const* aos = req->m_aos;
      assert(aos != nullptr   && aos->m_instr   != nullptr         &&
            (aos == m_aosMain || aos == m_aosTP || aos == m_aosSL) &&
            (aos->m_instr == &m_instr));

      QtyO   qty = a_tr.GetQty<QTO,QRO>();
      assert(IsPos(qty));
      PriceT px  = a_tr.m_px;
      assert(double(px) > 0.0);

      LOG_INFO(2,
        "OnOurTrade: TRADED: {} @ {}, {}, ExchID={}, ExecID={}",
        aos->m_isBuy ? 'B' : 'S',    QRO(qty),       double(px),
        a_tr.m_execID.ToString())

      if (aos == m_aosMain)
      {
        // This was the Main Order:
        // Memoise the Fill Px -- TODO: compute the AVG; for the moment, it's
        // the LAST Fill Px:
        m_fillPx = px;

        // If the Main Order is now Completely Filled and Inactive, place a
        // Passive Take-Profit Order (in the opposite direction to the Main
        // one). FIXME: Can we get stuck in a Part-Filled (not yet Inactive)
        //  state, so Take-Profit is not placed at all?
        //
        assert(IsPos(aos->GetCumFilledQty<QTO,QRO>()));

        if (aos->m_isInactive && m_aosTP == nullptr)
        {
          QtyO   tpQty = aos->GetCumFilledQty<QTO,QRO>();
          assert(IsPos(tpQty));

          PriceT tpPx = m_isBuy
                       ? (m_fillPx + m_takeProfit)   // To sell higher
                       : (m_fillPx - m_takeProfit);  // To buy  lower
          // IsAggressive=false:
          m_aosTP = PlaceOrder<false>
                    (!m_isBuy, tpPx, tpQty, a_tr.m_exchTS, a_tr.m_recvTS,
                     stratTime);
          assert(m_aosTP != nullptr);
        }
      }
    }

    //=======================================================================//
    // "OnConfirm" Call-Back:                                                //
    //=======================================================================//
    void OnConfirm(Req12 const& a_req) override
    {
      AOS const* aos = a_req.m_aos;
      assert(aos != nullptr);
      LOG_INFO(3,
        "OnConfirm: Order Confirmed by Exchange: AOSID={}, ReqID={}, ExcID="
        "{}", aos->m_id,  a_req.m_id,  a_req.m_exchOrdID.ToString())
    }

    //=======================================================================//
    // "OnCancel" Call-Back:                                                 //
    //=======================================================================//
    void OnCancel
    (
      AOS const&      a_aos,
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    )
    override
    {
      assert(a_aos.m_isInactive);
      // This should not happen, hence a warning:
      LOG_WARN(3, "OnCancel: Order CANCELLED: AOSID={}", a_aos.m_id)
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
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    )
    override
    {
      AOS const* aos  = a_req.m_aos;
      assert(aos != nullptr);

      LOG_ERROR(2,
        "OnOrderError: AOSID={}, ReqID={}: ErrCode={}: {}{}",
        aos->m_id, a_req.m_id, a_err_code, a_err_text,
        aos->m_isInactive ? ": ORDER FAILED"    : "",
        a_prob_filled     ? ": Probably Filled" : "")
    }

    //=======================================================================//
    // "OnSignal" Call-Back:                                                 //
    //=======================================================================//
    void OnSignal(signalfd_siginfo const& a_si) override
    {
      ++m_signalsCount;

      LOG_INFO(1,
        "OnSignal: Got a Signal ({}), Count={}, Exiting {}...",
        a_si.ssi_signo,         m_signalsCount,
        (m_signalsCount == 1) ? "Gracefully" : "Immediately")

      if (m_signalsCount == 1)
      {
        // Try to stop both Connectors, and then exit (by TradingEvent):
        m_mdc.Stop();
        m_omc.Stop();
      }
      else
      {
        assert(m_signalsCount >= 2);
        // Throw the "ExitRun" exception to exit the Main Event Loop (if we are
        // not in that loop, it will be handled safely anyway):
        m_reactor->ExitImmediately("OpeningGapStrat::OnSignal");
      }
    }

    //=======================================================================//
    // "PlaceOrder":                                                         //
    //=======================================================================//
    template<bool IsAggressive>
    AOS const* PlaceOrder
    (
      bool           a_is_buy,
      PriceT         a_ref_px,
      QtyO           a_qty,
      utxx::time_val a_ts_exch,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_strat
    )
    {
      //---------------------------------------------------------------------//
      // Calculate the Limit Px (Aggr or Passive):                           //
      //---------------------------------------------------------------------//
      PriceT px =
        IsAggressive
        ? (a_is_buy
           ? RoundUp  (1.01 * a_ref_px, m_instr.m_PxStep)
           : RoundDown(0.99 * a_ref_px, m_instr.m_PxStep))
        : a_ref_px;

      assert(IsPos(a_qty) && IsFinite(px));

      //---------------------------------------------------------------------//
      // Now actually place it:                                              //
      //---------------------------------------------------------------------//
      AOS const* aos = nullptr;
      try
      {
        aos = m_omc.NewOrder
        (
          this,
          m_instr,
          FIX::OrderTypeT::Limit,
          a_is_buy,
          px,
          IsAggressive,
          QtyAny(a_qty),
          a_ts_exch,
          a_ts_recv,
          a_ts_strat
        );
      }
      catch (exception const& exc)
      {
        LOG_ERROR(2, "PlaceOrder: EXCEPTION: {}", exc.what())
        return nullptr;
      }

      assert(aos != nullptr && aos->m_id > 0 && aos->m_isBuy == a_is_buy);
      Req12* req  = aos->m_lastReq;
      assert(req != nullptr && req->m_kind == Req12::KindT::New);
      int    lat  = req->GetInternalLatency();

      LOG_INFO(3,
        "PlaceOrder: NewOrder: AOSID={}, ReqID={}: {} {}: {} lots, Px={}: "
        "Latency={} usec",
        aos->m_id,   req->m_id, IsAggressive ? "Aggressive" : "Passive",
        aos->m_isBuy ? "Buy" : "Sell",   QRO(req->GetQty<QTO,QRO>()),
        double(px),  (lat != 0) ? to_string(lat) : "unknown")

      return aos;
    }
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

    //-----------------------------------------------------------------------//
    // Create the Logger and the Reactor:                                    //
    //-----------------------------------------------------------------------//
    shared_ptr<spdlog::logger> loggerShP =
      IO::MkLogger
      (
        "Main",
        pt.get<string>("Exec.LogFile"),
        false,        // MT is not required!
        pt.get<long>  ("Exec.LogFileSzM") * 1048576,
        pt.get<int>   ("Exec.LogFileRotations")
      );
    spdlog::logger*  logger = loggerShP.get();
    assert(logger != nullptr);

    EPollReactor reactor
    (
      logger,
      pt.get<int> ("Main.DebugLevel"),
      pt.get<bool>("Exec.UsePoll",      false),
      pt.get<bool>("Exec.UseKernelTLS", true),
      128,        // MaxFDs
      pt.get<int> ("Exec.StrategyCPU")
    );

    //-----------------------------------------------------------------------//
    // Create and Run the Strategy:                                          //
    //-----------------------------------------------------------------------//
    OpeningGapStrat ogs(&reactor, logger, pt);

    ogs.Run();
  }
  catch (exception const& exc)
  {
    cerr << "\nEXCEPTION: " << exc.what() << endl;
    return 1;
  }
  return 0;
}

# ifdef  __GNUC__
# pragma GCC   diagnostic pop
# endif
# ifdef  __clang__
# pragma clang diagnostic pop
# endif
