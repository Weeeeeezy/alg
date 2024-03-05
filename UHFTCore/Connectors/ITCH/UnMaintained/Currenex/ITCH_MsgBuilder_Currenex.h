// vim:ts=2:et:sw=2
//===========================================================================//
//                       "ITCH_MsgBuilder_Currenex.h":                       //
//          A Message Builder for ITCH Currenex Market Data Protocol         //
//===========================================================================//
#ifndef MAQUETTE_CONNECTORS_ITCH_MSGBUILDER_CURRENEX_H
#define MAQUETTE_CONNECTORS_ITCH_MSGBUILDER_CURRENEX_H

#include <utxx/compiler_hints.hpp>
#include <boost/iterator/iterator_concepts.hpp>
#include <Connectors/currenex-itch-2.0.generated.hpp>
#include <Connectors/Configs_Currenex.h>
#include <Connectors/MsgBuilder.hpp>
#include <Connectors/XConnector_MktData.h>
#include <StrategyAdaptor/EventTypes.h>
#include <Infrastructure/SecDefTypes.h>
#include <utxx/timestamp.hpp>
#include <unordered_map>

namespace CNX
{
  using namespace MAQUETTE;
  using namespace currenex::itch_2_0; // Auto-generated Currenex msg parser
  using utxx::time_val;
  using utxx::dynamic_io_buffer;

  namespace Msg
  {
    typedef header          Header;
    typedef logon           Logon;
    typedef logout_req      Logout;
    typedef heartbeat       Heartbeat;
    typedef instr_info      InstrInfo;
    typedef instr_info_ack  InstrInfoAck;
    typedef subscr_req      SubscrReq;
    typedef subscr_reply    SubscrReply;
    typedef price_msg       PriceMsg;
    typedef price_cancel    PriceCancel;
    typedef trade_ticker    TradeTicker;
    typedef reject          Reject;
  };

  template<class Processor>
  struct ITCH_Currenex_MsgBuilder final
    : public MAQUETTE::MsgBuilder_Base
             <ITCH_Currenex_MsgBuilder<Processor>, CNX::ChannelStateData>
  {
    using Self    = ITCH_Currenex_MsgBuilder<Processor>;
    using Base    = MAQUETTE::MsgBuilder_Base
                   <ITCH_Currenex_MsgBuilder<Processor>,CNX::ChannelStateData>;
    using Channel = typename Base::Channel;

  private:
    friend Base;      // So that Base::Create can call this class's constructor
    friend Processor;

    //-----------------------------------------------------------------------//
    // Ctor:                                                                 //
    //-----------------------------------------------------------------------//
    /// Ctor using a "config_tree" called from the Create() factory method.
    //
    ITCH_Currenex_MsgBuilder(EPollReactor& a_reactor, Processor* a_processor);

    //-----------------------------------------------------------------------//
    // Internals:                                                            //
    //-----------------------------------------------------------------------//
    Processor*        m_processor;      // Not Owned!!!
    Channel*          m_channel;        ///< Communications channel

    int32_t           m_session_id;     ///< Assigned on logon.

    static const char SOH = '\1';
    static const char ETX = '\3';

    long              m_midnight_usec;  ///< Microsec since epoch till midnight

    /// Exchange reported timestamp on the last arrived message
    /// expressed in the number of millis from midnight.
    long              m_exch_us_since_midnight;

    //-----------------------------------------------------------------------//
    // "AS":                                                                 //
    //-----------------------------------------------------------------------//
    // Helper function to make sure there's enough data, and invoke appropriate
    // onData callback:
    template <typename T, typename MsgHeader>
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

      onData(*msg);
    }

    //=======================================================================//
    // MsgBuilder: Incoming messages from exchange                           //
    //=======================================================================//
    template<class T>
    void onData(T           const& a_msg);
    void onData(logon       const& a_msg);
    void onData(logout_req  const& a_msg);
    void onData(heartbeat   const& a_msg);
    void onData(instr_info  const& a_msg);
    void onData(reject      const& a_msg);

    // No need for any custom post-initialization
    void DoInit(utxx::config_tree const& a_cfg) override;

    /// Actual msg building and Processor (call-back) invocation occurs here:
    template <class MsgHeader>
    int buildMsg(MsgHeader const* a_hdr, char const* a_end);

    /// Ack a logout
    void LogoutAck(std::string const& a_userID, Msg::Logout::logout_reason_t);

    template <class Msg>
    void SetReplyHeader(Msg& a_msg)
    {
      a_msg.seqno(m_channel->IncOseqNo());
      auto now = utxx::now_utc().microseconds() - m_midnight_usec;
      a_msg.timestamp(now);
      a_msg.ETX(ETX);     // Set the terminating symbol
    }

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
    static ITCH_Currenex_MsgBuilder* Create
    (
      EPollReactor&            a_reactor,
      Processor*               a_processor,
      utxx::config_tree const& a_cfg,
      bool                     a_validate = true
    );

    /// Dtor:
    ~ITCH_Currenex_MsgBuilder() {}

    Channel* Transport() { return m_channel; } ///< Communications channel

    //-----------------------------------------------------------------------//
    // High-Level API:                                                       //
    //-----------------------------------------------------------------------//
    /// Main event processing function to be called upon detecting activity
    /// on either m_fd or on m_timerFD:

    void Logon();     ///< Login to market data source
    void Logout();    /// Logout (note: logout is always initiated by CNX)

    /// Convert instrument index to a string representation
    Events::SymKey const& SymLookup(int16_t a_instrIdx)    const;
    int16_t               IdxLookup(Events::SymKey const&) const;

    /// Subscribe to given currency pair and optionally its trade ticker
    void Subscribe(int16_t a_instIdx, bool a_subscr, bool a_tradeTicker = true);
    void Subscribe(Events::SymKey const& a, bool a_subscr, bool a_trades = true)
      { auto i = IdxLookup(a); Subscribe(i, a_subscr, a_trades); }

    /// Get exchange-reported timestamp of the last message
    utxx::time_val ExchTimeStamp() const;

    //-----------------------------------------------------------------------//
    // OnRead:                                                               //
    //-----------------------------------------------------------------------//
    /// Invoked by a Channel when there is data to be read
    int  OnRead(Channel* a_ch, dynamic_io_buffer const& a_buf, size_t& a_count);

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
    bool OnError(Channel* a_ch, IOType a_tp, int a_ec, std::string const& a_err);

    //-----------------------------------------------------------------------//
    // OnConnected:                                                          //
    //-----------------------------------------------------------------------//
    /// Invoked by a Channel on successful connect
    void OnConnected(Channel* a_ch);
  };

} // namespace ITCH

#endif // MAQUETTE_CONNECTORS_ITCH_MSGBUILDER_CURRENEX_H
