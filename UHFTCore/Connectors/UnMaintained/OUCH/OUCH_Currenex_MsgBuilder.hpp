// vim:ts=2:et:sw=2
//===========================================================================//
//                      "OUCH_Currenex_MsgBuilder.hpp":                      //
//   A Common Message Parser Implementation for OUCH Order Entry Protocol    //
//===========================================================================//
#ifndef MAQUETTE_CONNECTORS_OUCH_CURRENEX_MSGBUILDER_HPP
#define MAQUETTE_CONNECTORS_OUCH_CURRENEX_MSGBUILDER_HPP

#include "Common/Args.h"
#include "Common/Maths.h"
#include "Connectors/Configs_Currenex.h"
#include "Connectors/OUCH_Currenex_MsgBuilder.h"
#include <Infrastructure/SecDefTypes.h>
#include <Infrastructure/MiscUtils.h>
#include <Infrastructure/Logger.h>
#include <utxx/timestamp.hpp>
#include <utxx/nchar.hpp>
#include <utxx/string.hpp>
#include <Infrastructure/Logger.h>
#include <sys/time.h>
#include <stdexcept>
#include <cassert>

namespace CNX
{
  using namespace currenex::ouch_1;
  using namespace Arbalete;
  using namespace MAQUETTE;

  //=========================================================================//
  // Ctors, Dtor:                                                            //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // (0) Create:                                                             //
  //-------------------------------------------------------------------------//
  template<class Processor>
  OUCH_Currenex_MsgBuilder<Processor>*
  OUCH_Currenex_MsgBuilder<Processor>::Create
  (
    EPollReactor&            a_reactor,
    Processor*               a_processor,
    utxx::config_tree const& a_cfg,
    bool                     a_validate
  )
  {
    return Base::Create(a_cfg, nullptr, a_validate,
                        // Args passed to ctor:
                        a_reactor, a_processor);
  }

  //-------------------------------------------------------------------------//
  // (1) Main Ctor:                                                          //
  //-------------------------------------------------------------------------//
  template<class Processor>
  OUCH_Currenex_MsgBuilder<Processor>::OUCH_Currenex_MsgBuilder
  (
    EPollReactor& a_reactor,
    Processor*    a_processor
  )
  : Base(a_reactor)
  , m_processor (a_processor)
  , m_channel   (nullptr)
  , m_session_id(0)
  {}

  //-------------------------------------------------------------------------//
  // "DoInit":                                                               //
  //-------------------------------------------------------------------------//
  template<class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::DoInit(utxx::config_tree const&)
  {
    if (this->Channels().size() == 0)
      throw utxx::badarg_error
        (utxx::type_to_string<OUCH_Currenex_MsgBuilder<Processor>>(),
         "::DoInit: missing channel configuration!");
    m_channel = &(this->Channels()[0]);
  }

  //-------------------------------------------------------------------------//
  // "SymLookup":                                                            //
  //-------------------------------------------------------------------------//
  template <class Processor>
  Events::SymKey const& OUCH_Currenex_MsgBuilder<Processor>::
  SymLookup(int16_t a_instr_idx) const
  {
    auto& map = m_processor->GetSecIDsMap();
    auto  it  = map.find(a_instr_idx);

    if (utxx::unlikely(it == map.end()))
      throw utxx::badarg_error
        (utxx::type_to_string<OUCH_Currenex_MsgBuilder<Processor>>(),
         " Cannot find symbol for instrument index (", a_instr_idx, ")");

    return it->second.m_Symbol;
  }

  //-------------------------------------------------------------------------//
  // "SymLookup":                                                            //
  //-------------------------------------------------------------------------//
  template <class Processor>
  int16_t OUCH_Currenex_MsgBuilder<Processor>::
  IdxLookup(Events::SymKey const& a_symbol) const
  {
    auto& map = m_processor->GetSymbolsMap();
    auto  it  = map.find(a_symbol);

    if (utxx::unlikely(it == map.end()))
      throw utxx::badarg_error
        (utxx::type_to_string<OUCH_Currenex_MsgBuilder<Processor>>(),
         "Symbol index '", Events::ToString(a_symbol, ' '),
         "' not provisioned!");

    return it->second.m_SecID;
  }

