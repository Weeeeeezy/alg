// vim:ts=2:et:sw=2
//===========================================================================//
//                       "XConnector_OUCH_Currenex.cpp":                     //
//                  Currenex ECN OUCH Order Management Protocol              //
//===========================================================================//
#include "Connectors/AOS.hpp"
#include "Connectors/Configs_DB.h"
#include "StrategyAdaptor/EventTypes.h"
#include "StrategyAdaptor/EventTypes.hpp"
#include "Connectors/Configs_Currenex.h"
#include "Connectors/XConnector_OUCH_Currenex.h"
#include "Connectors/XConnector_OrderMgmt.hpp"
#include "Connectors/XConnector_OrderMgmt.schema.generated.hpp"
#include "Connectors/XConnector_OUCH_Currenex.schema.generated.hpp"
#include "Infrastructure/Log_File.schema.generated.hpp"
#include "Infrastructure/Log_File_Msg.schema.generated.hpp"
#include "Infrastructure/Marshal.h"
#include <utxx/string.hpp>
#include <utxx/endian.hpp>
#include <utxx/time_val.hpp>
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <type_traits>
#include <sys/eventfd.h>

//===========================================================================//
// Utils:                                                                    //
//===========================================================================//
namespace
{
  // Static empty ExchOrdID:
  MAQUETTE::ExchOrdID  EmptyExchOrdID;
}

namespace CNX
{
  using namespace Arbalete;
  using namespace std;
  using namespace MAQUETTE;

  using eixx::eterm;
  using utxx::out;
  using utxx::inout;

  using MsgBldr = XConnector_OUCH_Currenex::MsgBldr;
  using Channel = XConnector_OUCH_Currenex::Channel;

  //=========================================================================//
  // XConnector Factory Registration:                                        //
  //=========================================================================//
  // This instance adds XConnector_OUCH_Currenex implementation to the
  // XConnector registrar:
  static XConnectorRegistrar::Factory<XConnector_OUCH_Currenex>
    s_xconnector_registrar
    (
      CFG::OUCH_Currenex::alias(),
      &XConnector_OUCH_Currenex::Create,
      CFG::OUCH_Currenex::instance()
    );

  //=========================================================================//
  // "XConnector_OUCH_Currenex" Factory:                                     //
  //=========================================================================//
  XConnector* XConnector_OUCH_Currenex::Create
  (
    std::string const&          a_conn_name,
    utxx::config_tree const&    a_cfg
  )
  {
    return static_cast<XConnector*>
      (new XConnector_OUCH_Currenex(a_conn_name, a_cfg));
  }

  //=========================================================================//
  // "XConnector_OUCH_Currenex":                                             //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Ctor":                                                                 //
  //-------------------------------------------------------------------------//
  XConnector_OUCH_Currenex::XConnector_OUCH_Currenex
  (
    const string& a_conn_name,
    const utxx::config_tree& a_cfg
  )
  : //-----------------------------------------------------------------------//
    // Virtual base of XConnector -- need explicit Ctor invocation:          //
    //-----------------------------------------------------------------------//
    Named_Base(a_conn_name, utxx::type_to_string<XConnector_OUCH_Currenex>())
  , //-----------------------------------------------------------------------//
    // This class has a virtual root -- need explicit Ctor:                  //
    //-----------------------------------------------------------------------//
    XConnector
    (
      utxx::type_to_string<XConnector_OUCH_Currenex>(),
      a_conn_name,
      nullptr,
      a_cfg
    )
    //-----------------------------------------------------------------------//
    // Alias<XConnector_OUCH_Currenex> Ctor:                                 //
    //-----------------------------------------------------------------------//
  , Alias<XConnector_OUCH_Currenex>(a_conn_name)
    //-----------------------------------------------------------------------//
    // Immediate base class Ctor:                                            //
    //-----------------------------------------------------------------------//
  , XConnector_OrderMgmt
    (
      a_conn_name,
      nullptr,
      a_cfg
    )
  , m_configuration   (a_cfg)
  , m_stop            (false)
  , m_msg_bldr        (nullptr)
  , m_raw_msg_log_file(LogFileFactory::Instance().Create
                        (a_cfg.get<std::string>
                          (CFG::OUCH_Currenex::RAW_MSG_LOGGER_TYPE()),
                         true, a_conn_name))
  , m_unhandled_msgs  (0)
  {
    //-----------------------------------------------------------------------//
    // Open / Construct the "ReqQ":                                          //
    //-----------------------------------------------------------------------//
    // An existing Queue  is destroyed if "clean_start" is requested:
    //
    string req_q_name = GetOrderMgmtReqQName(m_ConnName, m_env_type);
    if (this->m_CleanStart)
      (void)fixed_message_queue::remove(req_q_name.c_str());

    // If a new ReqQ is constructed, it should not be too long, as the Sender-
    // Thrd must be able to cope with incoming "Req"s to be sent.   The Queue
    // consists of "OrdReq" objects.
    // XXX: Still, the original size of 1024 appears to be too short; use 16k:
    try
    {
      m_req_queue.reset(new fixed_message_queue
        (
          BIPC::open_or_create,
          req_q_name.c_str(),
          16384,
          sizeof(OrdReq),
          BIPC::permissions(0660),
          GetOrderMgmtReqQMapAddr(m_ConnName, m_env_type)
        ));
    }
    catch (std::exception const& e)
    {
      throw utxx::runtime_error
            (ClassName(), "::Ctor: Cannot open/create ReqQ: ", req_q_name,
             ": ", e.what());
    }

    // Start the Threads:
    // The C3 Thread comes first, so it can immediately respond to various ext-
    // ernal requests (XXX: so this is not very important anymore...):
    /* TODO
    m_c3_thread.reset
      (new Arbalete::Thread([this]()->void { this->C3Loop(); }));
    */

    // And the thread which executes queued Start / Stop requests. XXX: once
    // again, this sequence may not be the best nowadays:
    m_start_stop_thread.reset
      (new Arbalete::Thread([this]()->void { this->StartStopLoop(); }));
  }

