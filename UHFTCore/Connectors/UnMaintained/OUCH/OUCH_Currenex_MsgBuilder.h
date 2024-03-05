// vim:ts=2:et:sw=2
//===========================================================================//
//                       "OUCH_Currenex_MsgBuilder.h":                       //
//   A Common Message Parser Implementation for OUCH Order Entry Protocol    //
//===========================================================================//
#ifndef MAQUETTE_CONNECTORS_OUCH_CURRENEX_MSGBUILDER_H
#define MAQUETTE_CONNECTORS_OUCH_CURRENEX_MSGBUILDER_H

#include "Common/DateTime.h"
#include "Connectors/Configs_Currenex.h"
#include "Connectors/MsgBuilder.hpp"
#include "Connectors/currenex-ouch-1.0.generated.hpp"
#include <utxx/meta.hpp>
#include <unordered_map>

namespace CNX
{
  using namespace MAQUETTE;
  using utxx::dynamic_io_buffer;
  using utxx::time_val;

  using MsgHeader = currenex::ouch_1::header;
  using OrderType = currenex::ouch_1::new_order_req::order_type_t;
  using SideT     = currenex::ouch_1::new_order_req::side_t;
  using TTL       = currenex::ouch_1::new_order_req::subscr_type_t;
  static const long ZeroVal = currenex::ouch_1::new_order_req::NULL_VALUE;

  namespace Msgs
  {
    using LogonAck   = currenex::ouch_1::logon        ;
    using Logout     = currenex::ouch_1::logout       ;
    using Htbeat     = currenex::ouch_1::heartbeat    ;
    using InstrInfo  = currenex::ouch_1::instr_info   ;
    using OrderAck   = currenex::ouch_1::new_order_ack;
    using CxlRej     = currenex::ouch_1::clx_order_rej;
    using ReplAck    = currenex::ouch_1::replace_ack  ;
    using Cxld       = currenex::ouch_1::canceled     ;
    using Trade      = currenex::ouch_1::trade        ;
  };

  //=========================================================================//
  // "OUCH_Currenex_MsgBuilder": MsgBuilder for HF API protocol:             //
  //=========================================================================//
  template<class Processor>
  struct OUCH_Currenex_MsgBuilder final
    : public MAQUETTE::MsgBuilder_Base
             <OUCH_Currenex_MsgBuilder<Processor>, CNX::ChannelStateData>
  {
    using Self    = OUCH_Currenex_MsgBuilder<Processor>;
    using Base    = MAQUETTE::MsgBuilder_Base
                   <OUCH_Currenex_MsgBuilder<Processor>, CNX::ChannelStateData>;
    using Channel = typename Base::Channel;

    friend Base;      // So that Base::Create can call this class's constructor
    friend Processor;

    //-----------------------------------------------------------------------//
    // Ctor:                                                                 //
    //-----------------------------------------------------------------------//
    /// Ctor using a "config_tree" called from the Create() factory method.
    //
    OUCH_Currenex_MsgBuilder(EPollReactor& a_reactor, Processor* a_processor);

  private:
    //-----------------------------------------------------------------------//
    // Internals:                                                            //
    //-----------------------------------------------------------------------//
    Processor*        m_processor;      // Not Owned!!!
    Channel*          m_channel;        ///< First channel in the Base's
                                        ///< list of channels.

    int32_t           m_session_id;     ///< Assigned on logon.
    time_t            m_midnight_usec;  ///< Microsec since epoch till midnight

    static const char SOH = '\1';
    static const char ETX = '\3';

    //-----------------------------------------------------------------------//
    // "AS":                                                                 //
    //-----------------------------------------------------------------------//
    // Helper function to make sure there's enough data, and invoke appropriate
    // onData callback:
    template <typename T>
    void AS(const MsgHeader* a_hdr, int& a_sz, const char* a_end)
    {
      const T*    msg = reinterpret_cast<const T*>   (a_hdr);
      const char* end = reinterpret_cast<const char*>(a_hdr) + sizeof(T);

      if (utxx::unlikely(end > a_end))
      {
        a_sz = 0;
        return;
      }

      // We do this specifically before calling onData, so that the size
      // is known even if onData may throw an exception:
      a_sz = sizeof(T);

      onData(msg);
    }

    //=======================================================================//
    // MsgBuilder: Incoming messages from exchange                           //
    //=======================================================================//
    template <class T>
    void onData(T const* a_msg);
    void onData(Msgs::LogonAck  const* a_msg);
    void onData(Msgs::Logout    const* a_msg);
    void onData(Msgs::Htbeat    const* a_msg);
    void onData(Msgs::InstrInfo const* a_msg);

    // No need for any custom post-initialization
    void DoInit(utxx::config_tree const& a_cfg) override;

    /// Actual msg building and Processor (call-back) invocation occurs here:
    int  buildMsg(MsgHeader  const* a_hdr, const char* a_end);

    template<class Msg> void SetHeader(Msg& a_msg);

    bool IsReady() const { return m_channel->IsReady(); }

  public:
    //-----------------------------------------------------------------------//
    // "Create":                                                             //
    //-----------------------------------------------------------------------//
    /// Factory method to create an instance of the message builder
    /// @param a_reactor   epoll reactor
    /// @param a_processor message processor
    /// @param a_cfg       configuration settings (see MsgBuilder.{h,xml})
    /// @param a_validate  true - perform config validation
    //
    static OUCH_Currenex_MsgBuilder* Create
    (
      EPollReactor&            a_reactor,
      Processor*               a_processor,
      utxx::config_tree const& a_cfg,
      bool                     a_validate = true
    );

    //-----------------------------------------------------------------------//
    // High-Level API:                                                       //
    //-----------------------------------------------------------------------//
    int32_t SessionID()  const { return m_session_id; }
    size_t  InstrCount() const { return m_processor->GetSymbolsMap().size(); }

    template <class Msg>
    time_val MsgTime(const Msg& a_msg)
    {
      return time_val(utxx::usecs(a_msg.timestamp()*1000 + m_midnight_usec));
    }

    Events::SymKey const& SymLookup(int16_t               a_instr_idx) const;
    int16_t               IdxLookup(Events::SymKey const& a_symbol)    const;

    /// Send logon request to a given channel
    void    Logon();
    /// Send logout request on both order and response channels
    void    Logout();
    /// Send heartbeat through the given channel
    void    Heartbeat();
    /// Request instrument info from CNX (this is called immed. after logon)
    void    InstrInfoReq();

    /// Return type for sending Application-Level requests:
    enum class ResCodeT
    {
      InvalidParams = INT_MIN,
      IOError       = -1,
      Ignore        = 0,
      OK            = 1
    };

    //-----------------------------------------------------------------------//
    /// Send new order                                                       //
    //-----------------------------------------------------------------------//
    // NB: The price is "int", ie it must be converted by the caller into the
    // OUCH internal format:
    //
    ResCodeT NewOrder
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
    );

