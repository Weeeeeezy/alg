// vim:ts=2:et
//===========================================================================//
//                         "Basis/OrdMgmtTypes.hpp":                         //
//      Types used by "EConnector_OrdMgmt" and its Derived Connectors        //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/SecDefs.h"
#include "Basis/PxsQtys.h"
#include "Basis/XXHash.hpp"
#include "InfraStruct/Strategy.hpp"
#include <utxx/enum.hpp>
#include <boost/core/noncopyable.hpp>
#include <string>
#include <functional>
#include <utility>
#include <type_traits>
#include <cstring>
#include <climits>

namespace MAQUETTE
{
  //=========================================================================//
  // "AOS" Class: Active Order Status:                                       //
  //=========================================================================//
  // "AOS" is an Equivalence Class of Client Requests ("Req12"s, see below). It
  // holds together a sequence of out-bound application-level Reqs related to a
  // particular Client Order. The typical Order life-cycle is:
  //     New --> multiple Modifications --> Fill or Cancel;
  // thus, there could be multiple "Req12"s per  a single "AOS":
  //
  class  EConnector_MktData;
  class  EConnector_OrdMgmt;
  struct Req12;
  struct Trade;

  struct AOS: public boost::noncopyable
  {
    //=======================================================================//
    // AOS Data Flds:                                                        //
    //=======================================================================//
    OrderID                   const m_id;          // AOS index in PersStorage
    SecDefD const*            const m_instr;       // Instrument; ptr NOT owned
    EConnector_OrdMgmt*       const m_omc;         // Ptr NOT owned,  mb reset
    bool                      const m_isBuy;
    FIX::OrderTypeT           const m_orderType;   // XXX: may be per-Req12,
    FIX::TimeInForceT         const m_timeInForce; //   but OK for now...
    int                       const m_expireDate;  // TimeInForce=GoodTillDate
                                                   // YYYYMMDD
    // Fld indicating the Qty semantics and the type of Qty Arithmetic (Long=
    // Whole | Double=Fractional) used in the OMC and in this AOS:
    QtyTypeT                  const m_qt;
    bool                      const m_withFracQtys;

    // NB: In some Exchanges, an Order which was submitted as an Iceberg (ie
    // with QtyShow < Qty, see Req12) must remain an Iceberg  throughout its
    // life time, even if Qtys change and the Order becomes fully-visible. We
    // adopt the same convention, hence the flag:
    bool                      const m_isIceberg;

    // Ptr to the MOST RECENT "Req12" (ie "Req12"s make a stack):
    mutable Req12*                  m_lastReq;

    // Similar, ptr to the MOST ANCIENT "Req12":
    mutable Req12*                  m_frstReq;

    // All Trades associated with this AOS:   The following ptr points to the
    // MOST RECENT Trade in the chain:
    mutable Trade const*            m_lastTrd;

    // Status Properties of the whole Order:
    mutable bool                    m_isInactive;  // Filled|Cancelled|Failed

    // NB: "m_isCxlPending" is either 0 (not pending cancel), or contains the
    // ReqID of the latest (should actually be unique) cancellation  request.
    // When "m_isInactive" is set, this fld is reset back to 0:
    mutable OrderID                 m_isCxlPending;

    // The total number of Failed Req12s associated with this AOS (for info on-
    // ly; OMCs currently set this value but do not use it):
    mutable int                     m_nFails;

    // The Cumulative Filled Qty. It is maintained for Strategies information
    // only, and is not part of any OMC state mgmt logic.  It should be equal
    // to the sum of Qtys of all "m_lastTrd", and to the sum of all calculated
    // Fill Qtys of all its Reqs:
    mutable QtyU                    m_cumFilledQty;

    // Strategy which owns this AOS. NB: Hash48 is needed when ShM AOS storage
    // is to be re-activated:
    Strategy*                 const m_strategy;
    unsigned long             const m_stratHash48; // 48-bit StratName HashCode

    // Finally, the user-Level Data (eg for Indexing) -- can be changed  by the
    // Client but is not affected by the AOS logic itself. Again, placed at the
    // for better L1 Cache utilisation:
    mutable UserData                m_userData;