  //-------------------------------------------------------------------------//
  // "Dtor":                                                                 //
  //-------------------------------------------------------------------------//
  XConnector_OUCH_Currenex::~XConnector_OUCH_Currenex()
  {
    MAQUETTE::TerminateThread(m_start_stop_thread);
    //MAQUETTE::TerminateThread(m_c3_thread);

    // Stop the Receiver Thread (this also closes the FDs if they are still
    // open, and destroys the Msg Builder):
    Stop();

    // Remove the ShMem Queue end-point, but preserve the Queue itself (XXX: is
    // it a good idea???):
    m_req_queue.reset();

    // Log a msg (XXX: No difference in ConnEventT between "Stop" and "Quit"?):
    LOG_INFO("%s::Dtor: CONNECTOR TERMINATED\n", ClassName().c_str());
  }

  //=========================================================================//
  // "DiscardReqQ":                                                          //
  //=========================================================================//
  // NB: Any errors here are fatal -- we need to re-start the Connector and re-
  // build the Queue to fix them:
  //
  void XConnector_OUCH_Currenex::DiscardReqQ()
  {
    XConnector_OrderMgmt::OrdReq discard;
    unsigned long exp_sz = sizeof(discard);
    unsigned long act_sz = 0;
    unsigned int  prio   = 0;
    try
    {
      while (m_req_queue->try_receive(&discard, exp_sz, out(act_sz), out(prio)));
    }
    catch (std::exception const& e)
    {
      LOG_FATAL("%s::DiscardReqQ: %s\n", ClassName().c_str(), e.what());
      throw; // FIXME: Handle exit more gracefully!
    }
  }

  //=========================================================================//
  // Construct the MsgBuilder:                                               //
  //=========================================================================//
  void XConnector_OUCH_Currenex::MkMsgBldr()
  {
    try
    {
      m_msg_bldr_ptr.reset
        (MsgBldr::Create(*m_reactor, this, m_configuration, false));
      m_msg_bldr = m_msg_bldr_ptr.get();
    }
    catch (std::exception const& e)
    {
      // XXX: Any error is deemed to be fatal, in particular because this meth-
      // od may be called from a "catch" section of "RecvrThreadBody":
      LOG_ERROR("%s::MkMsgBldr: %s TERMINATING\n", ClassName().c_str(), e.what());
      throw;
    }
  }

  //=========================================================================//
  // "Start":                                                                //
  //=========================================================================//
  // Invoked from the Management Thread (either directly or from the Ctor):
  //
  void XConnector_OUCH_Currenex::DoStart()
  {
    std::lock_guard<std::mutex> lock(m_starter_lock);

    if (this->GetStatus() == Status::Active)
      throw utxx::runtime_error(ClassName(), "::DoStart: already started!");

    m_stop = false;         // Clear the "Stop" flag (if it was set)

    // Open the raw message logger:
    m_raw_msg_log_file->Open();

    // Construct the Msg Builder:
    MkMsgBldr();
  }

  //=========================================================================//
  // "Stop":                                                                 //
  //=========================================================================//
  // This is much more involved than Start() -- must provide graceful shutdown.
  // Invoked from the Management Thread only, so no need for locking:
  //
  void XConnector_OUCH_Currenex::DoStop()
  {
    std::lock_guard<std::mutex> lock(m_starter_lock);

    // Post a flag on our intention to stop -- it has no effect until the Thrds
    // actually start terminating,  but it prevents them from re-starting auto-
    // matically:
    m_stop = true;

    // Submit a LogOut. NB: This cannot be done synchronously; the request must
    // be queued via the SenderThread, as otherwise there may be collsions with
    // whatever it is currently sending. (But if, for whatever reason, the Sen-
    // der Thread is not running, the msg will be sent synchronously):
    //SendAdminMsg(MsgT::LogOut);

    // Notify the Clients, so they stop submitting Reqs from now on. XXX: what
    // if some Client continues to do so? Apparently nothing wrong on the Con-
    // nector side, but the Client's orders will be lost:
    //
    this->NotifyAllClients(Events::EventT::ConnectorStop);

    // Wait for the Clients to stop submissions:
    sleep(1);

    // Clear out the ReqQ from what is already there:
    DiscardReqQ();
    m_req_queue.reset();

    // Remove the Msg Builder:
    m_msg_bldr_ptr.reset();
    m_msg_bldr = nullptr;
  }

  //=========================================================================//
  // "DoWaitForCompletion":                                                  //
  //=========================================================================//
  // Waiting for ALL threads to terminate, which normally happens on receiving
  // of a "quit" command by the "C3Thread", or a corresponding signal:
  //
  void XConnector_OUCH_Currenex::DoWaitForCompletion()
  {
    std::lock_guard<std::mutex> lock(m_starter_lock);

    MAQUETTE::JoinThread(m_c3_thread);
  }

  //=========================================================================//
  // "Subscribe":                                                            //
  //=========================================================================//
  // The Client subscribes to both OMC and (if available) MDC services.
  // NB: This method can only return "true" in the MDC Mode:
  //
  bool XConnector_OUCH_Currenex::Subscribe
  (
    CliInfo     const& a_client,
    std::string const& a_symbol,
    OrderID            a_req_id
  )
  {
    // Initially, "req_id" is always 0:
    assert(a_req_id == 0);

    // OE Subscription -- "symbol" will be ignored:
    return XConnector_OrderMgmt::Subscribe(a_client, a_symbol, a_req_id);
  }

  //=========================================================================//
  // "Unsubscribe":                                                          //
  //=========================================================================//
  bool XConnector_OUCH_Currenex::
  Unsubscribe(CliInfo& a_client, string const& a_symbol)
  {
    // "symbol" is actually ignored, so we effectively do "UnsubscribeAll":
    this->UnsubscribeAll(a_client, nullptr);
    return true;
  }

  //=========================================================================//
  // "UnsubscribeAll":                                                       //
  //=========================================================================//
  void XConnector_OUCH_Currenex::UnsubscribeAll
  (
    CliInfo&                 a_client,
    std::set<Events::SecID>* a_actually_unsubscribed
  )
  {
    XConnector_OrderMgmt::UnsubscribeAll(a_client, a_actually_unsubscribed);
  }

