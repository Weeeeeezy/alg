// vim:ts=2:et:sw=2
//===========================================================================//
//                        "ITCH_MsgBuilder_Currenex.hpp":                    //
//  A Message Builder Implementation for ITCH Currenex Market Data Protocol  //
//===========================================================================//
#ifndef MAQUETTE_CONNECTORS_ITCH_MSGBUILDER_CURRENEX_HPP
#define MAQUETTE_CONNECTORS_ITCH_MSGBUILDER_CURRENEX_HPP

#include <Common/Args.h>
#include <StrategyAdaptor/OrderBook.h>
#include <Infrastructure/MiscUtils.h>
#include <Connectors/ITCH_MsgBuilder_Currenex.h>
#include <Infrastructure/SecDefTypes.h>
#include <StrategyAdaptor/EventTypes.h>
#include <utxx/variant_tree_error.hpp>
#include <utxx/timestamp.hpp>

namespace CNX
{
  using namespace currenex::itch_2_0; // Auto-generated Currenex msg parser
  using MAQUETTE::EnvType;

  //=========================================================================//
  // Ctors, Dtor:                                                            //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // (0) Create:                                                             //
  //-------------------------------------------------------------------------//
  template<class Processor>
  ITCH_Currenex_MsgBuilder<Processor>*
  ITCH_Currenex_MsgBuilder<Processor>::Create
  (
    EPollReactor&            a_reactor,
    Processor*               a_processor,
    utxx::config_tree const& a_cfg,
    bool                     a_validate
  )
  {
    return Base::Create
      // Args passed to Base:      | Args passed to ctor:
      (a_cfg, nullptr, a_validate,   a_reactor, a_processor);
  }

  //-------------------------------------------------------------------------//
  // (1) Main Ctor:                                                          //
  //-------------------------------------------------------------------------//
  template<class Processor>
  ITCH_Currenex_MsgBuilder<Processor>::ITCH_Currenex_MsgBuilder
  (
    EPollReactor& a_reactor,
    Processor*    a_processor
  )
  : Base(a_reactor)
  , m_processor (a_processor)
  , m_channel   (nullptr)
  , m_session_id(0)
  {}

  //=========================================================================//
  // "ExchTimeStamp":                                                        //
  //=========================================================================//
  template <class Processor>
  inline utxx::time_val ITCH_Currenex_MsgBuilder<Processor>::
  ExchTimeStamp() const
  {
    return time_val(utxx::usecs(long(m_midnight_usec + m_exch_us_since_midnight)));
  }

  //-------------------------------------------------------------------------//
  // "DoInit":                                                               //
  //-------------------------------------------------------------------------//
  template<class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::DoInit(utxx::config_tree const&)
  {
    if (this->Channels().size() == 0)
      throw utxx::badarg_error
        (utxx::type_to_string<ITCH_Currenex_MsgBuilder<Processor>>(),
         "::DoInit: missing channel configuration!");
    m_channel = &(this->Channels()[0]);

    /// This call updates the cached timestamp::s_midnight_seconds variable
    utxx::timestamp::update();
    m_midnight_usec = utxx::timestamp::utc_midnight_useconds().value();
  }

  //=========================================================================//
  // "login":                                                                //
  //=========================================================================//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::Logon()
  {
    m_session_id = 0;
    logon msg(this->m_username, this->m_password, m_session_id);

    // We must always start with seqno=1
    m_channel->OseqNo(1);
    SetReplyHeader(msg);

    if (utxx::unlikely(m_processor->m_DebugLevel >= 1))
      SLOG(INFO) << '[' << m_channel->Name() << "] -> LOGON ("
                << "seqno=" << msg.seqno()    << ", username="
                << this->Username() << ')'    << std::endl;

    m_channel->State(ChannelStateData::StateT::LOGIN_SENT);
    m_channel->Write(&msg, sizeof(msg), 1);
  }

  //=========================================================================//
  // "Logout":                                                               //
  //=========================================================================//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::Logout()
  {
    logout_req msg
      (this->m_username, m_session_id, logout_req::logout_reason_t::USER_LOGOUT);
    SetReplyHeader(msg);
    m_channel->Write(&msg, sizeof(msg), 1);
    m_channel->State(ChannelStateData::StateT::DISCONNECTED);
    m_channel->Close();
  }

