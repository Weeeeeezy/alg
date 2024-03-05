// vim:ts=2:et
//===========================================================================//
//                         "InfraStruct/RiskMgrTypes.h":                     //
//                    Types Used by "RiskMgr" and its Clients                //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include "Basis/PxsQtys.h"
#include "Basis/Maths.hpp"
#include "InfraStruct/StaticLimits.h"
#include "Connectors/OrderBook.h"
#include <utxx/error.hpp>
#include <boost/container/static_vector.hpp>
#include <utility>
#include <functional>

namespace MAQUETTE
{
  //=========================================================================//
  // The Operating Mode:                                                     //
  //=========================================================================//
  enum class RMModeT
  {
    // STP Mode:
    // RiskMgr is used for tracking Instrument and Asset Positions, P&L, NAV
    // and Risks of Trades and other Transactions coming via  a  "Drop-Copy"
    // mechanism, but it does not control any Order Placement:
    STP     = 0,

    // Safe Mode: No New Orders or Order Modifications are allowed; Cancels
    // are still OK; MktData and ExecReports from already-placed Orders are
    // received and processed:
    Safe    = 1,

    // Normal Mode: All Orders (New / Modify / Cancel) are allowed, subject
    // to standard Risk Controls:
    Normal  = 2,

    // Relaxed Mode: All Orders are allowed; Position Info is maintained but
    // is not used for any controls; PnL, UnRPnL etc are not computed;   but
    // Active Orders and Order Placement / Filling Rates are  still enforced;
    // it is useful for eg Quoters which do not need to fully cover their po-
    // sitions:
    Relaxed = 3
  };

  //=========================================================================//
  // Double-Represented Qty Types:                                           //
  //=========================================================================//
  using RMQtyA = Qty<QtyTypeT::QtyA, double>;
  using RMQtyB = Qty<QtyTypeT::QtyB, double>;

  //=========================================================================//
  // "InstrRisks" Struct:                                                    //
  //=========================================================================//
  // "InstrRisks" describes a Traded Instrument which is a Risk Source; it is
  // always per-Connector (per-Exchange) and per-SettlDate, even though a col-
  // lection of "InstrRisks" can be maintained globally:
  //
  class  RiskMgr;
  struct AssetRisks;

  struct InstrRisks
  {
  public:
    //-----------------------------------------------------------------------//
    // "InstrRisks" Data Flds:                                               //
    //-----------------------------------------------------------------------//
    // Meta-Data:
    // NB: Normally, we would maintain a ptr to the OrderBook (to re-calculate
    // the risks on the MktData updates)   which in turn contains a ptr to the
    // corr SecDef. HOWEVER, in some cases, the OrderBook itself is not suffic-
    // iently "liquid", so we need a ptr to the SecDef and a separate OrderBook
    // ptr (possibly an alternative one):
    //
    RiskMgr       const*   m_outer;           // Non-Owned Ptr to main RiskMgr
    UserID                 m_userID;          // Owner of this InstrRisks
    SecDefD       const*   m_instr;           // Ptr NOT owned
    OrderBookBase const*   m_ob;              // Ptr NOT owned, NOT persistent!

    // NON-owned ptrs to Base(A) Asset and Quote(B) Ccy of this Instr; always
    // non-NULL:
    AssetRisks*            m_risksA;          // Non-NULL
    AssetRisks*            m_risksB;          // ditto

    // We need Rate(B/RFC) for computing Appreciation of Realised PnL  in  RFC.
    // The following only memoises the last val for comparisons; "m_risksB" is
    // the authoritative src of this rate:
    mutable double         m_lastRateB;

    // Position (in units of A), AvgPosPx, Realised PnL and UnRPnL (the latter
    // 3 are in units of B and in RFC) for this Instrument. NB: AvgPosPxAB  is
    // really in B units per A unit, with all conversion factors  already taken
    // into account:
    mutable utxx::time_val m_ts;              // ExchTS if available, or RecvTS
    mutable RMQtyA         m_posA;            // In units of AssetA
    mutable PriceT         m_avgPosPxAB;      // NOT accounting for trans costs
    mutable RMQtyB         m_realisedPnLB;    // Comes into corresp QuoteCcy (B)
    mutable RMQtyB         m_unrPnLB;         // UnRealised PnL, in B
    mutable double         m_realisedPnLRFC;  // Similar to above, but in RFC
    mutable double         m_apprRealPnLRFC;  // Appreciation part of above
    mutable double         m_unrPnLRFC;       // ditto

