// vim:ts=2:et
//===========================================================================//
//         "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.cpp":        //
//                          WS-Based OMC for BitFinex                        //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include "Connectors/H2WS/BitFinex/EConnector_WS_BitFinex_OMC.h"
#include "Connectors/EConnector_OrdMgmt.hpp"
#include "Connectors/H2WS/EConnector_WS_OMC.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"
#include "Venues/BitFinex/Configs_WS.h"
#include "Venues/BitFinex/SecDefs.h"
#include <cstring>
#include <utxx/compiler_hints.hpp>
#include <utxx/convert.hpp>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>
#include <gnutls/crypto.h>

using namespace std;

namespace
{
  using namespace MAQUETTE;

  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "SkipNFlds": Skip "a_n" comma-separated fields:                         //
  //-------------------------------------------------------------------------//
  template<unsigned N>
  inline char const* SkipNFlds(char const* a_curr)
  {
    assert(a_curr != nullptr);
    // XXX: The loop should be unrolled by the optimizer:
    for (unsigned i = 0; i < N; ++i)
    {
      a_curr = strchr(a_curr, ',');
      // NB: The following is NOT really an error:
      if (utxx::unlikely(a_curr == nullptr))
        throw utxx::runtime_error
              ("EConnector_WS_BitFinex_OMC: SkipNFlds: Invalid Fmt");
      ++a_curr;
    }
    return a_curr;
  }

  //-------------------------------------------------------------------------//
  // "GetSideFromQty":                                                       //
  //-------------------------------------------------------------------------//
  inline FIX::SideT GetSideFromQty(double a_trade_qty)
  {
    return
      (a_trade_qty > 0)
      ? FIX::SideT::Buy
      : (a_trade_qty < 0) ? FIX::SideT::Sell : FIX::SideT::UNDEFINED;
  }
}

namespace MAQUETTE
{
  //=========================================================================//
  // Config and Account Mgmt:                                                //
  //=========================================================================//
  //=========================================================================//
  // "GetAllSecDefs":                                                        //
  //=========================================================================//
  std::vector<SecDefS> const* EConnector_WS_BitFinex_OMC::GetSecDefs()
    { return &BitFinex::SecDefs; }

