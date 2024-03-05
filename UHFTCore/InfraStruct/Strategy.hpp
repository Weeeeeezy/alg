// vim:ts=2:et
//===========================================================================//
//                         "InfraStruct/Strategy.hpp":                       //
//                   Virtual Interface for Trading Strategies                //
//===========================================================================//
#pragma once

#ifdef  __GNUC__
#pragma GCC   diagnostic push
#pragma GCC   diagnostic ignored "-Wunused-parameter"
#endif
#ifdef  __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include "Basis/EPollReactor.h"
#include "Basis/BaseTypes.hpp"
#include "Basis/XXHash.hpp"
#include "Connectors/OrderBook.h"
#include "Protocols/FIX/Msgs.h"  // XXX: For "MDUpdateActionT"
#include <utxx/error.hpp>
#include <boost/core/noncopyable.hpp>
#include <string>
#include <functional>
#include <sys/socket.h>

namespace MAQUETTE
{
  //=========================================================================//
  // Fwd Class Declarations:                                                 //
  //=========================================================================//
  class  EConnector;
  struct Trade;
  struct AOS;
  struct Req12;
  class  ExchOrdID;

  //=========================================================================//
  // "Strategy" Class:                                                       //
  //=========================================================================//
  // Provides a virtual (XXX: yes!) interface to be implemented by actual Trad-
  // ing Strategies. The virtual methods listed below are NOT abstract: each of
  // them has a default "do-nothing" implementation; this is to simplify const-
  // ruction of "stub strategies":
  //
  class Strategy: public boost::noncopyable
  {
  public:
    //=======================================================================//
    // Common Internal Flds:                                                 //
    //=======================================================================//
    std::string     const          m_name;
    unsigned long   const          m_hash48;
    EPollReactor*   const          m_reactor;    // Ptr not owned
    spdlog::logger* const          m_logger;     // Ptr not owned
    int             const          m_debugLevel;

  protected:
    mutable std::vector<SecID>     m_subscribed; // SecIDs subscrd by this Strat

    // Stopping behavior: TS of the 1st SIGINT:
    mutable utxx::time_val         m_delayedStopInited;
    // Stop now?
    mutable bool                   m_nowStopInited;

    // Default Ctor is Deleted:
    Strategy() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    // Additional Actions:
    // (*) Specified signals will be targeted to the synchronous handler provi-
    //     ded by this Strategy;  if "a_sync_signals" is non-empty, then "a_re-
    //     actor" must be non-NULL;
    // (*) If The Logger was not initialised yet, it will be taken from the Re-
    //     actor specified (if any);
    // (*) The Loggers can be made asynchronous (NB: this affects the whole pro-
    //     cess) and running on the specified CPU:
    //
    Strategy
    (
      std::string const&      a_name,
      EPollReactor*           a_reactor,     // May be NULL here
      spdlog::logger*         a_logger,      // May be NULL here
      int                     a_debug_level,
      std::vector<int> const& a_sync_signals = std::vector<int>()
    )
    : m_name             (a_name),
      m_hash48           (To48(MkHashCode64(m_name))),
      m_reactor          (a_reactor),
      m_logger           (a_logger),
      m_debugLevel       (a_debug_level),
      m_subscribed       (),                          // Initially empty
      m_delayedStopInited(),
      m_nowStopInited    (false)
    {
      //---------------------------------------------------------------------//
      // Make the specified signals synchronous:                             //
      //---------------------------------------------------------------------//
      // NB: The "Reactor" param is actually only needed if we install the sig-
      // nal handlers (otherwise it can be NULL). XXX: This currently only works
      // in the non-LibVMA mode:
      //
      if (!a_sync_signals.empty()  && m_reactor != nullptr &&
          !(m_reactor->WithLibVMA()))
      {
        // SigHandler:
        IO::FDInfo::SigHandler sigH
        (
          [this](int, signalfd_siginfo const& a_si)->void
                { this->OnSignal(a_si); }
        );
        std::string  sigName = m_name + "-SigHandler";
        m_reactor->AddSignals
          (sigName.data(), a_sync_signals, sigH, IO::FDInfo::ErrHandler());
      }
    }

