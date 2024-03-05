// vim:ts=2:et
//===========================================================================//
//                   "Connectors/FIX/FIX_ConnectorSessMgr.h":                //
//                        FIX Connector-Side Session Mgmt                    //
//===========================================================================//
// Parameterised by the FIX Protocol Dialect and the "Processor" (which performs
// Processing of Appl-Level Msgs).
// On the Client-Side, there is no more than 1 Session at a time:
#pragma once

#include "Connectors/TCP_Connector.h"
#include "Protocols/FIX/ProtoEngine.h"
#include "Protocols/FIX/SessData.hpp"
#include "Connectors/FIX/Configs.hpp"
#include <boost/container/static_vector.hpp>
#include <boost/property_tree/ptree.hpp>
#include <memory>
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // "FIX_ConnectorSessMgr" Class:                                           //
  //=========================================================================//
  // IMPORTANT:
  // (*) for FIX Protocol Engine, IsServer=false, SessMgr is THIS class (as
  //     on the Connector Side, there could be only 1 FIX Session per Mgr);
  // (*) Processor is our template param; it can call our privite "Process*"
  //     methods;
  // (*) Processor may or may not be a "FIX_ConnectorSessMgr"-derived class;
  //     at run-time, it may be absent altogether:
  //
  template<FIX::DialectT::type D, typename Processor>
  class    FIX_ConnectorSessMgr:
    public TCP_Connector<FIX_ConnectorSessMgr<D, Processor>>,
    public FIX::SessData,
    public FIX::ProtoEngine       // NB: IsServer = false:
          <D, false, FIX_ConnectorSessMgr<D, Processor>, Processor>
  {
  protected:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    // NB: All those classes/structs must be made "friend"s:
    //
    using  TCPC = TCP_Connector<FIX_ConnectorSessMgr<D, Processor>>;
    friend class  TCP_Connector<FIX_ConnectorSessMgr<D, Processor>>;

    using  ProtoEngT =
      FIX::ProtoEngine
      <D, false, FIX_ConnectorSessMgr<D, Processor>, Processor>;

    friend class
      FIX::ProtoEngine
      <D, false, FIX_ConnectorSessMgr<D, Processor>, Processor>;

    //-----------------------------------------------------------------------//
    // "QtyN" type and its template params are defined by the ProtoEngine:   //
    //-----------------------------------------------------------------------//
    using                     QR   = typename ProtoEngT::QR;
    constexpr static QtyTypeT QT   = ProtoEngT::QT;
    using                     QtyN = typename ProtoEngT::QtyN;

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    std::string      const    m_name;
    EPollReactor*    const    m_reactor;    // Ptr NOT owned
    Processor*       const    m_processor;  // ditto
    spdlog::logger*  const    m_logger;     // ditto
    int              const    m_debugLevel;
    bool             const    m_isOrdMgmt;
    bool             const    m_isMktData;

    //-----------------------------------------------------------------------//
    // Default Ctor is deleted:                                              //
    //-----------------------------------------------------------------------//
    FIX_ConnectorSessMgr() = delete;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // NB:
    // (*) "a_params" must be a sub-tree where all relevant params are located
    //     at Level 1;
    // (*) ptrs listed below will NOT be owned by "FIX_ConnectorSessMgr":
    //
    FIX_ConnectorSessMgr
    (
      std::string const&                 a_name,
      EPollReactor  *                    a_reactor,   // Non-NULL
      Processor     *                    a_processor, // May be NULL
      spdlog::logger*                    a_logger,    // Non-NULL
      boost::property_tree::ptree const& a_params,
      FIXSessionConfig const&            a_sess_conf,
      FIXAccountConfig const&            a_acct_conf,
      SeqNum*                            a_tx_sn,     // If NULL, internal SNs
      SeqNum*                            a_rx_sn      // will be used
    );

    ~FIX_ConnectorSessMgr() noexcept;

    //=======================================================================//
    // "Start", "Stop*", Properties:                                         //
    //=======================================================================//
    void Start() { TCPC::Start(); }

    // NB:  If "FullStop" is set, we still do NOT stop immediately, but such a
    // stop is not followed by a restart:
    template<bool FullStop>
    void Stop ();

    // But "StopNow" results in a synchronous Session termination (required by
    // the "TCP_Connector" interface):
    template<bool FullStop>
    void StopNow(char const* a_where, utxx::time_val a_ts_recv);

    // "IsOrdMgmt" and "IsMktData" say whether this Session has Dynamic OrdMgmt
    // and MktData capabilities, resp., ie they are statically supported by the
    // Dialect and indeed enabled in the Config of this Session. XXX: In theory,
    // FIX_SessionMgr should not be concerned at all with the application level
    // (OrdMgmt / MktData); in reality, we still need this:
    //
    bool IsOrdMgmt() const    { return m_isOrdMgmt; }
    bool IsMktData() const    { return m_isMktData; }

    //=======================================================================//
    // Session- and Protocol-specific parts of State Transitions:            //
    //=======================================================================//
    // Other call-Backs used by "TCP_Connector":
    //
    // NB: For FIX, no special Session initialisation is required. So we immed-
    // iately signal completion of the SessionInit:
    //
    void InitSession     ()  { TCPC::SessionInitCompleted(); }
    void InitLogOn       ();
    void SendHeartBeat   ()  { this->ProtoEngT::SendHeartBeat(this, nullptr); }
    void InitGracefulStop();
    void GracefulStopInProgress();

    // Top-Level (NOT Session-Level) Stats Mgmt -- also required by the
    // "TCP_Connector":
    //
    void UpdateRxStats(int a_len, utxx::time_val a_ts_recv) const
    {
      if (utxx::likely(m_processor != nullptr))
        m_processor->EConnector::UpdateRxStats  (a_len, a_ts_recv);
    }

    void UpdateTxStats(int a_len, utxx::time_val a_ts_send) const
    {
      if (utxx::likely(m_processor != nullptr))
        m_processor->EConnector::UpdateTxStats  (a_len, a_ts_send);
    }

    void ServerInactivityTimeOut(utxx::time_val a_ts_recv);

    // NB: "ReadHandler" for the "TCP_Connector" is directly inherited from the
    // FIX ProtoEng!

  private:
    //=======================================================================//
    // Processing of Session-Level Msgs:                                     //
    //=======================================================================//
    // Temporal Params are RecvTS:
    //
    void Process(FIX::LogOn            const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::UserResponse     const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::LogOut           const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::ResendRequest    const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::SequenceReset    const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::TestRequest      const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::HeartBeat        const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::Reject           const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::News             const&,
                 FIX::SessData const*, utxx::time_val);

    // XXX: The following msgs, although formally of Application Level,  are
    // processed here at the Session Level because they primarily affect the
    // Session state (or are stateless at all):
    //
    void Process(FIX::TradingSessionStatus   const&,
                 FIX::SessData const*, utxx::time_val);

    void Process(FIX::OrderMassCancelReport  const&,
                 FIX::SessData const*, utxx::time_val);

    //=======================================================================//
    // Other Internal Utils:                                                 //
    //=======================================================================//
    // When the LogOn procedure is done (after all necessary confirmations are
    // received from the server):
    void LogOnCompleted
    (
      utxx::time_val  a_ts_exch,
      utxx::time_val  a_ts_recv
    );

    // "SeqNum"s mgmt:
    void HardSeqNumError
    (
      char const*     a_err_msg,
      SeqNum          a_sn,
      FIX::MsgT       a_msg_type,
      utxx::time_val  a_ts_recv
    );

  protected:
    // "CheckRxSN":
    // Returns "true" iff the actual RxSN is the same as the expected one. If an
    // uncorrectable error is encountered, this method invokes HardSeqNumError()
    // -> StopNow() before returning "false"; "false" is also returned on recov-
    // erable errors (when this methos sends "ResendRequest" to the server):
    //
    bool CheckRxSN(FIX::MsgPrefix const& a_prefix, utxx::time_val a_ts_recv);

    //=======================================================================//
    // Static SessMgr API Required by the FIX Protocol Layer:                //
    //=======================================================================//
    // "GetFIXSession":
    FIX::SessData* GetFIXSession(int DEBUG_ONLY(a_fd));

    // "CheckFIXSession": Always returns "true" for the Client-Side:
    template<FIX::MsgT::type>
    bool CheckFIXSession
    (
      int                    a_fd,
      FIX::MsgPrefix const*  a_msg_pfx,
      FIX::SessData**        a_sess
    )
    const;

    // "IsActiveSess":
    bool IsActiveSess(FIX::SessData const* a_sess)   const;

    // "IsActive": Wrapper around "IsActiveSess" above:
    bool IsActive() const;

    // "IsInactiveSess":
    bool IsInactiveSess(FIX::SessData const* a_sess) const;

    // "TerminateSession": Wrapper around "Stop", required as a call-back from
    // the Protocol Engine. Hence the "IsGraceful" template param:
    template<bool IsGraceful>
    void TerminateSession
    (
      int                  a_fd,
      FIX::SessData const* a_sess,
      char const*          a_err_msg,
      utxx::time_val       a_ts_recv
    );

    // "RejectMsg": Does nothing on the Client-Side:
    bool RejectMsg
    (
      int                  a_fd,
      FIX::SessData const* a_sess,
      FIX::MsgT            a_msg_type,
      SeqNum               a_seq_num,
      FIX::SessRejReasonT  a_rej_reason,
      char const*          a_err_msg
    )
    const;

    // "SendImpl": Performs actual sending of FIX Msgs via Reactor:
    utxx::time_val SendImpl
      (FIX::SessData const* a_sess, char const* a_buff, int a_len) const;
  };
}
// End namespace MAQUETTE
