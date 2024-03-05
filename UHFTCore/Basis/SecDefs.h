// vim:ts=2:et
//===========================================================================//
//                               "Basis/SecDefs.h":                          //
//                  Generic Security (Instrument) Definitions:               //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/PxsQtys.h"
#include "Basis/TimeValUtils.hpp"
#include <utxx/enum.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "SecTrStatusT":                                                         //
  //=========================================================================//
  // NB: This is a trading status for an individual Security, NOT  for the whole
  // Trading Session. XXX: In FIX, we have a much more detailed (and cumbersome)
  // "FIX::SecTrStatusT" -- that is a different enum!
  //
  UTXX_ENUM(
  SecTrStatusT, int,
    NoTrading,
    CancelsOnly,
    FullTrading
  );

  //=========================================================================//
  // "SecDefS" Struct: Static (Time-Invariant) Instrument Data:              //
  //=========================================================================//
  // IMPORTANT CONVENTIONS:
  // (1) Each Instrument can be considered as a PAIR A/B, where A and B Assets.
  //     Eg for FX, A is BaseCcy and B is QuoteCcy; for EQ, A typically coinci-
  //     des with the Instr itself and B is the Denominating Ccy. A may also be
  //     a "virtual" Asset such as an Index.
  // (2) Instrument Px (as used in MDCs and OMCs) can be quoted as the Px of  1
  //     Contract (ie 1 unit of that Instrument) OR 1 unit of AssetA, depending
  //     on the Venue. Internally,  we always recalculate the Px to be per unit
  //     of AssetA, expressed in units of AssetB, using the PxFactAB coeff.  To
  //     do that, we need to specify the PointPx and whether the Px (in Pts) is
  //     quoted per unit of 'A', or per Contract ('C').
  // (3) 1 Instrument (Contract) can be made of N units of AssetA or N units of
  //     AssetB, normally just N(A)=1. Using N(B) is rare  -- eg crypto-futures
  //     on BitMEX or OKEX. Obviously, only one of N(A), N(B) can be fixed, the
  //     other qty would be dynamic and would depend on the TradePx.
  //     Thus, we specify ABQtys as {N,0} (for AssetA) or as {0,N} (for AssetB).
  //     QtyA = Contracts * ABQtys[0]   (if ABQtys[0] > 0)
  //     QtyB = Contracts * ABQtys[1]   (if ABQtys[1] > 0)
  //     Cf "QtyTypeT"!
  //     The "IsContractInA" method returns "true" if N(A) is fixed, and "false"
  //     if N(B) is fixed.
  // (4) The quoted or traded Instrument Px (per Contract or Unit of A, see (2))
  //     is expressed in units, proportional to AssetB called Points.  The fixed
  //     value of 1 point in units of AssetB is called the PointPx. Often Point-
  //     Px=1, but not always (eg they are different when A is an Index). We al-
  //     ways recalculate PointPx to correspond to Px per unit of A,   yielding
  //     the PxFactAB coeff mentioned in (2).
  // (5) Note that by definition, AssetB is the one  in which  the Instr  has a
  //     const PointPx. XXX Eg consider the Instr called  "ETH/USD"  on BitMEX;
  //     its AssetA is ETH but AssetB is actually BTC because the P&L is formed
  //     in BTC; so it is actually not an FX Instr (a Ccy pair)  but an ETH/BTC
  //     Index with an alternative px dynamics!
  // (6) Although AssetB is always used for Px quoting and P&L denomination,  it
  //     is NOT necessarily for the actual operational settlement; the latter is
  //     not specified by "SecDefS".
  //     Eg USD/JPY pair: AssetA=USD and AssetB=JPY in our convention, but when
  //     traded eg on HotSpotFX, the actual operational settlment  would be  in
  //     USD, not in JPY, using the inverse Pxs.
  // (7) LotSize has a bit complicated semantics:
  //     (*) If the order sizes are specified (in MDC and/or OMC) in Contracts
  //     and are integral, then Contracts must be multiples of Lots:
  //         NContracts    = NLots * LotSize;
  //         often LotSize = 1;
  //     (*) Furthermore, order size may actually be given  in Lots rather than
  //         in Contracts (see "QtyTypeT"), for either or both of MDC, OMC;
  //     (*) More generally, if the order size is given in QtyA or QtyB, and is
  //         in general fractional, then LotSize is the order size "step" (inc-
  //         rement), and is also in general fractional.
  //     However, in any case, LotSize  has nothing to do  with pricing and P&L
  //     evaluation.
  // (8) MinSizeLots:  The minimal order size relative to the LotSize (or size
  //     increment), always an integer;  usually MinSizeLots=1.
  // (9) Expiration Date, Time, ... is for Derivatives  (Futures and  Options).
  //     For Options, more detailed contract spec may be required (eg Put/Call,
  //     or American/European, ...). It is currently NOT provided here.   Such
  //     info could be implied by "CFICode" or even derived from "Symbol":
  //
  struct SecDefS
  {
    //-----------------------------------------------------------------------//
    // Time-Constant Data Flds:                                              //
    //-----------------------------------------------------------------------//
    // Main Security Properties:
    SecID           m_SecID;              // From Exchange; modified in SecDefD
    SymKey          m_Symbol;             // Usually Exchange-defined
    SymKey          m_AltSymbol;          // Eg ISIN (if available)
    char            m_SecurityDesc[128];
    char            m_CFICode     [8];    // 6-char std code
    SymKey          m_Exchange;           // Exchange/ ECN / Aggregator
    SymKey          m_SessOrSegmID;       // Session, MktSegment,  etc
    SymKey          m_Tenor;              // Eg TOD  / TOM / SPOT  etc
    SymKey          m_Tenor2;             // For Swaps and similar instrs only
    SymKey          m_AssetA;             // AKA BaseCcy / CcyA / BaseAsset
    Ccy             m_QuoteCcy;           // AKA AssetB  / CcyB / DenomCcy

    // Qty Mgmt Params:
    double          m_ABQtys      [2];    // See Par.(3) above
    double          m_LotSize;            // ContractMultiplier (LotSize, eg 1)
    long            m_MinSizeLots;        // Normally 1, but not always

    // Px  Mgmt Params:
    double          m_PxStep;             // Price Step (Min Price Increment)
    double          m_PxFactAB ;          // ContrPx->Px(A/B) coeff (eg 1)
                                          // when Px is quoted per 'A' unit
    // For Derivatives only:
    int             m_ExpireDate;         // As YYYYMMDD: Maturity / Expiration
    int             m_ExpireTime;         // As IntraDay Secs in UTC
    utxx::time_val  m_ExpirePrec;         // Precise instant (eg for BSM)
    double          m_Strike;             // For Options only
    SecID           m_UnderlyingSecID;    // To identify the Underlying
    SymKey          m_UnderlyingSymbol;   // ditto

    // Some Properties:
    bool            m_IsFX;
    bool            m_IsSwap;

    //-----------------------------------------------------------------------//
    // Default Ctor: Zero it out:                                            //
    //-----------------------------------------------------------------------//
    SecDefS()
      { memset(this, '\0', sizeof(SecDefS)); }

    //-----------------------------------------------------------------------//
    // Full Non-Default Ctor for Static Initializations:                     //
    //-----------------------------------------------------------------------//
    // NB: "ExpirePrec" is omitted from the Ctor args list, because it is a
    // function of (ExpireDate, ExpireTime):
    SecDefS
    (
      SecID         a_sec_id,
      char const*   a_symbol,
      char const*   a_alt_symbol,
      char const*   a_sec_desc,
      char const*   a_cfi_code,
      char const*   a_exchange,
      char const*   a_segment,
      char const*   a_tenor,
      char const*   a_tenor2,
      char const*   a_asset_a,
      char const*   a_quote_ccy,    // AKA AssetB
      char          a_qty_type,     // 'A'|'B' for ABQty below,   see Par.(3)
      double        a_ab_qty,       // QtyPerContract
      double        a_lot_size,     // For order placement, not pxing
      long          a_min_size_lots,
      double        a_px_step,
      char          a_px_type,      // 'C' if Px is quoted per Contract, or 'A'
      double        a_point_px,     // PointPx (#B in 1 Point of ContractPx)
      int           a_expire_date,  // As YYYYMMDD
      int           a_expire_time,  // Intra-day secs in UTC
      double        a_strike,
      SecID         a_underlying_sec_id,
      char const*   a_underlying_symbol
    );

    //-----------------------------------------------------------------------//
    // "IsContractInA":                                                      //
    //-----------------------------------------------------------------------//
    bool IsContractInA() const
    {
      // Exactly one of "m_ABQtys" should be set for a non-empty "SecDefS":
      assert((m_ABQtys[0] > 0.0) ^ (m_ABQtys[1] > 0.0));
      return (m_ABQtys[0] > 0.0);
    }

    //-----------------------------------------------------------------------//
    // "GetPxAB":                                                            //
    //-----------------------------------------------------------------------//
    // Converting Contract/Instrument Px (as quoted at the Exchange or accepted
    // by OrderMgmt) into Px(A/B), that is, the number of B units equivalent in
    // value to 1 A unit:
    // TODO: Use different types for "Px" (similar to "Qty"):
    //
    PriceT GetPxAB(PriceT a_contract_px) const
      { return a_contract_px * m_PxFactAB; }

    //-----------------------------------------------------------------------//
    // "GetContractPx":                                                      //
    //-----------------------------------------------------------------------//
    // Converse of "GetPxAB":
    //
    PriceT GetContractPx(PriceT a_px_ab) const
      { return a_px_ab / m_PxFactAB; }
  };

  //=========================================================================//
  // "MkSecID":                                                              //
  //=========================================================================//
  // Hash Function for SecID creation (if not given explicitly by the Exchange),
  // Uses both "Symbol" and "Segment", and possibly "Tenor"s (but only if their
  // use is EXPLICITLY enabled and they are indeed available; explicit enabling
  // is to ensure compatible SecID generation in "SecDefD" Ctor and in receiving
  // MktData streams). On the other hand, using Tenors is sometimes is essential
  // to distinguish otherwise-similar Instruments (eg in FX):
  // NB: The "buff" construction here is simplified compared to stringification
  // in "SecDefD::ToString" below -- the resulting strings are NOT the same!
  //
  template<bool UseTenors>
  SecID  MkSecID
  (
    char const* a_symbol,           // Always non-empty
    char const* a_exchange,         // Always non-empty
    char const* a_segment,          // May be NULL or "" (but not recommended)
    char const* a_tenor  = nullptr, // Ignored in any case if !UseTenors
    char const* a_tenor2 = nullptr  // ditto
  );

  //=========================================================================//
  // "SecDefD": Static + Dynamic (Time-Variable) Instrument Data:            //
  //=========================================================================//
  // This is an extension of "SecDefS", rather than a separate struct, for per-
  // formance reasons (to minimise cache misses):
  //
  class  EConnector;

  struct SecDefD: public SecDefS
  {
    //-----------------------------------------------------------------------//
    // Time-Variable Data Flds:                                              //
    //-----------------------------------------------------------------------//
    // NB: For Spot-type instruments (eg EQ or FX Spot), SettlDate and Tenor
    // should be the same. But for Derivatives  (Futures and Options)  with
    // "long" Tenors and daily VM settlement, we may have SettlDate <= Tenor:
    int                   m_SettlDate;
    char                  m_SettlDateStr [12]; // As above, but string
    int                   m_SettlDate2;        // For Swaps only
    char                  m_SettlDate2Str[12]; // ditto
    ObjName               m_FullName;          // Symbol[-SessOrSegmID]
                                               //   [-Tenor][-Tenor2]
                                               //   -SettlDate[-SettlDate2]
    // XXX: Passive and Aggressive Trading Fee Rates, per value of the contract
    // traded (thus, it does not depend on the quoting or trading rules). Loca-
    // ted in the "dynamic" SecDef part because may be Account-dependent;   put
    // here by the OMC. NaN if not known:
    double                m_passFeeRate;
    double                m_aggrFeeRate;

    // Other flds can change in real time:
    mutable SecTrStatusT  m_TradingStatus;
    mutable double        m_LowLimitPx;
    mutable double        m_HighLimitPx;
    // For Derivatives only:
    mutable double        m_Volatility;
    mutable double        m_TheorPx;
    // NB: Except for Options, all of Margins below should be equal(?)
    mutable double        m_InitMarginOnBuy;
    mutable double        m_InitMarginOnSell;
    mutable double        m_InitMarginHedged;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    SecDefD()
    : m_SettlDate         (0),
      m_SettlDateStr      (),
      m_SettlDate2        (0),
      m_SettlDate2Str     (),
      m_FullName          (),
      m_passFeeRate       (NaN<double>),
      m_aggrFeeRate       (NaN<double>),
      m_TradingStatus     (SecTrStatusT::NoTrading),
      m_LowLimitPx        (NaN<double>),
      m_HighLimitPx       (NaN<double>),
      m_Volatility        (NaN<double>),
      m_TheorPx           (NaN<double>),
      m_InitMarginOnBuy   (NaN<double>),
      m_InitMarginOnSell  (NaN<double>),
      m_InitMarginHedged  (NaN<double>)
    {}

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // Copies an existing "SecDefS" into "SecDefD", other flds are set to their
    // default vals. May modify the SecID and Segment. NB: Other "SecDefD" flds
    // can be set freely on a consctructed obj, as they do not affect the SecID:
    //
    SecDefD
    (
      SecDefS const&      a_sds,
      bool                a_use_tenors,        // For SecID computation
      int                 a_settl_date,
      int                 a_settl_date2
    );
  };
} // End namespace MAQUETTE