  //=========================================================================//
  // "OnClientRequest":                                                      //
  //=========================================================================//
  void XConnector_OUCH_Currenex::OnClientMsg()
  {
    static const std::string s_function_name =
      utxx::to_string(ClassName(), "::OnClientRequest");

    //-----------------------------------------------------------------------//
    // Process all pending requests:                                         //
    //-----------------------------------------------------------------------//
    // TODO: we may need to balance this with processing incoming requests
    // in order to prevent thread's resource starvation:
    //
    while (m_msg_bldr->m_channel->Connected())
    {
      //---------------------------------------------------------------------//
      // Get the next request:                                               //
      //---------------------------------------------------------------------//
      // NB: This is a non-blocking receive: it succeeds iff the request queue
      // is currently non-empty:
      //
      unsigned long  exp_sz = sizeof(OrdReq);
      unsigned long  act_sz = 0;
      unsigned int   prio   = 0;
      utxx::time_val send_time;  // Initially {0, 0}
      OrdReq         req;

      bool got = m_req_queue->try_receive(&req, exp_sz, act_sz, prio);
      if (!got)
      {
        if (utxx::unlikely(m_DebugLevel >= 5))
          LOG_INFO("%s::OnClientRequest: request queue empty\n",
                   ClassName().c_str());
        // There's nothing else to do:
        return;
      }

      // OK, we got some "OrdReq". Check the sizes:
      if (utxx::unlikely(act_sz != exp_sz))
      {
        LOG_ERROR("OnClientRequest: Invalid OrdReq Size: got=%lu, expected=%lu\n",
                  act_sz, exp_sz);
        continue;
      }

      auto now = utxx::now_utc();

      // Non-empty error indicates that request wasn't processed successfully:
      std::string  error;  // Initially empty

      //-------------------------------------------------------------------//
      // Send an Admin Msg:                                                //
      //-------------------------------------------------------------------//
      // Any exceptions here imply internal logic errors, so they WILL result
      // in a Sender Thread termination:
      //
      if (utxx::unlikely(req.m_isAdmin))
      {
        AdminReq const* admin = req.ToAdmin();

        if (admin != NULL)
          SendAdminMsg(*admin, out(send_time), out(error));
        else
          error = "AdminReqPtr is NULL";

        if (error.empty())
          // Successful send: Add a sample to the throttler:
          m_msg_rate_throttler.add(now);
        else
        {
          // Send error: Refresh the throttler anyway:
          RefreshMsgRateThrottler(now);

          if (m_DebugLevel >= 1)
            LOG_ERROR("OnClientRequest: Couldn't send an admin msg");
          // NB: Do NOT explicitly close the connection here -- "SendAdminMsg"
          // has done it already if it was necessary. Do not return either --
          // we may very well be able to send the following-on msgs...
        }
        continue;
      }

      //---------------------------------------------------------------------//
      // Send an Application-Level Msg from an "AOSReq12":                   //
      //---------------------------------------------------------------------//
      // Get the Application-Level Request:
      AOSReq12*     msg = req.ToAppl();
      auto latency_usec = now.diff_usec(msg->m_ts_created);
      auto   qtime_usec = now.diff_usec(msg->m_ts_qued);
      AOS*          aos = (msg != nullptr) ? msg->m_aos : nullptr;

      if (utxx::unlikely(aos == nullptr))
      {
        // Cannot do anything more -- Log the error and try other msgs:
        if (m_DebugLevel >= 1)
          LOG_ERROR("OnClientRequest: ReqPtr=%p, AOSPtr=%p\n", msg, aos);
        continue;
      }
      assert(msg != nullptr && aos != nullptr);

      // Check right at this point that the AOS is still active:
      if (utxx::unlikely(aos->Inactive()))
        error = "AOS is inactive";
      else
      // Now check that the msg has not run out of time yet (this did not apply
      // to Admin msgs, and does not apply to Cancels and MassCancels; however,
      // it applies to NewOrders and Modifications):
      //
      if (utxx::unlikely(latency_usec >= this->m_max_msg_sending_delay
                     && (msg->Kind() == AOSReq12::ReqKindT::New    ||
                         msg->Kind() == AOSReq12::ReqKindT::Replace)))
      {
        ++m_too_late_msgs_count;
        error = utxx::to_string
          ("Msg Expired (DelayFromCreate=", latency_usec,
           "us, TransitDelay=",             qtime_usec,
           "us, TotalLateMsgs=",            m_too_late_msgs_count, ')');
      }
      else
      if (utxx::unlikely(this->GetStatus() == Status::Suspended))
        error = "Connector suspended";
      else
      if (utxx::unlikely(m_msg_rate_throttler.running_sum() >
                         m_msgs_per_sec_limit))
      {
        error = utxx::to_string
          ("Connector too busy (", m_msg_rate_throttler.running_sum(),
           " msgs/s)");
        ++this->m_throttler_activation_count;
      }
      else
        //-------------------------------------------------------------------//
        // "SendReqMsg" performs actual request sending:                     //
        //-------------------------------------------------------------------//
        // No exceptions are thrown by it -- all errors are saved in "error":
        //
        SendReqMsg(*msg, out(error));

      //---------------------------------------------------------------------//
      // Success?                                                            //
      //---------------------------------------------------------------------//
      if (error.empty())
      {
        // Log the latency (either Admin or Application msg has been sent);
        // (TODO: stats accumulator):
        if (utxx::unlikely(m_WithTiming && m_DebugLevel >= 2))
        {
          auto send_time         = utxx::now_utc();
          auto from_create_usec  = send_time.diff_usec(msg->m_ts_created);
          auto from_queuing_usec = send_time.diff_usec(msg->m_ts_qued);

          LOG_INFO("OnClientRequest: Latency "
                   "Transit=%ldus, Crate->Sock=%ldus, Que->Sock=%ldus\n",
                   qtime_usec, from_create_usec, from_queuing_usec);
        }
        // Add a sample to the throttler:
        IncrementMsgRateThrottler(now);
      }
      else
      //---------------------------------------------------------------------//
      // Failed (or the args were incorrect):                                //
      //---------------------------------------------------------------------//
      {
        assert(!error.empty());
        // NB: Do NOT explicitly close the connection here: if necessary, it
        // was already done by "SendReqMsg":
        // In any case, refresh the Msg Throttler:
        RefreshMsgRateThrottler(now);

        // Has the order become Inactive as a result? XXX: At the moment, we
        // consider the order to be Inactive iff the failed Req was "New":
        if (utxx::unlikely(msg->m_kind == AOSReq12::ReqKindT::New))
          aos->Status(AOS::StatusT::Inactive);

        // Log the error:
        if (m_DebugLevel >= 1)
          LOG_ERROR("OnClientMsg: Couldn't send an app msg to client: "
                    "RootID=%lX, ReqID=%ld: %s; Active=%s\n",
                    aos->RootID(), msg->m_id, error.c_str(),
                    aos->Active() ? "true" : "false");

        // Notify the Client (if known). Any further errors which may occur
        // here, are caught and logged:
        try
        {
          NotifyClientOnReject(msg, 0, error.c_str(), "OnClientRequest");
        }
        catch (exception const& exc)
        {
          if (m_DebugLevel >= 1)
            LOG_ERROR("OnClientRequest: Couldn't send the "
                      "prev error msg to the client: %s\n", exc.what());
        }
      }
    }
    // End of while-loop wrt available incoming msgs
  }

