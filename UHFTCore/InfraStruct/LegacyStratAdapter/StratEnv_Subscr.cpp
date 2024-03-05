// vim:ts=2:et:sw=2
//===========================================================================//
//                            "StratEnv_Subscr.cpp":                         //
//                     Subscriptions and Remote Management                   //
//===========================================================================//
#ifndef  JSON_USE_MSGPACK
#define  JSON_USE_MSGPACK
#endif
#ifdef   WITH_CONNEMU
#undef   WITH_CONNEMU
#endif

#include "Common/Maths.h"
#include "Common/Args.h"
#include "Connectors/Json.h"
#include "Infrastructure/MiscUtils.h"
#include "Connectors/Configs_DB.h"
#include "StrategyAdaptor/StratEnv.h"
#include "StrategyAdaptor/SecDefsShM.h"
#include <Infrastructure/Marshal.h>
#include <Infrastructure/Reactor.h>
#ifdef   WITH_CONNEMU
#include "StrategyAdaptor/ConnEmu.h"
#endif
#include <boost/filesystem.hpp>
#include <utxx/compiler_hints.hpp>
#include <utxx/verbosity.hpp>
#include <utxx/time_val.hpp>
#include <eixx/eterm.hpp>
#include <Infrastructure/Logger.h>
#include <cstring>
#include <cstdlib>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <csignal>
#include <unistd.h>

namespace MAQUETTE
{
  bool StratEnv::s_ExitSignal = false;
}

namespace
{
  using namespace MAQUETTE;
  using namespace Arbalete;
  using namespace std;

  //=========================================================================//
  // Signal Handler:                                                         //
  //=========================================================================//
  void SignalHandler(int sig_no)
    { StratEnv::s_ExitSignal = true; }

  //=========================================================================//
  // Is it a Real or Emulated Mode?                                          //
  //=========================================================================//
  bool IsEmulatedMode
  (
    vector<string> const& mdcs,
    vector<string> const& omcs
  )
  {
#   ifdef WITH_CONNEMU
    // Use  the DISJUNCTION of conditions to set the Strategy in the Emulated
    // Mode, as it would be the most dangerous situatiuon if a Strategy inten-
    // ded for emulation, starts to work in real mode:
    //
    if (getenv("CONNEMU_USE_SEGMENT")   != NULL ||
        getenv("CONNEMU_SEGMENT_SIZE")  != NULL ||
        getenv("CONNEMU_SIMULATE_FROM") != NULL ||
        getenv("CONNEMU_SIMULATE_TO")   != NULL)
    {
      // Yes, emulated mode has been requested, which requires OEs and MDs to
      // be the same:
      if (utxx::unlikely(mdcs != omcs))
        throw invalid_argument
              ("IsEmulatedMode: Emulated Mode requires MDs==OEs");
      else
        return true;
    }
    else
#   endif
      // NB: If "ConnEmu" is de-configured, always return "false":
      return false;
  }

  //=========================================================================//
  // "MkMsgQName":                                                           //
  //=========================================================================//
  string MkMsgQName
  (
    string const& a_q_name,
    string const& a_strat_name
    //EnvType       a_env_type
  )
  {
    if (a_q_name == "unique")
    {
      // dynInstName = stratName-timeStamp:
      // where "stratName" would normally be like "subtype-exch-env", eg
      //                                          "mm5-cnx-prod":
      char buff[256];
      DateTime now = CurrUTC();
      Date     d   = now.date();
      Time     t   = now.time_of_day();
      snprintf(buff, 256, "%s-%04d%02d%02d-%02d%02d%02d",
               boost::to_lower_copy(a_strat_name).c_str(),
               //boost::to_lower_copy(string(a_env_type.to_string())).c_str(),
               int(d.year()), int(d.month()), int(d.day()),
               t.hours(),     t.minutes(),    t.seconds());
      return string(buff);
    }
    else
    if (a_q_name.empty())
      return boost::to_lower_copy(a_strat_name);
             //(a_strat_name + '-' + string(a_env_type.to_string()));
    else
      // Use the "dynInstName" as specified, but still add the envType:
      return a_q_name;
  }
}

namespace MAQUETTE
{
  using namespace Arbalete;
  using namespace std;

  //=========================================================================//
  // "StratEnv::ConnDescr": Non-Default Ctor:                                //
  //=========================================================================//
  StratEnv::ConnDescr::ConnDescr
  (
    StratEnv*       env,
    string   const& conn_name,
    ConnModeT       conn_mode,
    int             conn_ref,
    bool            is_real     // false - emulated mode
  )
  : m_owner       (env),
    m_connName    (conn_name),
    m_is_real     (),           // Will actually be set below
    m_conn_mode   (conn_mode),
    m_ref         (conn_ref),
    m_segment     (),           // MD only
    m_err_storage (nullptr),
    m_reqQ        (),           // May be installed later (in "StratEnv" Ctor)
    m_uds         (-1),
    m_eventFD     (-1),
    m_graceful    (true)        // Try to do graceful Unsubscribe...
  {
    // So is it a "real" Connector?
    int   len = int(conn_name.size());
    m_is_real = is_real &&
                !(len > 3 && conn_name.substr(len-3, 3) == "-DB" &&
                  conn_mode == ConnModeT::MD);

    if (!is_real)
      // This means we are in the Emulated Mode; only storing the "conn_name"
      // is required. In this case, the Connector is not "real" either (even
      // if it is marked as an OE, because OEs and MDs are same then):
      return;

    //-----------------------------------------------------------------------//
    // Open the Connector's ShM segment (MDs & OEs):                         //
    //-----------------------------------------------------------------------//
    string segm_name =
      conn_mode == ConnModeT::MD
      ? GetMktDataSegmName         (conn_name, Env())
      : GetOrderStateMgrSegmName   (conn_name, Env());

    void*  segm_maddr =
      conn_mode == ConnModeT::MD
      ? GetMktDataSegmMapAddr      (conn_name, Env())
      : GetOrderStateMgrSegmMapAddr(conn_name, Env());

    try
    {
      m_segment =
        make_shared<BIPC::fixed_managed_shared_memory>
        (
          BIPC::open_only,
          segm_name.c_str(),
          segm_maddr
        );
    }
    catch (exception const& exc)
    {
      SLOG(ERROR)
        << "StratEnv::ConnDescr::Ctor: Cannot open the "
        << conn_mode.to_string() << " shm segment '" << segm_name << "': "
        << exc.what() << '\n';
      throw;
    }

    if (conn_mode == ConnModeT::OE)
    {
      auto attach_type = utxx::persist_attach_type::OPEN_READ_WRITE;

      try
      {
        (void)InitAOSStorage
          (m_connName,   *m_segment,    segm_name,       attach_type,
          m_aos_storage, m_req_storage, m_trade_storage, out(m_err_storage));
      }
      catch (std::exception const& e)
      {
        LOG_ERROR("StratEnv::ConnDescr::Ctor: failed to open '%s' OE connector"
                  " storage '%s': %s\n",
                  m_connName.c_str(), segm_name.c_str(), e.what());
        throw;
      }

      //---------------------------------------------------------------------//
      // Open the Connector's ReqQ (OEs only):                               //
      //---------------------------------------------------------------------//
      // "ReqQ" is used to send up Indications. This is a ShMem Queue with the
      // same name as that of the Connector (OE):
      string segm_name = GetOrderMgmtReqQName(conn_name, Env());
      try
      {
        m_reqQ =
          make_shared<fixed_message_queue>
          (
            BIPC::open_only,
            segm_name.c_str(),
            GetOrderMgmtReqQMapAddr(conn_name, Env())
          );
      }
      catch (exception const& exc)
      {
        LOG_ERROR("StratEnv::ConnDescr::Ctor: Cannot open the ReqQ of OEC=%s"
                  ": %s\n", segm_name.c_str(), exc.what());
        throw;
      }
      assert(m_reqQ.get() != NULL);

      //---------------------------------------------------------------------//
      // UDS for receiving EventFD:                                          //
      //---------------------------------------------------------------------//
      string sockName = GetOrderMgmtUDSName(conn_name, Env());

      // FileExists() doesn't work for special files. Why???
      if (boost::filesystem::exists(sockName) && m_is_real)
      {
        struct sockaddr_un address;
        memset(&address, 0, sizeof(struct sockaddr_un));
        address.sun_family = AF_UNIX;
        // NB: The output of "snprintf" is always guaranteed to be 0-terminated,
        // even if the buffer was full:
        snprintf(address.sun_path, sizeof(address.sun_path), sockName.c_str());

        m_uds = socket(PF_UNIX, SOCK_STREAM, 0);
        if (m_uds < 0 ||
            connect(m_uds, (struct sockaddr *) &address,
                     sizeof(struct sockaddr_un)) < 0)
          SystemError
            ("StratEnv::ConnDescr::Ctor: Could not open/connect UDS: "
             + sockName);

        // TODO: Send authentication information
        eixx::eterm login =
          eixx::eterm::format
            ("{login, ~s, ~i, ~s, ~s}",
             StratName().c_str(), getpid(), "" /* User */, "" /* Pwd */);

        // Write the login command
        try   { ErlWrite(m_uds, login); }
        catch ( std::exception const& e )
        {
          throw utxx::runtime_error("StratEnv::ConnDescr::Ctor: Could not login "
                                    "to UDS server: ", sockName, ": ", e.what());
        }

        try   { RecvFD(m_uds, out(m_eventFD)); }
        catch ( std::exception const& e )
        {
          throw utxx::runtime_error("StratEnv::ConnDescr::Ctor: Could not "
                                    "receive EventFD through UDS server ",
                                    sockName, ": ", e.what());
        }
        if (DebugLevel() >= 1)
          LOG_INFO("StratEnv::ConnDescr::Ctor: Received eventFD=%d from %s\n",
                    m_eventFD, conn_name.c_str());

        // Turn off blocking mode
        Blocking(m_eventFD, false);

        //------------------------------------------------------------------//
        // Create constant Erlang terms for future pattern-matching:        //
        //------------------------------------------------------------------//
        dynamic_io_buffer buf(256);
        eixx::eterm login_reply;

        try   { login_reply = ErlRead(m_uds, buf, true); }
        catch ( std::exception const& e )
        {
          throw utxx::runtime_error("StratEnv::ConnDescr::Ctor: No login "
                                    "response from UDS server: ",
                                    sockName, ": ", e.what());
        }

        static const eixx::eterm s_login_ack =
          eixx::eterm::format("{ok,    {session, SID::int()}}");
        static const eixx::eterm s_login_rej =
          eixx::eterm::format("{error, Reason}");

        eixx::varbind binding;
        if (!s_login_ack.match(login_reply, &binding))
          throw utxx::runtime_error("StratEnv::ConnDescr::Ctor: Invalid "
                                    "UDS reply: ", login_reply.to_string());
        if (DebugLevel() >= 1)
          LOG_INFO("StratEnv::ConnDescr::Ctor: Using eventFD=%d from server, "
                   "Session=%016lX\n", m_eventFD, binding["SID"]->to_long());
      }
      else
      if (DebugLevel() >= 1)
        LOG_INFO("StratEnv::ConnDescr::Ctor: UDS not in use (real=%s): %s\n",
                 utxx::to_string(m_is_real), sockName.c_str());
    }
  }