    //=======================================================================//
    // AOS Methods:                                                          //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Default Ctor: Creates an empty AOS:                                   //
    //-----------------------------------------------------------------------//
    AOS()
    : m_id           (0),
      m_instr        (nullptr),
      m_omc          (nullptr),
      m_isBuy        (false),
      m_orderType    (FIX::OrderTypeT  ::UNDEFINED),
      m_timeInForce  (FIX::TimeInForceT::UNDEFINED),
      m_expireDate   (0),
      m_qt           (QtyTypeT::UNDEFINED),
      m_withFracQtys (false),
      m_isIceberg    (false),
      m_lastReq      (nullptr),
      m_frstReq      (nullptr),
      m_lastTrd      (nullptr),
      m_isInactive   (false),       // XXX: But not active either!
      m_isCxlPending (0),
      m_nFails       (0),
      m_cumFilledQty (),            // 0
      m_strategy     (nullptr),
      m_stratHash48  (0),
      m_userData     ()
    {}

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // NB: There are no Qty args from which QT and QR could be inferred, so we
    // have to pass them directly:
    AOS
    (
      QtyTypeT            a_qt,              // From the owning OMC
      bool                a_with_frac_qtys,  // same
      Strategy*           a_strategy,        // Strat which placed that order
      OrderID             a_id,              // ReqID of the 1st  order req
      SecDefD const*      a_instr,           // Order Instrument
      EConnector_OrdMgmt* a_omc,             // The owning OMC
      bool                a_is_buy,          // Order side
      FIX::OrderTypeT     a_order_type,      // Other order details
      bool                a_is_iceberg,      // Mark it as Iceberg for life?
      FIX::TimeInForceT   a_time_in_force,   //
      int                 a_expire_date      //
    )
    : m_id           (a_id),
      m_instr        (a_instr),
      m_omc          (a_omc),
      m_isBuy        (a_is_buy),
      m_orderType    (a_order_type),
      m_timeInForce  ((m_orderType != FIX::OrderTypeT::Market) // IoC for Mkt!
                      ? a_time_in_force
                      : FIX::TimeInForceT::ImmedOrCancel),
      m_expireDate   (a_expire_date),
      m_qt           (a_qt),
      m_withFracQtys (a_with_frac_qtys),
      m_isIceberg    (a_is_iceberg),
      m_lastReq      (nullptr),    // Inited later when a "Req12" is attached
      m_frstReq      (nullptr),    // Same
      m_lastTrd      (nullptr),
      m_isInactive   (false),
      m_isCxlPending (0),
      m_nFails       (0),          // None yet
      m_cumFilledQty (),           // 0
      m_strategy     (a_strategy),
      m_stratHash48  (a_strategy->GetHash48()),
      m_userData     ()
    {
      // NB: Some ptrs are allowed to be NULL (eg any Call-Backs), but others
      // are not:
      CHECK_ONLY
      (
        if (utxx::unlikely(m_instr == nullptr))
          throw utxx::badarg_error
                ("AOS::Ctor: Instrument must be non-NULL");

        // Check that if it is a Market Order, we did not request an improper
        // TimeInForce:
        if (utxx::unlikely
           (a_order_type    == FIX::OrderTypeT::Market          &&
            a_time_in_force != FIX::TimeInForceT::ImmedOrCancel &&
            a_time_in_force != FIX::TimeInForceT::UNDEFINED))
          throw utxx::badarg_error
                ("AOS::Ctor: TimeInForce=", char(a_time_in_force), " is "
                 "incompatble with Market order");
      )
    }
    //-----------------------------------------------------------------------//
    // Dtor:                                                                 //
    //-----------------------------------------------------------------------//
    // Dtor is trivial, and is auto-generated. XXX: We don't reset all ptrs to
    // NULL because AOSes are never actually de-allocated,   only destroyed en
    // masse along with their daily pool.

    //-----------------------------------------------------------------------//
    // "IsFilled", "IsCancelled", "HasFailed":                               //
    //-----------------------------------------------------------------------//
    // The ultimate fate (it it has already occurred) of this AOS:
    //
    bool IsFilled   () const;
    bool IsCancelled() const;
    bool HasFailed  () const;

    //-----------------------------------------------------------------------//
    // Qty Accessors (with dynamic Qty Type checking):                       //
    //-----------------------------------------------------------------------//
    //-----------------------------------------------------------------------//
    // "GetLeavesQty":                                                       //
    //-----------------------------------------------------------------------//
    // Finds the latest non-Cancel Req and returns its UnFilled (Leaves)  Qty.
    // NB: This Req MAY also be an Indication; if we exclude Indications, then
    // we should probably exclude UnConfirmed Reqs as well, and that would be
    // too obscure. So:
    //
    template<QtyTypeT QT, typename QR, bool ConvReps=false>
    Qty<QT,QR> GetLeavesQty() const;

    //-----------------------------------------------------------------------//
    // "GetCumFilledQty":                                                    //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT, typename QR, bool ConvReps=false>
    Qty<QT,QR> GetCumFilledQty() const
    {
      // Use dynamic type checking here (may be eliminated by the optimiser):
      Qty<QT,QR>    res =
        m_cumFilledQty.GetQty<QT,QR,ConvReps>(m_qt, m_withFracQtys);
      assert(!IsNeg(res));
      return res;
    }
  };

