// vim:ts=2:et
//===========================================================================//
//                          "Protocols/FIX/Msgs.h":                          //
//               Generic Formats for Most Common FIX Msg Types               //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/PxsQtys.h"
#include "Protocols/FIX/Features.h"
#include <utxx/config.h>
#include <utxx/time_val.hpp>
#include <utxx/error.hpp>
#include <utxx/enumv.hpp>
#include <cstring>

namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // Enum of FIX Protocol Versions:                                          //
  //=========================================================================//
  enum class ApplVerIDT: char
  {
    UNDEFINED = '\0',
    FIX27     = '0',
    FIX30     = '1',
    FIX40     = '2',
    FIX41     = '3',
    FIX42     = '4',
    FIX43     = '5',
    FIX44     = '6',
    FIX50     = '7',
    FIX50SP1  = '8',
    FIX50SP2  = '9'
  };

  //=========================================================================//
  // Emum of Msg Types (Tags):                                               //
  //=========================================================================//
  // NB: Tags here are directly those used by the FIX Protocol:
  //
  UTXX_ENUMV(MsgT, (int, 0x00),               // Encoded as "int"
    //-----------------------------------------------------------------------//
    // Session-Level Msg Types:                                              //
    //-----------------------------------------------------------------------//
    (LogOn                         , LiteralEnum("A"))
    (LogOut                        , LiteralEnum("5"))
    (ResendRequest                 , LiteralEnum("2"))
    (SequenceReset                 , LiteralEnum("4"))
    (TestRequest                   , LiteralEnum("1"))
    (HeartBeat                     , LiteralEnum("0"))
    (Reject                        , LiteralEnum("3"))
    (News                          , LiteralEnum("B"))

    //-----------------------------------------------------------------------//
    // Application Session-Level Msg Types:                                  //
    //-----------------------------------------------------------------------//
    (UserRequest                   , LiteralEnum("BE"))
    (UserResponse                  , LiteralEnum("BF"))

    //-----------------------------------------------------------------------//
    // Application-Level Msg Types:                                          //
    //-----------------------------------------------------------------------//
    (NewOrderSingle                , LiteralEnum("D"))
    (OrderCancelRequest            , LiteralEnum("F"))
    (OrderCancelReplaceRequest     , LiteralEnum("G"))
    (OrderStatusRequest            , LiteralEnum("H"))
    (OrderMassCancelRequest        , LiteralEnum("q"))
    (OrderMassStatusRequest        , LiteralEnum("AF"))
    (ExecutionReport               , LiteralEnum("8"))
    (OrderCancelReject             , LiteralEnum("9"))
    (TradingSessionStatus          , LiteralEnum("h")) // MICEX only
    (OrderMassCancelReport         , LiteralEnum("r")) // MICEX only;
                                                       // FORTS uses Exec*Report
    (BusinessMessageReject         , LiteralEnum("j"))

    //-----------------------------------------------------------------------//
    // MktData Msgs:                                                         //
    //-----------------------------------------------------------------------//
    (MarketDataRequest             , LiteralEnum("V"))
    (MarketDataRequestReject       , LiteralEnum("Y"))
    (MarketDataIncrementalRefresh  , LiteralEnum("X"))
    (MarketDataSnapShot            , LiteralEnum("W"))
    (QuoteRequest                  , LiteralEnum("R"))
    (QuoteRequestReject            , LiteralEnum("AG"))
    (Quote                         , LiteralEnum("S"))
    (QuoteCancel                   , LiteralEnum("Z"))
    (QuoteStatusReport             , LiteralEnum("AI"))
    (SecurityDefinitionRequest     , LiteralEnum("c"))
    (SecurityDefinition            , LiteralEnum("d"))
    (SecurityStatusRequest         , LiteralEnum("e"))
    (SecurityStatus                , LiteralEnum("f"))
    (SecurityListRequest           , LiteralEnum("x"))
    (SecurityList                  , LiteralEnum("y"))
  );

  //=========================================================================//
  // Common Prefix of All FIX Msgs:                                          //
  //=========================================================================//
  // NB: The following FIX prefix and posfix flds are NOT stored:
  // BeginString  (8)
  // BodyLength   (9)
  // MsgType      (35)
  // CheckSum     (10)
  //
  struct MsgPrefix
  {
    // Data Flds:
    MsgT                m_MsgType;              // 35; still need it for ref
    SeqNum              m_MsgSeqNum;            // 34
    bool                m_PossDupFlag;          // 43
    bool                m_PossResend;           // 97; XXX: NOT USED by us???
    utxx::time_val      m_SendingTime;          // 52
    utxx::time_val      m_OrigSendingTime;      // 122 (if after ResendRequest)

    // The following flds are currently used by the Server-side only: XXX: For
    // Client-Side, they are still present in "MsgPrefix", but not parsed, and
    // not used:
    Credential          m_SenderCompID;         // 49
    Credential          m_SenderSubID;          // 50
    Credential          m_TargetCompID;         // 56
    Credential          m_TargetSubID;          // 57
    int                 m_OrigMsgSz;            // Size of the wire FIX msg

    // NB: No explicit Default Ctor for "MsgPrefix": All its extensions (the
    // actual FIX Msg Types) are fully initialised on their own...
  };

  //=========================================================================//
  // Session-Level Msgs:                                                     //
  //=========================================================================//
  //=========================================================================//
  // "LogOn":                                                                //
  //=========================================================================//
  // "User Status" (926 field):
  //
  UTXX_ENUMV(UserStatT, (int, 0), // Encoded as "int"
    (LoggedIn,            1)
    (NotLoggedIn,         2)
    (UserNotRecognized,   3)
    (PasswordIncorrect,   4)
    (PasswordChanged,     5)
    (Other,               6)
    (ForcedUserLogOut,    7)
    (SessionShutdownWarn, 8)
    (PasswordExparied,    1000)   // EBS FIX password expried
    (DuplicateSession,    1001)   // EBS FIX cancel duplicate session
    (PasswordChangeFail,  1002)   // EBS FIX password changing failed
  );

  // "User Request Type" (924 field):
  enum class UserRequestT: char
  {
    UNDEFINED           = '\0',
    LogOn               = '1',    // Log On User
    LogOff              = '2',    // Log Off User
    ChangePassword      = '3',    // Change Password For User
  };

  enum class PasswdChangeStatusT: char   // MICEX only
  {
    UNDEFINED           = '\0',
    PasswdChangeOK      = '0',
    InvalidPasswdChange = '3'
  };

  struct LogOn: public MsgPrefix
  {
    // "LogOn"-specific Data Flds:
    // The following 4 flds are for the Server-side only:
    int                 m_HeartBtInt;           // 108
    bool                m_ResetSeqNumFlag;      // 141
    Credential          m_UserName;             // 553
    Credential          m_Passwd;               // 554

    // The following fld is for the Client-side only:
    PasswdChangeStatusT m_PasswdChangeStatus;   // 1409 (resp to passwd change)

    // Default Ctor:
    LogOn()             { Init(); }
    void Init()         { memset(this, '\0', sizeof(LogOn)); }
  };

  //=========================================================================//
  // "UserResponse":                                                         //
  //=========================================================================//
  struct UserResponse: public MsgPrefix
  {
    // "UserResponse"-specific Data Flds:
    OrderID             m_UserReqID;            // 923
    Credential          m_UserName;             // 553
    int                 m_TradingSessionID;     // 336
    UserStatT           m_UserStat;             // 926
    char                m_UserStatText[64];     // 927

    // Default Ctor:
    UserResponse()      { Init(); }
    void Init()         { memset(this, '\0', sizeof(UserResponse)); }
  };

  //=========================================================================//
  // "LogOut":                                                               //
  //=========================================================================//
  struct LogOut: public MsgPrefix
  {
    // "LogOut"-specific Data Fld(s):
    char                m_Text[ErrTxtSz];         // 58

    // Default Ctor:
    LogOut()            { Init(); }
    void Init()         { memset(this, '\0', sizeof(LogOut)); }
  };

  //=========================================================================//
  // "ResendRequest":                                                        //
  //=========================================================================//
  struct ResendRequest: public MsgPrefix
  {
    // "ResendRequest"-specific Data Flds:
    SeqNum              m_BeginSeqNo;           // 7
    SeqNum              m_EndSeqNo;             // 16; 0 = +oo

    // Default Ctor:
    ResendRequest()     { Init(); }
    void Init()         { memset(this, '\0', sizeof(ResendRequest)); }
  };

  //=========================================================================//
  // "SequenceReset":                                                        //
  //=========================================================================//
  // If "m_GapFillFlag" is set, then MsgSeqNum is used to indicate which SeqNum
  // we are filling in. Do we need to set PossDupFlag in that case? Yes...
  //
  struct SequenceReset: public MsgPrefix
  {
    // "SequenceReset"-specific Data Flds:
    bool                m_GapFillFlag;          // 123 (false = real Reset)
    SeqNum              m_NewSeqNo;             // 36

    // Default Ctor:
    SequenceReset()     { Init(); }
    void Init()         { memset(this, '\0', sizeof(SequenceReset)); }
  };

  //=========================================================================//
  // "TestRequest":                                                          //
  //=========================================================================//
  struct TestRequest: public MsgPrefix
  {
    // "TestRequest"-specific Data Flds:
    char  m_TestReqID[16];                      // 112

    // Default Ctor:
    TestRequest()       { Init(); }
    void Init()         { memset(this, '\0', sizeof(TestRequest)); }
  };

  //=========================================================================//
  // "HeartBeat":                                                            //
  //=========================================================================//
  struct HeartBeat: public MsgPrefix
  {
    // Default Ctor:
    HeartBeat()         { Init(); }
    void Init()         { memset(this, '\0', sizeof(HeartBeat));   }
  };

  //=========================================================================//
  // "Reject":                                                               //
  //=========================================================================//
  enum class SessRejReasonT: int
  {
    InvalidTagNumber        = 0,            // XXX: Cannot add UNDEFINED = 0!
    RequiredTagMissing      = 1,
    TagNotForThisMsg        = 2,
    UndefinedTag            = 3,
    TagWithoutValue         = 4,
    ValueIsIncorrect        = 5,
    IncorrectDataFormat     = 6,
    DecryptionProblem       = 7,
    SignatureProblem        = 8,
    CompIDProblem           = 9,
    SendingTimeProblem      = 10,
    InvalidMsgType          = 11,
    XMLValidationError      = 12,
    TagAppearsMoreThanOnce  = 13,
    TagOutOfOrder           = 14,
    GroupFldsOutOfOrder     = 15,
    IncorrectNumInGroup     = 16,
    NonDataHasSOH           = 17,
    InvalidApplVersion      = 18,
    Other                   = 99,
    FloodControl            = 7100          // FORTS only
  };

  //-------------------------------------------------------------------------//
  // "Reject" Itself:                                                        //
  //-------------------------------------------------------------------------//
  // XXX: "RefMsgType" is actually a "MsgT", but it would be problematic to
  // parse if 2-char seqs are used; we only need it as a string:
  //
  struct Reject: public MsgPrefix
  {
    // "Reject"-specific Data Flds:
    SeqNum          m_RefSeqNum;            // 45:  SeqNum of the rejected Msg
    int             m_RefTagID;             // 371: FldID which caused reject
    char            m_RefMsgType[4];        // 372: Type of Rejected Msg
    SessRejReasonT  m_SessionRejectReason;  // 373; see above
    char            m_Text[ErrTxtSz];       // 58

    // Default Ctor:
    Reject()        { Init(); }
    void Init()     { memset(this, '\0', sizeof(Reject)); }
  };

  //=========================================================================//
  // "News":                                                                 //
  //=========================================================================//
  struct News: public MsgPrefix
  {
    // "News"-specific Data Fld(s):
    char                m_Headline[ErrTxtSz];         // 148
    int                 m_NumLinesOfText;             // 33
    char                m_LinesOfText[32][ErrTxtSz];  // 58

    // Default Ctor:
    News()              { Init(); }
    void Init()         { memset(this, '\0', sizeof(News));   }
  };

  //=========================================================================//
  // Application-Level Order Mgmt Msgs (Server-to-Client):                   //
  //=========================================================================//
  //=========================================================================//
  // "BusinessMessageReject":                                                //
  //=========================================================================//
  // XXX: Not at all clear why it is required in the first place; "ExecReport"
  // and "CancelReject" could be used instead:
  //
  enum class BusinessRejectReasonT: int
  {
    Other                   = 0,        // Same as UNDEFINED, actually
    UnknownID               = 1,
    UnknownSecurity         = 2,
    UnSupportedMessageType  = 3,
    ApplicationNotAvail     = 4,
    CondReqdFldMissing      = 5,
    NotAuthorized           = 6,
    DeliverToFirmNotAvail   = 7,
    InvalidPriceIncr        = 18        // XXX: Not 8?
  };

  //-------------------------------------------------------------------------//
  // "BusinessMessageReject" Itself:                                         //
  //-------------------------------------------------------------------------//
  struct BusinessMessageReject: public MsgPrefix
  {
    // "BusinessMessageReject"-specific Data Flds:
    SeqNum                m_RefSeqNum;            // 45:  Rejected Msg SeqNum
    OrderID               m_BusinessRejectRefID;  // 379: Rejected Msg ClOrdID
    BusinessRejectReasonT m_BusinessRejectReason; // 380
    char                  m_Text[ErrTxtSz];       // 58

    // Default Ctor:
    BusinessMessageReject() { Init(); }
    void Init()  { memset(this, '\0', sizeof(BusinessMessageReject)); }
  };

  //=========================================================================//
  // "ExecutionReport":                                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "OrdStatusT":                                                           //
  //-------------------------------------------------------------------------//
  enum class OrdStatusT: char
  {
    UNDEFINED             = '\0',
    New                   = '0',
    PartiallyFilled       = '1',
    Filled                = '2',
    DoneForDay            = '3',
    Cancelled             = '4',
    Replaced              = '5',        // XXX: FIX 4.2 only!
    PendingCancel         = '6',
    Stopped               = '7',
    Rejected              = '8',
    Suspended             = '9',
    PendingNew            = 'A',
    Calculated            = 'B',        // XXX: What is it?
    Expired               = 'C',
    AcceptedForBidding    = 'D',        // XXX: What is it?
    PendingReplace        = 'E'
  };

  //-------------------------------------------------------------------------//
  // "ExecTypeT":                                                            //
  //-------------------------------------------------------------------------//
  // NB: What is the precise difference between "OrderStatusT" and "ExecTypeT"?
  // -- "ExecTypeT" is the REASON WHY "ExecReport" was generated, ie it is a
  //    type of the Report itself, whereas "OrderStatusT"  is the  curr status
  //    of that order; as there may be many statuses (eg wrt Fill or Replace),
  //    they are ordered according to the FIX precedence rules (see above) to
  //    produce  the "top-level" status which is reported.
  // -- Over-all, "ExecTypeT" is more reliable for acting upon -- it refers to
  //    an immediate past event; but the STATUS ("OrdStatusT") is sometimes re-
  //    quired as well:
  //
  enum class ExecTypeT: char
  {
    UNDEFINED             = '\0',
    New                   = '0',
    PartialFill           = '1',        // XXX: 4.2 only!!!
    Fill                  = '2',        // XXX: 4.2 only!!!
    DoneForDay            = '3',
    Cancelled             = '4',
    Replaced              = '5',
    PendingCancel         = '6',
    Stopped               = '7',
    Rejected              = '8',
    Suspended             = '9',
    PendingNew            = 'A',
    Calculated            = 'B',        // XXX: What is it?
    Expired               = 'C',
    Restated              = 'D',        // Cancelled by the Exchange?
    PendingReplace        = 'E',
    Trade                 = 'F',        // PartialFill or Fill
    TradeCorrect          = 'G',
    TradeCancel           = 'H',
    OrderStatus           = 'I',        // OrderStatus only, no Exec
    TradeInClearingHold   = 'J',
    TradeReleased         = 'K',        // Trade has been released to Clearing
    BySystem              = 'L'
  };

  //-------------------------------------------------------------------------//
  // Other "ExecutionReport" enums:                                          //
  //-------------------------------------------------------------------------//
  enum class OrdRejReasonT: int
  {                                     // Cannot use 0 for UNDEFINED!
    BrokerCredit          = 0,
    UnknownSymbol         = 1,
    ExchangeClosed        = 2,
    OrderExceedsLimit     = 3,
    TooLateToEnter        = 4,
    UnknownOrder          = 5,
    DuplicateOrder        = 6,          // Eg duplicate "ClOrdID"
    DuplicateOfVerbal     = 7,
    StaleOrder            = 8,
    TradeAlongRequired    = 9,
    InvalidInvestorID     = 10,
    UnSupportedAttrs      = 11,
    SurveillenceOption    = 12,
    IncorrectQuantity     = 13,
    IncorrectAllocQty     = 14,
    UnknownAccount        = 15,
    PriceExceedsCurrBand  = 16,
    InvalidPriceIncr      = 17,
    Other                 = 99
  };

  //-------------------------------------------------------------------------//
  // "ExecutionReport" Itself:                                               //
  //-------------------------------------------------------------------------//
  // NB: If "ExecutionReport" is sent in response to "OrderCancelRequest", then
  // "m_ClOrdID" is the same as "m_ClOrdID" in that request, ie it is the ID of
  // the Cancellation Request, not of the original order being cancelled;   for
  // the latter, use "m_OrigClOrdID":
  //
  template<typename QtyT>
  struct ExecutionReport: public MsgPrefix
  {
    //-----------------------------------------------------------------------//
    // "ExecutionReport"-specific Data Flds:                                 //
    //-----------------------------------------------------------------------//
    char            m_OrderID  [16];    // 37: OrderID as assigned  by Exchge
    char            m_MDEntryID[16];    // 278: another OrderID assigned by Ex;
                                        //     may be used for MDC co-ordinatn
    char            m_ExecID   [16];    // 17: ExecID  as assigned  by Exchge
    OrderID         m_ClOrdID;          // 11: OrderID as specified by Client
    OrderID         m_OrigClOrdID;      // 41:  same as m_OrigClOrdID  in
                                        //      OrderCancelRequest
    OrdStatusT      m_OrdStatus;        // 39:  see above
    ExecTypeT       m_ExecType;         // 150: see above
    SymKey          m_Symbol;           // 55
    int             m_SettlDate;        // 64 (optional): YYYYMMDD
    SideT           m_Side;             // 54
    PriceT          m_Price;            // 44 OrigPx
    OrderTypeT      m_OrdType;          // 40 (optional): orig order type
    QtyT            m_CumQty;           // 14
    QtyT            m_LeavesQty;        // 151
    QtyT            m_LastQty;          // 32 (only for ExecType=Trade)
    PriceT          m_LastPx;           // 31 (only for ExecType=Trade)
    char            m_Text[ErrTxtSz];   // 58
    OrdRejReasonT   m_OrdRejReason;     // 103 (if ExecType=Rejected)
    utxx::time_val  m_TransactTime;     // 60: Time of this report

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    ExecutionReport()   { Init(); }

    void Init()
    {
      // Zero-out the obj, but Pxs are to be set to NaN:
      memset(this, '\0', sizeof(ExecutionReport));
      m_Price  = PriceT();
      m_LastPx = PriceT();   // NaN
    }
  };

  //=========================================================================//
  // "OrderCancelReject":                                                    //
  //=========================================================================//
  enum class CxlRejReasonT: int         // This is for Amendments as well!
  {
    TooLateToCancel             = 0,    // Cannot use 0 for UNDEFINED!
    UnknownOrder                = 1,
    BrokerCredit                = 2,
    AlreadyPendingCancel        = 3,
    UnableToProcessMassCancel   = 4,
    WrongOrigOrdModTime         = 5,    // 586 did not match 60 of Order???
    DuplicateClOrdID            = 6,
    PriceExceedsCurrPrice       = 7,
    PriceExceedsCurrBand        = 8,
    InvalidPriceIncr            = 18,
    Other                       = 99
  };

  enum class CxlRejRespT: char
  {
    UNDEFINED                   = '\0',
    Cancel                      = '1',
    CancelReplace               = '2'
  };

  //-------------------------------------------------------------------------//
  // "OrderCancelReject" Itself:                                             //
  //-------------------------------------------------------------------------//
  struct OrderCancelReject: public MsgPrefix
  {
    // Data Flds:
    //
    char            m_OrderID[16];      // 37:  assigned by Exchange, if known
    OrderID         m_ClOrdID;          // 11:  ID of the Cancel/Amend Request
    OrderID         m_OrigClOrdID;      // 41:  Orig ID of that order
    OrdStatusT      m_OrdStatus;        // 39:  Curr order status; MICEX:
                                        //      always equal to "RejectedT=8"
    CxlRejReasonT   m_CxlRejReason;     // 102: as above
    CxlRejRespT     m_CxlRejResponseTo; // 434: as above
    char            m_Text[ErrTxtSz];   // 58

    // Default Ctor:
    OrderCancelReject() { Init(); }
    void Init()         { memset(this, '\0', sizeof(OrderCancelReject)); }
  };

  //=========================================================================//
  // "TradSesStatusT":                                                       //
  //=========================================================================//
  // This is a status of the whole Trading Session, affecting all Securities:
  //
  enum class TradSesStatusT: int
  {
    UNDEFINED                 = 0,
    Halted                    = 1,
    Open                      = 2,
    Closed                    = 3,
    PreOpen                   = 4,
    PreClose                  = 5,
    RequestRejected           = 6,      // XXX: What is it?
    // MICEX-specific values:
    TSRestarted               = 100,
    ConnectedToTS             = 101,
    GracefulDisconnectFromTS  = 102,
    RoughDisconnectFromTS     = 103,
    ReconnectedToTS           = 104
  };

  //-------------------------------------------------------------------------//
  // "TradingSessionStatus" Itself:                                          //
  //-------------------------------------------------------------------------//
  // XXX: In some Dialects (eg AlfaFIX), text can be quite big as it contains
  // XML data; as we currently do not use that data, it's OK if Test is trun-
  // cated:
  struct TradingSessionStatus: public MsgPrefix
  {
    // "TradingSessionStatus"-specific Data Flds:
    char            m_TradingSessionID[16];   // 336: Eg "TQBR" on MICEX
    TradSesStatusT  m_TradSesStatus;          // 340
    char            m_Text[ErrTxtSz];         // 58

    // Default Ctor:
    TradingSessionStatus() { Init(); }
    void Init()     { memset(this, '\0', sizeof(TradingSessionStatus)); }
  };

  //=========================================================================//
  // "OrderMassCancelReport" (currently MICEX only):                         //
  //=========================================================================//
  enum class MassCancelRespT: char
  {
    UNDEFINED                           = '\0',
    CancelRequestRejected               = '0',
    CancelOrdersForASecurity            = '1',
    CancelOrdersForAnUnderlying         = '2',
    CancelOrdersForAProduct             = '3',
    CancelOrdersForACFICode             = '4',
    CancelOrdersForASecurityType        = '5',
    CancelOrdersForATradingSession      = '6',
    CancelAllOrders                     = '7',
    CancelOrdersForAMarket              = '8',
    CancelOrdersForAMarketSegment       = '9',
    CancelOrdersForASecurityGroup       = 'A',
    CancelOrdersForASecuritiesIssuer    = 'B',
    CancelOrdersForAnUnderlyingsIssuer  = 'C'
  };

  enum class MassCancelRejReasonT: int
  {                                           // Cannot use 0 for UNDEFINED!
    MassCancelNotSupported              = 0,
    InvalidSecurity                     = 1,
    InvalidUnderlying                   = 2,
    InvalidProduct                      = 3,
    InvalidCFICode                      = 4,
    InvalidSecurityType                 = 5,
    InvalidTradingSession               = 6,
    InvalidMarket                       = 7,
    InvalidMarketSegment                = 8,
    InvalidSecurityGroup                = 9,
    InvalidSecurityIssuer               = 10,
    InvalidUnderlyingIssuer             = 11,
    Other                               = 99
  };

  enum class MassCancelReqT: char
  {
    UNDEFINED   = '\0',
    ByInstr     = '1',   // Cancel ords for the given instrument: FORTS & MICEX
    ForAllMkts  = '7',   // Cancel all ords: MICEX only
    ForThisMkt  = '8'    // FORTS only
  };

  //-------------------------------------------------------------------------//
  // "OrderMassCancelReport" Itself:                                         //
  //-------------------------------------------------------------------------//
  struct OrderMassCancelReport: public MsgPrefix
  {
    //-----------------------------------------------------------------------//
    // "OrderMassCancelReport"-specific Data Flds:                           //
    //-----------------------------------------------------------------------//
    OrderID               m_ClOrdID;                // 11: ID of MassCancelReq
    MassCancelReqT        m_MassCancelRequestType;  // 530
    MassCancelRespT       m_MassCancelResponse;     // 531
    MassCancelRejReasonT  m_MassCancelRejectReason; // 532
    char                  m_Text[ErrTxtSz];         // 58

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    OrderMassCancelReport()    { Init(); }
    void Init() { memset(this, '\0', sizeof(OrderMassCancelReport)); }
  };

  //=========================================================================//
  // Application-Level Mkt Data Msgs (Server-to-Client):                     //
  //=========================================================================//
  //=========================================================================//
  // "MarketDataRequestReject":                                              //
  //=========================================================================//
  enum class MDReqRejReasonT: char
  {
    UNDEFINED                     = '\0',
    UnknownSymbol                 = '0',
    DuplicateMDReqID              = '1',
    InsufficientBandwidth         = '2',
    InsufficientPermissions       = '3',
    UnSupportedSubscrReqType      = '4',
    UnSupportedMktDepth           = '5',
    UnSupportedMDUpdateType       = '6',
    UnSupportedAggregatedBook     = '7',
    UnSupportedMDEntryType        = '8',
    UnSupportedTradingSessionID   = '9',
    UnSupportedScope              = 'A',
    UnSupportedOpenCloseSettlFlag = 'B',
    UnSupportedMDImplicitDelete   = 'C',
    Expired                       = 'Y',      // AlfaFIX2 only
    Unsubscribed                  = 'Z',      //
    Other                         = 'X'       //
  };

  //-------------------------------------------------------------------------//
  // "MarketDataRequestReject" Itself:                                       //
  //-------------------------------------------------------------------------//
  struct MarketDataRequestReject: public MsgPrefix
  {
    // "MarketDataRequestReject"-specific Data Flds:
    OrderID         m_MDReqID;                // 262 (from Client's request)
    char            m_Text[ErrTxtSz];         // 58
    MDReqRejReasonT m_MDReqRejReason;         // 281

    // Default Ctor:
    MarketDataRequestReject() { Init(); }
    void Init()     { memset(this, '\0', sizeof(MarketDataRequestReject)); }
  };

  //=========================================================================//
  // "MarketDataIncrementalRefresh":                                         //
  //=========================================================================//
  enum class MDUpdateActionT: char
  {
    UNDEFINED       = '\0',
    New             = '0',
    Change          = '1',
    Delete          = '2'
  };

  enum class SettlmntTypeT: char
  {
    UNDEFINED       = '\0',
    Regular         = '0',
    Cash            = '1',
    NextDay         = '2',
    TPlus2          = '3',
    TPlus3          = '4',
    TPlus4          = '5',
    Future          = '6',
    WhenAndIfIssued = '7',
    SellersOption   = '8',
    TPlus5          = '9',
  };

  enum class MDEntryTypeT: char
  {
    UNDEFINED         = '\0',
    Bid               = '0',    // Important!
    Offer             = '1',    // Important!
    Trade             = '2',    // Important! Aka "Last"!
    IndexValue        = '3',
    Open              = '4',
    Close             = '5',
    SettlePrice       = '6',
    High              = '7',
    Low               = '8',
    WAPrice           = '9',
    Imbalance         = 'A',
    TradeVolume       = 'B',
    OpenInterest      = 'C',
    CompositeUndPx    = 'D',
    SimulatedSellPx   = 'E',
    SimulatedBuyPx    = 'F',
    MarginRate        = 'G',
    MidPx             = 'H',
    EmptyBook         = 'J',    // Important!
    SettleHighPx      = 'K',
    SettleLowPx       = 'L',
    PriorSettlePx     = 'M',
    SessionHighBid    = 'N',
    SessionLowOffer   = 'O',
    EarlyPxs          = 'P',
    AuctionClearingPx = 'Q',
    SwapValueFactor   = 'S',
    DailyValAdjLong   = 'R',
    CumValAdjLong     = 'T',
    DailyValAdjShort  = 'U',
    CumValAdjShort    = 'V',
    FixingPx          = 'W',
    CashRate          = 'X',
    RecoveryRate      = 'Y',
    RecoveryRateLong  = 'Z',
    RecoveryRateShort = 'a',  // XXX: Lower-case codes for MICEX only!..
    ClosingVolume     = 'c',
    UncovTradePrevent = 'e',
    ClosingBuyVolume  = 'f',
    ClosingSellVolume = 'g',
    OpenPeriodPx      = 'h',
    LastBidPx         = 'i',
    LastOfferPx       = 'j',
    ClosePrediodPx    = 'k',
    MktPx2            = 'l',
    MktPx             = 'm',
    OfficialOpenPx    = 'o',
    OfficialCurrPx    = 'p',
    AdmittedQuote     = 'q',
    CrossRate         = 'r',  // XXX: Also OfficialClosePx!
    RealMinStep       = 's',
    Duration          = 'u',
    TotalBidVolume    = 'v',
    TotalOfferVolume  = 'w',
    DarkPoolVolume    = 'x',
    AccruedInterest   = 'y',
    TradeList         = 'z'
  };

  enum class MDTickDirectionT: char
  {
    UNDEFINED         = '\0',
    PlusTick          = '0',
    ZeroPlusTick      = '1',
    MinusTick         = '2',
    ZeroMinusTick     = '3'
  };

  enum class QuoteConditionT: char
  {
    UNDEFINED  = '\0',
    Active     = 'A',
    Indicative = 'B'
  };

  //-------------------------------------------------------------------------//
  // "MDEntryIncr":                                                          //
  //-------------------------------------------------------------------------//
  // NB: "Symbol" and "SettlDate" may belong either to "MDEntryIncr"  or to the
  // "MarketDataIncrementalRefresh" itself; for the sake if generality, we pro-
  // vide them in BOTH places (and then the actual parser  decides what to do):
  // for separate (per-"MDEntryIncr") values:
  // NB: This class is parameterised by "DialecT" because the logic of "GetSec-
  // ID" may depend on it:
  //
  template<typename QtyT>
  struct MDEntryIncr
  {
    //-----------------------------------------------------------------------//
    // "MDEntryIncr" Flds:                                                   //
    //-----------------------------------------------------------------------//
    MDUpdateActionT   m_MDUpdateAction;         // 279
    MDEntryTypeT      m_MDEntryType;            // 269
    PriceT            m_MDEntryPx;              // 270
    QtyT              m_MDEntrySize;            // 271
    MDTickDirectionT  m_MDTickDirection;        // 274
    char              m_MDPaidGiven;            // 7562 (Currenex only, P/G)
    char              m_MDEntryID   [16];       // 278
    char              m_MDEntryRefID[16];       // 280 (FXBA Change only)
    SymKey            m_Symbol;                 // 55
    SettlmntTypeT     m_SettlmntType;           // 63
    QuoteConditionT   m_QuoteCondition;         // 276

    //-----------------------------------------------------------------------//
    // "MDEntryIncr" Methods:                                                //
    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-out the obj, but Pxs are to be set to NaN:
    MDEntryIncr()   { Init(); }
    void   Init()
    {
      memset(this, '\0', sizeof(MDEntryIncr));
      m_MDEntryPx   = PriceT();        // NaN
      m_MDEntrySize = QtyT::Invalid(); // NaN
    }

    // "GetUpdateAction":
    MDUpdateActionT GetUpdateAction() const
      { return m_MDUpdateAction; }

    // "GetEntryType" (for OrderBooks, may also designate the Side):
    MDEntryTypeT GetEntryType() const { return m_MDEntryType; }

    // "GetMDEntryID":
    // XXX: It is currently assumed that "MDEntryID" is a numerical str:
    OrderID GetMDEntryID() const
    {
      OrderID res = 0;
      (void) utxx::fast_atoi<OrderID, false>
             (m_MDEntryID, m_MDEntryID + sizeof(m_MDEntryID), res);
      return res;
    }

    // "GetEntryRefID":
    // XXX: Similar assumption: Must be a numerical str:
    //
    OrderID GetEntryRefID() const
    {
      OrderID res = 0;
      (void) utxx::fast_atoi<OrderID, false>
             (m_MDEntryRefID, m_MDEntryRefID + sizeof(m_MDEntryRefID), res);
      return res;
    }

    QtyT   GetEntrySize()   const { return m_MDEntrySize; }
    PriceT GetEntryPx  ()   const { return m_MDEntryPx;   }

    // "WasTradedFOL" (FullOrdersLog only): Currently, we don't know how to
    // tell this in FIX, so NO:
    constexpr static bool WasTradedFOL()            { return false; }

    // "GetTrade*": Determine from MDTickDirection:
    SideT                 GetTradeAggrSide()  const
    {
      if (m_MDPaidGiven != '\0')
        return m_MDPaidGiven == 'P' ? FIX::SideT::Buy : FIX::SideT::Sell;

      if ((m_MDTickDirection == MDTickDirectionT::PlusTick) ||
          (m_MDTickDirection == MDTickDirectionT::ZeroPlusTick))
        return FIX::SideT::Buy;
      else if ((m_MDTickDirection == MDTickDirectionT::MinusTick) ||
               (m_MDTickDirection == MDTickDirectionT::ZeroMinusTick))
        return FIX::SideT::Sell;
      else
        return FIX::SideT::UNDEFINED;
    }
    constexpr static int  GetTradeSettlDate()       { return 0; }
    ExchOrdID const&      GetTradeExecID()    const { return EmptyExchOrdID;   }

    // Whether this MDEntry is usable as an OrderBook update: We assume that in
    // FIX MktData, there are no extraneous msgs, so generally YES   (XXX: here
    // we do not check other constraints, such as MDEntryType):
    //
    constexpr static bool IsValidUpdate()           { return true; }
  };

  constexpr int MaxMDEs = 1000;   // XXX: Sufficient?

  //-------------------------------------------------------------------------//
  // "MarketDataIncrementalRefresh" Itself:                                  //
  //-------------------------------------------------------------------------//
  template<DialectT::type D, typename  QtyT>
  struct MarketDataIncrementalRefresh: public MsgPrefix
  {
    using MDEntry = MDEntryIncr<QtyT>;

    //-----------------------------------------------------------------------//
    // "MarketDataIncrementalRefresh"-specific Data Flds:                    //
    //-----------------------------------------------------------------------//
    // NB: Unlike "ExecutionReport", here "SettlDate" could also be a Tenor str.
    // XXX: It is strange but true that "Symbol" is positioned in "MDEntryIncr"
    // but "SettlDate" is here at the top level:
    //
    OrderID         m_MDReqID;                // 262 (from client's request)
    char            m_SettlDate[16];          // 64: YYYYMMDD or TOD/TOM/SPOT...
    int             m_NoMDEntries;            // 268
    MDEntry         m_MDEntries[MaxMDEs];     // Size also given by (268)

    //-----------------------------------------------------------------------//
    // "MarketDataIncrementalRefresh" Methods:                               //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    MarketDataIncrementalRefresh() { Init(); }

    void Init()
    {
      m_MDReqID     = 0;
      m_NoMDEntries = 0;
      memset(m_SettlDate, '\0', sizeof(m_SettlDate));
      for (int i = 0; i < MaxMDEs; ++i)
        m_MDEntries[i].Init();     // All Pxs become NaN!
    }

    // "GetNEntries":
    int GetNEntries()                const { return m_NoMDEntries; }

    // "GetEntry":
    MDEntry const& GetEntry(int a_i) const
    {
      assert(0 <= a_i && a_i < m_NoMDEntries && m_NoMDEntries <= MaxMDEs);
      return m_MDEntries[a_i];
    }

    // "GetMDSubscrID":
    OrderID        GetMDSubscrID()   const { return m_MDReqID; }

    // "IsLastFragment":
    // For a TCP_based FIX, there is no msg fragmentation at all,  so this flag
    // is actually meaningless; by convention, return "true" (each msg would be
    // a last fragment):
    constexpr static bool IsLastFragment() { return true;      }

    //-----------------------------------------------------------------------//
    // Accessors using both "MarketDataIncrementalRefresh" and "MDEntry*":   //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    static char const* GetSymbol(MDEntry const& a_mde)
    {
      // For the moment, the Symbol is contained in the MDE only:
      return a_mde.m_Symbol.data();
    }

    // "GetSecID":
    // XXX: In FIX, we do not use "GetSecID", because the information contained
    // in the IncrRefresh may not always sufficient to identify the  Instrument
    // (eg the Segment may be missing).    So in FIX, the SubscrID will be used
    // instead:
    constexpr static SecID GetSecID  (MDEntry const&)     { return 0; }

    // "GetRptSeq":
    // NB: "RptSeq" is a per-Instrument counter, typically not supported by FIX:
    constexpr static SeqNum GetRptSeq(MDEntry const&)     { return 0; }

    // "GetEventTS":
    // NB: In FIX, "MDEntryTime" is typically not supported, so use "Sending-
    // Time":
    utxx::time_val GetEventTS(MDEntry const&) const
      { return m_SendingTime; }
  };

  //=========================================================================//
  // "MarketDataSnapShot":                                                   //
  //=========================================================================//
  // NB:
  // (*) For Banded Quotes, we get the range of Qtys [m_MinQty..m_MDEntrySize]
  //     for a given price "m_MDEntryPx":
  // (*) Unlike "MarketDataIncrementalRefresh", for "MarketDataSnapShot", the
  //     "Symbol" and "SettlDate" flds are always contained  in the  main msg
  //     body, NOT in "MDEntrySnap":
  //
  template<typename QtyT>
  struct MDEntrySnap
  {
    //-----------------------------------------------------------------------//
    // "MDEntrySnap" Flds:                                                   //
    //-----------------------------------------------------------------------//
    MDEntryTypeT      m_MDEntryType;            // 269
    PriceT            m_MDEntryPx;              // 270
    QtyT              m_MDEntrySize;            // 271
    MDTickDirectionT  m_MDTickDirection;        // 274
    char              m_MDPaidGiven;            // 7562 (Currenex only, P/G)
    QtyT              m_MinQty;                 // 110
    char              m_QuoteEntryID    [16];   // 299
    QuoteConditionT   m_QuoteCondition;         // 276
    char              m_TradingSessionID[16];   // 336

    //-----------------------------------------------------------------------//
    // "MDEntrySnap" Methods:                                                //
    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-out the obj, but Pxs must be set to NaN:
    MDEntrySnap()   { Init(); }

    void Init  ()
    {
      memset(this, '\0', sizeof(MDEntrySnap));
      m_MDEntryPx = PriceT();          // NaN
      m_MDEntrySize = QtyT::Invalid(); // NaN
    }

    // Accessors:
    // "GetUpdateAction": For SnapShot MDEs, it should always be "New":
    constexpr static MDUpdateActionT   GetUpdateAction()
      { return MDUpdateActionT::New; }

    // "GetEntryType" (for OrderBooks, may also designate the Side):
    MDEntryTypeT GetEntryType() const { return m_MDEntryType; }

    // "GetMDEntryID":
    // FIXME: Some Dialects use QuoteEntryID here, others possibly MDEntryID --
    // we need to make it Dialect-specific. For the moment, we use QuoteEntryID
    // only:
    OrderID GetMDEntryID()  const
    {
      // XXX: It is currently assumed that "QuoteEntryID" is a numerical str:
      OrderID res = 0;
      (void) utxx::fast_atoi<OrderID, false>
             (m_QuoteEntryID, m_QuoteEntryID + sizeof(m_QuoteEntryID), res);
      return res;
    }

    // "GetEntryRefID": Not applicable to SnapShots:
    constexpr static OrderID GetEntryRefID() { return 0.0; }

    QtyT   GetEntrySize()   const  { return m_MDEntrySize; }
    PriceT GetEntryPx  ()   const  { return m_MDEntryPx;   }

    // "WasTradedFOL" (FullOrdersLog only): Currently, we don't know how to
    // tell this in FIX, so NO:
    constexpr static bool WasTradedFOL()            { return false; }

    // "GetTrade*": There should be no Trade data in SnapShots:
    SideT                 GetTradeAggrSide()  const
    {
      if (m_MDPaidGiven != '\0')
        return m_MDPaidGiven == 'P' ? FIX::SideT::Buy : FIX::SideT::Sell;

      if ((m_MDTickDirection == MDTickDirectionT::PlusTick) ||
          (m_MDTickDirection == MDTickDirectionT::ZeroPlusTick))
        return FIX::SideT::Buy;
      else if ((m_MDTickDirection == MDTickDirectionT::MinusTick) ||
               (m_MDTickDirection == MDTickDirectionT::ZeroMinusTick))
        return FIX::SideT::Sell;
      else
        return FIX::SideT::UNDEFINED;
    }
    constexpr static int  GetTradeSettlDate()       { return 0; }
    ExchOrdID const&      GetTradeExecID()    const { return EmptyExchOrdID;   }

    // Whether this MDEntry is usable as an OrderBook or Trade update: Again,
    // generally YES:
    constexpr static bool IsValidUpdate()           { return true; }
  };

  //-------------------------------------------------------------------------//
  // "MarketDataSnapShot" Itself:                                            //
  //-------------------------------------------------------------------------//
  // NB: This class is parameterised by "DialecT" because the logic of "GetSec-
  // ID" may depend on it:
  //
  template<DialectT::type D, typename QtyT>
  struct MarketDataSnapShot: public MsgPrefix
  {
    using MDEntry = MDEntrySnap<QtyT>;

    //-----------------------------------------------------------------------//
    // "MarketDataSnapShot"-specific Data Flds:                              //
    //-----------------------------------------------------------------------//
    // Again, "SettlDate"s could also be Tenors:
    OrderID         m_MDReqID;                // 262 (from client's request)
    SymKey          m_Symbol;                 // 55
    SecID           m_SecID;                  // 48 (for LMAX)
    char            m_SettlDate [16];         // 64: YYYYMMDD or TOD/TOM/SPOT...
    char            m_SettlDate2[16];         // 193, AlfaFIX2 only
    QtyT            m_TotalVolumeTraded;      // 387
    int             m_NoMDEntries;            // 268
    MDEntry         m_MDEntries[MaxMDEs];

    //-----------------------------------------------------------------------//
    // "MarketDataSnapShot" Methods:                                         //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    MarketDataSnapShot() { Init(); }

    void Init()
    {
      m_MDReqID           = 0;
      m_Symbol            = EmptySymKey;
      memset(m_SettlDate,  '\0', sizeof(m_SettlDate) );
      memset(m_SettlDate2, '\0', sizeof(m_SettlDate2));
      m_TotalVolumeTraded = QtyT();  // 0
      m_NoMDEntries       = 0;
      for (int i = 0; i < MaxMDEs; ++i)
        m_MDEntries[i].Init();       // All Pxs become NaN!
    }

    // "GetNEntries":
    int GetNEntries()                const  { return m_NoMDEntries; }

    // "GetEntry":
    MDEntry const& GetEntry(int a_i) const
    {
      assert(0 <= a_i && a_i < m_NoMDEntries && m_NoMDEntries <= MaxMDEs);
      return m_MDEntries[a_i];
    }

    // "GetMDSubscrID":
    OrderID        GetMDSubscrID()   const  { return m_MDReqID; }

    // "IsLastFragment":
    // For a TCP_based FIX, there is no msg fragmentation at all,  so this flag
    // is actually meaningless; by convention, return "true" (each msg would be
    // a last fragment):
    constexpr static bool IsLastFragment()  { return true; }

    //-----------------------------------------------------------------------//
    // Accessors using both "MarketDataSnapShot" and "MDEntrySnap":          //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    char const* GetSymbol(MDEntry const&) const
    {
      // For the moment, the Symbol is contained in the main msg only:
      return m_Symbol.data();
    }

    // "GetSecID":
    // Similar to the IncrRefresh, for a SnapShot, we may not always be able to
    // return a correct SecID, so  always return 0:
    constexpr SecID GetSecID  (MDEntry const&) const
    {
      return m_SecID;
    }

    // "GetRptSeq":
    // NB: "RptSeq" is a per-Instrument counter, typically not supported by FIX:
    constexpr static SeqNum GetRptSeq(MDEntry const&)           { return 0; }

    // "GetEventTS":
    // NB: In FIX, "MDEntryTime" is typically not supported, so use "Sending-
    // Time":
    utxx::time_val GetEventTS(MDEntry const&) const { return m_SendingTime; }
  };

  //=========================================================================//
  // "Quote" (S):                                                            //
  //=========================================================================//
  // We fashion this after the MarketDataSnapShot interface so that we can
  // plug it into the existing framework.
  //
  enum class QuoteTypeT: char
  {
    UNDEFINED  = '\0',
    Indicative = '0',
    Tradable   = '1'
  };

  template<typename QtyT>
  struct MDEntryQuote
  {
    //-----------------------------------------------------------------------//
    // "MDEntryQuote" Flds:                                                  //
    //-----------------------------------------------------------------------//
    MDEntryTypeT    m_MDEntryType;            // 269
    PriceT          m_MDEntryPx;              // 270
    QtyT            m_MDEntrySize;            // 271

    //-----------------------------------------------------------------------//
    // "MDEntryQuote" Methods:                                               //
    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-out the obj, but Pxs must be set to NaN:
    MDEntryQuote()   { Init(); }

    void Init   ()
    {
      m_MDEntryPx = PriceT();  // NaN
      m_MDEntrySize = QtyT(0L);
    }

    // Accessors:
    // "GetUpdateAction": For SnapShot MDEs, it should always be "New":
    constexpr static MDUpdateActionT   GetUpdateAction()
      { return MDUpdateActionT::New; }

    // "GetEntryType" (for OrderBooks, may also designate the Side):
    MDEntryTypeT GetEntryType() const { return m_MDEntryType; }

    // Quotes don't have entry IDs
    OrderID GetMDEntryID() const { return 0; }

    // "GetEntryRefID": Not applicable to Quote:
    constexpr static OrderID GetEntryRefID() { return 0.0; }

    QtyT   GetEntrySize()   const  { return m_MDEntrySize; }
    PriceT GetEntryPx  ()   const  { return m_MDEntryPx;   }

    // "WasTradedFOL" (FullOrdersLog only): Currently, we don't know how to
    // tell this in FIX, so NO:
    constexpr static bool WasTradedFOL()            { return false; }

    // "GetTrade*": There should be no Trade data in Quote:
    SideT                 GetTradeAggrSide()  const { return SideT::UNDEFINED; }
    constexpr static int  GetTradeSettlDate()       { return 0; }
    ExchOrdID const&      GetTradeExecID()    const { return EmptyExchOrdID;   }

    // Whether this MDEntry is usable as an OrderBook or Trade update: Again,
    // generally YES:
    constexpr static bool IsValidUpdate()           { return true; }
  };
  //-------------------------------------------------------------------------//
  // "Quote" Itself:                                                         //
  //-------------------------------------------------------------------------//
  template<typename QtyT>
  struct Quote: public MsgPrefix
  {
    using MDEntry = MDEntryQuote<QtyT>;

    OrderID         m_QuoteReqID;             // 131
    char            m_QuoteID   [64];         // 117
    QuoteTypeT      m_QuoteType;              // 537
    SymKey          m_Symbol;                 // 55
    QtyT            m_OrderQty;               // 38 optional
    // This are encoded in the MDEntryQuote below
    // PriceT       m_BidPx;                  // 132
    // PriceT       m_OfferPx;                // 133
    // QtyT         m_BidSize;                // 134
    // QtyT         m_OfferSize;              // 135
    MDEntry         m_MDEntries[2];           // first bid, second offer
    utxx::time_val  m_ValidUntilTime;         // 62
    utxx::time_val  m_TransactTime;           // 60
    char            m_Currency  [8];          // 15
    int             m_QuoteVersion;           // 20000 Cumberland only

    // Default Ctor:
    Quote()     { Init(); }

    void Init()
    {
      memset(this, '\0', sizeof(Quote));
      m_MDEntries[0].m_MDEntryType = MDEntryTypeT::Bid;
      m_MDEntries[1].m_MDEntryType = MDEntryTypeT::Offer;
    }

    // "GetNEntries":
    // We always have 2 entries, one bid and one offer
    inline int GetNEntries()                const  { return 2; }

    // "GetEntry":
    inline MDEntry const& GetEntry(int a_i) const
    {
      assert(0 <= a_i && a_i < 2);
      return m_MDEntries[a_i];
    }

    // "GetMDSubscrID":
    inline OrderID GetMDSubscrID()          const { return m_QuoteReqID; }

    // "IsLastFragment":
    // For a TCP_based FIX, there is no msg fragmentation at all,  so this flag
    // is actually meaningless; by convention, return "true" (each msg would be
    // a last fragment):
    constexpr static bool IsLastFragment()  { return true; }

    //-----------------------------------------------------------------------//
    // Accessors using "MarketDataSnapShot", "MDEntrySnap", "MDEntryQuote":  //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    char const* GetSymbol(MDEntry const&) const
    {
      // For the moment, the Symbol is contained in the main msg only:
      return m_Symbol.data();
    }

    // "GetSecID":
    // Similar to the IncrRefresh, for a SnapShot, we may not always be able to
    // return a correct SecID, so  always return 0:
    constexpr static SecID GetSecID  (MDEntry const&)            { return 0; }

    // "GetRptSeq":
    // NB: "RptSeq" is a per-Instrument counter, typically not supported by FIX:
    constexpr static SeqNum GetRptSeq(MDEntry const&)            { return 0; }

    // "GetEventTS":
    // NB: In FIX, "MDEntryTime" is typically not supported, so use "Sending-
    // Time":
    utxx::time_val GetEventTS(MDEntry const&) const {
      if (m_TransactTime.seconds() > 0.0)
	return m_TransactTime;
      else
	return utxx::now_utc();
    }
  };

  //=========================================================================//
  // "SecurityDefinition":                                                   //
  //=========================================================================//
  // XXX: The "Underlying" prefixes are misleading -- they actually refer to
  // THIS security, not to any underlying instrument:
  //
  enum class PutOrCallT: char
  {
    UNDEFINED       = '\0',
    Put             = '0',
    Call            = '1'
  };

  enum class SecRespTypeT: char   // (by std, should be "int")
  {
    UNDEFINED                     = '\0',
    AcceptAsIs                    = '1',
    AcceptWithRevisions           = '2',
    ListOfSecTypes                = '3',
    ListOfSecurities              = '4',      // This is what we normally get
    RejectSecurityProposal        = '5',
    CanNotMatchSelectionCriteria  = '6'
  };

  struct RelatedSym
  {
    // Data Flds:
    SymKey          m_UnderlyingSymbol;               // 311
    char            m_UnderlyingSecurityID[16];       // 309
    char            m_UnderlyingSecurityDesc[64];     // 307: eg ISIN
    Ccy             m_UnderlyingCurrency;             // 318
    long            m_RatioQty;                       // 319: Lot Size
    char            m_UnderlyingSecurityExchange[16]; // 308
    int             m_UnderlyingMaturityMonthYear;    // 313: YYYYMM
    PutOrCallT      m_UnderlyingPutOrCall;            // 315
    PriceT          m_UnderlyingStrikePrice;          // 316 (for options)
    char            m_UnderlyingSecurityType[4];      // 310 (eg FUT, OPT)
    char            m_UnderlyingSymbolSfx[16];        // 312

    // Default Ctor: Zero-out the obj, but Pxs are to be set to NaN:
    RelatedSym()    { Init(); }

    void  Init()
    {
      memset(this, '\0', sizeof(RelatedSym));
      m_UnderlyingStrikePrice = PriceT(); // NaN
    }
  };

  enum class SecIDSrcT: char  // SecurityIDSource
  {
    UNDEFINED       = '\0',
    CUSIP           = '1',
    SEDOL           = '2',
    QUIK            = '3',    // XXX: Don't confuse it with QUIK terminal...
    ISIN            = '4',
    RICCode         = '5',
    ISOCcyCode      = '6',
    ISOCountryCode  = '7',
    ExchangeSymbol  = '8',
    CTASymbol       = '9',
    Bloomberg       = 'A',
    Wertpapier      = 'B',
    Dutch           = 'C',
    Valoren         = 'D',
    Sicovam         = 'E',
    Belgian         = 'F',
    Common          = 'G',
    ClearingHouse   = 'H',
    ISDA_FpML       = 'I',
    OPRA            = 'J'
  };

  // XXX: How many "RealtedSym"s can be carried in a single FIX msg?
  // We assume the following limit:
  constexpr int MaxRelSyms = 100;

  //-------------------------------------------------------------------------//
  // "SecurityDefinition" Itself:                                            //
  //-------------------------------------------------------------------------//
  struct SecurityDefinition: public MsgPrefix
  {
    OrderID         m_SecurityReqID;          // 320 (from Client's request)
    char            m_SecurityResponseID[16]; // 322
    SecRespTypeT    m_SecurityResponseType;   // 323
    SecIDSrcT       m_UnderlyingIDSource;     // 305
    int             m_TotalNumSecurities;     // 393
    int             m_NoRelatedSym;           // 146
    RelatedSym      m_RelatedSyms[MaxRelSyms];

    // Default Ctor:
    SecurityDefinition()    { Init(); }

    void Init()
    {
      m_SecurityReqID        = 0;
      memset(m_SecurityResponseID, '\0', sizeof(m_SecurityResponseID));
      m_SecurityResponseType = SecRespTypeT::UNDEFINED;
      m_UnderlyingIDSource   = SecIDSrcT   ::UNDEFINED;
      m_TotalNumSecurities   = 0;
      m_NoRelatedSym         = 0;
      for (int i = 0; i < MaxRelSyms; ++i)
        m_RelatedSyms[i].Init();
    }
  };

  //=========================================================================//
  // "SecTrStatusT":                                                         //
  //=========================================================================//
  // This is a trading status of an individual Security:
  //
  enum class SecTrStatusT: int
  {
    UNDEFINED                   = 0,
    OpeningDelay                = 1,
    TradingHalt                 = 2,          // HALT!
    Resume                      = 3,
    NoOpenNoResume              = 4,
    PriceIndication             = 5,
    TradingRangeIndication      = 6,
    MarketImbalanceBuy          = 7,
    MarketImbalanceSell         = 8,
    MarketOnCloseImbalanceBuy   = 9,
    MarketOnCloseImbalanceSell  = 10,
    // "11" is not in use
    NoMarketImbalance           = 12,
    NoMarketOnCloseImbalance    = 13,
    ITSPreOpening               = 14,
    NewPriceIndication          = 15,
    TradeDisseminationTime      = 16,
    ReadyToTrade                = 17,         // OPEN!
    NotAvailableForTrading      = 18,         // CLOSED!
    NotTradedOnThisMarket       = 19,
    UnknownOrInvalid            = 20,
    SessionInitiated            = 21          // Pre-Trade
  };

  //-------------------------------------------------------------------------//
  // "SecurityStatus" Itself:                                                //
  //-------------------------------------------------------------------------//
  struct SecurityStatus: public MsgPrefix
  {
    // "SecurityStatus"-specific Data Flds:
    OrderID         m_SecurityStatusReqID;    // 324 (from Client's request)
    SymKey          m_Symbol;                 // 55
    char            m_SecurityExchange[16];   // 207
    char            m_SecurityType[16];       // 167
    int             m_MaturityMonthYear;      // 200: YYYYMM
    int             m_MaturityDay;            // 205: DD
    PutOrCallT      m_PutOrCall;              // 201
    PriceT          m_StrikePrice;            // 202
    SecTrStatusT    m_SecurityTradingStatus;  // 326
    PriceT          m_HighPx;                 // 332 (for this session)
    PriceT          m_LowPx;                  // 333 (for this session)
    PriceT          m_LastPx;                 // 31

    // Default Ctor: Zero-out the obj, but Pxs are to be set to NaN:
    SecurityStatus() { Init(); }

    void Init()
    {
      memset(this, '\0', sizeof(SecurityStatus));
      m_StrikePrice = PriceT(); // All NaNs
      m_HighPx      = PriceT(); //
      m_LowPx       = PriceT(); //
      m_LastPx      = PriceT(); //
    }
  };

  //=========================================================================//
  // "MarketDataRequest":                                                    //
  //=========================================================================//
  // NB: We do not provide this msg type itself because it's an output (direct-
  // to-wire) only; we only provide the associated enums:
  //
  enum class SubscrReqTypeT: char
  {
    UNDEFINED       = '\0',
    SnapShot        = '0',                    // One-off    SnapShot
    Subscribe       = '1',                    // SnapShot + Updates
    UnSubscribe     = '2',                    // Disable (SnapShot + Updates)
    TradesOnly      = 'T'                     // XXX: Currenex only
  };

  enum class MDUpdateTypeT: char
  {
    UNDEFINED       = '\0',
    FullRefresh     = '0',                    // Same as SnapShot
    IncrRefresh     = '1'                     // Still, in this case we receive
  };                                          // the 1st snapshot in full

  //=========================================================================//
  // "SecurityListRequest" (x):                                              //
  //=========================================================================//
  enum class SecurityListRequestTypeT: char
  {
    UNDEFINED         = '\0',
    Symbol            = '0',
    SecurityType      = '1',
    Product           = '2',
    TradingSessionID  = '3',
    AllSecurities     = '4'              // Cumberland only supports this value
  };

  //-------------------------------------------------------------------------//
  // "SecurityListRequest" Itself:                                           //
  //-------------------------------------------------------------------------//
  struct SecurityListRequest: public MsgPrefix
  {
    char                      m_SecurityReqID[64];            // 320
    SecurityListRequestTypeT  m_SecurityListRequestType;      // 559
    SubscrReqTypeT            m_SubscriptionRequestType;      // 263

    // Default Ctor:
    SecurityListRequest() { Init(); }
    void Init()           { memset(this, '\0', sizeof(SecurityListRequest)); }
  };

  //=========================================================================//
  // "SecurityList" (y):                                                     //
  //=========================================================================//
  // NB: We are implementing the Cumberland message format, which is
  // non-standard. It's missing the fields SecurityResponseID (322) and
  // SecurityRequestResult (560), which are required by the FIX standard.
  // Furthermore, the symbols include non-standard fields to denote the
  // quantity, price, and amount scales, as well as the instrument status.
  //
  enum class InstrumentStatusT: char
  {
    UNDEFINED = '\0',
    Enabled   = '0',
    Disabled  = '1'
  };

  struct CumberlandSym
  {
    // Data Flds:
    SymKey              m_Symbol;               // 55
    int                 m_QuantityScale;        // 20001
    int                 m_PriceScale;           // 20002
    int                 m_AmountScale;          // 20003
    InstrumentStatusT   m_InstrumentStatus;     // 20004

    // Default Ctor: Zero-out the obj
    CumberlandSym()   { Init(); }
    void Init()       { memset(this, '\0', sizeof(CumberlandSym)); }
  };
  //-------------------------------------------------------------------------//
  // "SecurityList" Itself:                                                  //
  //-------------------------------------------------------------------------//
  struct SecurityList: public MsgPrefix
  {
    char            m_SecurityReqID[64];      // 320
    int             m_NoRelatedSym;           // 146
    CumberlandSym   m_Instruments[MaxRelSyms];

    // Default Ctor:
    SecurityList() { Init(); }
    void Init()    { memset(this, '\0', sizeof(SecurityList)); }
  };

  //=========================================================================//
  // Msgs to be Received on the Server-Side Only:                            //
  //=========================================================================//
  // XXX: Currently, they are quite minimalistic:
  //      Also note that here, Order- and ReqIDs are strings. We allow for at
  //      most 32 chars including the terminating '\0' for them:
  //
  //=========================================================================//
  // "NewOrderSingle":                                                       //
  //=========================================================================//
  // TODO: Parties and even Account are not supported yet!
  //
  template<typename QtyT>
  struct NewOrderSingle: public MsgPrefix
  {
    char            m_ClOrdID[32];            // 11
    OrderTypeT      m_OrdType;                // 40
    TimeInForceT    m_TmInForce;              // 59
    SymKey          m_Symbol;                 // 55
    SideT           m_Side;                   // 54
    QtyT            m_Qty;                    // 38
    PriceT          m_Px;                     // 44; NaN (not 0!) if undefined
    utxx::time_val  m_TransactTime;           // 60

    // Default Ctor: Zero-out the obj, but Pxs are to be set to NaN:
    NewOrderSingle() { Init(); }

    void Init()
    {
      memset(this, '\0', sizeof(NewOrderSingle));
      m_Px = PriceT();   // NaN
    }
  };

  //=========================================================================//
  // "OrderCancelRequest":                                                   //
  //=========================================================================//
  // TODO: This is the barest possible definition. Account, Side, Qty etc flds
  // are typically required by real-life FIX dialects,  but none are supported
  // here:
  struct OrderCancelRequest: public MsgPrefix
  {
    char            m_ClOrdID    [32];        // 11
    char            m_OrigClOrdID[32];        // 41
    utxx::time_val  m_TransactTime;           // 60

    // Default Ctor:
    OrderCancelRequest() { Init(); }
    void Init()   { memset(this, '\0', sizeof(OrderCancelRequest)); }
  };

  //=========================================================================//
  // "OrderCancelReplaceRequest":                                            //
  //=========================================================================//
  // TODO: Again, this is the minimum possible definition. No Side, Account etc:
  //
  template<typename QtyT>
  struct OrderCancelReplaceRequest: public MsgPrefix
  {
    char            m_ClOrdID    [32];        // 11
    char            m_OrigClOrdID[32];        // 41
    QtyT            m_Qty;                    // 38: NewQty; <= 0 if unchanged
    PriceT          m_Px;                     // 44: NewPx;  NaN  if unchanged
    utxx::time_val  m_TransactTime;           // 60

    // Default Ctor: Zero-out the obj, but Pxs are to be set to NaN:
    OrderCancelReplaceRequest() { Init(); }

    void Init()
    {
      memset(this, '\0', sizeof(OrderCancelReplaceRequest));
      m_Px = PriceT();   // NaN
    }
  };

  //=========================================================================//
  // "MarketDataRequest":                                                    //
  //=========================================================================//
  // The following assumptions are made -- ie there are no explicit msg flds to
  // control the bahavious listed below:
  //
  // (*) The request is for 1 Symbol only (Tag 146=1);
  // (*) If this is a Subscribe rqeuest (as opposed to UnSubscribe), then both
  //     Bid and Ask OrderBook sides are subscribed for; Trades will also be
  //     provided if supported by the Server (but usually not):
  //     no Tags 267 and 269;
  // (*) MktData will be provided up to the maximum OrderBook depth  supported
  //     by the Server: no Tag 264;
  // (*) MktData delivery mode  is as defined by the Server (eg SnapShot + Incr
  //     Updates if available, or by SnapShots only): no Tag 265:
  //
  struct MarketDataRequest: public MsgPrefix
  {
    char            m_ReqID    [32];          // 262
    SubscrReqTypeT  m_ReqType;                // 263
    char            m_SettlDate[16];          // 64: YYYYMMDD or TOD/TOM/SPOT...
    int             m_NoRelatedSym;           // 146: must be 1
    SymKey          m_Symbol;                 // 55 (formally in Group of 1)

    // Default Ctor:
    MarketDataRequest() { Init(); }
    void Init()         { memset(this, '\0', sizeof(MarketDataRequest)); }
  };

  //=========================================================================//
  // "OrderMassStatusRequest" (AF):                                          //
  //=========================================================================//
  // Cumberland only supports this value:
  //
  enum class MassStatusReqTypeT: char
  {
    UNDEFINED          = '\0',
    StatusForAllOrders = '7'
  };

  //-------------------------------------------------------------------------//
  // "OrderMassStatusRequest" Itself:                                        //
  //-------------------------------------------------------------------------//
  struct OrderMassStatusRequest: public MsgPrefix
  {
    char                      m_MassStatusReqID[64];    // 584
    MassStatusReqTypeT        m_MassStatusReqType;      // 585
    int                       m_NumberOfRecords;        // 20005 Cumberland only

    // Default Ctor:
    OrderMassStatusRequest() { Init(); }
    void Init()
    {
      memset(this, '\0', sizeof(OrderMassStatusRequest));
      // Cumberland only supports this value
      m_MassStatusReqType = MassStatusReqTypeT::StatusForAllOrders;
    }
  };

  //=========================================================================//
  // "QuoteRequest" (R):                                                     //
  //=========================================================================//
  template<typename QtyT>
  struct QuoteRequest: public MsgPrefix
  {
    char            m_QuoteReqID[64];         // 131
    int             m_NoRelatedSym;           // 146 must be 1
    SymKey          m_Symbol;                 // 55 (formally in Group of 1)
    QtyT            m_Qty;                    // 38 (in group with above,
                                              // optional, don't send if <= 0)
    Ccy             m_Currency;               // 15 currency for quantity above

    // Default Ctor:
    QuoteRequest() { Init(); }
    void Init()    { memset(this, '\0', sizeof(QuoteRequest)); }
  };

  //=========================================================================//
  // "QuoteRequestReject" (AG):                                              //
  //=========================================================================//
  enum class QuoteReqRejReasonT: int
  {
    UNDEFINED                   = 0,
    UnknownSymbol               = 1,
    ExchangeClosed              = 2,
    QuoteRequestExceedsLimit    = 3,
    TooLateToEnter              = 4,
    InvalidPrice                = 5,
    NotAuthorizedToRequestQuote = 6,
    NoMatchForInquiry           = 7,
    NoMarketForInstrument       = 8,
    NoInventory                 = 9,
    Pass                        = 10,
    Other                       = 99
  };

  //-------------------------------------------------------------------------//
  // "QuoteRequestReject" Itself:                                            //
  //-------------------------------------------------------------------------//
  struct QuoteRequestReject: public MsgPrefix
  {
    char                m_QuoteReqID[64];         // 131
    QuoteReqRejReasonT  m_QuoteReqRejReason;      // 658
    int                 m_NoRelatedSym;           // 146 assume always 1
    SymKey              m_Symbol;                 // 55 (formally in Group of 1)
    char                m_Text      [ErrTxtSz];   // 58

    // Default Ctor:
    QuoteRequestReject() { Init(); }
    void Init()          { memset(this, '\0', sizeof(QuoteRequestReject)); }
  };

  //=========================================================================//
  // "QuoteCancel" (Z):                                                      //
  //=========================================================================//
  // Cumberland only supports this non-standard value:
  //
  enum class QuoteCancelTypeT: char
  {
    UNDEFINED                     = '\0',
    CancelQuoteSpecifiedInQuoteID = '5'
  };

  //-------------------------------------------------------------------------//
  // "QuoteCancel" Itself:                                                   //
  //-------------------------------------------------------------------------//
  struct QuoteCancel: public MsgPrefix
  {
    char              m_QuoteID[64];            // 117
    QuoteCancelTypeT  m_QuoteCancelType;        // 298
    int               m_QuoteVersion;           // 20000 Cumberland only

    // Default Ctor:
    QuoteCancel() { Init(); }
    void Init()
    {
      memset(this, '\0', sizeof(QuoteCancel));
      // Cumberland only supports this value
      m_QuoteCancelType = QuoteCancelTypeT::CancelQuoteSpecifiedInQuoteID;
    }
  };

  //=========================================================================//
  // "QuoteStatusReport" (AI):                                               //
  //=========================================================================//
  enum class QuoteStatusT: int
  {
    UNDEFINED         = 0,
    RemovedFromMarket = 6,
    QuoteNotFound     = 9,
    Cancelled         = 17 // maybe non-standard (Cumberland)
  };

  //-------------------------------------------------------------------------//
  // "QuoteStatusReport" Itself:                                             //
  //-------------------------------------------------------------------------//
  struct QuoteStatusReport: public MsgPrefix
  {
    char            m_QuoteReqID[64];         // 131
    char            m_QuoteID   [64];         // 117
    SymKey          m_Symbol;                 // 55 optional
    QuoteStatusT    m_QuoteStatus;            // 297
    QuoteTypeT      m_QuoteType;              // 537
    int             m_QuoteVersion;           // 20000 Cumberland only, optional
    char            m_Text[ErrTxtSz];         // 58

    // Default Ctor:
    QuoteStatusReport() { Init(); }
    void Init()         { memset(this, '\0', sizeof(QuoteStatusReport)); }
  };

} // End namespace FIX
} // End namespace MAQUETTE