  //=========================================================================//
  // "LogoutAck":                                                            //
  //=========================================================================//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::
  LogoutAck(std::string const& a_userID, logout_req::logout_reason_t a_reason)
  {
    logout_req msg(a_userID, m_session_id, a_reason);
    SetReplyHeader(msg);
    m_channel->Write(&msg, sizeof(msg), 1);
  }

  //=========================================================================//
  // "Subscribe":                                                            //
  //=========================================================================//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::
  Subscribe(int16_t a_instIdx, bool a_subscr, bool a_trade_tick)
  {
    subscr_req msg
    (
      m_session_id,
      a_subscr     ? subscr_req::subscr_type_t::SUBSCRIBE
                   : subscr_req::subscr_type_t::UNSUBSCRIBE,
      a_instIdx,
      a_trade_tick ? subscr_req::subscr_to_ticker_t::TICKER_SUBSCR
                   : subscr_req::subscr_to_ticker_t::TICKER_DONT_SUBSCR
    );

    SetReplyHeader(msg);
    m_channel->Write(&msg, sizeof(msg), 1);
  }

  //=========================================================================//
  // "SymLookup":                                                            //
  //=========================================================================//
  template <class Processor>
  Events::SymKey const& ITCH_Currenex_MsgBuilder<Processor>::
  SymLookup(int16_t a_instr_idx) const
  {
    auto& map = this->m_processor->m_SecIDsMap;
    auto   it = map.find (a_instr_idx);
    return utxx::unlikely(it == map.end())
         ? Events::EmptySymKey() : it->second.m_Symbol;
  }

  //-------------------------------------------------------------------------//
  // "IdxLookup":                                                            //
  //-------------------------------------------------------------------------//
  template <class Processor>
  int16_t ITCH_Currenex_MsgBuilder<Processor>::
  IdxLookup(Events::SymKey const& a_symbol) const
  {
    auto& map = m_processor->GetSymbolsMap();
    auto it = map.find(a_symbol);

    if (utxx::unlikely(it == map.end()))
      throw utxx::badarg_error
        (utxx::type_to_string<ITCH_Currenex_MsgBuilder<Processor>>(),
         "Symbol index '", Events::ToString(a_symbol, ' '),
         "' not provisioned!");

    return it->second.m_SecID;
  }

  //=========================================================================//
  // High-Level API:                                                         //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "OnRead":                                                               //
  //-------------------------------------------------------------------------//
  //
  template<class Processor>
  int ITCH_Currenex_MsgBuilder<Processor>::OnRead
  (
    typename ITCH_Currenex_MsgBuilder<Processor>::
    Channel*                  a_channel,
    dynamic_io_buffer const&  a_buf,
    size_t&                   msg_count
  )
  {
    using MsgHeader = currenex::itch_2_0::header;

    // Process all complete msgs which happen to be in the buffer.
    // NB: "lastRxTime" is set irrespective to whether a complete msg was
    // processed, or not:
    char const* buff_begin = a_buf.rd_ptr();
    char const* buff_end   = a_buf.wr_ptr();
    char const* msg_begin  = buff_begin;

    // Use a local variable and update the msg_count reference on exit
    size_t msgs = 0;
    UTXX_SCOPE_EXIT([&]() { msg_count = msgs; });

    for(; msg_begin < buff_end; ++msgs)
    {
      // Find the msg beginning (marked by SOH):
      for (; msg_begin < buff_end && *msg_begin != SOH; ++msg_begin);

      if (msg_begin == buff_end)
        break;

      auto msg = reinterpret_cast<MsgHeader const*>(msg_begin);
      // Invoke the actual Builder (and Processor call-back):
      int sz   = this->buildMsg(msg, buff_end);

      if (utxx::unlikely(sz <= 0))
      {
        if (utxx::likely(sz == 0))
        {
          // there are no (more) complete msgs:
          if (utxx::unlikely(m_processor->m_DebugLevel >= 2))
            SLOG(INFO) << "ITCH_Currenex_MsgBuilder::OnRead: incomplete read ("
                      << (buff_end - msg_begin) << " bytes)" << std::endl;
          break;
        }
        // Couldn't determine the message size. There are two alternatives:
        // 1. Abort the connection
        // 2. Try to search for the ETX + SOH and recover.
        //
        // We choose the 1st approach:
        throw utxx::decode_error("Message not recognized");
      }

      char const* msg_end = msg_begin + sz;

      if (utxx::unlikely(*(msg_end-1) != ETX))
        throw utxx::decode_error
          ("Invalid message format: ETX not found at ", sz-1, " position: ",
           utxx::to_bin_string(msg_begin, std::min<size_t>(sz, 128)));
      // Next msg:
      msg_begin = msg_end;
    }
    // Return the number of bytes in complete msgs read:
    return (msg_begin - buff_begin);
  }

