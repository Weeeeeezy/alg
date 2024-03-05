// vim:ts=2:et:sw=2
//===========================================================================//
//                           "StratEnv_Events.cpp":                          //
//                Receiving and Processing of Incoming Events                //
//===========================================================================//
#include "StrategyAdaptor/StratEnv.h"
#include "StrategyAdaptor/SecDefsShM.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <Infrastructure/Logger.h>
#include <stdexcept>
#include <cstdlib>

namespace MAQUETTE
{
  using namespace std;
  using namespace Arbalete;

  //=========================================================================//
  // Receiving and Processing of Incoming Events:                            //
  //=========================================================================//
  //=========================================================================//
  // "GetQSymByMD":                                                          //
  //=========================================================================//
  // The pre-condition is that "mdcN" must be real (not -1). FIXME: this will
  // produce unreliable results if same symbol is received from several MDs.
  // TODO: normalisation of symbols:
  //
  StratEnv::QSym const& StratEnv::GetQSymByMD
  (
    Events::EventT        event_type,
    Events::SymKey const& sym_key,
    int                   mdcN
  )
  const
  {
    if (utxx::unlikely(*(sym_key.data()) == '\0'))
      throw utxx::badarg_error
            ("StratEnv::GetQSymByMD: EventT=", Events::ToString(event_type),
             ": Empty Symbol");

    if (utxx::unlikely(mdcN < 0 || mdcN >= int(m_MDs.size())))
      throw utxx::badarg_error
            ("StratEnv::GetQSymByMD: EventT=", Events::ToString(event_type),
             ": Invalid MD#="+ to_string(mdcN));

    assert(m_QSyms != NULL);
    for (QSymVec::const_iterator cit = m_QSyms->begin(); cit != m_QSyms->end();
         ++cit)
      if (cit->m_symKey == sym_key && cit->m_mdcN == mdcN)
        // Success:
        return *cit;

    // Throw an exception if the requested "QSym" was not found:
    throw utxx::runtime_error
          ("StratEnv::GetQSymByMD: EventT=", Events::ToString(event_type),
           ", SymKey=", Events::ToString(sym_key), ", MD#=", to_string(mdcN),
           "): NOT FOUND");
  }

  //=========================================================================//
  // "GetQSymByOE":                                                          //
  //=========================================================================//
  // The pre-condition is that "omcN" must be real (not -1):
  //
  StratEnv::QSym const& StratEnv::GetQSymByOE
  (
    Events::EventT        event_type,
    Events::SymKey const& sym_key,
    int                   omcN
  )
  const
  {
    if (utxx::unlikely(*(sym_key.data()) == '\0'))
      throw utxx::badarg_error
            ("StratEnv::GetQSymByOE: EventT=", Events::ToString(event_type),
             ": Empty Symbol");

    if (utxx::unlikely(omcN < 0 || omcN >= int(m_OEs->size())))
      throw utxx::badarg_error
            ("StratEnv::GetQSymByOE: EventT=", Events::ToString(event_type),
             ": Invalid OE#=", to_string(omcN));

    assert(m_QSyms != NULL);
    for (QSymVec::const_iterator cit = m_QSyms->begin(); cit != m_QSyms->end();
         ++cit)
      if (cit->m_symKey == sym_key && cit->m_omcN == omcN)
        return *cit;

    // Throw an exception if the requested "QSym" was not found:
    throw utxx::runtime_error
          ("StratEnv::GetQSymByOE: EventT=", Events::ToString(event_type),
           ", SymKey=", Events::ToString(sym_key), ", OE#=", to_string(omcN),
           "): NOT FOUND");
  }

