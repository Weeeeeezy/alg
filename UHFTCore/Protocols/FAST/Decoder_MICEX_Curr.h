// vim:ts=2:et
//===========================================================================//
//                  "Prococols/FAST/Decoder_MICEX_Curr.h":                   //
//  Types and Decoding Functions for FAST Messages for MICEX Equity and FX   //
//             For the "Curr" MICEX FAST Ver (as of 2017-Mar-30)             //
//===========================================================================//
#pragma  once

#include "Basis/BaseTypes.hpp"             // For "SeqNum", "ExchOrdID" etc
#include "Basis/TimeValUtils.hpp"
#include "Basis/Maths.hpp"
#include "Protocols/FIX/Msgs.h"            // For common FAST/FIX enums
#include "Protocols/FAST/Msgs_MICEX.hpp"
#include <utxx/convert.hpp>
#include <boost/container/static_vector.hpp>
#include <cstdint>
#include <cstddef>                         // For "offsetof"

namespace MAQUETTE
{
namespace FAST
{
namespace MICEX
{
  using AssetClassT = ::MAQUETTE::MICEX::AssetClassT;

  //=========================================================================//
  // NOTES:                                                                  //
  //=========================================================================//
  // XXX: Not Implemented (as not required at the moment):
  // ID=2101: Logon
  // ID=2102: Logout
  // ID=2523: MSR   -- Incremental Refresh, Statistics, EQ
  // ID=3613: MSR   -- Incremental Refresh, Statistics, FX
  // ID=2523: OB_EQ -- Incremental Refresh, OrderBook,  EQ
  // ID=3613: OB_FX -- Incremental Refresh, OrderBook,  FX
  //
  // In all Msgs below, "SendingTime" (52) has the following 18-digit format:
  // YYMMDDHHmmSSuuuuuu, but at the moment, XXX it is "Skip"ped everywhere!
  //
  //=========================================================================//
  // "MDEntryIncr" (for FullOrdersLog IncrementalRefresh):                   //
  //=========================================================================//
  // Same for both FX and EQ -- the difference between them is in Unused (Stat-
  // ic) Flds, so we can use the same type w/o any performance impact:
  //
  struct MDEntryIncr
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    uint32_t              m_MDUpdateAction;               // 279
    char                  m_MDEntryType[4];               // 269
    char                  m_MDEntryID  [16];              // 278
    char                  m_Symbol     [16];              // 55
    int32_t               m_RptSeq;                       // 83
    uint32_t              m_MDEntryDate;                  // 272
    uint32_t              m_MDEntryTime;                  // 273
    uint32_t              m_OrigTime;                     // 9412: usec
    Decimal               m_MDEntryPx;                    // 270
    Decimal               m_MDEntrySize;                  // 271
    char                  m_TradingSessionID[16];         // 336

    //-----------------------------------------------------------------------//
    // Unused (static) Data Flds:                                            //
    //-----------------------------------------------------------------------//
    static Decimal        s_Yield;                        // 236 : EQ only
    static char           s_OrdType[4];                   // 40  : EQ only
    static Decimal        s_TotalVolume;                  // 5791: EQ ony
    static Decimal        s_Price;                        // 44  : EQ only
    static char           s_OrderStatus[4];               // 10505
    static char           s_TradingSessionSubID[16];      // 625

    // Default Ctor is auto-generated

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    // "GetUpdateAction":
    // XXX: "MDUpdateAction" is Encoded as "int" here but as "char" in FIX, so:
    FIX::MDUpdateActionT GetUpdateAction() const
      { return FIX::MDUpdateActionT(m_MDUpdateAction + '0');   }

    // "GetEntryType" (for OrderBooks, may also designate the Side):
    FIX::MDEntryTypeT GetEntryType      () const
      { return FIX::MDEntryTypeT(m_MDEntryType[0]); }

    // "GetMDEntryID":
    // MDEntryID is a digital string, convert it into a numeric OrderID:
    OrderID GetMDEntryID() const
    {
      OrderID res = 0;
      (void) utxx::fast_atoi<OrderID, false>
             (m_MDEntryID, m_MDEntryID + sizeof(m_MDEntryID), res);
      return res;
    }

    // "GetEntrySize":
    // Although represented as Decimal, we assume that for FAST MICEX the Qtys
    // are Integral Lots (as specified in our "QtyF" type). So an exception is
    // thrown here if we actually get a fractional Qty:
    //
    QtyF GetEntrySize() const { return QtyF(m_MDEntrySize.m_val); }