  StratEnv::ConnDescr::~ConnDescr()
  {
    if (m_uds != -1)
    {
      ::close(m_uds);
      ::close(m_eventFD);
      m_uds     = -1;
      m_eventFD = -1;
    }
  }

  //=========================================================================//
  // "StratEnv::ConnDescr": Move Ctor:                                       //
  //=========================================================================//
  StratEnv::ConnDescr::ConnDescr(ConnDescr&& a_rhs)
  {
    *this = std::move(a_rhs);
  }

  void StratEnv::ConnDescr::operator=(ConnDescr&& a_rhs)
  {
    m_connName     = std::move(a_rhs.m_connName);
    m_is_real      = a_rhs.m_is_real;
    m_conn_mode    = a_rhs.m_conn_mode;
    m_ref          = a_rhs.m_ref;
    m_segment      = std::move(a_rhs.m_segment);
    m_aos_storage  = std::move(a_rhs.m_aos_storage);
    m_req_storage  = std::move(a_rhs.m_req_storage);
    m_trade_storage= std::move(a_rhs.m_trade_storage);
    m_err_storage  = a_rhs.m_err_storage;
    m_reqQ         = std::move(a_rhs.m_reqQ);
    m_uds          = a_rhs.m_uds         ; a_rhs.m_uds          = -1;
    m_eventFD      = a_rhs.m_eventFD     ; a_rhs.m_eventFD      = -1;
    m_graceful     = a_rhs.m_graceful    ; a_rhs.m_graceful     = false;

    a_rhs.m_err_storage = nullptr;
  }