  //=========================================================================//
  // "GetNextEvent":                                                         //
  //=========================================================================//
  // NB: "deadline" may be empty, it which case this method will wait indefini-
  // tely for the Event to come:
  //
  // TODO: redo deadline back to DateTime
  bool StratEnv::GetNextEvent
  (
    ConnEventMsg*         targ,
    utxx::time_val        deadline
  )
  {
    assert(targ != NULL);
    bool   with_deadline = !deadline.empty();

    //-----------------------------------------------------------------------//
    // First, check the Exit Flag (on an external signal):                   //
    //-----------------------------------------------------------------------//
    if (m_Exit.load(memory_order_relaxed))
    {
      targ->SetTag(Events::EventT::StratExit);

      // NB: Important: Clear the exit flag, so that the Strategy can continue
      // to receive further Events  (otherwise, "StratExit"  will be delivered
      // all the time):
      m_Exit.store(false);
      return true;      // Because we GOT an Event!
    }
    // XXX: The exit conditon may also be signalled directy  by "s_ExitSignal".
    // Normally, the C3Thread converts "s_ExitSignal" into "m_Exit", but that
    // thread itself exits on recevng the first "s_ExitSignal", so it would not
    // be able to process following-o signals (if any); do it here:
    if (s_ExitSignal)
    {
      targ->SetTag(Events::EventT::StratExit);
      s_ExitSignal = false;
      return true;
    }

    //-----------------------------------------------------------------------//
    // Apply Dynamic Updates to Strategy Variables (if any):                 //
    //-----------------------------------------------------------------------//
    if (!m_nextUpdate.empty())
      ApplyUpdates();

    //-----------------------------------------------------------------------//
    // Now Really Get the Next Event:                                        //
    //-----------------------------------------------------------------------//
    bool got = false;
    try
    {
#     ifdef WITH_CONNEMU
      if (m_emulated)
      {
        //-------------------------------------------------------------------//
        // Emulated Mode: From a DB:                                         //
        //-------------------------------------------------------------------//
        assert(m_connEmu != NULL &&  m_eventQ == NULL);
        got = m_connEmu->GetNextEvent(&targ, deadline);
      }
      else
#     endif
      {
        //-------------------------------------------------------------------//
        // Generic Case:  From ShM:                                          //
        //-------------------------------------------------------------------//
        assert(m_eventQ != NULL);
        unsigned long  expSz = sizeof(Events::EventMsg);
        unsigned long  actSz = 0;
        unsigned int   prio  = 0;       // XXX: "prio" is currently ignored

        if (with_deadline)
          // Try to get the next Event, with that "deadline":
          // TODO: avoid to_ptime call!
          got = m_eventQ->timed_receive
                (targ, expSz, actSz, prio, utxx::to_ptime(deadline));
        else
        {
          // Wait for the next Event indefinitely (will surely get it at the
          // end):
          m_eventQ->receive(targ, expSz, actSz, prio);
          got = true;
        }

        // If the msg has been received, check its size:
        if (utxx::unlikely(got && (actSz != expSz)))
        {
          LOG_ERROR("StratEnv::GetNextEvent: Invalid Msg Size: "
                    "Got=%ld, Expected=%ld\n", actSz, expSz);
          got = false;
        }
      }
      //---------------------------------------------------------------------//
      // If got an Event: process it:                                        //
      //---------------------------------------------------------------------//
      // Time of event received:
      utxx::time_val eventTime = utxx::time_val::universal_time();

      // (To update Books etc before passing event to the user). If any except-
      // ions occur here, they are logged, and an error msg is returned:
      //
      if (got)
        ProcessEvent(targ, eventTime);
    }
    //-----------------------------------------------------------------------//
    // Catch any exceptions in receiving or prpcessing of msgs:              //
    //-----------------------------------------------------------------------//
    catch (exception const& exc)
    {
      LOG_ERROR("StratEnv::GetNextEvent: EXCEPTION: %s\n", exc.what());
      got = false;
    }
    return got;
  }