  //-------------------------------------------------------------------------//
  // "OnTimeout":                                                            //
  //-------------------------------------------------------------------------//
  //
  template<class Processor>
  inline bool ITCH_Currenex_MsgBuilder<Processor>::OnTimeout
  (
    typename ITCH_Currenex_MsgBuilder<Processor>::
    Channel*                  a_ch,
    TimeoutType               a_type,
    utxx::time_val            a_now
  )
  {
    switch (a_type)
    {
      case TimeoutType::CLIENT_TIMEOUT:
        // Heartbeats are not initiated by the connector but rather are sent
        // in response to the heartbeat notifications from the server, so
        // we ignore this timeout:
        return true;

      case TimeoutType::SERVER_TIMEOUT:
        break;

      default:
        assert(false); // This should never happen
    }
    return false;
  }

  //-------------------------------------------------------------------------//
  // "OnError":                                                              //
  //-------------------------------------------------------------------------//
  //
  template<class Processor>
  inline bool ITCH_Currenex_MsgBuilder<Processor>::OnError
  (
    typename ITCH_Currenex_MsgBuilder<Processor>::
    Channel*                  a_ch,
    IOType                    a_type,
    int                       a_ecode,
    std::string const&        a_error
  )
  {
    SLOG(ERROR) << "ITCH_Currenex_MsgBuilder: channel[" << a_ch->Name() << "] "
               << MAQUETTE::ToString(a_type)
               << " error '" << a_ch->Url()
               << (a_ecode <= 0 ? "" : utxx::to_string("': ", a_error, " (",
                                                       a_ecode, ')'))
               << std::endl;
    return true;
  }

  //-------------------------------------------------------------------------//
  // "OnConnected":                                                          //
  //-------------------------------------------------------------------------//
  //
  template<class Processor>
  inline void ITCH_Currenex_MsgBuilder<Processor>::OnConnected
    (typename ITCH_Currenex_MsgBuilder<Processor>::Channel* a_ch)
  {
    SLOG(INFO) << "ITCH_Currenex_MsgBuilder::OnConnected: connected '"
              << a_ch->Name()
              << "' channel to " << a_ch->Url() << std::endl;
    Logon();
  }

  //=========================================================================//
  // "onData": incoming message processing                                   //
  //=========================================================================//
  // This is just a fasade to call the right function in the processor
  template <class Processor>
  template<class T>
  inline void ITCH_Currenex_MsgBuilder<Processor>::onData(T const& a_msg)
  {
    m_processor->OnData(a_msg);
  }

  //-------------------------------------------------------------------------//
  // "onData": LOGON ACK                                                     //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::onData(logon const& a_msg)
  {
    m_session_id = a_msg.session_id();

    if (utxx::unlikely(m_processor->m_DebugLevel >= 1))
      SLOG(INFO) << '[' << m_channel->Name() << "] <- LOGON ACK ("
                << "seqno=" << a_msg.seqno()  << ')' << std::endl;

    // Successful authentication - request instrument information details:
    m_channel->State(ChannelStateData::StateT::READY);

    // Propagate login event to the processor:
    // TODO: add delayed changing of the READY state on receipt of InstrInfo
    // message.
    m_processor->OnData(a_msg);
  }

  //-------------------------------------------------------------------------//
  // "onData": LOGOUT                                                        //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::onData(logout_req const& a_msg)
  {
    if (utxx::unlikely(m_processor->m_DebugLevel >= 1))
      SLOG(INFO) << '['  << m_channel->Name() << "] <- LOGOUT REQ: "
                << logout_req::decode_logout_reason(a_msg.logout_reason())
                << " (" << a_msg.m_logout_reason.to_string(' ') << ')'
                << std::endl;

    // TODO: add delayed changing of the READY state on receipt of InstrInfo
    // message.
    m_processor->OnData(a_msg);

    // Propagate login event to the processor:
    m_channel->Close();
  }

