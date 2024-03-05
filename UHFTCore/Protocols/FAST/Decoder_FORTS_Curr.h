// vim:ts=2:et
//===========================================================================//
//                  "Protocols/FAST/Decoder_FORTS_Curr.h":                   //
//     Types and Decoding Functions for FAST Messages for FORTS (all ACs)    //
//               For the "Curr" FORTS FAST Ver (29.02.2016)                  //
//           NB: Supporting the FullOrdersLog only, no Aggregates!           //
//===========================================================================//
#pragma  once

#include "Basis/BaseTypes.hpp"             // For "SeqNum", "ExchOrdID" etc
#include "Basis/SecDefs.h"                 // For abstract  "SecTrStatusT"
#include "Basis/TimeValUtils.hpp"
#include "Basis/Maths.hpp"
#include "Protocols/FIX/Msgs.h"            // For common FAST/FIX enums
#include "Protocols/FAST/Msgs_FORTS.hpp"
#include <utxx/convert.hpp>
#include <boost/container/static_vector.hpp>
#include <cstdint>
#include <cstddef>                         // For "offsetof"

namespace MAQUETTE
{
namespace FAST
{
namespace FORTS
{
  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetTradingStatus": Converting a num code to abstract "SecTrStatusT":   //
  //-------------------------------------------------------------------------//
  inline SecTrStatusT GetTradingStatus(uint32_t a_trad_stat)
  {
     FIX::SecTrStatusT trStat = FIX::SecTrStatusT(a_trad_stat);

     // XXX: "SecTrStatusT::CancelsOnly" is NOT generated here:
     return
       (trStat == FIX::SecTrStatusT::TradingHalt          ||
        trStat == FIX::SecTrStatusT::NotAvailableForTrading)
       ? SecTrStatusT::NoTrading
       :
       (trStat == FIX::SecTrStatusT::ReadyToTrade)
       ? SecTrStatusT::FullTrading
       : SecTrStatusT::UNDEFINED;
  }

  //=========================================================================//
  // "SecurityDefinition":                                                   //
  //=========================================================================//
  template<>
  constexpr TID SecurityDefinitionTID<ProtoVerT::Curr>() { return 3; }

  //-------------------------------------------------------------------------//
  // "SecurityDefinitionBase_Curr" (without vectors):                        //
  //-------------------------------------------------------------------------//
  struct SecurityDefinitionBase_Curr
  {
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34
    // Total count of SecurityDefinition messages:
    uint32_t          m_TotNumReports;                  // 911

    char              m_Symbol[16];                     // 55
    char              m_SecurityDesc[256];              // 107
    // Unique among all instruments; primary key:
    uint64_t          m_SecurityID;                     // 48

    char              m_SecurityAltID[64];              // 455
    char              m_SecurityAltIDSource[4];         // 456
    char              m_SecurityType[16];               // 167
    char              m_CFICode[16];                    // 461
    Decimal           m_StrikePrice;                    // 202
    Decimal           m_ContractMultiplier;             // 231
    uint32_t          m_SecurityTradingStatus;          // 326
    char              m_Currency[4];                    // 15
    char              m_MarketSegmentID[4];             // 1300
    uint32_t          m_TradingSessionID;               // 336
    uint32_t          m_ExchangeTradingSessionID;       // 5842
    Decimal           m_Volatility;                     // 5678

    // Price constraints:
    Decimal           m_HighLimitPx;                    // 1149
    Decimal           m_LowLimitPx;                     // 1148
    Decimal           m_MinPriceIncrement;              // 969
    Decimal           m_MinPriceIncrementAmount;        // 1146
    Decimal           m_InitialMarginOnBuy;             // 20002
    Decimal           m_InitialMarginOnSell;            // 20000
    Decimal           m_InitialMarginSyntetic;          // 20001
    char              m_QuotationList[64];              // 20005
    Decimal           m_TheorPrice;                     // 20006
    Decimal           m_TheorPriceLimit;                // 20007