  //=========================================================================//
  // "ProcessEvent":                                                         //
  //=========================================================================//
  // NB: On entering this function, "targ" is only partially-constructed: eg it
  // lacks "m_QSym", which needs to be filled in:
  //
  void StratEnv::ProcessEvent
    (ConnEventMsg* targ, utxx::time_val a_eventTime)
  {
    //-----------------------------------------------------------------------//
    // Verify the "targ":                                                    //
    //-----------------------------------------------------------------------//
    assert(targ != NULL);
    // XXX: The following only declares an alias -- "qsym" MUST NOT be used be-
    // fore it is properly initialised:
    QSym& qsym = targ->m_QSym;

    // Once again, re-set the "clean" flag -- we will almost certainly deal
    // with ShM:
    assert(m_clean != NULL);
    *m_clean = false;

    // "ref" is the Connector# in a through-out enumeration from which this
    // Msg came:
    long ref   = long(targ->GetClientRef());
    long nMDs = long(m_MDs.size());
    long nOEs = long(m_OEs->size());

    if (utxx::unlikely(ref < 0 || ref >= nMDs + nOEs))
      throw runtime_error
        ("StratEnv::ProcessEvent: Invalid Ref=" + to_string(ref));

    //=======================================================================//
    // Then process it by type:                                              //
    //=======================================================================//
    Events::EventT tag = targ->GetTag();
    switch (tag)
    {
      //=====================================================================//
      case Events::EventT::TradeBookUpdate:
      //=====================================================================//
      {
        TradeEntry const* trade = targ->GetTradePtr();
        assert(trade != NULL);

        // "ref" is the MD#. NB: Possible exception here:
        qsym = GetQSymByMD(tag, trade->m_symbol, int(ref));

        // XXX: The stored ptr is valid for a limited time period only!
        assert(m_tradeBooks  != NULL);
        (*m_tradeBooks)[qsym] = trade;
        break;
      }

      //=====================================================================//
      case Events::EventT::OrderBookUpdate:
      //=====================================================================//
      {
        OrderBookSnapShot const* ob = targ->GetOrderBookPtr();
        assert(ob != NULL);

        // "ref" is the MD#. NB: Possible exception here:
        qsym = GetQSymByMD(tag, ob->m_symbol, int(ref));

        // XXX: The stored ptr is valid for a limited time period only!
        assert(m_orderBooks  != NULL);
        (*m_orderBooks)[qsym] = ob;

        // Update the VM on this "qsym": NB: our convention is: VM is always
        // "pessimistic", ie it is calculated on the basis of potential aggr-
        // essive position liquidation: Long pos -> use Bid px, Short pos ->
        // use Ask px. On the other hand, unlike NLV, we always use BEST Bid
        // and Ask pxs for VM:
        //
        PositionsMap::iterator pit = m_positions->find(qsym);

        if (pit != m_positions->end())
        {
          PositionInfo& pi = pit->second;   // NB: a ref!
          double refPx =
            (pi.m_pos > 0 && ob->m_currBids >= 1)
            ? ob->m_bids[0].m_price
            :
            (pi.m_pos < 0 && ob->m_currAsks >= 1)
            ? ob->m_asks[0].m_price
            :
            (pi.m_pos == 0)
            ? 0.0
            : NaN<double>;    // Will not be able to adjust VM in this case

          if (Finite(refPx))
            // XXX: should we calculate VM including the trans costs, or not?
            // Currently, trans costs are NOT included (neither into "refPx"
            // nor into "avgPosPx"):
            pi.m_VM = double(pi.m_pos) * (refPx - pi.m_avgPosPx0);
        }
        break;
      }

      //=====================================================================//
      case Events::EventT::SecDefUpdate:
      //=====================================================================//
      {
        SecDef const* def = targ->GetSecDefPtr();
        assert(def != NULL);

        // "ref" is the MD#. NB: Possible exception here:
        qsym = GetQSymByMD(tag, def->m_Symbol, int(ref));

        // XXX: Here we currently do not measure the latency -- such events are
        // rare. The stored ptr  may or may not be valid for the whole session:
        assert(m_secDefs  != NULL);
        (*m_secDefs)[qsym] = def;
        break;
      }

      //=====================================================================//
      case Events::EventT::ConnectorStop:
      case Events::EventT::ConnectorStart:
      //=====================================================================//
        // No internal actions -- the Strategy reacts to these events according
        // to its own logic; cannot fill "qsym":
        break;

      //=====================================================================//
      case Events::EventT::RequestError:
      //=====================================================================//
      {
        // NB: The msg came from an OE:
        int                omcN = int(ref - nMDs);
        AOSReqErr   const* re   = targ->GetReqErrPtr();  assert(re  != NULL);
        AOSReq12    const* req  = re->Req();             assert(req != NULL);
        AOS         const* aos  = req->GetAOS();         assert(aos != NULL);

        // Log the error msg:
        SLOG(ERROR) << "StratEnv::ProcessEvent: RequestError: OE#=" << omcN
                    << ", Symbol=" << aos->m_symbol.data()
                    << ", ReqID="  << req->ID()
                    << ", RootID=" << aos->RootID()
                    << ": "        << re->Message() << std::endl;
        break;
      }

      //=====================================================================//
      case Events::EventT::OrderFilled:
      case Events::EventT::OrderPartFilled:
      case Events::EventT::MiscStatusChange:

      case Events::EventT::OrderCancelled:
      case Events::EventT::MassCancelReport:
      //=====================================================================//
      {
        //-------------------------------------------------------------------//
        // All of them are to do with AOS, etc. The msg came from an OE:    //
        //-------------------------------------------------------------------//
        // The returned pointer may be NULL
        AOSReqFill const* rf  = targ->GetReqFillPtr();

        // NB: "rf" is a partial case of "req":
        AOSReq12   const* req = targ->GetReqPtr();

        if (req == nullptr)
            req = rf->Req();

        // Get the AOS -- either from "req", or (in case of OrderCancelled or
        // MassCancelReport), directly from the msg:
        AOS*   aos  = (req != nullptr) ? req->m_aos : (targ->GetAOSPtr());

        // After that, "aos" must NOT be NULL; if it is, it is a serious error
        // condition:
        if (utxx::unlikely(aos == nullptr))
        {
          LOG_ERROR("StratEnv::ProcessEvent: AOS==NULL: Tag=%s, ReqID=%ld\n",
                    Events::ToString(tag), (req != NULL) ? req->ID() : 0);
          // XXX: Cannot do anything more in this case:
          break;
        }

        // Get the QSym (NB: Possible exception here):
        int    omcN = int(ref - nMDs);
        qsym = GetQSymByOE(tag, aos->m_symbol, omcN);

        // Connector Names. If qsym.m_{mdc,omc}N is (-1), the corresp Connector
        // Name is empty:
        string const  empty;
        string const& mdcName =
          (qsym.m_mdcN != -1) ? (m_MDs[qsym.m_mdcN].m_connName) : empty;
        string const& omcName =
          (qsym.m_omcN != -1) ? ((*m_OEs)[qsym.m_omcN].m_connName) : empty;

        // XXX: Searching for a better place, refresh the Throttler on the Pos-
        // ition Info for this QSym:
        PositionsMap::iterator pit = m_positions->find(qsym);
        PositionInfo*          pi  =
          (pit != m_positions->end())
          ? &(pit->second)
          : nullptr;

        if (pi != nullptr)
          pi->m_posRateThrottler.refresh(a_eventTime);

        // Check the hash values:
        if (utxx::unlikely(aos->CliCID() != m_strat_cid ||
                           aos->CliSID() != m_strat_sid ))
          throw utxx::runtime_error
                ("StratEnv::ProcessEvent: AOS{cid=", aos->CliCID(),
                 ", sid=", aos->CliSID(), "}, StratEnv{cid=", m_strat_cid,
                 ", sid=", m_strat_sid,   '}');

        //-------------------------------------------------------------------//
        // Update Positions and VM:                                          //
        //-------------------------------------------------------------------//
        if (tag == Events::EventT::OrderFilled ||
            tag == Events::EventT::OrderPartFilled)
          OnTrade(rf, qsym, pi, mdcName, omcName, a_eventTime);

        /*
        //-------------------------------------------------------------------//
        // Deal with "MassCancelReport" first:                               //
        //-------------------------------------------------------------------//
        if (tag == Events::EventT::MassCancelReport)
          break;

        //-------------------------------------------------------------------//
        // If the Order is No Longer Active:                                 //
        //-------------------------------------------------------------------//
        if (tag == Events::EventT::OrderFilled ||
            tag == Events::EventT::OrderCancelled)
        {
          char const* reason = aos->IsFilled() ? "FILLED" : "CANCELLED";
          UnMapAOS(aos, qsym, reason);
        }
        */
        break;
      }
      //=====================================================================//
      // No more EventTypes to consider:                                     //
      //=====================================================================//
      default: ;
    }
  }

