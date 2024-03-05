// vim:ts=2:et
//===========================================================================//
//         "Connectors/H2WS/LATOKEN/EConnector_WS_LATOKEN_MDC.cpp":          //
//                          WS-Based MDC for LATOKEN                         //
//===========================================================================//
#include "Connectors/H2WS/LATOKEN/EConnector_WS_LATOKEN_MDC.h"
#include "Connectors/H2WS/EConnector_WS_MDC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Protocols/H2WS/LATOKEN-OMC.hpp"
#include "Connectors/EConnector_MktData.hpp"
#include "Venues/LATOKEN/Configs_WS.h"
#include "Venues/LATOKEN/SecDefs.h"
#include <utxx/error.hpp>
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
  // "GetSecID": Extract SecID from a JSON Msg:                              //
  //-------------------------------------------------------------------------//
  inline SecID GetSecID(char const* a_msg)
  {
    assert(a_msg != nullptr);
    // Also, "a_msg" is assumed to be 0-terminated...

    char const* sidKey = strstr(a_msg, "\"sec\":");
    return
      (utxx::unlikely(sidKey == nullptr))
      ? 0
      : SecID(atol(sidKey + 6));
  }
}

namespace MAQUETTE
{
  //=========================================================================//
  // "GetWSConfig":                                                          //
  //=========================================================================//
  H2WSConfig const& EConnector_WS_LATOKEN_MDC::GetWSConfig(MQTEnvT a_env)
  {
    auto cit = LATOKEN::Configs_WS_MDC.find(a_env);

    if (utxx::unlikely(cit == LATOKEN::Configs_WS_MDC.cend()))
      throw utxx::badarg_error("EConnector_WS_LATOKEN_MDC: Envalid Env");

    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_LATOKEN_MDC::EConnector_WS_LATOKEN_MDC
  (
    EPollReactor*                       a_reactor,
    SecDefsMgr*                         a_sec_defs_mgr,
    vector<string>              const*  a_only_symbols,   // NULL=UseFullList
    RiskMgr*                            a_risk_mgr,
    boost::property_tree::ptree const&  a_params,
    EConnector_MktData*                 a_primary_mdc     // Normally NULL
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    // NB: As usual, AccountKey is the Connector Instance Name. It may look like
    // {AccountPfx}-WS-LATOKEN-MDC-{Env}:
    //
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "LATOKEN",
      0,                            // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                        // No BusyWait
      a_sec_defs_mgr,
      LATOKEN::SecDefs,
      a_only_symbols,               // Restricting LATOKEN::SecDefs
      a_params.get<bool>("ExplSettlDatesOnly",   false),
      false,                        // Presumable no Tenors in SecIDs
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_MDC":                                                  //
    //-----------------------------------------------------------------------//
    EConnector_WS_MDC<EConnector_WS_LATOKEN_MDC>
    (
      a_primary_mdc,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_LATOKEN_MDC" Itself:                                   //
    //-----------------------------------------------------------------------//
    m_reader (),
    m_handler(m_logger, m_debugLevel)
  {}

  //=========================================================================//
  // "InitSession":                                                          //
  //=========================================================================//
  void EConnector_WS_LATOKEN_MDC::InitSession() const
  {
    // The Path is trivial:
    ProWS::InitWSHandShake("/");
  }

  //=========================================================================//
  // "InitLogOn":                                                            //
  //=========================================================================//
  void EConnector_WS_LATOKEN_MDC::InitLogOn()
  {
    // Generate the subscription JSON obj -- for all configured instrs: It acts
    // as a "LogOn" msg:
    stringstream sp;
    sp << "{\"rqst\":\"sbscr\",\"secs\":";

    bool frst  = true;
    for (SecDefD const* instr: m_usedSecDefs)
    {
      assert(instr != nullptr);
      sp << (frst ? '[' : ',') << instr->m_SecID;
      frst = false;
    }
    sp << "]}";

    // Send it up as a WS frame:
    ProWS::PutTxtData  (sp.str().c_str());
    ProWS::SendTxtFrame();
  }

  //=========================================================================//
  // "ProcessWSMsg":                                                         //
  //=========================================================================//
  // Performs manual JSON parsing and processing of incoming MktData msgs. Ret-
  // urns "true" to continue receiving msgs, "false" to stop:
  //
  bool EConnector_WS_LATOKEN_MDC::ProcessWSMsg
  (
    char const*     a_msg_body,
    int             DEBUG_ONLY  (a_msg_len),
    bool            UNUSED_PARAM(a_last_msg),
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
    assert(a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    //-----------------------------------------------------------------------//
    // Subscription Response?                                                //
    //-----------------------------------------------------------------------//
    if (strncmp(a_msg_body, "{\"rqst\":\"sbscr\",\"secs\":[" , 24) == 0)
    {
      // This is a list of Susbcribed and (maybe) Not Subscribed SecIDs. For
      // the moment, we just check that the latter list is empty:
      constexpr char const* emptyUnable = "; Unable to Subscribe to secs:\"";

      if (utxx::unlikely(strstr(a_msg_body, emptyUnable) == nullptr))
      {
        LOG_ERROR(1,
          "EConnector_WS_LATOKEN_MDC::ProcessWSMsg: Subscription Error: {}, "
          "EXITING...", a_msg_body)
        this->Stop();
        return false;
      }
      // Subscription OK. We interpret it as completion of the LogOn procedure
      // (if not done yet). Declare ourselves to be Active:
      MDCWS::LogOnCompleted(a_ts_recv);
      return true;
    }
    //-----------------------------------------------------------------------//
    // In all other cases, SecID and OrderBook must be available:            //
    //-----------------------------------------------------------------------//
    SecID   secID = GetSecID(a_msg_body);
    OrderBook* ob = EConnector_MktData::FindOrderBook(secID);

    if (utxx::unlikely(secID == 0 || ob == nullptr))
    {
      LOG_WARN(2,
        "EConnector_WS_LATOKEN_MDC::ProcessWSMsg: Missing or Invalid SecID={} "
        "in: {}",  secID, a_msg_body)
      // Always continue:
      return true;
    }
    //-----------------------------------------------------------------------//
    // OrdersBook SnapShot (FOB)?                                            //
    //-----------------------------------------------------------------------//
    char const* fobKey = strstr(a_msg_body, "\"fob\":[");

    if (fobKey != nullptr)
    {
      // Then parse and process the SnapShot msg:
      bool ok = this->ProcessSnapShot(fobKey + 7, ob, a_ts_recv, a_ts_handl);

      if (utxx::unlikely(!ok))
        LOG_WARN(2,
          "EConnector_WS_LATOKEN_MDC::ProcessWSMsg: Invalid SnapShot Msg: {}",
          a_msg_body)
      // Always continue:
      return true;
    }
    //-----------------------------------------------------------------------//
    // OrdersLog:                                                            //
    //-----------------------------------------------------------------------//
    // TODO
    //-----------------------------------------------------------------------//
    // In any other case:
    //-----------------------------------------------------------------------//
    // Log this spurious msg but continue:
    LOG_WARN(2,
      "EConnector_WS_LATOKEN_MDC::ProcessWSMsg: UnExpected Msg: {}", a_msg_body)

    // Finally:
    return true;
  }

  //=========================================================================//
  // "ProcessSnapShot":                                                      //
  //=========================================================================//
  bool EConnector_WS_LATOKEN_MDC::ProcessSnapShot
  (
    char const*    a_fob,
    OrderBook*     a_ob,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_handl
  )
  {
    //-----------------------------------------------------------------------//
    // Checks:                                                               //
    //-----------------------------------------------------------------------//
    assert (a_fob != nullptr && a_ob != nullptr);
    // NB: "a_fob" is assumed to be 0-terminated!

    if (utxx::unlikely(*a_fob != '{'))
      return false;    // MalFormatted Msg

    // If the OrderBook has already been initialised, SnapShot is misplaced:
    if (utxx::unlikely(a_ob->IsInitialised()))
    {
      LOG_ERROR(1,
        "EConnector_WS_LATOKEN_MDC::ProcessSnapShot: OrderBook={} Already Init"
        "ialised", a_ob->GetInstr().m_FullName.data())
      return false;   // The CallER would still continue...
    }

    //-----------------------------------------------------------------------//
    // Parse and Process the Orders:                                         //
    //-----------------------------------------------------------------------//
    // They will be accumuated in the ascending order of TimeStamps (OrderIDs)
    // in the following map. XXX:
    // (*) A dynamic map is badly inefficient, but is required at the init time
    //     only, so this is acceptable;
    // (*) This map is required because LATOKEN SnapShots are NOT guaranteed to
    //     be collision-free,   and it is also NOT guaranteed that the SnapShot
    //     Orders are delivered in the ascending order of their TimeStamps;  ie
    //     the sole purpose of this map is to allows us to resulve Bid-Ask col-
    //     lisions correctly:
    //
    map<OrderID, H2WS::LATOKEN::MsgOMC> snapShotOrds;

    char const* currOrd = a_fob;
    while (true)
    {
      // NB: The JSON parser will need to parse each Order separately, so find
      // the and of the curr Order:
      char const* ordEnd = strchr(currOrd, '}');
      if (utxx::unlikely(ordEnd == nullptr))
        return false;  // MalFormatted Msg
      ++ordEnd;

      // "ordEnd" now points to the next char after this Order, which is a ','
      // or ']':
      bool lastOrd = (*ordEnd == ']');
      if (utxx::unlikely(!lastOrd && *ordEnd != ','))
        return false;  // MalFormatted Msg

      // 0-terminate the curr Order:
      char was = *ordEnd;
      *(const_cast<char*>(ordEnd)) = '\0';

      // Do this Order:
      bool ok = this->ProcessOrder(currOrd, a_ob, &snapShotOrds);

      // Restore the orig char for logging purposes:
      *(const_cast<char*>(ordEnd)) = was;

      if (utxx::unlikely(!ok))
      {
        LOG_ERROR(2,
          "Connector_WS_LATOKEN_MDC::ProcessSnapShot: Invalid Order in Snap"
          "Shot: {}", a_fob)
        // XXX: Still try to continue...
      }
      // Done, or Next Order?
      if (lastOrd)
        break;

      currOrd = ordEnd + 1;
    }
    //-----------------------------------------------------------------------//
    // Install the Accumulated Orders:                                       //
    //-----------------------------------------------------------------------//
    utxx::time_val procTS = utxx::now_utc();

    for (auto const& kv: snapShotOrds)
    {
      OrderID                      ordID = kv.first;
      H2WS::LATOKEN::MsgOMC const& msg   = kv.second;
      utxx::usecs                  exchUS(ordID);
      utxx::time_val               exchTS(exchUS);

      // Then the OrderAction is supposed to be "New": XXX: Strangely, the Px
      // is "ExecPx" but Qty is "OpenQty"!!!
      PriceT px  = msg.m_execPx ;
      QtyN   qty = msg.m_openQty;

      if (utxx::unlikely
         (msg.m_ordStatus != FIX::OrdStatusT::New ||
         (msg.m_side      != FIX::SideT::Buy      &&
          msg.m_side      != FIX::SideT::Sell)    || !IsPos(px) || !IsPos(qty)))
      {
        LOG_WARN(1,
          "EConnector_WS_LATOKEN_MDC::ProcessSnapShot: Invalid SnapShot Msg: "
          "OrdID={}, Side={}, Px={}, Qty={}",
          ordID, int(msg.m_side), double(px), double(qty))
        // Skip it and try to continue:
        continue;
      }
      // Check if this Order would cause Bid-Ask collision (as *may* happen in
      // LATOKEN, this is a normal "feature"). XXX: The following simple check
      // may still not be enough, as we do not fully account for the impact ma-
      // de by that aggressive order on the opposite OrderBook side:
      bool isBid = (msg.m_side == FIX::SideT::Buy);

      if (utxx::unlikely
         (( isBid && px >= a_ob->GetBestAskPx()) ||
          (!isBid && px <= a_ob->GetBestBidPx()) ))
      {
        LOG_WARN(2,
          "EConnector_WS_LATOKEN_MDC::ProcessSnapShot: OrderID={}, Px={}, "
          "Side={}: Collision, skipped",
          ordID, double(px), (isBid ? "Bid" : "Ask"))
        continue;
      }
      constexpr bool IsSnapShot = true;
      auto res =
        EConnector_MktData::ApplyOrderUpdate
          <IsSnapShot,       WithIncrUpdates, StaticInitMode,
           ChangeIsPartFill, NewEncdAsChange, ChangeEncdAsNew,
           WithRelaxedOBs,   IsFullAmt,       IsSparse,      QT,  QR>
          (
            ordID,
            FIX::MDUpdateActionT::New,
            a_ob,
            isBid ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer,
            msg.m_execPx,
            msg.m_execQty,
            0,
            0
          );
      OrderBook::UpdateEffectT upd   = get<0>(res);
      OrderBook::UpdatedSidesT sides = get<1>(res);

      // NB: UpdateEffect=NONE does not produce a warning (it simply means a
      // far-away Px which we cannot accommodate), but an ERROR does:
      if (utxx::unlikely(upd == OrderBook::UpdateEffectT::ERROR))
        LOG_WARN(2,
          "EConnector_WS_LATOKEN_MDC::ProcessSnapShot: OrderID={}, Px={}, "
          "Side={}: OrderBook UpdateEffect=ERROR",
          ordID, double(px), (isBid ? "Bid" : "Ask"))

      (void) EConnector_MktData::AddToUpdatedList
        (a_ob, upd, sides, exchTS, a_ts_recv, a_ts_handl, procTS);
    }
    //-----------------------------------------------------------------------//
    // SnapShot Done:                                                        //
    //-----------------------------------------------------------------------//
    // Mark the OrderBook as Initialised:
    a_ob->SetInitialised();
    return true;
  }

  //=========================================================================//
  // "ProcessOrder":                                                         //
  //=========================================================================//
  // Returns the ptr to the char beyond the last one parsed. NB: Here we re-use
  // the OMC/STP JSON parser because it turns out that the MDC Order  is a sub-
  // set of flds of OMC/STP:
  //
  inline bool EConnector_WS_LATOKEN_MDC::ProcessOrder
  (
    char const*                          a_order,
    OrderBook*                           DEBUG_ONLY(a_ob),
    map<OrderID, H2WS::LATOKEN::MsgOMC>* a_snap_shot_ords
  )
  {
    assert (a_order != nullptr && a_ob != nullptr && *a_order == '{');
    // NB: "a_order" is assumed to be 0-terminated
    //-----------------------------------------------------------------------//
    // Parsing:                                                              //
    //-----------------------------------------------------------------------//
    // Reset the Handler each time before processing (XXX there is a slight
    // over-head for doing that, but it is required for safety):
    m_handler.Reset();
    rapidjson::StringStream    js(a_order);
    bool parseOK = m_reader.Parse(js, m_handler);

    if (utxx::unlikely(!parseOK))
    {
      // Parsing error encountered: Log it but continue:
      size_t                errOff = m_reader.GetErrorOffset();
      rapidjson::ParseErrorCode ec = m_reader.GetParseErrorCode();

      LOG_ERROR(2,
        "EConnector_WS_LATOKEN_MDC::ProcessOrder: {}: PARSE ERROR: {} at "
        "offset {}", a_order, rapidjson::GetParseError_En(ec), errOff)
      return false;
    }
    //-----------------------------------------------------------------------//
    // Processing:                                                           //
    //-----------------------------------------------------------------------//
    H2WS::LATOKEN::MsgOMC const& msg = m_handler.GetMsg();

    // Construct the numerical OrderID from the ExchTS (microsecond-level); XXX
    // we *trust* that there will be no OrderID collisions:
    //
    utxx::time_val exchTS = H2WS::LATOKEN::MkTSFromMsgID(msg.m_ordID.data());
    OrderID        ordID  = OrderID(exchTS.microseconds());

    if (utxx::unlikely(exchTS.empty()))
    {
      LOG_ERROR(2,
        "EConnector_WS_LATOKEN_MDC::ProcessOrder: Invalid OrderID in: {}",
        a_order)
      return false;
    }
    //-----------------------------------------------------------------------//
    // SnapShot (FOB) Order?                                                 //
    //-----------------------------------------------------------------------//
    if (a_snap_shot_ords != nullptr)
    {
      // This Order is a part of a SnapShot, so put it into the accumulator:
      auto insRes = a_snap_shot_ords->insert(make_pair(ordID, msg));

      if (utxx::unlikely(!insRes.second))
      {
        // We can only log (and skip) this conflicting ID:
        LOG_ERROR(2,
          "EConnector_WS_LATOKEN_MDC::ProcessOrder: Duplicate OrderID in a "
          "SnapShot: {}", ordID)
        return false;
      }
      // Nothing else to do for a SnapShot Order:
      return true;
    }
    //-----------------------------------------------------------------------//
    // Generic Case: "Stand-Alone" (FOL) Order:                              //
    //-----------------------------------------------------------------------//
    // TODO...
    // All Done:
    return true;
  }
} // End namespace MAQUETTE
