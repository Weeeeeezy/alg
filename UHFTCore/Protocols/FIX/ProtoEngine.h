// vim:ts=2:et
//===========================================================================//
//                       "Protocols/FIX/ProtoEngine.h":                      //
//       FIX Protocol Engine: Reading, Parsing and Sending of FIX Msgs       //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/OrdMgmtTypes.hpp"
#include "Protocols/FIX/Msgs.h"
#include <boost/core/noncopyable.hpp>
#include <string>
#include <type_traits>
#include <cstring>
#include <fstream>

namespace MAQUETTE
{
  //=========================================================================//
  // Fwd Declarations:                                                       //
  //=========================================================================//
  struct SecDefD;
  class  Strategy;

namespace FIX
{
  struct SessData;

  //=========================================================================//
  // "ProtoEngine" Class:                                                    //
  //=========================================================================//
  template
  <
    DialectT::type  D,
    bool            IsServer,   // Server or Client-side?
    typename        SessMgr,
    typename        Processor
  >
  class ProtoEngine: public boost::noncopyable
  {
  public:
    //=======================================================================//
    // Types and Features:                                                   //
    //=======================================================================//
    // "Native" Qty Type, according to ProtocolFeatures of this Dialect:
    //
    constexpr static QtyTypeT QT = ProtocolFeatures<D>::s_QT;

    using QR    = typename std::conditional_t
                  <ProtocolFeatures<D>::s_hasFracQtys, double, long>;

    using QtyN  = Qty<QT,QR>;

    // Whether BatchSend is supported: For FIX, assume Yes, because multiple
    // FIX msgs can jusy be concatenadted together and sent at one:
    //
    constexpr static bool HasBatchSend    = true;

    // XXX: We assume that all FIX ProtoEngines have AtomicModify (ie a single
    // "CancelReplace"):
    constexpr static bool HasAtomicModify = true;

    // NativeMasscancel may or may not be present:
    constexpr static bool HasNativeMassCancel =
      ProtocolFeatures<D>::s_hasMassCancel;

  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Reactor, SessionMgr, Processor, Loggers:                              //
    //-----------------------------------------------------------------------//
    SessMgr*                                m_sessMgr;
    Processor*                              m_processor;

    //-----------------------------------------------------------------------//
    // NB: Typed FIX Msg Buffers are externally-visible for efficiency:      //
    //-----------------------------------------------------------------------//
    // NB: The msg types may or may not be "D"-specific (usually NOT):
    // Session- (Admin-) Level Msgs, both Client and Server Side:
    mutable LogOn                           m_LogOn;
    mutable UserResponse                    m_UserResponse;
    mutable LogOut                          m_LogOut;
    mutable ResendRequest                   m_ResendRequest;
    mutable SequenceReset                   m_SequenceReset;
    mutable TestRequest                     m_TestRequest;
    mutable HeartBeat                       m_HeartBeat;
    mutable Reject                          m_Reject;
    mutable News                            m_News;

    // Application-Level OrdMgmt Msgs, Client Side:
    mutable BusinessMessageReject           m_BusinessMessageReject;
    mutable ExecutionReport<QtyN>           m_ExecutionReport;
    mutable OrderCancelReject               m_OrderCancelReject;
    mutable TradingSessionStatus            m_TradingSessionStatus;
    mutable OrderMassCancelReport           m_OrderMassCancelReport;

    // Application-Level MktData Msgs, Client Side:
    mutable MarketDataRequestReject         m_MarketDataRequestReject;
    mutable MarketDataIncrementalRefresh<D, QtyN>
                                            m_MarketDataIncrementalRefresh;
    mutable MarketDataSnapShot<D, QtyN>     m_MarketDataSnapShot;
    mutable SecurityDefinition              m_SecurityDefinition;
    mutable SecurityStatus                  m_SecurityStatus;
    mutable SecurityList                    m_SecurityList;
    mutable QuoteRequestReject              m_QuoteRequestReject;
    mutable Quote<QtyN>                     m_Quote;
    mutable QuoteStatusReport               m_QuoteStatusReport;