    // "GetEntryPx":
    PriceT GetEntryPx() const { return PriceT(m_MDEntryPx.m_val); }

    //-----------------------------------------------------------------------//
    // The following Accessors are trivial on MICEX:                         //
    //-----------------------------------------------------------------------//
    // "GetEntryRefID":
    constexpr static OrderID   GetEntryRefID()  { return 0; }

    // Is this MDE a valid OrderBook update? Generally yes (provided that it
    // comes from a correct Channel, of course!) -- there are  no particular
    // invalidating factors, unlike FORTS:
    constexpr static bool      IsValidUpdate()  { return true;  }

    // Is it a Trade inferred from the FullOrdersLog? -- No, in MICEX we cannot
    // do that (because client-initiated Delete is no distibguishable from a
    // Fill):
    constexpr static bool      WasTradedFOL()   { return false; }

    // NB: "MDEntryIncr" on MICEX is NOT used for Trades (see "MDEntryTrade"
    // instead), so any Trade-related quieries must raise an exception:
    //
    // "GetTradeExecID":
    static ExchOrdID  GetTradeExecID()
      { throw utxx::badarg_error
              ("FAST::MICEX::MDEntryIncr::GetTradeExecID: Not a Trade");    }

    // "GetTradeSettlDate":
    static int        GetTradeSettlDate()
      { throw utxx::badarg_error
              ("FAST::MICEX::MDEntryIncr::GetTradeSettlDate: Not a Trade"); }

    // "GetTradeAggrSide":
    static FIX::SideT GetTradeAggrSide()
      { throw utxx::badarg_error
              ("FAST::MICEX::MDEntryIncr::GetTradeAggrSide:  Not a Trade"); }
  };

  //=========================================================================//
  // "MDEntryTrade":                                                         //
  //=========================================================================//
  // Extension of "MDEntryIncr": Contains extra flds for Trade info. Again, the
  // difference between EQ and FX is in Unused (Static) flds only,   so we  can
  // use the same type for both AssetClassTs. XXX: Cannot make "MDEntryTrade" a
  // formal extension of "MDEntryIncr" for layout reasons ("offsetof" would not
  // work then):
  //
  struct MDEntryTrade
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // Similar to "MDEntryIncr":
    uint32_t              m_MDUpdateAction;               // 279
    char                  m_MDEntryType[4];               // 269
    char                  m_MDEntryID  [16];              // 278
    char                  m_Symbol     [16];              // 55
    int32_t               m_RptSeq;                       // 83
    uint32_t              m_MDEntryDate;                  // 272
    uint32_t              m_MDEntryTime;                  // 273
    uint32_t              m_OrigTime;                     // 9412: usec
    Decimal               m_MDEntryPx;                    // 270
    Decimal               m_MDEntrySize;                  // 271
    char                  m_TradingSessionID[16];         // 336
    // Extra Flds specific to "MDEntryTrade":
    char                  m_OrderSide [4];                // 10504
    uint32_t              m_SettlDate;                    // 64
    char                  m_RefOrderID[16];               // 1080

    //-----------------------------------------------------------------------//
    // Unused (Static) Data Flds -- for Type Info only:                      //
    //-----------------------------------------------------------------------//
    // Similar to "MDEntryIncr":
    static Decimal        s_Yield;                        // 236 : EQ only
    static char           s_OrdType[4];                   // 40  : EQ only
    static Decimal        s_TotalVolume;                  // 5791: EQ ony
    static Decimal        s_Price;                        // 44  : EQ only
    static char           s_OrderStatus[4];               // 10505
    static char           s_TradingSessionSubID[16];      // 625
    // Extra static Flds specific to "MDEntryTrade":
    static Decimal        s_AccruedInterestAmt;           // 5384; EQ only
    static char           s_SettleType[4];                // 5459
    static Decimal        s_TradeValue;                   // 6143
    static int32_t        s_PriceType;                    // 423
    static Decimal        s_RepoToPx;                     // 5677
    static Decimal        s_BuyBackPx;                    // 5558
    static uint32_t       s_BuyBackDate;                  // 5559