  //=========================================================================//
  // "DoBeforeRun":                                                          //
  //=========================================================================//
  bool XConnector_OUCH_Currenex::DoBeforeRun()
  {
    // Seqno always starts with 1 upon restart
    this->NextTxSeqno(1);

    LOG_INFO("Connector started %s (TxSeqno=%ld, RxSeqno=%ld, NextOrdno=%ld\n",
             (m_CleanStart ? " (clean)" : ""),
             this->NextTxSeqno(), this->NextRxSeqno(), this->NextOrderNo());

    return true /* handled */;
  }

  //=========================================================================//
  // "DoRun":                                                                //
  //=========================================================================//
  void XConnector_OUCH_Currenex::DoRestartRun()
  {
    //-----------------------------------------------------------------------//
    // Error Handling:                                                       //
    //-----------------------------------------------------------------------//
    // This is an error condition, but because there is not external mo-
    // nitor which would re-start the Receiver, we have to recover from
    // it here by ourselves:
    usleep(100000); // Sleep for 100ms to prevent the flood of restart attempts
    // Re-create the Msg Builder:
    MkMsgBldr();
  }

  //=========================================================================//
  // "OrderCancelReq":                                                       //
  //=========================================================================//
  // Provided for compatibility with the template parameter in
  // XConnector_OrderMgmt::EmulateMassCancel():
  //
  void XConnector_OUCH_Currenex::OrderCancelReq
  (
    AOSReq12* a_cancel_req,
    string&   a_error
  )
  {
    // Find the original Req to be modified. XXX: "CxlReplModeT" is inherited
    // from FIX -- use the latest intended Req as the basis:
    //
    assert(a_cancel_req != NULL);
    AOS* aos = a_cancel_req->m_aos;
    assert(aos != NULL);

    AOSReq12 const* orig_req = SelectOrigReq(aos);

    if (utxx::unlikely(aos->Inactive() || orig_req == nullptr))
    {
      a_error = "CancelOrder: AOS is inactive or being cancelled already";
      return;
    }

    // Increment the msg counter, but only AFTER "SelectOrigReq":
    a_cancel_req->m_id = this->IncOrderNo();

    // Send the order up -- use the ORIGINAL "qty":
    MsgBldr::ResCodeT res =
      m_msg_bldr->CancelOrder
        (aos->RootID(), a_cancel_req->m_id, orig_req->m_id, aos->m_symbol);

    switch (res)
    {
      case MsgBldr::ResCodeT::InvalidParams:
        a_error = "CancelOrder: Invalid Param(s)";
        return;

      case MsgBldr::ResCodeT::IOError:
        // Disconnect now (if not disconnected already):
        m_msg_bldr->m_channel->Close();
        a_error = "CancelOrder: I/O Error";
        return;

      default: ;  // OK
    }
  }