  //-------------------------------------------------------------------------//
  // "onData": HEARTBEAT                                                     //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::onData(heartbeat const& a_msg)
  {
    if (utxx::unlikely(m_processor->m_DebugLevel >= 4))
      SLOG(INFO) << '['  << m_channel->Name() << "] <- HEARTBEAT (session="
                << a_msg.session_id()        << ')'
                << std::endl;

    // Reply to the server by ack'ing the heartbeat
    currenex::itch_2_0::heartbeat msg(m_session_id);
    SetReplyHeader(msg);
    m_channel->Write(&msg, sizeof(msg), 1);
  }

  //-------------------------------------------------------------------------//
  // "onData": InstrInfo                                                     //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::onData(instr_info const& a_msg)
  {
    // Store the instrument information (including its settlement date)
    // mapping (bi-directional):
    //
    Events::SymKey sym;
    Events::InitFixedStr(sym, a_msg.instr_id().to_string(' '));
    auto instr_idx     = a_msg.instr_index();
    auto update        = [&](MAQUETTE::SecDef& a_def, bool a_inserted)
    {
      a_def.m_SecID    = instr_idx;
      a_def.m_Maturity = a_msg.settle_date() / 1000;

      // NB: We could use a_inserted indicator to figure out if SecDef
      // was inserted to the connector's SymbolMap, however, since we
      // need to perform a lookup on the m_order_books anyway, we ignore
      // a_inserted indicator and do a lookup/insert unconditionally:
      auto& books = this->m_processor->m_order_books;
      auto it     = books.find(a_def.m_SecID);
      if (it != books.end())
        // FIXME: we are passing seqno=1 just to avoid a warning/error
        // in implementation of the order book:
        it->second.Clear    // Clear the book
          (m_channel->LastRxTime(), m_channel->LastRxTime(), 1);
      else
      {
        // (2) Create an INTERNAL OrderBook -- but the ShM one will be created
        //     automatically on export (as well as a ShM TradeBook). The Order
        //     Book created uses the OrderID API ("true" flag):
        (void) books.emplace
        (
          a_def.m_SecID,
          MAQUETTE::OrderBook
            (a_def.m_SecID, a_def.m_Symbol, m_processor->m_MktDepth, true,
             m_processor->m_VolWindow,      m_processor->m_PressureWinSec,
             m_processor->m_DebugLevel,     a_def.m_MinPriceIncrement));
      }

      return a_def;
    };

    // Fill in the protocol-independent "SecDef":
    SecDef const* sec_def;
    bool          installed;
    std::tie(sec_def, installed) = m_processor->UpdateSecDefMaps(sym, update);
    assert(sec_def);

    if (utxx::unlikely(!installed))
      SLOG(WARNING) << "ITCH_MsgBuilder_Currenex: "
                    << "SecDef not installed in ShM/DB: "
                    << sec_def->m_Symbol.data() << " due to prefix filtering"
                    << std::endl;

    // Acknowledge receipt of the instrument info message
    instr_info_ack reply(m_session_id, instr_idx);
    SetReplyHeader(reply);
    m_channel->Write(&reply, sizeof(reply), 1);

    std::string status;

    if (m_processor->m_SymPrefixes.size() != 0 && !m_processor->PrefixMatch(sym))
    {
      status    = utxx::to_string("subscription ignored");
      installed = false;
    }
    else if (m_processor->m_auto_subscribe)
    {
      // Subscribe to this instrument
      Subscribe(instr_idx, true);
      status = "subscribed";
    }
    else
      status = "auto-subscription disabled";

    // TODO: Move all code below to the connector implementation:
    MAQUETTE::XConnector_MktData* mkt_conn =
      dynamic_cast<MAQUETTE::XConnector_MktData*>(m_processor);

    if (!mkt_conn && this->m_processor->IsEnvironment(EnvType::PROD))
      throw utxx::logic_error
        ("ITCH_MsgBuilder_Currenext::onData: invalid connector "
          "Implementation: class must derive from XConnector_MktData!");

    //-----------------------------------------------------------------------//
    // Update ShM Data Structs: "SecDefsShM":                                //
    //-----------------------------------------------------------------------//
    if (!m_processor->NoShM() && mkt_conn && installed) // NOT just DB Recording
    {
      assert(mkt_conn != nullptr);

      MAQUETTE::SecDefsShM* shm = mkt_conn->GetSecDefsShM();

      assert(shm != nullptr);

      // Install the "def" in Shared Memory, creating a new Book if necessary,
      // without any subscriptions yet:
      (void) shm->Add(instr_idx, sec_def->m_Symbol, *sec_def, false);

      if (utxx::unlikely(m_processor->DebugLevel() >= 2))
        SLOG(INFO) << "ITCH_MsgBuilder_Currenex::buildMsg: added '"
                   << Events::ToString(sec_def->m_Symbol)  << "' -> "
                   << instr_idx << " mapping (" << status << ')' << std::endl;
    }

  }

