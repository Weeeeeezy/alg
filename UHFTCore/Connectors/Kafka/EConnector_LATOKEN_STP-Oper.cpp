// vim:ts=2:et
//===========================================================================//
//            "Connectors/Kafka/EConnector_LATOKEN_STP-Oper.cpp":            //
//          Kafka-Based STP/DropCopy for LATOKEN: Steady Operations          //
//===========================================================================//
#include "Basis/OrdMgmtTypes.hpp"
#include "Basis/ConfigUtils.hpp"
#include "Basis/Maths.hpp"
#include "Basis/IOUtils.hpp"
#include "Connectors/Kafka/EConnector_LATOKEN_STP.h"
#include "Connectors/EConnector_MktData.hpp"
#include "Protocols/H2WS/LATOKEN-OMC.hpp"
#include "InfraStruct/SecDefsMgr.h"
#include "Venues/LATOKEN/SecDefs.h"
#include "Venues/LATOKEN/Configs_WS.h"
#include "Venues/Binance/SecDefs.h"
#include <utxx/compiler_hints.hpp>
#include <rapidjson/error/en.h>
#include <cstring>
#include <cassert>

using namespace std;

namespace
{
  using namespace MAQUETTE;

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "IsExecMsg":                                                            //
  //-------------------------------------------------------------------------//
  // Is it a REAL TRADE (ie MATCH, ie FILL or PART-FILL). It must also have a
  // valid ExecPx and ExecQty is that case:
  //
  inline bool IsExecMsg(H2WS::LATOKEN::MsgOMC const& a_msg)
  {
    return
      (a_msg.m_ordStatus == FIX::OrdStatusT::Filled           ||
       a_msg.m_ordStatus == FIX::OrdStatusT::PartiallyFilled) &&
       IsPos(a_msg.m_execPx) &&  IsPos(a_msg.m_execQty);
  }

  //-------------------------------------------------------------------------//
  // "InstallFeeTypeInMsgID":                                                //
  //-------------------------------------------------------------------------//
  // Extend MsgID by FeeType for uniqueness:
  //
  inline void InstallFeeTypeInMsgID
  (
    H2WS::LATOKEN::MsgOMC const& a_msg,
    ObjName*                     a_uniq_id
  )
  {
    assert(a_uniq_id != nullptr);
    char* id     =  a_uniq_id->data();
    char* curr   =  id + strlen(id);
    assert(*curr == '\0');
    *curr        =  '+';
    *(++curr)    =  char('0' + int(a_msg.m_feeType));
    ++curr;
    assert(curr - id < int(sizeof(ObjName)) && *curr == '\0');
  }

  //-------------------------------------------------------------------------//
  // "InstallUserCcyInMsgID":                                                //
  //-------------------------------------------------------------------------//
  // Extend MsgID by UserID and CcyID for uniqueness:
  //
  inline void InstallUserCcyInMsgID
  (
    H2WS::LATOKEN::MsgOMC const& a_msg,
    ObjName*                     a_uniq_id
  )
  {
    assert(a_uniq_id != nullptr);
    char* id     =  a_uniq_id->data();
    char* curr   =  id + strlen(id);
    assert(*curr == '\0');
    *curr        =  '+';
    ++curr;
    (void)  utxx::itoa  (a_msg.m_userID, curr);
    *curr        =  '+';
    ++curr;
    (void)  utxx::itoa  (a_msg.m_ccyID,  curr);
    assert(curr - id < int(sizeof(ObjName)) && *curr == '\0');
  }
}