  //-------------------------------------------------------------------------//
  // "SetHeader":                                                            //
  //-------------------------------------------------------------------------//
  template<class Processor>
  template<class Msg>
  inline void OUCH_Currenex_MsgBuilder<Processor>::SetHeader(Msg& a_msg)
  {
    // Note that a_msg constructor automatically sets SOH, so we don't need to
    // bother with setting it manually.
    auto seqno          = m_processor->IncTxSeqno();
    auto since_midnight = utxx::now_utc().microseconds() - m_midnight_usec;
    a_msg.seqno         (seqno);
    a_msg.timestamp     (since_midnight / 1000);
    a_msg.ETX(ETX);     // Set the terminating symbol
  }

  //-------------------------------------------------------------------------//
  // "Login":                                                                //
  //-------------------------------------------------------------------------//
  template<class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::Logon()
  {
    currenex::ouch_1::logon msg(this->Username(), this->Password(), 0);
    SetHeader(msg);

    if (utxx::unlikely(m_processor->m_DebugLevel >= 1))
      LOG_INFO("[%s] -> LOGON (seqno=%d, username=%s)\n",
               m_channel->Name().c_str(), msg.seqno(), this->Username().c_str());

    m_channel->Write(&msg, sizeof(msg), 1);
    m_channel->State(ChannelStateData::StateT::LOGIN_SENT);

    utxx::timestamp::update();
    m_midnight_usec = utxx::timestamp::utc_midnight_useconds().value();
  }

  //-------------------------------------------------------------------------//
  // "Logout":                                                               //
  //-------------------------------------------------------------------------//
  template<class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::Logout()
  {
    if (!m_channel->Connected())
      return;

    using ReasonT = logout::logout_reason_t;

    // There's no logout message, so we just close the channels
    currenex::ouch_1::logout msg
      (this->Username(), m_session_id, ReasonT::USER_LOGOUT);
    SetHeader(msg);

    m_channel->Write(&msg, sizeof(msg), 1);
    m_channel->Close();
  }

  //-------------------------------------------------------------------------//
  // "Heartbeat":                                                            //
  //-------------------------------------------------------------------------//
  template<class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::Heartbeat()
  {
    if (utxx::unlikely(!m_channel->Connected()))
      return;

    if (utxx::unlikely(!IsReady(m_channel)))
      return;

    currenex::ouch_1::heartbeat msg(m_session_id);
    SetHeader(msg);
    m_channel->Write(&msg, sizeof(msg), 1);
  }

  //-------------------------------------------------------------------------//
  // "InstrInfoReq":                                                         //
  //-------------------------------------------------------------------------//
  // Request instrument directory
  template<class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::InstrInfoReq()
  {
    if (!m_channel->Connected())
      return;

    currenex::ouch_1::instr_info_req msg(m_session_id);
    SetHeader(msg);
    m_channel->Write(&msg, sizeof(msg), 1);
  }

