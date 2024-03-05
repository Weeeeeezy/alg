// vim:ts=2:et
//===========================================================================//
//             "Connectors/P2CGate/EConnector_P2CGate_FORTS_MDC.cpp":        //
//                   PlazaII--CGate MktData Connector for FORTS              //
//===========================================================================//
#include "Connectors/P2CGate/EConnector_P2CGate_FORTS_MDC.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/TWIME/EConnector_TWIME_FORTS.h"
#include "Venues/FORTS/SecDefs.h"
#include <utxx/enumv.hpp>
#include <utxx/error.hpp>
#include <cgate.h>
#include <ctime>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // Error Codes and States:                                                 //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "CGErrCodeT":                                                           //
  //-------------------------------------------------------------------------//
  UTXX_ENUMV(CGErrCodeT, (unsigned, 0),
    (OK,              0)
    (InternalError,   CG_ERR_INTERNAL)
    (InvalidArgument, CG_ERR_INVALIDARGUMENT)
    (UnSupported,     CG_ERR_UNSUPPORTED)
    (TimeOut,         CG_ERR_TIMEOUT)
    (More,            CG_ERR_MORE)
    (IncorrectState,  CG_ERR_INCORRECTSTATE)
    (DuplicateID,     CG_ERR_DUPLICATEID)
    (BufferTooSmall,  CG_ERR_BUFFERTOOSMALL)
    (OverFlow,        CG_ERR_OVERFLOW)
    (UnderFlow,       CG_ERR_UNDERFLOW)
    (EoF,             CG_ERR_EOF)
  );

  //-------------------------------------------------------------------------//
  // "CGConnStateT" (for Connection and Listener States):                    //
  //-------------------------------------------------------------------------//
  UTXX_ENUMV(CGConnStateT, (unsigned, 0),
    (Closed,          CG_STATE_CLOSED)
    (Error,           CG_STATE_ERROR)
    (Opening,         CG_STATE_OPENING)
    (Active,          CG_STATE_ACTIVE)
  );

  //-------------------------------------------------------------------------//
  // "CGMsgT":                                                               //
  //-------------------------------------------------------------------------//
  UTXX_ENUMV(CGMsgT, (unsigned, 0),
    (StreamOpen,      CG_MSG_OPEN )
    (StreamClose,     CG_MSG_CLOSE)
    (Time,            CG_MSG_TIME )               // Undocumented
    (Data,            CG_MSG_DATA )               // Eg OrdMgmt reply
    (StreamData,      CG_MSG_STREAM_DATA)         // Eg MktData
    (DataArray,       CG_MSG_DATAARRAY)           // Undocumented
    (ObjFailed,       CG_MSG_OBJFAILED)           // Undocumented
    (ParseErr,        CG_MSG_PARSEERR )           // Undocumented
    (TnBegin,         CG_MSG_TN_BEGIN )
    (TnCommit,        CG_MSG_TN_COMMIT)
    (TnRollBack,      CG_MSG_TN_ROLLBACK)         // Undocumented
    (MQTimeOut,       CG_MSG_P2MQ_TIMEOUT  )
    (ReplLifeNum,     CG_MSG_P2REPL_LIFENUM)
    (ReplAllDeleted,  CG_MSG_P2REPL_CLEARDELETED)
    (ReplOnLineMode,  CG_MSG_P2REPL_ONLINE )
    (ReplState,       CG_MSG_P2REPL_REPLSTATE)
  );

  //=========================================================================//
  // Global Env and Connection Obj are Initialised?                          //
  //=========================================================================//
  bool EConnector_P2CGate_FORTS_MDC::s_isEnvInited = false;

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_P2CGate_FORTS_MDC::EConnector_P2CGate_FORTS_MDC
  (
    EPollReactor*                      a_reactor,
    SecDefsMgr*                        a_sec_defs_mgr,
    vector<string>              const* a_only_symbols,
    RiskMgr*                           a_risk_mgr,
    boost::property_tree::ptree const& a_params,
    EConnector_MktData*                a_primary_mdc
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "FORTS",
      0,                               // XXX: No extra ShM data at the moment
      a_reactor,
      true,                            // BusyWait IS REQUIRED
      a_sec_defs_mgr,
      FORTS::GetSecDefs_All
        (EConnector::GetMQTEnv (a_params.get<std::string>("AccountKey"))),
      a_only_symbols,
      a_params.get<bool>       ("ExplSettlDatesOnly",   true),
      false,                           // No Tenors: FORTS SecIDs are static!
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>    // WithFracQtys (= false)
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_MktData":                                                 //
    //-----------------------------------------------------------------------//
    EConnector_MktData
    (
      true,                            // Yes, MDC is Enabled!
      a_primary_mdc,
      a_params,
      false,                           // No FullAmt
      false,                           // No Sparse OrderBooks
      true,                            // WithSeqNums!
      false,                           // No RptSeqs
      false,                           // No ContPreSeqs, therefore
      0,                               // MktDepth=+oo
      // XXX: At the moment, similar to FAST FORTS, we only receive Trades from
      // the FullOrdersLog, not from explicit subscription:
      false,
      a_params.get<bool>("WithTrades"),
      false                            // No DynInit -- TCP-based MDC!
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_P2CGate_FORTS_MDC" Itself:                                //
    //-----------------------------------------------------------------------//
    m_conn        (nullptr),
    m_lsnOBs      (nullptr),
    m_lsnTrs      (nullptr),
    m_isActive    (false),
    m_isOnLineOBs (false)
  {
    //-----------------------------------------------------------------------//
    // Initialise the CGate Env -- but only once:                            //
    //-----------------------------------------------------------------------//
    // XXX: Only "Prod" and "Test" modes are currently supported; "Hist" mode
    // is not:
    bool isProd = FORTS::IsProdEnv(m_cenv);

    if (utxx::likely(!s_isEnvInited))
    {
      string settingsFile = a_params.get<string>("SettingsFile", "");
      string key          = isProd ? "55555555" : "11111111";
      string initStr      =
        ((!settingsFile.empty()) ? ("ini=" + settingsFile + ";") : "")   +
        "minloglevel=info;log=p2:p2syslog;subsystems=mq,replclient;key=" + key;

      CHECK_ONLY(auto res =) cg_env_open(initStr.data());
      CHECK_ONLY
      (
        if (utxx::unlikely(res != 0))
          throw utxx::runtime_error
                ("EConnector_P2CGate_FORTS_MDC::Ctor: Failed to Initialise the"
                 " Env: ", CGErrCodeT(res).c_str());
      )
      s_isEnvInited = true;
    }

    //-----------------------------------------------------------------------//
    // Create the Connection Obj:                                            //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) Although the main communications between  our process  and  the P2MQ
    //     Router is via the ShM, {IP:Port} are used for the local control ses-
    //     sion;
    // (*) the PortNum is hard-wired (4001 for Prod, 3001 for Test); it must be
    //     same as indicated in the [P2MQRouter] section of the corresp  "P2MQ-
    //     Router.ini" file;
    // (*) all other connection params will take their default vals:
    //
    assert(s_isEnvInited && m_conn == nullptr);

    string connStr =
      "p2lrpcq://127.0.0.1:" + string(isProd ? "4001" : "3001") +
      ";app_name=" + m_name;

    CHECK_ONLY(auto res =) cg_conn_new(connStr.data(), &m_conn);
    CHECK_ONLY
    (
      if (utxx::unlikely(res != 0 || m_conn == nullptr))
        throw utxx::runtime_error
              ("EConnector_P2CGate_FORTS_MDC::Ctor: Failed to Create the "
               "Connection Obj: ", connStr, ": ", CGErrCodeT(res).c_str());
    )
    assert(m_conn != nullptr);

    //-----------------------------------------------------------------------//
    // Create the OrderBooks Listener:                                       //
    //-----------------------------------------------------------------------//
    // Config String for the OrderBook Listener NB: In the Prod mode, we use
    // the "FASTREPL" stream, and in the Test mode, just "REPL":
    //
    assert(m_lsnOBs == nullptr);
    string ext       = isProd ? "FASTREPL" : "REPL";
    string lsnOBsStr =
      "p2ordbook://FORTS_ORDLOG_" + ext + ";snapshot=FORTS_ORDBOOK_" + ext +
      ";name=OrderLogsAll";

    CHECK_ONLY(res =)
      cg_lsn_new(m_conn, lsnOBsStr.data(), OrderBooksLsnCB, this, &m_lsnOBs);

    CHECK_ONLY
    (
      if (utxx::unlikely(res != 0 || m_lsnOBs == nullptr))
        throw utxx::runtime_error
              ("EConnector_P2CGate_FORTS_MDC::Ctor: Failed to Create the Order"
               "Books Listener: ", lsnOBsStr, ": ", CGErrCodeT(res).c_str());
    )
    assert(m_lsnOBs != nullptr);

    //-----------------------------------------------------------------------//
    // Create the Trades Listener:                                           //
    //-----------------------------------------------------------------------//
    assert(m_lsnTrs == nullptr);
    if (m_withExplTrades)
    {
      // XXX: For the moment, we do NOT receive multi-leg trades:
      string lsnTrsStr = "p2repl://FORTS_DEALS_" + ext + ";tables=deal";

      CHECK_ONLY(res =)
        cg_lsn_new(m_conn, lsnTrsStr.data(), TradesLsnCB, this, &m_lsnTrs);

      CHECK_ONLY
      (
        if (utxx::unlikely(res != 0 || m_lsnTrs == nullptr))
          throw utxx::runtime_error
                ("EConnector_P2CGate_FORTS_MDC::Ctor: Failed to Create the "
                 "Trades Listener: ", lsnTrsStr, ": ",
                 CGErrCodeT(res).c_str());
      )
    }
    // All Done!
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  // In case of any error in the Dtor, log them but obviously do NOT throw any
  // exceptions:
  //
  EConnector_P2CGate_FORTS_MDC::~EConnector_P2CGate_FORTS_MDC() noexcept
  {
    // Make sure exceptions do not propagate out of the Dtor:
    try
    {
      //---------------------------------------------------------------------//
      // If the Connector is still running, Stop it first:                   //
      //---------------------------------------------------------------------//
      Stop();
      bool ok = true;

      //---------------------------------------------------------------------//
      // Destroy the Trades Listener (if exists):                            //
      //---------------------------------------------------------------------//
      if (m_lsnTrs != nullptr)
      {
        CHECK_ONLY(auto res =) cg_lsn_destroy(m_lsnTrs);
        CHECK_ONLY
        (
          if (utxx::unlikely(res != 0))
          {
            LOG_ERROR(1,
              "EConnector_P2CGate_FORTS_MDC::Dtor: Failed to Destroy the "
              "Trades Listener: {}", CGErrCodeT(res).c_str())
            ok = false;
          }
        )
        // XXX: Still, reset the ptr:
        m_lsnTrs = nullptr;
      }

      //---------------------------------------------------------------------//
      // Destroy the OrderBooks Listener:                                    //
      //---------------------------------------------------------------------//
      if (utxx::likely(m_lsnOBs != nullptr))
      {
        CHECK_ONLY(auto res = ) cg_lsn_destroy(m_lsnOBs);
        CHECK_ONLY
        (
          if (utxx::unlikely(res != 0))
          {
            LOG_ERROR(1,
              "EConnector_P2CGate_FORTS_MDC::Dtor: Failed to Destroy the Order"
              "Books Listener: {}", CGErrCodeT(res).c_str())
            ok = false;
          }
        )
        // XXX: Still, reset the ptr:
        m_lsnOBs = nullptr;
      }

      //---------------------------------------------------------------------//
      // Destroy the Connection Obj:                                         //
      //---------------------------------------------------------------------//
      if (utxx::likely(m_conn != nullptr))
      {
        CHECK_ONLY(auto res =) cg_conn_destroy(m_conn);
        CHECK_ONLY
        (
          if (utxx::unlikely(res != 0))
          {
            LOG_ERROR(1,
              "EConnector_P2CGate_FORTS_MDC::Dtor: Failed to Destroy the "
              "Connection Obj: {}", CGErrCodeT(res).c_str())
            ok = false;
          }
        )
        // XXX: Still, reset the ptr:
        m_conn = nullptr;
      }

      //---------------------------------------------------------------------//
      // Close the P2CGate Env:                                              //
      //---------------------------------------------------------------------//
      if (utxx::likely(s_isEnvInited))
      {
        CHECK_ONLY(auto res =) cg_env_close();
        CHECK_ONLY
        (
          if (utxx::unlikely(res != 0))
          {
            LOG_ERROR(1,
              "EConnector_P2CGate_FORTS_MDC::Dtor: Failed to Close the Env: {}",
              CGErrCodeT(res).c_str())
            ok = false;
          }
        )
        // XXX: Still, reset the flag in any case:
        s_isEnvInited = false;
      }

      //---------------------------------------------------------------------//
      // Log the Msg:                                                        //
      //---------------------------------------------------------------------//
      if (utxx::likely(ok))
        { LOG_INFO(1, "EConnector_P2CGate_FORTS_MDC::Dtor: Finalised Cleanly") }
      else
        { LOG_WARN(1,
          "EConnector_P2CGate_FORTS_MDC::Dtor: Finalised but with Errors!")    }
    }
    catch(...){}
  }

  //=========================================================================//
  // "Start" (XXX: SYNCHRONOUS / BLOCKING!):                                 //
  //=========================================================================//
  void EConnector_P2CGate_FORTS_MDC::Start()
  {
    //-----------------------------------------------------------------------//
    // Open the Connection and wait for it to become Active:                 //
    //-----------------------------------------------------------------------//
    // If we got here, we must be initialised in the first place:
    assert(s_isEnvInited && m_conn != nullptr && m_lsnOBs != nullptr);

    // Delays before re-testing the state are 10 sec:
    timespec const waitTime { 0, 10'000'000 };

    // XXX: The wait params are currently hard-wrired. Total wait <= 1 sec:
    for (int i = 0; i < 100; ++i)
    {
      unsigned   state    =  0;
      CHECK_ONLY(auto res =) cg_conn_getstate(m_conn, &state);
      CHECK_ONLY
      (
        if (utxx::unlikely(res != 0))
          throw utxx::runtime_error
                ("EConnector_P2CGate_FORTS_MDC::Start: ConnGetState Failed: ",
                 CGErrCodeT(res).c_str());
      )
      //---------------------------------------------------------------------//
      // Examine the Connection State:                                       //
      //---------------------------------------------------------------------//
      if (state == CG_STATE_CLOSED)
      {
        // Yes, initiate  Connection Opening:
        CHECK_ONLY(res =) cg_conn_open(m_conn, nullptr);
        CHECK_ONLY
        (
          if (utxx::unlikely(res != 0))
            throw utxx::runtime_error
                  ("EConnector_P2CGate_FORTS_MDC::Start: Failed to Open the "
                   "Connection: ", CGErrCodeT(res).c_str());
        )
      }
      else
      if (state == CG_STATE_ACTIVE)
      {
        // The wait is completed (or maybe the Connection was already Active --
        // this is OK):
        LOG_INFO(2,
          "EConnector_P2CGate_FORTS_MDC::Start: P2MQRouter Connection is now "
          "Active")
        break;
      }
      else
      if (state == CG_STATE_OPENING)
      {
        // Then wait:
        (void) nanosleep(&waitTime, nullptr);
        continue;
      }
      else
        // Most likely, the Connection is in an Error state:
        throw utxx::runtime_error
              ("EConnector_P2CGate_FORTS_MDC::Start: Got Invalid ConnState=",
               CGConnStateT(state).c_str());
    }

    //-----------------------------------------------------------------------//
    // Then Open the Listener(s):                                            //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) should we use the same or longer time-out here? --  Allow for 5 sec;
    // (*) the Trades Listener does not exist unless "WithTrades" flag  is set:
    //
    assert((m_lsnTrs != nullptr) == m_withExplTrades);

    for (int i = 0; i < 500; ++i)
    {
      //---------------------------------------------------------------------//
      // Get 1 or 2 Listener States:                                         //
      //---------------------------------------------------------------------//
      unsigned   stateOBs    = 0;
      unsigned   stateTrs    = 0;

      CHECK_ONLY(auto resOBs =) cg_lsn_getstate(m_lsnOBs, &stateOBs);

      CHECK_ONLY(auto resTrs =)
        (m_lsnTrs != nullptr)
        ? cg_lsn_getstate(m_lsnTrs, &stateTrs)
        : 0;

      CHECK_ONLY
      (
        if (utxx::unlikely(resOBs != 0 || resTrs != 0))
          throw utxx::runtime_error
                ("EConnector_P2CGate_FORTS_MDC::Start: LsnGetState Failed: "
                 "OBs=",   CGErrCodeT(resOBs).c_str(),
                 ", Trs=", CGErrCodeT(resTrs).c_str());
      )
      bool opening =  false;
      if (stateOBs == CG_STATE_CLOSED)
      {
        // Yes, Open the OrderBooks Listener. Before that, clean-up the state:
        ResetOrderBooksAndOrders();

        CHECK_ONLY(resOBs =)
          cg_lsn_open(m_lsnOBs, nullptr);  // No extra params here!

        CHECK_ONLY
        (
          if (utxx::unlikely(resOBs != 0))
            throw utxx::runtime_error
                  ("EConnector_P2CGate_FORTS_MDC::Start: Failed to Open the "
                   "Order Books Listener: ", CGErrCodeT(resOBs).c_str());
        )
        opening = true;
      }

      if (m_lsnTrs != nullptr && stateTrs == CG_STATE_CLOSED)
      {
        // Yes, also Open the Trades Listener.   NB: It is opened in the OnLine
        // Mode (as opposed to the default SnapShot+OnLine),   so we will  just
        // start getting new updates from the Listener opening time -- all past
        // Trades are dropped. This is what we actually need:
        //
        CHECK_ONLY(resTrs =) cg_lsn_open(m_lsnTrs, "mode=online");
        CHECK_ONLY
        (
          if (utxx::unlikely(resTrs != 0))
            throw utxx::runtime_error
                  ("EConnector_P2CGate_P2CGate::Start: Failed to Open the "
                   "Trades Listener: ", CGErrCodeT(resTrs).c_str());
        )
        opening = true;
      }

      // If we just initiated Opening of 1 or 2 Listeners, go to the next iter:
      if (opening)
        continue;

      //---------------------------------------------------------------------//
      // Otherwise: Check the Active Status of Listeners:                    //
      //---------------------------------------------------------------------//
      if (stateOBs == CG_STATE_ACTIVE     &&
         (m_lsnTrs == nullptr || stateTrs == CG_STATE_ACTIVE))
      {
        LOG_INFO(1,
          "EConnector_P2CGate_FORTS_MDC::Start: OrderBooks Listener Active, "
          "Trades Listener is {}",
          (m_lsnTrs != nullptr) ? "Active" : "NotUsed")
        break;  // OK, our wait is completed
      }

      // Otherwise, any mixture of "Active" and "Opening" states is OK: It means
      // we continue our wait:
      if ((stateOBs == CG_STATE_ACTIVE || stateOBs == CG_STATE_OPENING) &&
          (m_lsnTrs == nullptr         ||
           stateTrs == CG_STATE_ACTIVE || stateTrs == CG_STATE_OPENING))
      {
        (void) nanosleep(&waitTime, nullptr);
        continue;
      }

      // In all other cases: Got an error:
      throw utxx::runtime_error
            ("EConnector_P2CGate_FORTS_MDC::Start: Got Invalid ListenerState"
             "(s): OBs=", CGConnStateT(stateOBs).c_str(),
             ", Trs=",    (m_lsnTrs != nullptr) ?
                          CGConnStateT(stateTrs).c_str() : "NotUsed");
    }

    //-----------------------------------------------------------------------//
    // Create a "GenericHandler" and attach it to the Reactor:               //
    //-----------------------------------------------------------------------//
    assert(m_reactor != nullptr);

    IO::FDInfo::GenericHandler gh
      ([this]()->void { ProcessCGEvents(); });

    m_reactor->AddGeneric(m_name.data(), gh);

    //-----------------------------------------------------------------------//
    // We are now Active: Notify the subscribed Strategies:                  //
    //-----------------------------------------------------------------------//
    m_isActive = true;

    // ON=true, for all SecIDs, no ExchTS, has ConnectorTS:
    ProcessStartStop(true, nullptr, 0, utxx::time_val(), utxx::now_utc());

    LOG_INFO(1, "EConnector_P2CGate_FORTS_MDC::Start: CONNECTOR STARTED!")
    // "Start" has been completed:
  }

  //=========================================================================//
  // "Stop":                                                                 //
  //=========================================================================//
  void EConnector_P2CGate_FORTS_MDC::Stop()
  {
    // XXX: Here we rely upon the following flag, rather than on "cg_conn_get-
    // state" and "cg_lsn_getstate" calls:
    //
    if (utxx::unlikely(!m_isActive))
    {
      LOG_INFO(2, "EConnector_P2CGate_FORTS_MDC::Stop: Already Inactive")
      return;
    }

    //-----------------------------------------------------------------------//
    // First, Detach us from the Reactor:                                    //
    //-----------------------------------------------------------------------//
    assert(m_reactor != nullptr);

    if (utxx::unlikely(!(m_reactor->RemoveGeneric(m_name.data())) ))
      LOG_ERROR(2,
        "EConnector_P2CGate_FORTS_MDC::Stop: Failed to Detach {} from the "
        "EPollReactor",  m_name)
      // But go on in spite of that...

    //-----------------------------------------------------------------------//
    // Then close the Listeners:                                             //
    //-----------------------------------------------------------------------//
    // Here and below, any errors are logged, but they do not trigger exceptions
    // -- we want to complete the "Stop" to the maximum extent!):
    //
    // Trades Listener:
    assert(m_withExplTrades || m_lsnTrs == nullptr);
    if (m_lsnTrs != nullptr)
    {
      CHECK_ONLY(auto res =) cg_lsn_close(m_lsnTrs);
      CHECK_ONLY
      (
        if (utxx::unlikely(res != 0))
        LOG_ERROR(1,
          "EConnector_P2CGate_FORTS_MDC::Stop: Failed to Close the Trades "
          "Listener: {}", CGErrCodeT(res).c_str())
      )
    }
    // XXX: Reset the Listener ptr in any case:
    m_lsnTrs = nullptr;

    // OrderBooks Listener (normally present):
    if (utxx::likely(m_lsnOBs != nullptr))
    {
      CHECK_ONLY(auto res =) cg_lsn_close(m_lsnOBs);
      CHECK_ONLY
      (
        if (utxx::unlikely(res != 0))
          LOG_ERROR(1,
            "EConnector_P2CGate_FORTS_MDC::Stop: Failed to Close the Order"
            "Books Listener: {}", CGErrCodeT(res).c_str())
      )
    }
    // XXX: Reset the Listener ptr in any case, and for safety, clean-up the
    // state:
    m_lsnOBs    = nullptr;
    ResetOrderBooksAndOrders();

    //-----------------------------------------------------------------------//
    // Close the Connection:                                                 //
    //-----------------------------------------------------------------------//
    if (utxx::likely(m_conn != nullptr))
    {
      CHECK_ONLY(auto res =) cg_conn_destroy(m_conn);
      CHECK_ONLY
      (
        if (utxx::unlikely(res != 0))
          LOG_ERROR(1,
            "EConnector_P2CGate_FORTS_MDC::Stop: Failed to Close the "
            "Connection: {}",   CGErrCodeT(res).c_str())
      )
    }
    // XXX: Reset the Connection ptr in any case:
    m_conn = nullptr;

    //-----------------------------------------------------------------------//
    // Notify the subscribed Strategies:                                     //
    //-----------------------------------------------------------------------//
    m_isActive = false;

    // ON=false, for all SecIDs, no ExchTS, has ConnectorTS:
    ProcessStartStop(false, nullptr, 0, utxx::time_val(), utxx::now_utc());

    LOG_INFO(1, "EConnector_P2CGate_FORTS_MDC::Stop: CONNECTOR STOPPED")
    // NB: Still return "true" because "Stop" has been completed:
  }

  //=========================================================================//
  // "ProcessCGEvents":                                                      //
  //=========================================================================//
  void EConnector_P2CGate_FORTS_MDC::ProcessCGEvents()
  {
    // Normally, the Connection must be Active, but take care of the unexpected:
    if (utxx::unlikely(!m_isActive || m_conn == nullptr))
      return;

    // The following call fetches the incoming events from the "P2MQRouter" and
    // eventually invokes the Listeners and our Call-Backs. Use TimeOut=0,  so
    // if there are no such events, the Reactor will immediately proceed to the
    // normal EPoll:
    CHECK_ONLY(auto res =) cg_conn_process(m_conn, 0, nullptr);

    // XXX: If "res" indicates a Time-Out, it is OK: it simply means that there
    // are no events to process(???):
    CHECK_ONLY
    (
      if (utxx::unlikely(res != 0 && res != CG_ERR_TIMEOUT))
      {
        // Log the error and Stop the Connector:
        LOG_ERROR(1,
          "EConnector_P2CGate_FORTS_MDC::ProcessCGEvents: Got {}, Stopping the"
          " Connector...", CGErrCodeT(res).c_str())
        Stop();
      }
    )
  }

  //=========================================================================//
  // OrderBooks Listener Call-Back:                                          //
  //=========================================================================//
  unsigned EConnector_P2CGate_FORTS_MDC::OrderBooksLsnCB
  (
    cg_conn_t*      DEBUG_ONLY(a_conn),
    cg_listener_t*  DEBUG_ONLY(a_lsn),
    cg_msg_t*       a_msg
  )
  {
    //-----------------------------------------------------------------------//
    // Get the MsgType:                                                      //
    //-----------------------------------------------------------------------//
    assert(a_conn == m_conn && a_lsn == m_lsnOBs && a_msg != nullptr);
    unsigned msgType = a_msg->type;

    LOG_INFO(3,
      "EConnector_P2CGate_FORTS_MDC::OrderBooksLsnCB: Got MsgType={}, IsOnLine"
      "={}", CGMsgT(msgType).c_str(), m_isOnLineOBs)

    switch (msgType)
    {
      //---------------------------------------------------------------------//
      case CG_MSG_CLOSE:
      //---------------------------------------------------------------------//
        // The Stream has been unexpectedly closed, so Stop the whole Connector:
        Stop();
        return 0;

      //---------------------------------------------------------------------//
      case CG_MSG_P2REPL_ONLINE:
      //---------------------------------------------------------------------//
        // The Stream is now in the OnLine mode; SnapShots initialisation is
        // complete:
        LOG_INFO(2,
          "EConnector_P2CGate_FORTS_MDC::OrderBooksLsnCB: Switching to OnLine "
          "Mode{}", m_isOnLineOBs ? ": AGAIN???" : "")
        m_isOnLineOBs = true;
        return 0;

      //---------------------------------------------------------------------//
      case CG_MSG_STREAM_DATA:
      //---------------------------------------------------------------------//
      {
        // Generic Case: MktData  update, but it may be an OrdBook SnapShot, or
        // an OrdLog Increment -- depending on the "m_isOnLineOBs" flag:
        //
        cg_msg_streamdata_t const* sdm =
          reinterpret_cast<cg_msg_streamdata_t const*>(a_msg);

        CHECK_ONLY
        (
          size_t maxIdx =
            m_isOnLineOBs
            ? P2CGate::OrdLog::sys_events_index
            : P2CGate::OrdBook::info_index;

          if (utxx::unlikely
             (sdm->data == nullptr   || sdm->data_size == 0 ||
              sdm->msg_index > maxIdx))
          {
            LOG_WARN(2,
              "EConnector_P2CGate_FORTS_MDC::OrderBooksLsnCB: Invalid Msg"
              "StreamData for MsgIdx={}, MsgID={}, Rev={}, IsOnLine={}",
              sdm->msg_index, sdm->msg_id, sdm->rev, m_isOnLineOBs)
            return CG_ERR_INVALIDARGUMENT;
          }
        )
        // If OK: Determine the Msg Sub-Type. Most of the time, we will be in
        // the OnLine mode, of course:
        if (utxx::unlikely(!m_isOnLineOBs &&
                           sdm->msg_index == P2CGate::OrdBook::orders_index))
        {
          //-----------------------------------------------------------------//
          // OrderBook SnapShot:                                             //
          //-----------------------------------------------------------------//
          P2CGate::OrdBook::orders const* tmsg =
            reinterpret_cast<P2CGate::OrdBook::orders const*>(sdm->data);

          bool    ok = Process
            (*tmsg,      sdm->data_size, sdm->owner_id, sdm->msg_id, sdm->rev,
             sdm->nulls, sdm->num_nulls, sdm->user_id);
          return (ok ? 0 : CG_ERR_INVALIDARGUMENT);
        }
        else
        if (utxx::likely(m_isOnLineOBs &&
                         sdm->msg_index == P2CGate::OrdLog::orders_log_index))
        {
          //-----------------------------------------------------------------//
          // OrderLog Incremental:                                           //
          //-----------------------------------------------------------------//
          P2CGate::OrdLog::orders_log const* tmsg =
            reinterpret_cast<P2CGate::OrdLog::orders_log const*>(sdm->data);

          bool    ok = Process(*tmsg, sdm->data_size);
          return (ok ? 0 : CG_ERR_INVALIDARGUMENT);
        }
        else
          // Any other Sub-Types are currently ignored, XXX incl "sys_events"
          // (the latter could be used to track the SessID, but  this is not
          // done yet, nor in FAST...):
          LOG_INFO(3,
            "EConnector_P2CGate_FORTS_MDC::OrderBooksLsnCB(MsgStreamData): Got"
            " MsgSubType={}: Ignored", sdm->msg_index)
        break;
      }

      //---------------------------------------------------------------------//
      default: ;
      //---------------------------------------------------------------------//
        // XXX: Most of other Msg Types are disregarded, for example:
        // (*) CG_MSG_OPEN  is not used to confirm our "Active" status  --  it
        //     actually means that the Scheme has been initialised, but we are
        //     not using dynamic / custim schemas anyway;
        // (*) we assume that OrderLog updates are non-transactional, so we ig-
        //     nore the CG_MSG_TN_* msgs...
    }
    // All Done:
    return 0;
  }

  //=========================================================================//
  // Trades Listener Call-Back:                                              //
  //=========================================================================//
  unsigned EConnector_P2CGate_FORTS_MDC::TradesLsnCB
  (
    cg_conn_t*      DEBUG_ONLY(a_conn),
    cg_listener_t*  DEBUG_ONLY(a_lsn),
    cg_msg_t*       a_msg
  )
  {
    //-----------------------------------------------------------------------//
    // Get the MsgType:                                                      //
    //-----------------------------------------------------------------------//
    assert(a_conn == m_conn && a_lsn == m_lsnOBs && a_msg != nullptr);
    unsigned msgType = a_msg->type;

    LOG_INFO(3,
      "EConnector_P2CGate_FORTS_MDC::TradesLsnCB: GotMsgType={}",
      CGMsgT(msgType).c_str())

    // All Done:
    return 0;
  }

  //=========================================================================//
  // Process (OrderBooks SnapShot):                                          //
  //=========================================================================//
  inline bool EConnector_P2CGate_FORTS_MDC::Process
  (
    P2CGate::OrdBook::orders    const& UNUSED_PARAM(a_tmsg),
    size_t                             CHECK_ONLY  (a_len),
    long                               UNUSED_PARAM(a_owner_id),
    unsigned                           UNUSED_PARAM(a_msg_id),
    long                               UNUSED_PARAM(a_rev),
    uint8_t const*                     UNUSED_PARAM(a_nulls),
    size_t                             UNUSED_PARAM(a_nn),
    uint64_t                           UNUSED_PARAM(a_user_id)
  )
  {
    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
/*
cerr << "OwnerID="   << a_owner_id << ", MsgID=" << a_msg_id << ", Rev="
     << a_rev        << ", NN=" << a_nn << ", UserID=" << a_user_id
     << ", ReplID="  << a_tmsg.replID << ", ReplRev=" << a_tmsg.replRev
     << ", ReplAct=" << a_tmsg.replAct << endl;
*/
    // This msg can only be received in the "OffLine" mode:
    assert(!m_isOnLineOBs);

    // Check the static and dynamic msg size:
    static_assert
      (sizeof(P2CGate::OrdBook::orders) == P2CGate::OrdBook::sizeof_orders,
       "P2CGate::OrdBook::orders size mismatch");

    CHECK_ONLY
    (
      if (utxx::unlikely(a_len != P2CGate::OrdBook::sizeof_orders))
      {
        LOG_ERROR(2,
          "EConnector_P2CGate_FORTS_MDC::Process(P2CGate::OrdBook::orders): "
          "Invalid MsgLen={}: Expected={}",
        a_len,  P2CGate::OrdBook::sizeof_orders)

        // XXX: This is a serious error which threatens the OrderBook consist-
        // ency:
        return false;
      }
    )
    //-----------------------------------------------------------------------//
    // OK, Actually Process the Msg:                                         //
    //-----------------------------------------------------------------------//
    // Skip unrelated msgs:
/*
    bool ok =
      UpdateOrderBooks
      <
        true,               //  IsSnapShot
        false,              // !IsMultiCast
        true,               //  WithIncrUpdates
        true,               //  WithOrders
        false,              // !WithRelaxedOBs
        true,               //  ChangeIsPartFill
        false,              // !NewEncdAsChange
        false,              // !ChangeEncdAsNew
        FindOrderBookBy::SecID,
        EConnector_TWIME_FORTS
      >
      (
        0,      //  No SeqNums
        a_tmsg,
        true    //  Yes, this is a DynInitMode
      );
    return ok;
*/
    return true;
  }

  //=========================================================================//
  // Process (OrderLog Incremental):                                         //
  //=========================================================================//
  inline bool EConnector_P2CGate_FORTS_MDC::Process
  (
    P2CGate::OrdLog::orders_log const& UNUSED_PARAM(a_tmsg),
    size_t                             CHECK_ONLY  (a_len)
/*
    uint8_t const*                     a_nulls,
    size_t                             a_nn
*/
  )
  {
    //-----------------------------------------------------------------------//
    // Check the Args:                                                       //
    //-----------------------------------------------------------------------//
    // This msg can only be received in the "OnLine" mode:
    assert(m_isOnLineOBs);

    // Check the static and dynamic msg size:
    static_assert
      (sizeof(P2CGate::OrdLog::orders_log) ==
        P2CGate::OrdLog::sizeof_orders_log,
       "P2CGate::OrdBook::orders_log size mismatch");

    CHECK_ONLY
    (
      if (utxx::unlikely(a_len != P2CGate::OrdLog::sizeof_orders_log))
      {
        LOG_ERROR(2,
          "EConnector_P2CGate_FORTS_MDC::Process(P2CGate::OrdLog::orders_log):"
          " Invalid MsgLen={}, Expected={}",
          a_len, P2CGate::OrdLog::sizeof_orders_log)

        // XXX: This is a serious error which threatens the OrderBook consist-
        // ency:
        return false;   // CG_ERR_INVALIDARGUMENT
      }
    )
    //-----------------------------------------------------------------------//
    // OK, Actually Process the Msg:                                         //
    //-----------------------------------------------------------------------//
    return true;
  }
}