    // Total (absolute) size of all Active Orders for this Instrument -- needs
    // an RFC val as well (because the RFC Totals maintained by the  RiskMgr::
    // ActiveOrders are Instr-based, not Asset-based):
    mutable RMQtyA         m_activeOrdsSzA;   // In units of "A" (NOT "B"!)
    mutable double         m_activeOrdsSzRFC; // Same in RFC
    mutable long           m_ordsCount;       // Total Orders issued: a Ticker!

    // The following is for optimisation only (to prevent multiple unsuccessful
    // init attempts):
    mutable long           m_initAttempts;

  private:
    // Comparison is not supported:
    bool operator==(InstrRisks const&) const;
    bool operator!=(InstrRisks const&) const;

  public:
    //-----------------------------------------------------------------------//
    // Default and Non-Default Ctors:                                        //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    InstrRisks();

    // "IsEmpty": Was it just created by the Default Ctor?
    bool IsEmpty()  const;

    // Non-Default Ctor: Creates a Non-Empty obj:
    InstrRisks
    (
      RiskMgr       const* a_outer,           // Non-NULL
      UserID               a_user_id,         // 0 if not known
      SecDefD       const& a_instr,
      OrderBookBase const* a_ob,              // NULL OK but not recommended!
      AssetRisks*          a_ar_a,            // Always non-NULL
      AssetRisks*          a_ar_b             // Always non-NULL
    );

    //-----------------------------------------------------------------------//
    // "ResetTransient":                                                     //
    //-----------------------------------------------------------------------//
    // NB: All Transient and Non-ShM data are reset. In particular, the Order-
    // Book ptr is replaced by the new one (may be NULL). All cumulative data
    // are preserved:
    void ResetTransient(OrderBookBase const* a_new_ob);

    //-----------------------------------------------------------------------//
    // "OnMktDataUpdate":                                                    //
    //-----------------------------------------------------------------------//
    // Updates UnRPnL  on a MktData tick.
    // Args:   BestBid and BestAsk Pxs of A/B (normalised as
    //         #(QtyB) per 1(QtyA))
    // NB: This method is not called in the Realxed mode!
    //
    void OnMktDataUpdate
    (
      OrderBookBase const& a_ob,              // For identification only
      PriceT               a_bid_px_ab,       // Already computed by the CallER
      PriceT               a_ask_px_ab,       // ditto
      utxx::time_val       a_ts               // ExchTS if known, or LocalTS
    )
    const;

    //-----------------------------------------------------------------------//
    // "OnTrade":                                                            //
    //-----------------------------------------------------------------------//
    // Updates "m_posA", "m_avgPosPxAB", maybe resets "m_unrPnLB" on OUR Trade.
    // NB:
    // (*) Unless UnRPnL is reset, it is NOT re-computed by "OnTrade": generic-
    //     ally, a trade at curr mkt price does not affect the UnRPnL, and it
    //     will be re-evaled at the next MktData tick anyway;
    // (*) Trade Px and Qty must ALREADY be normalised by the Caller: Px  is
    //     for A/B itself, Qty is in A units:
    // (*) Invokes "OnTrade" on "AssetRisks" A and B;
    // (*) Returns the size  of this Trade in RFC:
    //
    double OnTrade
    (
      ExchOrdID const&  a_trade_id,       // For passing to Call-Back only
      bool              a_is_buy,         // From UserID's point of view
      PriceT            a_px_ab,          // Px(A/B)
      RMQtyA            a_qty_a,          // In units of A
      RMQtyB            a_fee_b,          // Fee/Commission in units of B
      utxx::time_val    a_ts              // ExchTS if known, LocalTS otherwise
    );
  };
  // End of "InstrRisks"