  //=========================================================================//
  // "SendReqMsg":                                                           //
  //=========================================================================//
  // This is a demultiplexer acting on "AOSReq12"; actual sending is done by
  // the corresp functions in "OUCH_Currenex_MsgBuilder":
  //
  void XConnector_OUCH_Currenex::SendReqMsg
  (
    AOSReq12& a_msg,         // In-out param
    string  & a_error        // Out param
  )
  {
    AOS* aos = a_msg.GetAOS();
    //-----------------------------------------------------------------------//
    // Check the Req:                                                        //
    //-----------------------------------------------------------------------//
    // "error" must initially be empty:
    assert(aos && a_error.empty());

    // Multi-legged orders are currently not supported:
    if (utxx::unlikely(a_msg.IsMultiLegged()))
    {
      a_error = "Multi-Legged orders not supportred";
      return;
    }
    else
    if (utxx::unlikely(a_msg.GetAOS()->Side() == MAQUETTE::SideT::Undefined))
    {
      a_error = "Undefined side";
      return;
    }

    //-----------------------------------------------------------------------//
    // Order Side, Type (aka "PriceCond") and TTL (aka "OrdCond"):           //
    //-----------------------------------------------------------------------//
    // They are required in all cases except Cancellations. XXX: "side", "ord_
    // type" ("price_cond") and "ttl" ("ord_cond"): need to be translated from
    // their FIX-encoded reps in the AOS, into the resp OUCH reps:
    //
    assert(m_msg_bldr != nullptr);

    auto       req_kind       = a_msg.Kind();
    SideT      ord_side       = aos->IsBuy() ? SideT::BUY : SideT::SELL;
    OrderType  ord_type       = OrderType::LIMIT;
    TTL        ord_ttl        = TTL::UNDEFINED;

    if (req_kind != AOSReq12::ReqKindT::Cancel &&
        req_kind != AOSReq12::ReqKindT::MassCancel)
    {
      // Order Type -- not applicable to "Cancel" or "MassCancel":
      switch (aos->m_orderType)
      {
        // TODO: Add iceberg order type classification!
        case FIX::OrderTypeT::Limit:
          ord_type = OrderType::LIMIT;
          break;
        default:
        {
          // XXX: Other FIX order types are not implemented; OUCH types "Condi-
          // tional" and "Best" are not supported yet:
          a_error = utxx::to_string("UnSupported AOS OrderType: ",
                                    char(aos->m_orderType), " (",
                                    int (aos->m_orderType), ')');
          return;
        }
      }

      // Order TTL:
      switch (aos->m_timeInForce)
      {
        case FIX::TimeInForceT::Day:
          ord_ttl = TTL::GTC;   // "Normal" TTL (Day)
          break;
        case FIX::TimeInForceT::ImmedOrCancel:
          ord_ttl = TTL::IOC;
          break;
        default:
        {
          a_error = "UnSupported AOS OrderTTL: " +
                    string(1, char(aos->m_timeInForce));
          return;
        }
      }
    }

    // Client and once again by the actual sending function...
    MsgBldr::ResCodeT res;

    switch (req_kind)
    {
      //---------------------------------------------------------------------//
      case AOSReq12::ReqKindT::Replace:
      //---------------------------------------------------------------------//
      {
        // Order Modification (Amendment) Request
        // Find the original Req to be modified. XXX: "CxlReplModeT" is inherited
        // from FIX -- use the latest intended Req as the basis:
        //
        AOSReq12 const* orig_req = this->SelectOrigReq(aos);

        if (utxx::unlikely(aos->Inactive() || orig_req == NULL))
        {
          a_error = "SendReqMsg(Modify): AOS is inactive or being cancelled";
          return;
        }

        // Install proper values for modified Px and/or Qty, if the curr values
        // are "unchanged" indicators:
        if (a_msg.m_qty <  0)
          a_msg.m_qty = orig_req->m_qty;

        if (!Finite(a_msg.m_px))
          a_msg.m_px  = orig_req->m_px;

        // Verify the params once again:
        if (a_msg.m_qty <= 0 || a_msg.m_qty > MAX_ORDER_QTY ||
          !Finite(a_msg.m_px))
        {
          a_error = utxx::to_string
            ("ModifyOrder: Invalid Qty=", a_msg.m_qty, " and/or Px=", a_msg.m_px);
          return;
        }

        // Convert the price into an int:
        double pxStep = aos->m_secDef->m_MinPriceIncrement;
        assert(pxStep > 0.0);

        // XXX: The following value could be pre-computed and stored in SecDef,
        // but the problem is that it is highly protocol-specific:
        int    cmpl   = int(Round(100000.0   * pxStep));
        int    ipx    = int(Round(a_msg.m_px / pxStep)) * cmpl;

        // XXX: Is "ModifyOrder" allowed to alter the Side (eg BUY->SELL), Price
        // Cond (eg LIMIT->MARKET) or TTL (eg FAS->FOK)? -- Probably NOT; this is
        // not supported by our StratEnv API anyway (though some changes such  as
        // LIMIT->MARKET could be quite useful):
        //
        // Send the order up:
        res = m_msg_bldr->ModifyOrder
              (aos->RootID(), a_msg.ID(), orig_req->m_id, aos->m_symbol, ipx,
               a_msg.m_qty);
        break;
      }
      //-----------------------------------------------------------------------//
      case AOSReq12::ReqKindT::New:
      //-----------------------------------------------------------------------//
      {
        // New Order:
        // Verify the args:
        // NB: Px and Qty verification is normally performed by the Client, but
        // we do it once again here, taking into account the OUCH-specific limits:
        //
        if (utxx::unlikely
          ((ord_type == OrderType::LIMIT) != Finite(a_msg.m_px) ||
            a_msg.m_qty <= 0 || a_msg.m_qty > MAX_ORDER_QTY))
        {
          a_error = utxx::to_string
                    ("NewOrder: Invalid Px=", a_msg.m_px, " and/or Qty=",
                    a_msg.m_qty);
          return;
        }

        // If there is no SecDef ptr in the AOS, install it:
        if (aos->m_secDef == nullptr)
        {
          SymbolsMap::const_iterator cit = m_SymbolsMap.find(aos->m_symbol);
          if (utxx::unlikely(cit == m_SymbolsMap.end()))
          {
            a_error = utxx::to_string
                      ("NewOrder: Cannot get SecDef for ", aos->m_symbol.data());
            return;
          }
          aos->m_secDef = &(cit->second);
        }

        // Convert the price into an int:
        double pxStep = aos->m_secDef->m_MinPriceIncrement;
        assert(pxStep > 0.0);

        // XXX: The following value could be pre-computed and stored in SecDef,
        // but the problem is that it is highly protocol-specific:
        int    cmpl   = int(Round(100000.0   * pxStep));
        int    ipx    = int(Round(a_msg.m_px / pxStep)) * cmpl;

        // Send the order up:
        res = m_msg_bldr->NewOrder
        (
          aos->RootID(),
          a_msg.ID(), aos->m_symbol, ord_side,         ord_type,     ord_ttl,
          ipx,        a_msg.m_qty,   a_msg.m_qty_show, a_msg.m_qty_min
        );

        break;
      }
      //---------------------------------------------------------------------//
      case AOSReq12::ReqKindT::Cancel:
      //---------------------------------------------------------------------//
      {
        // Order Cancellation:
        OrderCancelReq(&a_msg, out(a_error));
        if (utxx::unlikely(!a_error.empty()))
          return;
        break;
      }
      //---------------------------------------------------------------------//
      case AOSReq12::ReqKindT::MassCancel:
      //---------------------------------------------------------------------//
      {
        auto CancelFunc =
          // NB: "socket" is unused; "send_time" is currently unused:
          [this, &a_error]
          (AOSReq12* clx_req, int socket, utxx::time_val& send_time)->bool
          {
            // Similar to above:
            this->OrderCancelReq(clx_req,   out(a_error));

            // If there was an error in the above call, log it right here
            // because it could be lost / overwritten otherwise:
            bool ok = a_error.empty();
            if (utxx::unlikely(!ok))
              LOG_ERROR("%s::OrderCancelReq FAILED: RootID=%012lX, ReqID=%ld: "
                        "%s\n", ClassName().c_str(),
                        clx_req->GetAOS()->RootID(), clx_req->ID(),
                        a_error.c_str());
            return ok;
          };

        utxx::time_val send_time;
        // If any errors wwere encountered, they are stored in "a_error":
        EmulateMassCancel(CancelFunc, &a_msg, -1, out(send_time));

        if (utxx::unlikely(!a_error.empty()))
          return;

        res = MsgBldr::ResCodeT::Ignore;

        break;
      }
      //---------------------------------------------------------------------//
      default:
      //---------------------------------------------------------------------//
      {
        a_error = utxx::to_string
          ("UnSupported ReqType: RootID=", ToHexStr16(aos->RootID()),
          ", Symbol=", Events::ToString(aos->m_symbol),
          ", Price=",  a_msg.m_px, ", Qty=", a_msg.m_qty,
          ", OrderType=", ToString(aos->m_orderType));
        return;
      }
    }
    //-----------------------------------------------------------------------//
    // Success:                                                              //
    //-----------------------------------------------------------------------//
    // If we got here: No error:
    assert(a_error.empty());

    switch (res)
    {
      case MsgBldr::ResCodeT::InvalidParams:
        a_error = "Invalid Param(s)";
        return;

      case MsgBldr::ResCodeT::IOError:
        a_error = "I/O Error";
        return;

      case MsgBldr::ResCodeT::Ignore:
        return;

      default:
        // Attach the request to AOS:
        aos->Link(this->ReqStorage(), &a_msg);
    }
  }

