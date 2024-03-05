// vim:ts=2:et
//===========================================================================//
//                        "Connectors/FAST/IncrsCh.hpp":                     //
//    Channel for Receiving OB and Trade Increments (curr MICEX and FORTS)   //
//===========================================================================//
// This class provides call-backs to "EPollReactor". It is templated because it
// contains calls to methods of the outer class ("EConnector_FAST"). Its actual
// derived class ("EConnecyor_FAST_Der") is only used as a parsing helper:
#pragma once

#include "Protocols/FAST/LowLevel.hpp"
#include "Connectors/SSM_Channel.hpp"
#include "Connectors/SeqNumBuffer.hpp"
#include <utxx/time_val.hpp>
#include <type_traits>

namespace MAQUETTE
{
namespace FAST
{
  //=========================================================================//
  // "IncrsCh" Class:                                                        //
  //=========================================================================//
  template
  <
    typename EConnector_FAST,
    typename EConnector_FAST_Der,
    bool     ForOrderBooks  // "true" for OrderBooks, "false" for Trades
  >
  class IncrsCh:
    public SSM_Channel<IncrsCh
                      <EConnector_FAST, EConnector_FAST_Der, ForOrderBooks>>
  {
  private:
    //=======================================================================//
    // Type Renamings and Consts:                                            //
    //=======================================================================//
    // NB: "IncrementalRefresh", "Processor" and "TID" may refer to OrderBook /
    // FullOrdersLog Increment,  or to a Trades Increment, depending   on the
    // "ForOrderBooks" param:
    //
    using IncrementalRefresh     =
      typename std::conditional_t
      < ForOrderBooks,
        typename EConnector_FAST::IncrRefrOBT,
        typename EConnector_FAST::IncrRefrTrT
      >;

    using TradingSessionStatus   =
      typename EConnector_FAST::TrSessStatT;

    using Processor              =
      typename std::conditional_t
      < ForOrderBooks,
        typename EConnector_FAST::OBsProc,
        typename EConnector_FAST::TradeProc
      >;

    constexpr static TID IncrTID =
      ForOrderBooks
      ? EConnector_FAST::GetIncrRefrOBTID()
      : EConnector_FAST::GetIncrRefrTrTID();

    constexpr static TID TradingSessionStatusTID =
      EConnector_FAST::GetTrSessStatTID();

    constexpr static TID HeartBeatTID =
      EConnector_FAST::GetHeartBeatTID ();

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // NB: "IncrsBuff" uses THIS "IncrsCh" class as an Emplacer, and a special
    //     class from "EConnector_FAST" as a Processor:
    using IncrsBuff = SeqNumBuffer<IncrementalRefresh, IncrsCh, Processor>;

    EConnector_FAST* const       m_mdc;         // NOT owned
    IncrsBuff*       const       m_incrsBuff;   // NOT owned

    // Mutable flds for msg decoding.  They are required to carry the corresp
    // info into the Emplacer function:
    mutable char const*          m_begin;
    mutable char const*          m_expEnd;
    mutable char const*          m_actEnd;
    mutable uint32_t             m_pktSN;       // SeqNum from pkt pfx, 32bit
    mutable PMap                 m_pmap;
    mutable TID                  m_tid;
    mutable TradingSessionStatus m_sessStatus;  // Decoding buffer

    // Default Ctor is deleted -- we must always create a Channel in the con-
    // text of an outer Connector:
    IncrsCh() = delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    IncrsCh
    (
      char               a_ab,   // Feed 'A' or 'B'
      SSM_Config  const& a_config,
      std::string const& a_iface_ip,
      EConnector_FAST*   a_mdc
    )
    : // Base Class Ctor:
      SSM_Channel<IncrsCh<EConnector_FAST, EConnector_FAST_Der, ForOrderBooks>>
      (
        a_config,
        a_iface_ip,

        a_mdc->m_name +
          std::string(ForOrderBooks ? "-OBIncrsCh-" : "-TradesCh-") +
          std::string(1, a_ab),

        a_mdc->m_reactor,
        a_mdc->m_logger,
        a_mdc->m_debugLevel
      ),
      // Ptr to the Outer Class object (NOT owned):
      m_mdc       (a_mdc),

      // Ptr to the "SeqNumBuff" which will be used to synchronise "A" and "B"
      // feeds and manage out-of-order msgs: NOT owned:
      // NB: The NULL ptr arg is for overloading resolution -- there are Incrs-
      // Buffs for both OrderBooks and Trades:
      m_incrsBuff (m_mdc->GetIncrsBuff(static_cast<IncrsBuff*>(nullptr))),

      // Other flds are zeroed-out initially:
      m_begin     (nullptr),
      m_expEnd    (nullptr),
      m_actEnd    (nullptr),
      m_pktSN     (0),
      m_pmap      (0),
      m_tid       (0),
      m_sessStatus()
    {
      assert(m_mdc != nullptr && (a_ab == 'A' || a_ab == 'B'));
    }

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    // Reset all flds to 0:
    ~IncrsCh() noexcept
    {
      m_begin     = nullptr;
      m_expEnd    = nullptr;
      m_actEnd    = nullptr;
      m_pktSN     = 0;
      m_pmap      = 0;
      m_tid       = 0;
      memset(&m_sessStatus, '\0', sizeof(m_sessStatus));
    }
    //-----------------------------------------------------------------------//
    // NB: "Start", "Stop" are inherited from the base class                 //
    //-----------------------------------------------------------------------//

    //=======================================================================//
    // "RecvHandler":                                                        //
    //=======================================================================//
    // Actual "IncrementalRefresh" decoding and processing occurs HERE:
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
      // NB: "WithOrdersLog" is set to "true" -- this is the case for all FAST
      // impls we currently know. Similarly, other flags are set as below:
      //
      if (utxx::unlikely(a_buff == nullptr || a_size <= 0))
      {
        constexpr bool IsMultiCast     = true;
        constexpr bool WithIncrUpdates = true;
        constexpr bool WithOrdersLog   = true;
        constexpr bool WithRelaxedOBs  = false;

        m_mdc->template ProcessEndOfDataChunk
              <IsMultiCast,
               WithIncrUpdates,
               WithOrdersLog,
               WithRelaxedOBs,
               EConnector_FAST::FQT,
               typename EConnector_FAST::FQR
              >();
        return true;
      }

      //---------------------------------------------------------------------//
      // Generic Case: First of all, update the Rx Stats on the MDC:         //
      //---------------------------------------------------------------------//
      assert(a_fd  == this->m_fd && a_buff != nullptr && a_size > 0 &&
            !a_ts_recv.empty()   && m_mdc  != nullptr);
      m_mdc->UpdateRxStats(a_size,  a_ts_recv);

      //---------------------------------------------------------------------//
      // Decode and Emplace the Msg:                                         //
      //---------------------------------------------------------------------//
      // Generically, we expect that "tid" is "IncrTID"; the latter depends on
      // the Channel template params:
      //
      m_begin     = nullptr;         // Will set it after the Header is decoded
      m_expEnd    = a_buff + a_size; // End of full packet
      m_actEnd    = nullptr;         // Not known yet
      // Get the SeqNum from msg prefix, before the whole msg is decoded. It
      // comes as a 4-byte Little-Endian value:
      m_pktSN     = *(reinterpret_cast<uint32_t const*>(a_buff));
      m_pmap      = 0;
      m_tid       = 0;

      // NB: If any error occurs during msg decoding, it is logged here (NOT at
      // the Reactor level) via the Connector-specific logger:
      try
      {
        //-------------------------------------------------------------------//
        // Get the PMap and TID, and set "m_begin":                          //
        //-------------------------------------------------------------------//
        m_begin     =
          GetMsgHeader
          (a_buff + 4, m_expEnd, &m_pmap, &m_tid, this->m_shortName.data());

        //-------------------------------------------------------------------//
        // Special Case:                                                     //
        //-------------------------------------------------------------------//
        // XXX: Only "IncrementalRefresh" msgs are actually emplaced; for  all
        // other msg types, a placeholder (carrying only a SeqNum) is emplaced;
        // we still need to do that to ensure correctness of SeqNums.
        // However, "TradingSessionStatus" is processed immediately here, even
        // before a placeholder is emplaced:
        //
        if (utxx::unlikely(m_tid == TradingSessionStatusTID))
        {
          m_actEnd     = m_sessStatus.Decode(m_begin, m_expEnd, m_pmap);
          m_mdc->Process(m_sessStatus, a_ts_recv);
          // Still fall through to "Put" below!
        }
        //-------------------------------------------------------------------//
        // Try to Emplace the msg into the "IncrsBuff":                      //
        //-------------------------------------------------------------------//
        // NB:
        // (*) We must do this even if the msg TID is not "IncrTID", because all
        //     msgs within the Channel share a common continuous "SeqNum" space.
        // (*) However,  if the msg is not of the required type,  the  Emplacer
        //     put an EMPTY STUB instead, and the Processor will ignore it.
        // (*) All further processing will be through the "IncrsBuff".    If the
        //     msg is new (its SeqNum is not in the Buffer yet, eg came from an-
        //     other Channel), it will be Emplaced there, and eventually Proces-
        //     sed (but the Processor is not part of this class).
        // (*) Thus, FULL DEPTH OF CALL-BACKS (down to the Strategy level)  will
        //     be invoked through the following call:
        //
        m_incrsBuff->Put(this, SeqNum(m_pktSN), a_ts_recv, handlTS);
      }
      catch (EPollReactor::ExitRun const&)
        { throw; }
      catch (std::exception const& exc)
      {
        // (In most cases, TID will be known):
        LOG_WARN(1,
          "IncrsCh::RecvHandler: Exception: TID={}: {}", m_tid, exc.what())
      }

      // Tell the Reactor to continue getting Incrs:
      return true;
    }