    //-----------------------------------------------------------------------//
    /// Modify existing order                                                //
    //-----------------------------------------------------------------------//
    // NB: The price is "int", ie it must be converted by the caller into the
    // OUCH internal format:
    //
    ResCodeT ModifyOrder
    (
      Events::OrderID       a_root_id,
      int                   a_ord_num,
      int                   a_orig_ord_num,
      Events::SymKey const& a_symbol,
      int                   a_px,
      long                  a_qty
    );

    //-----------------------------------------------------------------------//
    /// Cancel existing order                                                //
    //-----------------------------------------------------------------------//
    ResCodeT CancelOrder
    (
      Events::OrderID       a_root_id,
      int                   a_ord_num,
      int                   a_orig_ord_num,
      Events::SymKey const& a_symbol
    );

    //-----------------------------------------------------------------------//
    // OnRead:                                                               //
    //-----------------------------------------------------------------------//
    /// Invoked by a Channel when there is data to be read.
    /// @return The number of bytes in complete msgs read:
    int OnRead(Channel* a_ch, dynamic_io_buffer const& a_buf, size_t& a_count);

    //-----------------------------------------------------------------------//
    // OnTimeout:                                                            //
    //-----------------------------------------------------------------------//
    /// Invoked by a Channel when there is either server/client timeout.
    /// This is the place to implement heartbeats.
    /// @return For SERVER_TIMEOUT, return true if the event was handled and is
    ///         recoverable; otherwise return of false will result in the
    ///         exception being thrown. For CLIENT_TIMEOUT the return is ignored
    bool OnTimeout(Channel* a_ch, TimeoutType a_tp, time_val a_now);

    //-----------------------------------------------------------------------//
    // OnError:                                                              //
    //-----------------------------------------------------------------------//
    /// Invoked by a Channel when there is an I/O error
    /// @return true if the error was handled by the callee
    bool OnError
         (Channel* a_ch, IOType a_tp, int a_ec, std::string const& a_err);

    //-----------------------------------------------------------------------//
    // OnConnected:                                                          //
    //-----------------------------------------------------------------------//
    /// Invoked by a Channel on successful connect
    void OnConnected(Channel* a_ch);
  };

}
#endif // MAQUETTE_CONNECTORS_OUCH_CURRENEX_MSGBUILDER_H
