// vim:ts=2:et
//===========================================================================//
//                          "Protocols/TWIME/Msgs.h":                        //
//===========================================================================//
#pragma  once

#include "Basis/BaseTypes.hpp"
#include "Basis/PxsQtys.h"
#include <utxx/config.h>
#include <utxx/compiler_hints.hpp>
#include <utxx/enumv.hpp>
#include <utxx/time_val.hpp>
#include <cstdint>
#include <climits>
#include <unordered_map>

namespace MAQUETTE
{
namespace TWIME
{
  //=========================================================================//
  // Base Types:                                                             //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Empty Values of Integers:                                               //
  //-------------------------------------------------------------------------//
  constexpr int8_t   EmptyInt8   = 127;
  constexpr int16_t  EmptyInt16  = 32767;
  constexpr int32_t  EmptyInt32  = 2147483647;
  constexpr int64_t  EmptyInt64  = 9223372036854775807L;

  constexpr uint8_t  EmptyUInt8  = 255;
  constexpr uint16_t EmptyUInt16 = 65535;
  constexpr uint32_t EmptyUInt32 = 4294967295;
  constexpr uint64_t EmptyUInt64 = 18446744073709551615UL;

  //-------------------------------------------------------------------------//
  // Time Stamps:                                                            //
  //-------------------------------------------------------------------------//
  // TimeDelta in milliseconds (NB: NOT microseconds!):
  using     DeltaMilliSecsT = uint32_t;
  constexpr DeltaMilliSecsT   EmptyDelta = EmptyUInt32;

  // Time in nanoseconds(!) since the UNIX Epoch (1970.0), in ths MSK (UTC+3)
  // TimeZone:
  using     TimeStampT      = uint64_t;
  constexpr TimeStampT        EmptyTimeStamp = EmptyUInt64;