  //-------------------------------------------------------------------------//
  // "GetWSConfig":                                                          //
  //-------------------------------------------------------------------------//
  H2WSConfig const& EConnector_WS_BitFinex_OMC::GetWSConfig(MQTEnvT a_env)
  {
    auto   cit =  BitFinex::Configs_WS_OMC.find(a_env);
    assert(cit != BitFinex::Configs_WS_OMC.cend());
    return cit->second;
  }

  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  EConnector_WS_BitFinex_OMC::EConnector_WS_BitFinex_OMC
  (
    EPollReactor*                      a_reactor,
    SecDefsMgr*                        a_sec_defs_mgr,
    vector<string>              const* a_only_symbols,
    RiskMgr*                           a_risk_mgr,
    boost::property_tree::ptree const& a_params
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string>("AccountKey"),
      "BitFinex",
      0,                                   // No extra ShM data as yet...
      a_reactor,
      false,                               // No BusyWait
      a_sec_defs_mgr,
      BitFinex::SecDefs,
      a_only_symbols,                      // Restricting BitFinex::SecDefs
      a_params.get<bool>("ExplSettlDatesOnly",   false),
      false,                               // Presumably no Tenors in SecIDs
      a_risk_mgr,
      a_params,
      QT,
      std::is_floating_point_v<QR>
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_WS_OMC":                                                  //
    //-----------------------------------------------------------------------//
    // NB: As usual, AccountKey is the Connector Instance Name. It may look
    // like {AccountPfx}-WS-BitFinex-OMC-{Env}:
    //
    OMCWS        (a_params),

    //-----------------------------------------------------------------------//
    // "EConnector_WS_BitFinex_OMC":                                         //
    //-----------------------------------------------------------------------//
    m_currDate   (GetCurrDate()),
    m_currDateStr(),
    m_tmpBuff    ()
  {}

  //=========================================================================//
  // Dtor is Trivial:                                                        //
  //=========================================================================//
  EConnector_WS_BitFinex_OMC::~EConnector_WS_BitFinex_OMC() noexcept {}

  //=========================================================================//
  // "InitWSSession":                                                        //
  //=========================================================================//
  void EConnector_WS_BitFinex_OMC::InitWSSession() const
  {
    // Fill in "m_currDateStr" with the CurrDate in the format "YYYY-MM-DD".
    // XXX: This means that MUST restart the OMC at the date change (UTC):
    m_currDate  = GetCurrDate();
    char*  curr = m_currDateStr;

    (void) utxx::itoa_right<int, 4, char>(curr, GetCurrYear(),  '0');
    curr += 4;
    *curr = '-';
    ++curr;
    (void) utxx::itoa_right<int, 2, char>(curr, GetCurrMonth(), '0');
    curr += 2;
    *curr = '-';
    ++curr;
    (void) utxx::itoa_right<int, 2, char>(curr, GetCurrDay(),   '0');
    curr += 2;
    *curr = '\0';

    // Clear "m_tmpBuff" just for extra safety:
    memset(m_tmpBuff, '\0', sizeof(m_tmpBuff));

    // Proceed with the HTTP/WS HandShake using the following Path:
    LOG_INFO(2,
      "EConnector_WS_BitFinex_OMC::InitWSSession: Running InitWSHandShake")

    ProWS::InitWSHandShake("/ws/2");
  }

  //=========================================================================//
  // "InitWSLogOn":                                                          //
  //=========================================================================//
  void EConnector_WS_BitFinex_OMC::InitWSLogOn() const
  {
    memset(m_tmpBuff, 0, 100);

    // AUTHENTICATE!
    char* data = m_outBuff + OutBuffDataOff;
    char* curr = data;

    // NB: DMS=4 is CancelOnDisconnect:
    curr = stpcpy(curr, "{\"event\":\"auth\",\"dms\":4,\"apiKey\":\"");
    curr = stpcpy(curr, m_account.m_APIKey.data());

    curr = stpcpy(curr, "\",\"authNonce\":\"");
    char authNonce[64];
    (void)utxx::itoa_left(authNonce, utxx::now_utc().microseconds());
    curr = stpcpy(curr, authNonce);

    char authPayload[64];
    curr = stpcpy(curr, "\",\"authPayload\":\"");
    (void) strcpy(authPayload, "AUTH");
    (void) strcpy(authPayload + 4, authNonce);
    curr = stpcpy(curr, authPayload);

    // Compute the signature of "authPayload" and put it (eventually) at the
    // end of "curr":
    curr = stpcpy(curr, "\",\"authSig\":\"");
    assert(gnutls_hmac_get_len(GNUTLS_MAC_SHA384) == 48);
    unsigned char authSHA384[48];

    DEBUG_ONLY(int ok =)
      gnutls_hmac_fast(GNUTLS_MAC_SHA384,
                       m_account.m_APISecret.data(),
                       m_account.m_APISecret.size(),
                       authPayload, strlen(authPayload), authSHA384);
    assert(ok == 0);

    gnutls_datum_t authHMAC{authSHA384, uint8_t(sizeof(authSHA384))};
    size_t signHexSz = 97;  // Including the terminating '\0'

    DEBUG_ONLY(ok =) gnutls_hex_encode(&authHMAC, curr, &signHexSz);
    assert(ok == 0 && signHexSz > 0);
    curr += (signHexSz-1);
    assert(*curr == '\0');

    // Finally:
    curr = stpcpy(curr, "\",\"calc\":1,\"filter\":[\"trading\"]}");
    LOG_INFO(2,
      "EConnector_WS_BitFinex_OMC::InitWSLogOn: Sending Authentication:\n{}",
      m_tmpBuff)

    // The data are already in the "m_outBuff", so simply wrap them up intp a
    // a WS frame, and send it out:
    ProWS::WrapTxtData (int(curr - data));
    CHECK_ONLY(OMCWS::LogMsg<true>(data);)      // IsSend=true
    (void) ProWS::SendTxtFrame();
  }

  //========================================================================//
  // "ProcessWSMsg":                                                        //
  //========================================================================//
  // Returns "true" on success, "false" on error:
  //
  bool EConnector_WS_BitFinex_OMC::ProcessWSMsg
  (
    char const*    a_msg_body,
    int            a_msg_len,
    bool           UNUSED_PARAM(a_last_msg),
    utxx::time_val a_ts_recv,
    utxx::time_val UNUSED_PARAM(a_ts_handl)
  )
  {
    // NB: "a_msg_body" should be non-empty, and 0-terminated by the Caller:
    assert(a_msg_body != nullptr && a_msg_len > 0 &&
           a_msg_body[a_msg_len] == '\0');

    //-----------------------------------------------------------------------//
    // Get the Msg:                                                          //
    //-----------------------------------------------------------------------//
    // XXX: We currently assume that there is only one msg "[...]" or "{...}"
    // per a WS frame:
    double      px           = NaN<double>; // OrigPx  or ReplPx, NOT TradePx
    double      newLeavesQty = NaN<double>;
    double      newTotalQty  = NaN<double>;
    double      sigTotalQty  = NaN<double>;
    char const* curr         = a_msg_body;
    char*       after        = nullptr;
    char*       msgEnd       = const_cast<char*>(a_msg_body + a_msg_len);
    assert(curr < msgEnd);

    try
    {
      //--------------------------------------------------------------------//
      // Check if it is Data Array ("[...]") or a one-off Event ("{...}"):  //
      // Turns out there can be several messages in here                    //
      //--------------------------------------------------------------------//
      if (utxx::likely(a_msg_body[0] == '[' && a_msg_body[a_msg_len-1] == ']'))
      {
        while (curr != nullptr) {
          //------------------------------------------------------------------//
          // This is a Data Array:                                            //
          //------------------------------------------------------------------//
          curr = SkipNFlds<1>(curr);   // Skip ChanID and, perhaps "]["

          // Either "on", "n", "ou" or "oc" (for now), followed by a nested msg:
          bool msgN  = strncmp(curr,  "\"n\",[", 5) == 0;
          bool msgON = strncmp(curr, "\"on\",[", 6) == 0;
          bool msgOU = strncmp(curr, "\"ou\",[", 6) == 0;
          bool msgOC = strncmp(curr, "\"oc\",[", 6) == 0;

          if (utxx::unlikely(!(msgN || msgON || msgOC || msgOU)))
          {
            // XXX: Other msg types are currently ignored,
            // But the following msgs might be qualified:
            curr = strstr(curr, "][");
            continue;
          }

          // For "msgN", get the ReqKind to which this Resp was made, and move
          // to the inner msg:
          bool ONReq = false;
          bool OUReq = false;
          bool OCReq = false;
          if (msgN)
          {
            curr  = SkipNFlds<1>(curr + 5);
            ONReq = strncmp(curr, "\"on-req\"", 8);
            OUReq = strncmp(curr, "\"ou-req\"", 8);
            OCReq = strncmp(curr, "\"oc-req\"", 8);

            curr  = strchr(curr, '[') + 1;
          } else
            curr += 6;
          // We are now past the opening '['...
          //-----------------------------------------------------------------//
          // Get the Common Flds:                                            //
          //-----------------------------------------------------------------//
          // Use a different pointer here to ensure curr != nullptr
          CHECK_ONLY(char const* checkCurr = nullptr;)

          // ExchID:
          OrderID exchID = 0;
          CHECK_ONLY(checkCurr =)
            utxx::fast_atoi<OrderID, false>(curr, msgEnd, exchID);
          CHECK_ONLY
          (
            if (utxx::unlikely(checkCurr == nullptr || *checkCurr != ','))
              LOG_WARN(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: No ExchID in Msg={}"
                ", Curr={}", a_msg_body, curr)
          )

          // Skip 1 Fld and get ClientOrderID. NB: This is the UNCHANGED ClOrdID
          // of this Order -- so it is the AOSID, not ReqID:
          curr          = SkipNFlds<2>(curr);
          OrderID aosID = 0;
          CHECK_ONLY(checkCurr =)
            utxx::fast_atoi<OrderID, false>(curr, msgEnd, aosID);
          CHECK_ONLY
          (
            if (utxx::unlikely(checkCurr == nullptr || *checkCurr != ','))
              LOG_WARN(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: No ClientOrdID in "
                "Msg={}, Curr={}", a_msg_body, curr)
          )

          // Get CreateTS:
          curr          = SkipNFlds<2>(curr);  // Skip Symbol
          uint64_t msTs = 0;
          CHECK_ONLY(checkCurr =)
            utxx::fast_atoi<uint64_t, false>(curr, msgEnd, msTs);
          CHECK_ONLY
          (
            if (utxx::unlikely(checkCurr == nullptr || *checkCurr != ','))
              LOG_WARN(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: No TS in Msg={}, "
                "Curr={}", a_msg_body, curr)
          )

          // Get OrderQty (ie LeavesQty) and OrigQty (ie TotalQty). NB: They may
          // be negative for Sell, so apply Abs():
          curr         = SkipNFlds<2>(curr);
          newLeavesQty = Abs(strtod(curr, &after));
          CHECK_ONLY
          (
            if (utxx::unlikely(after == curr))
              LOG_WARN(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: No LeavesQty in "
                "Msg={}, Curr={}", a_msg_body, curr)
          )
          curr         = SkipNFlds<1>(curr);
          sigTotalQty  = strtod(curr, &after);
          newTotalQty  = Abs(sigTotalQty);
          CHECK_ONLY
          (
            if (utxx::unlikely(after == curr))
              LOG_WARN(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: No OrigQty in Msg="
                "{}, Curr={}", a_msg_body, curr)
          )

          // Obviously, we should have
          // newTotalQty > 0 and newLeavesQty <= newTotalQty. If this is not the
          // case, produce an error but still continue:
          CHECK_ONLY
          (
            if (utxx::unlikely(newTotalQty == 0 || newLeavesQty > newTotalQty))
              LOG_ERROR(1,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Inconsistent Qtys: "
                "NewLeavesQty={}, NewTotalQty={}", newLeavesQty, newTotalQty)
          )
          // Get the TimeInForce TimeStamp -- it contains an encoded real
          // ClOrdID (ie ReqID). For Cancel, it would be missing (0), which is
          // OK:
          curr         = SkipNFlds<3>(curr);
          OrderID clID = 0;

          long tifTS = 0;
          curr =  utxx::fast_atoi<long, false>(curr, msgEnd, tifTS);
          CHECK_ONLY
          (
            // XXX: Produce an error if TiF is missing,   but only if it is a
            // msgN; in all other cases, missing TiF means that the order has
            // been Filled or Cancelled already:
            if (utxx::unlikely(!msgN && (curr == nullptr || *curr != ',')))
            {
              LOG_ERROR(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: No TiF in Msg={}, "
                "Curr={}", a_msg_body, curr)
              return false;
            }
          )
          ++curr;

          CHECK_ONLY(
            if (tifTS <= 0)
              LOG_ERROR(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Negative TifTS, "
                "Msg={}", a_msg_body)
          )

          if (utxx::unlikely(tifTS <= 0)) {
            LOG_WARN(2, "EConnector_WS_BitFinex_OMC::ProcessWSMsg: "
                "Unknown ClID: Probably null-msg: a failed oc-/ou-req, "
                "Msg={}", a_msg_body)
            curr = strstr(curr, "][");
            continue;
            // return true;
          }
          utxx::msecs    tifMS(tifTS);
          utxx::time_val tif  (tifMS);

          // ClOrdID is the delta (in milliseconds) between "tif" and 23:50:00
          // UTC of today! If, for some reason, that date has already passed,
          // we would get a negative result -- increment it by a whole number
          // of days:
          long  diff = (tif - GetCurrDate()).milliseconds() - 85'800'000;
          while (utxx::unlikely(diff < 0))
            diff += 86'400'000;

          clID = OrderID(diff);
          curr = SkipNFlds<2>(curr);

          // Get Order Status Field:
          char const*  stat   = curr;
          bool statusActive   = strncmp(stat, "\"ACTIVE\"",     8) == 0;
          bool statusCanceled = strncmp(stat, "\"CANCELED",     9) == 0
                                ||  strncmp(stat, "\"POSTONLY", 9) == 0;
          bool statusExecuted = strncmp(stat, "\"EXECUTED",     9) == 0;
          bool statusPartFill = strncmp(stat, "\"PARTIALLY",   10) == 0;

          //-----------------------------------------------------------------//
          // Reject or Ack?                                                  //
          //-----------------------------------------------------------------//
          if (msgN)
          {
            // XXX: The following search can be long and costly!
            bool isSuccess = strstr(curr, "\"SUCCESS\"") != nullptr;

            if (utxx::unlikely(!isSuccess))
            {
              if (ONReq)
                // A New Order has failed -- so we know it is non-existent:
                (void) EConnector_OrdMgmt::OrderRejected<QT, QR>
                (
                  this, this, 0, clID, aosID, FuzzyBool::True, 0, nullptr,
                  utxx::time_val(utxx::msecs(msTs)), a_ts_recv
                );
              else
              if (OUReq || OCReq)
              {
                // FIXME: Is there any difference between these cases? It dep-
                // ends on whether PartFilled orders can be Modified.  If not,
                // then failure of Modify  can be due to  both  PartFill  and
                // (Cancel or Fill = NonExistent order),    ie it does NOT by
                // itself imply a NonExistent order.  Since PartFilled orders
                // ARE actually modifyable in BitFinex, then either Cancale or
                // Update fails, this implies a NonExistent (Filled or Cancel-
                // led) order:
                ExchOrdID exchOrdID(exchID);

                (void) EConnector_OrdMgmt::OrderCancelOrReplaceRejected<QT, QR>
                (
                  this,  this,
                  clID,  0, aosID, &exchOrdID, FuzzyBool::UNDEFINED,
                  FuzzyBool::True, 0, nullptr,
                  utxx::time_val(utxx::msecs(msTs)),      a_ts_recv
                );
              }
              else
                // This is strange:
                LOG_WARN(2,
                  "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Spurious Msg: {}: "
                  "No ReqKind?", a_msg_body)
            }
            // TODO: If SUCCESS is found, may need to do OrderAcknowledged"
            // call. For now, we are done in any case...
          }
          else
          {
            // Get the Orig or Replaced Px (incl the Px at which the order has
            // been Cancelled): Skipping 3 flds incl the above Status:
            // Find comma+quote, since statuses can be of a comma-sep'd list
            // form:
            curr = strstr(curr, "\",") + 2;
            curr = SkipNFlds<2>(curr);

            // XXX: What if it was a MktOrder w/o a specified Px (they are NOT
            // supported currently)? Then the Px must remain NaN, not 0, so:
            if (*curr != ',')
            {
              px   = strtod(curr, &after);
              curr = after;
            }
            ++curr;

            //----------------------------------------------------------------//
            // REPLACED?                                                      //
            //----------------------------------------------------------------//
            if (msgOU && statusActive)
            {
              CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "REPLACED");)

              EConnector_OrdMgmt::OrderReplaced<QT, QR>
              (
                this, this, clID,   0,     aosID,   ExchOrdID(exchID),
                ExchOrdID(exchID),  ExchOrdID(0),   PriceT(px),
                QtyN(newLeavesQty), utxx::time_val(utxx::msecs(msTs)),
                a_ts_recv
              );
            }
            else
            //---------------------------------------------------------------//
            // Complete Fill?                                                //
            //---------------------------------------------------------------//
            if (statusExecuted)
            {
              // In this case, we should expect: Executed==OC:
              CHECK_ONLY
              (
                if (utxx::unlikely((statusExecuted != msgOC)))
                  LOG_WARN(2,
                    "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Inconsistency "
                    "in Msg={}: Executed={}, OC={}",
                    a_msg_body, statusExecuted, msgOC)
              )
              // Parse the Px/Qty of the Trade (as opposed to the Px/Qty of the
              // original Req) -- they are encoded in Status:
              double tradePx  = NaN<double>;
              double tradeQty = NaN<double>;

              stat = strstr(stat, " @ ");
              CHECK_ONLY
              (
                if (utxx::unlikely(stat == nullptr))
                {
                  LOG_ERROR(2,
                    "EConnector_WS_BitFinex_OMC::ProcessWSMsg: No TradePx/Qty "
                    "in Msg={}, Curr={}", a_msg_body, curr)
                  return false;
                }
              )
              tradePx  = strtod(stat + 3, &after);
              stat     = after;
              assert(*stat == '(');

              tradeQty = strtod(stat + 1, &after);
              stat     = after;

              FIX::SideT ourSide = GetSideFromQty(tradeQty);
              tradeQty = Abs(tradeQty);

              CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "EXECUTED");)

              (void) EConnector_OrdMgmt::OrderTraded<QT, QR, QT, QR>
              (
                this,            this,
                nullptr,         clID,            aosID,    nullptr,  ourSide,
                FIX::SideT::UNDEFINED,            ExchOrdID(exchID),
                ExchOrdID(0),    ExchOrdID(0),    PriceT(px),
                PriceT(tradePx), QtyN(tradeQty),  QtyN::Invalid(),
                QtyN::Invalid(), QtyN::Invalid(), 0,
                FuzzyBool::True, utxx::time_val(utxx::msecs(msTs)),  a_ts_recv
              );
            }
            else
            //---------------------------------------------------------------//
            // PartFill? (May actually be a Replace!!!)                      //
            //---------------------------------------------------------------//
            if (statusPartFill)
            {
              // In this case, we should expect:
              // PartFill==OU:
              CHECK_ONLY
              (
                if (utxx::unlikely
                  ((statusPartFill != msgOU && statusPartFill != msgON)))
                  LOG_WARN(2,
                    "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Inconsistency "
                    "in Msg={}: PartFill={}, OU={}, ON={}",
                    a_msg_body, statusPartFill, msgOU, msgON)
              )

              // So: Replaced or Traded?
              // If newTotalQty - newLeavesQty == prevCumQty, than it's Replaced
              // order with a STATUS left over from previous operations. NB: For
              // reliability, we should compare CumQtys, not LeavesQtys, and ta-
              // ke CumQty from AOS, not from Req12,     as there may be complex
              // combined effects of PartFills / Replacements here (???):
              //
              constexpr unsigned Mask =
                unsigned(Req12::KindT::New) | unsigned(Req12::KindT::Modify);

              Req12* req = EConnector_OrdMgmt::GetReq12<Mask, true>
                 (clID, aosID, PriceT(px), QtyN(newLeavesQty),
                  "EConnector_WS_BitFinex_OMC::ProcessWSMsg");
              assert(req != nullptr);

              AOS   const* aos = req->m_aos;
              assert(aos != nullptr);

              QtyN   prevCumQty = aos->GetCumFilledQty<QT,QR>();
              assert(!IsNeg(prevCumQty));
              QtyN   newCumQty(newTotalQty - newLeavesQty);

              if (newCumQty <= prevCumQty)
              {
                // Thus, "CumQty" has not increased as a result of this update,
                // so it is Replacement. XXX: Produce a warning if it has actu-
                // ally decreased:
                CHECK_ONLY
                (
                  OMCWS::LogMsg<false>(a_msg_body, "REPLACED");
                  if (utxx::unlikely(newCumQty < prevCumQty))
                    LOG_WARN(2,
                      "EConnector_WS_BitFinex_OMC::ProcesMsg: ReqID={}, AOSID="
                      "{}: Inconsistency: PrevCumQty={}, NewCumQty={}",
                      req->m_id, aos->m_id, QR(prevCumQty), QR(newCumQty))
                )
                EConnector_OrdMgmt::OrderReplaced<QT, QR>
                (
                  this, this,  clID,  0,     aosID,  ExchOrdID(exchID),
                  ExchOrdID(exchID),  ExchOrdID(0),  PriceT(px),
                  QtyN(newLeavesQty), utxx::time_val(utxx::msecs(msTs)),
                  a_ts_recv
                );
                // return true;
                curr = strstr(a_msg_body, "][");
                continue;
              }
              // Otherwise: Qtys balance has changed, so Traded:
              FIX::SideT ourSide = GetSideFromQty(sigTotalQty);

              CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "PART-FILLED");)
              // NB: CompleteFill=False:
              (void) EConnector_OrdMgmt::OrderTraded<QT, QR, QT, QR>
              (
                this,            this,
                nullptr,         clID,        aosID,        req, ourSide,
                FIX::SideT::UNDEFINED,        ExchOrdID(exchID), ExchOrdID(0),
                ExchOrdID(0),    PriceT(px),  PriceT(px),
                QtyN(newCumQty - prevCumQty), QtyN::Invalid(),
                QtyN::Invalid(),              QtyN::Invalid(),              0,
                FuzzyBool::False,             utxx::time_val(utxx::msecs(msTs)),
                a_ts_recv
              );
            }
            else
            //---------------------------------------------------------------//
            // Order Cancelled?                                              //
            //---------------------------------------------------------------//
            if (msgOC && statusCanceled)
            {
              // NB: CxlID is not available, since Cancel Reqs are sent w/o TiF,
              // so no way to embed ReqID:
              //
              CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "CANCELED");)

              EConnector_OrdMgmt::OrderCancelled<QT, QR>
              (
                this,     this,
                0,  clID, aosID, ExchOrdID(exchID),   ExchOrdID(0), PriceT(),
                QtyN::Invalid(), utxx::time_val(utxx::msecs(msTs)), a_ts_recv
              );
            }
            else
            //---------------------------------------------------------------//
            // New Order Confirmation?                                       //
            //---------------------------------------------------------------//
            if (msgON)
            {
              CHECK_ONLY(OMCWS::LogMsg<false>(a_msg_body, "CONFIRMED");)

              EConnector_OrdMgmt::OrderConfirmedNew<QT, QR>
              (
                this,   this, clID,       aosID,   ExchOrdID(exchID),
                ExchOrdID(0), PriceT(px), QtyN(newLeavesQty),
                utxx::time_val(utxx::msecs(msTs)), a_ts_recv
              );
            }
            else
            //---------------------------------------------------------------//
            // Spurious / UnExpected Condition:                              //
            //---------------------------------------------------------------//
            {
              LOG_WARN(2,
                "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Spurious Msg: {}: "
                "ON={}, OU={}, OC={}, Active={}, Cancelled={}, Executed={}, "
                "PartFill={}",
                a_msg_body, msgON, msgOU, msgOC, statusActive, statusCanceled,
                statusExecuted,   statusPartFill)
            }
          }
          curr = strstr(curr, "][");
        }
      }
      else
      {
        //-------------------------------------------------------------------//
        // Some Event:                                                       //
        //-------------------------------------------------------------------//
        assert(a_msg_body[0] == '{' && a_msg_body[a_msg_len-1] == '}' &&
               curr == a_msg_body);

        // What kind of Event is it?
        if (strstr(curr, "{\"event\":\"info\"") != nullptr)
        {
          // Info: Just log it:
          LOG_INFO(1,
            "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Info Msg: {}", curr)
        }
        else
        if (strstr
           (curr, "{\"event\":\"auth\",\"status\":\"FAILED\"") != nullptr)
        {
          // Authentication Failure: Stop now:
          LOG_ERROR(1,
            "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Auth Failed: {}", curr)
          return false;
        }
        else
        if (strstr(curr, "{\"event\":\"auth\",\"status\":\"OK\"") != nullptr)
        {
          // Authetication Successful:
          LOG_INFO(1,
            "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Auth Successful: {}",
            curr)
          // The LogOn process is now considered to be Complete:
          OMCWS::LogOnCompleted(a_ts_recv);
        }
        else
        if (strstr(curr, "{\"event\":\"error\"") != nullptr)
        {
          //-----------------------------------------------------------------//
          // Some Error: Log it and Exit:                                    //
          //-----------------------------------------------------------------//
          LOG_ERROR(1, "EConnector_WS_BitFinex_OMC::ProcessWSMsg: Got {}", curr)
          return false;
        }
        else
        {
          //-----------------------------------------------------------------//
          // Any other Event -- just log it:                                 //
          //-----------------------------------------------------------------//
          LOG_INFO(1, "EConnector_WS_BitFinex_OMC::ProcessWSMsg: {}", curr)
        }
      }
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is propagated: It is not an error:
      throw;
    }
    catch (exception const& exn)
    {
      // Log the exception and stop:
      LOG_ERROR(2, "EConnector_WS_BitFinex_OMC::ProcessWSMsg: {}", exn.what())
      return false;
    }
    //-----------------------------------------------------------------------//
    // All Done: Restore the original trailing char and continue:            //
    //-----------------------------------------------------------------------//
    return   true;
  }
}
// End namespace MAQUETTE
