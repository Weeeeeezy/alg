// vim:ts=2:et:sw=2
//===========================================================================//
//                     "XConnector_ITCH_Currenex.h":                         //
//        Instantiation of the ITCH Protocol Connector for Currenex          //
//===========================================================================//
#ifndef MAQUETTE_CONNECTORS_XCONNECTOR_ITCH_CURRENEX_H
#define MAQUETTE_CONNECTORS_XCONNECTOR_ITCH_CURRENEX_H

#include "Common/Threading.h"
#include "Connectors/Configs_Currenex.h"
#include "Connectors/XConnector_MktData.h"
#include "Infrastructure/MiscUtils.h"
#include "Connectors/AOS.h"
#include "Connectors/ITCH_MsgBuilder_Currenex.h"
#include "Infrastructure/Log_File_Msg.h"
#include "Infrastructure/Log_File_Trade.h"
#include <utxx/config_tree.hpp>
#include <eixx/alloc_std.hpp>
#include <eixx/eterm.hpp>
#include <utxx/nchar.hpp>

namespace CNX
{
  using namespace MAQUETTE;

  //=========================================================================//
  // "XConnector_ITCH_Currenex" Class:                                       //
  //=========================================================================//
  struct XConnector_ITCH_Currenex
    : public virtual XConnector_MktData
    , public virtual MAQUETTE::Alias<XConnector_ITCH_Currenex>
  {
    // NB: The "Processor" for "Kospi200_HF_MsgBuilder" is the "XConnector_
    // Kospi200_HF" itself:
    using Base    = XConnector_MktData;
    using MsgBldr = ITCH_Currenex_MsgBuilder<XConnector_ITCH_Currenex>;
    using Channel = typename MsgBldr::Channel;

    friend class ITCH_Currenex_MsgBuilder<XConnector_ITCH_Currenex>;
  protected:
    //-----------------------------------------------------------------------//
    // Internals:                                                            //
    //-----------------------------------------------------------------------//

    // Admin Reqs are internal,  so they do not need to reside in
    // ShMem, but the Queue must be a single one (otherwise we would need a
    // Select on it,  which may  be more difficult to implement):
    //
    utxx::config_tree                             m_configuration;
    std::atomic<bool>                             m_stop;
    std::unique_ptr<MsgBldr>                      m_msg_bldr_ptr;
    MsgBldr*                                      m_msg_bldr;
    OrderBooksT                                   m_order_books;
    ConnSess                                      m_conn_sess;

    // Multi-Threading:
    std::unique_ptr<std::thread>                  m_c3_thread;
    std::unique_ptr<std::thread>                  m_start_stop_thread;
    std::unique_ptr<std::thread>                  m_flusher_thread;
    std::mutex                                    m_starter_lock;

    /// Indicates the type of recorder to use for logging raw market data
    std::unique_ptr<Log_Base>                     m_raw_msg_log_file;

    size_t                                        m_unhandled_msgs;

    /// Auto-subscribe to instruments at startup.
    ///
    /// When enabled, the connector will subscribe to all symbols
    /// (unless restricted in the "instruments" configuration list)
    /// upon startup
    bool                                          m_auto_subscribe;

    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Dtor:                                               //
    //-----------------------------------------------------------------------//
    /// Constructs Kospi200 connector
    /// @param a_conn_name    the name of the connector
    /// @param a_cfg          the configuration of the connector
    //
    XConnector_ITCH_Currenex
    (
      std::string const&       a_conn_name,
      EPollReactor*            a_reactor,
      utxx::config_tree const& a_cfg
    );

    Channel*  channel() { return m_msg_bldr->m_channel; }

  public:
    //-----------------------------------------------------------------------//
    /// Factory method to create an instance of this connector               //
    //-----------------------------------------------------------------------//
    /// \code
    /// connector cnx-ouch {
    ///   db-recording = true
    ///   debug-level  = 0
    ///   receiver-cpu = -1
    ///   market-depth = 10
    ///   clean-start  = false
    ///   with-timing  = true
    ///   shmem-sz-mb  = 65535
    ///   log-filename = "$path{${TMP}/logfile%Y%m%d-%T.log}"
    ///
    ///   channel-defaults {
    ///     # See MsgBuilder.{h,xml}
    ///   }
    ///   channel X1 { address = "udp://..." }
    ///   channel X2 { address = "udp://..." }
    ///   ...
    ///   channel Xn { address = "udp://..." }
    /// }
    /// \endcode
    //
    static XConnector_ITCH_Currenex* Create
    (
      std::string       const& a_name,
      utxx::config_tree const& a_cfg
    );

    virtual ~XConnector_ITCH_Currenex() override;

    //-----------------------------------------------------------------------//
    // Implementations of "XConnector" interface methods:                    //
    //-----------------------------------------------------------------------//
    // Dynamic behaviour:
    void DoStart()                       override;
    void DoStop()                        override;
    void DoRestartRun()                  override;
    void DoWaitForCompletion()           override;

    // Statistics:
    long TotalBytesUp()            const override;
    long TotalBytesDown()          const override;

    //=======================================================================//
    // Client UDS Connection Events:                                         //
    //=======================================================================//
    void OnClientMsg() {}
  private:
    //-----------------------------------------------------------------------//
    // Internal Functions:                                                   //
    //-----------------------------------------------------------------------//
    void MkMsgBldr();

    //=======================================================================//
    // MsgBuilder: Incoming messages from exchange                           //
    //=======================================================================//
    template<typename T>
    void OnData(T                const& a_msg) { m_unhandled_msgs++; }
    void OnData(Msg::Logon       const& a_msg);
    void OnData(Msg::Reject      const& a_msg);
    void OnData(Msg::Logout      const& a_msg);
    void OnData(Msg::PriceMsg    const& a_msg);
    void OnData(Msg::PriceCancel const& a_msg);
    void OnData(Msg::TradeTicker const& a_msg);
    void OnData(Msg::SubscrReply const& a_msg);

    //-----------------------------------------------------------------------//
    // MsgBuilder: Error reports                                             //
    //-----------------------------------------------------------------------//
    // TODO: implement
    bool OnError(Channel* a_ch, IOType, int a_ecode, std::string const& a_error)
      { return false; }
  };

} // namespace CNX

#endif  // MAQUETTE_CONNECTORS_XCONNECTOR_ITCH_CURRENEX_H
