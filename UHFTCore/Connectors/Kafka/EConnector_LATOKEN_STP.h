// vim:ts=2:et
//===========================================================================//
//                 "Connectors/Kafka/EConnector_LATOKEN_STP.h":              //
//                     Kafka-Based STP/DropCopy for LATOKEN                  //
//===========================================================================//
#pragma once

#include "Connectors/EConnector.h"
#include "Protocols/H2WS/LATOKEN-OMC.h"
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "InfraStruct/RiskMgr.h"
#include <utxx/rate_throttler.hpp>
#include <librdkafka/rdkafka.h>
#include <boost/interprocess/allocators/private_node_allocator.hpp>
#include <vector>
#include <utility>
#include <set>
#include <map>
#include <deque>

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_LATOKEN_STP" Class:                                         //
  //=========================================================================//
  // NB:
  // (*) It is derived from "EConnector" itself, not from "EConnector_OrdMgmt",
  //     because for STP/DropCopy purposes,  we do not need full OMC functiona-
  //     lity;
  // (*) At the same time, it provides some basic MDC functionality for LATOKEN
  //     as required in order to value our own Instruments and Assets, but it
  //     is NOT a full-fledged "EConnector_MktData" either:
  //
  class EConnector_LATOKEN_STP final: public EConnector
  {
  private:
    //=======================================================================//
    // Types and Consts:                                                     //
    //=======================================================================//
    // NB: IsKafka=true in HandlerOMC below:
    //
    using  Handler = H2WS::LATOKEN::HandlerOMC<true>;

    //-----------------------------------------------------------------------//
    // Singleton Mgmt:                                                       //
    //-----------------------------------------------------------------------//
    // XXX: Currently, this class is a Singleton (this is due to Kafka logging
    // interaction): Only 1 instance can be created at a time:
    //
    static EConnector_LATOKEN_STP const* s_Singleton;

    //-----------------------------------------------------------------------//
    // Support for LATOKEN Liquidity Stats:                                  //
    //-----------------------------------------------------------------------//
    // For all UserIDs other than LATOKEN internal UserIDs, and for all Passive
    // Orders placed, we determine whether a given Order:
    // (A) would be filled if our pxs were same as BenchMark pxs;
    // (B) was actually filled;
    // (*) the ratio of Count(B)/Count(A) is a measure of LATOKEN "liquidity
    //     efficiency"; in theory, it could even be > 1 ("super-efficient");
    // (*) aggressive orders are eliminated from this statistics as far as we
    //     can (using the info available at placing, as well as the time span
    //     of an Order being live):
    // (*) this stats are accumulated in ShM and flushed to DB:
    //
    struct LiqStats
    {
      // Data Flds:
      ObjName                m_ordID;      // OrderID (NOT TradeID,  as the lat-
      UserID                 m_userID;     //   ter is different across events!)
      bool                   m_isBid;      // Side
      PriceT                 m_px;
      RMQtyA                 m_qty;        // Order Qty in A units, fractnl
      mutable RMQtyA         m_filledQty;
      mutable utxx::time_val m_bmFilledAt; // Would be Filled on BenchMarkExch?
      mutable PriceT         m_bmPx;       // The corresp BenchMarkPx

      // Default Ctor:
      LiqStats()
      : m_ordID        (EmptyObjName),
        m_userID       (0),
        m_isBid        (false),
        m_px           (),
        m_qty          (),
        m_filledQty    (),
        m_bmFilledAt   (),
        m_bmPx         ()
      {}

      // Non-Default Ctor:
      LiqStats
      (
        ObjName const& a_ord_id,
        UserID         a_user_id,
        bool           a_is_bid,
        PriceT         a_px,
        RMQtyA         a_qty
      );

      // Whether this Order would be filled at the BenchMark Exchange?
      bool WouldBeFilled() const;
    };
    //-----------------------------------------------------------------------//
    // ShM-Based Mechanisms for Removal of Duplicate MsgIDs and LiqStats:    //
    //-----------------------------------------------------------------------//
    constexpr static size_t DRMSetSz = 4096;
    constexpr static size_t DRMDEQSz =  128;

    // Allocator for the ShM Sets of "ObjName"s:
    using DRMSetAlloc =
          boost::interprocess::private_node_allocator
          <ObjName, FixedShM::segment_manager, DRMSetSz>;

    // The Set of "ObjName"s itself:
    using DRMSet      = std::set<ObjName, FStrLessT<ObjNameSz>, DRMSetAlloc>;

    // Allocator for DEQ of "ObjName"s:
    using DRMDEQAlloc =
          boost::interprocess::private_node_allocator
          <ObjName, FixedShM::segment_manager, DRMDEQSz>;

    // The DEQ of "ObjNames"s itself:
    using DRMDEQ      = std::deque<ObjName, DRMDEQAlloc>;

    // The theoretical maximum of LA OrderBooks we hold in ShM:
    constexpr static unsigned MaxLABooks = 3072;

    // Estimated ShM Size to hold these data structures: With Contingency:
    constexpr static size_t ShMSz =
      2 * (DRMSetSz   * 6 * sizeof(DRMSet::value_type) +
           DRMDEQSz   *     sizeof(DRMDEQ::value_type) +
           MaxLABooks *     sizeof(OrderBook))         + (1 << 18);

    //-----------------------------------------------------------------------//
    // Technical Stuff:                                                      //
    //-----------------------------------------------------------------------//
    // Producer and Consumer Kafka Topics, for InstrRisks and AssetRisks:
    constexpr   static int   NConsTopics = 4;
    static char const* const s_kafkaConsumerInstrTopicsProd[NConsTopics];
    static char const* const s_kafkaConsumerAssetTopicsProd[NConsTopics];
    static char const* const s_kafkaConsumerInstrTopicsTest[NConsTopics];
    static char const* const s_kafkaConsumerAssetTopicsTest[NConsTopics];

    constexpr   static int   NProdTopics = 4;
    static char const* const s_kafkaProducerInstrTopicsProd[NProdTopics];
    static char const* const s_kafkaProducerAssetTopicsProd[NProdTopics];
    static char const* const s_kafkaProducerInstrTopicsTest[NProdTopics];
    static char const* const s_kafkaProducerAssetTopicsTest[NProdTopics];

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // Config and Run-Time Params:
    bool                                 m_withXTrds;   // Process Self-Trades?
    bool                                 m_noDBMode;    // No Kafka/DB output
    bool                                 m_useLATrades; // L1 OrderBook or Trds
    mutable bool                         m_isActive;    // ...in LA UnRPnL
    mutable bool                         m_stpDynInitMode;
    // Kafka:
    rd_kafka_t*                          m_kafkaC;      // Consumer
    rd_kafka_t*                          m_kafkaP;      // Producer
    rd_kafka_queue_t*                    m_kq;
    int                                  m_kfd;         // FDs for Kafka events:
    int                                  m_rfd;         //   Kafka and Reactor
    int                                  m_topicN;      // For testing mode only
    int                                  m_batchSzC;    // For Consumer only
    rd_kafka_topic_partition_list_t*     m_kpartsC;     // ditto
    mutable long                         m_msgCount;    // (On Recv)
    std::vector<rd_kafka_topic_t*>       m_instrTopics; // Producer Topics:
    std::vector<rd_kafka_topic_t*>       m_assetTopics; //   Instrs and  Assets

    // Duplicates Removal Mechanism in ShM:
    DRMSet*                              m_drmOrdSet;   // Orders  (XXX Not yet)
    DRMSet*                              m_drmTrdSet;   // Trades  (REALLY)
    DRMSet*                              m_drmBalSet;   // Balance (w/ ExchTS)
    DRMDEQ*                              m_drmDEQ;      // Balance (no ExchTS)

    // Buffer for Liquidity Stats (to avoid DB access for "curr" stats which are
    // likely to  change soon). Inner Map:  By OrigExchTS (actually by TradeID);
    // Outer Map: By Instrument:
    mutable std::map<SecDefD const*, std::map<utxx::time_val, LiqStats>>
                                         m_liqStatsMap;
    // XXX: For compatibility with IO::ReadUntilEAgain, we still need an FDInfo
    // which contains the FD and the RddBuff:
    mutable utxx::dynamic_io_buffer      m_rbuff;
    mutable IO::FDInfo                   m_fdInfo;

    // RapidJSON: Reading and Processing of JSON Msgs:
    mutable rapidjson::Reader            m_reader;
    mutable Handler                      m_handler;
    mutable char                         m_sbuff[8192];  // Sending Kafka msgs

    // XXX: MDCs are currently hard-wired, because we need to select the set of
    // required Valuation Instrs from them (in order not to create ALL OrderBo-
    // oks):
    std::vector<SecDefS>                 m_instrsBinance;
    EConnector_H2WS_Binance_MDC<Binance::AssetClassT::Spt>*
                                         m_mdcBinance;
    std::vector<EConnector_MktData*>     m_allMDCs;

    // We also implement a simplistic kind of "LATOKEN MDC" in ShM:
    int                                  m_nLABooks;     // Actual #Books ShM
    OrderBookBase*                       m_laBooksShM;

    // Map:     AssetName => CcyID, used to generate Kafka/JSON output msgs:
    std::map<char const*, unsigned, CStrLessT>
                                         m_ccyIDs;
    // Inverse Map: CcyID => AssetName,  used in processing of Balance msgs:
    std::map<unsigned, char const*>      m_ccyNames;

    // Map:     AssetName => FeeRate,    used in Fees computations:
    std::map<char const*, double,   CStrLessT>
                                         m_feeRates;
    // Decimal Precision for JSON/Kafka output:
    constexpr static int                 DecPrec = 9;

    // Rate Throttlers for outputting MtM msgs to Kafka, for each Instr and
    // Asset. On average, ~1 MtM output per Instr or Asset per 10 sec is OK.
    // Window is 100 sec, with 1 bucket/sec, ie 100 buckets altogether:
    constexpr static int                 MtMThrotWinSec = 100;
    constexpr static int                 MtMThrotBkts  =   1;

    using Throttler =
      utxx::basic_rate_throttler<MtMThrotWinSec, MtMThrotBkts>;

    // Throttlers for all Instrs and Assets:
    std::map<SecID,       Throttler>     m_instrThrots;
    std::map<char const*, Throttler, CStrLessT>
                                         m_assetThrots;
    long                                 m_throtLimit;

  public:
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    // NB: The list of MDCs is used to provide Mark-to-Market valuations for
    // LATOKEN-traded Instruments and Assets, in the decreasing order of pri-
    // orities:
    EConnector_LATOKEN_STP
    (
      EPollReactor*                      a_reactor,
      SecDefsMgr*                        a_sec_defs_mgr,
      RiskMgr*                           a_risk_mgr,
      boost::property_tree::ptree const& a_params
    );

    ~EConnector_LATOKEN_STP()  override;

    //=======================================================================//
    // Start / Stop / Properties:                                            //
    //=======================================================================//
    void Start()          override;
    void Stop ()          override;
    bool IsActive() const override { return m_isActive; }

    // For Kafka logging, we need static access to the Logger:
    static spdlog::logger*  GetLogger()
      { return (s_Singleton != nullptr) ? s_Singleton->GetLogger() : nullptr; }

    //=======================================================================//
    // Call-Backs from RiskMgr:                                              //
    //=======================================================================//
    void InstrRisksUpdate
    (
      InstrRisks const& a_ir,        // State  AFTER  the Trade
      RMQtyA            a_ppos_a,    // Pos(A) BEFORE the Trade
      char const*       a_trade_id,  // Trade ID (NULL or empty on MtM)
      bool              a_is_buy,    // For the UserID's side; irrelev on MtM
      PriceT            a_px_ab,     // ExecPx(A/B)
      RMQtyA            a_qty_a,     // Qty(A) traded   (0 on MtM)
      RMQtyB            a_rgain_b,   // RealisedGain(B) (0 if not closing)
      double            a_b_rfc,     // ValRate (B/RFC)
      double            a_rgain_rfc  // RealisedGain(RFC)
    );

    void AssetRisksUpdate
    (
      AssetRisks const& a_ar,
      char const*       a_trans_id,  // NULL or empty on MtM
      AssetRisks::AssetTransT
                        a_trans_t,
      double            a_qty,       // Always >= 0 (0 for MtM only)
      double            a_eval_rate
    );

    //=======================================================================//
    // Reading and Processing Kafka Msgs:                                    //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetKafkaMsgs": Main Msg Reading and Processing Method:               //
    //-----------------------------------------------------------------------//
    void GetKafkaMsgs();

  private:
    // This class is final, so the following method must finally be implemented:
    void EnsureAbstract()   const override {}

    //-----------------------------------------------------------------------//
    // "KafkaPipeError" Error Handler for the Signalling Interface:          //
    //-----------------------------------------------------------------------//
    void KafkaPipeError
    (
      int             a_bytes_read, // Normally (-1) if there was an error
      int             a_err_code,   // ErrNo
      uint32_t        a_events,     // If from EPoll
      char const*     a_msg         // Non-NULL
    );

    //-----------------------------------------------------------------------//
    // "ParseMsg" (same Parser for Trade and balance Msgs):                  //
    //-----------------------------------------------------------------------//
    void ParseMsg
    (
      bool            a_is_trd,    // true: Trade; false: Balance
      char const*     a_data       // Non-NULL,  0-terminated
    )
    const;

    //-----------------------------------------------------------------------//
    // "Process{Trade|Balance}Msg: Semantic Processing (RiskMgr Incocation): //
    //-----------------------------------------------------------------------//
    // Both methods return "true" on success, "false" on semantic error:
    //
    bool ProcessTradeMsg
    (
      H2WS::LATOKEN::MsgOMC const& a_msg,
      utxx::time_val               a_ts_exch,
      utxx::time_val               a_ts_recv
    )
    const;

    bool ProcessBalanceMsg       // NB: No RecvTS here!
    (
      H2WS::LATOKEN::MsgOMC const& a_msg,
      utxx::time_val               a_ts_exch
    )
    const;

    // Top-level checks whether the Msg is relevant at all:
    // Returns a flag and the ExchTS (the latter is always empty if the flag is
    // "false"):
    std::pair<bool, utxx::time_val> IsNotDuplicateMsg
      (H2WS::LATOKEN::MsgOMC const& a_msg, bool a_is_trd) const;

    //-----------------------------------------------------------------------//
    // "UpdateL1Pxs": Managing LATOKEN L1 OrderBooks:                        //
    //-----------------------------------------------------------------------//
    void UpdateL1Pxs
    (
      SecDefD const&    a_instr,
      PriceT            a_bid_px,
      PriceT            a_ask_px,
      utxx::time_val    a_ts
    )
    const;

    //-----------------------------------------------------------------------//
    // "GetFeeRate": FIXME: Replacement to Fee extraction from Balance Msgs: //
    //-----------------------------------------------------------------------//
    double GetFeeRate(SecDefD const& a_instr) const;

    //=======================================================================//
    // Sending Kafka Msgs, etc:                                              //
    //=======================================================================//
    // Send the msg which was previously formed in "m_sbuff":
    template<bool IsInstr>
    void SendToKafka(UserID a_user_id, int a_len) const;

    // Switch to "steady operational mode":
    void LeaveSTPDynInitMode() const;

    //=======================================================================//
    // "LiqStats" Mgmt:                                                      //
    //=======================================================================//
    // On a Trade or Order:
    void UpdateLiqStats
    (
      SecDefD const&               a_instr,
      H2WS::LATOKEN::MsgOMC const& a_msg,
      utxx::time_val               a_ts_exch     // NB: ExchTS here!
    )
    const;

    // On a BenchMark (RefPx) MktData tick:
    void UpdateLiqStats
    (
      SecDefD const&             a_instr,
      bool                       a_ref_is_bid, // Side of "a_ref_px" below
      PriceT                     a_ref_px,
      utxx::time_val             a_ts_recv     // NB: But RecvTS here because
    )                                          //   the BenchMak Exch is also
    const;                                     //   involved...

    // Outputting the LiqStats to a Log or a DB:
    void OutputLiqStats
    (
      SecDefD  const&            a_instr,
      utxx::time_val             a_ts_orig,
      utxx::time_val             a_ts_exch,
      LiqStats const&            a_ls,
      char const*                a_evt         // Fill|Cxl|Time
    )
    const;
  };
} // End namespace MAQUETTE