  //-------------------------------------------------------------------------//
  // "NewOrder":                                                             //
  //-------------------------------------------------------------------------//
  // NB: The price is "int", ie it has already been converted into the OUCH in-
  // ternal format:
  //
  template<class Processor>
  typename OUCH_Currenex_MsgBuilder<Processor>::ResCodeT
  OUCH_Currenex_MsgBuilder<Processor>::NewOrder
  (
    Events::OrderID       a_root_id,
    int                   a_ord_num,
    Events::SymKey const& a_symbol,
    SideT                 a_side,
    OrderType             a_type,
    TTL                   a_ttl,
    int                   a_px,
    long                  a_qty,
    long                  a_show_qty,
    long                  a_min_qty
  )
  {
    if (utxx::unlikely(!IsReady()))
      return ResCodeT::IOError;

    // Check the validity of the args. Since Mkt Orders are not allowed, Px
    // must always be finite:
    if (utxx::unlikely(a_qty <= 0 || a_show_qty < 0 || a_min_qty < 0 ||
                       a_show_qty >  a_qty          || a_min_qty > a_qty))
      return ResCodeT::InvalidParams;

    int16_t idx = IdxLookup(a_symbol);

    // XXX: Will min_qty==0 be accepted by the Exchange?

    // And "show_qty" is for explicit Iceberg orders only (and vice versa):
    if (a_show_qty < a_qty)
      a_type = OrderType::ICEBERG;
    else
      a_show_qty = ZeroVal;

    // Log the args first -- this is useful if an error occurs in the follow-
    // ing:
    if (utxx::unlikely(m_processor->m_DebugLevel >= 3))
      LOG_INFO("[%s] -> ORDER %s %s %ld @ %.5f "
               "(ord-id=%d, root-id=%lX, min=%ld, show=%ld) %s %s\n",
               m_channel->Name().c_str(), a_symbol.data(),
               new_order_req::decode_side(a_side), a_qty, double(a_px)/100000,
               a_ord_num, a_root_id, a_min_qty, a_show_qty,
               new_order_req::decode_order_type(a_type),
               new_order_req::decode_expire_type(a_ttl));

    new_order_req msg
      (a_ord_num, a_type, idx, a_side, a_qty, a_min_qty, a_px, a_show_qty,
       a_ttl);

    SetHeader(msg);
    int     bytes_sent  = m_channel->Write(&msg, sizeof(msg), 1);
    return (bytes_sent >= 0) ? ResCodeT::OK : ResCodeT::IOError;
  }

  //-------------------------------------------------------------------------//
  // "ModifyOrder":                                                          //
  //-------------------------------------------------------------------------//
  // NB: The price is "int", ie it has already been converted into the OUCH in-
  // ternal format:
  //
  template<class Processor>
  typename OUCH_Currenex_MsgBuilder<Processor>::ResCodeT
  OUCH_Currenex_MsgBuilder<Processor>::ModifyOrder
  (
    Events::OrderID       a_root_id,
    int                   a_ord_num,
    int                   a_orig_ord_num,
    Events::SymKey const& a_symbol,
    int                   a_px,
    long                  a_qty
  )
  {
    if (utxx::unlikely(!IsReady()))
      return ResCodeT::IOError;

    int16_t idx = IdxLookup(a_symbol);
    replace_or_cancel msg(a_ord_num, a_orig_ord_num, a_qty, a_px, idx);

    if (utxx::unlikely(m_processor->m_DebugLevel >= 3))
      LOG_INFO("[%s] -> MODIFY %s %ld @ %.5f "
               "(ord-id=%d, orig-id=%d, root-id=%lX\n",
               m_channel->Name().c_str(), a_symbol.data(), a_qty,
               double(a_px)/100000,
               a_ord_num, a_orig_ord_num, a_root_id);

    SetHeader(msg);
    int     bytes_sent  = m_channel->Write(&msg, sizeof(msg), 1);
    return (bytes_sent >= 0) ? ResCodeT::OK : ResCodeT::IOError;
  }

  //-------------------------------------------------------------------------//
  // "CancelOrder":                                                          //
  //-------------------------------------------------------------------------//
  template<class Processor>
  typename OUCH_Currenex_MsgBuilder<Processor>::ResCodeT
  OUCH_Currenex_MsgBuilder<Processor>::CancelOrder
  (
    Events::OrderID       a_root_id,
    int                   a_ord_num,
    int                   a_orig_ord_num,
    Events::SymKey const& a_symbol
  )
  {
    if (utxx::unlikely(!IsReady()))
      return ResCodeT::IOError;

    // XXX: Any params verification here?

    int16_t idx = IdxLookup(a_symbol);

    cancel_order msg(a_ord_num, a_orig_ord_num, idx);

    if (utxx::unlikely(m_processor->m_DebugLevel >= 3))
      LOG_INFO("[%s] -> CLX %s (ord-id=%d, orig-id=%d, root-id=%lX\n",
               m_channel->Name().c_str(), a_symbol.data(),
               a_ord_num, a_orig_ord_num, a_root_id);

    SetHeader(msg);
    int     bytes_sent  = m_channel->Write(&msg, sizeof(msg), 1);
    return (bytes_sent >= 0) ? ResCodeT::OK : ResCodeT::IOError;
  }