    // Application-Level Msgs are normally processed on the Server Side, so they
    // would not be required here. However, NewOrderSingle  may also be received
    // on the Client Side in case of Streaming Quotes. So we still install these
    // flds:
    mutable NewOrderSingle<QtyN>            m_NewOrderSingle;
    mutable OrderCancelRequest              m_OrderCancelRequest;
    mutable OrderCancelReplaceRequest<QtyN> m_OrderCancelReplaceRequest;
    // XXX: Do we need the following?
    mutable MarketDataRequest               m_MarketDataRequest;

    //-----------------------------------------------------------------------//
    // Misc Data:                                                            //
    //-----------------------------------------------------------------------//
    int                                     m_reqIDPfxSz;     // For efficiency
    // In- and Out- Msg Buffers:
    mutable char                            m_currMsg[8192];  // Input copy
    mutable size_t                          m_currMsgLen;     // <= 8192
    mutable char                            m_outBuff[65536]; // Output
    mutable int                             m_outBuffLen;     // <= 65536

    //=======================================================================//
    // Ctors, Dtor:                                                          //
    //=======================================================================//
    // Default Ctor is deleted:
    ProtoEngine() = delete;

    // No-Default Ctor:
    ProtoEngine
    (
      SessMgr*     a_sess_mgr,
      Processor*   a_processor
    );

    // Dtor is trivial:
    ~ProtoEngine() noexcept {}

    //=======================================================================//
    // Parsers:                                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "ReadHandler": Multiplexed Call-Back for all FIX Responses:           //
    //-----------------------------------------------------------------------//
    // Call-Back invoked via "m_readH" which is registered with the Reactor.
    // Returns the number of bytes actually consumed (>= 0), or a  negative
    // value to indicate immediate stop:
    //
    int ReadHandler
    (
      int             a_fd,
      char const*     a_buff,         // NB: Buffer is managed by the Reactor
      int             a_size,         //
      utxx::time_val  a_ts_recv
    );

    int m_last_real_fd;

  private:
    //-----------------------------------------------------------------------//
    // Top-Level Msg Parser:                                                 //
    //-----------------------------------------------------------------------//
    MsgPrefix* GetMsg
    (
      MsgT            a_msg_type,
      char*           a_msg_body,
      char*           a_body_end,
      int             a_total_len,
      SessData*       a_curr_sess,
      int             a_fd,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    );

    //-----------------------------------------------------------------------//
    // Parsing of Session-Level Msgs:                                        //
    //-----------------------------------------------------------------------//
    void ParseLogOn        (char* a_msg_body, char const* a_body_end) const;
    void ParseUserResponse (char* a_msg_body, char const* a_body_end) const;
    void ParseLogOut       (char* a_msg_body, char const* a_body_end) const;
    void ParseResendRequest(char* a_msg_body, char const* a_body_end) const;
    void ParseSequenceReset(char* a_msg_body, char const* a_body_end) const;
    void ParseTestRequest  (char* a_msg_body, char const* a_body_end) const;
    void ParseHeartBeat    (char* a_msg_body, char const* a_body_end) const;
    void ParseReject       (char* a_msg_body, char const* a_body_end) const;
    void ParseNews         (char* a_msg_body, char const* a_body_end) const;

    //-----------------------------------------------------------------------//
    // Parsing of Application-Level OrdMgmt Msgs:                            //
    //-----------------------------------------------------------------------//
    void ParseBusinessMessageReject
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseExecutionReport
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseOrderCancelReject
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseTradingSessionStatus
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseOrderMassCancelReport
         (char* a_msg_body, char const*    a_body_end) const;

    //-----------------------------------------------------------------------//
    // Parsing of Application-Level MktData Msgs:                            //
    //-----------------------------------------------------------------------//
    void ParseMarketDataRequestReject
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseMarketDataIncrementalRefresh
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseMarketDataSnapShot
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseSecurityDefinition
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseSecurityStatus
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseQuoteRequestReject
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseQuote
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseQuoteStatusReport
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseSecurityList
         (char* a_msg_body, char const*    a_body_end) const;

    //-----------------------------------------------------------------------//
    // Parsing of Server-Side and "Quote-Based" Application-Level Msgs:      //
    //-----------------------------------------------------------------------//
    void ParseNewOrderSingle
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseOrderCancelRequest
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseOrderCancelReplaceRequest
         (char* a_msg_body, char const*    a_body_end) const;

    void ParseMarketDataRequest
         (char* a_msg_body, char const*    a_body_end) const;

