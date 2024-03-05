// vim:ts=2:et:sw=2
//===========================================================================//
//                      "XConnector_ITCH_Currenex.cpp":                      //
//          Instantiation of the ITCH Protocol Connector for Currenex        //
//===========================================================================//
#include "Common/DateTime.h"
#include "Connectors/ITCH_MsgBuilder_Currenex.hpp"
#include "Connectors/XConnector_ITCH_Currenex.h"
#include "Connectors/XConnector_ITCH_Currenex.schema.generated.hpp"
#include "Infrastructure/MiscUtils.h"
#include "StrategyAdaptor/EventTypes.h"
#include "StrategyAdaptor/OrderBook.h"
#include <utxx/string.hpp>
#include <utxx/verbosity.hpp>
#include <stdexcept>
#include <cassert>
#include <type_traits>

using namespace currenex::itch_2_0; // Auto-generated Currenex msg parser
using namespace Arbalete;
using namespace std;
using namespace MAQUETTE;

namespace CNX
{
  //=========================================================================//
  // XConnector Factory Registration:                                        //
  //=========================================================================//
  // This instance adds XConnector_ITCH_Currenex implementation to the
  // XConnector registrar:
  static XConnectorRegistrar::Factory<XConnector_ITCH_Currenex>
    s_xconnector_registrar
    (
      CFG::ITCH_Currenex::alias(),
      &XConnector_ITCH_Currenex::Create,
      CFG::ITCH_Currenex::instance()
    );

  //=========================================================================//
  // "XConnector_ITCH_Currenex::Create":                                     //
  //=========================================================================//
  XConnector_ITCH_Currenex* XConnector_ITCH_Currenex::Create
  (
    std::string       const& a_conn_name,
    utxx::config_tree const& a_cfg
  )
  {
    try   { return new XConnector_ITCH_Currenex(a_conn_name, nullptr, a_cfg); }
    catch ( std::exception const& e )
    {
      if (utxx::verbosity::enabled(utxx::VERBOSE_DEBUG))
        a_cfg.dump(std::cerr << utxx::type_to_string<XConnector_ITCH_Currenex>()
                   << "::Create:\n", 2, false, true);
      throw;
    }
  }

