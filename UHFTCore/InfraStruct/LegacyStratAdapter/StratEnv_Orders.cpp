// vim:ts=2:et:sw=2
//===========================================================================//
//                           "StratEnv_Orders.cpp":                          //
//                          Sending the Requests Up:                         //
//===========================================================================//
#include "Infrastructure/MiscUtils.h"
#include "Connectors/XConnector_OrderMgmt.h"
#include "Connectors/AOS.hpp"
#include "StrategyAdaptor/StratEnv.h"
#include <utxx/compiler_hints.hpp>
#include <Infrastructure/Logger.h>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <cerrno>

namespace MAQUETTE
{
  using namespace std;
  using namespace Arbalete;

  //=========================================================================//
  // "CheckQSymAOS":                                                         //
  //=========================================================================//
  bool StratEnv::CheckQSymAOS
  (
     QSym const& qsym,
     AOS  const* aos,
     char const* where
  )
  const
  {
    // Check the QSym:
    if (utxx::unlikely
       (*qsym.m_symKey.data() == '\0'          ||
       ((qsym.m_mdcN != -1 && qsym.m_mdcN < 0) ||
         qsym.m_mdcN >= int(m_MDs.size()))    ||
       ((qsym.m_omcN != -1 && qsym.m_omcN < 0) ||
         qsym.m_omcN >= int(m_OEs->size()))  ))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s: Invalid QSym=%s\n",
                  where, qsym.ToString().c_str());
      return false;
    }

    // If the AOS Ptr is given, its Symbol and Ref(s) must be consistent with
    // those provided by QSym.   In particular, if AOS is used, then the QSym
    // must have a valid OE:
    if (aos == NULL)
      return true;

    if (utxx::unlikely(qsym.m_omcN < 0))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s: AOS given, but no OE in QSym\n", where);
      return false;
    }

    if (utxx::unlikely(qsym.m_symKey != aos->m_symbol))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s: Symbols MisMatch: AOS=%s, QSym=%s\n",
                  where, aos->m_symbol.data(), qsym.m_symKey.data());
      return false;
    }

    int ref = int(m_MDs.size()) + qsym.m_omcN;
    if (utxx::unlikely(aos->CliRef() != ref))
    {
      // XXX: It would be nice to check aos->m_info->m_rf as well, but unfor-
      // tunately, aos->m_info points to a Connector's memory segment rather
      // than ShM, so do NOT try de-reference it -- will get a segfault!
      //
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s: ClientRefs MisMatch: AOS=%d, QSym=%d\n",
                  where, aos->CliRef(), ref);
      return false;
    }
    // If we got here, all is fine:
    return true;
  }

  //=========================================================================//
  // "NewOrderImpl":                                                         //
  //=========================================================================//
  AOSReq12* StratEnv::NewOrderImpl
  (
    QSym const&          qsym,
    OrderTypeT           ord_type,
    SideT                side,
    double               price,            // NaN if Mkt Order, peg_diff
    long                 qty,              //             if "is_pegged"
    Events::AccCrypt     acc_crypt,
    AOS::UserData const* user_data,
    FIX::TimeInForceT    time_in_force,
    utxx::time_val       expire_date,
    bool                 this_side_only,   // For Pegged or MassCancel orders
    long                 show_qty,
    long                 min_qty,
    utxx::time_val       ts,
    char const*          where
  )
  {
    assert(side != SideT::Undefined || ord_type == OrderTypeT::MassCancel);

    // Check the args:
    if (!CheckQSymAOS(qsym, NULL, where))
      return NULL;

    // Find the required OE:
    if (utxx::unlikely(qsym.m_omcN == -1))
      throw invalid_argument
            (string("StratEnv::NewOrderImpl: ") + where + ": No OEs(?)");

    // Otherwise, "CheckQSymAOS" ensures that omcN is valid:
    ConnDescr& conn = (*m_OEs)[qsym.m_omcN];

    // ClientRef of this OE:
    int ref = qsym.m_omcN + m_MDs.size();

    // Allocate a new AOS (including the initial Req) on the segment belonging
    // to the Connector.
    // NB: The AOS Ctor performs checking of all params:
    //
    AOSReq12*     req;
    OrderID       rootID;
    std::tie(req, rootID) = AOS::Make
    (
      conn.m_aos_storage,
      conn.m_req_storage,
      AOSReq12::ReqKindT::New,
      acc_crypt,
      m_strat_cid,
      m_strat_sid,
      ref,
      qsym.m_symKey,
      ord_type,
      side,
      price,
      qty,
      show_qty,
      min_qty,
      time_in_force,
      expire_date,
      this_side_only,
      ts,
      false,        // Not manual
      user_data
    );

    bool   ok = SendReqPtr(req, qsym.m_omcN, &qsym, where);
    return ok ? req : nullptr;
  }

  //=========================================================================//
  // "ModifyOrderImpl":                                                      //
  //=========================================================================//
  bool StratEnv::ModifyOrderImpl
  (
    AOS*                    aos,
    QSym const*             qsym,       // May be NULL if only 1 OE
    double                  new_price,  // NaN if not to change
    long                    new_qty,    // = 0 if not to change
    AOS::UserData const*    user_data,
    long                    new_show_qty,
    long                    new_min_qty,
    bool                    is_manual,
    char const*             where
  )
  {
    // Check the Args:
    assert(aos != NULL);

    // If the order is marked as Inactive or already being cancelled, log an
    // error and return "false":
    if (utxx::unlikely(aos->Inactive() || aos->CxlPending()))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s: RootID=%012lX: Order is Already Inactive or "
                  "Being Cancelled", where, aos->RootID());
      return false;
    }

    int omc = -1;

    if (utxx::likely(qsym != NULL))
    {
      if (!CheckQSymAOS(*qsym, aos, where))
        return false;
      omc = qsym->m_omcN;
    }
    else
    if (utxx::unlikely(utxx::unlikely(m_OEs->size() != 1)))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s: RootID=%012lX: Cannot get the OE# -- must be "
                  "specified explicitly via QSym", where, aos->RootID());
      return false;
    }
    else
      omc = 0;

    // Find the required OE:
    if (utxx::unlikely(omc == -1))
      throw invalid_argument
            (string("StratEnv::NewOrderImpl: ") + where + ": No OEs(?)");

    // Otherwise, "CheckQSymAOS" ensures that omcN is valid:
    ConnDescr& conn = (*m_OEs)[omc];

    // Verify that the AOS is valid: the order is still active, and of correct
    // type (NB: currently, Mkt Orders cannot be cancelled,  and any non-Limit
    // orders cannot be modified):
    bool is_mod = !Finite(new_price) && (new_qty == 0);

    if (utxx::unlikely
         (aos->m_orderType == OrderTypeT::MassCancel) ||
         (aos->m_orderType == OrderTypeT::Market)     ||
         (is_mod && aos->m_orderType != OrderTypeT::Limit)
       )
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s: RootID=%012lX: Order is not suitable for "
                  "Cancel/Modify\n", where, aos->RootID());
      return false;
    }

    // Update UserData if provided:
    if (user_data != NULL)
      aos->m_userData = *user_data;

    // Create and insert a Modification Request:
    AOSReq12* req;
    std::tie (req, std::ignore) = AOS::Make
    (
      conn.m_req_storage,
      aos,
      AOSReq12::ReqKindT::Replace, // "Cancel" is "Modify" with all Qtys=0
      new_price,                   // Price     (NaN for Cancel or if no change)
      new_qty,                     // Qty       (0 for Cancel)
      new_show_qty,                // ShowQty   (0 for Cancel)
      new_min_qty,                 // MinQty    (0 for Cancel)
      is_manual,
      utxx::now_utc()              // TimeStamp
    );
    assert(req != NULL);

    // If the order is being cancelled, mark it as such:
    if (!is_mod)
      aos->Status(AOS::StatusT::CxlPending);

    // Send the Request up to the Connector:
    return SendReqPtr(req, (qsym != NULL) ? qsym->m_omcN : 0, qsym, where);
  }

  //=========================================================================//
  // "NewOrder":                                                             //
  //=========================================================================//
  AOSReq12* StratEnv::NewOrder
  (
    QSym const&             qsym,
    OrderTypeT              ord_type,
    SideT                   side,
    double                  price,     // 0 for MktOrder, > 0 for LimitOrder
    long                    qty,
    Events::AccCrypt        acc_crypt,
    AOS::UserData const*    user_data,
    FIX::TimeInForceT       time_in_force,
    utxx::time_val          expire_date,
    bool                    peg_to_this_side,
    long                    show_qty,
    long                    min_qty
  )
  {
    // The args must NOT imply "MassCancel":
    if (utxx::unlikely
       (qty <= 0 || (ord_type == OrderTypeT::Pegged && !Finite(price))))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("Events::OrderID NewOrder: Invalid Qty=%ld and/or Px=%f, "
                  "IsPegged=%d\n", qty, price, ord_type == OrderTypeT::Pegged);
      return NULL;
    }

    // NB: The default "show_qty" is LONG_MAX but this is only meant to indi-
    // cate the "qty":
    if (utxx::likely(show_qty == LONG_MAX))
      show_qty = qty;

    // NB: "min_qty" does not require adjustment

    return
      // NB: DoSubmit = true. Params are eventually passed to "MkAOS"  and then
      // to the AOS Ctor; the latter performs all params verifications -- so do
      // NOT do it here:
      //
      NewOrderImpl
      (
        qsym,             ord_type, side, price,         qty,
        acc_crypt,        user_data,      time_in_force, expire_date,
        peg_to_this_side, show_qty,       min_qty,
        utxx::now_utc(),  "NewOrder"
      );
  }

  //=========================================================================//
  // "ModifyOrder":                                                          //
  //=========================================================================//
  bool StratEnv::ModifyOrder
  (
    AOS*                    aos,
    QSym const*             qsym,
    double                  new_price,
    long                    new_qty,
    AOS::UserData const*    user_data,
    long                    new_show_qty,
    long                    new_min_qty,
    bool                    is_manual
  )
  {
    // NB: "qty" must either be < 0, ie unchanged, or be positive; new_qty==0
    // is only allowed for a Cancel:
    if (utxx::unlikely(new_qty == 0  ||
                      (!Finite(new_price) && new_qty < 0 && new_show_qty < 0 &&
                       new_min_qty < 0)))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::ModifyOrder: Invalid args: NewQty=%ld, NewPx=%f, "
                  "NewShowQty=%ld, NewMinQty=%ld\n", new_qty, new_price,
                  new_show_qty, new_min_qty);
      return false;
    }

    // Adjust "new_show_qty":
    // The default for it is LONG_MAX which means using "new_qty"; if the lat-
    // ter is (-1) (unchanged), then "new_show_qty" will be (-1) as well,  ie
    // unchanged from its prev value -- but XXX in this case, it is NOT guaran-
    // teed that that actual(new_qty)==actual(new_show_qty) -- to avoid this,
    // the caller must specify the qtys explicitly!
    //
    if (new_show_qty == LONG_MAX)
      new_show_qty = new_qty;

    // NB: "new_min_qty" does not require adjustment...

    return ModifyOrderImpl
           (aos, qsym,   new_price, new_qty, user_data, new_show_qty,
            new_min_qty, is_manual, "ModifyOrder");
  }

  //=========================================================================//
  // "CancelOrder":                                                          //
  //=========================================================================//
  // This is just a partial case of "Modify": Px is NaN and all Qtys are 0:
  //
  bool StratEnv::CancelOrder
  (
    AOS*            aos,
    QSym const*     qsym,
    bool            is_manual
  )
  {
    // Invoke the common implementation:
    return
      ModifyOrderImpl(aos, qsym, NaN<double>, 0, NULL, 0, 0,
                      is_manual, "CancelOrder");
  }

  //=========================================================================//
  // "CancelAllOrders":                                                      //
  //=========================================================================//
  // (For a given "QSym"). This is actually a special case of "NewOrder". Re-
  // turns a ptr to the AOS created for the MassCancelRequest:
  //
  AOSReq12* StratEnv::CancelAllOrders
  (
    QSym const&             qsym,
    Events::AccCrypt        acc_crypt,
    bool                    this_side_only,
    SideT                   side
  )
  {
    return NewOrderImpl
    (
      qsym,
      OrderTypeT::MassCancel,
      side,
      NaN<double>,          // Px does not matter here
      0,                    // Qty is 0
      acc_crypt,
      nullptr,              // No UserData
      FIX::TimeInForceT::Day,
      utxx::time_val(),     // No explicit expiration
      this_side_only,       // Cancel one side (as of "is_buy") or both sides?
      0,                    // ShowQty is 0
      0,                    // MinQty  is 0
      utxx::now_utc(),
      "CancelAllOrders"
    );
  }

  //=========================================================================//
  // "CountActiveOrders":                                                    //
  //=========================================================================//
  // (1) For a particulat "QSym":
  //
  int StratEnv::CountActiveOrders(QSym const& qsym) const
  {
    AOSMapQPV::const_iterator cit = m_aoss->find(qsym);
    return
      (cit == m_aoss->cend())
      ? 0
      : int(cit->second.size());
  }

  // (2) Total -- for all "QSym"s:
  //
  int StratEnv::CountActiveOrders() const
  {
    int res = 0;
    for (AOSMapQPV::const_iterator cit = m_aoss->cbegin();
         cit != m_aoss->cend();  ++cit)
      res += int(cit->second.size());
    return res;
  }

  //=========================================================================//
  // "StreamingQuote":                                                       //
  //=========================================================================//
  // XXX: This has not been tested yet:
  //
  /*
  bool StratEnv::StreamingQuote
  (
    QSym const&           qsym,
    Events::AccCrypt      acc_crypt,
    AOS**                 bid_aos,    // *bid_aos == NULL if new quote
    double                bid_px,
    long                  bid_qty,
    AOS::UserData const*  bid_user_data,
    AOS**                 ask_aos,    // *ask_aos == NULL if new quote
    double                ask_px,
    long                  ask_qty,
    AOS::UserData const*  ask_user_data
  )
  {
    //-----------------------------------------------------------------------//
    // Verify the Args:                                                      //
    //-----------------------------------------------------------------------//
    assert(bid_aos != NULL && ask_aos != NULL);

    // NB: The following is OK even if the AOS ptrs are NULL:
    //
    if (!CheckQSymAOS(qsym, *bid_aos, "StreamingQuote(Bid)" ||
        !CheckQSymAOS(qsym, *ask_aos, "StreamingQuote(Ask)"))
      return false;
    auto ts = utxx::now_utc();

    // Find the required OE:
    if (utxx::unlikely(qsym.m_omcN == -1))
      throw invalid_argument("StratEnv::StreamingQuote: No OEs(?)");

    // Otherwise, "CheckQSymAOS" ensures that omcN is valid:
    ConnDescr& conn = (*m_OEs)[qsym.m_omcN];

    // ClientRef of this OE:
    int ref = qsym.m_omcN + m_MDs.size();

    // We require that either both AOSes are provided (active or otherwise), so
    // it's a modification of a previous Quote; or none is provided, then it is
    // a new Quote:
    //-----------------------------------------------------------------------//
    // New Quote:                                                            //
    //-----------------------------------------------------------------------//
    if (*bid_aos == NULL && *ask_aos == NULL)
    {
      // Must create 2 new AOSes and link them (and the corresp Reqs) together.
      // These are supposed to be Limit Orders with a standard TimeInForce. But
      // do NOT submit them yet (DoSubmit=false):
      //
      // The RootIDs are also related (differ just by 1); the Ask Side has Bit0
      // cleared, and the Bid Side has Bit0 set:
      // FIXME: This mechanism may not work as intended -- verify it!
      //
      // Pxs and Qtys must be valid:
      if (utxx::unlikely
         (bid_qty <= 0 || !Finite(bid_px) || ask_qty <= 0 || !Finite(ask_px)))
        throw invalid_argument
              ("StratEnv::StreamingQuote(New): Invalid Px(s) and/or Qty(s)");

      Events::OrderID quoteID   = MkRootID();
      Events::OrderID bidRootID = quoteID   & 1;
      Events::OrderID askRootID = bidRootID - 1;

      // Bid Side:
      *bid_aos = NewOrderImpl
      (
        qsym,  true,   bid_px,  bid_qty,    acc_crypt, bid_user_data,
        FIX::TimeInForceT::Day, utxx::time_val(),      false,   false,
        0,     0,      ts,      "StreamingQuote(Bid)"
      );
      assert(*bid_aos != NULL && (*bid_aos)->m_pending.count() == 1);

      // Get Bid Req (there is only one):
      AOSReq12* bidReq = (*bid_aos)->m_pending.peek();
      assert(bidReq   != NULL && bidReq->m_kind == ReqKindT::New);

      // Ask Side:
      *ask_aos = NewOrderImpl
      (
        qsym,  false,  ask_px,  ask_qty,    acc_crypt, ask_user_data,
        FIX::TimeInForceT::Day, utxx::time_val(),      false,   false,
        0,     0,      ts,      "StreamingQuote(Ask)"
      );
      assert(*ask_aos != NULL && (*ask_aos)->m_pending.count() == 1);

      // Get the Ask Req (there is only one):
      AOSReq12* askReq = (*ask_aos)->m_pending.peek();
      assert(askReq   != NULL && askReq->m_kind == AOS::ReqKindT::New);

      // Now link together the AOSes and Reqs:
      (*bid_aos)->m_linkedAOS = *ask_aos;
      (*ask_aos)->m_linkedAOS = *bid_aos;

      bidReq->m_linkedReq     = askReq;
      askReq->m_linkedReq     = bidReq;

      // Submit only the Ask Side Req -- the Bid Side one will be found by the
      // Connector using the links constructed:
      //
      SendReqPtr(askReq, qsym.m_omcN, &qsym, "StreamingQuote");
    }
    else
    //-----------------------------------------------------------------------//
    // Quote Modification:                                                   //
    //-----------------------------------------------------------------------//
    if (*bid_aos != NULL && *ask_aos != NULL)
    {
      // XXX: One of bid_aos, ask_aos may be Inactive (but then the corresp
      // modification params must be invalid as well), but not both:
      //
      if (utxx::unlikely((*bid_aos)->IsInactive() && (*ask_aos)->IsInactive()))
        throw invalid_argument
          ("StratEnv::StreamingQuote(Modify): Both BidAOS (RootID="   +
           ToHexStr16((*bid_aos)->RootID()) + ") and AskAOS (RootID=" +
           ToHexStr16((*ask_aos)->RootID()) + ") are Inactive");

      // Pxs and Qtys must be either valid or invalid together:
      if (utxx::unlikely
         (( Finite(bid_px) && bid_qty <= 0) ||
          (!Finite(bid_px) && bid_qty >  0) ||
          ( Finite(ask_px) && ask_qty <= 0) ||
          (!Finite(ask_px) && ask_qty >  0)))
        throw invalid_argument
              ("StratEnv::StreamingQuote(Modify): InConsistent Px/Qty values "
               "for Bid and/or Ask");

      // NB: Due to the above, only 1 of the conjuctive conds below is actualy
      // needed, but we still use both for clarity:
      bool modifyBid = Finite(bid_px) && bid_qty > 0;
      bool modifyAsk = Finite(ask_px) && ask_qty > 0;

      // Can the Bid be modified, if requested?
      if (utxx::unlikely((*bid_aos)->IsInactive() && modifyBid))
        throw invalid_argument
          ("StratEnv::StreamingQuote(Modify): Bid side is Inactive, cannot be "
           "modified");

      // Can the Ask be modified, if requested?
      if (utxx::unlikely((*ask_aos)->IsInactive() && modifyAsk))
        throw invalid_argument
          ("StratEnv::StreamingQuote(Modify): Ask side is Inactive, cannot be "
           "modified");

      // So: Modify the Bid and/or Ask Sides if they are Active, and if their
      // modification have been requested:
      //
      AOSReq12* bidReq = NULL;
      AOSReq12* askReq = NULL;

      if (!(*bid_aos)->IsInactive() && modifyBid)
        // XXX: We currently assume that separate ReqIDs are not required for
        // StreamingQuote Reqs (if the Connector thinks otherwise, it can al-
        // ways over-write the ID):
        bidReq =
          (*bid_aos)->AddIntend

      if (!(*ask_aos)->IsInactive() && modifyAsk)
        // Similarly, modify the Ask Side:
        askReq =
          (*ask_aos)->AddIntend
            (AOS::ReqKindT::Modify, ask_px, ask_qty, 0, 0, ts, false);

      // Link the Reqs together (but be careful of NULLs):
      if (bidReq != NULL)
        bidReq->m_linkedReq = askReq;

      if (askReq != NULL)
        askReq->m_linkedReq = bidReq;

      // Submit only 1 Req (Ask Side if both Bid and Ask are non-NULL):
      if (askReq != NULL)
        SendReqPtr(askReq, qsym.m_omcN, &qsym, "StreaminQuote");
      else
      {
        assert(bidReq != NULL);
        SendReqPtr(bidReq, qsym.m_omcN, &qsym, "StreamingQuote");
      }
    }
    //-----------------------------------------------------------------------//
    // Any other cases: invalid:                                             //
    //-----------------------------------------------------------------------//
    else
      throw invalid_argument
            ("StratEnv::StreamingQuote: Invalid AOS Ptrs combination");
    return true;
  }
  */

  //=========================================================================//
  // "SendReqPtr":                                                           //
  //=========================================================================//
  // NB: "qsym" may be NULL if unknown (eg when cancelling or amending an order
  // based on RootID alone):
  //
  bool StratEnv::SendReqPtr
  (
    AOSReq12*   appl,
    int         omcN,
    QSym const* qsym,   // May be NULL
    char const* where
  )
  {
    //-----------------------------------------------------------------------//
    // Verify the Args:                                                      //
    //-----------------------------------------------------------------------//
    assert(appl != NULL && appl->m_aos != NULL);

    if (utxx::unlikely(omcN < 0 || omcN >= int(m_OEs->size())))
    {
      if (m_debugLevel >= 1)
        LOG_ERROR("StratEnv::%s Invalid OE#=%d\n", where, omcN);
      return false;
    }
    assert(qsym == NULL || qsym->m_omcN == omcN);

    if (qsym != NULL && !CheckQSymAOS(*qsym, appl->m_aos, "SendReqPtr"))
      return false;

    ConnDescr const& omc = (*m_OEs)[omcN];

    if (!m_emulated)
    {
      //---------------------------------------------------------------------//
      // Non-Emulated Mode:                                                  //
      //---------------------------------------------------------------------//
      // Make an "OrdReq", installing the TimeStamp in the Req:
      XConnector_OrderMgmt::OrdReq req;
      req.m_isAdmin   = false;
      req.m_U.m_appl  = appl;
      appl->m_ts_qued = utxx::now_utc();

      // Get the "ReqQ" of that OE, must be non-NULL:
      // XXX: If this is an MS "StratEnv" instance, then "ReqQ" may not exist
      // yet -- this is to prevent rance conditions on MS/Connectors start-up,
      // so would have to open it on-the-fly:
      //
      fixed_message_queue* reqQ  = omc.m_reqQ.get();
      assert(reqQ != NULL);

      // Submit the "OrdReq". Any exceptions are propagated to the Caller (the
      // Startegy); priorities are currently not used (0):
      //
      if (utxx::unlikely(!reqQ->try_send(&req, sizeof(req), 0)))
      {
        // Queue full?
        if (m_debugLevel >= 1)
          LOG_ERROR("StratEnv::%s: OE ReqQ full: OE#=%d\n", where, omcN);
        return false;
      }

      //---------------------------------------------------------------------//
      // If this OE has provided an EventFD, notify it:                      //
      //---------------------------------------------------------------------//
      if (omc.m_eventFD != -1)
      {
        uint64_t n = 1; // XXX: Don't use 0 here!!!
        while (true)
          if (utxx::unlikely(write(omc.m_eventFD, &n, sizeof(n)) < 0))
            if (utxx::unlikely(errno == EAGAIN || errno == EINTR))
              continue;
            else
            {
              if (m_debugLevel >= 1)
                LOG_ERROR("StratEnv::%s: Cannot signal the EventFD: %s\n",
                          where, strerror(errno));
              return false;
            }
          else
            break;
      }
      else
      if (m_debugLevel >= 1)
        LOG_WARNING("StratEnv::SendReqPtr: EventFD not in use!\n");
    }
#   ifdef WITH_CONNEMU
    else
    {
      //---------------------------------------------------------------------//
      // Emulated Mode:                                                      //
      //---------------------------------------------------------------------//
      assert(m_connEmu.get() != NULL);
      m_connEmu->SendRequest(appl);
    }
#   endif
    // All done:
    return true;
  }

  //=========================================================================//
  // "SendHeartBeat":                                                        //
  //=========================================================================//
  // FIXME: To be replaced by "Echo":
  /*
  void StratEnv::SendHeartBeat(int omcN) const
  {
    if (utxx::unlikely(omcN == -1 || m_emulated))
      // No OE, or not supported in the Emulated Mode -- silently return:
      return;

    // Otherwise: Generic Case:
    if (utxx::unlikely(omcN < 0 || omcN >= int(m_OEs->size())))
      throw invalid_argument
            ("StratEnv::SendHeartBeat: Invalid OE#: "+ to_string(omcN));

    ConnDescr const& omc = (*m_OEs)[omcN];

    // Make an "OrdReq":
    XConnector_OrderMgmt::OrdReq req;
    req.m_isAdmin      = true;
    req.m_U.m_admin    = { FIX::MsgT::HeartBeat, 0, 0 };

    // Get the ReqQ of that OE:
    fixed_message_queue* reqQ = omc.m_reqQ.get();
    assert(reqQ != NULL);

    // Submit the "OrdReq". Any exceptions are propagated to the Caller  (the
    // Startegy); priorities are currently not used (0):
    //
    if (utxx::unlikely(!reqQ->try_send(&req, sizeof(req), 0)))
      // Queue full?
      throw runtime_error
            ("StratEnv::SendHeartBeat: OE ReqQ full: OE#="+ to_string(omcN));
  }
  */

  //=========================================================================//
}