    //=======================================================================//
    // Dtor is Trivial:                                                      //
    //=======================================================================//
    virtual ~Strategy() noexcept {}

    //======================================================================//
    // Callback invoked by the quantizer to signal a new bar class predict ,//
    // which is calculated, e.g. by the TensorFlow engine                   //
    //======================================================================//
    virtual void OnNewClassPrediction
    (
      int            a_class_number,
      utxx::time_val a_ts_exch,
      utxx::time_val a_ts_recv,
      utxx::time_val a_ts_handl,
      std::map<std::string, std::pair<unsigned, float*>> const*
                     a_features = nullptr
    )
    {}

  public:
    //=======================================================================//
    // Common Call-Backs for MDCs and OMCs:                                  //
    //=======================================================================//
    // "OnTradingEvent":
    // For TradingEvents (TradingSessionStatus, SecurityStatus  or Connector-
    // Status events), Call-Backs are arranged directly by Strategy; because
    // such events are very rare, a simple vector is sufficient:
    // Arg1 is the originating "EConnector";
    // Arg2 is "true" for Trading Start and "false" for Trading Stop;
    // Arg3 is the SecID of the instrument which this event refers to, or 0 if
    //      it is for ALL instruments;
    //      this call-back is also invoked on Connector Start  and Stop events
    //      (with SecID==0):
    //
    virtual void OnTradingEvent
    (
      EConnector const& a_econn,
      bool              a_is_on,
      SecID             a_sec_id,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    )
    {}

    //-----------------------------------------------------------------------//
    // Call-Backs for MDCs only:                                             //
    //-----------------------------------------------------------------------//
    // "OnOrderBookUpdate":
    // Self-Explanatory; MDC ref can be obtained via the OrderBook: NB:
    // (*) no need for timing params here -- they are available via "OrderBook"
    //     ("Get{Event,Recv}Time");
    // (*) if "a_is_error" flag is set, the Contents of the OrderBook is poten-
    //     tially erroneous and cannot be trusted; only the Meta-Data  can  be
    //     used, eg for error handling;
    // (*) this  is the only method where "a_ts_strat" is passed from the Caller
    //     (rather than computed by the Callee), because it is computed anyway
    //     by "EConnector_MktData" for statistical purposes:
    //
    virtual void OnOrderBookUpdate
    (
      OrderBook const&          a_ob,
      bool                      a_is_error,
      OrderBook::UpdatedSidesT  a_sides,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv,
      utxx::time_val            a_ts_strat
    )
    {}

    // "OnTradeUpdate":
    // Here "Trade" is any MktData trade, NOT necessarily our trade (the latter
    // trades are reported via the "OnTrade" OMC call-back given below); again,
    // the MDC ref can be obtained via the "Trade" object:
    // Again, no need for timing params here -- they are available via "Trade"
    // ("m_{event,recv}TS"):
    // NB:
    // (*) Because "TradeUpdate" is a rather simple  "stateless" event,  then,
    //     unlike "OnOrderBookUpdate", there is no error reporting here:
    // (*) Do NOT memoise a ptr to "a_tr" itself -- it may be an ON-STACK obj!
    //
    virtual void OnTradeUpdate(Trade const& a_tr) {}

    //-----------------------------------------------------------------------//
    // Call-Backs for OMCs only:                                             //
    //-----------------------------------------------------------------------//
    // "OnConfirm":
    // Call-Back invoked when New Order or Order Modification was confirmed;
    // (*) "a_req" is the confirmed New or Modification Request:
    // (*) confirmation Time Stamps are contained in "Req12": use "m_ts_conf_
    //     {exch,conn}":
    //
    virtual void OnConfirm(Req12 const& a_req) {}

    // "OnOurTrade":
    // Call-Back invoked when a Trade (Fill or PartialFill) occurs  on a given
    // Order. All necessary info can be obtained from the "Trade" obj:
    // Pre-Cond:  a_tr.m_ourReq != nullptr:
    //
    virtual void OnOurTrade(Trade const& a_tr) {}

    // "OnCancel":
    // Call-Back invoked when whole Order has been cancelled in a normal way:
    // (*) "a_aos" is this AOS -- there is no need to provide individual "Req12"
    //     in case of the whole AOS being cancelled    (though the last "Done"
    //     "Req12" can be found if necessary):
    //
    virtual void OnCancel
    (
      AOS const&        a_aos,
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    )
    {}