  //=========================================================================//
  // "XConnector_ITCH_Currenex" Ctor:                                        //
  //=========================================================================//
  XConnector_ITCH_Currenex::XConnector_ITCH_Currenex
  (
    std::string       const& a_conn_name,
    EPollReactor*            a_reactor,
    utxx::config_tree const& a_cfg
  )
  : //-----------------------------------------------------------------------//
    // Virtual base of XConnector -- need explicit Ctor invocation:          //
    //-----------------------------------------------------------------------//
    Named_Base(a_conn_name)
    //-----------------------------------------------------------------------//
    // This class has a virtual base -- need explicit Ctors invocation:      //
    //-----------------------------------------------------------------------//
  , XConnector
    (
      utxx::type_to_string<XConnector_ITCH_Currenex>(),
      a_conn_name,
      a_reactor,
      a_cfg
    )
    //-----------------------------------------------------------------------//
    // "XConnector_MktData" itself:                                          //
    //-----------------------------------------------------------------------//
  , XConnector_MktData        // Base class initialisation
    (
      a_conn_name,
      a_reactor,
      a_cfg
    )                         // Flds of this class
    //-----------------------------------------------------------------------//
    // Alias<XConnector_ITCH_Currenex> Ctor:                                 //
    //-----------------------------------------------------------------------//
  , Alias<XConnector_ITCH_Currenex>(a_conn_name)
  , m_configuration   (a_cfg)
  , m_stop            (false)
  , m_raw_msg_log_file(LogFileFactory::Instance().Create
                        (a_cfg.get<std::string>
                          (CFG::ITCH_Currenex::RAW_MSG_LOGGER_TYPE()),
                         true, a_conn_name))
  , m_unhandled_msgs  (0)
  , m_auto_subscribe  (a_cfg.get<bool>
                        (CFG::ITCH_Currenex::AUTO_SUBSCRIBE()))
  {
    // NB: The above call does NOT create MktData-related data structs in
    // ShM or internal memory, so do it explicitly:
    //
    for (auto cit =
         this->m_SymbolsMap.cbegin();  cit != this->m_SymbolsMap.cend(); ++cit)
    {
      SecDef const& def = cit->second;

      // If SecID is not invalid, insert it to the (SecID -> SecDef) map:
      auto it = this->m_SecIDsMap.find(def.m_SecID);
      if (def.m_SecID != Events::InvalidSecID)
      {
        if (it == this->m_SecIDsMap.end())
          m_SecIDsMap.insert(std::make_pair(def.m_SecID, def));
        else if (def.m_SecID != it->second.m_SecID)
          throw utxx::runtime_error
            (ClassName(), "::Ctor: mismatch in SecID of ",
             Events::ToString(def.m_Symbol),
             " SecIDsMap.SecID=",  it->second.m_SecID,
             " SymbolsMap.SecID=", def.m_SecID);
      }

      // (1) Create a "SecDefsShM" entry:
      if (this->NoShM())
        SLOG(WARNING) << ClassName()
                     << "::Ctor: running without shared memory!" << endl;
      else if (this->PrefixMatch(def.m_Symbol))
      {
        assert(this->m_SecDefsShM != NULL);
        // NB: "create_book" is "true":
        (void) this->m_SecDefsShM->Add(def.m_SecID, def.m_Symbol, def, true);
      }

      // (2) Create an INTERNAL OrderBook -- but the ShM one will be created
      //     automatically on export (as well as a ShM TradeBook). The Order
      //     Book created uses the OrderID API ("true" flag):
      (void) m_order_books.emplace
      (
        def.m_SecID,
        OrderBook
          (def.m_SecID, def.m_Symbol, m_MktDepth, true, this->m_VolWindow,
           this->m_PressureWinSec, this->m_DebugLevel, def.m_MinPriceIncrement)
      );
    }

    // Start the Threads:
    // The C3 Thread comes first, so it can immediately respond to various ext-
    // ernal requests (XXX: so this is not very important anymore...):
    // TODO
    /*
    m_c3_thread.reset
      (new Arbalete::Thread([this]()->void { this->C3Loop(); }));
    */

    // The Flusher thread (since the "SecDef"s are now in place) -- only if DB
    // recording is in place; Connector Events are recorded in the DB directly:
    if (this->m_WithDBRec)
      m_flusher_thread.reset
        (new Arbalete::Thread([this]()->void
                            { DB_MktData::ToDB::FlusherThreadBody(this); }));

    // And the thread which executes queued Start / Stop requests. XXX: once
    // again, this sequence may not be the best nowadays:
    m_start_stop_thread.reset
      (new Arbalete::Thread([this]()->void { this->StartStopLoop(); }));
  }

  //=========================================================================//
  // "XConnector_ITCH" Dtor:                                                 //
  //=========================================================================//
  XConnector_ITCH_Currenex::~XConnector_ITCH_Currenex()
  {
    // Terminate the "StartStopThread" (first, so it will not interfere) and
    // the "C3Thread":
    TerminateThread(m_start_stop_thread);
    //TerminateThread(m_c3_thread);

    // Stop the Receiver Thread (this also closes the FDs if they are still
    // open, and destroys the Msg Builder):
    Stop();

    // Stop the Flusher Thread. We do it at the last point because it may still
    // have to record some data originating from the above operations:
    sleep(1);
    TerminateThread(m_flusher_thread);

    // Log a msg (XXX: No difference in ConnEventT between "Stop" and "Quit"?):
    SLOG(INFO) << ClassName() + "::Dtor: CONNECTOR TERMINATED" << std::endl;
  }