    Decimal           m_UnderlyingQty;                  // 879
    char              m_UnderlyingCurrency[4];          // 318
    // UTC Date Only: YYYYMMDD:
    uint32_t          m_MaturityDate;                   // 541
    // UTC Time: HHMMSSsss:
    uint32_t          m_MaturityTime;                   // 1079

    // Counters for Vectors (see below); not really needed, provided for compl-
    // eteness:
    uint32_t          m_NoMDFeedTypes;                  // 1141
    uint32_t          m_NoUnderlyings;                  // 711
    uint32_t          m_NoLegs;                         // 555
    uint32_t          m_NoInstrAttribs;                 // 870
    uint32_t          m_NoEvents;                       // 864

    // Default Ctor:
    SecurityDefinitionBase_Curr() { Init(); }

    // "Init":  Zero-Out the Struct:
    void Init() { memset(this, '\0', sizeof(SecurityDefinitionBase_Curr)); }
  };

  //-------------------------------------------------------------------------//
  // "SecurityDefinition" Itself:                                            //
  //-------------------------------------------------------------------------//
  template<>
  struct SecurityDefinition<ProtoVerT::Curr>: public SecurityDefinitionBase_Curr
  {
    //-----------------------------------------------------------------------//
    // Sequence Entry Types:                                                 //
    //-----------------------------------------------------------------------//
    // XXX: This msg type uses "static_vector" instead of fixed-size arrays to
    //      store Sequences, for the following 2 reasons:
    // (*) there are multiple Sequences, as well as Sequences nested at more
    //     than 1 level, within a msg;   handling them via fixed-size arrays
    //     would be very tedeous and memory-inefficient;
    // (*) processing of "SecurityDefinition" msgs is typically non-performan-
    //     ce-critical (done off-line):
    //-----------------------------------------------------------------------//
    // "MDFeedType":                                                         //
    //-----------------------------------------------------------------------//
    struct MDFeedType
    {
      char            m_MDFeedType[64];                 // 1022
      uint32_t        m_MarketDepth;                    // 264
      uint32_t        m_MDBookType;                     // 1021

      MDFeedType()    { Init(); }
      void Init()     { memset(this, '\0', sizeof(MDFeedType)); }
    };

    // Up to 8 "MDFeedType"s per "SecurityDefinition" are allowed:
    using MDFeedTypesVec = boost::container::static_vector<MDFeedType, 8>;

    //-----------------------------------------------------------------------//
    // "Underlying":                                                         //
    //-----------------------------------------------------------------------//
    struct Underlying
    {
      char            m_UnderlyingSymbol[16];           // 311
      uint64_t        m_UnderlyingSecurityID;           // 309

      Underlying()    { Init(); }
      void Init()     { memset(this, '\0', sizeof(Underlying)); }
    };

    // Up to 8 "Underlying"s per "MarketSegment" are currently allowed:
    using UnderlyingsVec = boost::container::static_vector<Underlying, 8>;

    //-----------------------------------------------------------------------//
    // "InstrLeg":                                                           //
    //-----------------------------------------------------------------------//
    struct InstrLeg
    {
      char            m_LegSymbol[64];                  // 600
      uint64_t        m_LegSecurityID;                  // 602
      Decimal         m_LegRatioQty;                    // 623

      InstrLeg()      { Init(); }
      void Init()     { memset(this, '\0', sizeof(InstrLeg)); }
    };

    // Up to 8 "InstrLeg"s per "SecurityDefinition" are allowed:
    using InstrLegsVec = boost::container::static_vector<InstrLeg, 8>;

    //-----------------------------------------------------------------------//
    // "InstrAttrib":                                                        //
    //-----------------------------------------------------------------------//
    struct InstrAttrib
    {
      int32_t         m_InstrAttribType;                // 871
      char            m_InstrAttribValue[64];           // 872