  //=========================================================================//
  // "TotalBytes{Up,Down}":                                                  //
  //=========================================================================//
  // XXX: This statistics is not exact -- the byte counts are reset when the
  // Connector is "Stop"ped:
  //
  long XConnector_OUCH_Currenex::TotalBytesUp()  const
    { return utxx::likely(m_msg_bldr) ? m_msg_bldr->TxBytes() : 0u; }

  long XConnector_OUCH_Currenex::TotalBytesDown() const
    { return utxx::likely(m_msg_bldr) ? m_msg_bldr->RxBytes() : 0u; }

  //=========================================================================//
  // Processing Responses from the Exchange-Side OUCH Server:                //
  //=========================================================================//
  //=========================================================================//
  // "LogonAck":                                                             //
  //=========================================================================//
  void XConnector_OUCH_Currenex::OnData(Msgs::LogonAck const& a_msg)
  {
    if (utxx::unlikely(m_DebugLevel >= 1))
      LOG_INFO("[%s] <- LOGON ACK (seqno=%d, session=%d)\n",
               m_msg_bldr->m_channel->Name().c_str(),
               a_msg.seqno(), a_msg.session_id());

    // Send event to eventfd in order to begin processing client req queue
    this->WakeupSignal();
  }

  //=========================================================================//
  // "Logout":                                                               //
  //=========================================================================//
  void XConnector_OUCH_Currenex::OnData(Msgs::Logout const& a_msg)
  {
    if (utxx::unlikely(m_DebugLevel >= 1))
      LOG_INFO("[%s] <- LOGOUT (seqno=%d, session=%d)\n",
               m_msg_bldr->m_channel->Name().c_str(),
               a_msg.seqno(), a_msg.session_id());
    // TODO: Notify clients?
  }

  //=========================================================================//
  // "OrderAck":                                                             //
  //=========================================================================//
  // This is a New Order Acknowledgement. Includes both Confirmations and Rej-
  // ects. Closely corresponds to "ExecReport" in FIX, but in OUCH, there are
  // no intermediate "Acknowledged" states -- only "Confirmed" (or "Cancelled"
  // if there was a Reject):
  //
  void XConnector_OUCH_Currenex::OnData(Msgs::OrderAck const& a_msg)
  {
    //-----------------------------------------------------------------------//
    // Update the Order State. NB: RootID is NOT received in "a_msg", use 0: //
    //-----------------------------------------------------------------------//
    int64_t         clord_id    = a_msg.clorder_id();
    int64_t         cnx_ord_id  = a_msg.order_id();
    utxx::time_val  ts          = m_msg_bldr->MsgTime(a_msg);
    Events::OrderID root_id     = 0;
    AOSReq12 const* req         = nullptr;

    if (utxx::likely(a_msg.status() == Msgs::OrderAck::status_t::CONFIRMED))
    {
      //---------------------------------------------------------------------//
      // Confirmation:                                                       //
      //---------------------------------------------------------------------//
      // Then Exchange-assigned OrderID must be valid:
      if (utxx::unlikely(cnx_ord_id < 0))
        throw utxx::badarg_error
              (ClassName(), "::OnData(OrderAck): Status=Confirmed "
               "but ExchOrdID=", cnx_ord_id);

      // TODO: update Confirm to return AOS;
      // "to_cancel" flag is "false":
      ExchOrdID xchg_id(cnx_ord_id);
      req = this->Acknowledge
        (clord_id, &xchg_id, ts, m_msg_bldr->m_channel->LastRxTime());
    }
    else
    {
      //---------------------------------------------------------------------//
      // Rejection:                                                          //
      //---------------------------------------------------------------------//
      // The corresp event type is "RequestError"; StateMgr will also produce
      // an "OrderCancelled" event sent to the Client:
      //
      assert(a_msg.status() == Msgs::OrderAck::status_t::REJECTED);
      if (utxx::unlikely(cnx_ord_id != -1))
        throw utxx::badarg_error
              (ClassName(), "::OnData(OrderAck): Status=Rejected "
               "but ExchOrdID=", cnx_ord_id);

      req = this->OrderRejected
      (
        clord_id,
        int(a_msg.error_code()),
        Msgs::OrderAck::decode_error_code(a_msg.error_code()),
        ts,
        m_msg_bldr->m_channel->LastRxTime()
      );
    }
    //-----------------------------------------------------------------------//
    // Logging:                                                              //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_DebugLevel >= 3))
    {
      auto info = req->GetAOS()->UnsafeClient();
      if (a_msg.status() == Msgs::OrderAck::status_t::CONFIRMED)
        SLOG(INFO) << '[' << m_msg_bldr->m_channel->Name()
                   << "] <- ACKd ord-id="    << clord_id
                   << " (cnx-id="            << cnx_ord_id
                   << ", root-id="           << root_id
                   << ", client="  << (info ? info->Name() : "")
                   << " ("         << (info ? info->CID()  : -1)
                   << ")\n";
      else
        SLOG(INFO) << '[' << m_msg_bldr->m_channel->Name()
                   << "] <- REJECTED ord-id="<< clord_id
                   << " (cnx-id="            << cnx_ord_id
                   << ", root-id="           << root_id
                   << ", client="  << (info ? info->Name() : "")
                   << " ("         << (info ? info->CID()  : -1)
                   << "): "
                   << Msgs::OrderAck::decode_error_code(a_msg.error_code())
                   << " (" << utxx::to_underlying(a_msg.error_code()) << ")\n";
    }
  }