    //-----------------------------------------------------------------------//
    // "TryRejectMsg":                                                       //
    //-----------------------------------------------------------------------//
    // Invoked if parsing fails (currently, for Server-Side only).
    // Returns "true" iff "ReadHandler" should continue after that:
    //
    bool TryRejectMsg
    (
      int             a_fd,
      SessData*       a_curr_sess,
      MsgT            a_msg_type,
      SeqNum          a_seq_num,
      SessRejReasonT  a_rej_reason,
      char const*     a_err_msg,
      utxx::time_val  a_ts_recv
    )
    const;

  public:
    // Inject recorded data for debug purposes
    void InjectData(const std::string& file) {
      std::ifstream istm(file);
      std::string line;
      while (std::getline(istm, line)) {
        auto datetime = line.substr(0, 23);
        auto data = line.substr(24, line.size() - 24) + "\01";

        int yr;
        unsigned mon, day, hr, min, sec, usec;
        // 2021-02-05 16:03:00.611
        if (sscanf(datetime.c_str(), "%i-%u-%u %u:%u:%u.%u",
            &yr, &mon, &day, &hr, &min, &sec, &usec) != 7)
          throw utxx::runtime_error("Could not parse {} as datetime", datetime);

        utxx::time_val ts(yr, mon, day, hr, min, sec, usec);
        ReadHandler(m_last_real_fd, data.c_str(), int(data.size()), ts);
      }
    }

    //=======================================================================//
    // Sending Session-Level and MktData-Related Msgs:                       //
    //=======================================================================//
    // NB: There is currently NO "Reject" sending -- we assume that the Server
    // never generates errors which may warrant a "Reject" on our side:
    //
    void SendLogOn        (SessData* a_sess) const;
    void SendAppLogOn     (SessData* a_sess) const;
    void SendLogOut       (SessData* a_sess) const;
    void SendAppLogOff    (SessData* a_sess) const;
    void SendTestRequest  (SessData* a_sess) const;

    void SendResendRequest(SessData* a_sess, SeqNum from,  SeqNum upto)  const;
    void SendGapFill      (SessData* a_sess, SeqNum from,  SeqNum upto)  const;
    void SendHeartBeat    (SessData* a_sess, char const* a_test_req_id)  const;

    // "SendMktDataRequest":
    // (*) If "IsSubscribe" is set, "a_subscr_id" must be 0 (not yet known); the
    //     method returns the new SubscrID;
    // (*) Otherwise, "a_subscr_id" must be the ID of an existing subscription;
    //     the method returns 0:
    //
    template<bool IsSubscribe>
    OrderID SendMarketDataRequest
    (
      SessData*       a_sess,
      SecDefD  const& a_instr,
      OrderID         a_subscr_id,
      int             a_mkt_depth,
      bool            a_with_obs,
      bool            a_with_trds
    )
    const;

    // "SendQuoteRequest":
    // (*) "a_quantity" can be 0 and it can either be in QtyA or QtyB, so not
    //     necessarily same as main "QT":
    //
    template<QtyTypeT QT1>
    OrderID SendQuoteRequest
    (
      SessData*       a_sess,
      SecDefD  const& a_instr,
      Qty<QT1,QR>     a_quantity
    )
    const;

    // "QuoteCancel":
    void SendQuoteCancel
    (
      SessData*       a_sess,
      const char      a_quote_id[64],
      SeqNum          a_quote_version = 0
    )
    const;

    //=======================================================================//
    // Sending of Application-Level OrdMgmt Msgs:                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "NewOrder":                                                           //
    //-----------------------------------------------------------------------//
    // Either generates a "NewOrderSingle" or propagates an exception; updates
    // "a_new_req":
    //
    void NewOrderImpl
    (
      SessData*           a_sess,           // Non-NULL
      Req12*              a_new_req,        // Non-NULL
      bool                a_batch_send      // To send multiple msgs at once
    )
    const;

    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    // Either generates an "OrderCancelRequest" or propagates an exception;
    // updates "a_clx_req":
    //
    void CancelOrderImpl
    (
      SessData*           a_sess,           // Non-NULL
      Req12*              a_clx_req,        // Non-NULL
      Req12 const*        a_orig_req,       // Non-NULL
      bool                a_batch_send      // To send multiple msgs at once
    )
    const;