  //=========================================================================//
  // Construct the MsgBuilder:                                               //
  //=========================================================================//
  void XConnector_ITCH_Currenex::MkMsgBldr()
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
      SLOG(ERROR) << ClassName()     << "::MkMsgBldr: " << e.what()
                 << ": TERMINATING" << std::endl;
      throw;
    }
  }

  //=========================================================================//
  // "DoStart":                                                              //
  //=========================================================================//
  // Invoked from the Management Thread (either directly or from the Ctor):
  //
  void XConnector_ITCH_Currenex::DoStart()
  {
    std::lock_guard<std::mutex> lock(m_starter_lock);

    if (this->GetStatus() == Status::Active)
      throw utxx::runtime_error(ClassName(), "::DoStart: already started!");

    m_stop = false;         // Clear the "Stop" flag (if it was set)

    // Construct the Msg Builder:
    MkMsgBldr();

    this->SetStatus(Status::Active);
  }

  //=========================================================================//
  // "DoStop":                                                               //
  //=========================================================================//
  // This is much more involved than Start() -- must provide graceful shutdown.
  // Invoked from the Management Thread only, so no need for locking:
  //
  void XConnector_ITCH_Currenex::DoStop()
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
  void XConnector_ITCH_Currenex::DoWaitForCompletion()
  {
    std::lock_guard<std::mutex> lock(m_starter_lock);

    // The following calls reset the unique pointers after completion of join:
    JoinThread(m_c3_thread);
    JoinThread(m_start_stop_thread);
  }

  //=========================================================================//
  // "DoRun":                                                                //
  //=========================================================================//
  void XConnector_ITCH_Currenex::DoRestartRun()
  {
    SLOG(ERROR) << ClassName() << "::DoRestartRun: recreating MsgBuilder\n";

    // Remove the Msg Builder:
    m_msg_bldr_ptr.reset();

    // Remove accumulated MktData, both internal and external, since we
    // will receive new snapshots after re-connection:
    //
    // Internal OrderBooks:
    for (auto it = m_order_books.begin(); it != m_order_books.end();  ++it)
      it->second.Invalidate();

    // External (ShM) OrderBooks and TradeBooks (SecDefs are preserved
    // since they are statically-initialised):
    assert(this->m_OrderBooksShM != NULL);
    this->m_OrderBooksShM->ClearAllBooks();

    assert(this->m_TradeBooksShM != NULL);
    this->m_TradeBooksShM->ClearAllBooks();

    // Minor wait to prevent bombarding the server with failed connect
    // attempts:
    usleep(100000);

    // Re-create the Msg Builder:
    MkMsgBldr();
  }

  //=========================================================================//
  // "TotalBytes{Up,Down}":                                                  //
  //=========================================================================//
  // XXX: This statistics is not exact -- the byte counts are reset when the
  // Connector is "Stop"ped:
  //
  long XConnector_ITCH_Currenex::TotalBytesUp()  const
    { return utxx::likely(m_msg_bldr) ? m_msg_bldr->RxBytes() : 0u; }

  long XConnector_ITCH_Currenex::TotalBytesDown() const
    { return utxx::likely(m_msg_bldr) ? m_msg_bldr->RxBytes() : 0u; }


  //=========================================================================//
  // ITCH Currenex Message Processor (Call-Back Functions):                  //
  //=========================================================================//

  //-------------------------------------------------------------------------//
  // "Logon" Call-Back:                                                      //
  //-------------------------------------------------------------------------//
  // Notify al Clients that the Connector is on-line. XXX: However, most lik-
  // ely, there are no subscribed Clients yet:
  //
  void XConnector_ITCH_Currenex::OnData(Msg::Logon const& a_msg)
  { this->NotifyAllClients(Events::EventT::ConnectorStart); }

  //-------------------------------------------------------------------------//
  // "LoginRejected", "EndOfSession" and "LogoutRequest" Call-Backs:         //
  //-------------------------------------------------------------------------//
  // Notify al Clients that the Connector is off-line:
  //
  void XConnector_ITCH_Currenex::OnData(Msg::Reject const& a_msg)
  {
    // If this is a logon reject, notify the clients
    if (a_msg.reject_msg_type() == Msg::Logon::s_msg_type)
      this->NotifyAllClients(Events::EventT::ConnectorStop);
  }

  void XConnector_ITCH_Currenex::OnData(Msg::Logout const& a_msg)
  { this->NotifyAllClients(Events::EventT::ConnectorStop);  }

  //-------------------------------------------------------------------------//
  // "PriceMsg" Call-Back:                                                   //
  //-------------------------------------------------------------------------//
  void XConnector_ITCH_Currenex::OnData(Msg::PriceMsg const& a_msg)
  {
    // Install this Order in the "scratch" OrderBook:
    Msg::PriceMsg::side_t  bs = a_msg.side();
    auto            instr_idx = a_msg.instr_index();
    Events::SecID       secID = instr_idx;
    OrderBooksT::iterator  it = this->m_order_books.find(secID);
    if (utxx::unlikely(it    == this->m_order_books.end()))
    {
      if (this->m_DebugLevel >= 3)
        SLOG(WARNING) << this->ClassName()
                    << "::Processor: PriceMsg: ignoring data for SecID: "
                    << instr_idx << endl;
      return;
    }
    OrderBook*             ob = &(it->second);
    // XXX: Presumably, the qty is always integral:
    long                  qty = long(a_msg.max_amount());
    double                 px = a_msg.price();
    bool               is_bid = bs == Msg::PriceMsg::side_t::BID;

    //-----------------------------------------------------------------------//
    // Memoise the L1 data: "bids", "asks" are pairs (Entry*, NEntries):     //
    //-----------------------------------------------------------------------//
    auto bids  = ob->GetBids();
    auto asks  = ob->GetAsks();
    int  nbids = bids.second;
    int  nasks = asks.second;

    OrderBook::Entry const* bid0 = (nbids >= 1) ? bids.first : NULL;
    OrderBook::Entry const* ask0 = (nasks >= 1) ? asks.first : NULL;

    //-----------------------------------------------------------------------//
    // OK, ready to install it:                                              //
    //-----------------------------------------------------------------------//
    assert(ob != NULL);
    int  ordID = a_msg.price_id();
    auto seqno = a_msg.seqno();

    std::pair<bool, int> res = ob->UpsertOrder
    (
      ordID,
      is_bid,
      px,
      qty,  // XXX: Presumably, the qty is always integral
      utxx::time_val(),         // XXX: "recv_time" not available
      m_msg_bldr->Transport()->LastRxTime(),
      seqno
    );

    bool inserted = res.first;
    int  level    = res.second;

    if (utxx::unlikely(level < 0)) // Usually because the level exceeds max
      return;

    //-----------------------------------------------------------------------//
    // Propagate the updated OrderBook to ShM and/or DB:                     //
    //-----------------------------------------------------------------------//
    // The book is already sorted (this is guaranteed to the OrderID-based
    // insertion mechanism):
    this->OrderBookExport(ob, seqno,
                          m_msg_bldr->Transport()->LastRxTime(), true);

    if (utxx::unlikely(m_DebugLevel >= 1))
    {
      auto lat = utxx::now_utc().diff_usec
                 (m_msg_bldr->Transport()->LastRxTime());
      // Only log the L1 updates if px was updated, not just qty:
      bid0 = (bids.second >= 1) ? bids.first : NULL;
      ask0 = (asks.second >= 1) ? asks.first : NULL;

      double newBidPx  = NaN<double>;
      long   newBidQty = 0;
      double newAskPx  = NaN<double>;
      long   newAskQty = 0;

      if (utxx::likely(bid0 != NULL))
      {
        newBidPx  = bid0->m_price;
        newBidQty = bid0->m_qty;
      }
      if (utxx::likely(ask0 != NULL))
      {
        newAskPx  = ask0->m_price;
        newAskQty = ask0->m_qty;
      }

      if (utxx::unlikely(m_DebugLevel >= 3 || (m_DebugLevel >= 1 && level == 0)))
      {
        auto t = m_msg_bldr->ExchTimeStamp();
        char buf[32];
        char* p = utxx::timestamp::write_time
                  (buf, t, utxx::TIME_WITH_MSEC, true, '\0');
        *p++    = ' ';
        p       = stpncpy(p, ob->GetSymbol().data(), 7);
        *p      = '\0';

        // FIXME: using this utxx API instead of LOG_INFO so that it doesn't
        // record __FILE__:__LINE__. This can be controlled in the upcoming
        // version of utxx, but for now call the internal implementation
        // directly:
        utxx::_lim(utxx::logger::instance(), utxx::LEVEL_INFO, "").log
          ("%s |%c|%c| %7.2f @ %10.5f |L%02d/%02d "
           "{%7.2f @ %10.5f, %7.2f @ %10.5f } (%4ldus) #%d\n",
            buf, (is_bid ? 'B' : 'S'), (inserted ? 'N' : 'M'),
            double(qty)/1000000, px,
            level, (is_bid ? nbids : nasks),
            double(newBidQty)/1000000, newBidPx,
            double(newAskQty)/1000000, newAskPx,
            lat, a_msg.price_id());

        if (utxx::unlikely(m_DebugLevel >= 6))
          ob->Print();
      }
    }
    // Update run-time stats:
    // TODO:
    //auto sit  = this->m_SecIDsMap.find(instr_idx);
    //if  (sit != this->m_SecIDsMap.end())
    //  sit->second.m_NewPriceCount++;
  }

  //-------------------------------------------------------------------------//
  // "PriceCancel" Call-Back:                                                //
  //-------------------------------------------------------------------------//
  void XConnector_ITCH_Currenex::OnData(Msg::PriceCancel const& a_msg)
  {
    // Install this Order in the "scratch" OrderBook:
    Events::SecID       secID = a_msg.instr_index();
    OrderBooksT::iterator  it = this->m_order_books.find(secID);
    if (it == this->m_order_books.end())
    {
      char const*        pair = (char const*)(&secID);
      throw utxx::runtime_error
            ("XConector_ITCH_Currenex::Processor: Order"
              "Cancelled: UnRecognised Pair: " + string(pair));
    }
    OrderBook* ob = &(it->second);
    int     ordID = a_msg.price_id();
    auto    seqno = a_msg.seqno();

    // Now actually modify it:
    OrderBook::En3 deleted;
    int level =
      ob->DeleteOrder(ordID, { 0, 0 }, time_val(), seqno, &deleted);

    if (utxx::unlikely(m_DebugLevel >= 3 || (m_DebugLevel >= 1 && level == 1)))
    {
      //---------------------------------------------------------------------//
      // Memoise the L1 data: "bids", "asks" are pairs (Entry*, NEntries):   //
      //---------------------------------------------------------------------//
      auto bids  = ob->GetBids();
      auto asks  = ob->GetAsks();
      int  nbids = bids.second;
      int  nasks = asks.second;

      OrderBook::Entry const* bid0 = (nbids >= 1) ? bids.first : NULL;
      OrderBook::Entry const* ask0 = (nasks >= 1) ? asks.first : NULL;

      int    bid_qty = bid0 ? bid0->m_qty   : 0;
      double bid_px  = bid0 ? bid0->m_price : 0.0;
      int    ask_qty = ask0 ? ask0->m_qty   : 0;
      double ask_px  = ask0 ? ask0->m_price : 0.0;

      auto lat = utxx::now_utc().diff_usec
                 (m_msg_bldr->Transport()->LastRxTime());

      auto t = m_msg_bldr->ExchTimeStamp();
      char buf[32];
      char* p = utxx::timestamp::write_time
                (buf, t, utxx::TIME_WITH_MSEC, true, '\0');
      *p++    = ' ';
      p       = stpncpy(p, ob->GetSymbol().data(), 7);
      *p      = '\0';

      // FIXME: using this utxx API instead of LOG_INFO so that it doesn't
      // record __FILE__:__LINE__. This can be controlled in the upcoming
      // version of utxx, but for now call the internal implementation
      // directly:
      utxx::_lim(utxx::logger::instance(), utxx::LEVEL_INFO, "").log
        ("%s |%c|X| %7.2f @ %10.5f |L%02d/%2d "
         "{%7.2f @ %10.5f, %7.2f @ %10.5f } (%4ldus) #%d [%ld]\n",
         buf,
         (deleted.m_isBid ? 'B' : 'S'),
         double(deleted.m_qty)/1000000, deleted.m_price,
         level, (deleted.m_isBid ? nbids : nasks),
         double(bid_qty)/1000000, bid_px,
         double(ask_qty)/1000000, ask_px,
         lat, a_msg.price_id(), m_msg_bldr->Transport()->SeqGaps());
    }

    // Propagate the changes:
    this->OrderBookExport(ob, 0, m_msg_bldr->Transport()->LastRxTime(), true);
  }

  //-------------------------------------------------------------------------//
  // "TradeTicker" Call-Back:                                                //
  //-------------------------------------------------------------------------//
  void XConnector_ITCH_Currenex::OnData(Msg::TradeTicker const& a_msg)
  {
    // The Trade data are exported directly into "TradeBooksShM":
    auto             bs = a_msg.ticker_type();
    auto      instr_idx = a_msg.instr_index();
    Events::SecID secID = instr_idx;
    Events::SymKey  sym = m_msg_bldr->SymLookup(instr_idx);

    // XXX: Aggregate initialiser does not work for some reason:
    TradeEntry    trEn;
    trEn.m_RecvTime  = m_msg_bldr->Transport()->LastRxTime();
    trEn.m_SeqNum    = a_msg.seqno();
    trEn.m_symbol    = sym;
    trEn.m_price     = a_msg.rate();
    trEn.m_qty       = 0;            // XXX: Qty is not available!
    trEn.m_eventTS   = m_msg_bldr->Transport()->LastRxTime(); // TODO ???
    trEn.m_aggressor = ( (bs == trade_ticker::ticker_type_t::GIVEN)
                          ? AggressorT::BidSide
                          : (bs == trade_ticker::ticker_type_t::PAID
                              ? AggressorT::AskSide : AggressorT::Undefined) );
    // Export it:
    this->TradeExport(secID, trEn);

    // Update run-time stats:
    // TODO:
    //auto it  = this->m_SecIDsMap.find(instr_idx);
    //if  (it != this->m_SecIDsMap.end())
    //  it->second.m_TradeCount++;
  }

  //=========================================================================//
  // "SubscriptionReply" Call-Back:                                          //
  //=========================================================================//
  void XConnector_ITCH_Currenex::OnData(Msg::SubscrReply const& a_msg)
  {
    Events::SecID secID = a_msg.instr_index();

    // TODO: Notify the clients!

    if (a_msg.sub_rep_type()
        == Msg::SubscrReply::sub_rep_type_t::SUBSCR_ACCEPTED)
    {
      SLOG(INFO)     << "XConnector_ITCH_Currenex: subscribed to instrument: "
                    << secID << endl;
    }
    else
    {
      auto reason = a_msg.sub_rep_reason().to_string(' ');

      if (reason.find_first_of("Duplicate subscription") != string::npos)
      {
        SLOG(WARNING)  << utxx::type_to_string<XConnector_ITCH_Currenex>()
                      << "::OnData: subscription to instrument "
                      << secID << " failed: " << reason << endl;
      }
    }
  }

} // namespace ITCH