    //=======================================================================//
    // Emplacer for "IncrementalRefresh" Msgs:                               //
    //=======================================================================//
    // Actual decoding occurs here. The decoded (typed) msg is placed straight
    // into the "SeqNumBuff" at the address specified as "a_targ":
    //
    void operator()(IncrementalRefresh* a_targ) const
    {
      assert(a_targ != nullptr && m_begin != nullptr && m_begin < m_expEnd);

      //---------------------------------------------------------------------//
      // Manage the Msg Type:                                                //
      //---------------------------------------------------------------------//
      // We use "if" here, rather than "switch", because all cases except the
      // 1st one are extremely unlikely:
      //
      if (utxx::likely(m_tid == IncrTID))
      {
        // Decode an incoming "IncrementalRefresh" msg (eg OrderBook Increment,
        // or a Trade).
        // XXX: The reason why we cannot just invoke a_targ->Decode() is  that
        // the Decode method may require other (Der-specific) params, so a uni-
        // fied interface "DecodeIncr" is required:
        //
        m_actEnd = EConnector_FAST_Der::DecodeIncr
                   (m_begin, m_expEnd, m_pmap, a_targ);

        // After the msg has been decoded, verify  that its  SeqNum  is indeed
        // the expected one. If not, this is a very serious error condition --
        // maybe not fatal because we use "RptSeq"s (not "MsgSeqNum"s) in Ord-
        // erBook updates, but still serious. For now, "m_pltSN" will prevail:
        CHECK_ONLY
        (
          if (utxx::unlikely(m_pktSN != a_targ->m_MsgSeqNum))
          {
            LOG_WARN(2,
              "IncrsCh::Emplacer({}): SeqNums Mismatch: Decoded={}, Expected="
              "{}: Will use the latter", ForOrderBooks ? "OBIncrs" : "Trades",
              m_pktSN, a_targ->m_MsgSeqNum)

            a_targ->m_MsgSeqNum = m_pktSN;
          }
        )
      }
      else
      {
        // Any other msg type: Put a DUMMY STUB into "a_targ": Use valid SeqNum
        // and RecvTime, but zero-out all other flds:
        // TODO: Factor this out into a special "DummyDecoder"?
        a_targ->m_MsgSeqNum   = m_pktSN;
        a_targ->m_NoMDEntries = 0;

        // Produce a warning if an Inexpected Msg Type was received. Apart from
        // IncrRefresh, we can only expect a HeartBeat or TradingSessionStatus:
        CHECK_ONLY
        (
          if (utxx::unlikely
             (m_tid != HeartBeatTID && m_tid != TradingSessionStatusTID))
            LOG_WARN(2,
              "IncrsCh::Emplacer: UnExpected TID={}: Ignored...", m_tid)
        )
      }

      //---------------------------------------------------------------------//
      // Verify the Msg End (if set non-trivially by a Decoder):             //
      //---------------------------------------------------------------------//
      // XXX: We assume that there is only 1 msg per UDP pkt -- so we must have
      // arrived at the "m_expEnd".    But "m_actEnd" may not be set if the msg
      // was not even parsed (eg HeartBeat, or an ignored type):
      CHECK_ONLY
      (
        if (utxx::unlikely(m_actEnd != nullptr && m_actEnd != m_expEnd))
          LOG_WARN(2,
            "IncrsCh::Emplacer: End-ptr wrong by {} bytes",
            int(m_actEnd - m_expEnd))
      )
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
        "IncrsCh::ErrHandler: IOError: FD={}, ErrNo={}, Events={}: {}: Will "
        "re-start the Connector",
        a_fd, a_err_code, this->m_reactor->EPollEventsToStr(a_events), a_msg)
      this->m_logger->flush();

      // Stop the entire Connector (but do not terminate the Application). XXX:
      // The following operations can presumably be done synchrobously:
      m_mdc->Stop ();
      m_mdc->Start();
    }
  };
} // End namespace FAST
} // End namespace MAQUETTE