    //-----------------------------------------------------------------------//
    // "ModifyOrderImpl":                                                    //
    //-----------------------------------------------------------------------//
    // Either generates an "OrderCancelReplaceRequest" or propagates an excep-
    // tion;  updates "a_mod_req1":
    //
    void ModifyOrderImpl
    (
      SessData*           a_sess,           // Non-NULL
      Req12*              a_mod_req0,       // Typically NULL in FIX
      Req12*              a_mod_req1,       // Non-NULL
      Req12 const*        a_orig_req,       // Non-NULL
      bool                a_batch_send      // To send multiple msgs at once
    )
    const;

    //-----------------------------------------------------------------------//
    // "CancelAllOrdersImpl":                                                //
    //-----------------------------------------------------------------------//
    // Generates an "OrderMassCancelRequest". Currently no need to return a TS:
    //
    void CancelAllOrdersImpl
    (
      SessData*           a_sess,           // Non-NULL
      unsigned long       a_strat_hash48,   // May be 0
      SecDefD const*      a_instr,          // May be NULL
      SideT               a_side,           // May be UNDEFINED
      char const*         a_segm_id         // SegmOrSessD; may be NULL or ""
    )
    const;

    //-----------------------------------------------------------------------//
    // "SendSecurityListRequest":                                            //
    //-----------------------------------------------------------------------//
    // Generates a "SendSecurityListRequest".
    //
    void SendSecurityListRequest(SessData* a_sess) const;   // Sess: Non-NULL

    //-----------------------------------------------------------------------//
    // "SendSecurityDefinitionRequest":                                      //
    //-----------------------------------------------------------------------//
    // Generates a "SendSecurityDefinitionRequest".
    //
    void SendSecurityDefinitionRequest
    (
      SessData*           a_sess,           // Sess: Non-NULL
      const char*         a_sec_type,
      const char*         a_exch_id
    )
    const;

    //-----------------------------------------------------------------------//
    // "FlushOrdersImpl":                                                    //
    //-----------------------------------------------------------------------//
    // Send out all msgs accumulated in "m_outBuff". To be invoked after a ser-
    // ies of "delayed" orders:
    //
    utxx::time_val FlushOrdersImpl(SessData* a_sess) const;

    //-----------------------------------------------------------------------//
    // "ResetOutBuff" (so there will be nothing to Flush):                   //
    //-----------------------------------------------------------------------//
    void ResetOutBuff() const
      { m_outBuffLen = 0; }

  private:
    //=======================================================================//
    // Internal Utils:                                                       //
    //=======================================================================//
    // "MkPreamble":
    // Outputs  a FIX Msg Preamble. Optionally, "CreateTS" can be provided  as
    // an arg;  if not, it is computed internally.
    // Returns: (curr_buff_ptr, msg_body_ptr, send_time, txSN):
    //
    template<MsgT::type Type>
    std::tuple<char*, char*, utxx::time_val, SeqNum> MkPreamble
    (
      SessData*       a_sess,
      utxx::time_val  a_ts_create = utxx::time_val()
    )
    const;

    // "MkReqID":
    // ReqID Generation:
    //
    char* MkReqID
    (
      SessData*   a_sess,
      char*       a_curr,
      char const* a_tag,
      OrderID     a_req_id
    )
    const;

    // "MkSegmSessDest":
    // The 2nd overloaded version is only used on the Server-Side:
    //
    char* MkSegmSessDest(char* a_curr, SecDefD const& a_instr)   const;
    char* MkSegmSessDest(char* a_curr, char    const* a_segm_id) const;

    // "CompleteSendLog":
    // Complete (incl Check-Sum), Send Out and Log a FIX Msg.
    // Returns send time if the msg was sent straight away, or empty value if it
    // was buffered (NB: the msg  is always buffered, rather than being sent out
    // straight away if possible, if "a_batch_send" flag is set):
    //
    template<MsgT::type Type>
    utxx::time_val CompleteSendLog
    (
      SessData*   a_sess,
      char*       a_body_begin,
      char*       a_body_end,
      bool        a_batch_send
    )
    const;

    // "LogMsg":
    template<bool IsSend>
    void LogMsg(char* a_buff, int a_len) const;
  };
} // End namespace FIX
} // End namespace MAQUETTE