namespace MAQUETTE
{
  //=========================================================================//
  // "GetKafkaMsgs": Main Msg Reading / Parsing / Processing Method:         //
  //=========================================================================//
  void EConnector_LATOKEN_STP::GetKafkaMsgs()
  {
    //-----------------------------------------------------------------------//
    // Read the data from the FD to cancel the event:                        //
    //-----------------------------------------------------------------------//
    // Normally, exactly 1 byte should be available, but it is theoretically
    // possible that Kafka sends multiple events in batch, hemce we need the
    // "m_rbuff".   The bytes read are discarded, as they are for signalling
    // only:
    m_fdInfo.m_fd      = m_rfd;
    m_fdInfo.m_rd_buff = &m_rbuff;
    m_fdInfo.m_tlsSess = nullptr;
    m_fdInfo.m_inst_id = 24459308806;    // Just any non-0

    // IsSocket=false,  No TLS:
    IO::ReadUntilEAgain<false, IO::TLSTypeT::None>
    (
      &m_fdInfo,
      // Action is to return ("consume") the whole number of bytes read w/o
      // analysing them:
      [](char const*, int a_data_sz, utxx::time_val)->int
      {
        assert(a_data_sz >= 0);
        return a_data_sz;
      },
      // Error Handler:
      [this](int a_ret,  int a_err_code)->void
        { this->KafkaPipeError(a_ret, a_err_code, 0, "GetKafkaMsgs"); },
      "EConnector_LATOKEN_STP::GetKafkaMsgs"
    );
    //-----------------------------------------------------------------------//
    // Now REALLY Get Kafka Msg(s):                                          //
    //-----------------------------------------------------------------------//
    while (true)
    {
      //---------------------------------------------------------------------//
      // Get the Event:                                                      //
      //---------------------------------------------------------------------//
      // NB: No time-out: Exit immediately if no more Events are available:
      //
      rd_kafka_event_t* kev = rd_kafka_queue_poll(m_kq, 0);
      if (kev == nullptr)
        break;

      //---------------------------------------------------------------------//
      // Analyse the Event:                                                  //
      //---------------------------------------------------------------------//
      switch (rd_kafka_event_type(kev))
      {
      case RD_KAFKA_EVENT_FETCH:
      {
        //-------------------------------------------------------------------//
        // Generic Case: A DataMsg (or an ErrorMsg):                         //
        //-------------------------------------------------------------------//
        // On the Consumer side, there is exactly 1 DataMsg per event.
        //
        rd_kafka_message_t const* kmsg = rd_kafka_event_message_next(kev);
        assert(kmsg != nullptr && kmsg->payload != nullptr);

        // Partition should be 0, as we do not use partitioning here:
        assert(kmsg->partition == 0);

        // Determine whether it is a Trade or Balance Msg, depending on the
        // Topic:
        char const* topicName = rd_kafka_topic_name(kmsg->rkt);
        bool   isTrd =
          IsProdEnv()
          ? (strncmp(topicName, s_kafkaConsumerInstrTopicsProd[0], 19) == 0)
          : (strncmp(topicName, s_kafkaConsumerInstrTopicsTest[0], 19) == 0);

        DEBUG_ONLY
        (
        bool   isBal =
          IsProdEnv()
          ? (strncmp(topicName, s_kafkaConsumerAssetTopicsProd[0], 19) == 0)
          : (strncmp(topicName, s_kafkaConsumerAssetTopicsTest[0], 19) == 0);
        assert(isBal == !isTrd);
        )
        // The payload (actually JSON) to be processed:
        char const* payload = static_cast<char const*>(kmsg->payload);

        // Could it be an error? If so, we better stop (see also below):
        if (utxx::unlikely(kmsg->err != 0))
        {
          LOG_ERROR(1,
            "EConnector_LATOKEN_STOP::GetKafkaMsgs: Got a Kafka Consumer Error"
            ": {}, EXITING...", string(payload, kmsg->len))
          Stop();
          break;
        }
        else
        if (utxx::likely(kmsg->len > 0))
        {
          //-----------------------------------------------------------------//
          // Generic Case: Normal Trade or Balance Payload (JSON):           //
          //-----------------------------------------------------------------//
          // Parse and Process it, catching all possible exceptions.
          // For Logging and Parsing, 0-terminate "a_data" (XXX: we may write
          // beyond the buffer end, but this is extremely unlikely):
          auto  len =       kmsg->len;
          char  was =       payload [len];
          const_cast<char*>(payload)[len] = '\0';

          LOG_INFO(3,
            "EConnector_LATOKEN_STP::GetKafkaMsgs: GOT {} from {}",
            payload, topicName)
          try
          {
            // Invoke the Parser (and Processor):
            ParseMsg(isTrd, payload);
          }
          catch (exception const& exn)
          {
            LOG_ERROR(1,
              "EConnector_LATOKEN_STP:::GetKafkaMsgs: EXCEPTION: {}",
              exn.what())
            // XXX: If we got an error processing a Kafka msg, this could indi-
            // cate a major inconsistency.  Eg, not clear what is the state of
            // the ShM-based RiskMgr vs the DBs. We better stop and investigate:
            Stop();
          }
          // In any case: Restore the orig char beyond the end:
          const_cast<char*>(payload)[len] = was;
        }
        // If we have reached this point, msg processing was successful (incl  a
        // possible error msg). Commit the offset (Async=true). XXX: In order to
        // reduce the async queue load,   do NOT commit each msg individually --
        // do it at BatchSz interval:
        //
        if (utxx::unlikely(m_msgCount % m_batchSzC == 0))
          rd_kafka_commit_message(m_kafkaC, kmsg, true);

        ++m_msgCount;
        break;
      }

      case RD_KAFKA_EVENT_ERROR:
        //-------------------------------------------------------------------//
        // Error Event:                                                      //
        //-------------------------------------------------------------------//
        // XXX: Not sure how to handle it properly, so better stop as well:
        LOG_ERROR(1,
          "EConnector_LATOKEN_STP::GetKafkaMsgs: Got a Kafka Error: {}, "
          "EXITING...", rd_kafka_event_error_string(kev))
        Stop();
        break;

      case RD_KAFKA_EVENT_REBALANCE:
        //-------------------------------------------------------------------//
        // ReBalance Event:                                                  //
        //-------------------------------------------------------------------//
        // XXX: Again, we currently do not know how to handle it properly, and
        // are even unsure if it can occur in our Kafka archirecture:
        LOG_WARN(1,
          "EConnector_LATOKEN_STP::GetKafkaMsgs: Got a ReBalance Event which "
          "is not supported yet, EXITING...")
        Stop();
        break;

      default: ;
        // Any other possible Event Types are silently ignored...
      }
      //---------------------------------------------------------------------//
      // Finally, destroy the Event (and the Msg):                           //
      //---------------------------------------------------------------------//
      rd_kafka_event_destroy(kev);
    }
  }

  //=========================================================================//
  // "LeaveSTPDynInitMode":                                                  //
  //=========================================================================//
  // Backlog of Kafka msgs has been fetched, we are now receiving msgs in Real-
  // Time:
  void EConnector_LATOKEN_STP::LeaveSTPDynInitMode() const
  {
    assert(m_stpDynInitMode);
    m_stpDynInitMode = false;

    // We can now start the MDCs without the risk of starving them:
    LOG_INFO(1,
      "EConnector_LATOKEN_STP::LeaveSTPDynInitMode: Starting the MDCs")

    for (EConnector_MktData* mdc: m_allMDCs)
    {
      assert(mdc != nullptr);
      mdc->Start();
    }
    // And MktData Updates to Risks are now Enabled:
    m_riskMgr->EnableMktDataUpdates();
  }

  //=========================================================================//
  // "KafkaPipeError":                                                       //
  //=========================================================================//
  // Invoked whern there is an error in the Pipe between Kafka and our Reactor
  // (either in EPoll or in Reading):
  //
  void EConnector_LATOKEN_STP::KafkaPipeError
  (
    int         a_bytes_read,  // Normally (-1) if there was an error
    int         a_err_code,    // ErrNo
    uint32_t    a_events,      // If from EPoll
    char const* a_msg          // Non-NULL
  )
  {
    assert(a_msg != nullptr);
    LOG_ERROR(1,
      "EConnector_LATOKEN_STP: IO Error on Kafka Pipe: FD={}, RC={}, ErrNo={},"
      " Events={}: {}: EXITING...",      m_rfd, a_bytes_read, a_err_code,
      EConnector::m_reactor->EPollEventsToStr(a_events), a_msg)

    // This is a serious error condition -- Stop:
    Stop();
  }

  //=========================================================================//
  // "ParseMsg":                                                             //
  //=========================================================================//
  // Perform JSON Parsing. NB: "a_data" is assumed to be 0-terminated by the
  // CallER:
  //
  void EConnector_LATOKEN_STP::ParseMsg(bool a_is_trd, char const* a_data)
  const
  {
    assert(a_data != nullptr);
    utxx::time_val   recvTS(utxx::now_utc());  // Actually "HandlTS"...

    //-----------------------------------------------------------------------//
    // Run the Parser:                                                       //
    //-----------------------------------------------------------------------//
    // IMPORTANT: Zero-out the "MsgOMC" obj in Handler before parsing a new
    // msg:
    m_handler.Reset();
    rapidjson::StringStream    js(a_data);
    bool parseOK = m_reader.Parse(js, m_handler);

    if (utxx::unlikely(!parseOK))
    {
      // Parsing error encountered: Log it but continue:
      size_t                errOff = m_reader.GetErrorOffset();
      rapidjson::ParseErrorCode ec = m_reader.GetParseErrorCode();

      LOG_ERROR(1,
        "EConnector_LATOKEN_STP::ParseTradeMsg: {}: PARSE ERROR: {} at offset "
        "{}", a_data, rapidjson::GetParseError_En(ec), errOff)
      return;
    }
    //-----------------------------------------------------------------------//
    // Parsing Successful:                                                   //
    //-----------------------------------------------------------------------//
    H2WS::LATOKEN::MsgOMC const& msg = m_handler.GetMsg();

    // Perform top-level checks on whether this msg is relevant at all. This
    // computes the ExchTS (if available):
    pair<bool, utxx::time_val> checkRes = IsNotDuplicateMsg(msg, a_is_trd);
    bool                       isRel    = checkRes.first;
    utxx::time_val             exchTS   = checkRes.second;   // From MsgID

    if (isRel)
    {
      // OK, go for semantical processing:
      bool procOK =
        a_is_trd
        ? ProcessTradeMsg  (m_handler.GetMsg(), exchTS, recvTS)
        : ProcessBalanceMsg(m_handler.GetMsg(), exchTS);    // No RecvTS here

      // If the msg appears to be semantically invalid, log it:
      if (utxx::unlikely(!procOK))
        LOG_ERROR(2,
          "EConnector_LATOKEN_STP::ParseTradeMsg: Kafka Msg Processing Failed: "
         "{}", a_data)
    }
    // Otherwise, the msg is dropped w/o any notification...
  }