  //-------------------------------------------------------------------------//
  // "onData": SUBSCR_REPLY                                                  //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void ITCH_Currenex_MsgBuilder<Processor>::onData(reject const& a_msg)
  {
    if (m_channel->State() == ChannelStateData::StateT::LOGIN_SENT)
    {
      m_channel->State(ChannelStateData::StateT::DISCONNECTED);
      m_channel->Close();
    }
    m_processor->OnData(a_msg);
  }

  //-------------------------------------------------------------------------//
  // Helper functions:                                                       //
  //-------------------------------------------------------------------------//
  namespace
  {
    template <typename T>
    constexpr char MT() { return T::s_msg_type; }
  }

  //-------------------------------------------------------------------------//
  // "buildMsg":                                                             //
  //-------------------------------------------------------------------------//
  template<class Processor>
  template <class MsgHeader>
  int ITCH_Currenex_MsgBuilder<Processor>::buildMsg
    (MsgHeader const* a_hdr, char const* a_end)
  {

    //auto their_seqno = a_hdr->seqno();
    // TODO: Detect seqno gaps
    //m_channel->iseqno(their_seqno);
    m_exch_us_since_midnight = long(a_hdr->timestamp()) * 1000;

    int n = -1;

    try
    {
      switch (a_hdr->msg_type())
      {
        //-------------------------------------------------------------------//
        // Incoming messages                                                 //
        //-------------------------------------------------------------------//
        case MT<logon         >(): AS<logon         >(a_hdr, out(n), a_end); break;
        case MT<logout_req    >(): AS<logout_req    >(a_hdr, out(n), a_end); break;
        case MT<heartbeat     >(): AS<heartbeat     >(a_hdr, out(n), a_end); break;
        case MT<instr_info    >(): AS<instr_info    >(a_hdr, out(n), a_end); break;
        case MT<instr_info_ack>(): AS<instr_info_ack>(a_hdr, out(n), a_end); break;
        case MT<subscr_reply  >(): AS<subscr_reply  >(a_hdr, out(n), a_end); break;
        case MT<price_msg     >(): AS<price_msg     >(a_hdr, out(n), a_end); break;
        case MT<price_cancel  >(): AS<price_cancel  >(a_hdr, out(n), a_end); break;
        case MT<trade_ticker  >(): AS<trade_ticker  >(a_hdr, out(n), a_end); break;
        case MT<reject        >(): AS<reject        >(a_hdr, out(n), a_end); break;
        //-------------------------------------------------------------------//
        default:                                                             //
        //-------------------------------------------------------------------//
          m_channel->IncRxInvalidMsgs();
          SLOG(WARNING) << "ITCH_Currenex_MsgBuilder::buildMsg: invalid msg:  \n"
            << utxx::to_bin_string(reinterpret_cast<const char*>(a_hdr),
                                   sizeof(a_hdr), false, true, true);
          // This would return -1
          break;
      }
    }
    catch (std::exception const& e)
    {
      SLOG(ERROR) << "ITCH_Currenex_MsgBuilder::buildMsg: "
                 << e.what() << std::endl;
      assert(n > -1);
    }

    return n;
  }

} // namespace CNX

#endif // MAQUETTE_CONNECTORS_ITCH_MSGBUILDER_CURRENEX_HPP