      InstrAttrib()   { Init(); }
      void Init()     { memset(this, '\0', sizeof(InstrAttrib)); }
    };

    // Up to 8 "InstrAttrib"s per "SecurityDefinition" are allowed:
    using InstrAttribsVec = boost::container::static_vector<InstrAttrib, 8>;

    //-----------------------------------------------------------------------//
    // "Event":                                                              //
    //-----------------------------------------------------------------------//
    struct Event
    {
      int32_t         m_EventType;                      // 865
      uint32_t        m_EventDate;                      // 866
      uint64_t        m_EventTime;                      // 1145

      Event()         { Init(); }
      void Init()     { memset(this, '\0', sizeof(Event)); }
    };

    // Up to 8 "Event"s per "SecurityDefinition" are allowed:
    using EventsVec = boost::container::static_vector<Event, 8>;

    //-----------------------------------------------------------------------//
    // Counters and Vectors:                                                 //
    //-----------------------------------------------------------------------//
    // NB: The following counters do not really need to be stored in the msg
    // because "static_vector" sizes are known anyway  -- but we still store
    // the counters for ease of msg de-coding:
    //
    MDFeedTypesVec    m_MDFeedTypes;

    UnderlyingsVec    m_Underlyings;

    InstrLegsVec      m_InstrLegs;

    InstrAttribsVec   m_InstrAttribs;

    EventsVec         m_Events;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    SecurityDefinition()
    : SecurityDefinitionBase_Curr(),
      m_MDFeedTypes     (),
      m_Underlyings     (),
      m_InstrAttribs    (),
      m_Events          ()
    {}

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );
  };

  //=========================================================================//
  // "SecurityDefinitionUpdate":                                             //
  //=========================================================================//
  // (Only updating Vols and TheorPxs of Options; XXX no updates for Futures?):
  template<>
  constexpr TID SecurityDefinitionUpdateTID<ProtoVerT::Curr>() { return 4; }

  template<>
  struct SecurityDefinitionUpdate<ProtoVerT::Curr>
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34
    uint64_t          m_SecurityID;                     // 48
    Decimal           m_Volatility;                     // 5678
    Decimal           m_TheorPrice;                     // 20006
    Decimal           m_TheorPriceLimit;                // 20007

    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-Out the Struct:                                    //
    //-----------------------------------------------------------------------//
    SecurityDefinitionUpdate()  { Init(); }
    void Init() { memset(this, '\0', sizeof(SecurityDefinitionUpdate)); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );
  };

  //=========================================================================//
  // "SecurityStatus":                                                       //
  //=========================================================================//
  // Actually, this is another dynamic update to "SecurityDefinition", incl
  // Futures:
  template<>
  constexpr TID SecurityStatusTID<ProtoVerT::Curr>() { return 5; }

  template<>
  struct SecurityStatus<ProtoVerT::Curr>
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34
    uint64_t          m_SecurityID;                     // 48

    char              m_Symbol[16];                     // 55
    uint32_t          m_SecurityTradingStatus;          // 326
    Decimal           m_HighLimitPx;                    // 1149
    Decimal           m_LowLimitPx;                     // 1148