  //=========================================================================//
  // "StratEnv": Non-Default Ctor:                                           //
  //=========================================================================//
  StratEnv::StratEnv
  (
    string         const& strat_type,     // Type    (eg "MM")
    string         const& strat_subtype,  // Subtype (eg "GatlingMM")
    string         const& strat_name,     // Strategy Name (eg "mm5-cnx")
    EnvType               env_type,       // Environment (Prod, QA, Test)
    string         const& q_name,         // Eg "cnx-mm5-test-DATETIME"
    vector<string> const& mdcs,
    vector<string> const& omcs,
    SecDefSrcPair  const& secdef_src,    // SecDef source
    StartModeT            start_mode,
    int                   debug_level,
    int                   rt_prio,
    int                   cpu_affinity,
    int                   shmem_sz_mb,
    int                   pos_rate_thr_window
  )
  : m_envType         (env_type),
    m_stratType       (strat_type),
    m_stratSubType    (strat_subtype),
    m_stratName       (strat_name),
    m_strat_cid       (InvalidClientID()),
    m_strat_sid       (0),
    m_msgQName        (MkMsgQName(q_name, strat_name)),
    m_startMode       (start_mode),
    m_emulated        (IsEmulatedMode(mdcs, omcs)),
    // Event Queue: Created below (XXX: use raw ptr: easier to delete):
    m_eventQ          (NULL),
    m_MDs             (),
    m_OEs             (&m_InternalOEs),
    m_debugLevel      (debug_level),
    m_MDsMap          (mdcs.size()),
    m_OEsMap          (omcs.size()),
    m_QSymInfoMap     (),
    m_RNG             (utxx::now_utc().usec()),
    m_Distr           (),
    m_Exit            (false),
    m_C3Thread        (NULL),
    m_req_timeout_sec (5),
    m_MDNames         (NULL),
    m_OENames         (NULL),
    m_QSyms           (NULL),
    m_Ccys            (NULL),
    m_pos_rate_thr_win(pos_rate_thr_window),
    m_positions       (NULL),
    m_Greeks          (NULL),
    m_ourTrades       (NULL),
    m_aoss            (NULL),
    m_orderBooks      (NULL),
    m_tradeBooks      (NULL),
    m_secDefs         (NULL),
    m_secdef_cache    (),      // Only for SecDefs used by this Strategy
    m_cashInfo        (NULL),
    m_totalInfo       (NULL),
    m_statusLine      (NULL),
    m_clean           (NULL),
    m_connEmu         (NULL),  // NB: This is a raw ptr
    m_updatables      (),
    m_nextUpdate      ()
    // All "unique_ptrs" not explicitly listed here, are initialised to NULL
  {
    //-----------------------------------------------------------------------//
    // Real-Time Behaviour:                                                  //
    //-----------------------------------------------------------------------//
    if (!m_emulated)
    {
      if (rt_prio > 0)
        // Try to elevate ourselves to the specified Real-Time Priority. Howe-
        // ver, this will succeed for privileged (E)UID only:
        SetRTPriority(rt_prio);

      if (cpu_affinity >= 0)
      {
        SetCPUAffinity(cpu_affinity);
        LOG_INFO("StratEnv::Ctor: Running on CPU#%d\n", cpu_affinity);
      }

      // Install the Signal Handlers:
      struct sigaction sigAct = { 0 };
      sigAct.sa_handler = SignalHandler;

      if (sigaction(SIGINT,  &sigAct, NULL) != 0 ||
          sigaction(SIGQUIT, &sigAct, NULL) != 0 ||
          sigaction(SIGTERM, &sigAct, NULL) != 0 ||
          sigaction(SIGPIPE, &sigAct, NULL) != 0)
        SystemError("StratEnv::Ctor: Cannot set up signal handlers");

      // Also, for diagnostic purposes, allow unlimited core dump size:
      struct rlimit rl = { 0 };
      rl.rlim_cur = RLIM_INFINITY;
      rl.rlim_max = RLIM_INFINITY;

      if (setrlimit(RLIMIT_CORE, &rl) != 0)
        SystemError("StratEnv::Ctor: Cannot set CoreDumpSize=UnLimited");
    }
    else
    {
      // Emulation does not make much sense in the Safe Mode:
      if (start_mode == StartModeT::Safe)
        throw invalid_argument
              ("StratEnv::Ctor: Emulated and Safe Modes are currently "
               "incompatible");

      LOG_INFO("Strategy %s started in the EMULATED Mode\n",
               m_stratName.c_str());
    }

    //-----------------------------------------------------------------------//
    // Shared Memory Segment holding the Data:                               //
    //-----------------------------------------------------------------------//
    // The Data Segment is always required; the Event Queue is not required in
    // the Emulated Mode:
    string segmNameData = GetStrategyDataSegmName(m_msgQName, m_envType);
    string segmNameEQ   = GetStrategyEventQName  (m_msgQName, m_envType);
    bool   removed      = false;

    // Open the segment, and again, if it exists and is marked non-clean from a
    // previous run, remove it and re-create:
    if (start_mode == StartModeT::Clean)
    {
      // Remove the Segments unconditionally:
      (void) fixed_message_queue::remove(segmNameEQ.c_str());
      BIPC::shared_memory_object::remove(segmNameData.c_str());
      removed    = true;  // Maybe it did not exist, but never mind...
    }
    else
    {
      try
      {
        // Check if the Data Segment already exists:
        m_segment.reset
          (new BIPC::fixed_managed_shared_memory
            (
              BIPC::open_only,
              segmNameData.c_str(),
              GetStrategyDataSegmMapAddr(m_msgQName, m_envType)
          ));

        // If we got here, the segment does exit -- but check if it is Clean.
        // XXX: If the "Clean" flag itself does NOT exist, we assume that the
        // segment is corrupted, and thus NOT clean:
        //
        pair<bool*, size_t> clean = m_segment->find<bool>("Clean");

        if (clean.first == NULL || clean.second != 1 || !(*(clean.first)))
        {
          // It is "dirty", so remove it (along with the MsgQ):
          (void) fixed_message_queue::remove(segmNameEQ.c_str());
          BIPC::shared_memory_object::remove(segmNameData.c_str());
          m_segment.reset();
          removed    = true;
        }
      }
      catch (...)
      {
        // It does NOT exist, so will have to create a new one...
        assert(!removed);     // Nothing to remove!
      }
    }

    //-----------------------------------------------------------------------//
    // Load SecDefs                                                          //
    //-----------------------------------------------------------------------//
    LoadSecDefs(secdef_src.first, secdef_src.second);

    //-----------------------------------------------------------------------//
    // So: create a new Data Segment?                                        //
    //-----------------------------------------------------------------------//
    long ShMemSz = shmem_sz_mb * 1024 * 1024;
    if (m_segment.get() == NULL)
    try
    {
      // XXX: Liberal segment permissions:
      m_segment.reset
        (new BIPC::fixed_managed_shared_memory
          (
            BIPC::create_only,
            segmNameData.c_str(),
            ShMemSz,
            GetStrategyDataSegmMapAddr(m_msgQName, m_envType),
            BIPC::permissions(0660)
        ));

      if (m_debugLevel >= 1)
        LOG_INFO("StratEnv::Ctor: opened shared memory segment %s (%ldM)\n",
                 segmNameData.c_str(), ShMemSz / 1024 / 1024);
    }
    catch (exception const& exc)
    {
      // This is currently fatal -- a Strategy cannot work without its Data
      // segment:
      LOG_ERROR("StratEnv::Ctor: Cannot create ShMem Data Segment: %s, "
                "size=%ld: %s\n", segmNameData.c_str(), ShMemSz, exc.what());
      exit(1);
    }

    // Finally, we must have got it:
    assert(m_segment.get() != NULL);

    //-----------------------------------------------------------------------//
    // Event Queue:                                                          //
    //-----------------------------------------------------------------------//
    // Create or open a new Event Queue;  the size should be moderate, as a too
    // long Queue is useless anyway -- it means that the Strategy is swamped by
    // incoming events (XXX: unless it is stopped, and GUI/MS trade on its beh-
    // alf!..)
    // If the Strategy goes down and then re-starts (same instance),  and the
    // Queue becomes full in the meantime,  the Strategy gets Unsubscribed by
    // the Connectors. However, this is OK -- it should re-subscribe when re-
    // started, rather than rely on a *long*  MsgQ which would become full of
    // obsolete msgs in that case:
    // EventQueue is required iff NOT in the Emulated Mode:
    if (!m_emulated)
    try
    {
      m_eventQ = new fixed_message_queue
      (
        BIPC::open_or_create,
        segmNameEQ.c_str(),
        131072,
        sizeof(Events::EventMsg),
        BIPC::permissions(0660),
        GetStrategyEventQMapAddr(m_msgQName, m_envType)
      );
    }
    catch (exception const& exc)
    {
      // This is currently fatal -- a Strategy cannot work without its EventQ:
      LOG_ERROR("StratEnv::Ctor: Cannot create ShMem Event Queue: %s: %s\n",
                segmNameEQ.c_str(), exc.what());
      exit(1);
    }

    //-----------------------------------------------------------------------//
    // Emulation?                                                            //
    //-----------------------------------------------------------------------//
#   ifdef WITH_CONNEMU
    // So create the "ConnEmu" if requested:
    if (m_emulated)
      m_connEmu  = new ConnEmu(m_msgQName, int(mdcs.size()), m_debugLevel);
#   endif

    //-----------------------------------------------------------------------//
    // Market Data Connectors (MDs):                                         //
    //-----------------------------------------------------------------------//
    // For each MD, create the corresp Descriptor.   This is for Non-Emulated
    // Mode only; in the Emulated Mode, we create an "InstrsMap" entry instead,
    // and at least 1 MD is mandatory:
    //
    if (mdcs.empty())
    {
      if (!m_emulated)
        LOG_WARNING("StratEnv::Ctor: No MDs configured\n");
      else
      {
        // Fatal error:
        LOG_ERROR("StratEnv::Ctor(Emulated): Cannot proceed without MDs\n");
        exit(1);
      }
    }

    for (int i = 0; i < int(mdcs.size()); ++i)
    {
      // Repeated Connector Names are allowed externally (for User convenience)
      // but not internally. So check uniqueness and map the name to its previ-
      // ous occurrence if not unique:
      bool duplicate = false;
      for (int j = 0; j < int(m_MDs.size()); ++j)
        if (m_MDs[j].m_connName == mdcs[i])
        {
          duplicate = true;
          m_MDsMap[i] = j;
          break;
        }
      if (duplicate)
        continue;

      // Otherwise: Generic Case:
      // Create a new Descriptor, connect it to the Connector via ZeroMQ and
      // save it in "m_MDs". (In the Emulated Mode, only the Connector Name
      // is saved):
      //
      int ref = m_MDs.size();
      m_MDs.emplace_back(this, mdcs[i], ConnModeT::MD, ref, true);

      m_MDsMap[i] = ref;

      // ZeroMQ communications: only in Non-Emulated Mode, and only if the
      // Connector is "real" (otherwise, we will only use SecDefs from it,
      // but will not subscribe to MktData via ZeroMQ):
      //
      if (!m_emulated && m_MDs.back().m_is_real)
      {
        XConnector::Status status = GetConnectorStatus(m_MDs.back());

        if (utxx::unlikely(status != XConnector::Status::Active))
          throw runtime_error("StratEnv::ConnDescr::Ctor: MD#" + to_string(i) +
                              " (" + mdcs[i] + ") is not active");
        // Check if this Connector is active; can throw an exception:
        LOG_INFO("StratEnv::Ctor: MD#%d: %s is alive\n", i, mdcs[i].c_str());

        // If the Event Queue was removed: For safety, Unsubscribe from the
        // corresp MD (so that we will not get a stale subscription):
        if (removed)
          UnsubscribeAll(m_MDs[ref]);
      }
      // Otherwise (Emulated Mode, or a DB Connector): wait for the first
      // "Subscribe"...
    }

    //-----------------------------------------------------------------------//
    // Order Management Connectors (OEs):                                    //
    //-----------------------------------------------------------------------//
    if (!m_emulated)
    {
      if (omcs.empty())
        LOG_WARNING("StratEnv::Ctor: No OE connectors configured\n");

      for (int i = 0; i < int(omcs.size()); ++i)
      {
        // Again, manage repeated Connector Names. XXX: HOWEVER, it is current-
        // ly NOT ALLOWED for "MDs" and "OEs" to have a non-empty intersecti-
        // on:
        if (utxx::unlikely
           (find(mdcs.begin(), mdcs.end(), omcs[i]) != mdcs.end()))
          throw invalid_argument
               ("StratEnv::Ctor: OE=" + omcs[i] + " is also declared an MD."
                 " This is currently not allowed");

        bool duplicate = false;
        for (int j = 0; j < int(m_InternalOEs.size()); ++j)
          if (m_InternalOEs[j].m_connName == omcs[i])
          {
            duplicate = true;
            m_OEsMap[i] = j;
            break;
          }
        if (duplicate)
          continue;

        // Otherwise: Generic Case:
        // Create the Descriptor, connect it to the Connector via ZeroMQ and
        // save it in "m_OEs": NB: Duplicate OE names are allowed (and may
        // be useful):
        //
        int ref = m_InternalOEs.size();
        m_InternalOEs.emplace_back(this, omcs[i], ConnModeT::OE, ref, true);

        m_OEsMap[i] = ref;

        XConnector::Status status = GetConnectorStatus(m_InternalOEs.back());

        if (utxx::unlikely(status != XConnector::Status::Active))
          throw runtime_error("StratEnv::ConnDescr::Ctor: OE#" + to_string(i) +
                              " (" + omcs[i] + ") is not active");
        // Check if this Connector is active; XXX: can throw an exception:
        LOG_INFO("StratEnv::Ctor: OEC#%d: %s is alive\n", i, omcs[i].c_str());

        // If the Event Queue was removed: For safety, Unsubscribe from the
        // corresp OE (so that we will not get a stale subscription):
        if (removed)
          UnsubscribeAll(m_InternalOEs[ref]);

        // Subscribe again to that OE -- without any Symbols:
        Subscribe(m_InternalOEs[ref], "", "ConnDescr::Ctor-OE");
      }
    }
    else
    {
      // In the Emulated Mode, OEs must be same as MDs (verified by "IsEmula-
      // tedMode"):
      assert(omcs == mdcs);
      m_OEs = &m_MDs;
    }

    //-----------------------------------------------------------------------//
    // Now create the ShM Allocator:                                         //
    //-----------------------------------------------------------------------//
    // Required in the Emulated Mode as well, because ShM data structures used
    // by the StratEnv itself, remain unchanged:
    //
    BIPC::fixed_managed_shared_memory::segment_manager* segMgr =
      m_segment->get_segment_manager();
    assert(segMgr != NULL);

    m_shMAlloc.reset(new ShMAllocator(segMgr));

    //-----------------------------------------------------------------------//
    // Data Structs in Shared Memory:                                        //
    //-----------------------------------------------------------------------//
    // If they exist (the Segment has not been re-allocated on Clean Start),
    // they will be re-used:
    //
    // "MDNames":
    m_MDNames      =
      m_segment->find_or_construct<StrVec>       ("MDNames")
      (*m_shMAlloc);

    for (int i = 0; i < int(m_MDs.size()); ++i)
      m_MDNames->push_back
      (ShMString(m_MDs[i].m_connName.c_str(), *m_shMAlloc));

    // "OENames":
    m_OENames      =
      m_segment->find_or_construct<StrVec>       ("OENames")
      (*m_shMAlloc);

    for (int i = 0; i < int(m_OEs->size()); ++i)
      m_OENames->push_back
      (ShMString((*m_OEs)[i].m_connName.c_str(), *m_shMAlloc));

    // "QSyms" and "Ccys":
    m_QSyms         =
      m_segment->find_or_construct<QSymVec>      ("QSymsVec")
      (*m_shMAlloc);

    m_Ccys          =
      m_segment->find_or_construct<CcyVec>       ("CcyVec")
      (*m_shMAlloc);

    // "Positions":
    m_positions     =
      m_segment->find_or_construct<PositionsMap> ("PositionsMap")
      (less<QSym>(), *m_shMAlloc);

    // "Greeks":
    m_Greeks        =
      m_segment->find_or_construct<GreeksMap>    ("GreeksMap")
      (less<QSym>(), *m_shMAlloc);

    // "OurTrades":
    m_ourTrades     =
      m_segment->find_or_construct<OurTradesMap> ("OurTradesMap")
      (less<QSym>(), *m_shMAlloc);

    // "AOSMapQPV":
    m_aoss          =
      m_segment->find_or_construct<AOSMapQPV>    ("AOSMapQPV")
      (less<QSym>(), *m_shMAlloc);

    // "OrderBooks":
    // NB: Do NOT re-use the existing Segment, because its data are now
    // obsolete:
    m_segment->destroy<OrderBooksMap>            ("OrderBooksMap");
    m_orderBooks    =
      m_segment->find_or_construct<OrderBooksMap>("OrderBooksMap")
      (less<QSym>(), *m_shMAlloc);

    // "TradeBooks":
    // NB: Do NOT re-use the existing Segment, because its data are now
    // obsolete:
    m_segment->destroy<TradeBooksMap>            ("TradeBooksMap");
    m_tradeBooks    =
      m_segment->find_or_construct<TradeBooksMap>("TradeBooksMap")
      (less<QSym>(), *m_shMAlloc);

    // "SecDefs":
    // NB: Do NOT re-use the existing Segment, because its data may now
    // be obsolete:
    m_segment->destroy<SecDefsMap>               ("SecDefsMap");
    m_secDefs       =
      m_segment->find_or_construct<SecDefsMap>   ("SecDefsMap")
      (less<QSym>(), *m_shMAlloc);

    // "CashInfo":
    m_cashInfo      =
      m_segment->find_or_construct<CashMap>      ("CashInfo")
      (less<Events::Ccy>(),  *m_shMAlloc);

    // "TotalInfo":
    m_totalInfo     =
      m_segment->find_or_construct<CashMap>      ("TotalInfo")
      (less<Events::Ccy>(),  *m_shMAlloc);

    // "StatusLine":
    m_statusLine    =
      m_segment->find_or_construct<ShMString>    ("StatusLine")
      (m_StatusLineSz, '\0', *m_shMAlloc);

    // "Clean": If created, mark the segment as "clean"; it is marked as "non-
    // clean" once first incoming event is received (because this would almost
    // certainly affect the ShM); mark as "clean" again before graceful exit:
    m_clean  =
      m_segment->find_or_construct<bool>         ("Clean")(true);
    *m_clean = true;    // If it did exist, re-confirm cleanness

    // Also, "m_QSyms" is used to check subscription uniqueness, so reset it
    // UNLESS we are in the Safe Mode:
    if (m_startMode != StartModeT::Safe)
      m_QSyms->clear();

    //-----------------------------------------------------------------------//
    // Start the C3 Thread:                                                  //
    //-----------------------------------------------------------------------//
    // NB: C3 Thread is not used in the Emulated Mode (because ZeroMQ is not
    // used either):
    if (!m_emulated)
    { m_C3Thread = NULL; /* new Thread([this]()->void { this->C3Loop(); }); */ }
    else
      assert(m_C3Thread == NULL);

    //-----------------------------------------------------------------------//
    // If the Safe Mode was requested, enter the Safe Mode Loop:             //
    //-----------------------------------------------------------------------//
    if (start_mode == StartModeT::Safe)
      SafeModeRun();
  }