  //=========================================================================//
  // "ProcessTradeMsg":                                                      //
  //=========================================================================//
  bool EConnector_LATOKEN_STP::ProcessTradeMsg
  (
    H2WS::LATOKEN::MsgOMC const& a_msg,
    utxx::time_val               a_ts_exch,
    utxx::time_val               a_ts_recv
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert(!a_ts_exch.empty());
    char const* trdID = a_msg.m_id.data();

    // Get the SecDef -- it must be valid:
    SecDefD const* instr = m_secDefsMgr->FindSecDefOpt(a_msg.m_secID);

    if (utxx::unlikely(instr == nullptr))
    {
      LOG_ERROR(1,
        "EConnector_LATOKEN_STP::ProcessTradeMsg: TradeID={}: SecID={} Not "
        "Found", trdID, a_msg.m_secID)
      return false;
    }
    // Before going any further, use this Msg as a "ticker" to update the Liqu-
    // idity Stats (in memory and/or in DB):
    //
    UpdateLiqStats(*instr, a_msg, a_ts_exch);

    // Filter by OrdStatus: It must really be an Exec Msg (ie a Buy|Sell Trade).
    // XXX: We currently use such Exec Msgs for 2 purposes:
    // (*) Drop-Copy (STP)  of all  Trades at LATOKEN
    // (*) "Poor Man's Replacement" of L1 MktData at LATOKEN:
    // In both cases, OrderConfirmations, Cancellations etc are excluded:
    //
    if (!IsExecMsg(a_msg))
      return true; // Not an error

    //-----------------------------------------------------------------------//
    // Try to determine the AggressorSide (never mind if it fails):          //
    //-----------------------------------------------------------------------//
    FIX::SideT aggrSide = FIX::SideT::UNDEFINED;

    if (a_msg.m_makerUserID == a_msg.m_userID)
    {
      // Then this Trade is a Passive Hit/Lift, so the Aggressor was on the
      // OPPOSITE side:
      if (a_msg.m_side == FIX::SideT::Buy)
        aggrSide = FIX::SideT::Sell;
      else
      if (a_msg.m_side == FIX::SideT::Sell)
        aggrSide = FIX::SideT::Buy;
    }
    else
    if (a_msg.m_takerUserID == a_msg.m_userID)
      // Then this Trade is an Aggression, and the Aggressor was on THIS side:
      aggrSide = a_msg.m_side;

    // Still, "aggrSide" may remain UNDEFINED...
    //-----------------------------------------------------------------------//
    // Try to extract L1 MktData:                                            //
    //-----------------------------------------------------------------------//
    // XXX: We do this even for "historical" (in-the-past) LATOKEN Trades. For
    // external Exchanges, however, historical MktData ticks are not received.
    // First of all, try to use L1 OrderBook data if available:
    //
    if (!m_useLATrades)
      UpdateL1Pxs(*instr, a_msg.m_bestBidPx, a_msg.m_bestAskPx, a_ts_recv);
    else
    // "Poor Man's Solution":    Using latest Trade Pxs for L1 OrderBook ones.
    // This requires the AggrSide to be available: only 1 side is updated at a
    // time:
    if (utxx::likely(aggrSide != FIX::SideT::UNDEFINED))
    {
      // The Side on which we update the OrderBook is opposite to the Aggressor
      // Side. We use the ExecPx as a proxy of the curr L1 Px at that Side:
      //
      bool   isBid = (aggrSide == FIX::SideT::Sell);
      PriceT bidPx =  isBid ? a_msg.m_execPx : PriceT();
      PriceT askPx = !isBid ? a_msg.m_execPx : PriceT();

      UpdateL1Pxs(*instr, bidPx, askPx, a_ts_recv);
    }
    //-----------------------------------------------------------------------//
    // Check the TimeStamp for DynInitMode:                                  //
    //-----------------------------------------------------------------------//
    // IMPORTANT: As long as ExchTS is far behind RecvTS, we remain in DynIn-
    // itMode (ie fetching backlog of Kafka msgs). Once ExchTS catches up w/
    // RecvTS, are are ready to exit DynInitMode:
    if (m_stpDynInitMode && (a_ts_recv - a_ts_exch).sec() < 10)
        LeaveSTPDynInitMode();

    //-----------------------------------------------------------------------//
    // Check UserIDs once again:                                             //
    //-----------------------------------------------------------------------//
    // Is it an Internal MM account?
    bool isIntMM =
      (LATOKEN::APIKeysIntMM.find(a_msg.m_userID) !=
       LATOKEN::APIKeysIntMM.cend());

    // For Internal MM accounts (any ONLY for them!),    if all 3 IDs are same,
    // then it is an intra-account Trade  which we normally do not need to pro-
    // cess at all, because it does not change any positions and does not incur
    // any fees (unless "WithXTrds" is set). For all other accounds, incl ExtMM
    // ones, we DO record self-trades because they may still incur trading fees:
    //
    if (isIntMM && !m_withXTrds && a_msg.m_makerUserID == a_msg.m_userID &&
                                   a_msg.m_takerUserID == a_msg.m_userID)
      return true; // Not an error

    // The opposite situation: both MakerID and TakerID are different from the
    // UserID (eg if MakerID and TakerID are both 0).  This should not happen
    // for a real Match; but we only produce a warning in this case and still
    // proceed:
    if (utxx::unlikely (a_msg.m_makerUserID != a_msg.m_userID &&
                        a_msg.m_takerUserID != a_msg.m_userID))
      LOG_WARN(2,
        "EConnector_LATOKEN_STP::ProcessTradeMsg: TradeID={}: Inconsistency: "
        "UserID={}, MakerUserID={}, TakerUserID={}",
        trdID,  a_msg.m_userID, a_msg.m_takerUserID, a_msg.m_takerUserID)

    //-----------------------------------------------------------------------//
    // Compute the Fee/Commission:                                           //
    //-----------------------------------------------------------------------//
    // TODO:  Currently, the Fees are 0 for MM accounts, or a fixed percentage
    // of the Traded Qty expressed in QuoteCcy, depending on the latter.  Need
    // to implement mechanisms for analysing Trade and Balance msgs to support
    // variable (per-UserID) Fees:
    //
    RMQtyB feeB
    (
      isIntMM
      ? 0.0
      : GetFeeRate(*instr) * double(a_msg.m_execQty) * double(a_msg.m_execPx)
    );
    //-----------------------------------------------------------------------//
    // Create a "Trade" Msg:                                                 //
    //-----------------------------------------------------------------------//
    // As we now know that "MsgOMC" is a valid Trade indeed. It is NOT allocat-
    // ed in ShM, so its Idx=0:
    Trade tr
    (
      0,
      nullptr,      // NOT from an MDC
      instr,
      nullptr,      // No Req12 because it is STP
      a_msg.m_userID,
      ExchOrdID(a_msg.m_id.data(), sizeof(a_msg.m_id)),
      a_msg.m_execPx,
      a_msg.m_execQty,
      feeB,
      aggrSide,
      a_msg.m_side,
      a_ts_recv,    // XXX: Do NOT use OrigTS here; the latter is OrderOrigina-
      a_ts_recv     // tion time!
    );
    //-----------------------------------------------------------------------//
    // Invoke the RiskMgr:                                                   //
    //-----------------------------------------------------------------------//
    m_riskMgr->OnTrade(tr);

    // All Done:
    return true;
  }