  //=========================================================================//
  // Call-Back on "InstrRisks" Updates:                                     //
  //=========================================================================//
  using InstrRisksUpdateCB =
    std::function
    <void
     (
       InstrRisks const& a_ir,        // State  AFTER  the Trade
       RMQtyA            a_ppos_a,    // Pos(A) BEFORE the Trade
       char const*       a_trade_id,  // Trade ID (NULL or empty on MtM)
       bool              a_is_buy,    // For the UserID's side, irrelev on MtM
       PriceT            a_px_ab,     // ExecPx(A/B) or MtM Px
       RMQtyA            a_qty_a,     // Qty(A) traded, or <= 0 on MtM
       RMQtyB            a_rgain_b,   // RealisedGain(B) (0 if not closing|MtM)
       double            a_b_rfc,     // ValRate (B/RFC)
       double            a_rgain_rfc  // RealisedGain(RFC)
     )
    >;

  //=========================================================================//
  // "AssetRisks" Struct:                                                    //
  //=========================================================================//
  // (Asset, SettlDate) view of Risks:
  //
  struct AssetRisks
  {
  public:
    //-----------------------------------------------------------------------//
    // Asset Transaction Type:                                               //
    //-----------------------------------------------------------------------//
    enum class AssetTransT: unsigned
    {
      UNDEFINED   =  0,
      Buy         =  1,
      Sell        =  2,
      MtM         =  3,   // Not actually a transaction, but may be recorded
      Initial     =  4,
      TransferIn  =  5,   // Re-attribution to UserIDs
      TransferOut =  6,   //
      Deposit     =  7,
      Withdrawal  =  8,
      Borrowing   =  9,   // Transfer-like(?) but with obligation to return
      Lending     = 10
    };

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // Meta-Data:
    RiskMgr const*         m_outer;    // Non-Owned Ptr to the main RiskMgr obj
    UserID                 m_userID;   // Owner of this AssetRisk
    SymKey                 m_asset;
    bool                   m_isRFC;    // To avoid unnecessary comparisons...
    int                    m_settlDate;

    // For evaluation of this Asset in RFC; if the Asset is a Ccy,    this is
    // effectively a Ccy->RFC Converter. It can be given by OrderBook(s) (for
    // Asset/RFC or RFC/Asset if Asset is a Ccy), or by a FixedRate (which is
    // ALWAYS Asset/RFC); in particular, for the RFC itself it is 1.0:
    //
    double                 m_fixedEvalRate;

    // "IsDirect" indicates whether the OrderBook(s) below provide a Direct
    // (Asset/RFC) or Inverse (RFC/CcyAsset) Rate:
    // OrderBooks used for valuation: if OB1 is NULL, then OB2 (and RollOver-
    // Time) must also be NULL / empty, and we only use a FixedRate:
    //
    bool                   m_isDirectRate;
    OrderBookBase const*   m_evalOB1;
    double                 m_adj1[2];  // {Ask, Bid}!!!
    utxx::time_val         m_rollOverTime;
    OrderBookBase const*   m_evalOB2;
    double                 m_adj2[2];  // {Ask, Bid}!!!

    // In case if OrderBook data is not available for some reason, we memoise
    // the last valuation rate used, so that it could be used to valuate exis-
    // ting positions. It is also used as a FixedRate if OrderBooks above are
    // not available. So this rate is ALWAYS Asset/RFC:
    //
    mutable double         m_lastEvalRate;

    // POSITIONS in units of this Asset itself:
    // XXX: For Debts, we ideally need a separate struct w/ Repayment Dates:
    //
    mutable utxx::time_val m_ts;        // ExchTS or (if not avail)  RecvTS
            utxx::time_val m_epoch;     // Starting point of various Accums
            double         m_initPos;   // Existed @ Epoch, origin uncertain
    mutable double         m_trdDelta;  // Pos delta since Epoch due to Trading
    mutable double         m_cumTranss; // Inter-account transfers since Epoch
                                        //   (TransfersIn - TransfersOut)
    mutable double         m_cumDeposs; // External Net Deposits   since Epoch
                                        //   (Deposits    - Withdrawals)
    mutable double         m_cumDebt;   // Net Debts               since Epoch
                                        //   (Borrowed    - LentOut)
    mutable double         m_extTotalPos;
                                        // Total Pos from External srcs
    // POSITIONS in RFC:
    mutable double         m_initRFC;
    mutable double         m_trdDeltaRFC;
    mutable double         m_cumTranssRFC;
    mutable double         m_cumDepossRFC;
    mutable double         m_cumDebtRFC;