  //=========================================================================//
  // "StratEnv" Dtor:                                                        //
  //=========================================================================//
  StratEnv::~StratEnv()
  {
#   ifdef WITH_CONNEMU
    if (!m_emulated)
    {
#   endif
      //---------------------------------------------------------------------//
      // Not Emulated:                                                       //
      //---------------------------------------------------------------------//
      // Be polite to the Connectors: Unsubscribe all symbols subscribed on
      // them, so that the Connectors may free up the corresp resources  --
      // XXX: unless "non-graceful" mode has been set, or the MD is a "DB"
      // one:
      for (int i = 0; i < int(m_MDs.size()); ++i)
        if (m_MDs[i].m_graceful && m_MDs[i].m_is_real)
          UnsubscribeAll(m_MDs[i]);

      // Remove all OEs as well -- use just "Unsubscribe" as no symbols are
      // involved anyway:
      for (int i = 0; i < int(m_OEs->size()); ++i)
        if ((*m_OEs)[i].m_graceful)
          UnsubscribeAll((*m_OEs)[i]);

      // Remove the Event Queue End-Point, if exists:
      if (m_eventQ != NULL)
      {
        delete m_eventQ;
        m_eventQ = NULL;
      }

      // Send cancellation to the C3Thread:
      if (m_C3Thread != NULL)
      {
        (void) pthread_cancel(m_C3Thread->native_handle());

        m_C3Thread->join();
        delete m_C3Thread;
        m_C3Thread = NULL;
      }

      // Mark the Data Segment as clean (check for non-NULL for safety):
      assert (m_clean != NULL);
      *m_clean = true;

#   ifdef WITH_CONNEMU
    }
    else
    {
      //---------------------------------------------------------------------//
      // Emulated Mode:                                                      //
      //---------------------------------------------------------------------//
      // Always remove the ShM data segment:
      string segmNameData = GetStrategyDataSegmName(m_msgQName, m_envType);
      BIPC::shared_memory_object::remove           (segmNameData.c_str());

      // TODO: De-Allocate "ConnEmu":
    }
#   endif
  }