  //=========================================================================//
  // "ProcessBalanceMsg":                                                    //
  //=========================================================================//
  // Returns "true" on success, "false" on error:
  //
  bool EConnector_LATOKEN_STP::ProcessBalanceMsg
  (
    H2WS::LATOKEN::MsgOMC const& a_msg,
    utxx::time_val               a_ts_exch
  )
  const
  {
    // First of all, it must have a valid UserID and CcyID, otherwise this Bal-
    // ance update cannot be applied at all:
    if (utxx::unlikely(a_msg.m_userID == 0 || a_msg.m_ccyID == 0))
    {
      LOG_WARN(2,
        "EConnector_LATOKEN_STP::ProcessBalanceMsg: MsgID={}: Invalid: UserID="
        "{}, CcyID={}", a_msg.m_id.data(), a_msg.m_userID, a_msg.m_ccyID)
      return false;
    }

    // Get the AssetName:
    auto cit =  m_ccyNames.find(a_msg.m_ccyID);
    if  (utxx::unlikely(cit == m_ccyNames.cend()))
    {
      LOG_WARN(2,
        "EConnector_LATOKEN_STP::ProcessBalanceMsg: CcyID={} Not Found",
        a_msg.m_ccyID)
      return false;
    }
    char const* assetName = cit->second;
    assert(assetName != nullptr);

    // FIXME: SettlDate is NOT available from "a_msg". For now, we always assume
    // it to be 0, ie "Instantaneous Spot":
    int const settlDate = 0;

    // Try to determine the AssetTransT:
    AssetRisks::AssetTransT transType =
      (a_ts_exch.empty() && a_msg.m_feeType == H2WS::LATOKEN::FeeT::UNDEFINED &&
       a_msg.m_fee == 0.0)
      ? // FIXME: This is a Transfer{In|Out} or {Deposit|Withdrawal}  or {Borr-
        // owing|Lending}. Of them, only the 1st option is currently supported.
        // The direction does not matter (adjusted  automatically):
        AssetRisks::AssetTransT::TransferIn

      : // Otherwise, it is typically an Order/Trade, but that info is irrelev-
        // ant to RiskMgr: It would only set the InitPos (if applicable) in
        // that case, so:
        AssetRisks::AssetTransT::UNDEFINED;

    // Invoke the RiskMgr:
    m_riskMgr->OnBalanceUpdate
    (
      assetName,
      settlDate,
      a_msg.m_id.data(),
      a_msg.m_userID,
      transType,
      double(a_msg.m_amount), // NB: "Amount", NOT "ExecQty" here!
      a_ts_exch
    );
    return true;
  }

