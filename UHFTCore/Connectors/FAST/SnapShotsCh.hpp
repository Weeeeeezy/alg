// vim:ts=2:et
//===========================================================================//
//                   "Connectors/FAST/SnapShotsCh.hpp":                      //
//   Channel for Receiving Orders SnapShots (currently for MICEX and FORTS)  //
//===========================================================================//
// This class provides call-backs to "EPollReactor". It is templated because it
// contains calls to methods of the outer class ("EConnector_FAST"). The actual
// derived class ("EConnector_FAST_Der") is only used as aparsing helper:
//
#pragma once

#include "Basis/Maths.hpp"
#include "Basis/TimeValUtils.hpp"
#include "Protocols/FAST/LowLevel.hpp"
#include "Connectors/EConnector_MktData.hpp"
#include "Connectors/SSM_Channel.hpp"
#include "Connectors/OrderBook.h"
#include <utxx/error.hpp>

namespace MAQUETTE
{
namespace FAST
{
  //=========================================================================//
  // "SnapShotsCh" Class:                                                    //
  //=========================================================================//
  template<typename EConnector_FAST, typename EConnector_FAST_Der>
  class SnapShotsCh:
    public SSM_Channel<SnapShotsCh<EConnector_FAST, EConnector_FAST_Der>>
  {
  private:
    //=======================================================================//
    // Type Renamings and Consts:                                            //
    //=======================================================================//
    using SnapShot                    =
      typename EConnector_FAST::SnapShotOBT;

    using TradingSessionStatus        =
      typename EConnector_FAST::TrSessStatT;

    constexpr static TID SnapShotTID  =
      EConnector_FAST::GetSnapShotOBTID();

    constexpr static TID TradingSessionStatusTID =
      EConnector_FAST::GetTrSessStatTID();

    constexpr static TID HeartBeatTID =
      EConnector_FAST::GetHeartBeatTID ();

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // NB: Unlike "SeqNumBuffer", here it's OK to use 0 (not -1) as the Invalid
    // value for "m_nextSeqNum":
    //
    EConnector_FAST_Der* const     m_mdc;
    mutable SnapShot               m_snapShot;   // FAST msgs parsed into here!
    mutable TradingSessionStatus   m_sessStatus; //
    mutable int                    m_round;
    mutable bool                   m_skipThisRound;
    mutable SeqNum                 m_nextSeqNum;
    mutable bool                   m_done;
    int     const                  m_maxInitRounds;

    // Default Ctor is deleted -- we must always create a Channel in the con-
    // text of an outer Connector:
    SnapShotsCh() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    SnapShotsCh
    (
      SSM_Config  const&   a_config,
      std::string const&   a_iface_ip,
      EConnector_FAST_Der* a_mdc,
      int                  a_max_init_rounds
    )
    : // Base Class Ctor:
      SSM_Channel<SnapShotsCh<EConnector_FAST, EConnector_FAST_Der>>
      (
        a_config,
        a_iface_ip,
        a_mdc->m_name + "-SnapShotsCh-A",
        a_mdc->m_reactor,
        a_mdc->m_logger,
        a_mdc->m_debugLevel
      ),
      // Ptr to the Outer Class object:
      m_mdc          (a_mdc),

      // Buffers for msgs decoding -- initially zeroed-out:
      m_snapShot     (),
      m_sessStatus   (),

      // Initially, we would generically be at Round=0 which corresponds to the
      // partially-complete SnapShots, that's why "m_skip_this_round" is SET:
      m_round        (0),
      m_skipThisRound(true),
      m_nextSeqNum   (0),
      m_done         (false),
      m_maxInitRounds(a_max_init_rounds)
    {
      // Checks:
      assert(m_mdc != nullptr);
      CHECK_ONLY
      (
        if (utxx::unlikely(m_maxInitRounds <= 0))
          throw utxx::badarg_error
                ("SnapShotsCh::Ctor: Need MaxInitRounds >= 1");

        // Also, a SnapShots Channel can only exist within a Primary MDC:
        if (utxx::unlikely(!(m_mdc->IsPrimaryMDC())))
          throw utxx::badarg_error
                ("SnapShotsCh::Ctor: Must be for Primary MDC");
      )
    }

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~SnapShotsCh() noexcept
    {
      memset(&m_snapShot,   '\0', sizeof(m_snapShot)  );
      memset(&m_sessStatus, '\0', sizeof(m_sessStatus));
      m_round         = 0;
      m_skipThisRound = false;
      m_nextSeqNum    = 0;
      m_done          = false;
    }
    //-----------------------------------------------------------------------//
    // NB: "Start", "Stop" are inherited from the base class                 //
    //-----------------------------------------------------------------------//

    //=======================================================================//
    // "RecvHandler":                                                        //
    //=======================================================================//
    // Actual "SnapShot" decoding and processing occurs HERE.
    // XXX: There is no SeqNumBuffer for SnapShots, as they come from a single
    // channel only, and we explicitly verify their SeqNums. FIXME: this some-
    // times causes trouble in unreliable testing envs:
    //
    bool RecvHandler
    (
      int            DEBUG_ONLY(a_fd),
      char const*    a_buff,
      int            a_size,
      utxx::time_val a_ts_recv
    )
    {
      utxx::time_val handlTS = utxx::now_utc();

      //---------------------------------------------------------------------//
      // Special Case: End-of-Chunk Event:                                   //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(a_buff == nullptr || a_size <= 0))
        // In the steady mode, we would invoke "ProcessEndOfDataChunk" on the
        // MDC, but we are in the StaticInitMode -- so no point:
        return true;

      //---------------------------------------------------------------------//
      // Generic Case: First of all, update the Rx Stats on the MDC:         //
      //---------------------------------------------------------------------//
      assert(a_fd  == this->m_fd && a_buff != nullptr && a_size > 0 &&
            !a_ts_recv.empty()   && m_mdc !=  nullptr);
      m_mdc->UpdateRxStats(a_size, a_ts_recv);

      //---------------------------------------------------------------------//
      // Get the Msg Header:                                                 //
      //---------------------------------------------------------------------//
      // Generically, we expect that "tid" is "SnapShotTID"; the latter depends
      // on the Channel template params:
      PMap pmap       = 0;
      TID  tid        = 0;
      char const* end = a_buff + a_size;
      try
      {
        // NB: The initial 4 bytes are the SeqNum prefix in a Little-Endian fmt:
        // TODO: Generalise this!
        SeqNum pktSN    = *(reinterpret_cast<uint32_t const*>(a_buff));
        a_buff          =
          GetMsgHeader(a_buff + 4, end, &pmap, &tid, this->m_shortName.data());

        //-------------------------------------------------------------------//
        // Decode the msg:                                                   //
        //-------------------------------------------------------------------//
        // We use "if" here, rather than "switch", because all cases except the
        // 1st one are extremely unlikely:
        if (utxx::likely(tid == SnapShotTID))
        {
          // Generic case: Decode the SnapShot. XXX: Unfortunately, the Decoder
          // may (or may not) depend on some static params (eg "AssetClass") of
          // the "EConnector_FAST_Der", so we need to delegated "Decode" to the
          // latter:
          a_buff = EConnector_FAST_Der::DecodeOBSnapShot
                   (a_buff, end, pmap,  &m_snapShot);
          return Process(pktSN,  a_ts_recv, handlTS);
        }
        else
        if (tid == TradingSessionStatusTID)
        {
          // "TradingSessionStatus" is processed by "EConnector*" itself -- it
          // typically requires a Connector-wide response:
          //
          a_buff = m_sessStatus.Decode (a_buff, end, pmap);
          return m_mdc->Process(m_sessStatus, a_ts_recv);
        }
        else
        if (tid == HeartBeatTID)
          // Don't even decode it, just skip:
          a_buff = end;
        else
        {
          // UnExpected Msg Type (could be SequenceReset of whatever -- which
          // we don't use here):
          LOG_WARN(2, "SnapShotsCh: UnExpected TID={}: Ignored...", tid)
          a_buff = end;
        }

        // XXX: Current implementation assumes that there is only 1 FAST msg
        // per UDP pkt (otherwise there would be no 4-byte SeqNum prefix) --
        // so we must have arrived at the "end". TODO: Generalise this...
        CHECK_ONLY
        (
          if (utxx::unlikely(a_buff != end))
            LOG_WARN(2,
              "SnapShotsCh: End-ptr wrong by {} bytes", int(a_buff - end))
        )
      }
      catch (EPollReactor::ExitRun const&)
        { throw; }
      catch (std::exception const& exc)
      {
        // Log the exception and continue (so it does not propagate to the Reac-
        // tor level, and gets logged via a Connector-specific logger). In most
        // cases, TID will be known:
        LOG_ERROR(1,
          "SnapShotsCh::RecvHandler: Exception: TID={}: {}", tid, exc.what())
      }
      // Still continue (so not to confuse the Reactor):
      return true;
    }

    //=======================================================================//
    // "Process":                                                            //
    //=======================================================================//
    // Processing a decoded "SnapShot" msg:
    // (*) We need to process a full round of "SnapShot" msgs, without any lost
    //     or unordered ones, because for SnapShots (unlike Order/Trade Incrs),
    //     there is no "SeqNumBuffer" (otherwise, interaction between SnapShots
    //     and OrderIncrs would become extremely complicated);
    // (*) so we go round-by-round until we succeed;
    // (*) if there is an error within a round, the rest of it skipped; clean-
    //     up is performed before starting a new one:
    //
    // Returns a "continue" ("true") or "stop now" ("false") flag:
    //
    bool Process
    (
      SeqNum          a_pkt_sn,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl
    )
    {
      //---------------------------------------------------------------------//
      // Special Cases:                                                      //
      //---------------------------------------------------------------------//
      // If we are completely done, return immediately:
      if (utxx::unlikely(m_done))
        return false;

      // Starting a new round now?
      if (utxx::unlikely(m_snapShot.m_MsgSeqNum == 1))
      {
        ProcessRoundBegin();

        // And check again whether we should stop immediately:
        if (m_done)
          return false;
      }

      // If (the rest of) this round is to be skipped, nothing more to do:
      if (m_skipThisRound)
        return true;

      assert(m_round >= 1); // Because Round#0 is always skipped

      // XXX: The following 2 errors would be fatal. Note that UDP msg loss is
      //      not tolerated here because "SnapShot"s are only used in the init
      //      mode:
      // (*)  Prefix SeqNum   != Decoded SeqNum: invalid FAST feed
      // (*)  Decoded  SeqNum != Next SeqNum   : UDP msg loss
      //
      if (utxx::unlikely
         (a_pkt_sn != m_snapShot.m_MsgSeqNum || a_pkt_sn != m_nextSeqNum))
      {
        LOG_ERROR(1,
          "SnapShotsCh::Process: Round={}: SeqNums Mismatch: Prefix={}, "
          "Decoded={}, Expected={}",
          m_round, a_pkt_sn, m_snapShot.m_MsgSeqNum, m_nextSeqNum)
        this->m_logger->flush();

        m_skipThisRound = true;
        return true;        // Still go for next rounds...
      }

      //---------------------------------------------------------------------//
      // Generic Case: Process the SnapShot msg:                             //
      //---------------------------------------------------------------------//
      // It's safe to increment "m_nextSeqNum" now -- it's NOT used again to
      // the rest  of this invocation:
      ++m_nextSeqNum;

      // The OrderIncrs-compatible SeqNum (NOT the SnapShot SeqNum!) provided
      // by this SnapShot;   further increments will be applied on top of the
      // "generation" identified by this SeqNum:
      //
      SeqNum isn = m_snapShot.m_LastMsgSeqNumProcessed;
      if (utxx::unlikely(isn == 0))
      {
        // Then this SnapShot cannot be applied -- there is no Incrs SeqNum for
        // it, so it would be useless. Most probably, it is empty --  this  may
        // happen for illiquid instruments. Simply leave this Book uninitialised
        // for now -- we may not need it at all:
        //
        LOG_WARN(3,
          "SnapShotsCh::Process: Round={}: Skipped: LastMsgSeqNumProcessed={}",
          m_round, isn)

        // And go on:
        return true;
      }

      // Install this SnapShot into the OrderBook, and return the result flag
      // (OK or otherwise). NB: Use "isn" for SeqNum here, not "a_pkt_sn"!
      // NB: Dynamic InitMode is certainly "true" in this case (as we are re-
      // ceiving a SnapShot in FAST!):
      //
      bool ok = m_mdc->template UpdateOrderBooks
      <
        true,                            // IsSnapShot
        true,                            // IsMultiCast,
        true,                            // WithIncrUpdates
        EConnector_FAST::HasOrdersLog(), // WithOrdersLog
        false,                           // !WithRelaxedOBs
        true,                            // ChangeIsPartFill
        false,                           // !NewEncdAsChange
        false,                           // !ChangeEncdAsNew
        false,                           // !IsFullAmt
        false,                           // !IsSparse
        EConnector_MktData::FindOrderBookBy::SecID,
        EConnector_FAST::FQT,
        typename   EConnector_FAST::FQR,
        typename   EConnector_FAST::OMCT
      >
      (isn, m_snapShot, true, a_ts_recv, a_ts_handl);

      // In case of error, remaining msgs of this Round are to be skipped:
      if (utxx::unlikely(!ok))
      {
        LOG_WARN(2,
          "SnapShotsCh::Process: Round={}: Skipped: Failed to install the "
          "SnapShot into the OrderBook(s)", m_round)
        m_skipThisRound = true;
      }
      // Unless we are exiting by Round# (see above), we must return "true" and
      // continue receiving msgs -- even if there was an error above:
      return true;
    }

    //=======================================================================//
    // "ProcessRoundBegin":                                                  //
    //=======================================================================//
    // Logic to be executed if we encountered beginning of a new round:
    //
    void ProcessRoundBegin()
    {
      // First, increment the Round#; but "m_skipThisRound" at this point still
      // refers to the previous round:
      ++m_round;

      //---------------------------------------------------------------------//
      // All rounds completed?                                               //
      //---------------------------------------------------------------------//
      // Check that all our OrderBooks are indeed initialised. NB: we are NOT
      // done if the prev round was (partly) skipped, because this could mean
      // an error and incomplete initialisation.  Round#0 is always a partial
      // one, Round#1 is the 1st one which may succeed, so we would check that
      // at the beginning of Round#2:
      //
      if (m_round >= 2 && !m_skipThisRound)
      {
        bool done    = true;
        SeqNum minSN = LONG_MAX;  // XXX: Remember, it's Signed!!!

        //-------------------------------------------------------------------//
        // Compute "minSN" for all OrderBooks:                               //
        //-------------------------------------------------------------------//
        for (OrderBook const& ob: *(m_mdc->m_orderBooks))
        {
          // The Book must be Consistent wrt Best{Bid,Ask}Pxs, and properly
          // "touched" by at least 1 SnapShot:
          char const* instrName = ob.GetInstr().m_FullName.data();
          SeqNum            isn = ob.GetLastUpdateSeqNum();
          SeqNum            rpt = ob.GetLastUpdateRptSeq();
          PriceT      bestBidPx = ob.GetBestBidPx();
          PriceT      bestAskPx = ob.GetBestAskPx();

          if (utxx::unlikely(!ob.IsConsistent()))
          {
            LOG_WARN(1,
              "SnapShotsCh: Round={}, Instr={}: Inconsistent OrderBook: "
              "[{} .. {}]{}",
              m_round - 1, instrName, double(bestBidPx), double(bestAskPx),
              (m_round <= m_maxInitRounds)
               ? (": Go for next round (" + std::to_string(m_round) + ")")
               : "")
            this->m_logger->flush();
            done = false;
            break;
          }

          // If OK: SnapShots for this OrderBook have been installed (but not
          // IncrUpdates yet):
          LOG_INFO(1,
            "SnapShotsCh: Round={}, Instr={}: SnapShots successfully "
            "installed: SeqNum={}, RptSeq={}: [{} .. {}]",
            m_round - 1, instrName, isn, rpt, double(bestBidPx),
               double(bestAskPx))

          // While computing "minSN", skip the books which have not been touched
          // at all -- but this may still be perfectly OK, eg the corresp instrs
          // are just not traded for the rest of the day.  So this will not pre-
          // vent us from completing the initialisation, but obviously, such em-
          // pty books cannot be subscribed for!
          //
          if (utxx::likely(isn > 0 && rpt > 0))
            minSN = std::min<SeqNum>(minSN, isn);
        }
        // And of OrderBooks traversing

        LOG_INFO(2,
          "SnapShotsCh::ProcessRoundBegin: Round={}: #OrderBooks={}, MinSN="
          "{}, Done={}", m_round, m_mdc->m_orderBooks->size(), minSN, done)

        //-------------------------------------------------------------------//
        // Yes, all OrderBook SnapShots are in place, and got "minSN":       //
        //-------------------------------------------------------------------//
        if (utxx::likely(done && 0 < minSN && minSN < LONG_MAX))
        {
          // NB: We must get a real "minSN". This is because there is at least
          // 1 OrderBook available, as verified by the "EConnector_FAST" Ctor.
          // TODO: This will no longer be true when non-tradable securities (eg
          // FX Baskets or EQ Indices) become supported:
          //
          assert(!(m_mdc->m_orderBooks->empty()));

          int nBooks = int(m_mdc->m_orderBooks->size());
          LOG_INFO(1,
              "SnapShotsCh::ProcessRoundBegin: Round={}: All SnapShots success"
              "fully installed in {} OrderBook(s); SnapShots Mode completed @ "
              "SeqNum={}", m_round - 1, nBooks, minSN)
          this->m_logger->flush();

          //-----------------------------------------------------------------//
          // Apply all pending IncrUpdates (upto & incl "minSN"):            //
          //-----------------------------------------------------------------//
          // Close the Init Mode of the Order IncrsBuff using this "minSN" (the
          // following call is ignored if, for any reason, it occurs twice). It
          // will also apply all pending IncrUpdates:
          //
          m_mdc->m_obIncrsBuff.CloseInitMode(minSN);

          // After pending IncrUpdates are installed, we can mark all OrderBooks
          // as "Initialised":
          int i = 0;
          for (OrderBook& ob: *(m_mdc->m_orderBooks))
          {
            ob.SetInitialised();
            SecDefD const& instr     = ob.GetInstr();
            char const*    instrName = instr.m_FullName.data();

            // For extra clarity, provide initial Bid and Ask pxs:
            LOG_INFO(1,
              "SnapShotsCh::ProcessRoundBegin: Round={}: Initialised OrderBook "
              "#{}: {}: SeqNum={}, RprSeq={}: [{} .. {}]",
              m_round - 1, i, instrName, ob.GetLastUpdateSeqNum(),
              ob.GetLastUpdateRptSeq(),  double(ob.GetBestBidPx()),
              double(ob.GetBestAskPx()))
            ++i;
          }
          assert(i == nBooks);

          //-----------------------------------------------------------------//
          // Finishing up the Initialisation:                                //
          //-----------------------------------------------------------------//
          // All done, stop the SnapShots channel, it is no longer needed, tho-
          // ugh we may still receive a few msgs if they were already buffered
          // by the kernel(???); in that case, they will be ignored:
          this->Stop();

          // STILL, we may get some initial msgs from the next round before we
          // get unsubscribed; they are to be skipped:
          m_nextSeqNum     = 1;
          m_skipThisRound  = true;
          m_done           = true;  // To guarantee we won't process anything...

          // ONLY NOW we can notify our subscribers that the Connector is fully
          // Active -- provided that the Derived class has finished its own ini-
          // tialisations. OrderBooks are now fully initialised, but "m_mdc" in
          // general may or may not be:
          m_mdc->m_orderBooksInited = true;

          if (m_mdc->IsFullyInited())
          {
            m_mdc->m_isStarting = false;    // Fully-Active by now
            assert(m_mdc->IsActive());      // Same as "IsFullyInited"

            LOG_INFO(1,
              "SnapShotsCh: All SnapShots and Delayed Incrs Done, CONNECTOR "
              "STARTED")
            this->m_logger->flush();

            // ON=true, for all SecIDs, no ExchTS, has ConnectorTS:
            utxx::time_val now = utxx::now_utc();
            m_mdc->ProcessStartStop(true, nullptr, 0, now, now);
          }
          // All Done:
          return;
        }
        // Otherwise, fall through -- will have to go for another round...
      }

      //---------------------------------------------------------------------//
      // If we got here: NOT done yet, must go for another round:            //
      //---------------------------------------------------------------------//
      // Too many rounds? If so, this is currently a fatal error:
      if (utxx::unlikely(m_round > m_maxInitRounds))
      {
        LOG_CRIT(1,
          "SnapShotsCh::ProcessRoundBegin: Round={}, and not done yet: "
          "EXITING...", m_round)
        this->m_logger->flush();

        // Terminate the Connector:
        this->Stop();
      }

      // Otherwise: Really starting a new round. Always reset the following
      // vars at the beginning:
      m_nextSeqNum    = 1;
      m_skipThisRound = false;

      // Do the following clean-up unless it's Round#1 (because in Round#0 we
      // did not update the state at all):
      if (m_round >= 2)
        // Reset all OrderBooks and Orders, even those which have already been
        // inited -- will have to do all of them again:
        m_mdc->ResetOrderBooksAndOrders();
    }

    //=======================================================================//
    // "ErrHandler" (for low-level IO errors):                               //
    //=======================================================================//
    void ErrHandler
    (
      int               a_fd,
      int               a_err_code,
      uint32_t          a_events,
      char const*       a_msg
    )
    {
      assert(a_fd == this->m_fd && a_msg != nullptr);

      // Log it:
      LOG_ERROR(1,
        "SnapShotsCh::ErrHandler: IOError: FD={}, ErrNo={}, Events={}: {}: "
        "Will re-start the Connector",
        a_fd, a_err_code, this->m_reactor->EPollEventsToStr(a_events), a_msg)

      this->m_logger->flush();

      // Re-start the entire Connector (but do not terminate the Application).
      // The following operations can presumably be done synchrobously:
      m_mdc->Stop ();
      m_mdc->Start();
    }
  };
} // End namespace FAST
} // End namespace MAQUETTE