  //=========================================================================//
  // "Req12" Struct:                                                         //
  //=========================================================================//
  // A short summary of an order / request made, for State Management purposes
  // within the "AOS" (see below):
  //
  struct Req12: public boost::noncopyable
  {
    //-----------------------------------------------------------------------//
    // Enums:                                                                //
    //-----------------------------------------------------------------------//
    //-----------------------------------------------------------------------//
    // "Req12::StatusT":                                                     //
    //-----------------------------------------------------------------------//
    // Status of any Pending (Active) Request:
    // NB:
    // (*) If the request itself was a "Cancel", then "Confirmed" means that
    //     the corresp Order was indeed cancelled (and then the whole AOS is
    //     removed), and the cancellation subject is marked as "Cancelled";
    // (*) The order of declarations is critically important here  -- Status
    //     Priorities are based on it;
    // (*) There is no "PartFilled" status (though it could in theory come af-
    //     ter "Confirmed") -- this property applies to the whole AOS, not to
    //     Req12:
    //
    UTXX_ENUM(
    StatusT, int,
      // Active States (NB: "Indicated" is also considered to be "Active", but
      // on the Client Side yet):
      Indicated,    // Not even sent yet, because of Flow Ctl (!PipeLinedReqs)
      New,          // Just sent up by the Client / Connector
      Acked,        // Acknowledgement received from the Exchange
      Confirmed,    // Confirmation    received from the Exchange
      PartFilled,   // Implies "Confirmed"

      // InActive States:
      Cancelled,    // End-of-Life for the whole AOS
      Replaced,     // End-of-Life for this Req12: became another "Req12"
      Failed,       // End-of-Life for this Req12 (eg Rejected); the whole AOS
                    //   may or may not have failed as well
      Filled        // End-of-Life for the whole AOS
    );

    //-----------------------------------------------------------------------//
    // "Req12::KindT":                                                       //
    //-----------------------------------------------------------------------//
    // This is a kind of Request within the current AOS (NOT the type  of the
    // whole AOS -- the latter is given by FIX::OrderTypeT, see below);
    // (*) for Limit Orders (as specified in the AOS), all "KindT" values are
    //     applicable (throughout the order lifetime);
    // (*) for Market orders,  there could only be "New";
    // (*) XXX: "MassCancel" currently does not have a state on its own, so is
    //     not in "KindT";
    // (*) "ModLegC" and "ModLegN" are the 2 legs of a "Cancel-New" Tandem emu-
    //     lating a "Modify"   (if the latter is not directly supported by the
    //     Exchange);
    // (*) "EConnector_OrdMgmt" provides search for "Req12" whose Kind belongs
    //     to a specified set, hence we need the following binary encoding (pow
    //     of 2):
    UTXX_ENUMV(KindT, (unsigned, 0),
      (New,      1)
      (Modify,   2)
      (Cancel,   4)
      (ModLegC,  8)
      (ModLegN, 16)
    );
    // BitMask of the above vals which corresponds to ANY "KindT":
    constexpr static unsigned AnyKindT = 31;

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) The name "Req12" is historical and does not currently imply that the
    //     struct contains 12 flds (once upon a time, it did);
    // (*) ReqID is typically the ClOrdID (though it might be different is some
    //     rare cases, eg in BitFinex);
    // (*) LinkedID is currently UNUSED; it is reserved for Multi-Legged orders
    //     (eg Swaps, Hedged Orders or Order+Stop) if supported by the corresp
    //     Exchange:
    //
    AOS*     const         m_aos;          // Ptr to the AOS this Req belongs to
    OrderID  const         m_id;           // ReqID of this Req12
    OrderID  const         m_linkedID;     // ID of the Linked Req (RESERVED)
    OrderID  const         m_origID;       // If it is a Cancel|Modify: Target
    QtyTypeT const         m_qt;           // For strict Qty typing; same as in
    bool     const         m_withFracQtys; //   the AOS (just a short-cut)
    KindT    const         m_kind;
    PriceT   const         m_px;           // NaN for Cancel or Mkt Orders;
    bool     const         m_isAggr;       // Intended to be Aggressive?
    QtyU     const         m_qty;          // OrigQty of this Req, 0 for Cancel
    QtyU     const         m_qtyShow;      // 0 if not used
    QtyU     const         m_qtyMin;       // 0 if not used
    bool     const         m_pegSide;      // True: This side; False: Opposite
    double   const         m_pegOffset;    // NaN if no pegging

    mutable SeqNum         m_seqNum;       // Not a const: unknown before send
    mutable QtyU           m_leavesQty;    // Yet unfilled Qty
    mutable StatusT        m_status;       // Current status of this Req12
    mutable utxx::time_val m_throttlUntil; // If Indicated due to Throttling
    mutable bool           m_willFail;     // Not failed yet, but surely will!
    mutable bool           m_probFilled;   // Probably Filled (not sure yet)

    // NB: The "m_next"/"m_prev" relationship is simply the order of Req12s
    // creation within the given AOS. It would normally be  the same as the
    // "m_id"/"m_origID" relationship, but not always  -- the discrepancies
    // occur when there are Req12 failures (m_status==Failed || m_willFail):
    //
    mutable Req12*         m_prev;         // Previous req   related to this AOS
    mutable Req12*         m_next;         // Next (MORE RECENT) Req in this AOS