  //=========================================================================//
  // "CxlRej":                                                               //
  //=========================================================================//
  // This is a response to a (failed) "OrderCancel" -- unlike in FIX, where si-
  // milar msgs are also sent in responce to failed "OrderCancelReplace" reqs:
  //
  void XConnector_OUCH_Currenex::OnData(Msgs::CxlRej const& a_msg)
  {
    //-----------------------------------------------------------------------//
    // Update the Order State (Cancel Reject):                               //
    //-----------------------------------------------------------------------//
    Events::OrderID root_id     = 0;
    Events::HashT   client_hash = 0;

    auto clord_id      = a_msg.new_clorder_id ();
    auto prev_clord_id = a_msg.prev_clorder_id();

    // Did this Cancel fail because the order was non-existent (maybe already
    // filled or cancelled)? -- Most likely:
    // FIXME: should we consider the following?
    //bool non_existent =
    //  (a_msg.error_code() == Msgs::CxlRej::error_code_t::INVALID_ORDER);

    // TODO: update state_mgr to return AOS
    this->CancelRejected
    (
      clord_id,
      prev_clord_id,
      int(a_msg.error_code()),
      Msgs::CxlRej::decode_error_code(a_msg.error_code()),
      m_msg_bldr->MsgTime(a_msg),
      m_msg_bldr->m_channel->LastRxTime()
    );

    //-----------------------------------------------------------------------//
    // Logging:                                                              //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_DebugLevel >= 3))
      SLOG(INFO) << '[' << m_msg_bldr->m_channel->Name()
                << "] <- CLX REJ ord-id=" << clord_id
                << ", prev-id="           << prev_clord_id
                << ", root-id="           << root_id
                << ", client="            << client_hash
                << ": " << Msgs::CxlRej::decode_error_code(a_msg.error_code())
                << " (" << utxx::to_underlying(a_msg.error_code()) << ")\n";
  }

  //=========================================================================//
  // "ReplAck":                                                              //
  //=========================================================================//
  // 3-way response to "OrderReplaceOrCancel";   the order can be successfully
  // Replaced, or only Canceled (without replacement), or the modification req
  // can Fail altogether:
  //
  void XConnector_OUCH_Currenex::OnData(Msgs::ReplAck const& a_msg)
  {
    //-----------------------------------------------------------------------//
    // Update the Order State:                                               //
    //-----------------------------------------------------------------------//
    Events::OrderID root_id       = 0;
    Events::HashT   client_hash   = 0;
    utxx::time_val  ts            = m_msg_bldr->MsgTime(a_msg);
    auto            clord_id      = a_msg.new_clorder_id();
    auto            prev_clord_id = a_msg.prev_clorder_id();

    switch (a_msg.status())
    {
    case Msgs::ReplAck::status_t::CANCEL:
      // The Replace request has been converted into Cancel request. Typically,
      // this happens if the orig order has been partially filled -- then,  in
      // OUCH, it cannot be replaced, so the remaining part is cancelled. This
      // msg does NOT mean that the orig order has already been cancelled (that
      // will be reported separately), only that the cancellation is now inten-
      // ded (to_cancel = true):
      //
      this->OrderCancelled
        (clord_id, EmptyExchOrdID, ts, m_msg_bldr->m_channel->LastRxTime());
      break;

    case Msgs::ReplAck::status_t::REPLACED:
      // The order has been successfully replaced -- invoke confirmation;
      // to_cancel=false:
      this->OrderReplaced
        (clord_id, false, EmptyExchOrdID,
         ts, m_msg_bldr->m_channel->LastRxTime());
      break;

    case Msgs::ReplAck::status_t::REJECTED:
    {
      //---------------------------------------------------------------------//
      // Rejection of the whole request:                                     //
      //---------------------------------------------------------------------//
      // Order modification has failed -- but presumably, the original order is
      // still alive (XXX???). Because of its FIX roots, we invoked "CancelRej-
      // etc" in this case, but indicate that the original order was NOT a Can-
      // cel:
      // Did this Cancel fail because the order was non-existent (maybe already
      // filled or cancelled)? -- Most likely:
      //
      // XXX: This is not a reliable test; there are some other error codes (eg
      // INVALID_ERROR) which could also imply a non-existing order, but we do
      // not know for sure. This flag is only a suggestion, anyway:
      //
      bool non_existent =
        (a_msg.error_code() == Msgs::ReplAck::error_code_t::ORDER_NOT_ACTIVE);

      this->CancelReplaceRejected
      (
        clord_id,
        prev_clord_id,
        int(a_msg.error_code()),
        Msgs::ReplAck::decode_error_code(a_msg.error_code()),
        non_existent, ts, m_msg_bldr->m_channel->LastRxTime()
      );
      break;
    }
    default:
      assert(false);
    }

    //-----------------------------------------------------------------------//
    // Logging:                                                              //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_DebugLevel >= 3))
    {
      auto ec = a_msg.error_code();

      SLOG(INFO) << '[' << m_msg_bldr->m_channel->Name()
                << "] <- REPL ACK ord-id=" << clord_id
                << ", prev-id="            << prev_clord_id
                << ", root-id="            << root_id
                << ", client="             << client_hash
                << (ec == Msgs::ReplAck::error_code_t::SUCCESS
                    ? std::string() : utxx::to_string
                      (": ", a_msg.decode_error_code(ec),
                        " (", utxx::to_underlying(ec), ") "))
                << std::endl;
    }
  }