  //=========================================================================//
  // "IsNotDuplicateMsg":                                                    //
  //=========================================================================//
  // Filter-in only pertinent msgs. NB: "a_is_trd" is used to distinguish Msgs
  // coming from "Trd" Kafka Topics (actually including Order Acks!),  and Bal
  // Topics:
  //
  std::pair<bool, utxx::time_val> EConnector_LATOKEN_STP::IsNotDuplicateMsg
    (H2WS::LATOKEN::MsgOMC const& a_msg, bool a_is_trd)
  const
  {
    //-----------------------------------------------------------------------//
    // Checks the UserID:                                                    //
    //-----------------------------------------------------------------------//
    // It is OK if the UserID is 0 (eg currently happens on CancelReject msgs),
    // but we just skip it:
    if (utxx::unlikely(a_msg.m_userID == 0))
      return make_pair(false, utxx::time_val());

    //-----------------------------------------------------------------------//
    // Extract the ExchTS from the MsgID:                                    //
    //-----------------------------------------------------------------------//
    utxx::time_val exchTS = H2WS::LATOKEN::MkTSFromMsgID(a_msg.m_id.data());

    // TradeMsgs MUST have a valid ExchTS:
    if (utxx::unlikely(a_is_trd  && exchTS.empty()))
    {
      LOG_ERROR(1,
        "EConnector_LATOKEN_STP::ProcessTradeMsg: Invalid TrdID={}",
        a_msg.m_id.data())
      return make_pair(false, utxx::time_val());
    }

    //-----------------------------------------------------------------------//
    // IMPORTANT: Check for Duplicate IDs:                                   //
    //-----------------------------------------------------------------------//
    // This may require modification of MsgIDs, but keep the original ID intact:
    // XXX: At the moment, TrdID/TransID is sufficient here; we do not need the
    // OrdID:
    ObjName uniqID = a_msg.m_id;

    if (utxx::likely(!exchTS.empty()))
    {
      //---------------------------------------------------------------------//
      // Msg with ExchTS:                                                    //
      //---------------------------------------------------------------------//
      // Any Msgs with a valid ExchTS (all TradeMsgs, and most of BalanceMsgs)
      // are checked against "m_drm{Ord,Trd,Bal}Set" which is a running window
      // of Msgs' ExchTimeStamps:
      //
      // Select the Set against which we make a check:
      DRMSet*     drmSet = nullptr;
      char const* where  = "";

      if (a_is_trd)
      {
        // Ie NOT a Balance Msg. NB: The Key is a TradeID, and it is supposed
        // to be unique: No modifications are required:
        //
        if (IsExecMsg(a_msg))
        {
          // REALLY a Trade (Fill/PartFill):
          drmSet = m_drmTrdSet;
          where  = "Trd";
        }
        else
        {
          // An Order Ack (but STILL coming from a "*trd*" Kafka Topic!):
          drmSet = m_drmOrdSet;
          where  = "Ord";
        }
      }
      else
      {
        // Balance Msg with a TS: MsgID (or TS) is NOT a unique Key; must ext-
        // end it to MsgID+FeeType:
        InstallFeeTypeInMsgID(a_msg, &uniqID);
        drmSet = m_drmBalSet;
        where  = "Bal";
      }
      // Try to insert the ExchTS into this Set. If it already exists there,
      // insert wlll fail, and we know it is a Duplicate:
      auto insRes = drmSet->insert(uniqID);

      if (utxx::unlikely(!insRes.second))
      {
        // Yes, it is a Duplicate!
        LOG_WARN(1,
          "EConnector_LATOKEN_STP::IsNotDuplicateMsg(WithTS, {}): Found "
          "Duplicate MsgID={}", where, uniqID.data())
        return make_pair(false, utxx::time_val());
      }
      // Otherwise: Insert successful -- remove the oldest element (normally at
      // most 1) to bring the Set size back to a constant:
      //
      while (drmSet->size() > DRMSetSz - 1)
        drmSet->erase(drmSet->begin());
    }
    else
    {
      //---------------------------------------------------------------------//
      // Msg w/o an ExchTS:                                                  //
      //---------------------------------------------------------------------//
      // Such Msg is certainly a Balance one. It is a Transfer or a similar
      // transaction:
      assert(!a_is_trd);

      // The TransactionID (36 bytes) may not be unique, as a single transaction
      // may have multiple destinations. Install extension: "+UserID+CcyID":
      InstallUserCcyInMsgID(a_msg, &uniqID);

      // from the front (most recent) towards the end (oldest)  IDs:
      auto cit = find(m_drmDEQ->cbegin(), m_drmDEQ->cend(), uniqID);

      if (utxx::unlikely(cit != m_drmDEQ->cend()))
      {
        // Yes, it is a Duplicate!
        LOG_WARN(1,
          "EConnector_LATOKEN_STP::IsNotDuplicateMsg(w/o TS): Found Duplicate "
          "MsgID={}", uniqID.data())
        return make_pair(false, utxx::time_val());
      }
      // Otherwise: Not a Duplicate. Pop out the front (earlist) ID(s) and push
      // back the new one:
      while (m_drmDEQ->size() >= DRMDEQSz - 1)
        m_drmDEQ->pop_back ();
      m_drmDEQ->push_front(uniqID);
    }
    //-----------------------------------------------------------------------//
    // If we got here: YES, it is relevant:                                  //
    //-----------------------------------------------------------------------//
    return make_pair(true, exchTS);
  }

  //=========================================================================//
  // "GetFeeRate":                                                           //
  //=========================================================================//
  double EConnector_LATOKEN_STP::GetFeeRate(SecDefD const& a_instr) const
  {
    // Get the QuoteCcy:
    char const* ccyB = a_instr.m_QuoteCcy.data();

    // Find the corresp Fee Rate (default is 0.001):
    double rate = 0.001;
    auto   cit  = m_feeRates.find(ccyB);

    if (utxx::likely(cit != m_feeRates.cend()))
      rate = cit->second;
    else
      LOG_WARN(1, "EConnector_LATOKEN_STP::GetFeeRate: No data for {}", ccyB)

    return rate;
  }

  //=========================================================================//
  // "UpdateL1Pxs":                                                          //
  //=========================================================================//
  void EConnector_LATOKEN_STP::UpdateL1Pxs
  (
    SecDefD const&    a_instr,
    PriceT            a_bid_px,
    PriceT            a_ask_px,
    utxx::time_val    a_ts
  )
  const
  {
    //-----------------------------------------------------------------------//
    // Find the corresp L1 OrderBook in ShM -- linear search is OK:          //
    //-----------------------------------------------------------------------//
    OrderBookBase* ob = nullptr;
    for (int i = 0; i < m_nLABooks; ++i)
      if (&(m_laBooksShM[i].GetInstr()) == &a_instr)
      {
        ob = m_laBooksShM + i;
        break;
      }
    if (utxx::unlikely(ob == nullptr))
    {
      // This must NOT happen, because we create OrderBooks for ALL LATOKEN
      // Instrs:
      LOG_ERROR(1,
        "EConnector_LATOKEN_STP::UpdateL1Pxs: OrderBook for {} Not Found",
        a_instr.m_FullName.data())
      return;
    }
    //-----------------------------------------------------------------------//
    // Try to update this OrderBook and invoke the RiskMgr:                  //
    //-----------------------------------------------------------------------//
    PriceT wasBid = ob->GetBestBidPx();
    PriceT wasAsk = ob->GetBestAskPx();

    // NB: IsRelaxed=true (Bid-Ask collisions are tolerated to some extent). If
    // either Bid or Ask px is not available (or both), do not try to set it:
    bool ok = true;
    if (IsPos(a_bid_px))
      ok  = ob->SetBestBidPx<true>(a_bid_px);
    if (IsPos(a_ask_px))
      ok &= ob->SetBestAskPx<true>(a_ask_px);

    // If successfully updated, invoke the RiskMgr:
    if (utxx::likely(ok))
      m_riskMgr->OnMktDataUpdate(*ob, a_ts);
    else
      LOG_WARN(3,
        "EConnector_LATOKEN_STP::UpdateL1Pxs: {}: Conflict: Prev={}..{}, "
        "New={}..{}",   a_instr.m_FullName.data(),
        double(wasBid), double(wasAsk), double(a_bid_px), double(a_ask_px))

    // NB: Do NOT update the LiqStats here -- it was a LATOKEN MktData tick,
    // NOT a BenchMark one!
  }