    // Time Stamps (NB: PartialFills are not recorded and not Time-Stamped):
    //
    utxx::time_val   const m_ts_md_exch;   // ExchTime      of triggering event
    utxx::time_val   const m_ts_md_conn;   // ConnectorTime of triggering event
    utxx::time_val   const m_ts_md_strat;  // StrategyTime  of triggering event
    utxx::time_val   const m_ts_created;   // When created  by the Strategy
    mutable utxx::time_val m_ts_sent;      // When sent     to the Xch by Conn
    mutable utxx::time_val m_ts_conf_exch; // When confed   by Xch (their time)
    mutable utxx::time_val m_ts_conf_conn; // When confirm  received by Conn
    mutable utxx::time_val m_ts_end_exch;  // When Filled/Cancelled/Failed @Exg
    mutable utxx::time_val m_ts_end_conn;  // When that received by Connector
    // NB:
    // (*) Initial Acknowledgement times are not recorded -- not supported  by
    //     many of the underlying Protocols;
    // (*) Cancellation or Modification time of the order given by resp AOS is
    //     the Confirmation time of the corersp "Cancel" or "Modify" Req12;
    // (*) Trade or Error time stamps are not provided here because such events
    //     occur not for every Request; rather, those time stamps are communi-
    //     cated via Strategy Call-Backs;
    // (*) The INTERNAL latency of this "Req12" is (m_ts_sent - m_ts_md_conn),
    //     see below:
    // The following flds are 32 bytes each, hence placed at the end, to improve
    // L1 Cache utilisation:
    mutable ExchOrdID      m_exchOrdID;    // Assigned on Confirm
    mutable ExchOrdID      m_mdEntryID;    // ditto

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    Req12()
    : m_aos           (nullptr),
      m_id            (0),
      m_linkedID      (0),
      m_origID        (0),
      m_qt            (QtyTypeT::UNDEFINED),
      m_withFracQtys  (false),
      m_kind          (KindT::UNDEFINED),
      m_px            (),  // NaN
      m_isAggr        (false),
      m_qty           (),  // Zero
      m_qtyShow       (),  //
      m_qtyMin        (),  //
      m_pegSide       (false),
      m_pegOffset     (NaN<double>),
      m_seqNum        (0),
      m_leavesQty     (),  // Zero
      m_status        (StatusT::UNDEFINED),
      m_throttlUntil  (),
      m_willFail      (false),
      m_probFilled    (false),
      m_prev          (nullptr),
      m_next          (nullptr),
      // All TimeStamps are initialised to empty (0) values:
      m_ts_md_exch    (),
      m_ts_md_conn    (),
      m_ts_md_strat   (),
      m_ts_created    (),
      m_ts_sent       (),                   // Not yet
      m_exchOrdID     (),
      m_mdEntryID     ()
    {}

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // XXX: Params are NOT checked here, to avoid too many stages of verificat-
    // ion. They are verified on the Client side,  then by the AOS Ctor   (but
    // that only applies to Reqs which go into new AOSes), and then finally by
    // the corresponding Connector when the Req12 is acted upon.
    //
    // NB: If "AttachToAOS" is False, the Req12 constructed still points to the
    // given AOS, but not attached as "m_lastReq" to it:
    //
    template<QtyTypeT QT, typename QR>
    Req12
    (
      AOS*             a_aos,
      bool             a_attach_to_aos,
      OrderID          a_id,
      OrderID          a_orig_id,
      KindT            a_req_kind,
      PriceT           a_px,
      bool             a_is_aggr,
      Qty<QT,QR>       a_qty,
      Qty<QT,QR>       a_qty_show,
      Qty<QT,QR>       a_qty_min,
      bool             a_peg_side,
      double           a_peg_offset,
      utxx::time_val   a_ts_md_exch,  // Originating Event's TS
      utxx::time_val   a_ts_md_conn,  // When it was received by the Connector
      utxx::time_val   a_ts_md_strat, // When it was received by the Strategy
      utxx::time_val   a_ts_created = utxx::now_utc ()  // If no explicit value
    )
    : m_aos           (a_aos),        // NB: In any case (even if not Attached)
      m_id            (a_id),
      m_linkedID      (0),            // NB: To be set separately if used
      m_origID        (a_orig_id),    // 0 for New, > 0 otherwise
      m_qt            (QT),
      m_withFracQtys  (std::is_floating_point_v<QR>),
      m_kind          (a_req_kind),
      m_px            (a_px),
      m_isAggr        (a_is_aggr),
      m_qty           (a_qty),
      m_qtyShow       (a_qty_show),
      m_qtyMin        (a_qty_min),
      m_pegSide       (a_peg_side),
      m_pegOffset     (a_peg_offset),
      m_seqNum        (0),            // Not yet
      m_leavesQty     (a_qty),        // Not filled yet
      m_status        (StatusT::Indicated), // Because not sent out yet!
      m_throttlUntil  (),
      m_willFail      (false),        // Not failing yet...
      m_prev          (nullptr),
      m_next          (nullptr),      // Non-NULL cases are rare...
      m_ts_md_exch    (a_ts_md_exch),
      m_ts_md_conn    (a_ts_md_conn),
      m_ts_md_strat   (a_ts_md_strat),
      m_ts_created    (a_ts_created),
      m_ts_sent       (),             // Not yet
      // Other TimeStamps are initialised to empty (0) values
      m_exchOrdID     (),             // Not yet
      m_mdEntryID     ()              // Not yet
    {
      CHECK_ONLY
      (
        // All ReqIDs are assigned in a monotonic order from 1. Any Req except
        // New or ModLegN must have the Orig (Target) one:
        bool isAnyN = (m_kind == KindT::New    || m_kind == KindT::ModLegN);
        bool isAnyC = (m_kind == KindT::Cancel || m_kind == KindT::ModLegC);
        if (utxx::unlikely
           (m_id == 0 || m_id <= m_origID || (m_origID == 0) != isAnyN))
          throw utxx::badarg_error
                ("Req12::Ctor: Invalid ID(s): ReqID=", m_id, ", OrigID=",
                 m_origID,  ", Kind=", m_kind.to_string());

        // Unless it is a Cancel or ModLegC, Qty must be positive, otherwise 0:
        if (utxx::unlikely((!isAnyC && !IsPos     (a_qty))  ||
                           ( isAnyC && !IsSpecial0(a_qty))))
          throw utxx::badarg_error
                ("Req12::Ctor: Invalid Qty=", QR(a_qty), ", Kind=",
                 int(m_kind));

        // QtyShow and QtyMin should be consistent with Qty:
        if (utxx::unlikely
           (isAnyN &&
            (IsNeg(a_qty_show) || a_qty_show > a_qty ||
             IsNeg(a_qty_min)  || a_qty_min  > a_qty)))
          throw utxx::badarg_error
                ("Req12::Ctor: Inconsistent Qtys: Qty=",    QR(a_qty),
                 ", QtyShow=", QR(a_qty_show), ", QtyMin=", QR(a_qty_min));

        // The AOS ptr must always be non-NULL:
        if (utxx::unlikely(m_aos == nullptr))
          throw utxx::badarg_error("Req12::Ctor: AOS Ptr must not be NULL");
      )
      // Cancel Reqs are NON-aggressive by definition:
      assert(!(isAnyC && m_isAggr));

      // Now attach this "Req12" to the specified "AOS", if AttachToAOS is set;
      // AOS.m_lastReq points to the most recent "Req12", and "m_back"/"m_next"
      // ptrs are consistent with chronological order:
      //
      if (a_attach_to_aos)
      {
        m_prev = m_aos->m_lastReq;
        if (utxx::likely(m_prev != nullptr))
        {
          assert(m_prev->m_next == nullptr); // Since it was the most recent one
          m_prev->m_next = this;
        }
        CHECK_ONLY
        (
        else
        if (utxx::unlikely(!IsPos(a_qty)))
          throw utxx::badarg_error
                ("Req12::Ctor: First Req12 for an AOS must have Qty > 0");
        )
        // NB: this->m_next remains NULL

        // If this is the first Req for that AOS, set AOS.m_frstReq:
        if (utxx::unlikely(m_aos->m_frstReq == nullptr))
        {
          assert(m_aos->m_lastReq == nullptr);
          m_aos->m_frstReq = this;
        }
        // And the LastReq is set in any case:
        m_aos->m_lastReq   = this;
      }
      // All Done!
    }