    // Appreciations (except Debt which is "not appreciated", because it is to
    // be repaid in this same Asset, not in RFC):
    mutable double         m_apprInitRFC;
    mutable double         m_apprTrdDeltaRFC;
    mutable double         m_apprTranssRFC;
    mutable double         m_apprDepossRFC;

    // The following is for optimisation only (to prevent multiple unsuccessful
    // init attempts):
    mutable long           m_initAttempts;

    //-----------------------------------------------------------------------//
    // Default and Non-Default Ctors:                                        //
    //-----------------------------------------------------------------------//
    // Default Ctor:
    AssetRisks();

    // Was it just created by the Default Ctor?
    bool IsEmpty()  const;

    // Is it non-Empty and having a Valuator installed, so ready for use?
    bool IsReady()  const;

    // Non-Default Ctor:
    AssetRisks
    (
      RiskMgr const*  a_outer,
      UserID          a_user_id,
      SymKey  const&  a_asset,
      int             a_settl_date
    );

    // NB: "ResetValuator" resets previous Valuator settings (which are Transi-
    // ent / Non-ShM). It preserved all cumulative data:
    //
    void ResetValuator();

    // Do we only have a Trivial Valuator?
    bool HasTrivialValuator() const;

    //-----------------------------------------------------------------------//
    // "InstallValuator" (OrderBooks):                                       //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) Because the instrs to be used in valuation may depend on the time of
    //     the day (eg TOD and TOM CcyAsset/RFC rates), up to 2 instrs can be
    //     specified, separated by the roll-over time (in UTC);
    // (*) It is possible to apply an additive spread (positive or negative, se-
    //     parately for Bid and Ask pxs) to the BestBid and BestAsk of the corr-
    //     esp OrderBook; typically, the spread comes from an FX swap:
    // (*) If a Valuator was already installed for this Asset, the old one is
    //     overridden:
    //
    void InstallValuator
    (
      OrderBookBase const* a_ob1,
      double               a_bid_adj1       = 0.0,
      double               a_ask_adj1       = 0.0,
      utxx::time_val       a_roll_over_time = utxx::time_val(),
      OrderBookBase const* a_ob2            = nullptr,
      double               a_bid_adj2       = 0.0,
      double               a_ask_adj2       = 0.0
    );

    //-----------------------------------------------------------------------//
    // "InstallValuator" (Fixed Asset/RFC Rate):                             //
    //-----------------------------------------------------------------------//
    // Again, if a Valuator was already installed for this asset, the old one
    // is overridden:
    //
    void InstallValuator(double a_fixed_rate);

    //-----------------------------------------------------------------------//
    // "InstallValuator" (copy whatever Valuator from another "AssetRisks"): //
    //-----------------------------------------------------------------------//
    void InstallValuator(AssetRisks const& a_ar);

    //-----------------------------------------------------------------------//
    // "OnTrade":                                                            //
    //-----------------------------------------------------------------------//
    void OnTrade
    (
      ExchOrdID const& a_trade_id,   // For propagation into Call-Back only
      double           a_pos_delta,  // Normally non-0
      utxx::time_val   a_ts          // ExchTS or (if unavail) RecvTS
    )
    const;

    //-----------------------------------------------------------------------//
    // Update on a (possible) Valuator Tick:                                 //
    //-----------------------------------------------------------------------//
    // This is similar to "InstrRisks::OnMktDataUpdate":
    //
    void OnValuatorTick
    (
      OrderBookBase const& a_ob,
      utxx::time_val       a_ts
    )
    const;

    //-----------------------------------------------------------------------//
    // Update on an External Balance Change:                                 //
    //-----------------------------------------------------------------------//
    // NB: It may also initialise the "InitPos" (if not set yet), and change
    // "m_extTotalPos":
    //
    void OnBalanceUpdate
    (
      char const*      a_trans_id,   // Non-NULL
      AssetTransT      a_trans_t,
      double           a_new_total,
      utxx::time_val   a_ts_exch
    );