    // Default Ctor is auto-generated

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    // "GetUpdateAction":
    // XXX: "MDUpdateAction" is Encoded as "int" here but as "char" in FIX, so:
    FIX::MDUpdateActionT GetUpdateAction() const
      { return FIX::MDUpdateActionT(m_MDUpdateAction + '0');   }

    // "GetEntryType" (for OrderBooks, may also designate the Side):
    FIX::MDEntryTypeT GetEntryType      () const
      { return FIX::MDEntryTypeT(m_MDEntryType[0]); }

    // "GetMDEntryID":
    // XXX: For a Trade, we must NOT return "MDEntryID" here,  because that is
    // actually an ExecID (to be returned by "GetTradeExecID"). So return 0:
    constexpr static OrderID GetMDEntryID() { return 0; }

    // "GetEntryRefID":
    // For a Trade, it returns "RefOrderID" which is the MDEntryID of the Pass-
    // ive Order which was hit or lifted by this Trade:
    // RefOrderID is a digital string, convert it into a numeric OrderID:
    OrderID GetEntryRefID() const
    {
      OrderID res = 0;
      (void) utxx::fast_atoi<OrderID, false>
             (m_RefOrderID, m_RefOrderID + sizeof(m_RefOrderID), res);
      return res;
    }

    // "GetEntrySize": Again, return our QtyF:
    QtyF GetEntrySize()  const    { return QtyF(m_MDEntrySize.m_val); }

    // "GetEntryPx":
    PriceT GetEntryPx()  const    { return PriceT(m_MDEntryPx.m_val); }

    // "GetTradeAggrSide":
    FIX::SideT      GetTradeAggrSide() const
    {
      return
        (m_OrderSide[0] == 'B')
        ? FIX::SideT::Buy :
        (m_OrderSide[0] == 'S')
        ? FIX::SideT::Sell
        : FIX::SideT::UNDEFINED;
    }

    // "GetTradeExecID":
    // For a MICEX Trade, ExecID is given by the "MDEntryID" which is a digital
    // string:
    ExchOrdID GetTradeExecID() const
    {
      OrderID res = 0;
      (void) utxx::fast_atoi<OrderID, false>
             (m_MDEntryID, m_MDEntryID + sizeof(m_MDEntryID), res);
      return ExchOrdID(res);
    }

    // "GetTradeSettlDate":
    int GetTradeSettlDate()    const { return int(m_SettlDate);        }
  };

  //=========================================================================//
  // "MDEntrySnap":                                                          //
  //=========================================================================//
  // Similar to "MDEntryIncr" and "MDEntryTrade", but for SnapShots. Some flds
  // are for EQ only, but again, these are Unused (Static) flds, so we can use
  // the same "MDEntrySnap" struct for both EQ and FX w/o any performance imp-
  // act:
  struct MDEntrySnap
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    char                  m_MDEntryType[4];             // 269
    char                  m_MDEntryID[16];              // 278
    uint32_t              m_MDEntryDate;                // 272
    uint32_t              m_MDEntryTime;                // 273
    uint32_t              m_OrigTime;                   // 9412: usec
    Decimal               m_MDEntryPx;                  // 270
    Decimal               m_MDEntrySize;                // 271

    //-----------------------------------------------------------------------//
    // Unused (Static) Flds:                                                 //
    //-----------------------------------------------------------------------//
    static Decimal        s_Yield;                      // 236 : EQ only
    static char           s_OrderStatus[16];            // 10505
    static char           s_OrdType[4];                 // 40  : EQ only
    static Decimal        s_TotalVolume;                // 5791: EQ only
    static char           s_TradingSessionSubID[16];    // 625

    // Default Ctor is auto-generated

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    // "GetUpdateAction": For SnapShot MDEs, it's always "New":
    FIX::MDUpdateActionT GetUpdateAction() const
      { return FIX::MDUpdateActionT::New;   }

    // "GetEntryType" (for OrderBooks, may also designate the Side):
    FIX::MDEntryTypeT GetEntryType      () const
      { return FIX::MDEntryTypeT(m_MDEntryType[0]); }

    // "GetMDEntryID":
    // MDEntryID is a digital string, convert it into a numeric OrderID:
    OrderID GetMDEntryID() const
    {
      OrderID res = 0;
      (void) utxx::fast_atoi<OrderID, false>
             (m_MDEntryID, m_MDEntryID + sizeof(m_MDEntryID), res);
      return res;
    }