  //=========================================================================//
  // "OnTrade":                                                              //
  //=========================================================================//
  void StratEnv::OnTrade
  (
    AOSReqFill const*     rf,
    QSym   const&         qsym,
    PositionInfo*         pi,          // MUTABLE!
    string const&         mdc_name,
    string const&         omc_name,
    utxx::time_val        a_eventTime
  )
  {
    //-----------------------------------------------------------------------//
    // Get the AOS and trade details:                                        //
    //-----------------------------------------------------------------------//
    assert(rf  != NULL);
    auto* aos = rf->Req()->GetAOS();
    assert(aos != NULL);

    // Get the QSymInfo and Ccy for this QSym:
    QSymInfoMap::const_iterator qit = m_QSymInfoMap.find(qsym);
    if (utxx::unlikely(qit == m_QSymInfoMap.end()))
      throw runtime_error
            ("StratEnv::ProcessEvent: QSym not found in QSymInfoMap: "+
             qsym.ToString());

    QSymInfo const& qInfo = qit->second;

    // Price and Qty of the LAST Fill:
    // XXX: AOS may have been updated asynchronously (on the Connector side)
    // since the EventMsg was sent to the Strategy, and this will make the
    // "LastQty" invalid. So (FIXME) currently use the CumQty, but this may
    // reduce the accuracy of VM calculations:
    //
    assert(m_positions != NULL);
    double px  = rf->Price();
    long   qty = rf->Qty();

    //-----------------------------------------------------------------------//
    // If first pos update for this Symbol: Create a new "PositionInfo":     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(pi == NULL))
    {
      // Because this is the 1st fill for this Symbol,  "cumQty" should be the
      // same as "lastQty" (if they are not, produce a warning and use the FOR-
      // MER -- see "px" and "qty" above):
      //
      if (utxx::unlikely
         (aos->m_cumQty != rf->Qty()  || aos->m_avgPx != rf->Price()))
        LOG_WARNING("StratEnv::ProcessEvent: 1st fill for %s but CumQty=%ld, "
                    "LastQty=%ld, AvgPx=%.6f, LastPx=%.5f\n",
                    qsym.ToString().c_str(), aos->CumQty(), rf->Qty(),
                    aos->AvgPrice(),         rf->Price());
      PositionInfo newPI
        (qsym, aos->m_accCrypt, 0, 0.0, 0.0, 0.0, 0.0,
         m_pos_rate_thr_win);

      // Save it in the Map:
      pair<PositionsMap::iterator, bool> res =
        m_positions->insert(make_pair(qsym, newPI));
      assert(res.second);
      pi = &(res.first->second);
    }
    assert(pi != NULL);

    //-----------------------------------------------------------------------//
    // Transaction Costs (aka Fees / Commissions):                           //
    //-----------------------------------------------------------------------//
    // XXX: We currently override the commission data provided  by the FIX
    // protocol (if 0) using the settings from the corresp "QSymInfo":
    //
    double exchComm   = aos->m_exchFee;
    double brokerComm = aos->m_brokFee;

    if (exchComm == 0.0 && brokerComm == 0.0)
    {
      exchComm   = qInfo.m_ExchCommRate   * double(qty);
      brokerComm = qInfo.m_BrokerCommRate * double(qty);

      if (qInfo.m_CommRateType == CommRateT::PerValue)
      {
        exchComm   *= px;
        brokerComm *= px;
      }
    }
    double transCost = exchComm + brokerComm;

    //-----------------------------------------------------------------------//
    // Update the "PositionInfo" (Realized PnL, AvgPosPxs, VM and Cash):     //
    //-----------------------------------------------------------------------//
    // Check AccCrypt consistency:
    if (utxx::unlikely(pi->m_accCrypt == 0))
      pi->m_accCrypt = aos->m_accCrypt;
    else
    if (utxx::unlikely(pi->m_accCrypt != aos->m_accCrypt))
      throw runtime_error
            ("StratEnv::OnTrade: PositionInfo.AccCrypt="   +
             ToHexStr16(pi->m_accCrypt) + ", AOS.AccCrypt=" +
             ToHexStr16(aos->m_accCrypt));

    //-----------------------------------------------------------------------//
    // Position Change:                                                      //
    //-----------------------------------------------------------------------//
    // NB: "basePos" is the "old" pos which is used in re-calculation of Avg-
    // PosPx. It there was 0 crossing, it will be reset to 0 (see below):
    //
    long oldPos   = pi->m_pos;
    long deltaPos = aos->IsBuy() ? qty : (- qty);
    long newPos   = oldPos + deltaPos;
    long basePos  = oldPos;
    pi->m_pos     = newPos;

    //-----------------------------------------------------------------------//
    // Effects of moving "pos" towards 0 and/or away from 0:                 //
    //-----------------------------------------------------------------------//
    // "redQty" is part of "qty" which contributed to position reduction towards
    // 0. Ie we consider 2 cases:
    // (1) "newPos" is of the same sign  (or became  0) as "oldPos";
    // (2) "newPos" has changed sign (passed through 0) compared to "oldPos".
    // NB: simple condition |newPos| < |oldPos| can NOT be  used  for detecting
    // pos reductions, because a position can cross 0.
    //
    // Similarly, "awayQty" is part of "qty" which resulted in position moving
    // away from 0. There are 3 possible cases:
    // (1) qty == redQty :          pos just  moved towards 0;
    // (2) qty == awayQty:          pos just  moved towards 0;
    // (3) qty == redQty + awayQty: pos first moved towards 0, crossed it, and
    //                              then moved away from 0; note that it CANNOT
    //                              happen in the opposite order.
    // IMPORTANT: When "pos" shrinks towards 0, RealizedPnL is updated, BUT the
    // "AvgPosPx" of the remaining pos (if non-0) remains the same.  Thus, once
    // we got a position on one side,  all further updates  of "AvgPosPx"  will
    // occur due to trades on THAT side only;  trades on the opposite side will
    // affect PnL but not "AvgPosPx", and that will continue  until pos returns
    // to 0:
    long redQty =
      (oldPos > 0  &&  newPos < oldPos)
      ? (oldPos -  max<long>(0, newPos)) // NB: "max" to account for 0-crossing
      :
      (oldPos < 0  &&  newPos > oldPos)
      ? (min<long>(newPos, 0) - oldPos)  // NB: "min" to account for 0-crossing
      : 0;
    assert(0 <= redQty  && redQty  <= qty && redQty  <= abs(oldPos));

    long awayQty = qty - redQty;
    assert(0 <= awayQty && awayQty <= qty && awayQty <= abs(newPos));

    //-----------------------------------------------------------------------//
    // Moving towards 0:                                                     //
    //-----------------------------------------------------------------------//
    if (redQty > 0)
    {
      // Update RealizedPnL but NOT AvgPosPx:
      assert(oldPos != 0);

      // The change in pos made by "redQty":
      long deltaPosRed = aos->IsBuy() ? redQty : (- redQty);

      // It is valid to say that the "deltaPosRed" now dissipated, was previo-
      // usly acquired at "m_avgPosPx0" price  (not including trans costs,  as
      // the latter are incorporated into RealizedPnL anyway); so the Realized
      // PnL impact will be:
      pi->m_realizedPnL += double(deltaPosRed) * (pi->m_avgPosPx0 - px);

      // XXX: If "newPos" arrives at 0 or crosses 0, AvgPosPxs  are to be reset
      // (because they are discontinuous at that point), and so reset the VM as
      // well:
      if (redQty == abs(oldPos))
      {
        pi->m_avgPosPx0 = 0.0;  // Without trans costs
        pi->m_avgPosPx1 = 0.0;  // With    trans costs
        pi->m_VM        = 0.0;
        basePos         = 0;    // If we then re-calculate AvgPosPx
      }
    }

    //-----------------------------------------------------------------------//
    // Moving away from 0:                                                   //
    //-----------------------------------------------------------------------//
    if (awayQty > 0)
    {
      // Update AvgPosPx. Obviously, in this case "newPos" cannot be 0:
      assert(newPos != 0);

      // Without trans costs:
      // NB: "basePos" is used instead of "oldPos", since the former takes pos-
      // sible 0-crossing into account;  "basePos" must be 0, or of the same
      // sign as "newPos":
      // XXX: Do not use integral multiplication below, an overflow can occur:
      //
      assert(double(basePos) * double(newPos) >= 0          &&
            (( aos->IsBuy() && newPos == basePos + awayQty) ||
             (!aos->IsBuy() && newPos == basePos - awayQty)));

      // Thus:
      double deltaCost0 = double(aos->IsBuy() ? awayQty : (- awayQty)) * px;
      pi->m_avgPosPx0   =
        (double(basePos) * pi->m_avgPosPx0 + deltaCost0) / double(newPos);

      // Now the effect of "transCost": NB: it is divided by "newPos",  not by
      // "qty" -- so can get disproportionalely large if large qtys are traded
      // but the resulting pos is small:
      pi->m_avgPosPx1   =  pi->m_avgPosPx0 +   transCost / double(newPos);
    }

    //-----------------------------------------------------------------------//
    // Now update the VM ("refPx" depends on the sign of "newPos"):          //
    //-----------------------------------------------------------------------//
    // Get the most recent Mkt Data (XXX: there is no guarantee  that they were
    // effective at the time when the trade has occurred -- they may correspond
    // to an earlier as well as a later time instant, but that's OK for our pur-
    // poses):
    if (newPos != 0)
    {
      OrderBookSnapShot const& ob = GetOrderBook(qsym);
      double bidPx =
        (ob.m_currBids >= 1) ? ob.m_bids[0].m_price : NaN<double>;
      double askPx =
        (ob.m_currAsks >= 1) ? ob.m_asks[0].m_price : NaN<double>;
      double refPx =
        (newPos > 0) ? bidPx : askPx;

      if (Finite(refPx))
        pi->m_VM = double(newPos) * (refPx - pi->m_avgPosPx0);
      else
        // XXX: If no mkt data for VM update is available, it's safer to
        // reset VM to 0
        pi->m_VM = 0.0;
    }
    else
      // Otherwise, newPos==0, and AvgPosPxs remain 0 (have been reset
      // above):
      assert(pi->m_avgPosPx0 == 0.0 && pi->m_avgPosPx1 == 0.0 &&
             pi->m_VM        == 0.0);

    //-----------------------------------------------------------------------//
    // XXX: Check for fast position movements, either short or long:         //
    //-----------------------------------------------------------------------//
    if (pi->m_pos < oldPos && pi->m_pos <= 0)
    {
      // Compute the magnitude of the movement: only the negative positions
      // count (ie, if oldPos > 0 but newPos < 0, the positive part of old-
      // Pos is not counted as movement magnitude):
      //
      long   magn = min<long>(oldPos,0) - newPos;
      assert(magn >= 0);
      // NB: Iff we drop from oldPos > 0 to newPos==0, then magn==0, and this
      // is correct since we did not move beyond 0 yet!
      //
      pi->m_posRateThrottler.add(a_eventTime, magn);
    }
    else
    if (oldPos < newPos && newPos >= 0)
    {
      // Similar to the above, but in the positive range:
      long   magn = newPos - max<long>(oldPos, 0);
      assert(magn >= 0);
      // NB: again, iff we grow position from oldPos < 0 to newPos==0, then
      // magn==0, and this is correct since we did not move beyound 0:
      //
      pi->m_posRateThrottler.add(a_eventTime, magn);
    }
    else
      // Otherwise, we do not count it as a movement because we are moving
      // towards 0:
      pi->m_posRateThrottler.refresh(a_eventTime);

    //-----------------------------------------------------------------------//
    // Update "OurTrades" in ShM:                                            //
    //-----------------------------------------------------------------------//
    assert(aos->m_cumQty > 0 && m_ourTrades != NULL);

    OurTradesMap::iterator oit  = m_ourTrades->find(qsym);

    if (oit == m_ourTrades->end())
    {
      // Symbol is not in OurTrades yet? Install it there with an empty
      // vector:
      pair<OurTradesMap::iterator, bool> ins =
        m_ourTrades->insert
        (make_pair(qsym, OurTradesVector(*m_shMAlloc)));
      assert(ins.second);
      oit  = ins.first;
    }

    // NB: commission / fee values are also recorded here:
    OurTrade ot   =
    {
      .m_execID   = rf->ExecID(),
      .m_qsym     = qsym,
      .m_accCrypt = aos->m_accCrypt,
      .m_isBuy    = aos->IsBuy(),
      .m_cumQty   = aos->CumQty(),
      .m_lastQty  = rf->Qty(),
      .m_avgPx    = aos->AvgPrice(),
      .m_lastPx   = rf->Price(),
      .m_exchFee  = exchComm,
      .m_brokFee  = brokerComm,
      .m_ts       = rf->ConnTime(),
      .m_isManual = rf->Req()->Manual()
    };
    oit->second.push_back(ot);
  }

  //=========================================================================//
  // "SetCommRates":                                                         //
  //=========================================================================//
  // (Useful in case if Exchange / Broker commisioins are not directly report-
  // ted by the underlying protocol):
  //
  void StratEnv::SetCommRates
  (
    QSym const& qsym,
    CommRateT   rate_type,
    double      exch_rate,
    double      broker_rate
  )
  {
    // Get the corresp "QSymInfo":
    QSymInfoMap::iterator it = m_QSymInfoMap.find(qsym);
    if (utxx::unlikely(it == m_QSymInfoMap.end()))
      throw invalid_argument
            ("StratEnv::SetCommRates: QSym not found: "+ qsym.ToString());

    QSymInfo& qInfo = it->second;
    qInfo.m_CommRateType   = rate_type;
    qInfo.m_ExchCommRate   = exch_rate;
    qInfo.m_BrokerCommRate = broker_rate;
  }
}