    //-----------------------------------------------------------------------//
    // "IsApplicableOrderBook":                                              //
    //-----------------------------------------------------------------------//
    // Whether the given OrderBook might be a Valuator for this Asset:
    //
    bool IsApplicableOrderBook(OrderBookBase const& a_ob) const
      { return (m_evalOB1 == &a_ob || m_evalOB2 == &a_ob); }

    //-----------------------------------------------------------------------//
    // "GetValuationRate" (Asset/RFC):                                       //
    //-----------------------------------------------------------------------//
    // May still return an invalid one (eg NaN) if valuation data is not avail-
    // able for some reason. If "FullReCalc" is set, the rate is re-calculated
    // from scratch; otherwise, "m_lastEvalRate" is used if "IsValidRate":
    //
    template<bool FullReCalc>
    double GetValuationRate(utxx::time_val a_now) const;

    //-----------------------------------------------------------------------//
    // Check whether the ValuationRate is potentially valid:                 //
    //-----------------------------------------------------------------------//
    // Infinite, NaN and non-positive vals are NOT valid:
    //
    static bool IsValidRate(double rate)
      { return  IsFinite(rate)  && rate > 0.0; }

    //-----------------------------------------------------------------------//
    // "ToRFC":                                                              //
    //-----------------------------------------------------------------------//
    // Performs the actual Asset->RFC valuation, using the mechanism available
    // at the given time (if multiple mechanisms are available, the actual one
    // is selected according to their priorities):
    // Returns the valuation rate (Asset/RFC), or NaN if a valid rate could not
    // be found.
    // The result is stored in "a_res". If valuation fails for whatever reason
    // (ie NaN is returned), "a_res" is UNCHANGED:
    //
    template<bool FullReCalc>
    double ToRFC
    (
      double         a_asset_qty,
      utxx::time_val a_now,
      double*        a_res   // Non-NULL
    )
    const;

    //-----------------------------------------------------------------------//
    // Totals Calculation:                                                   //
    //-----------------------------------------------------------------------//
    // "GetGrossTotalPos":
    // "Pos" is in the units of this Asset, incl Debt:
    //
    double GetGrossTotalPos() const
      { return m_initPos + m_trdDelta + m_cumTranss + m_cumDeposs + m_cumDebt; }

    // "GetNetTotalPos":
    // As above, but w/o the Debt:
    //
    double GetNetTotalPos()   const
      { return m_initPos + m_trdDelta + m_cumTranss + m_cumDeposs; }

    // "GetTrdPos":
    // Getthe pos originating from Trading alone:
    double GetTrdPos()        const      { return m_trdDelta;      }

    // "GetNetNonTradPos":
    // As above, but due to non-trading transactions only (and again w/o Debt):
    //
    double GetNetNonTradPos() const
      { return m_initPos + m_cumTranss + m_cumDeposs; }

    // "GetNetTotalRFC":
    // Integrated RFC val of "NetTotalPos". NB: There is no "GrossTotalRFC",
    // because Debt is not valued in RFC.   Used in TotalNAV calculations:
    //
    double GetNetTotalRFC()   const
      { return m_initRFC  + m_trdDeltaRFC + m_cumTranssRFC + m_cumDepossRFC; }

    // "GetTrdRFC":
    // Integrated Trading Pos in RFC:
    double GetTrdRFC()        const      { return m_trdDeltaRFC; }

    // "GetNetNonTradRFC":
    // As above, but due to non-trading transactions only (and again w/o Debt):
    //
    double GetNetNonTradRFC() const
      { return m_initRFC  + m_cumTranssRFC   + m_cumDepossRFC;   }
  };
  // End of "AssetRisks"

  //=========================================================================//
  // Call-Back on "AssetRisks" Updates:                                     //
  //=========================================================================//
  using AssetRisksUpdateCB  =
    std::function
    <void
     (
       AssetRisks const&        a_ar,
       char const*              a_trans_id,   // NULL or empty for MtM
       AssetRisks::AssetTransT  a_trans_t,
       double                   a_qty,        // Always >= 0
       double                   a_eval_rate   // (Asset/RFC) rate
     )
    >;
}
// End namespace MAQUETTE