    // The Dtor is trivial (does nothing), and is auto-generated

    //-----------------------------------------------------------------------//
    // Properties:                                                           //
    //-----------------------------------------------------------------------//
    bool IsInactive() const
      // XXX: Be careful here -- the order of "StatusT" decls is important!!!
      { return (m_status >= StatusT::Cancelled); }

    // Whether this Req12 is Pending Modify:
    // That happens iff it is not the last one in a chain, and the following
    // one is a "Modify" or "ModLegC" Req, and this Req has not been Replaced
    // or Cancelled itself yet:
    //
    bool IsModPending() const
    {
      if (m_next == nullptr)
        return false;
      assert(m_next->m_kind != KindT::New);
      return
        (m_next->m_kind == KindT::Modify   ||
         m_next->m_kind == KindT::ModLegC) && !IsInactive();
    }

    // Whether this Req12 is Pending Cancel (then the AOS is Pending Cancel as
    // well, but the converse is not true). NB: "ModLegC" next is NOT conside-
    // red to be Pending Cancel; it is Pending Modify!
    //
    bool IsCxlPending() const
    {
      if (m_next == nullptr)
        return false;
      assert(m_next->m_kind != KindT::New && m_aos != nullptr);

      bool    res =  m_next->m_kind == KindT::Cancel && !IsInactive();
      assert(!res || m_aos ->m_isCxlPending);
      return  res;
    }

    // Ptr to the enclosing "EConnector_OrdMgmt" (always non-NULL, but for cla-
    // rity, we prefer a Ptr to a Ref):
    EConnector_OrdMgmt* GetOMC() const { return m_aos->m_omc; }