  //=========================================================================//
  // "InstrRisksUpdate": Call-Back from "RiskMgr":                           //
  //=========================================================================//
  // Invoked when a Trade was processed by the RiskMgr:
  //
  void EConnector_LATOKEN_STP::InstrRisksUpdate
  (
    InstrRisks const& a_ir,        // State  AFTER  the Trade
    RMQtyA            a_ppos_a,    // Prev Pos
    char const*       a_trade_id,  // Trade ID
    bool              a_is_buy,    // For the UserID's side; if MtM, it is the
                                   //   side of MtM RefPx below
    PriceT            a_px_ab,     // ExecPx(A/B)  or MtM RefPx
    RMQtyA            a_qty_a,     // 0 for MtM, otherwise > 0
    RMQtyB            a_rgain_b,   // RealisedGain(B) (0 if not closing or MtM)
    double            a_b_rfc,     // ValRate (B/RFC)
    double            a_rgain_rfc  // RealisedGain(RFC)
  )
  {
    //-----------------------------------------------------------------------//
    // MtM Rate Throttling:                                                  //
    //-----------------------------------------------------------------------//
    bool isMtM = IsZero(a_qty_a);

    // Get the Throttler for this Instr:
    SecID secID = a_ir.m_instr->m_SecID;
    auto  it    = m_instrThrots.find(secID);
    if (utxx::unlikely(it == m_instrThrots.end()))
    {
      LOG_WARN(1,
        "EConnector_LATOKEN_STP::InstrRisksUpdate: Throttler for SecID={} "
        "Not Found", secID)
      return;
    }
    Throttler& throt = it->second; // Ref!

    // Get the curr time (NOT ExchTS):
    utxx::time_val now = utxx::now_utc();

    // Refresh the Throttler in any case:
    throt.refresh(now);

    if  (isMtM)
    {
      // IN ANY CASE, on an MtM update, re-calculate the Liquidity Stats, but
      // only if it is an external (BenchMark) px, NOT LATOKEN MktData tick.
      // These two cases are distinguished by the non-NULL MDC ptr in the corr
      // OrderBook. Then "a_is_buy" is the side of the RefPx:
      //
      if (a_ir.m_ob != nullptr && a_ir.m_ob->GetMDCOpt() != nullptr)
        UpdateLiqStats(*a_ir.m_instr, a_is_buy, a_px_ab, now);

      // If it is indeed an MtM, check the RunningSum:
      if (throt.running_sum() >= m_throtLimit)
        return;

      // If we got here, we DO proceed with an MtM update -- so increment the
      // counter:
      throt.add(now, 1);
    }
    //-----------------------------------------------------------------------//
    // Generate a JSON msg in "m_sbuff":                                     //
    //-----------------------------------------------------------------------//
    // NB: PrevPosA is currently not recorded;
    //     QtyA     is unused because available via the TradeID;
    //     TS       is similar:
    //
    char*   curr = m_sbuff;
    char*   end  = m_sbuff + sizeof(m_sbuff);

    // (1) Ver=2, ExchTS:
    curr  = stpcpy(curr, "{\"ver\":2,\"exchts\":");
    (void)  utxx::itoa  (a_ir.m_ts.sec(),  curr);
    *curr = '.';
    ++curr;
    (void)  utxx::itoa  (a_ir.m_ts.usec(), curr);

    // (2) UserID:
    curr  = stpcpy(curr, ",\"userid\":");
    (void)  utxx::itoa  (a_ir.m_userID,    curr);

    // (3) Instrument (as SecID):
    curr  = stpcpy(curr, ",\"instrid\":");
    (void)  utxx::itoa  (a_ir.m_instr->m_SecID, curr);

    // (4) RFC: XXX: We install the RFC Symbol here, NOT the LATOKEN AssetID:
    curr  = stpcpy(curr, ",\"rfc\":\"");
    curr  = stpcpy(curr, m_riskMgr->GetRFC().data());

    // (5) TransType:
    //     If "a_qty_a" is 0, it is MtM ('3'), otherwise use "a_is_buy":
    //     NB: This encoding is consistent with "AssetTransT"!
    assert(isMtM || IsPos(a_qty_a));
    assert(isMtM == (a_trade_id == nullptr || *a_trade_id == '\0'));

    curr  = stpcpy(curr, "\",\"trntp\":");
    *curr = isMtM ? '3' : (a_is_buy ? '1' : '2');
    ++curr;

    // (6) TradeID: Skip altogether if empty (MtM):
    if (!isMtM)
    {
      curr  = stpcpy(curr, ",\"trdid\":\"");
      curr  = stpcpy(curr, a_trade_id);
      *curr = '"';
      ++curr;
    }
    // (7) ExecPx or MtM RefPx: Must be valid:
    assert(IsFinite(double(a_px_ab)));
    curr  = stpcpy(curr, ",\"prx\":");
    curr += utxx::ftoa_left
              (double(a_px_ab),      curr,  int(end-curr), DecPrec);

    // (8) New (Current) Pos in this Instr (ie PosA):
    assert(IsFinite(double(a_ir.m_posA)));
    curr  = stpcpy(curr, ",\"posa\":");
    curr += utxx::ftoa_left
              (double(a_ir.m_posA),  curr,  int(end-curr), DecPrec);

    // (9) CostPx:
    assert(IsFinite(double(a_ir.m_avgPosPxAB)));
    curr  = stpcpy(curr, ",\"costpx\":");
    curr += utxx::ftoa_left
              (double(a_ir.m_avgPosPxAB),   curr, int(end-curr), DecPrec);

    // (10) ValuationRate(B/RFC): XXX: Check that it is valid (may be NaN,
    //    although this is extremely unlikely -- then skip it):
    if (utxx::likely(AssetRisks::IsValidRate(a_b_rfc)))
    {
      curr  = stpcpy(curr, ",\"brfcrate\":");
      curr += utxx::ftoa_left(a_b_rfc,      curr, int(end-curr), DecPrec);
    }
    // (11) RealisedGain of this Trade(B):   Always 0 for MtM:
    assert(IsFinite(double(a_rgain_b))  && (!isMtM || a_rgain_b == 0.0));
    curr  = stpcpy(curr, ",\"rlsdgnb\":");
    curr += utxx::ftoa_left(double(a_rgain_b),  curr, int(end-curr), DecPrec);

    // (12) RealisedGain of this Trade(RFC): Always 0 for MtM. Skip if NaN:
    if (utxx::likely(IsFinite(a_rgain_rfc)))
    {
      assert(!isMtM || a_rgain_rfc == 0.0);
      curr  = stpcpy(curr, ",\"rlsdgnrfc\":");
      curr += utxx::ftoa_left(a_rgain_rfc,      curr, int(end-curr), DecPrec);
    }
    // (13) UnRealisedPnL (B):
    assert(IsFinite(double(a_ir.m_unrPnLB)));
    curr  = stpcpy(curr, ",\"unrlsdpnlb\":");
    curr += utxx::ftoa_left
            (double(a_ir.m_unrPnLB), curr, int(end-curr), DecPrec);

    // (14) UnRealisedPnL (RFC): Skip if NaN:
    if (utxx::likely(IsFinite(a_ir.m_unrPnLRFC)))
    {
      curr  = stpcpy(curr, ",\"unrlsdpnlrfc\":");
      curr += utxx::ftoa_left(a_ir.m_unrPnLRFC, curr, int(end-curr), DecPrec);
    }
    // (15) Cumulative RealisedPnL (B):
    assert(IsFinite(double(a_ir.m_realisedPnLB)));
    curr  = stpcpy(curr, ",\"cumrlsdpnlb\":");
    curr += utxx::ftoa_left
              (double(a_ir.m_realisedPnLB),     curr, int(end-curr), DecPrec);

    // (16) Cumulative RealisedPnL (RFC): Skip if NaN:
    if (utxx::likely(IsFinite(a_ir.m_realisedPnLRFC)))
    {
      curr  = stpcpy(curr, ",\"cumrlsdpnlrfc\":");
      curr += utxx::ftoa_left
                (a_ir.m_realisedPnLRFC, curr, int(end-curr), DecPrec);
    }
    // (17) Appreciation Part of Cum Realised PnL (RFC): Skip if NaN:
    if (utxx::likely(IsFinite(a_ir.m_apprRealPnLRFC)))
    {
      curr  = stpcpy(curr, ",\"apprrlsdpnlrfc\":");
      curr += utxx::ftoa_left
                (a_ir.m_apprRealPnLRFC, curr, int(end-curr), DecPrec);
    }
    // Terminator:
    *curr = '}';
    ++curr;
    *curr = '\0';
    assert(curr < end);

    int    msgLen = int(curr - m_sbuff);
    assert(msgLen > 0);

    //-----------------------------------------------------------------------//
    // Send it out (IsInstr=true):                                           //
    //-----------------------------------------------------------------------//
    SendToKafka<true>(a_ir.m_userID, msgLen);

    LOG_INFO(3,
      "EConnector_LATOKEN_STP::InstrRisksUpdate: OrigTS={}, PrevPosA={}, "
      "QtyA={}",  a_ir.m_ts.sec(), double(a_qty_a), double(a_ppos_a))
  }