    // "GetEntrySize": Again, return our QtyF:
    QtyF GetEntrySize() const { return QtyF(m_MDEntrySize.m_val); }

    // "GetEntryPx":
    PriceT GetEntryPx() const { return PriceT(m_MDEntryPx.m_val); }

    //-----------------------------------------------------------------------//
    // The following Accessors are trivial on MICEX:                         //
    //-----------------------------------------------------------------------//
    // "GetEntryRefID":
    constexpr static OrderID   GetEntryRefID()  { return 0; }

    // Is this MDE a valid OrderBook or Trade update? Generally yes -- again,
    // there are no particular invaliadting factors:
    constexpr static bool      IsValidUpdate()  { return true;  }

    // "WasTradedFOL": Again, in MICEX we currenly do NOT infer trades from FOL:
    constexpr static bool      WasTradedFOL()   { return false; }

    // NB: "MDEntrySnap" on MICEX is NOT used for Trades (see "MDEntryTrade"
    // instead), so any Trade-related quieries must raise an exception:
    //
    // "GetTradeExecID":
    static ExchOrdID  GetTradeExecID()
      { throw utxx::badarg_error
              ("FAST::MICEX::MDEntrySnap::GetTradeExecID: Not a Trade");    }

    // "GetTradeSettlDate":
    static int        GetTradeSettlDate()
      { throw utxx::badarg_error
              ("FAST::MICEX::MDEntrySnap::GetTradeSettlDate: Not a Trade"); }