    // "OnOrderError":
    // Call-Back invoked on any error within an OMC:
    // (*) the error may or may not result in the whole Order failing; the lat-
    //     ter condition can be determined  via Req12->AOS->StatusT;
    // (*) if the Order fails as a result of an error, NO separate "OnCancelCB"
    //     invocation is performed, because it is not a normal Cancel:
    //
    virtual void OnOrderError
    (
      Req12 const&      a_req,
      int               a_err_code,
      char const*       a_err_text,
      bool              a_prob_fill,  // Order has probably been Filled?
      utxx::time_val    a_ts_exch,
      utxx::time_val    a_ts_recv
    )
    {}

    //-----------------------------------------------------------------------//
    // Other Call-Backs:                                                     //
    //-----------------------------------------------------------------------//
    // "OnSignal":
    // Synchronous Signal Handling Call-Back. NB: Probably no need for timing
    // params here -- this is NOT an Exchange-originating event:
    //
    virtual void OnSignal(signalfd_siginfo const& a_si)     {}

    //=======================================================================//
    // Meta-Data on the Strategy (non-virtual methods):                      //
    //=======================================================================//
    std::string const& GetName()   const  { return m_name;   }
    unsigned long      GetHash48() const  { return m_hash48; }

    //=======================================================================//
    // Session Mgmt:                                                         //
    //=======================================================================//
    void InitDelayedStop(utxx::time_val a_ts) { m_delayedStopInited = a_ts; }

    // Is the Strategy stopping?
    bool IsStopping() const
      { return (!m_delayedStopInited.empty() || m_nowStopInited); }

    //=======================================================================//
    // Subscription Mgmt:                                                    //
    //=======================================================================//
    // "Subscribe": Register a SecID on the "m_subscribed" list:
    //
    void Subscribe(SecID a_sec_id) const
    {
      if (utxx::unlikely(a_sec_id == 0))
        throw utxx::badarg_error("Stratrgy::Subscribe: Empty SecID");

      if (std::find(m_subscribed.cbegin(), m_subscribed.cend(), a_sec_id) ==
          m_subscribed.cend())
        m_subscribed.push_back(a_sec_id);
    }

    // "UnSubscribe": Self-Evident:
    void UnSubscribe(SecID a_sec_id) const
    {
      // Do nothing if this SecID is not on the list (eg if it is 0):
      auto it = std::find(m_subscribed.begin(), m_subscribed.end(), a_sec_id);

      if (it != m_subscribed.end())
        m_subscribed.erase(it);
    }

    // "IsSubscribed":
    bool IsSubscribed(SecID a_sec_id) const
    {
      return
        (std::find(m_subscribed.cbegin(), m_subscribed.cend(), a_sec_id) !=
         m_subscribed.cend());
    }
  };
} // End namespace MAQUETTE

//===========================================================================//
// "SAFE_STRAT_CB" Macro:                                                    //
//===========================================================================//
// Wrapper for Strategy Call-Backs invocations -- preventing possible Strategy
// exceptions from propagating into the Connector (to guarantee the integrity
// of the latter), apart from "EPollReactor::ExitRun" which is intended to
// terminate the Main Reactor Loop:
//
# ifdef  SAFE_STRAT_CB
# undef  SAFE_STRAT_CB
# endif
# define SAFE_STRAT_CB(strat, CallBack, ArgsTuple)  \
  try \
  {   \
    strat->CallBack ArgsTuple;         \
  }   \
  catch (EPollReactor::ExitRun const&) \
  {   \
    throw; \
  }   \
  catch (std::exception const& exc)    \
  {   \
    if (utxx::likely(m_logger != nullptr)) \
      m_logger->warn  \
        ("{}: EXCEPTION in {}::" #CallBack ": {}", __func__, strat->GetName(), \
         exc.what()); \
  }
// End of SAFE_STRAT_CB

#ifdef  __GNUC__
#pragma GCC   diagnostic pop
#endif
#ifdef  __clang__
#pragma clang diagnostic pop
#endif