    //-----------------------------------------------------------------------//
    // Internal Latency Measurement (in usec):                               //
    //-----------------------------------------------------------------------//
    int GetInternalLatency()
    {
      return
        (utxx::likely(!m_ts_sent.empty() && !m_ts_md_conn.empty()))
        ? int((m_ts_sent - m_ts_md_conn).microseconds())
        : 0;
    }

    //-----------------------------------------------------------------------//
    // Qty Accessors:                                                        //
    //-----------------------------------------------------------------------//
    // In the Debug Mode, they provide dynamic qty type checks:
    //
    template<QtyTypeT QT,   typename QR, bool ConvReps=false>
    Qty<QT,QR> GetQty()     const
      { return  m_qty      .GetQty<QT,QR,ConvReps>(m_qt, m_withFracQtys); }

    template<QtyTypeT QT,   typename QR, bool ConvReps=false>
    Qty<QT,QR> GetQtyShow() const
      { return  m_qtyShow  .GetQty<QT,QR,ConvReps>(m_qt, m_withFracQtys); }

    template<QtyTypeT QT,   typename QR, bool ConvReps=false>
    Qty<QT,QR> GetQtyMin()  const
      { return  m_qtyMin   .GetQty<QT,QR,ConvReps>(m_qt, m_withFracQtys); }