  //=========================================================================//
  // "Cxld":                                                                 //
  //=========================================================================//
  // "OrderCancel" positive confirmation. The order could be Cancelled, or Exp-
  // ired, but we do not make distinction between the two cases:
  //
  void XConnector_OUCH_Currenex::OnData(Msgs::Cxld const& a_msg)
  {
    static const string  s_empty;
    Events::OrderID      root_id    = 0;
    auto                 clord_id   = a_msg.clorder_id();

    //-----------------------------------------------------------------------//
    // Update the Order State (Successful Cancellation):                     //
    //-----------------------------------------------------------------------//
    auto req = this->OrderCancelled
    (
      clord_id,
      EmptyExchOrdID,         // Not reported by OUCH, so empty
      m_msg_bldr->MsgTime(a_msg),
      m_msg_bldr->m_channel->LastRxTime()
    );
    //-----------------------------------------------------------------------//
    // Logging:                                                              //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_DebugLevel >= 3))
    {
      auto client = req ? req->GetAOS()->UnsafeClient() : nullptr;
      SLOG(INFO) << '[' << m_msg_bldr->m_channel->Name()
                << "] <- CLXd ord-id=" << clord_id
                << ", root-id="        << root_id
                << ", client="         << (client ? client->Name() : "")
                << ": " << a_msg.decode_status(a_msg.status())
                << " (" << a_msg.decode_type(a_msg.type()) << ")\n";
    }
  }

  //=========================================================================//
  // "Trade":                                                                //
  //=========================================================================//
  // Fill or PartialFill:
  //
  void XConnector_OUCH_Currenex::OnData(Msgs::Trade const& a_msg)
  {
    //-----------------------------------------------------------------------//
    // Update the Order State (Fill or Partial Fill):                        //
    //-----------------------------------------------------------------------//
    // XXX: We currently do not distinguish between different ExecTypes: New,
    // Suspended, Promoted and Demoted Trades. All except New ones are deemed
    // to be rare(???)
    // XXX: Symbol, ExecBroker, ExecID, SettleDate, AggressorFlag and TransTS
    // are currently unused...
    //
    bool is_buy             = (a_msg.side() == Msgs::Trade::side_t::BUY);
    long left               = a_msg.leaves_amt();
    assert(left >= 0);

    static const string       s_empty;
    Events::OrderID           root_id    = 0;
    long                      clord_id   = a_msg.clorder_id();
    long                      cnx_ord_id = a_msg.order_id();
    auto                      px         = a_msg.fill_rate();
    auto                      qty        = a_msg.fill_amt();

    auto req = this->Trade
    (
      clord_id,
      is_buy,
      ExchOrdID(cnx_ord_id),
      ExchOrdID(a_msg.m_exec_id, ' '),
      px,
      qty,
      0.0,                    // ExchFee:     Unknown, 0 is OK
      0.0,                    // BrokerFee:   Unknown, 0 is OK
      a_msg.leaves_amt(),
      -1,                     // CumQty:      Unknown
      NaN<double>,            // AvgPx:       Unknown
      m_msg_bldr->MsgTime(a_msg),
      m_msg_bldr->m_channel->LastRxTime()
    )->Req();

    //-----------------------------------------------------------------------//
    // Logging:                                                              //
    //-----------------------------------------------------------------------//
    // (1) Into a special TradeLogFile. TODO: simple PnL analysis right here?
    //
    auto sym  = m_msg_bldr->SymLookup(a_msg.instr_index());

    long settle_date = a_msg.settle_date() / 1000;

    auto write_trade = [&, this, req](TradeLogFile::Trade& a_trade)
    {
      auto info = req->GetAOS()->UnsafeClient();

      a_trade.m_time       = m_msg_bldr->m_channel->LastRxTime();
      a_trade.m_clord_id   = clord_id;
      a_trade.m_root_id    = root_id;
      utxx::copy(a_trade.m_strategy,  info ? info->Name() : std::string());
      utxx::copy(a_trade.m_exchange,  m_exchange_name);
      utxx::copy(a_trade.m_account,   m_msg_bldr->Username());
      utxx::copy(a_trade.m_symbol,    sym.data(), sizeof(sym)-1, ' ');
      a_trade.m_side       = a_msg.side() == Msgs::Trade::side_t::BUY ? 'B':'S';
      a_trade.m_qty        = a_msg.fill_amt();
      a_trade.m_px         = a_msg.fill_rate();
      a_trade.m_fees       = 0.0; // TODO
      a_trade.m_settle_date= settle_date;
      a_trade.m_exec_time  .milliseconds(a_msg.trans_time());
      a_msg.exec_id().copy_to(a_trade.m_exec_id, sizeof(a_trade.m_exec_id), ' ');
      a_trade.m_aggressor  =
        a_msg.aggressor_flag() == Msgs::Trade::aggressor_flag_t::AGRESSOR
        ? 'Y' : 'N';
    };

    m_trade_log_file->Write(write_trade);

    // (2) General Log:
    //
    if (utxx::unlikely(m_DebugLevel >= 3))
    {
      char sbuf[16], tbuf[32];
      utxx::timestamp::write_date(sbuf, settle_date, true);
      utxx::timestamp::format
        (utxx::DATE_TIME_WITH_MSEC,
         utxx::time_val(utxx::usecs(a_msg.trans_time()*1000)),
         tbuf, sizeof(tbuf), true);

      auto info = req->GetAOS()->UnsafeClient();
      char aggr = a_msg.aggressor_flag() == trade::aggressor_flag_t::AGRESSOR
                ? 'Y' : 'N';

      SLOG(INFO) << '[' << m_msg_bldr->m_channel->Name()
                << "] <- TRADE "
                << (a_msg.side() == Msgs::Trade::side_t::BUY ? "BUY" : "SELL")
                << qty << " @ " << utxx::fixed(px, 10, 5)
                << " (ord-id="  << clord_id << ", root-id=" << root_id
                << ", client="  << (info ? info->Name() : std::string())
                << " ["         << (req  ? req->GetAOS()->CliCID() : 0)
                << "], left="   << a_msg.leaves_amt()
                << ", aggr="    << aggr
                << ", exec-id=" << a_msg.exec_id().to_string(' ')
                << ", type="    << a_msg.decode_exec_type(a_msg.exec_type())
                << ", time="    << tbuf
                << ", settle="  << sbuf
                << std::endl;
    }
  }

} // namespace CNX