  //=========================================================================//
  // High-Level API:                                                         //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "OnRead":                                                               //
  //-------------------------------------------------------------------------//
  template<class Processor>
  int OUCH_Currenex_MsgBuilder<Processor>::OnRead
  (
    typename OUCH_Currenex_MsgBuilder
      <Processor>::Channel*   a_channel,
    dynamic_io_buffer const&  a_buf,
    size_t&                   msg_count
  )
  {
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

      if (msg_begin + sizeof(MsgHeader) >= buff_end)
        break;

      auto msg = reinterpret_cast<MsgHeader const*>(msg_begin);
      // Invoke the actual Builder (and Processor call-back):
      int sz = this->buildMsg(msg, buff_end);

      if (utxx::unlikely(sz <= 0))
      {
        if (utxx::likely(sz == 0))
        {
          // there are no (more) complete msgs:
          if (utxx::unlikely(m_processor->m_DebugLevel >= 2))
            SLOG(INFO) << "OUCH_Currenex_MsgBuilder::OnRead: incomplete read ("
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
  inline bool OUCH_Currenex_MsgBuilder<Processor>::OnTimeout
  (
    typename OUCH_Currenex_MsgBuilder
      <Processor>::Channel*         a_ch,
    TimeoutType                     a_type,
    utxx::time_val                  a_now
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
  bool OUCH_Currenex_MsgBuilder<Processor>::OnError
  (
    typename OUCH_Currenex_MsgBuilder
      <Processor>::Channel*         a_ch,
    IOType                          a_type,
    int                             a_ecode,
    std::string const&              a_error
  )
  {
    LOG_ERROR("OUCH_Currenex_MsgBuilder: channel[%s] %s error '%s'%s\n",
              a_ch->Name().c_str(), MAQUETTE::ToString(a_type),
              a_ch->Url().c_str(),
              (a_ecode <= 0
                ? ""
                : utxx::to_string(": ", a_error, " (", a_ecode, ')').c_str()));
    return true;
  }

  //-------------------------------------------------------------------------//
  // "OnConnected":                                                          //
  //-------------------------------------------------------------------------//
  //
  template<class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::OnConnected
    (typename OUCH_Currenex_MsgBuilder<Processor>::Channel* a_ch)
  {
    LOG_INFO("OnConnected: connected '%s' channel to %s\n",
             a_ch->Name().c_str(), a_ch->Url().c_str());
    m_processor->NextTxSeqno(1);
    utxx::timestamp::update();
    m_midnight_usec = utxx::timestamp::utc_midnight_useconds().value();

    Logon();
  }

  //-------------------------------------------------------------------------//
  // "onData":                                                               //
  //-------------------------------------------------------------------------//
  // This is just a fasade to call the right function in the processor
  template <class Processor>
  template<class T>
  inline void OUCH_Currenex_MsgBuilder<Processor>::onData(T const* a_msg)
  {
    m_processor->OnData(*a_msg);
  }

  //-------------------------------------------------------------------------//
  // "onData": LOGON ACK                                                     //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::onData(Msgs::LogonAck const* a_msg)
  {
    m_session_id = a_msg->session_id();

    // Successful authentication - request instrument information details:
    currenex::ouch_1::instr_info_req msg(m_session_id);
    SetHeader(msg);
    m_channel->Write(&msg, sizeof(msg), 1);

    // Propagate login event to the processor:
    m_channel->State(ChannelStateData::StateT::READY);

    // TODO: add delayed changing of the READY state on receipt of InstrInfo
    // message.
    m_processor->OnData(*a_msg);
  }

  //-------------------------------------------------------------------------//
  // "onData": LOGOUT                                                        //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::onData(Msgs::Logout const* a_msg)
  {
    // Propagate login event to the processor:
    m_channel->Close();

    // TODO: add delayed changing of the READY state on receipt of InstrInfo
    // message.
    m_processor->OnData(*a_msg);
  }

  //-------------------------------------------------------------------------//
  // "onData": HEARTBEAT                                                     //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::onData(Msgs::Htbeat const* a_msg)
  {
    // Reply to the server by ack'ing the heartbeat
    currenex::ouch_1::heartbeat msg(m_session_id);
    SetHeader(msg);
    if (m_channel->CliTimeoutSec())
      m_channel->NextRxTimeout
        (m_channel->LastRxTime().add_sec(m_channel->CliTimeoutSec()));
    m_channel->Write(&msg, sizeof(msg), 1);
  }

  //-------------------------------------------------------------------------//
  // "onData": InstrInfo                                                     //
  //-------------------------------------------------------------------------//
  template <class Processor>
  void OUCH_Currenex_MsgBuilder<Processor>::onData(Msgs::InstrInfo const* a_msg)
  {
    // Store the instrument information (including its settlement date)
    // mapping (bi-directional):
    //
    Events::SymKey sym;
    Events::InitFixedStr(sym, a_msg->instr_id().to_string(' '));
    auto update        = [&](MAQUETTE::SecDef& a_def, bool a_inserted)
    {
      a_def.m_SecID    = a_msg->instr_index();
      a_def.m_Maturity = a_msg->settle_date() / 1000;
      return a_def;
    };

    m_processor->UpdateSecDefMaps(sym, update);

    // TODO: Do we need to notify the processor?
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
  int OUCH_Currenex_MsgBuilder<Processor>::buildMsg
    (MsgHeader const* a_hdr, char const* a_end)
  {
    using namespace currenex::ouch_1;

    auto their_seqno = a_hdr->seqno();
    // TODO: Detect seqno gaps
    m_processor->NextRxSeqno(their_seqno);

    int n = -1;

    try
    {
      switch (a_hdr->msg_type())
      {
        //-------------------------------------------------------------------//
        // Incoming messages                                                 //
        //-------------------------------------------------------------------//
        case MT<logon        >(): AS<logon        >(a_hdr, out(n), a_end); break;
        case MT<logout       >(): AS<logout       >(a_hdr, out(n), a_end); break;
        case MT<heartbeat    >(): AS<heartbeat    >(a_hdr, out(n), a_end); break;
        case MT<instr_info   >(): AS<instr_info   >(a_hdr, out(n), a_end); break;
        case MT<new_order_ack>(): AS<new_order_ack>(a_hdr, out(n), a_end); break;
        case MT<clx_order_rej>(): AS<clx_order_rej>(a_hdr, out(n), a_end); break;
        case MT<replace_ack  >(): AS<replace_ack  >(a_hdr, out(n), a_end); break;
        case MT<canceled     >(): AS<canceled     >(a_hdr, out(n), a_end); break;
        case MT<trade        >(): AS<trade        >(a_hdr, out(n), a_end); break;
        //-------------------------------------------------------------------//
        default:                                                             //
        //-------------------------------------------------------------------//
          m_channel->IncRxInvalidMsgs();
          LOG_WARNING("buildMsg: invalid msg: %s\n",
                      utxx::to_bin_string
                        (reinterpret_cast<const char*>(a_hdr),
                         sizeof(a_hdr), false, true, true).c_str());
          // This would return -1
          break;
      }
    }
    catch (std::exception const& e)
    {
      LOG_ERROR("buildMsg: %s\n", e.what());
      assert(n > -1);
    }

    return n;
  }

} // namespace OUCH

#endif // MAQUETTE_CONNECTORS_OUCH_CURRENEX_MSGBUILDER_HPP