  //=========================================================================//
  // "Subscribe":                                                            //
  //=========================================================================//
  //=========================================================================//
  // MD version -- with Symbol:                                              //
  //=========================================================================//
  void StratEnv::Subscribe(QSym& a_qsym)
  {
    //=======================================================================//
    // Verify / re-write the "QSym" if necessary:                            //
    //=======================================================================//
    // First of all, check that the Symbol is non-empty:
    //
    CheckQSymAOS(a_qsym, NULL, "Subscribe");

    // If there were repeated Connector Names in the "StratEnv" Ctor, the user
    // provides a "qsym" to "Subscribe" following same numbering convention;
    // re-write the "qsym" to map MD# and/or OE# to unique internal numbers:
    //
    if (a_qsym.m_mdcN != -1)
    {
      // Verify that the original "mdcN" is OK, so we can do re-writing:
      if (utxx::unlikely
         (a_qsym.m_mdcN < 0 || a_qsym.m_mdcN >= int(m_MDsMap.size())))
        throw invalid_argument
              ("StratEnv::Subscribe(MD): Invalid QSym="+ a_qsym.ToString() +
               " (before re-writing)");

      // Perform re-writing:
      int oldMDN  = a_qsym.m_mdcN;
      int newMDN  = m_MDsMap[oldMDN];
      a_qsym.m_mdcN = newMDN;

      if (oldMDN != newMDN)
        LOG_INFO("StratEnv::Subscribe(MD): QSym=%s: Re-mapped MD#: %d --> "
                 "%d\n", a_qsym.ToString().c_str(), oldMDN, newMDN);

      // Verify once again that the new "mdcN" is valid:
      if (utxx::unlikely
         (a_qsym.m_mdcN < 0 || a_qsym.m_mdcN >= int(m_MDs.size())))
        throw runtime_error
              ("StratEnv::Subscribe(MD): Invalid QSym="+ a_qsym.ToString() +
               " (after re-writing)");
    }

    // Similarly for OEs:
    if (a_qsym.m_omcN != -1)
    {
      // Verify that the original "omcN" OK, so we can do re-writing:
      if (utxx::unlikely
         (a_qsym.m_omcN < 0 || a_qsym.m_omcN >= int(m_OEsMap.size())))
        throw invalid_argument
              ("StratEnv::Subscribe(MD-OE): Invalid QSym="+ a_qsym.ToString() +
               " (before re-writing)");

      // Perform re-writing:
      int oldOEN  = a_qsym.m_omcN;
      int newOEN  = m_OEsMap[oldOEN];
      a_qsym.m_omcN = newOEN;

      // Verify once again that the new "omcN" is valid:
      if (utxx::unlikely
         (a_qsym.m_omcN < 0 || a_qsym.m_omcN >= int(m_OEs->size())))
        throw runtime_error
              ("StratEnv::Subscribe(MD-OE): Invalid QSym="+ a_qsym.ToString() +
               " (after re-writing)");

      if (oldOEN != newOEN)
        LOG_INFO("StratEnv::Subscribe(MD-OE): QSym=%s: Re-mapped OE#: "
                 "%d --> %d\n", a_qsym.ToString().c_str(), oldOEN, newOEN);
    }

    //-----------------------------------------------------------------------//
    // Check Uniqueness of (SymKey, MD#) and (SymKey, OE#):                  //
    //-----------------------------------------------------------------------//
    // While "SymKey" does not need to be unique, the combination of SymKey and
    // each MD and OE does;  ie,  within a given OE or MD, "SymKey" must be
    // unique:
    for (int i = 0; i < int(m_QSyms->size()); ++i)
      if (utxx::unlikely
         ( (*m_QSyms)[i].m_symKey == a_qsym.m_symKey &&
          ((*m_QSyms)[i].m_mdcN   == a_qsym.m_mdcN   ||
           (*m_QSyms)[i].m_omcN   == a_qsym.m_omcN)))
        throw runtime_error
              ("StratEnv::Subscribe(MD): QSym="+ a_qsym.ToString()
              +" is already subscribed (A)?");

    //=======================================================================//
    // Get "SecDef" for this Symbol from the corresp MD:                     //
    //=======================================================================//
    // Will have to iterate over all "SecDef"s of that MD if (BEFARE!) the MD
    // is present at all!
    // XXX: But we need a valid MD# which may not be available for this QSym!
    // In that case, look for an already-subscribed QSym with the  same SymKey
    // and a valid MD:
    //
    int effMDN = a_qsym.m_mdcN;
    if (effMDN == -1)
      for (int i = 0; i < int(m_QSyms->size()); ++i)
        if ((*m_QSyms)[i].m_symKey == a_qsym.m_symKey)
        {
          effMDN = (*m_QSyms)[i].m_mdcN;
          LOG_WARNING("StratEnv::Subscribe(MD): QSym=%s: Obtaining effective "
                      "MD# from another QSym=%s\n", a_qsym.ToString().c_str(),
                      (*m_QSyms)[i].ToString().c_str());
          break;
        }
    if (utxx::unlikely(effMDN == -1))
      throw runtime_error
            ("StratEnv::Subscribe(MD): QSym=" + a_qsym.ToString()
            +": Cannot get a valid MD# to obtain SecDef");

    // If we got a valid MD#:
    assert(0 <= effMDN && effMDN < int(m_MDs.size()));
    ConnDescr const& thisMD = m_MDs[effMDN];

    //-----------------------------------------------------------------------//
    // Use a DB to get the "SecDef" (OK in any mode):                        //
    //-----------------------------------------------------------------------//
#   ifdef WITH_CONNEMU
    Events::ObjName dbName =
      Events::MkFixedStr<Events::ObjNameSz>(thisMD.m_connName);
#   endif

    // NB: If this is just a DB Connector, put the SecDef into "m_secDefs-
    // Buff"; an Emulator needs to do some work on top of that (sending an
    // Event to the Strategy):
    //
    SecDef const* sdef =
#   ifdef WITH_CONNEMU
      m_emulated
      ? m_connEmu->GetSecDef(dbName, a_qsym.m_symKey, out(m_secdef_cache))
      : GetSecDef           (a_qsym.m_symKey);
#   else
      GetSecDef             (a_qsym.m_symKey);
#   endif

    if (sdef != NULL)
      (*(this->m_secDefs))[a_qsym] = sdef;

    //-----------------------------------------------------------------------//
    // So have we eventually got a "SecDef" for this "QSym"?                 //
    //-----------------------------------------------------------------------//
    assert(m_secDefs != NULL);
    SecDefsMap::const_iterator sdit = m_secDefs->find(a_qsym);

    if (utxx::unlikely(sdit == m_secDefs->end()))
      // Probably invalid "QSym" -- was not in Connector's SecDefs; or no data
      // in the Connector:
      throw invalid_argument
            ("StratEnv::Subscribe(MD): QSym="+ a_qsym.ToString()
            +": SecDef not available");

    // Otherwise: Get the Ccy for this QSym. XXX: IMPORTANT: Use the Settlemnt
    // Ccy rather than just Ccy, eg for FX (For/Dom), Ccy=For and SettlCcy=Dom,
    // use the latter:
    //
    assert(sdit->second != NULL);
    SecDef      const& qdef = *(sdit->second);
    Events::Ccy const& ccy  = qdef.m_SettlCcy;

    // Check if the Ccy is valid after all:
    if (utxx::unlikely(Events::IsEmpty(ccy)))
      throw runtime_error("StratEnv::Subscribe(MD): QSym=" + a_qsym.ToString() +
                          ": Empty Ccy");

    //=======================================================================//
    // Construct the "QSymInfo":                                             //
    //=======================================================================//
    QSymInfo qInfo;                         // Initially empty

    // Common flds:
    qInfo.m_This      = a_qsym;
    strncpy(qInfo.m_CFICode, qdef.m_CFICode, 6);
    qInfo.m_Ccy       = qdef.m_SettlCcy;    // NB:  Again, use "SettlCcy"!
    qInfo.m_IR        = 0.0;                // XXX: No IR data feed yet...

    // Just in case, check that this "qinfo" is not on the Map already:
    if (utxx::unlikely(m_QSymInfoMap.find(a_qsym) != m_QSymInfoMap.end()))
      throw runtime_error
            ("StratEnv::Subscribe(MD): QSym=" + a_qsym.ToString()
            +" is already subscribed (B)?");

    if (qdef.m_CFICode[0] == 'O')
    {
      //---------------------------------------------------------------------//
      // This is an Option, so do need to extract the Underlying:            //
      //---------------------------------------------------------------------//
      Events::SymKey const& underSym = qdef.m_UnderlyingSymbol;
      if (utxx::unlikely(*(underSym.data()) == '\0'))
        throw runtime_error
              ("StratEnv::Subscribe(MD): QSym="+ a_qsym.ToString()
              +": Option with Empty Underlying");

      // Find the (unique) underlying for this Option in the "DerPatchMap" con-
      // structed so far (and also check once again that "qsym" itself is not
      // repeated):
      QSym qUnd;  // Initially empty

      for (QSymInfoMap::const_iterator cit = m_QSymInfoMap.begin();
           cit != m_QSymInfoMap.end(); ++cit)
      {
        if ((cit->first).m_symKey == underSym)
        {
          // Yes, the Underlying has been found -- but continue searching to
          // make sure that there are no  similarly-named  underlyings which
          // would not be resolvable in that case:
          if (utxx::likely(qUnd == QSym()))
            qUnd = cit->first;
          else
            throw runtime_error
                  ("StratEnv::Subscribe(MD): Repeated underlyings for QSym=" +
                   a_qsym.ToString() + ": UnderlQSym=" + qUnd.ToString());
        }
      }
      // So: have we got the Underlying?
      if (utxx::unlikely(qUnd == QSym()))
        throw runtime_error
              ("StratEnv::Subscribe(MD): QSym="+ a_qsym.ToString() +
               ", Underlying=" + Events::ToString(underSym)        +
               ": Must subscribe to the Underlying first");

      // If we got it, install it in the "qInfo". If the Expiration (Maturity)
      // time for the Option itself is given, use it; otheriwe (eg on FORTS),
      // will have to take it from the Underlying:
      //
      qInfo.m_Underlying = qUnd;
      qInfo.m_Expir      = qdef.m_Maturity
                         ? utxx::time_val(qdef.m_Maturity, 0)
                         : m_QSymInfoMap[qUnd].m_Expir;
      qInfo.m_Strike     = qdef.m_Strike;
      qInfo.m_ImplVol    = qdef.m_Volatility;

      if (utxx::unlikely(qInfo.m_Strike <= 0.0 || qInfo.m_ImplVol <= 0.0))
        throw runtime_error
              ("StratEnv::Subscribe(MD): QSym=" + a_qsym.ToString()
              +": Invalid Strike=" + to_string(qInfo.m_Strike)
              + " and/or ImplVol=" + to_string(qInfo.m_ImplVol));
    }
    else
    {
      //---------------------------------------------------------------------//
      // This is a Futures, Equity, FX contract etc: Underlying is itself:   //
      //---------------------------------------------------------------------//
      // Assume (though this is not exactly correct from the theoretical point
      // of view) that it serves as its own underlying:
      qInfo.m_Underlying = a_qsym;
      qInfo.m_Expir      = utxx::time_val(qdef.m_Maturity, 0);
      qInfo.m_Strike     = 0.0;
      qInfo.m_ImplVol    = 0.0;
    }

    // At the end, verify once again that if "qInfo" corresponds to an Options
    // or Futures, it must have a valid Expiration time:
    //
    if (utxx::unlikely
       ((qInfo.m_CFICode[0] == 'O' || qInfo.m_CFICode[0] == 'F') &&
         qInfo.m_Expir.empty()))
      throw runtime_error
            ("StratEnv::Subscribe(MD): QSym="+ a_qsym.ToString() +
             ": Empty Expiration");

    // Attach the "qInfo" to the Map:
    m_QSymInfoMap[a_qsym] = qInfo;

    //=======================================================================//
    // Subscribe to the given MD for MktData on this Symbol:                 //
    //=======================================================================//
    if (a_qsym.m_mdcN != -1 && m_startMode != StartModeT::Safe)
    {
      assert(0 <= a_qsym.m_mdcN && a_qsym.m_mdcN < int(m_MDs.size()));
      ConnDescr const& thisMD =  m_MDs[a_qsym.m_mdcN];

      if (!m_emulated && thisMD.m_is_real)
      {
        Subscribe(thisMD, a_qsym.GetSymbol(), "Subscribe-MD");
      }
#     ifdef WITH_CONNEMU
      else
      if (m_emulated)
      {
        // In the Emulated Mode (or in case of a DB Connector), just install
        // this Symbol in the Instrs list:
        Events::ObjName dbName = thisMD.m_connName;

        assert(m_connEmu.get() != NULL);
        // NB: Ref = mdcN:
        m_connEmu->Subscribe(dbName, a_qsym.m_symKey, a_qsym.m_mdcN);
      }
#     endif
      else
        // DB-Recording Conenctor: Nothing else to do:
        assert(!thisMD.Real());
    }

    //-----------------------------------------------------------------------//
    // Create entries in the ShMem Vectors / Maps:                           //
    //-----------------------------------------------------------------------//
    // If this "QSym" is not installed yet (NB: the above uniqueness check does
    // NOT guarantee that "QSym" by itself is unique!), put it in.
    // XXX: From this point on, ShM is NOT clean:
    //
    assert(m_clean != NULL);
    *m_clean = false;

    //-----------------------------------------------------------------------//
    // "QSym":                                                               //
    //-----------------------------------------------------------------------//
    if (find(m_QSyms->begin(), m_QSyms->end(), a_qsym) == m_QSyms->end())
      m_QSyms->push_back(a_qsym);                    // "QSymsVec"

    //-----------------------------------------------------------------------//
    // "ccy" (if it is not yet in):                                          //
    //-----------------------------------------------------------------------//
    assert(m_Ccys != NULL);

    if (find(m_Ccys->begin(), m_Ccys->end(), ccy) == m_Ccys->end())
    {
      // This Ccy entry does NOT exist in ShM (NB: it could have existed from
      // a previous run):
      // Then the corresp Cash and TotalCash entries should not exist either:
      assert(m_cashInfo != NULL);
      if (m_cashInfo->find(ccy) != m_cashInfo->end())
      {
        LOG_WARNING("StratEnv::Subscribe(MD): Ccy=%s is not yet in, but its "
                    "CashInfo is --> clearing it\n", ccy.data());
        m_cashInfo->erase(ccy);
      }
      // Install an empty placeholder into CashInfo:
      (*m_cashInfo)[ccy]    = CashInfo();

      assert(m_totalInfo != NULL);
      if (m_totalInfo->find(ccy) != m_totalInfo->end())
      {
        LOG_WARNING("StratEnv::Subscribe(MD): Ccy=%s is not yet in, but its "
                    "TotalCash is --> clearing it\n", ccy.data());
        m_totalInfo->erase(ccy);
      }

      // Install the "ccy" itself:
      m_Ccys->push_back(ccy);
    }
    //-----------------------------------------------------------------------//
    // Position:                                                             //
    //-----------------------------------------------------------------------//
    // (May already exist in ShM from a previous run, in which case we leave
    // that map untouched):
    //
    assert(m_positions != NULL);
    if (m_positions->find(a_qsym) == m_positions->end())
      // Install "PositionInfo". NB: AccCrypt is not known yet, hence 0:
      m_positions->insert(make_pair
      (
        a_qsym,
        PositionInfo(a_qsym, 0, 0, 0.0, 0.0, 0.0, 0.0, m_pos_rate_thr_win)
      ));

    //-----------------------------------------------------------------------//
    // Trades:                                                               //
    //-----------------------------------------------------------------------//
    // At the moment, just insert an empty "OurTradesVector" for this "qsym":
    //
    (void) m_ourTrades->insert                      // "OurTradesMap"
      (make_pair(a_qsym, OurTradesVector(*m_shMAlloc)));
    //
    // NB: All "qsym"s are dufferent, so if we got to this point w/o except-
    // ion, "insert" must succeed -- unless there are left-over data in ShM!..
    // So no "assert"!..

    //-----------------------------------------------------------------------//
    // "AOSes":                                                              //
    //-----------------------------------------------------------------------//
    // XXX: Similarly, insert an empty "AOSPtrVec" for this "qsym":
    //
    (void) m_aoss->insert                           // "AOSMapQPV"
      (make_pair(a_qsym, AOSPtrVec(*m_shMAlloc)));

    // XXX: Cannot initialise "OrderBooksMap", "TradeBooksMap" and "SecDefsMap"
    // yet -- they contain ptrs which can only be received from the Connector...
  }

