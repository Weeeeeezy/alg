// vim:ts=2 et sw=2
//===========================================================================//
//                       "XConnector_OUCH_Currenex.h":                       //
//                  Currenex ECN OUCH order management protocol              //
//===========================================================================//
#ifndef MAQUETTE_CONNECTORS_XCONNECTOR_KOSPI200_HF_H
#define MAQUETTE_CONNECTORS_XCONNECTOR_KOSPI200_HF_H

#include "Common/Threading.h"
#include "Connectors/Configs_Conns.h"
#include "Connectors/XConnector_OrderMgmt.h"
#include "Infrastructure/MiscUtils.h"
#include "Connectors/AOS.h"
#include "Connectors/OUCH_Currenex_MsgBuilder.hpp"
#include "Infrastructure/Log_File_Msg.h"
#include <utxx/config_tree.hpp>
#include <eixx/alloc_std.hpp>
#include <eixx/eterm.hpp>
#include <utxx/nchar.hpp>
#include <utxx/multi_file_async_logger.hpp>
#include <atomic>
#include <thread>

namespace CNX
{
  using namespace MAQUETTE;

  //=========================================================================//
  // "XConnector_OUCH_Currenex" Class:                                       //
  //=========================================================================//
  struct XConnector_OUCH_Currenex
    : public XConnector_OrderMgmt
    , public virtual Alias<XConnector_OUCH_Currenex>
  {
    // NB: The "Processor" for "Kospi200_HF_MsgBuilder" is the "XConnector_
    // Kospi200_HF" itself:
    using Base    = XConnector_OrderMgmt;
    using MsgBldr = OUCH_Currenex_MsgBuilder<XConnector_OUCH_Currenex>;
    using Channel = typename MsgBldr::Channel;

    friend class OUCH_Currenex_MsgBuilder<XConnector_OUCH_Currenex>;
  protected:
    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Dtor:                                               //
    //-----------------------------------------------------------------------//
    /// Constructs Kospi200 connector
    /// @param a_conn_name    the name of the connector
    /// @param a_cfg          the configuration of the connector
    //
    XConnector_OUCH_Currenex
    (
      std::string const&       a_conn_name,
      utxx::config_tree const& a_cfg
    );

  private:
    //-----------------------------------------------------------------------//
    // Internals:                                                            //
    //-----------------------------------------------------------------------//

    // Admin Reqs are internal,  so they do not need to reside in
    // ShMem, but the Queue must be a single one (otherwise we would need a
    // Select on it,  which may  be more difficult to implement):
    //
    std::unique_ptr<fixed_message_queue>          m_req_queue;

    utxx::config_tree                             m_configuration;
    std::atomic<bool>                             m_stop;
    std::unique_ptr<MsgBldr>                      m_msg_bldr_ptr;
    MsgBldr*                                      m_msg_bldr;

    // Multi-Threading:
    std::unique_ptr<std::thread>                  m_c3_thread;
    std::unique_ptr<std::thread>                  m_start_stop_thread;
    std::mutex                                    m_starter_lock;

    /// Indicates the type of recorder to use for logging raw market data
    std::unique_ptr<Log_Base>                     m_raw_msg_log_file;

    size_t                                        m_unhandled_msgs;

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    Channel*  channel() { return m_msg_bldr->m_channel; }

    //-----------------------------------------------------------------------//
    // Msg Sending:                                                          //
    //-----------------------------------------------------------------------//
    void      DiscardReqQ();

    // TODO: implement
    bool      SendAdminMsg
    (
      AdminReq const& a_msg,
      time_val      & a_send_time,
      std::string   & a_error
    )
    { return false; }

    // NB: "a_msg" is a non-const ref: it gets modified by Send (OrderID is
    // installed):
    void      SendReqMsg
    (
       AOSReq12   & a_msg,
       std::string& a_error
    );

    void OrderCancelReq
    (
      AOSReq12   * a_cancel_req,
      std::string& a_error
    );

  public:
    // FIXME: Is this value good as the upper bound for qty?
    static constexpr const long MAX_ORDER_QTY = 100 * 1000 * 1000;

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
    static XConnector* Create
    (
      std::string       const& a_name,
      utxx::config_tree const& a_cfg
    );

    virtual ~XConnector_OUCH_Currenex() override;

    //-----------------------------------------------------------------------//
    // Implementations of "XConnector" interface methods:                    //
    //-----------------------------------------------------------------------//
    // Dynamic behaviour:
    void DoStart()                       override;
    void DoStop()                        override;
    bool DoBeforeRun()                   override;
    void DoRestartRun()                  override;
    void DoWaitForCompletion()           override;

    // Statistics:
    long TotalBytesUp()            const override;
    long TotalBytesDown()          const override;

    //-----------------------------------------------------------------------//
    // "Subscribe":                                                          //
    //-----------------------------------------------------------------------//
    // Further overriding of "XConnector_OrderMgmt::Subscribe" and "XConnector_
    // MktData::Subscribe":
    //
    bool Subscribe
    (
      CliInfo     const& a_client,
      std::string const& a_symbol,
      OrderID            a_req_id
    )
    override;

    //-----------------------------------------------------------------------//
    // "Unsubscribe*":                                                       //
    //-----------------------------------------------------------------------//
    // Final overrider of "Unsubscribe*" methods of 2 parent classes:
    bool Unsubscribe(CliInfo& a_client, std::string const& a_symbol)
    override;

    void UnsubscribeAll
    (
      CliInfo&                 a_client,
      std::set<Events::SecID>* a_actually_unsubscribed  // May be NULL
    )
    override;

    //-----------------------------------------------------------------------//
    // Implementations of "XConnector_OrderMgmt" interface methods:          //
    //-----------------------------------------------------------------------//
    void OnClientMsg() override;

  private:
    //-----------------------------------------------------------------------//
    // Internal Functions:                                                   //
    //-----------------------------------------------------------------------//
    void MkMsgBldr();

    //=======================================================================//
    // MsgBuilder related callbacks:                                         //
    //=======================================================================//
    // TODO: implement
    bool OnError(Channel* a_ch, IOType, int a_ecode, std::string const& a_error)
      { return false; }

    //=======================================================================//
    // MsgBuilder: Incoming messages from exchange                           //
    //=======================================================================//
    void OnData(Msgs::LogonAck const&);
    void OnData(Msgs::Logout   const&);
    void OnData(Msgs::OrderAck const&);
    void OnData(Msgs::ReplAck  const&);
    void OnData(Msgs::CxlRej   const&);
    void OnData(Msgs::Cxld     const&);
    void OnData(Msgs::Trade    const&);

    //-----------------------------------------------------------------------//
    // Catch-All Call-Back:                                                  //
    //-----------------------------------------------------------------------//
    // For all msg types which we can ignore.
    //
    template<typename T>
    void OnData(Channel* a_ch, T const& a_msg)
    {
      if (utxx::unlikely(m_DebugLevel >= 2))
        SLOG(INFO) << ClassName() << "::OnData: got unhandled message "
                   << utxx::type_to_string<T>()        << ": "
                   << utxx::to_bin_string(&a_msg, 128) << "... " << std::endl;
      m_unhandled_msgs++;
    }
  };
}
#endif  // MAQUETTE_CONNECTORS_XCONNECTOR_KOSPI200_HF_H