    template<QtyTypeT QT,   typename QR, bool ConvReps=false>
    Qty<QT,QR> GetLeavesQty()  const
      { return  m_leavesQty.GetQty<QT,QR,ConvReps>(m_qt, m_withFracQtys); }
  };

  //=========================================================================//
  // "MkTmpReqID": Creates a ReqID from a NanoSecond TimeStamp:              //
  //=========================================================================//
  inline OrderID MkTmpReqID(utxx::time_val a_ts)
  {
    // XXX: The result is 31-bit long only (do we need a longer one?):
    long   res = a_ts.nanoseconds() & 0x7FFFFFFFL;
    assert(res > 0);
    return OrderID(res);
  }

  //=========================================================================//
  // AOS Properties: We can now implement them (in terms of "Req12"s):       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "AOS::IsFilled":                                                        //
  //-------------------------------------------------------------------------//
  inline bool AOS::IsFilled() const
  {
    // Short-cut: To avoid scanning the whole chain of "Req12"s to no avail,
    // return "false" immediately if the AOS is not Inactive:
    if (!m_isInactive)
      return false;

    // Then traverse all "Req12"s backwards, skipping "Cancel" and "ModLegC"
    // ones, as of course, they cannot be Filled:
    //
    for (Req12 const* req = m_lastReq; req != nullptr; req = req->m_prev)
    {
      if (req->m_kind == Req12::KindT::Cancel ||
          req->m_kind == Req12::KindT::ModLegC)
        continue;

      if (req->m_status == Req12::StatusT::Filled)
        return true;    // Yes, found a "Filled" Req!

      if (req->m_status == Req12::StatusT::Cancelled)
        return false;   // Cancelled, not Filled!
    }
    // If we got here and did not encounter a "Filled" or "Cancelled" Req, this
    // probably means that we are in a "Failed" state -- the very first Req has
    // Failed. If this is not the case, raise an exception:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (m_frstReq == nullptr ||
          m_frstReq->m_status  != Req12::StatusT::Failed))
        throw utxx::logic_error
              ("AOS::IsFilled: AOSID=", m_id, ": Expected Status=Failed, but "
               "Failed Req not found");
    )
    // In any case, if we got here, we are NOT Filled:
    return false;
  }

  //-------------------------------------------------------------------------//
  // "AOS::IsCancelled":                                                     //
  //-------------------------------------------------------------------------//
  inline bool AOS::IsCancelled() const
  {
    // Again, return "false" immediately if the AOS is not Inactive:
    if (!m_isInactive)
      return false;

    // Search for a Req with a "Cancelled" status:
    for (Req12 const* req = m_lastReq; req != nullptr; req = req->m_prev)
    {
      // "Cancel" Reqs by themselves are considered as well -- if such a Req is
      // "Confirmed",  there is no need to look futher:
      if (req->m_kind   == Req12::KindT::Cancel    &&
          req->m_status == Req12::StatusT::Confirmed)
        return true;

      if (req->m_status == Req12::StatusT::Cancelled)
      {
        // It should be a "New" or "Modify" or "ModLegN" Req, of course.
        // If not, it is a serious error:
        CHECK_ONLY
        (
          if (utxx::unlikely
             (req->m_kind != Req12::KindT::New    &&
              req->m_kind != Req12::KindT::Modify &&
              req->m_kind != Req12::KindT::ModLegN))
            throw utxx::logic_error
                  ("AOS::IsCancelled: AOSID=",    m_id, ": ReqID=", req->m_id,
                   " is Cancelled but its Kind=", int(req->m_kind));
        )
        // If OK: We are done. NB: Normally, we would not get here at all --
        // the first criterion above would be applied; but this is not so in
        // case of Mass Cancellations:
        return true;
      }

      // If, on the other hand, the Req is Filled, it is not Cancelled:
      if (req->m_status == Req12::StatusT::Filled)
        return false;

      // Otherwise, continue further...
    }
    // If we got here: The order should have Failed (as it is anyway Inactive,
    // but we did not encounter Cancelled or Filled Reqs). If not, it is a lo-
    // gic error:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (m_frstReq == nullptr ||
          m_frstReq->m_status  != Req12::StatusT::Failed))
        throw utxx::logic_error
              ("AOS::IsCancelled: AOSID=", m_id, ": Expected Status=Failed, "
               "but Failed Req not found");
    )
    // In any case, if we got here, we are NOT Cancelled:
    return false;
  }

  //-------------------------------------------------------------------------//
  // "AOS::HasFailed":                                                       //
  //-------------------------------------------------------------------------//
  inline bool AOS::HasFailed() const
  {
    // Again, return "false" immediately if the AOS is not Inactive:
    if (!m_isInactive)
      return false;

    // Then check the First Req of this AOS -- only its failure can give rise
    // to an over-all Failed Status. XXX: But this is not proved formally:
    return (m_frstReq != nullptr &&
            m_frstReq->m_status  == Req12::StatusT::Failed);
  }

  //-------------------------------------------------------------------------//
  // "AOS::GetLeavesQty":                                                    //
  //-------------------------------------------------------------------------//
  // NB: The LeavesQty of the AOS is the LeavesQty of the last non-Cancel Req:
  //
  template<QtyTypeT QT, typename QR, bool ConvReps>
  inline   Qty<QT,QR> AOS::GetLeavesQty() const
  {
    for (Req12 const* req = m_lastReq; req != nullptr; req = req->m_prev)
    {
      assert(req->m_qt == m_qt && req->m_withFracQtys == m_withFracQtys);

      // So Cancel and ModLegC Reqs are skipped. Otherwise, perform the calcul-
      // ations:
      if (req->m_kind != Req12::KindT::Cancel &&
          req->m_kind != Req12::KindT::ModLegC)
      {
        Qty<QT,QR> leavesQty = req->GetLeavesQty<QT,QR,ConvReps>();
#       ifndef NDEBUG
        Qty<QT,QR> reqQty    = req->GetQty      <QT,QR,ConvReps>();
#       endif
        assert(IsPos(reqQty) && !IsNeg(leavesQty) && reqQty >= leavesQty);
        return leavesQty;
      }
    }
    // We MUST NOT get here: This would mean that there were no non-Cancel
    // Reqs for this AOS, so throw an exception:
    throw utxx::logic_error
          ("AOS::LeavesQty: AOSID=", m_id, ": No Non-Cancel Reqs found?");
  }

  //=========================================================================//
  // "Trade" Struct (Same for MDC / OMC / STP):                              //
  //=========================================================================//
  // NB:
  // (*) Same struct is used for both 3rd-Party Trades (eg coming from  an MDC
  //     ot STP) and our own Trades (normally coming from an OMC, but they can
  //     also be inferred from the MDC data, or from STP perhaps);
  // (*) If it is our own Trade, then Req12 ptrs is non-NULL (so we know which
  //     Req exactly resulted in this Trade); from that, AOS and OMC ptrs  can
  //     be found; MDC ptr may also be non-NULL if our Trade was first inferred
  //     from the MDC data;
  // (*) For 3rd-party Trades, MDC ptr is always non-NULL, and Req12 ptr is al-
  //     ways NULL;
  // (*) This struct is perfectly copyable:
  //
  struct Trade
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // Meta-Data:
    OrderID             const   m_id;         // Index in the ShM array, >= 0
    EConnector_MktData* const   m_mdc;        // If it first came from an MDC
                                              //   (or MDC only)
    SecDefD  const*     const   m_instr;      // Ptr NOT owned, always non-NULL
    Req12    const*     const   m_ourReq;     // Non-NULL iff OUR OWN Trade (ptr
                                              //   not owned)
    unsigned            const   m_accountID;  // To whom this Trade is attrbtd

    // Trade Itself:
    ExchOrdID           const   m_execID;     // Assigned by the Exchange
    PriceT              const   m_px;         // Trade Px
    QtyTypeT            const   m_qt;         // For strict Qty typing...
    QtyTypeT            const   m_qf;         // As above, but for the Fee
    bool                const   m_withFracQtys;
                                              // NB: Fees are always Fractional
    QtyU                const   m_qty;        // Trade Size (Qty)
    QtyU                const   m_fee;        // Commission / Fee
    FIX::SideT          const   m_aggressor;  // Buy: BidAggr;  Sell: AskAggr
    FIX::SideT          const   m_accSide;    // Trade Side for AccountID holder

    // XXX: Currently, SettlDate is always taken from the Instr!

    // Time Stamps:
    utxx::time_val      const   m_exchTS;     // Trade TS, from the Exchange
    utxx::time_val      const   m_recvTS;     // When received

    // Prev (MORE ANCIENT) and Next (MORE RECENT) Trades for the AOS they bel-
    // ong to:
    mutable Trade const*        m_prev;
    mutable Trade const*        m_next;

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    Trade()
    : m_id          (0),
      m_mdc         (nullptr),
      m_instr       (nullptr),
      m_ourReq      (nullptr),
      m_accountID   (0),
      m_execID      (),
      m_px          (),
      m_qt          (),
      m_qf          (),
      m_withFracQtys(false),
      m_qty         (),
      m_fee         (),
      m_aggressor   (FIX::SideT::UNDEFINED),
      m_accSide     (FIX::SideT::UNDEFINED),
      m_exchTS      (),
      m_recvTS      (),
      m_prev        (nullptr),
      m_next        (nullptr)
    {}

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // NB: Different reps can be used for the TradedQty and for the Fee. Eg the
    // former could be in Integral Contracts,  whereas the latter in Fractional
    // QtyB units:
    //
    template<QtyTypeT QT, typename QR, QtyTypeT QTF, typename QRF>
    Trade
    (
      OrderID                   a_id,
      EConnector_MktData*       a_mdc,
      SecDefD   const*          a_instr,
      Req12     const*          a_our_req,
      unsigned                  a_account_id,
      ExchOrdID const&          a_exec_id,
      PriceT                    a_px,
      Qty<QT, QR>               a_qty,
      Qty<QTF,QRF>              a_fee,
      FIX::SideT                a_aggressor,
      FIX::SideT                a_acc_side,
      utxx::time_val            a_ts_exch,
      utxx::time_val            a_ts_recv
    )
    : m_id                     (a_id),
      m_mdc                    (a_mdc),
      m_instr                  (a_instr),
      m_ourReq                 (a_our_req),
      m_accountID              (a_account_id),
      m_execID                 (a_exec_id),
      m_px                     (a_px),
      m_qt                     (QT),
      m_qf                     (QTF),
      m_withFracQtys           (std::is_floating_point_v<QR>),
      m_qty                    (a_qty),
      m_fee                    (a_fee),
      m_aggressor              (a_aggressor),
      m_accSide                (a_acc_side),
      m_exchTS                 (a_ts_exch),
      m_recvTS                 (a_ts_recv),
      m_prev                   (nullptr),
      m_next                   (nullptr)
    {
      // Either Req12 is NULL, or it must be consistent with the Instr, which
      // is always non-NULL:
      assert(m_instr  != nullptr &&
            (m_ourReq == nullptr || m_ourReq->m_aos->m_instr == a_instr));

      // NB: Empty EventTS is allowed, but RecvTS must always be known; also,
      // Aggressor may be unknown; Px and Qty must always be valid:
      CHECK_ONLY
      (
        if (utxx::unlikely(m_recvTS.empty()))
          throw utxx::badarg_error("Trade::Ctor: Invalid TimeStamp: " +
                std::to_string(m_recvTS.microseconds()));

        if (utxx::unlikely(!IsFinite(m_px)))
          throw utxx::badarg_error("Trade::Ctor: Invalid price: " +
                std::to_string(double(m_px)));

        if (utxx::unlikely(!IsPos(m_qty)))
          throw utxx::badarg_error("Trade::Ctor: Invalid qty: " +
              std::to_string(double(GetQty<QT,QR>())));
      )
    }
    //-----------------------------------------------------------------------//
    // The Dtor is trivial, and is auto-generated!                           //
    //-----------------------------------------------------------------------//
    //-----------------------------------------------------------------------//
    // Properties:                                                           //
    //-----------------------------------------------------------------------//
    // Is it our own Trade?
    //
    bool IsOurTrade() const
    {
      // NB: For 3rd-party Trades, MDC is always non-NULL; for our own Trades,
      //     MDC can be any, but "m_ourReq" is non-NULL. Thus:
      assert (m_ourReq != nullptr || m_mdc != nullptr);

      // The result:
      return (m_ourReq != nullptr);
    }

    //-----------------------------------------------------------------------//
    // "GetQty" (for the Qty Traded):                                        //
    //-----------------------------------------------------------------------//
    template<QtyTypeT QT,  typename QR, bool ConvReps=false>
    Qty<QT,QR>   GetQty()  const
      { return m_qty.GetQty<QT,QR,ConvReps>(m_qt, m_withFracQtys); }

    //-----------------------------------------------------------------------//
    // "GetFee":                                                             //
    //-----------------------------------------------------------------------//
    // NB: The Fee is always assumed to be Fractional, though "QRF" mat be any:
    //
    template<QtyTypeT QTF, typename QRF, bool ConvReps=false>
    Qty<QTF,QRF> GetFee()  const
      { return m_fee.GetQty<QTF,QRF,ConvReps>(m_qf, true); }
  };
} // End namespace MAQUETTE