  //-------------------------------------------------------------------------//
  // "Subscribe":                                                            //
  //-------------------------------------------------------------------------//
  eterm StratEnv::Subscribe
    (ConnDescr const& a_conn, std::string const& a_sym, const char* where)
  {
    // Make the SUBSCRIBE request.
    // IMPORTANT: Ref is the MD#, so when Events with this Ref are sent
    // back, we know which Connector they are coming from; this is neces-
    // sary for identifying its ShMem segment, so the Event data  can be
    // accessed; "prio" is currently not used and not transmitted:
    //
    static const eterm s_subscribe =
      eterm::format("{subscribe, Sym::string(), Ref::int()}");
    static const atom am_var_sym("Sym");
    static const atom am_var_ref("Ref");

    // Send request over UDS:
    return SimpleReqResp
    (
      a_conn,
      s_subscribe.apply({{am_var_sym, a_sym}, {am_var_ref, a_conn.Ref()}}),
      where
    );
  }

  //=========================================================================//
  // "Unsubscribe":                                                          //
  //=========================================================================//
  //=========================================================================//
  // MD version -- with "QSym":                                              //
  //=========================================================================//
  void StratEnv::Unsubscribe(QSym const& qsym)
  {
    CheckQSymAOS(qsym, NULL, "Unsubscribe");
    if (qsym.m_mdcN == -1)
      return;   // This QSym does not use an MD, nothing to do:

    // Otherwise: Gneric Case:
    if (utxx::unlikely(qsym.m_mdcN < 0 || qsym.m_mdcN >= int(m_MDs.size())))
      throw invalid_argument
            ("StratEnv::Unsubscribe: Invalid QSym: "+ qsym.ToString());

    ConnDescr const& thisMD = m_MDs[qsym.m_mdcN];

    if (!m_emulated && thisMD.m_is_real)
    {
      //---------------------------------------------------------------------//
      // Non-Emulated, Non-DB Mode: Make an UNSUBSCRIBE request to that MD:  //
      //---------------------------------------------------------------------//
      static const eterm s_unsubscribe =
        eterm::format("{unsubscribe, Sym::string()}");
      static const atom am_var_sym("Sym");
      static const atom am_var_ref("Ref");

      // Send the request to Connector:
      auto req = s_unsubscribe.apply
        ({{am_var_sym, qsym.GetSymbol()}, {am_var_ref, qsym.m_mdcN}});

      const char* where = "Unsubscribe-MD";
      CheckOK(SimpleReqResp(thisMD, req, where), where);

      // Remove this "QSym" from the ShMem vector:
      assert(m_QSyms != NULL);

      bool erased = false;
      for (QSymVec::iterator it = m_QSyms->begin(); it != m_QSyms->end(); ++it)
        if (*it == qsym)
        {
          m_QSyms->erase(it);
          erased = true;
          break;
        }
      if (!erased)
        LOG_WARNING("StratEnv::Unsubscribe: QSym=%s not found in m_QSyms\n",
                    qsym.ToString().c_str());
    }
#   ifdef WITH_CONNEMU
    else
    if (m_emulated)
    {
      //---------------------------------------------------------------------//
      // Emulated Mode:                                                      //
      //---------------------------------------------------------------------//
      assert(m_connEmu.get() != NULL);

      // Get the Connector / DB Name:
      Events::ObjName dbName(thisMD.m_connName);

      m_connEmu->Unsubscribe(dbName, qsym.m_symKey);
    }
#   endif
    else
      //---------------------------------------------------------------------//
      // DB Mode: Nothing else to do:                                        //
      //---------------------------------------------------------------------//
      assert(!thisMD.m_is_real);
  }