  // "MkTimeStamp":
  // Constructing TWIME TimeStamps from "utxx::time_val": We artificially apply
  // a 3hr shift  to make the TimeStamp looking like MSK=UTC+3.   XXX: In fact,
  // this is logically WRONG  -- but required by FORTS:
  //
  constexpr long MSKOffsetNanoSec(3L * 3600L * 1'000'000'000L);

  inline TimeStampT MkTimeStamp(utxx::time_val a_utc)
    { return TimeStampT(a_utc.nanoseconds()) + MSKOffsetNanoSec; }

  // "TimeStampToUTC":
  // Other way round:
  inline utxx::time_val TimeStampToUTC (TimeStampT a_msk)
    { return utxx::time_val(utxx::nsecs(a_msk - MSKOffsetNanoSec)); }

  //-------------------------------------------------------------------------//
  // Enums (encoded as "uint8_t"):                                           //
  //-------------------------------------------------------------------------//
  UTXX_ENUMV(TerminationCodeE, (uint8_t, EmptyUInt8),
    (Finished,              0)
    (UnspecifiedError,      1)
    (ReRequestOutOfBounds,  2)
    (ReRequestInProgress,   3)
    (TooFastClient,         4)
    (TooSlowClient,         5)
    (MissedHeartbeat,       6)
    (InvalidMessage,        7)
    (TCPFailure,            8)
    (InvalidSequenceNumber, 9)
    (ServerShutdown,       10)
  );

  UTXX_ENUMV(EstablishmentRejectCodeE, (uint8_t, EmptyUInt8),
    (Unnegotiated,          0)
    (AlreadyEstablished,    1)
    (SessionBlocked,        2)
    (KeepAliveInterval,     3)
    (Credentials,           4)
    (Unspecified,           5)
  );

  UTXX_ENUMV(SessionRejectReasonE,     (uint8_t, EmptyUInt8),
    (ValueIsIncorrect,      5)
    (Other,                99)
    (SystemIsUnavailable, 100)
  );

  // NB: The following type is semi-compatible with  FIX::TimeInForceT; the diff
  // is the shift in encoding (TWIME::TimeInForceE = FIX::TimeInForceT - '0'):
  //
  UTXX_ENUMV(TimeInForceE,  (uint8_t, EmptyUInt8),
    (Day,        0)
    (IOC,        3)
    (FOK,        4)
  );

  UTXX_ENUMV(SideE,         (uint8_t, EmptyUInt8),
    (Buy,        1)
    (Sell,       2)
    (AllOrders, 89)
  );

  UTXX_ENUMV(ModeE,         (uint8_t, EmptyUInt8),
    (DontChangeOrderQty,          0)
    (ChangeOrderQty,              1)
    (CheckOrderQtyAndCancelOrder, 2)
    (FIXStyleReplace,             3)
  );

  UTXX_ENUMV(SecurityTypeE, (uint8_t, EmptyUInt8),
    (Future,     0)
    (Option,     1)
  );

  UTXX_ENUMV(CheckLimitE,   (uint8_t, EmptyUInt8),
    (DontCheck,  0)
    (Check,      1)
  );

  UTXX_ENUMV(TradSesEventE, (uint8_t, EmptyUInt8),
    (SessionDataReady,          101)
    (IntradayClearingFinished,  102)
    (IntradayClearingStarted,   104)
    (ClearingStarted,           105)
    (ExtensionOfLimitsFinished, 106)
    (BrokerRecalcFinished,      108)
  );

  //-------------------------------------------------------------------------//
  // Decimal (with implicit 5 decimal points in the fractional part):        //
  //-------------------------------------------------------------------------//
  struct Decimal5T
  {
    int64_t   m_mant;
    operator double() const { return double(m_mant) * 1e-5; }

    // Default Ctor:
    Decimal5T(): m_mant(0) {}

    // Non-Default Ctor: From a "Price":
    Decimal5T(PriceT a_px)
    : m_mant
      (IsFinite(a_px)
       ? int64_t(Round(double(a_px) * 1e5))
       : throw utxx::badarg_error("TWIME::Decimal5T Ctor: Non-Finite Px"))
    {}

    // Equality, Inequality:
    bool operator== (Decimal5T  a_right) const
      { return (this->m_mant == a_right.m_mant); }

    bool operator != (Decimal5T a_right) const
      { return (this->m_mant != a_right.m_mant); }

    // There currently no need for Assignment or Arithmetic Operators...
  }
  __attribute__((packed));

  // NB: There is no "EmptyDecimal" -- Decimal vals are always non-optional in
  // TWIME!

  //=========================================================================//
  // Standard Hdr of all Msgs:                                               //
  //=========================================================================//
  struct MsgHdr
  {
    uint16_t  m_BlockLength;
    uint16_t  m_TemplateID;
    uint16_t  m_SchemaID;
    uint16_t  m_Version;

    friend std::ostream& operator<< (std::ostream&, MsgHdr const&);
  }
  __attribute__((packed));

  // NB: SchemaID and Version, although transmitted in MsgHeader, have constant
  // vals for the moment:
  constexpr uint16_t SchemaID  = 19781;
  constexpr uint16_t SchemaVer = 1;

  //=========================================================================//
  // Session-Level Mgs (Bi-Directional):                                     //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Establish":                                                            //
  //-------------------------------------------------------------------------//
  struct Establish: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5000;
    TimeStampT                      m_TimeStamp;
    DeltaMilliSecsT                 m_KeepAliveInterval;
    char                            m_Credentials[20];

    friend std::ostream& operator<< (std::ostream&, Establish const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "EstablishmentAck":                                                     //
  //-------------------------------------------------------------------------//
  struct EstablishmentAck: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5001;
    TimeStampT                      m_RequestTimeStamp;
    DeltaMilliSecsT                 m_KeepAliveInterval;
    uint64_t                        m_NextSeqNo;

    friend std::ostream& operator<< (std::ostream&, EstablishmentAck const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "EstablishmentReject":                                                  //
  //-------------------------------------------------------------------------//
  struct EstablishmentReject: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5002;
    TimeStampT                      m_RequestTimeStamp;
    EstablishmentRejectCodeE::type  m_EstablishmentRejectCode;

    friend std::ostream& operator<< (std::ostream&, EstablishmentReject const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "Terminate":                                                            //
  //-------------------------------------------------------------------------//
  struct Terminate: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5003;
    TerminationCodeE::type          m_TerminationCode;

    friend std::ostream& operator<< (std::ostream&, Terminate const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "ReTransmitRequest":                                                    //
  //-------------------------------------------------------------------------//
  struct ReTransmitRequest: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5004;
    TimeStampT                      m_TimeStamp;
    uint64_t                        m_FromSeqNo;
    uint32_t                        m_Count;

    friend std::ostream& operator<< (std::ostream&, ReTransmitRequest const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "ReTransmission":                                                       //
  //-------------------------------------------------------------------------//
  struct ReTransmission: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5005;
    uint64_t                        m_NextSeqNo;
    TimeStampT                      m_RequestTimeStamp;
    uint32_t                        m_Count;

    friend std::ostream& operator<< (std::ostream&, ReTransmission const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "Sequence":                                                             //
  //-------------------------------------------------------------------------//
  struct Sequence: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5006;
    uint64_t                        m_NextSeqNo;

    friend std::ostream& operator<< (std::ostream&, Sequence const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "FloodReject ":                                                         //
  //-------------------------------------------------------------------------//
  struct FloodReject: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5007;
    uint64_t                        m_ClOrdID;
    uint32_t                        m_QueueSize;
    uint32_t                        m_PenaltyRemain;

    friend std::ostream& operator<< (std::ostream&, FloodReject const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "SessionReject ":                                                       //
  //-------------------------------------------------------------------------//
  struct SessionReject: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 5008;
    uint64_t                        m_ClOrdID;
    uint32_t                        m_RefTagID;
    SessionRejectReasonE::type      m_SessionRejectReason;

    friend std::ostream& operator<< (std::ostream&, SessionReject const&);
  }
  __attribute__((packed));

  //=========================================================================//
  // Application-Level Msgs:                                                 //
  //=========================================================================//
  //=========================================================================//
  // Application-Level Requests:                                             //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "NewOrderSingle":                                                       //
  //-------------------------------------------------------------------------//
  struct NewOrderSingle: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 6000;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_ExpireDate;
    Decimal5T                       m_Price;
    int32_t                         m_SecurityID;
    int32_t                         m_ClOrdLinkID;
    uint32_t                        m_OrderQty;
    TimeInForceE::type              m_TimeInForce;
    SideE::type                     m_Side;
    CheckLimitE::type               m_CheckLimit;
    char                            m_Account[7];

    friend std::ostream& operator<< (std::ostream&, NewOrderSingle const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "NewOrderMultiLeg":                                                     //
  //-------------------------------------------------------------------------//
  struct NewOrderMultiLeg: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 6001;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_ExpireDate;
    Decimal5T                       m_Price;
    int32_t                         m_SecurityID;
    int32_t                         m_ClOrdLinkID;
    uint32_t                        m_OrderQty;
    TimeInForceE::type              m_TimeInForce;
    SideE::type                     m_Side;
    char                            m_Account[7];

    friend std::ostream& operator<< (std::ostream&, NewOrderMultiLeg const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderCancelRequest":                                                   //
  //-------------------------------------------------------------------------//
  struct OrderCancelRequest: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 6002;
    uint64_t                        m_ClOrdID;
    int64_t                         m_OrderID;
    char                            m_Account[7];

    friend std::ostream& operator<< (std::ostream&, OrderCancelRequest const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderReplaceRequest":                                                  //
  //-------------------------------------------------------------------------//
  struct OrderReplaceRequest: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 6003;
    uint64_t                        m_ClOrdID;
    int64_t                         m_OrderID;
    Decimal5T                       m_Price;
    uint32_t                        m_OrderQty;
    int32_t                         m_ClOrdLinkID;
    ModeE::type                     m_Mode;
    CheckLimitE::type               m_CheckLimit;
    char                            m_Account[7];

    friend std::ostream& operator<< (std::ostream&, OrderReplaceRequest const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderMassCancelRequest":                                               //
  //-------------------------------------------------------------------------//
  struct OrderMassCancelRequest: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 6004;
    uint64_t                        m_ClOrdID;
    int32_t                         m_ClOrdLinkID;
    int32_t                         m_SecurityID;
    SecurityTypeE::type             m_SecurityType;
    SideE::type                     m_Side;
    char                            m_Account[7];
    char                            m_SecurityGroup[25];

    friend
    std::ostream& operator<< (std::ostream&, OrderMassCancelRequest const&);
  }
  __attribute__((packed));

  //=========================================================================//
  // Application-Level Responses:                                            //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "NewOrderSingleResponse":                                               //
  //-------------------------------------------------------------------------//
  struct NewOrderSingleResponse: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 7000;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    TimeStampT                      m_ExpireDate;
    int64_t                         m_OrderID;
    int64_t                         m_Flags;
    Decimal5T                       m_Price;
    int32_t                         m_SecurityID;
    uint32_t                        m_OrderQty;
    int32_t                         m_TradingSessionID;
    int32_t                         m_ClOrdLinkID;
    SideE::type                     m_Side;

    friend
    std::ostream& operator<< (std::ostream&, NewOrderSingleResponse const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "NewOrderMultiLegResponse":                                             //
  //-------------------------------------------------------------------------//
  struct NewOrderMultiLegResponse: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 7001;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    TimeStampT                      m_ExpireDate;
    int64_t                         m_OrderID;
    int64_t                         m_Flags;
    Decimal5T                       m_Price;
    int32_t                         m_SecurityID;
    uint32_t                        m_OrderQty;
    int32_t                         m_TradingSessionID;
    int32_t                         m_ClOrdLinkID;
    SideE::type                     m_Side;

    friend
    std::ostream& operator<< (std::ostream&, NewOrderMultiLegResponse const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "NewOrderReject":                                                       //
  //-------------------------------------------------------------------------//
  struct NewOrderReject: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 7002;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int32_t                         m_OrdRejReason;

    friend std::ostream& operator<< (std::ostream&, NewOrderReject const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderCancelResposne":                                                  //
  //-------------------------------------------------------------------------//
  struct OrderCancelResponse: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 7003;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int64_t                         m_OrderID;
    int64_t                         m_Flags;
    uint32_t                        m_OrderQty;
    int32_t                         m_TradingSessionID;
    int32_t                         m_ClOrdLinkID;

    friend std::ostream& operator<< (std::ostream&, OrderCancelResponse const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderCancelReject":                                                    //
  //-------------------------------------------------------------------------//
  struct OrderCancelReject: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 7004;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int32_t                         m_OrdRejReason;

    friend std::ostream& operator<< (std::ostream&, OrderCancelReject const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderReplaceResponse":                                                 //
  //-------------------------------------------------------------------------//
  struct OrderReplaceResponse: public MsgHdr
  {
    constexpr static uint16_t       s_id = 7005;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int64_t                         m_OrderID;
    int64_t                         m_PrevOrderID;
    int64_t                         m_Flags;
    Decimal5T                       m_Price;
    uint32_t                        m_OrderQty;
    int32_t                         m_TradingSessionID;
    int32_t                         m_ClOrdLinkID;

    friend
    std::ostream& operator<< (std::ostream&, OrderReplaceResponse const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderReplaceReject":                                                   //
  //-------------------------------------------------------------------------//
  struct OrderReplaceReject: public MsgHdr
  {
    constexpr static uint16_t       s_id = 7006;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int32_t                         m_OrdRejReason;

    friend std::ostream& operator<< (std::ostream&, OrderReplaceReject const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "OrderMassCancelResponse":                                              //
  //-------------------------------------------------------------------------//
  struct OrderMassCancelResponse: public MsgHdr
  {
    constexpr static uint16_t       s_id = 7007;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int32_t                         m_TotalAffectedOrders;
    int32_t                         m_OrdRejReason;

    friend
    std::ostream& operator<< (std::ostream&, OrderMassCancelResponse const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "ExecutionSingleReport":                                                //
  //-------------------------------------------------------------------------//
  struct ExecutionSingleReport: public MsgHdr
  {
    constexpr static uint16_t       s_id = 7008;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int64_t                         m_OrderID;
    int64_t                         m_TrdMatchID;
    int64_t                         m_Flags;
    Decimal5T                       m_LastPx;
    uint32_t                        m_LastQty;
    uint32_t                        m_OrderQty;
    int32_t                         m_TradingSessionID;
    int32_t                         m_ClOrdLinkID;
    int32_t                         m_SecurityID;
    SideE::type                     m_Side;

    friend
    std::ostream& operator<< (std::ostream&, ExecutionSingleReport const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "ExecutionMultiLegReport":                                              //
  //-------------------------------------------------------------------------//
  struct ExecutionMultiLegReport: public MsgHdr
  {
    constexpr static uint16_t       s_id = 7009;
    uint64_t                        m_ClOrdID;
    TimeStampT                      m_TimeStamp;
    int64_t                         m_OrderID;
    int64_t                         m_TrdMatchID;
    int64_t                         m_Flags;
    Decimal5T                       m_LastPx;
    Decimal5T                       m_LegPrice;
    uint32_t                        m_LastQty;
    uint32_t                        m_OrderQty;
    int32_t                         m_TradingSessionID;
    int32_t                         m_ClOrdLinkID;
    int32_t                         m_SecurityID;
    SideE::type                     m_Side;

    friend
    std::ostream& operator<< (std::ostream&, ExecutionMultiLegReport const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "EmptyBook":                                                            //
  //-------------------------------------------------------------------------//
  struct EmptyBook: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 7010;
    TimeStampT                      m_TimeStamp;
    int32_t                         m_TradingSessionID;

    friend std::ostream& operator<< (std::ostream&, EmptyBook const&);
  }
  __attribute__((packed));

  //-------------------------------------------------------------------------//
  // "SystemEvent":                                                          //
  //-------------------------------------------------------------------------//
  struct SystemEvent: public MsgHdr
  {
    constexpr static uint16_t       s_id   = 7011;
    TimeStampT                      m_TimeStamp;
    int32_t                         m_TradingSessionID;
    TradSesEventE::type             m_TradSesEvent;

    friend std::ostream& operator<< (std::ostream&, SystemEvent const&);
  }
  __attribute__((packed));

  //=========================================================================//
  // Classification of MsgTypes:                                             //
  //=========================================================================//
  inline bool IsSessionMsg (uint16_t a_tid)
    { return (5000 <= a_tid && a_tid <= 5008); }

  inline bool IsApplReqMsg (uint16_t a_tid)
    { return (6000 <= a_tid && a_tid <= 6004); }

  inline bool IsApplRespMsg(uint16_t a_tid)
    { return (7000 <= a_tid && a_tid <= 7011); }

  //=========================================================================//
  // Expected (Typed) Lengthes and Names of All Msgs (by TemplateID):        //
  //=========================================================================//
  extern uint16_t    const MsgSizes    [26];
  extern char const* const MsgTypeNames[26];

  inline uint16_t GetMsgSize(uint16_t a_tid)
  {
    return
      // Initial 9 Msgs: Session-Level:
      IsSessionMsg (a_tid)
      ? MsgSizes[a_tid - 5000]
      :
      // Then  5 Application-Level Requests:
      IsApplReqMsg (a_tid)
      ? MsgSizes[a_tid - 5991]
      :
      // Then 12 Application-Level Responses:
      IsApplRespMsg(a_tid)
      ? MsgSizes[a_tid - 6986]
      // Any other TemplateID: Invalid:
      : 0;
  }

  inline char const* GetMsgTypeName(uint16_t a_tid)
  {
    return
      // Initial 9 Msgs: Session-Level:
      IsSessionMsg (a_tid)
      ? MsgTypeNames[a_tid - 5000]
      :
      // Then  5 Application-Level Requests:
      IsApplReqMsg (a_tid)
      ? MsgTypeNames[a_tid - 5991]
      :
      // Then 12 Application-Level Responses:
      IsApplRespMsg(a_tid)
      ? MsgTypeNames[a_tid - 6986]
      // Any other TemplateID: Invalid:
      : "Invalid";
  }

  //=========================================================================//
  // Error Codes for "RejectReason"s:                                        //
  //=========================================================================//
  extern std::unordered_map<int32_t, char const*> const RejectReasons;

  inline char const* GetRejectReasonStr(int32_t a_code)
  {
    auto cit = RejectReasons.find(a_code);
    return
      utxx::unlikely(cit == RejectReasons.cend())
      ? "Unknown Reason"
      : cit->second;
  }
} // End namespace TWIME
} // End namespace MAQUETTE