    Decimal           m_InitialMarginOnBuy;             // 20002
    Decimal           m_InitialMarginOnSell;            // 20000
    Decimal           m_InitialMarginSyntetic;          // 20001

    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-Out the Struct:                                    //
    //-----------------------------------------------------------------------//
    SecurityStatus()  { Init(); }
    void Init()       { memset(this, '\0', sizeof(SecurityStatus)); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );
  };

  //=========================================================================//
  //  "HeartBeat":                                                           //
  //=========================================================================//
  // (Recognised but Ignored):
  template<>
  constexpr TID HeartBeatTID<ProtoVerT::Curr>() { return 6; }

  template<>
  struct HeartBeat<ProtoVerT::Curr>
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34

    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-Out the Struct:                                    //
    //-----------------------------------------------------------------------//
    HeartBeat()       { Init(); }
    void Init()       { memset(this, '\0', sizeof(HeartBeat)); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );
  };

  //=========================================================================//
  // "SequenceReset":                                                        //
  //=========================================================================//
  template<>
  constexpr TID SequenceResetTID<ProtoVerT::Curr>() { return 7; }

  template<>
  struct SequenceReset<ProtoVerT::Curr>
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34
    uint32_t          m_NewSeqNo;                       // 36

    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-Out the Struct:                                    //
    //-----------------------------------------------------------------------//
    SequenceReset()   { Init(); }
    void Init()       { memset(this, '\0', sizeof(SequenceReset)); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );
  };

  //=========================================================================//
  // "TradingSessionStatus":                                                 //
  //=========================================================================//
  template<>
  constexpr TID TradingSessionStatusTID<ProtoVerT::Curr>() { return 8; }

  template<>
  struct TradingSessionStatus<ProtoVerT::Curr>
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34

    uint64_t          m_TradSesOpenTime;                // 342
    uint64_t          m_TradSesCloseTime;               // 344
    uint64_t          m_TradSesIntermClearingStartTime; // 5840
    uint64_t          m_TradSesIntermClearingEndTime;   // 5841

    uint32_t          m_TradingSessionID;               // 336
    uint32_t          m_ExchangeTradingSessionID;       // 5842
    uint32_t          m_TradSesStatus;                  // 340
    char              m_MarketSegmentID[16];            // 1300
    int32_t           m_TradSesEvent;                   // 1368

    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-Out the Struct:                                    //
    //-----------------------------------------------------------------------//
    TradingSessionStatus()   { Init(); }
    void Init() { memset(this, '\0', sizeof(TradingSessionStatus)); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );
  };

  //=========================================================================//
  // "News":                                                                 //
  //=========================================================================//
  template<>
  constexpr TID NewsTID<ProtoVerT::Curr>() { return 9; }

  template<>
  struct News<ProtoVerT::Curr>
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34
    uint32_t          m_LastFragment;                   // 893

    char              m_NewsID[16];                     // 1472
    uint64_t          m_OrigTime;                       // 42
    char              m_LanguageCode[16];               // 1474
    uint32_t          m_Urgency;                        // 61
    char              m_HeadLine[128];                  // 148
    char              m_MarketSegmentID[16];            // 1300

    // NB: We concatenate all lines (deliminated by '\n') into a single Text
    // block:
    uint32_t          m_NoLinesOfText;                  // 33
    char              m_Text[8192];                     // 58

    //-----------------------------------------------------------------------//
    // Deafault Ctor: Zero-Out the Struct:                                   //
    //-----------------------------------------------------------------------//
    News()            { Init(); }
    void Init()       { memset(this, '\0', sizeof(News)); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    // XXX: Currently Not Implemented -- declaration only!
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );
  };

  //=========================================================================//
  // Masks for Filtering Out and In of OrdersLog MDEntries:                  //
  //=========================================================================//
  // Filter-Out the following:
  constexpr long InapprOBMask =
    0x2             // IOC Order -- will NOT come into the OrderBook
  | 0x4             // OTC Order or Trade (separate OrderBook)?
  | 0x8             // Position-Transfer Trade
  | 0x20            // Option Exercise Trade (NB: FORTS Opts are American!)
  | 0x80            // Instrument Expiration (Opt or Fut)
  | 0x20000         // REPO Trade
  | 0x40000         // Multiple Trades in one MDE   (???)
  | 0x80000         // FOK  Order -- will NOT come into the OrderBook
  | 0x800000        // Option Expiration Trade
  | 0x2000000       // Clearing-Session Trade (XXX: how is it possible?)
  | 0x4000000       // Negotiated Trade (REPO only?)
  | 0x8000000       // Multi-Leg Trade  (separate OrderBook!)
  | 0x10000000      // Trade on Non-Delivery  (XXX: what is it???)
  | 0x40000000;     // Futures Exercise Trade

  // Filter-In the following:
  constexpr long TransEndMask = 0x1000;

  //=========================================================================//
  // "OrdersLogIncrRefresh":                                                 //
  //=========================================================================//
  template<>
  constexpr TID OrdersLogIncrRefreshTID<ProtoVerT::Curr>() { return 14; }

  template<>
  struct OrdersLogIncrRefresh<ProtoVerT::Curr>
  {
    //=======================================================================//
    // "OrdersLogIncrRefresh::MDEntry":                                      //
    //=======================================================================//
    struct MDEntry
    {
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      uint32_t        m_MDUpdateAction;                 // 279
      char            m_MDEntryType[4];                 // 269
      int64_t         m_MDEntryID;                      // 278
      uint64_t        m_SecurityID;                     // 48
      uint32_t        m_RptSeq;                         // 83
      uint32_t        m_MDEntryDate;                    // 272
      uint64_t        m_MDEntryTime;                    // 273
      Decimal         m_MDEntryPx;                      // 270
      int64_t         m_MDEntrySize;                    // 271
      Decimal         m_LastPx;                         // 31
      int64_t         m_LastQty;                        // 32
      int64_t         m_TradeID;                        // 1003
      uint32_t        m_ExchangeTradingSessionID;       // 5842
      int64_t         m_MDFlags;                        // 20017
      uint64_t        m_Revision;                       // 20018

      //---------------------------------------------------------------------//
      // Accessors:                                                          //
      //---------------------------------------------------------------------//
      // "GetUpdateAction": XXX: "MDUpdateAction" is Encoded as "int" here but
      // as "char" in FIX, so:
      FIX::MDUpdateActionT GetUpdateAction() const
        { return FIX::MDUpdateActionT(m_MDUpdateAction + '0'); }

      // "GetEntryType" (for OrderBooks, may also designate the Side):
      FIX::MDEntryTypeT GetEntryType      () const
        { return FIX::MDEntryTypeT(m_MDEntryType[0]); }

      // "GetTradeAggrSide": XXX: Although the Aggressor is not directly known,
      // it would be on the opposite side of the OrderBook, so:
      FIX::SideT GetTradeAggrSide() const
      {
        return
          (m_TradeID != 0 && m_MDEntryType[0] == '0')
          // Bid Side updated => Aggr is Ask:
          ? FIX::SideT::Sell :
          (m_TradeID != 0 && m_MDEntryType[0] == '1')
          // Ask Side updated => Aggr is Bid
          ? FIX::SideT::Buy
          // In all other cases, it is undefined or unknown:
          : FIX::SideT::UNDEFINED;
      }

      // "GetTradeExecID":    XXX: Returning a copy of the "ExchOrdID" obj:
      ExchOrdID GetTradeExecID() const { return ExchOrdID(OrderID(m_TradeID)); }

      // "GetTradeSettlDate": Not available in FORTS:
      constexpr static int GetTradeSettlDate() { return 0; }

      // "WasTradedFOL":
      bool WasTradedFOL()     const { return (m_TradeID != 0);     }

      // "GetMDEntryID":
      OrderID GetMDEntryID()  const { return OrderID(m_MDEntryID); }

      // "GetEntryRefID": No RefID here:
      constexpr static OrderID GetEntryRefID()  { return 0; }

      // "GetEntrySize":  NB: Strongly-Typed!
      QtyF   GetEntrySize()   const { return QtyF(m_MDEntrySize);  }

      // "GetEntryPx":
      PriceT GetEntryPx()     const { return PriceT(m_MDEntryPx.m_val); }

      // Is this MDE a valid OrderBook (and possibly a Trade) update? -- Need
      // to filter out the unrelated updates. The following criteria apply:
      // (1) If it is a "New" one, it must NOT be a Trade (but it may be such
      //     if it is a "Delete" or "Change"), and it must be End-of-Transact
      //     (to exclude NewOrders which are matched immediately and do not ac-
      //     tually get to the OrderBook);
      // (3) It must NOT otherwise be Inappropriate (eg an OTC or Negotiated
      //     Order):
      bool IsValidUpdate()  const
      {
        bool notNew     =  (m_MDUpdateAction != 0);
        bool notTrade   =  (m_TradeID        == 0);   // !WasTradedFOL
        bool isTransEnd = ((m_MDFlags & TransEndMask) != 0);
        bool isAppropr  = ((m_MDFlags & InapprOBMask) == 0);

        return  (notNew || (notTrade && isTransEnd)) && isAppropr;
      }
    };

    //=======================================================================//
    // "OrdersLogIncrRefresh" Itself:                                        //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34
    uint32_t          m_LastFragment;                   // 893
    uint32_t          m_NoMDEntries;                    // 268

    // Fixed-size buffer for "MDEntry"s. Must be m_NoMDEntries <= MaxMDEs:
    constexpr static int MaxMDEs = 128;
    MDEntry           m_MDEntries[MaxMDEs];

    //-----------------------------------------------------------------------//
    // Ctor and Accessors:                                                   //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    OrdersLogIncrRefresh() { Init(); }

    // "Init":
    // Zero-Out the whole object, except the "LastFragment" flag which must be
    // set to 1 by default:
    void Init()
    {
      memset(this, '\0', sizeof(OrdersLogIncrRefresh));
      m_LastFragment = 1;
    }

    // "InitFixed":
    // Similar to "Init", but only initialises the "fixed part" of the obj (be-
    // fore "m_MDEntries"):
    void InitFixed()
    {
      memset(this, '\0', offsetof(OrdersLogIncrRefresh, m_MDEntries));
      m_LastFragment = 1;
    }

    // "InitVar":
    // Initialises only the "variable part" ofthe obj (MDEntries array):
    void InitVar()
      { memset(m_MDEntries, '\0', m_NoMDEntries * sizeof(MDEntry));     }

    // "GetNEntries":
    int GetNEntries()              const   { return int(m_NoMDEntries); }

    // "GetEntry":
    MDEntry const& GetEntry(int i) const
    {
      assert(0 <= i && i < int(m_NoMDEntries) && m_NoMDEntries <= MaxMDEs);
      return m_MDEntries[i];
    }

    // "GetMDSubscrID":
    // FAST is based on UDP multicast, without any explicit susbcriptions for
    // individual instrs; hence the returned value is always 0:
    constexpr static OrderID    GetMDSubscrID()    { return 0; }

    // "IsLastFragment":
    // Whether this msg is the last one in a series of LOGICAL updates (in that
    // case, presumably of the same Symbol): Normally yes:
    //
    bool IsLastFragment() const { return bool(m_LastFragment); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );

    //-----------------------------------------------------------------------//
    // Accessors using both "OrdersLogIncrRefresh" and its "MDEntry":        //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    // No Symbol here -- only the SecID is available. XXX: Maybe even return a
    // NULL?
    constexpr static char const* GetSymbol (MDEntry const&) { return ""; }

    // "GetSecID":
    // In FORTS, numerical SecID is really transmitted in "OrdersLogIncrRefresh"
    // msgs, ie in "MDEntry"es:
    //
    static SecID  GetSecID          (MDEntry const& a_mde)
    {
      static_assert(!::MAQUETTE::FORTS::UseTenorsInSecIDs);
      return a_mde.m_SecurityID;
    }

    // "GetRptSeq":
    static SeqNum GetRptSeq         (MDEntry const& a_mde)
      { return a_mde.m_RptSeq; }

    // "GetEventTS":
    static utxx::time_val GetEventTS(MDEntry const& a_mde)
    {
      // XXX: No microsecond-level resolution in FORTS:
      return DateTimeToTimeValFAST(GetCurrDate(), int(a_mde.m_MDEntryTime), 0);
    }
  };
  // End of "OrdersLogIncrRefresh"

  //=========================================================================//
  // "OrdersLogSnapShot":                                                    //
  //=========================================================================//
  template<>
  constexpr TID OrdersLogSnapShotTID<ProtoVerT::Curr>() { return 15; }

  template<>
  struct OrdersLogSnapShot<ProtoVerT::Curr>
  {
    //=======================================================================//
    // "OrdersLogSnapShot::MDEntry":                                         //
    //=======================================================================//
    struct MDEntry
    {
      //---------------------------------------------------------------------//
      // Data Flds:                                                          //
      //---------------------------------------------------------------------//
      char            m_MDEntryType[4];                 // 269
      int64_t         m_MDEntryID;                      // 278
      uint32_t        m_MDEntryDate;                    // 272
      uint64_t        m_MDEntryTime;                    // 273
      Decimal         m_MDEntryPx;                      // 270
      int64_t         m_MDEntrySize;                    // 271
      int64_t         m_TradeID;                        // 1003
      int64_t         m_MDFlags;                        // 20017

      //---------------------------------------------------------------------//
      // Accessors:                                                          //
      //---------------------------------------------------------------------//
      // "GetUpdateAction": For SnapShot MDEs, it's always "New":
      FIX::MDUpdateActionT GetUpdateAction() const
        { return FIX::MDUpdateActionT::New; }

      // "GetEntryType" (for OrderBooks, may also designate the Side):
      FIX::MDEntryTypeT GetEntryType      () const
        { return FIX::MDEntryTypeT(m_MDEntryType[0]); }

      // "GetTradeAggrSide": XXX: Although the Aggressor is not directly known,
      // it would be on the opposite side of the OrderBook  --  but NOT really
      // used for SnapShots:
      FIX::SideT GetTradeAggrSide() const
      {
        return
          (m_TradeID != 0 && m_MDEntryType[0] == '0')
          // Bid Side updated => Aggr is Ask:
          ? FIX::SideT::Sell :
          (m_TradeID != 0 && m_MDEntryType[0] == '1')
          // Ask Side updated => Aggr is Bid
          ? FIX::SideT::Buy
          // In all other cases, it is undefined or unknown:
          : FIX::SideT::UNDEFINED;
      }

      // "GetTradeExecID":
      ExchOrdID GetTradeExecID() const { return ExchOrdID(OrderID(m_TradeID)); }

      // "GetTradeSettlDate": Not available here:
      constexpr static int GetTradeSettlDate()         { return 0; }

      // "WasTradedFOL":
      bool WasTradedFOL()     const { return (m_TradeID != 0);     }

      // "GetMDEntryID":
      OrderID GetMDEntryID()  const { return OrderID(m_MDEntryID); }

      // "GetEntryRefID": No RefID here:
      constexpr static OrderID GetEntryRefID() { return 0; }

      // "GetEntrySize":  NB: Stronly-Typed!
      QtyF GetEntrySize()     const { return QtyF(m_MDEntrySize);  }

      // "GetEntryPx":
      PriceT GetEntryPx()     const { return PriceT(m_MDEntryPx.m_val); }

      // "WasTraded" (though it is irrelevant in case of SnapShots):
      bool WasTraded()        const { return (m_TradeID != 0);  }

      // Is this MDE a valid OrderBook (and possibly Trade) update? -- Need to
      // filter out the unrelated updates. The following criteria apply:
      // (1) Since a SnapShot MDE is always a "New" one, it must NOT be a Trade;
      // (2) It must NOT otherwise be Inappropriate (eg an OTC or Negotiated
      //     Order).
      // NB: End-of-Transaction flag is NOT required for a SnapShot:
      //
      bool IsValidUpdate()  const
        { return m_TradeID == 0 && (m_MDFlags & InapprOBMask) == 0; }
    };

    //=======================================================================//
    // "OrdersLogSnapShot" Itself:                                           //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t   s_SendingTime;                    // 52: XXX: Skipped!
    uint32_t          m_MsgSeqNum;                      // 34
    uint32_t          m_LastMsgSeqNumProcessed;         // 369
    uint32_t          m_RptSeq;                         // 83
    uint32_t          m_LastFragment;                   // 893
    uint32_t          m_RouteFirst;                     // 7944
    uint32_t          m_ExchangeTradingSessionID;       // 5842
    uint64_t          m_SecurityID;                     // 48
    uint32_t          m_NoMDEntries;                    // 268

    // Fixed-size buffer for "MDEntry"s. Must be m_NoMDEntries <= MaxMDEs:
    constexpr static int MaxMDEs = 128;
    MDEntry           m_MDEntries[MaxMDEs];

    //-----------------------------------------------------------------------//
    // Ctor and Accessors:                                                   //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    OrdersLogSnapShot() { Init(); }

    // "Init":
    // Zero-Out the whole object, except the "LastFragment" flag which must be
    // set to 1 by default:
    void Init()
    {
      memset(this, '\0', sizeof(OrdersLogSnapShot));
      m_LastFragment = 1;
    }

    // "InitFixed":
    // Similar to "Init", but only initialises the "fixed part" of the obj (be-
    // fore "m_MDEntries"):
    void InitFixed()
    {
      memset(this, '\0', offsetof(OrdersLogSnapShot, m_MDEntries));
      m_LastFragment = 1;
    }

    // "InitVar":
    // Initialises only the "variable part" ofthe obj (MDEntries array):
    void InitVar()
      { memset(m_MDEntries, '\0', m_NoMDEntries * sizeof(MDEntry));   }

    // "GetNEntries":
    int GetNEntries()              const   { return int(m_NoMDEntries); }

    // "GetEntry":
    MDEntry const& GetEntry(int i) const
    {
      assert(0 <= i && i < int(m_NoMDEntries) && m_NoMDEntries <= MaxMDEs);
      return m_MDEntries[i];
    }

    // "GetMDSubscrID":
    // FAST is based on UDP multicast, without any explicit susbcriptions for
    // individual instrs; hence the returned value is always 0:
    constexpr static OrderID GetMDSubscrID()  { return 0; }

    // "IsLastFragment":
    // Whether this msg is the last one in a series of LOGICAL updates (in that
    // case, presumably of the same Symbol): Normally yes:
    //
    bool IsLastFragment()  const { return bool(m_LastFragment); }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );

    //-----------------------------------------------------------------------//
    // Accessors using both "OrdersLogSnapShot" and its "MDEntry":           //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    // No Symbol here -- only the SecID is available. XXX: Maybe even return a
    // NULL?
    constexpr static char const* GetSymbol(MDEntry const&) { return ""; }

    // "GetSecID":
    // In FORTS, numerical SecID is really transmitted in "*SnapShot" msgs:
    //
    SecID  GetSecID (MDEntry const&) const
    {
      static_assert(!::MAQUETTE::FORTS::UseTenorsInSecIDs);
      return m_SecurityID;
    }

    // "GetRptSeq":
    SeqNum GetRptSeq(MDEntry const&) const   { return m_RptSeq; }

    // "GetEventTS":
    static utxx::time_val GetEventTS(MDEntry const& a_mde)
    {
      // XXX: No microsecond-level resolution in FORTS:
      return DateTimeToTimeValFAST(GetCurrDate(), int(a_mde.m_MDEntryTime), 0);
    }
  };
  // End of "OrdersLogSnapShot"
} // End namespace FORTS
} // End namespace FAST
} // End namespace MAQUETTE