  //=========================================================================//
  // "UnsubscribeAll":                                                       //
  //=========================================================================//
  // This is an MD version only:
  //
  void StratEnv::UnsubscribeAll(ConnDescr const& a_conn)
  {
#   ifdef WITH_CONNEMU
    // Emulation Mode:
    if (m_emulated)
    {
      assert(m_connEmu.get() != NULL);
      m_connEmu->UnsubscribeAll(a_conn.Name());
      return;
    }
#   endif

    // Generic Case: Make the UNSUBSCRIBE_ALL request (if this is not a DB-Re-
    // cording MD):
    if (a_conn.Real())
    {
      static const eterm s_unsubscribe = eterm::format("{unsubscribe, all}");

      // Send the request to Connector.
      const char* where = "UnsubscribeAll";
      CheckOK(SimpleReqResp(a_conn, s_unsubscribe, where), where);
    }

    // Remove all QSyms with this "mdcN" from the ShMem vector. NB:  "m_QSyms"
    // is NULL if "UnsubscribeAll" is invoked from the Ctor for initial clean-
    // up:
    if (m_QSyms != NULL)
    {
      int ref = a_conn.Ref();
      for (int i = 0; i < int(m_QSyms->size()); )
        if ((a_conn.Mode() == ConnModeT::MD && (*m_QSyms)[i].m_mdcN == ref) ||
            (a_conn.Mode() == ConnModeT::OE && (*m_QSyms)[i].m_omcN == ref))
          m_QSyms->erase(m_QSyms->begin() + i);
        else
          ++i;  // Go to next...
    }
  }

  //=========================================================================//
  // Remote Management:                                                      //
  //=========================================================================//
  void StratEnv::CheckOK(const eterm& a_result, const char* where) const
  {
    static const eterm s_ok       = atom("ok");
    static const eterm s_error    = eterm::format("{error, Why}");
    static const atom  am_var_why = atom("Why");

    varbind binding;

    if (s_ok.match(a_result))
      return;
    else if (s_error.match(a_result, &binding))
      throw utxx::runtime_error
        ("StratEnv::SimpleReqResp: ", where, " error response from server: ",
         binding[am_var_why]->to_string());
    else
      throw utxx::runtime_error
        ("StratEnv::SimpleReqResp: ", where, " invalid response from server: ",
         a_result.to_string());
  }

  //-------------------------------------------------------------------------//
  // "SimpleReqResp":                                                        //
  //-------------------------------------------------------------------------//
  eterm StratEnv::SimpleReqResp
  (
    ConnDescr const& conn,
    eterm     const& request,
    char      const* where
  )
  const
  {
    if (utxx::unlikely(m_debugLevel >= 5))
      SLOG(INFO) << "StratEnv::SimpleReqResp(name="
                 << conn.m_connName     << ") -> "
                 << request.to_string() << " ["  << where << ']' << endl;

    utxx::basic_io_buffer<1024> buf;
    ErlWrite(conn.m_uds, request);
    eterm reply = ErlRead(conn.m_uds, buf.to_dynamic(), true);

    if (utxx::unlikely(m_debugLevel >= 5))
      SLOG(INFO) << "StratEnv::SimpleReqResp(name="
                 << conn.m_connName     << ") <- "
                 << reply.to_string()   << " ["  << where << ']' << endl;
    return reply;
  }

  //-------------------------------------------------------------------------//
  // "GetConnectorStatus":                                                   //
  //-------------------------------------------------------------------------//
  XConnector::Status StratEnv::GetConnectorStatus(ConnDescr const& a_conn) const
  {
    static const eterm s_status = atom("status");
    static const eterm s_reply  = eterm::format("{status, Status::atom()}");
    static const atom  am_var_status("Status");

    static const atom  am_active("active");

    // NB: Do not check the reply for "OK" here:
    eterm reply = SimpleReqResp(a_conn, s_status, "GetConnectorStatus");

    varbind binding;
    if (!s_reply.match(reply, &binding))
    {
      SLOG(ERROR) << "StratEnv::GetConnectorStatus: invalid reply from server "
                  << a_conn.Name() << ": "         << s_reply.to_string()
                  << " (assuming Inactive status)" << endl;
      return XConnector::Status::Inactive;
    }

    auto& status_str = binding[am_var_status]->to_atom().to_string();

    auto result = XConnector::Status::from_string(status_str);

    if (result == XConnector::Status::UNDEFINED)
    {
      SLOG(WARNING) << "StratEnv::GetConnectorStatus: Invalid Status of "
                    << a_conn.Name() << ": " << status_str << endl;
    }

    return result;
  }

  //=========================================================================//
  // "C3Loop":                                                               //
  //=========================================================================//
  /*
  void StratEnv::C3Loop()
  {
    assert(m_cmdSock1.get() != NULL);

    EPollReactor reactor;

    while (!s_ExitSignal)
    {
      Json resp;
      try
      {
        //-------------------------------------------------------------------//
        // Wait for next request from a Client (eg Management Server):       //
        //-------------------------------------------------------------------//
        // However, to prevent indefinite blocking, use "poll"; time-out is in
        // milliseconds:
        //
        zmq::pollitem_t pollItem = { (void*)(*m_cmdSock1), -1, ZMQ_POLLIN, 0 };
        zmq::poll(&pollItem, 1, 1000);

        if ((pollItem.revents & ZMQ_POLLIN) == 0)
          // No event: Time-out has occured: Next iteration:
          continue;

        // Otherwise, there is an incoming event:
        zmq::message_t    reqMsg;
        m_cmdSock1->recv(&reqMsg);

        Json js;
        if (!js.msgunpack(reqMsg.data(), reqMsg.size(), NULL))
        {
          resp["command"] = "error";
          resp["message"] = "Unparsable command.";
        }
        else
        //-------------------------------------------------------------------//
        // Interpret the Command:                                            //
        //-------------------------------------------------------------------//
        //-------------------------------------------------------------------//
        if (strcmp(js["command"], "quit") == 0)
        //-------------------------------------------------------------------//
        {
          // Set the Exit Flag, but do NOT exit immediately -- the main thread
          // may ignore this msg, actually!
          m_Exit.store(true);
          resp["command"] = "ok";
        }
        else
        //-------------------------------------------------------------------//
        if (strcmp(js["command"], "ping") == 0)
        //-------------------------------------------------------------------//
          resp["command"] = "pong";
        else
        //-------------------------------------------------------------------//
        if (strcmp(js["command"], "parameters") == 0)
        //-------------------------------------------------------------------//
        {
          map<string, Json> const& jMap = js["parameters"].GetObject();
          bool   ok = true;
          string unrecognised;  // Empty if "ok"

          // Unpack the list of updates and put it into "m_nextUpdate";   the
          // updates will be picked up from there next time "GetNextEvent" is
          // run:
          m_nextUpdate.clear();

          for (map<string, Json>::const_iterator cit = jMap.begin();
               cit != jMap.end();              ++cit)
          {
            // Check each key in the "nextUpdate" against the dictionary of
            // declared of updatable variables; any mismatch will result in
            // the whole update being rejected, as doing partial updates may
            // lead to inconsistencies:
            //
            string const& varName = cit->first;
            double        varVal  = double(cit->second);

            map<string, double*>::iterator it = m_updatables.find(varName);

            if (it == m_updatables.end())
            {
              // Invalid variable name:
              ok           = false;
              unrecognised = varName;
              m_nextUpdate.clear();
              break;
            }
            else
              // Name is OK, so record this update (to be applied later):
              m_nextUpdate[varName] = varVal;
          }

          // Make the response:
          if (ok)
            resp["command"] = "ok";
          else
          {
            resp["command"] = "error";
            resp["message"] = "Unrecognised parameter: "+ unrecognised;
          }
        }
        //-------------------------------------------------------------------//
        else
        //-------------------------------------------------------------------//
        {
          // Unrecognised command:
          resp["command"]         = "notImplemented";
          resp["originalCommand"] = js["command"];
        }
      }
      catch (zmq::error_t const& err)
      {
        // Log the error and return:
        LOG_ERROR("StratEnv::C3Loop: ZeroMQ ERROR, EXITING: %s\n", err.what());
        m_Exit.store(true);
        break;
      }
      catch (exception const& exc)
      {
        // Any other exception: send an error msg back to the Client:
        resp["command"] = "error";
        resp["message"] = exc.what();
      }

      //---------------------------------------------------------------------//
      // Send the response out:                                              //
      //---------------------------------------------------------------------//
      // Again, exit on any errors:
      try
      {
        SendJS(*m_cmdSock1, resp);
      }
      catch (zmq::error_t const& err)
      {
        LOG_ERROR("StratEnv::C3Loop: ZeroMQ ERROR, EXITING: %s\n", err.what());
        m_Exit.store(true);
        break;
      }
    }
    // End of "while" loop.
    // Have we exited it via a signal?
    if (s_ExitSignal)
    {
      LOG_INFO("StratEnv::C3Loop: Exited via a Signal\n");
      // Confirm the over-all exit:
      m_Exit.store(true);
      s_ExitSignal = false;
    }
  }
  */

