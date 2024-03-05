// vim:ts=2:et
//===========================================================================//
//                "Connectors/TWIME/EConnector_TWIME_FORTS.h":               //
//     Order Management Embedded Connector for the FORTS TWIME Protocol      //
//===========================================================================//
#pragma once

#include "Connectors/TCP_Connector.h"
#include "Connectors/EConnector_OrdMgmt.h"
#include "Connectors/TWIME/Configs.h"
#include "Protocols/TWIME/Msgs.h"

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_TWIME_FORTS" Class:                                         //
  //=========================================================================//
  class EConnector_TWIME_FORTS final:
    public EConnector_OrdMgmt,
    public TCP_Connector<EConnector_TWIME_FORTS>
  {
  private:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    using  TCPC = TCP_Connector<EConnector_TWIME_FORTS>;
    friend class  TCP_Connector<EConnector_TWIME_FORTS>;
    friend class  EConnector_OrdMgmt;

  public:
    //-----------------------------------------------------------------------//
    // Native Qty Type and Other Defs:                                       //
    //-----------------------------------------------------------------------//
    // In TWIME FORTS, all Qtys are Integral Contracts:
    constexpr static QtyTypeT QT   = QtyTypeT::Contracts;
    using                     QR   = long;
    using                     QtyN = Qty<QT,QR>;

    // TWIME ProtoEngine is this class itself.
    // BatchSend IS supported by TWIME (as multiple msgs can be concatenadted
    // in a buffer and sent together):
    constexpr static bool     HasBatchSend        = true;

    // Obviously, TWIME has AtomicModify:
    constexpr static bool     HasAtomicModify     = true;

    // And it has native MassCancel:
    constexpr static bool     HasNativeMassCancel = true;

    // "Cancel" is NOT free (ie it counts as a throttlable req):
    constexpr static bool     HasFreeCancel       = false;

    //=======================================================================//
    // TWIME-Specific Data Flds:                                             //
    //=======================================================================//
    // Copy of the Session Config -- needed for RecoveryMode mgmt:
    TWIMESessionConfig const        m_sessConf;

    // Client Credentials:
    Credential const                m_clientID;
    Credential const                m_account;

    // The following flds are for compatibility with the SessMgr static iface
    // only:
    SeqNum*                         m_txSN;
    SeqNum*                         m_rxSN;

    // Managing HeartBeats and Re-Transmissions:
    bool       const                m_resetSeqNums;
    uint32_t   const                m_heartBeatDecl;    // Negotiated with Srv
    uint32_t   const                m_heartBeatMSec;    // Real one (for TCPC)
    mutable TWIME::TerminationCodeE m_termCode;

    constexpr  static int           MaxReSends = 1000;  // In RecoveryMode
    mutable    bool                 m_recoveryMode;
    mutable    int                  m_reSendCountCurr;
    mutable    int                  m_reSendCountTotal;

    // Buffer where Out-Bound Msgs are formed:
    mutable char                    m_outBuff[65536];   // Output
    mutable int                     m_outBuffLen;       // <= 65536

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    EConnector_TWIME_FORTS
    (
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      std::vector<std::string>    const* a_only_symbols,
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params
    );

    // Default Ctor is deleted:
    EConnector_TWIME_FORTS() = delete;

    // Dtor:
    ~EConnector_TWIME_FORTS()  noexcept override;

    //=======================================================================//
    // Start / Stop Mgmt:                                                    //
    //=======================================================================//
    // Implementing the pure virtual methods of "EConnector_OrdMgmt":
    void Start()           override;
    void Stop()            override;
    bool IsActive() const  override;

  private:
    // "Start" and "Stop" internal implementations:
    // Immediate Stop:
    template<bool IsFull>
    void StopNow(char const* a_where,   utxx::time_val a_ts_recv);

    // Graceful Stop:
    template<bool IsFull>
    typename TCPC::StopResT Stop(TWIME::TerminationCodeE a_tcode);

    //-----------------------------------------------------------------------//
    // Call-Backs used by "TCP_Connector":                                   //
    //-----------------------------------------------------------------------//
    // NB: "InitSession" does nothing in TWIME, so immediately signals complet-
    // ion:
    void InitSession()   { TCPC::SessionInitCompleted(); }
    void InitLogOn();
    void InitGracefulStop();
    void GracefulStopInProgress();

    void LogOnCompleted(utxx::time_val a_ts_recv);

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //=======================================================================//
    // Sending Session-Level TWIME Msgs:                                     //
    //=======================================================================//
    // NB: For Session-Level Msgs, "FlushOrders" is automatically invoked:
    //
    void SendEstablish();
    void SendTerminate();
    void SendSequence ();
    void SendHeartBeat()  { SendSequence(); }  // For "TCP_Connector" iface
    void SendReTransmitReq();

    // Sender Utils:
    template<typename T>
    T* MkMsgHdr() const;

    //=======================================================================//
    // "ReadHandler":                                                        //
    //=======================================================================//
    int ReadHandler
    (
      int             a_fd,
      char const*     a_buff,       // Points to dynamic buffer of Reactor
      int             a_size,
      utxx::time_val  a_ts_recv
    );

    // Call-Back invoked by "TCP_Connector" if read activity ceased:
    void ServerInactivityTimeOut(utxx::time_val a_ts_recv);

    // Checking SeqNums and managing Re-Sends of Appl-Level Msgs.
    // Returns "true" to continue, "false" to stop immediately:
    bool ManageApplSeqNums
    (
      uint16_t        a_tid,
      utxx::time_val  a_ts_recv
    );

    //=======================================================================//
    // Processing of Received Msgs:                                          //
    //=======================================================================//
    // XXX: Searching for a better place: The following Mask32 is required by
    // the "Process" methods below:
    //
    constexpr static unsigned long Mask32 = 0xFFFFFFFFUL;

    // Params of all "Process" methods: (Msg, RecvTime):
    // Returns:  "true" to continue reading, "false" to stop immediately:
    //
    // Session-Level Msgs:
    bool Process(TWIME::EstablishmentAck         const&, utxx::time_val);
    bool Process(TWIME::EstablishmentReject      const&, utxx::time_val);
    bool Process(TWIME::Terminate                const&, utxx::time_val);
    bool Process(TWIME::ReTransmission           const&, utxx::time_val);
    bool Process(TWIME::Sequence                 const&, utxx::time_val);
    bool Process(TWIME::FloodReject              const&, utxx::time_val);
    bool Process(TWIME::SessionReject            const&, utxx::time_val);

    // Application-Level Responses:
    bool Process(TWIME::NewOrderSingleResponse   const&, utxx::time_val);
    bool Process(TWIME::NewOrderMultiLegResponse const&, utxx::time_val);
    bool Process(TWIME::NewOrderReject           const&, utxx::time_val);
    bool Process(TWIME::OrderCancelResponse      const&, utxx::time_val);
    bool Process(TWIME::OrderCancelReject        const&, utxx::time_val);
    bool Process(TWIME::OrderReplaceResponse     const&, utxx::time_val);
    bool Process(TWIME::OrderReplaceReject       const&, utxx::time_val);
    bool Process(TWIME::OrderMassCancelResponse  const&, utxx::time_val);
    bool Process(TWIME::ExecutionSingleReport    const&, utxx::time_val);
    bool Process(TWIME::ExecutionMultiLegReport  const&, utxx::time_val);
    bool Process(TWIME::EmptyBook                const&, utxx::time_val);
    bool Process(TWIME::SystemEvent              const&, utxx::time_val);

  public:
    //=======================================================================//
    // Impls of Virtual "Send" Methods from "EConnector_OrdMgmt":            //
    //=======================================================================//
    // Re-using generic impls from the same base class; "SessMgr" is THIS cls:
    //=======================================================================//
    // "NewOrder":                                                           //
    //=======================================================================//
    AOS const* NewOrder
    (
      // The originating Stratergy:
      Strategy*           a_strategy,

      // Main order params:
      SecDefD const&      a_instr,
      FIX::OrderTypeT     a_ord_type,
      bool                a_is_buy,
      PriceT              a_px,
      bool                a_is_aggr,      // Aggressive Order?
      QtyAny              a_qty,          // Full Qty

      // Temporal params of the triggering MktData event:
      utxx::time_val      a_ts_md_exch    = utxx::time_val(),
      utxx::time_val      a_ts_md_conn    = utxx::time_val(),
      utxx::time_val      a_ts_md_strat   = utxx::time_val(),

      // Optional order params:
      // NB: TimeInForceT::UNDEFINED must automatically resolve to a Connector-
      // specific default:
      bool                a_batch_send    = false,
      FIX::TimeInForceT   a_time_in_force = FIX::TimeInForceT::UNDEFINED,
      int                 a_expire_date   = 0,      // Or YYYYMMDD
      QtyAny              a_qty_show      = QtyAny::PosInf(),
      QtyAny              a_qty_min       = QtyAny::Zero  (),
      // The following params are irrelevant -- Pegged Orders are NOT supported
      // in TWIME:
      bool                a_peg_side      = true,
      double              a_peg_offset    = NaN<double>
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "NewOrderImpl" -- Call-Back from "EConnector_OrdMgmt":                //
    //-----------------------------------------------------------------------//
    void NewOrderImpl
    (
      EConnector_TWIME_FORTS* a_dummy,
      Req12*                  a_new_req,      // Non-NULL
      bool                    a_batch_send    // To send multiple msgs at once
    );

  public:
    //=======================================================================//
    // "CancelOrder":                                                        //
    //=======================================================================//
    bool CancelOrder
    (
      AOS const*              a_aos,          // May be NULL...
      utxx::time_val          a_ts_md_exch  = utxx::time_val(),
      utxx::time_val          a_ts_md_conn  = utxx::time_val(),
      utxx::time_val          a_ts_md_strat = utxx::time_val(),
      bool                    a_batch_send  = false
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "CancelOrderImpl" -- Call-Back from "EConnector_OrdMgmt":             //
    //-----------------------------------------------------------------------//
    void CancelOrderImpl
    (
      EConnector_TWIME_FORTS* a_dummy,
      Req12*                  a_clx_req,      // Non-NULL
      Req12 const*            a_orig_req,     // Non-NULL
      bool                    a_batch_send    // To send multiple msgs at once
    );

  public:
    //=======================================================================//
    // "ModifyOrder":                                                        //
    //=======================================================================//
    bool ModifyOrder
    (
      AOS const*              a_aos,          // Non-NULL
      PriceT                  a_new_px,
      bool                    a_is_aggr,      // Becoming Aggr?
      QtyAny                  a_new_qty,
      utxx::time_val          a_ts_md_exch   = utxx::time_val(),
      utxx::time_val          a_ts_md_conn   = utxx::time_val(),
      utxx::time_val          a_ts_md_strat  = utxx::time_val(),
      bool                    a_batch_send   = false,
      QtyAny                  a_new_qty_show = QtyAny::PosInf(),
      QtyAny                  a_new_qty_min  = QtyAny::Zero  ()
    )
    override;

  private:
    //-----------------------------------------------------------------------//
    // "ModifyOrderImpl" -- Call-Back from "EConnector_OrdMgmt":             //
    //-----------------------------------------------------------------------//
    void  ModifyOrderImpl
    (
      EConnector_TWIME_FORTS* a_dummy,
      Req12*                  a_mod_req0,     // NULL in TWIME
      Req12*                  a_mod_req1,     // Non-NULL
      Req12 const*            a_orig_req,     // Non-NULL
      bool                    a_batch_send    // To send multiple msgs at once
    );

  public:
    //=======================================================================//
    // "CancelAllOrders": Only the externally-visible method:                //
    //=======================================================================//
    void CancelAllOrders
    (
      unsigned long  a_strat_hash48 = 0,                     // All  Strats
      SecDefD const* a_instr        = nullptr,               // All  Instrs
      FIX::SideT     a_side         = FIX::SideT::UNDEFINED, // Both Sides
      char const*    a_segm_id      = nullptr                // All  Segms
    )
    override;

    //=======================================================================//
    // "FlushOrders*":                                                       //
    //=======================================================================//
    // Externally-visible method:
    utxx::time_val FlushOrders() override;

  private:
    //-----------------------------------------------------------------------//
    // "FlushOrdersImpl":                                                    //
    //-----------------------------------------------------------------------//
    // "EConnector_OrdMgmt" call-back:
    //
    utxx::time_val FlushOrdersImpl(EConnector_TWIME_FORTS* a_dummy) const;

    //=======================================================================//
    // "LogMsg":                                                             //
    //=======================================================================//
    // XXX: Provide a full body here, to avoid the need for a separate HPP file:
    //
    template<bool IsSend = true, typename T>
    void LogMsg(T const& a_tmsg)
    {
      if (utxx::unlikely(m_protoLogger != nullptr))
      {
        // FIXME: The following is HUGELY INEFFICIENT:
        std::stringstream ssm;
        ssm << a_tmsg;
        m_protoLogger->info("{} {}", IsSend ? "-->" : "<==", ssm.str());
      }
    }
  };
}
// End namespace MAQUETTE