  //=========================================================================//
  // "AssetRisksUpdate": Call-Back from "RiskMgr":                           //
  //=========================================================================//
  void EConnector_LATOKEN_STP::AssetRisksUpdate
  (
    AssetRisks const&       a_ar,
    char const*             a_trans_id,
    AssetRisks::AssetTransT a_trans_t,
    double                  a_qty,
    double                  a_eval_rate
  )
  {
    //-----------------------------------------------------------------------//
    // MtM Rate Throttling:                                                  //
    //-----------------------------------------------------------------------//
    bool isMtM = (a_trans_t == AssetRisks::AssetTransT::MtM);

    // Get the Throttler for this Asset:
    auto  it    = m_assetThrots.find(a_ar.m_asset.data());
    if (utxx::unlikely(it == m_assetThrots.end()))
    {
      LOG_WARN(1,
        "EConnector_LATOKEN_STP::AssetRisksUpdate: Throttler for Asset={} "
        "Not Found", a_ar.m_asset.data())
      return;
    }
    Throttler& throt = it->second; // Ref!

    // Get the curr time (NOT ExchTS):
    utxx::time_val now = utxx::now_utc();

    // Refresh the Throttler in any case:
    throt.refresh(now);

    if  (isMtM)
    {
      // If it is indeed an MtM, check the RunningSum:
      if (throt.running_sum() >= m_throtLimit)
        return;

      // If we got here, we DO proceed with an MtM update -- so increment the
      // counter:
      throt.add(now, 1);
    }
    //-----------------------------------------------------------------------//
    // Generate a JSON msg in "m_sbuff":                                     //
    //-----------------------------------------------------------------------//
    char*   curr = m_sbuff;
    char*   end  = m_sbuff + sizeof(m_sbuff);

    // (1) Ver=2, ExchTS:
    curr  = stpcpy(curr, "{\"ver\":2,\"exchts\":");
    (void)  utxx::itoa  (a_ar.m_ts.sec(),  curr);
    *curr = '.';
    ++curr;
    (void)  utxx::itoa  (a_ar.m_ts.usec(), curr);

    // (2) UserID:
    curr  = stpcpy(curr, ",\"userid\":");
    (void)  utxx::itoa  (a_ar.m_userID,    curr);

    // (3) CcyID: Perform Look-Up:
    auto cit = m_ccyIDs.find(a_ar.m_asset.data());
    if (utxx::unlikely(cit == m_ccyIDs.cend()))
    {
      LOG_ERROR(1,
        "EConnector_LATOKEN_STP::AssetRisksUpdate: Asset Not Found: {}",
        a_ar.m_asset.data())
      // Skip this update but do not throw any exceptions:
      return;
    }
    // If found:
    unsigned ccyID = cit->second;
    curr  = stpcpy(curr, ",\"ccyid\":");
    (void)  utxx::itoa(ccyID, curr);

    // (4) SettlDate:
    curr  = stpcpy(curr, ",\"settldate\":");
    (void)  utxx::itoa(a_ar.m_settlDate, curr);

    // (5) RFC: XXX: We install the RFC Symbol here, NOT the LATOKEN AssetID:
    curr  = stpcpy(curr, ",\"rfc\":\"");
    curr  = stpcpy(curr, m_riskMgr->GetRFC().data());

    // (6) TransType:
    curr  = stpcpy(curr, "\",\"transtype\":");
    (void)  utxx::itoa(unsigned(a_trans_t),  curr);

    // (7) TransID: Unavailable for MtM (may stil lbe available for Initial):
    bool   hasTransID =  (a_trans_id != nullptr && *a_trans_id != '\0');
    assert(hasTransID == !isMtM);
    if (hasTransID)
    {
      curr  = stpcpy(curr, ",\"transid\":\"");
      curr  = stpcpy(curr, a_trans_id);
      *curr = '"';
      ++curr;
    }
    // (8) Epoch:
    curr  = stpcpy(curr, ",\"epoch\":");
    (void)  utxx::itoa  (a_ar.m_epoch.sec(),  curr);
    *curr = '.';
    ++curr;
    (void)  utxx::itoa  (a_ar.m_epoch.usec(), curr);

    // (9) InitialPos: XXX: May be NaN, then skip it:
    if (utxx::likely(IsFinite(a_ar.m_initPos)))
    {
      curr  = stpcpy(curr, ",\"initialp\":");
      curr += utxx::ftoa_left(a_ar.m_initPos,    curr, int(end-curr), DecPrec);
    }
    // (10) TradingDelta:
    assert(IsFinite(a_ar.m_trdDelta));
    curr  = stpcpy(curr, ",\"trddelta\":");
    curr += utxx::ftoa_left(a_ar.m_trdDelta,     curr, int(end-curr), DecPrec);

    // (11) CumTransfers (TransfersIn - TransfersOut):
    assert(IsFinite(a_ar.m_cumTranss));
    curr  = stpcpy(curr, ",\"cumtransfers\":");
    curr += utxx::ftoa_left(a_ar.m_cumTranss,    curr, int(end-curr), DecPrec);

    // (12) CumDeposits (Deposits    - Withdrawals):
    assert(IsFinite(a_ar.m_cumDeposs));
    curr  = stpcpy(curr, ",\"cumdeposits\":");
    curr += utxx::ftoa_left(a_ar.m_cumDeposs,    curr, int(end-curr), DecPrec);

    // (13) CumDebt     (Borrowings  - Lendings):
    assert(IsFinite(a_ar.m_cumDebt));
    curr  = stpcpy(curr, ",\"cumdebt\":");
    curr += utxx::ftoa_left(a_ar.m_cumDebt,      curr, int(end-curr), DecPrec);

    // (14) Asset/RFC Valuation Rate: Skip it if NaN:
    if (utxx::likely(AssetRisks::IsValidRate(a_eval_rate)))
    {
      curr  = stpcpy(curr, ",\"assetrfcrate\":");
      curr += utxx::ftoa_left(a_eval_rate,       curr, int(end-curr), DecPrec);
    }
    // (15) Initial Pos in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_initRFC)))
    {
      curr  = stpcpy(curr, ",\"initialrfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_initRFC,         curr, int(end-curr), DecPrec);
    }
    // (16) Trading Delta in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_trdDeltaRFC)))
    {
      curr  = stpcpy(curr, ",\"trddeltarfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_trdDeltaRFC,     curr, int(end-curr), DecPrec);
    }
    // (17) Cumulative Transfers in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_cumTranssRFC)))
    {
      curr  = stpcpy(curr, ",\"cumtransfersrfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_cumTranssRFC,    curr, int(end-curr), DecPrec);
    }
    // (18) Cumulative Deposits  in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_cumDepossRFC)))
    {
      curr  = stpcpy(curr, ",\"cumdepositsrfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_cumDepossRFC,    curr, int(end-curr), DecPrec);
    }
    // (19) Cumulative Debt in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_cumDebtRFC)))
    {
      curr  = stpcpy(curr, ",\"cumdebtrfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_cumDebtRFC,      curr, int(end-curr), DecPrec);
    }
    // (20) Appreciation Part of Initial in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_apprInitRFC)))
    {
      curr  = stpcpy(curr, ",\"apprinitialrfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_apprInitRFC,     curr, int(end-curr), DecPrec);
    }
    // (21) Appreciation Part of Trading Delta in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_apprTrdDeltaRFC)))
    {
      curr  = stpcpy(curr, ",\"apprtrddeltarfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_apprTrdDeltaRFC, curr, int(end-curr), DecPrec);
    }
    // (22) Appreciation Part of Transfers in RFC: Skip in NaN:
    if (utxx::likely(IsFinite(a_ar.m_apprTranssRFC)))
    {
      curr  = stpcpy(curr, ",\"apprtransfersrfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_apprTranssRFC,   curr, int(end-curr), DecPrec);
    }
    // (23) Appreciation Part of Deposits  in RFC: Skip if NaN:
    if (utxx::likely(IsFinite(a_ar.m_apprDepossRFC)))
    {
      curr  = stpcpy(curr, ",\"apprdepositsrfc\":");
      curr += utxx::ftoa_left
              (a_ar.m_apprDepossRFC,   curr, int(end-curr), DecPrec);
    }
    // Terminator:
    *curr = '}';
    ++curr;
    *curr = '\0';
    assert(curr < end);

    int    msgLen = int(curr - m_sbuff);
    assert(msgLen > 0);

    //-----------------------------------------------------------------------//
    // Send it out (IsInstr=false):                                          //
    //-----------------------------------------------------------------------//
    SendToKafka<false>(a_ar.m_userID, msgLen);

    LOG_INFO(3,
      "EConnector_LATOKEN_STP::AssetRisksUpdate: TransTS={}, TransQty={}",
      a_ar.m_ts.sec(), a_qty)
  }

  //=========================================================================//
  // "SendToKafka":                                                          //
  //=========================================================================//
  // Sends out the content of "m_sbuff". Returns the ptr to the TopicName used
  // (for logging only).
  // Returns "true" on successful sending, "false" otherwise:
  //
  template<bool IsInstr>
  inline void EConnector_LATOKEN_STP::SendToKafka(UserID a_user_id, int a_len)
  const
  {
    assert(a_len > 0);
    bool sent = false;

    // We select the Topic even if sendingf is dosabled:
    // Get the list of applicable Topics:
    vector<rd_kafka_topic_t*> const& topics =
      IsInstr ? m_instrTopics : m_assetTopics;

    // The list must be non-empty (currently of length 4):
    unsigned N = unsigned(topics.size());
    assert  (N > 0);

    // Select the actual topic by UserID:
    rd_kafka_topic_t* topic = topics[a_user_id % N];
    char const*       tName = rd_kafka_topic_name(topic);
    assert(topic != nullptr && tName != nullptr);

    // Send the msg (UNLESS Sending is disabled). Partition=0 for the moment.
    // The content of our buffer is to be copied, as the send is asynchronous
    // and we will start over-writing "m_sbuff" immediately afterwards.    No
    // MsgKeys etc:
    if (!m_noDBMode)
    {
      int rc =
        rd_kafka_produce
          (topic,   0, RD_KAFKA_MSG_F_COPY, m_sbuff, size_t(a_len),
           nullptr, 0, nullptr);

      // XXX: Currently, we do NOT throw an exception if the send has failed: An
      // exception would result in non-commit of the input Kafka msg, and there-
      // fore on repeated processing of that msg later.   This would normally be
      // caught by duplicates-removal logic, but better be avoided anyway.   And
      // a failed DB update would "heal itself" in subsequent updates,  provided
      // that the ShM data structures are consistent:
      //
      if (utxx::unlikely(rc != 0))
      {
        LOG_ERROR(1,
          "EConnector_LATOKEN_STP::SendToKafka: SEND FAILED: {}: {}",
          tName, rd_kafka_err2str(rd_kafka_last_error()))
      }
      else
        // Successfully sent, finally:
        sent = true;
    }
    // Log the result anyway:
    LOG_INFO(3,
      "EConnector_LATOKEN_STP::SendToKafka: {} to {}: {}",
      (sent ? "SENT" : "NOT SENT"), tName, m_sbuff)

    // Finally, just for extra safety, 0-terminate "m_sbuff":
    m_sbuff[0] = '\0';
  }
} // End namespace MAQUETTE