  //=========================================================================//
  // "SafeModeRun":                                                          //
  //=========================================================================//
  void StratEnv::SafeModeRun()
  {
    //-----------------------------------------------------------------------//
    // Subscribe to OEs, Unsubscribe from MDs:                               //
    //-----------------------------------------------------------------------//
    // Get "QSym"s from the Positions Map, not from "QSymVec", as the latter
    // was cleared by the previous Dtor:
    //
    LOG_INFO("StartEnv::SafeModeRun: Subscribing to QSyms...\n");
    assert(m_positions != NULL);
    vector<QSym> qsyms;

    for (PositionsMap::const_iterator cit = m_positions->begin();
         cit != m_positions->end(); ++cit)
      qsyms.push_back(cit->first);

    for (vector<QSym>::iterator it = qsyms.begin(); it != qsyms.end();  ++it)
    {
      QSym qsym  =  *it;
      Unsubscribe(qsym);
      Subscribe  (qsym);
      LOG_INFO("StratEnv::SafeModeRun: Subscribed: %s\n",
               qsym.ToString().c_str());
    }
    //-----------------------------------------------------------------------//
    // Run a simple Event Loop:                                              //
    //-----------------------------------------------------------------------//
    LOG_INFO("StartEnv::SafeModeRun: Running...\n");
    while (true)
    {
      StratEnv::ConnEventMsg msg;
      bool got = GetNextEvent(&msg, utxx::time_val(utxx::rel_time(1, 0)));
      if (!got)
        continue;

      // If exit is requested, exit immediately:
      if (msg.GetTag() == Events::EventT::StratExit)
        break;
    }
    //-----------------------------------------------------------------------//
    // On Exit:                                                              //
    //-----------------------------------------------------------------------//
    // Run the Dtor on ourselves, and really exit:
    //
    LOG_INFO("StartEnv::SafeModeRun: Finalising...\n");
    this->~StratEnv();

    LOG_INFO("StartEnv::SafeModeRun: Exiting...\n");
    exit(0);
  }

  //=========================================================================//
  // "StartModeOfStr":                                                       //
  //=========================================================================//
  StratEnv::StartModeT StratEnv::StartModeOfStr(string const& str)
  {
    if (str == "Safe")
      return StartModeT::Safe;
    else
    if (str == "Normal")
      return StartModeT::Normal;
    else
    if (str == "Clean")
      return StartModeT::Clean;
    else
      throw invalid_argument
            ("StratEnv::StartModeOfStr: Invald Mode=" + str);
  }

  //=========================================================================//
  // Dynamically-Updatable Strategy Variables:                               //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "DeclareUpdatable":                                                     //
  //-------------------------------------------------------------------------//
  // XXX: Once declared, an updatable variable stays as such -- we currently do
  // not provide a function to remove it from the "updatables" map:
  //
  void StratEnv::DeclareUpdatable(string const& name, double* ptr)
  {
    if (utxx::unlikely(name.empty() || ptr == NULL))
      throw invalid_argument
            ("StratEnv::DeclareUpdatable: Empty Key and/or NULL Value");

    // Update the "m_updatables" map:
    m_updatables[name] = ptr;
  }

  //-------------------------------------------------------------------------//
  // "ApplyUpdates":                                                         //
  //-------------------------------------------------------------------------//
  void StratEnv::ApplyUpdates() const
  {
    for (map<string, double>::iterator cit = m_nextUpdate.begin();
         cit != m_nextUpdate.end();  ++cit)
    {
      // Get the actual address of the variable to be updated:
      map<std::string, double*>::const_iterator cpit =
        m_updatables.find(cit->first);

      assert(cpit != m_updatables.end());  // Verified when the update arrived
      double* ptr  = cpit->second;
      assert(ptr  != NULL);                // Verified when declared updatable

      // Do it!
      *ptr = cit->second;
    }

    // Now re-set the updates, so that they would not be applied automatically
    // again if the corresp variable is changed on the user side:
    m_nextUpdate.clear();
  }

  //=========================================================================//
  // "OutputStatusLine":                                                     //
  //=========================================================================//
  void StratEnv::OutputStatusLine(char const* line)
  {
    assert(m_statusLine != NULL && line != NULL);
    // Copy the "line" to ShM and put 0-terminator at the end for safety:
    strncpy((char*)(m_statusLine->data()), line, m_StatusLineSz);
    (*m_statusLine)[m_StatusLineSz - 1] =  '\0';
  }


  template class CircBuffSimple<SecDef>;
  template class BooksGenShM   <SecDef>;

  //=========================================================================//
  // "LoadSecDefs":                                                          //
  //=========================================================================//
  void StratEnv::LoadSecDefs(SecDefSrcT a_src, std::string const& a_name)
  {
    // If a_src is the DB name, and a_name is not in proper URI form as defined
    // in Config_DB::URI(), format it properly:
    string name = (a_src == SecDefSrcT::DB || a_src == SecDefSrcT::MD) &&
                  (a_name.find("://") == string::npos)
                ? utxx::to_string("mysql://", Config_MySQL_StratEnv.m_user,
                                  ':', Config_MySQL_StratEnv.m_passwd,
                                  "@/", a_name)
                : a_name;

    vector<SecDef> defs;

    // Populate temp_storage with SecDef from a file, if it's found on disk,
    // or else try to load it from the database. We are using temporary vector
    // so that if there's a problem with parsing/loading some records, partial
    // results are ignored:

    size_t   actual, total;
    std::tie(actual, total) = SecDef::Load
      (std::vector<SecDefSrcPair>{std::make_pair(a_src, name)},
       defs, m_debugLevel);

    // Update the SecDef maps:
    for (auto& def : defs)
      m_secdef_cache.emplace(def.m_Symbol, def);

    // TODO: review cases when either map is empty.
    // Verify what we got. XXX: In some cases, SecIDsMap may still be empty
    // (if SecIDs are assigned dynamically), so:
    //
    if (m_secdef_cache.empty())
      // Could not get any Symbols. This may STILL be OK if eg this Connector
      // is a combined OE/MD (eg FIX and P2CGate can do this);    but still
      // produce a warning:
      SLOG(WARNING) << "StratEnv_Subscr::LoadSecDefs: Could not load "
                       "SecDefs (actual=" << actual << ", total=" << total
                    << ')' << endl;
    else
      SLOG(INFO)    << "XConnector::LoadSecDefs: Loaded "
                    << m_secdef_cache.size() << " SecDef symbols" << endl;
  }

  //=========================================================================//
  // "GetSecDef":                                                            //
  //=========================================================================//
  SecDef const* StratEnv::GetSecDef(Events::SymKey const& symbol) const
  {
    auto    it  = m_secdef_cache.find(symbol);
    return (it == m_secdef_cache.end()) ? nullptr : &it->second;
  }

}