    // "GetTradeAggrSide":
    static FIX::SideT GetTradeAggrSide()
      { throw utxx::badarg_error
              ("FAST::MICEX::MDEntryIncr::GetTradeAggrSide:  Not a Trade"); }
  };

  //=========================================================================//
  // "IncrementalRefresh" for FullOrdersLog:                                 //
  //=========================================================================//
  // ID=2520: OL_EQ,
  // ID=3610: OL_FX:
  //
  template<>
  struct IncrementalRefresh<ProtoVerT::Curr>
  {
    using MDEntry = MDEntryIncr;

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t       s_SendingTime;                  // 52: XXX: Skipped!
    uint32_t              m_MsgSeqNum;                    // 34
    uint32_t              m_NoMDEntries;                  // 268

    // Fixed-size buffer for "MDEntry"s. Must be m_NoMDEntries <= MaxMDEs:
    constexpr static int  MaxMDEs = 128;
    MDEntry               m_MDEntries[MaxMDEs];

    //-----------------------------------------------------------------------//
    // Ctor and Accessors:                                                   //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    IncrementalRefresh() { Init(); }

    // "Init":
    // Zero-out the whole object:
    void Init()      { memset(this, '\0', sizeof(IncrementalRefresh)); }

    // "InitFixed":
    // Zero-Out only the "fixed part" of the obj (before "m_MDEntries"):
    void InitFixed()
      { memset(this, '\0', offsetof(IncrementalRefresh, m_MDEntries)); }

    // "InitVar":
    // Zero-Out only the "variable part" of the obj (MDEntries array):
    void InitVar()
      { memset(m_MDEntries, '\0', m_NoMDEntries * sizeof(MDEntry));   }

    // "GetNEntries":
    int GetNEntries()              const { return int(m_NoMDEntries); }

    // "GetEntry":
    MDEntry const& GetEntry(int i) const
    {
      assert(0 <= i && i < int(m_NoMDEntries) && m_NoMDEntries <= MaxMDEs);
      return m_MDEntries[i];
    }

    // "GetMDSubscrID":
    // FAST is based on UDP multicast, without any explicit susbcriptions for
    // individual instrs; hence the returned value is always 0:
    //
    constexpr static OrderID GetMDSubscrID()  { return 0; }

    // "IsLastFragment":
    // XXX: There is no "m_LastFragment" fld in MICEX "IncrementalRefresh" msgs.
    // We assume that because FullOrdersLog MDEs are quite small,  it is always
    // possible to fit a series of consistent OrderBook updates in 1 msgs,  so:
    //
    constexpr static bool    IsLastFragment() { return true; }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    template<AssetClassT::type>
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );

    //-----------------------------------------------------------------------//
    // Accessors using both "IncrementalRefresh" and its "MDEntry":          //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    static char const*    GetSymbol(MDEntry const& a_mde)
      { return a_mde.m_Symbol; }

    // "GetSecID":
    // NB:  "MkSecID" does NOT use Tenors here. The Exchange name must be comp-
    // patible with static SecDef configs (ie "MICEX" -- XXX: hard-coded!!!):
    //
    static SecID          GetSecID(MDEntry const& a_mde)
    {
      static_assert        (!::MAQUETTE::MICEX::UseTenorsInSecIDs);
      return MkSecID<false>(a_mde.m_Symbol, "MICEX", a_mde.m_TradingSessionID);
    }

    // "GetRptSeq":
    static SeqNum         GetRptSeq (MDEntry const& a_mde)
      { return a_mde.m_RptSeq; }

    // "GetEventTS":
    static utxx::time_val GetEventTS(MDEntry const& a_mde)
    {
      // Here MDE always contains a microsecond-precision TimeStamp:
      return DateTimeToTimeValFAST
             (GetCurrDate(), a_mde.m_MDEntryTime, int(a_mde.m_OrigTime));
    }
  };
  // End of "IncrementalRefresh"

  //=========================================================================//
  // "TradesIncrement":                                                      //
  //=========================================================================//
  // Similar to "IncrementralRefresh", but contains Trade data:
  // ID=2521: Tr_EQ
  // ID=3611: Tr_FX
  //
  template<>
  struct TradesIncrement<ProtoVerT::Curr>
  {
    using MDEntry = MDEntryTrade;

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t       s_SendingTime;                  // 52: XXX: Skipped!
    uint32_t              m_MsgSeqNum;                    // 34
    uint32_t              m_NoMDEntries;                  // 268

    // Fixed-size buffer for "MDEntry"s. Must be m_NoMDEntries <= MaxMDEs:
    constexpr static int  MaxMDEs = 128;
    MDEntry               m_MDEntries[MaxMDEs];

    //-----------------------------------------------------------------------//
    // Ctor and Accessors:                                                   //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    TradesIncrement() { Init(); }

    // "Init":
    // Zero-out the whole object:
    void Init()      { memset(this, '\0', sizeof(TradesIncrement));   }

    // "InitFixed":
    // Zero-Out only the "fixed part" of the obj (before "m_MDEntries"):
    void InitFixed()
      { memset(this, '\0', offsetof(TradesIncrement, m_MDEntries));   }

    // "InitVar":
    // Zero-Out only the "variable part" of the obj (MDEntries array):
    void InitVar()
      { memset(m_MDEntries, '\0', m_NoMDEntries * sizeof(MDEntry));   }

    // "GetNEntries":
    int GetNEntries()              const { return int(m_NoMDEntries); }

    // "GetEntry":
    MDEntry const& GetEntry(int i) const
    {
      assert(0 <= i && i < int(m_NoMDEntries) && m_NoMDEntries <= MaxMDEs);
      return m_MDEntries[i];
    }

    // "GetMDSubscrID":
    // FAST is based on UDP multicast, without any explicit susbcriptions for
    // individual instrs; hence the returned value is always 0:
    //
    constexpr static OrderID GetMDSubscrID()  { return 0; }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    template<AssetClassT::type>
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );

    //-----------------------------------------------------------------------//
    // Accessors using both "TradesIncrement" and its "MDEntry":             //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    static char const*    GetSymbol(MDEntry const& a_mde)
      { return a_mde.m_Symbol; }

    // "GetSecID":
    // NB:  "MkSecID" does NOT use Tenors here. Again, Exchange="MICEX" here:
    //
    static SecID          GetSecID(MDEntry const& a_mde)
    {
      static_assert        (!::MAQUETTE::MICEX::UseTenorsInSecIDs);
      return MkSecID<false>(a_mde.m_Symbol, "MICEX", a_mde.m_TradingSessionID);
    }

    // "GetEventTS":
    static utxx::time_val GetEventTS(MDEntry const& a_mde)
    {
      // Here MDE always contains a microsecond-precision TimeStamp:
      return DateTimeToTimeValFAST
             (GetCurrDate(), a_mde.m_MDEntryTime, int(a_mde.m_OrigTime));
    }
  };
  // End of "TradesIncrement"

  //=========================================================================//
  // "SnapShot" for FullOrdersLog:                                           //
  //=========================================================================//
  // ID=2510: Orders_EQ,
  // ID=3600: Orders_FX:
  //
  template<>
  struct SnapShot<ProtoVerT::Curr>
  {
    using MDEntry = MDEntrySnap;

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static uint64_t       s_SendingTime;                  // 52: XXX: Skipped!
    uint32_t              m_MsgSeqNum;                    // 34
    char                  m_TradingSessionID[16];         // 336
    char                  m_Symbol[16];                   // 55
    uint32_t              m_LastMsgSeqNumProcessed;       // 369
    int32_t               m_RptSeq;                       // 83
    uint32_t              m_LastFragment;                 // 893
    uint32_t              m_NoMDEntries;                  // 268

    // Fixed-size buffer for "MDEntry"s. Must be m_NoMDEntries <= MaxMDEs:
    constexpr static int  MaxMDEs = 128;
    MDEntry               m_MDEntries[MaxMDEs];

    //-----------------------------------------------------------------------//
    // Unused (Static) Flds:                                                 //
    //-----------------------------------------------------------------------//
    // XXX: "TradSesStatus" and "MDSecurityTradingStatus" SHOULD actually be
    // used, but for now,  they are not:
    static uint32_t       s_RouteFirst;                   // 7944
    static int32_t        s_TradSesStatus;                // 340
    static int32_t        s_MDSecurityTradingStatus;      // 1682
    static uint32_t       s_AuctionIndicator;             // 5509

    //-----------------------------------------------------------------------//
    // Ctor and Accessors:                                                   //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    SnapShot()       { Init(); }

    // "Init":
    // Zero-out the whole object:
    void Init()      { memset(this, '\0', sizeof(SnapShot)); }

    // "InitFixed":
    // Zero-Out only the "fixed part" of the obj (before "m_MDEntries"):
    void InitFixed() { memset(this, '\0', offsetof(SnapShot, m_MDEntries)); }

    // "InitVar":
    // Zero-Out only the "variable part" of the obj (MDEntries array):
    void InitVar()
      { memset(m_MDEntries, '\0', m_NoMDEntries * sizeof(MDEntry));       }

    // "GetNEntries":
    int GetNEntries()                 const  { return int(m_NoMDEntries); }

    // "GetEntry":
    MDEntry const& GetEntry(int i)    const
    {
      assert(0 <= i && i < int(m_NoMDEntries) && m_NoMDEntries <= MaxMDEs);
      return m_MDEntries[i];
    }

    // "IsLastFragment":
    // Whether this msg is the last one in a series of LOGICAL updates (in that
    // case, presumably of the same Symbol):
    //
    bool IsLastFragment()             const  { return m_LastFragment; }

    // "GetMDSubscrID":
    // This Accessor is trivial on MICEX:
    constexpr static OrderID GetMDSubscrID() { return 0; }

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    template<AssetClassT::type>
    char const* Decode
    (
      char const*     a_buff,
      char const*     a_end,
      PMap            a_pmap
    );

    //-----------------------------------------------------------------------//
    // Accessofrs using both "SnapShot" and its "MDEntry":                   //
    //-----------------------------------------------------------------------//
    // "GetSymbol":
    char const* GetSymbol(MDEntry const&) const { return m_Symbol; }

    // "GetSecID":
    // NB: "MkSecID" does NOT use Tenors here. Again, Exchange="MICEX" here:
    //
    SecID       GetSecID (MDEntry const&) const
    {
      static_assert(!::MAQUETTE::MICEX::UseTenorsInSecIDs);
      return MkSecID<false>(m_Symbol, "MICEX", m_TradingSessionID);
    }

    // "GetRptSeq":
    SeqNum      GetRptSeq(MDEntry const&) const { return m_RptSeq; }

    // "GetEventTS":
    static utxx::time_val GetEventTS(MDEntry const& a_mde)
    {
      // Here MDE always contains a microsecond-precision TimeStamp:
      return DateTimeToTimeValFAST
             (GetCurrDate(), a_mde.m_MDEntryTime, int(a_mde.m_OrigTime));
    }
  };
  // End of "SnapShot"

  //=========================================================================//
  // "SecurityDefinition":                                                   //
  //=========================================================================//
  // ID=2115:
  // XXX: This msg type is NOT used in prime-time, so we don't need much optim-
  // isation here (such as making unused flds "static"):
  //
  //=========================================================================//
  // "SecurityDefinitionBase_Curr" (w/o vectors):                            //
  //=========================================================================//
  struct SecurityDefinitionBase_Curr
  {
    uint32_t              m_MsgSeqNum;                    // 34
    uint64_t              m_SendingTime;                  // 52
    char                  m_MessageEncoding[16];          // 347
    int32_t               m_TotNumReports;                // 911
    char                  m_Symbol[16];                   // 55
    char                  m_SecurityID[16];               // 48
    char                  m_SecurityIDSource[16];         // 22
    int32_t               m_Product;                      // 460
    char                  m_CFICode[16];                  // 461
    char                  m_SecurityType[16];             // 167
    uint32_t              m_MaturityMonthYear;            // 200
    uint32_t              m_MaturityDate;                 // 541
    uint32_t              m_SettlDate;                    // 64
    char                  m_SettleType[4];                // 5459
    Decimal               m_OrigIssueAmt;                 // 5850
    uint32_t              m_CouponPaymentDate;            // 224
    Decimal               m_CouponRate;                   // 223
    uint32_t              m_SettlFixingDate;              // 9119
    Decimal               m_DividendNetPx;                // 9982
    char                  m_SecurityDesc[64];             // 107
    char                  m_EncodedSecurityDesc[64];      // 351
    char                  m_QuoteText[64];                // 9696
    char                  m_Currency[4];                  // 15
    char                  m_SettlCurrency[4];             // 120
    int32_t               m_PriceType;                    // 423
    char                  m_StateSecurityID[24];          // 5217
    char                  m_EncodedShortSecurityDesc[32]; // 5383
    char                  m_MarketCode[16];               // 5385
    Decimal               m_MinPriceIncrement;            // 969
    Decimal               m_MktShareLimit;                // 5387
    Decimal               m_MktShareThreshold;            // 5388
    Decimal               m_MaxOrdersVolume;              // 5389
    Decimal               m_PriceMvmLimit;                // 5470
    Decimal               m_FaceValue;                    // 5508
    Decimal               m_BaseSwapPx;                   // 5556
    Decimal               m_RepoToPx;                     // 5677
    Decimal               m_BuyBackPx;                    // 5558
    uint32_t              m_BuyBackDate;                  // 5559
    Decimal               m_NoSharesIssued;               // 7595
    Decimal               m_HighLimit;                    // 9199
    Decimal               m_LowLimit;                     // 9200
    int32_t               m_NumOfDaysToMaturity;          // 10508

    // Counters for Vectors (see below); not really needed, provided for comple-
    // teness:
    uint32_t              m_NoInstrAttribs;               // 870
    uint32_t              m_NoMarketSegments;             // 1310

    // Default Ctor:
    SecurityDefinitionBase_Curr() { Init(); }

    // "Init": Zero-Out the Struct:
    void Init() { memset(this, '\0', sizeof(SecurityDefinitionBase_Curr)); }
  };

  //=========================================================================//
  // "SecurityDefinition" Itself:                                            //
  //=========================================================================//
  template<>
  struct SecurityDefinition<ProtoVerT::Curr>: public SecurityDefinitionBase_Curr
  {
    //-----------------------------------------------------------------------//
    // Sequence Entry Types:                                                 //
    //-----------------------------------------------------------------------//
    // XXX: Unlike "IncrementalRefresh" and "SnapShot",   this msg type uses
    //      "static_vector" instead of fixed-size arrays  to store Sequences,
    //      for the following 2 reasons:
    // (*) there are multiple Sequences, as well as Sequences nested at more
    //     than 1 level, within a msg;   handling them via fixed-size arrays
    //     would be very tedeous and memory-inefficient;
    // (*) processing of "SecurityDefinition" msgs is typically non-performan-
    //     ce-critical (done off-line):
    //-----------------------------------------------------------------------//
    // "InstrumentAtrib":                                                    //
    //-----------------------------------------------------------------------//
    struct InstrumentAttrib
    {
      int32_t             m_InstrAttribType;              // 871
      char                m_InstrAttribValue[64];         // 872

      InstrumentAttrib() { Init(); }
      void Init()        { memset(this, '\0', sizeof(InstrumentAttrib)); }
    };

    // Up to 8 "InstrumentAttrib"s per "SecurityDefinition" are allowed:
    using InstrumentAttribsVec =
          boost::container::static_vector<InstrumentAttrib, 8>;

    //-----------------------------------------------------------------------//
    // "SessionRule":                                                        //
    //-----------------------------------------------------------------------//
    struct SessionRule
    {
      char                m_TradingSessionID[16];         // 336
      char                m_TradingSessionSubID[16];      // 625
      int32_t             m_SecurityTradingStatus;        // 326
      int32_t             m_OrderNote;                    // 9680

      SessionRule() { Init(); }
      void Init()   { memset(this, '\0', sizeof(SessionRule)); }
    };

    // Up to 8 "SessionRule"s per "MarketSegment" (see below) are llowed:
    using SessionRulesVec =
          boost::container::static_vector<SessionRule, 8>;

    //-----------------------------------------------------------------------//
    // "MarketSegment":                                                      //
    //-----------------------------------------------------------------------//
    struct MarketSegment
    {
      Decimal             m_RoundLot;                     // 561
      uint32_t            m_NoTradingSessionRules;        // 1309
      SessionRulesVec     m_SessionRules;

      MarketSegment()
      : m_RoundLot (),
        m_NoTradingSessionRules(0),
        m_SessionRules()
      {}
    };

    // Up to 8 "MarketSegment"s per "SecurityDefinition" are allowed:
    using MarketSegmentsVec = boost::container::static_vector<MarketSegment, 8>;

    //-----------------------------------------------------------------------//
    // Vectors:                                                              //
    //-----------------------------------------------------------------------//
    InstrumentAttribsVec  m_InstrAttribs;
    MarketSegmentsVec     m_MarketSegments;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    SecurityDefinition()
    : SecurityDefinitionBase_Curr(),
      m_InstrAttribs    (),
      m_MarketSegments  ()
    {}

    //-----------------------------------------------------------------------//
    // Decoder:                                                              //
    //-----------------------------------------------------------------------//
    char const* Decode
    (
      char const*   a_buff,
      char const*   a_end,
      PMap          a_pmap
    );
  };

  //=========================================================================//
  // "SecurityStatus":                                                       //
  //=========================================================================//
  // ID=2106:
  // XXX: At the moment, this msg is NOT RECEIVED AT ALL in "EConnector_FAST_
  // MICEX", but it obviously should be:
  //
  template<>
  struct SecurityStatus<ProtoVerT::Curr>
  {
    // Data Flds:
    uint32_t              m_MsgSeqNum;                    // 34
    char                  m_Symbol[16];                   // 55
    char                  m_TradingSessionID[16];         // 336
    int32_t               m_SecurityTradingStatus;        // 326
    // Unused (static) flds:
    static uint64_t       s_SendingTime;                  // 52
    static char           s_TradingSessionSubID[16];      // 625
    static uint32_t       s_AuctionIndicator;             // 5509

    // Default Ctor:
    SecurityStatus()  { Init(); }
    void Init()       { memset(this, '\0', sizeof(SecurityStatus)); }

    // Decoder:
    char const* Decode
    (
      char const*   a_buff,
      char const*   a_end,
      PMap          a_pmap
    );
  };

  //=========================================================================//
  // "TradingSessionStatus":                                                 //
  //=========================================================================//
  // ID=2107:
  //
  template<>
  struct TradingSessionStatus<ProtoVerT::Curr>
  {
    // Data Flds:
    static uint64_t       s_SendingTime;                  // 52: XXX: Skipped!
    uint32_t              m_MsgSeqNum;                    // 34
    int32_t               m_TradSesStatus;                // 340
    char                  m_Text[64];                     // 58
    char                  m_TradingSessionID[16];         // 336

    // Default Ctor:
    TradingSessionStatus() { Init(); }
    void Init()            { memset(this, '\0', sizeof(TradingSessionStatus)); }

    // Decoder:
    char const* Decode
    (
      char const*   a_buff,
      char const*   a_end,
      PMap          a_pmap
    );
  };

  //=========================================================================//
  //  "HeartBeat":                                                           //
  //=========================================================================//
  // ID=2108:
  //
  template<>
  struct HeartBeat<ProtoVerT::Curr>
  {
    static uint64_t       s_SendingTime;                  // 52: XXX: Skipped!
    uint32_t              m_MsgSeqNum;                    // 34

    // Default Ctor:
    HeartBeat() { Init(); }
    void Init() { memset(this, '\0', sizeof(HeartBeat)); }

    // Decoder:
    char const* Decode
    (
      char const*   a_buff,
      char const*   a_end,
      PMap          a_pmap
    );
  };
} // End namespace MICEX
} // End namespace FAST
} // End namespace MAQUETTE
