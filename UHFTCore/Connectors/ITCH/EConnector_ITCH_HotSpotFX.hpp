// vim:ts=2:et
//===========================================================================//
//              "Connectors/ITCH/EConnector_ITCH_HotSpotFX.hpp":             //
//    "EConnector_TCP_ITCH" Impl for ITCH::DialectT::HotSpotFX_{Gen,Link}    //
//===========================================================================//
#pragma once

#include "Connectors/TCP_Connector.hpp"
#include "Connectors/ITCH/EConnector_TCP_ITCH.hpp"
#include "Connectors/ITCH/EConnector_ITCH_HotSpotFX.h"
#include "Venues/HotSpotFX/Configs_ITCH.hpp"
#include "Basis/TimeValUtils.hpp"
#include "Protocols/ITCH/Msgs_HotSpotFX.hpp"
#include <utxx/convert.hpp>
#include <cstddef>
#include <string>
#include <cassert>

//===========================================================================//
// Macros:                                                                   //
//===========================================================================//
//===========================================================================//
// "CHECK_MSG_END_HSFX" (also performs logging of in-bound Msgs):            //
//===========================================================================//
# ifdef  CHECK_MSG_END_HSFX
# undef  CHECK_MSG_END_HSFX
# endif
# define CHECK_MSG_END_HSFX(MsgType)       \
  CHECK_ONLY \
  ( \
    if (utxx::unlikely(msgEnd > chunkEnd))   \
    { \
      /* This msg is incomplete and cannot be processed: */                \
      msgEnd = msgBegin;  \
      break;              \
    } \
    /* Found "msgEnd": */ \
    assert(msgBegin < msgEnd);               \
    /* The end is within the curr buffer: There must be LF there:      */    \
    if (utxx::unlikely(*(msgEnd-1) != '\n')) \
    { \
      /* Serious format error -- considered to be unrecoverable:       */    \
      LOG_ERROR(2, \
        "EConnector_ITCH_HotSpotFX: " #MsgType ": Missing terminating LF")   \
      /* Stop the Connector, but allow for restart: FullStop=false:    */    \
      this->template StopNow<false>("ReadHandler: MsgEnd error", a_ts_recv); \
      return -1; \
    } \
    /* Possibly log this Msg (IsSend=false): */                  \
    if (utxx::unlikely(this->m_protoLogger != nullptr)) \
       /* NB: Do NOT use sizeof(MsgType), as it may contain a var part! */   \
      ITCH::HotSpotFX::LogMsg<false> \
        (this->m_protoLogger, msgBegin, int(msgEnd - msgBegin)); \
  )

//===========================================================================//
// "GET_MSG_HSFX":                                                           //
//===========================================================================//
# ifdef  GET_MSG_HSFX
# undef  GET_MSG_HSFX
# endif
# define GET_MSG_HSFX(MsgType) \
    case MsgT::MsgType:        \
      /* Check the msg end: */ \
      msgEnd = msgBegin + sizeof(MsgType); \
      CHECK_MSG_END_HSFX(MsgType)

//===========================================================================//
// "GENERATE_0ARG_SENDER_HSFX": Such msgs are distinguished only by Type:    //
//===========================================================================//
# ifdef  GENERATE_0ARG_SENDER_HSFX
# undef  GENERATE_0ARG_SENDER_HSFX
# endif
# define GENERATE_0ARG_SENDER_HSFX(MethodName, MsgType)        \
  template<ITCH::DialectT::type D> \
  inline void EConnector_ITCH_HotSpotFX<D>::MethodName() const \
  { \
    ITCH::HotSpotFX::MsgType msg;  \
    this->SendMsg(msg);            \
  }

//===========================================================================//
// "GENERATE_1ARG_SENDER_HSFX": The Arg is a SecDef:                         //
//===========================================================================//
# ifdef  GENERATE_1ARG_SENDER_HSFX
# undef  GENERATE_1ARG_SENDER_HSFX
# endif
# define GENERATE_1ARG_SENDER_HSFX(MethodName, MsgType) \
  template<ITCH::DialectT::type D> \
  inline void EConnector_ITCH_HotSpotFX<D>::MethodName  \
    (SecDefD const& a_instr)       \
  const \
  { \
    ITCH::HotSpotFX::MsgType msg(a_instr.m_Symbol);     \
    this->SendMsg(msg); \
  }
//===========================================================================//

namespace MAQUETTE
{
  //=========================================================================//
  // "EConnector_ITCH_HotSpotFX" Non-Default Ctor:                           //
  //=========================================================================//
  template<ITCH::DialectT::type D>
  inline EConnector_ITCH_HotSpotFX<D>::EConnector_ITCH_HotSpotFX
  (
    EPollReactor*                          a_reactor,
    SecDefsMgr*                            a_sec_defs_mgr,
    std::vector<std::string>    const*     a_only_symbols,
    RiskMgr*                               a_risk_mgr,
    boost::property_tree::ptree const&     a_params,
    EConnector_MktData*                    a_primary_mdc
  )
  : //-----------------------------------------------------------------------//
    // "EConnector": Virtual Base:                                           //
    //-----------------------------------------------------------------------//
    EConnector
    (
      a_params.get<std::string> ("AccountKey"),  // Becomes "m_name"
      "HotSpotFX",
      0,                        // XXX: No extra ShM data at the moment...
      a_reactor,
      false,                    // No need for BusyWait in HotSpotFX
      a_sec_defs_mgr,
      Get_TCP_ITCH_SrcSecDefs<D>
          (EConnector::GetMQTEnv(a_params.get<std::string> ("AccountKey"))),
      a_only_symbols,
      a_params.get<bool>        ("ExplSettlDatesOnly", true),
      Get_TCP_ITCH_UseTenorsInSecIDs<D>(),
      a_risk_mgr,
      a_params,
      QT,
      TCPI::WithFracQtys        // XXX: Why is it configured in TCP_ITCH?
    ),
    //-----------------------------------------------------------------------//
    // "EConnector_TCP_ITCH":                                                //
    //-----------------------------------------------------------------------//
    // NB: As usual, AccountKey is the Connector Instance Name, eg
    // {AccountPfx}-TCP_ITCH-HotSpotFX-{Env}:
    //
    EConnector_TCP_ITCH<D, EConnector_ITCH_HotSpotFX<D>>
    (
      a_primary_mdc,
      a_params
    ),
    //-----------------------------------------------------------------------//
    // Now flds of "EConnector_ITCH_HotSpotFX" itself:                       //
    //-----------------------------------------------------------------------//
    // Extras Flag: XXX: For that, need to get the Config, which is actually
    // (in this case) a derived class instance: "TCP_ITCH_Config_HotSpotFX":
    m_withExtras
      (static_cast<TCP_ITCH_Config_HotSpotFX const*>(this->m_config)->
      m_WithExtras)
  {}

  //=========================================================================//
  // "LogMsg":                                                               //
  //=========================================================================//
  // Implemented via the corresp Protocol-Level method, which outputs msgs in
  // their original txt format (in case of HotSpotFX_{Gen,Link}):
  //
  template<ITCH::DialectT::type D>
  template<bool IsSend>
  inline void EConnector_ITCH_HotSpotFX<D>::LogMsg
    (char const* a_data, int a_len)   const
    { ITCH::HotSpotFX::LogMsg<IsSend>(this->m_protoLogger, a_data, a_len); }

  //=========================================================================//
  // "InitLogOn":                                                            //
  //=========================================================================//
  template<ITCH::DialectT::type D>
  inline void EConnector_ITCH_HotSpotFX<D>::InitLogOn() const
  {
    // Fill in a "LogInReq" msg and send it over:
    ITCH::HotSpotFX::LogInReq msg
      (this->m_config->m_UserName, this->m_config->m_Passwd);
    this->SendMsg(msg);
  }

  //=========================================================================//
  // "ParseAndProcessMktSnapShot":                                            /
  //=========================================================================//
  // Returns: "true" to continue, "false" to stop further reading immediately:
  //
  template<ITCH::DialectT::type D>
  template<bool WithExtras>
  inline bool EConnector_ITCH_HotSpotFX<D>::ParseAndProcessMktSnapShot
  (
    char const*     a_msg_body,
    int             CHECK_ONLY(a_len),
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_handl
  )
  {
    using namespace ITCH::HotSpotFX;

    //-----------------------------------------------------------------------//
    // Checks / Init:                                                        //
    //-----------------------------------------------------------------------//
    utxx::time_val procTS = utxx::now_utc(); // XXX: So Processor level!
    assert(a_msg_body != nullptr && a_len > 0);

    //-----------------------------------------------------------------------//
    // Go Parsing:                                                           //
    //-----------------------------------------------------------------------//
    char const* curr   = a_msg_body;
    SymKey      symbol = MkSymKey(""); // Initially empty

    unsigned    nPairs = GetInt<unsigned, 4>(curr);
    curr += 4;
    try
    {
      //---------------------------------------------------------------------//
      // For all Ccy Pairs:                                                  //
      //---------------------------------------------------------------------//
      for (unsigned i = 0; i < nPairs; ++i)
      {
        // Get the Symbol (CcyPair):
        InitFixedStr(&symbol, curr, 7);
        curr += 7;

        // Get the OrderBook: Produce a warn msg if not found (but no exn). NB:
        // We KNOW that for HotSpotFX, all Symbols are UNIQUE:
        // UseAltSymbol=false:
        //
        OrderBook* obp = this->template FindOrderBook<false>(symbol.data());
        CHECK_ONLY
        (
          if (utxx::unlikely(obp == nullptr))
            LOG_WARN(1,
              "EConnector_ITCH_HotSpotFX::ParseAndProcessMktSnapShot: Symbol="
              "{}: OrderBook not found", symbol.data())
          // XXX: We still have to continue, otherwise the msg would not be
          // parsed correctly!
        )
        if (TCPI::IsFullAmt && obp != nullptr)
          // IMPORTANT: If it is FullAmount stream, the OrderBook needs to be
          // completely cleared before proceeding (in that case, StaticInit-
          // Mode is "false": all SnapShots are coming in the "steady mode"):
          //
          obp->Clear<false>(0, 0);

        //-------------------------------------------------------------------//
        // For Bids and Asks:                                                //
        //-------------------------------------------------------------------//
        for (int s = 1; s >= 0; --s)
        {
          bool isBid = bool(s);

          // Get the number of PxLevels (each of them may consistxs of multiple
          // Orders):
          unsigned nPxs = GetInt<unsigned, 4>(curr);
          curr += 4;

          //-----------------------------------------------------------------//
          // For all Px Levels on this Side:                                 //
          //-----------------------------------------------------------------//
          for (unsigned j = 0; j < nPxs; ++j)
          {
            // Get the Px:
            PriceT px = GetOrderPx<10>(curr);
            curr += 10;

            unsigned nOrds = GetInt<unsigned, 4>(curr);
            curr += 4;

            //---------------------------------------------------------------//
            // For all Orders at this PxLevel:                               //
            //---------------------------------------------------------------//
            for (unsigned k = 0; k < nOrds; ++k)
            {
              // The Qty can be Integral or Fractinal:
              Qty<QT,QR> qty = GetOrderQty<QT, QR, 16>(curr);

              // MinQty[16] and LotSz[16] are either not present at all, or
              // skipped anyway:
              curr += (WithExtras ? 48 : 16);

              // Get the OrderID (per-Symbol):
              OrderID oid = GetInt<OrderID, 15>(curr);
              curr += 15;

              //-------------------------------------------------------------//
              // Install this Order (as "New"):                              //
              //-------------------------------------------------------------//
              // Construct the OrderID which is uniq across all CcyPairs:
              oid = MkUniqOrderID(oid, symbol);

              if (utxx::likely(obp != nullptr))
              {
                // Now install it: SeqNums and RptSeqs are not used (0s):
                constexpr bool IsSnapShot      = true;

                auto res =
                  this->template ApplyOrderUpdate
                    <IsSnapShot,             WithIncrUpdates,
                     TCPI::StaticInitMode,   TCPI::ChangeIsPartFill,
                     TCPI::NewEncdAsChange,  TCPI::ChangeEncdAsNew,
                     TCPI::WithRelaxedOBs,   TCPI::IsFullAmt,
                     TCPI::IsSparse,         QT,            QR>
                    (
                      oid,    FIX::MDUpdateActionT::New,    obp,
                      isBid ? FIX::MDEntryTypeT::Bid : FIX::MDEntryTypeT::Offer,
                      px,     qty, 0, 0
                    );
                OrderBook::UpdateEffectT upd   = std::get<0>(res);
                OrderBook::UpdatedSidesT sides = std::get<1>(res);

                // Obviously, ERROR and NONE update results are NOT allowed;
                // If that happens, we better stop and re-start the Connector,
                // because OrderBook consistency could be lost:
                CHECK_ONLY
                (
                  if (utxx::unlikely
                     (upd == OrderBook::UpdateEffectT::ERROR ||
                      upd == OrderBook::UpdateEffectT::NONE))
                  {
                    LOG_ERROR(1,
                      "EConnector_ITCH_HotSpotFX::ParseAndProcessMktSnapShot: "
                      "Failed to install SnapShot: Symbol={}, Side={}, Px={}, "
                      "Qty={}: Res={}",
                      symbol.data(), isBid ? "Bid" : "Ask", double(px),
                      QR(qty),       int(upd))

                    // FullStop=false:
                    this->template StopNow<false>("SnapShot Error", a_ts_recv);
                    return false;
                  }
                )
                // If OK: Memoise the update result (the ERROR cond is already
                // managed above):
                DEBUG_ONLY(bool ok =)
                  this->AddToUpdatedList
                    (obp, upd, sides, a_ts_exch, a_ts_recv, a_ts_handl, procTS);
                assert(ok);
              }
            } // End of Orders loop
          } // End of Pxs loop
        } // End of Sides loop
      } // End of CcyPairs loop
    } // End of "try"-block
    //-----------------------------------------------------------------------//
    // Exception Processing:                                                 //
    //-----------------------------------------------------------------------//
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated:
      throw;
    }
    catch (std::exception const& exc)
    {
      // Any other error in the SnapShot processing: It is probably safer to
      // stop and re-start the Connector (XXX: though on case  of FullAmount
      // SnapShots, this error might not be that critical):
      // FullStop=false:
      this->template StopNow<false>(exc.what(), a_ts_recv);
      return false;
    }

    //-----------------------------------------------------------------------//
    // Post-Processing:                                                      //
    //-----------------------------------------------------------------------//
    // Check that we have arrived to the correct msg end. If not, we are in a
    // grave condition -- stop immediately (but with a re-start):
    CHECK_ONLY
    (
      int actLen = int(curr - a_msg_body);

      if (utxx::unlikely(actLen != a_len))
      {
        LOG_CRIT(1,
          "EConnector_ITCH_HotSpotFX::ParseAndProcessMktDataSnapShot: ExpLen="
          "{}, ActLen={}: EXITING...", a_len, actLen)
        // FullStop=false:
        this->template StopNow<false>("SnapShotLen MisMatch", a_ts_recv);
        return false;
      }
    )
    // All Done:
    return true;
  }

  //=========================================================================//
  // "ParseAndProcessSequencedPayLoad":                                      //
  //=========================================================================//
  // Return value:
  // (*) > 0: the msg has been successfully parsed and processed, its actual
  //          len was returned;
  // (*) ==0: the msg is incomplete (this is NOT an error -- the Caller will
  //          try to parse it again when more data becomes available);
  // (*) < 0: an error was encontered, stop the Connector now:
  //
  template<ITCH::DialectT::type D>
  template<bool WithExtras>
  inline int EConnector_ITCH_HotSpotFX<D>::ParseAndProcessSequencedPayLoad
  (
    char const*     msgBegin,             // XXX: Macro-compatible name
    char const*     CHECK_ONLY(chunkEnd), // XXX: Macro-compatible name
    utxx::time_val  a_ts_recv,
    utxx::time_val  handlTS               // XXX: Macro-compatible name
  )
  {
    //-----------------------------------------------------------------------//
    // Get the Header:                                                       //
    //-----------------------------------------------------------------------//
    using namespace ITCH::HotSpotFX;
    assert(msgBegin != nullptr && chunkEnd != nullptr && msgBegin < chunkEnd);

    SequencedDataHdr const* sdh =
      reinterpret_cast<SequencedDataHdr const*>(msgBegin);

    // Get the Exchange-Side TimeStamp (IsStrict=true). XXX: Is it always UTC?
    utxx::time_val exchTS =
      GetCurrDate() + TimeToTimeValITCH<true>(sdh->m_Time);

    // NB: Initially, set "msgEnd" to coincide with "msgBegin", which indica-
    // tes MsgLen=0, ie an incomplete msg (not an error!):
    char const* msgEnd    = msgBegin;

    //-----------------------------------------------------------------------//
    // Get and Process the PayLoad Type:                                     //
    //-----------------------------------------------------------------------//
    PayLoadT pldType(sdh->m_PayLoadType);
    switch  (pldType)
    {
      //---------------------------------------------------------------------//
      case PayLoadT::NewOrder:
      //---------------------------------------------------------------------//
      {
        NewOrder const* pld = reinterpret_cast<NewOrder const*>(msgBegin);

        msgEnd = msgBegin + sizeof(NewOrder);
        // Allow for optional MinQty and LotSz flds (ignored anyway):
        if (WithExtras)
          msgEnd += 32;
        CHECK_MSG_END_HSFX(NewOrder)

        // Get the NewOrder details:
        // XXX: For compaibility with the internal TCP ITCH API, Symbol is re-
        // presented as a 16-byte SymKey rather than an 8-byte obj  (possibly
        // even a "long") which would be sufficient for  HotSpotFX Ccy Pairs.
        // This is a minor inefficiency:
        SymKey  symbol;
        InitFixedStr<SymKeySz, sizeof(pld->m_CcyPair)>
                                     (&symbol, pld->m_CcyPair);
        OrderID    oid   = GetInt<OrderID>    (pld->m_OrderID);
        bool       isBuy = GetOrderSide       (pld->m_Side);
        PriceT     px    = GetOrderPx         (pld->m_Price);
        Qty<QT,QR> qty   = GetOrderQty<QT,QR> (pld->m_Qty);

        // Modify the OrderID to make it unique:
        oid = MkUniqOrderID(oid, symbol);

        // Invoke the Processor:
        this->template ProcessNewOrder<QT,QR>
          (symbol, oid, isBuy, px, qty, exchTS, a_ts_recv, handlTS);
        break;
      }
      //---------------------------------------------------------------------//
      case PayLoadT::ModifyOrder:
      //---------------------------------------------------------------------//
      {
        ModifyOrder const* pld =
          reinterpret_cast<ModifyOrder const*>(msgBegin);

        msgEnd = msgBegin + sizeof(ModifyOrder);
        // Allow for optional MinQty and LotSz flds (ignored anyway):
        if (WithExtras)
          msgEnd += 32;
        CHECK_MSG_END_HSFX(ModifyOrder)

        // Get the Order Modification details. Only the Qty can be changed
        // (this may happen due to a Partial Fill):
        SymKey  symbol;
        InitFixedStr<SymKeySz, sizeof(pld->m_CcyPair)>
                                     (&symbol,  pld->m_CcyPair);
        OrderID    oid = GetInt<OrderID>       (pld->m_OrderID);
        Qty<QT,QR> qty = GetOrderQty<QT,QR>    (pld->m_Qty);

        // Modify the OrderID to make it unique:
        oid = MkUniqOrderID(oid, symbol);

        // Invoke the Processor:
        this->template ProcessModifyOrder<QT,QR>
          (symbol, oid, qty, exchTS, a_ts_recv, handlTS);
        break;
      }
      //---------------------------------------------------------------------//
      case PayLoadT::CancelOrder:
      //---------------------------------------------------------------------//
      {
        CancelOrder const* pld = reinterpret_cast<CancelOrder const*>(msgBegin);

        msgEnd = msgBegin + sizeof(CancelOrder);
        // NB: No optional flds here!
        CHECK_MSG_END_HSFX(CancelOrder)

        SymKey  symbol;
        InitFixedStr<SymKeySz, sizeof(pld->m_CcyPair)>
                            (&symbol, pld->m_CcyPair);
        OrderID oid =GetInt<OrderID>(pld->m_OrderID);

        // Modify the OrderID to make it unique:
        oid = MkUniqOrderID(oid, symbol);

        // Invoke the Processor:
        this->template ProcessCancelOrder<QT,QR>
          (symbol, oid, exchTS, a_ts_recv, handlTS);
        break;
      }
      //---------------------------------------------------------------------//
      case PayLoadT::MktSnapShot:
      //---------------------------------------------------------------------//
      {
        // This is more complicated, because "MktSnapShot" does not have a
        // fixed structure:
        // First of all, get the length of the variable part:
        MktSnapShot const* pld =
          reinterpret_cast<MktSnapShot const*>(msgBegin);

        int varLen = GetInt<int>(pld->m_Length);

        // In calculating the "msgEnd", take into account the flds up to, and
        // including, the "Length",  and the terminating LF:
        //
        msgEnd = msgBegin + sizeof(SequencedDataHdr) +
                            sizeof(pld->m_Length)    + varLen + 1;
        CHECK_MSG_END_HSFX(MktSnapShot)

        // Now really parse and process the Variable Part:
        bool ok = ParseAndProcessMktSnapShot<WithExtras>
                  (pld->m_Var, varLen, exchTS, a_ts_recv, handlTS);

        if (utxx::unlikely(!ok))
          // Then the Connector is already stopping, so there is no point in
          // returning the msg len, even if it has been determined correctly:
          return -1;
        break;
      }
      //---------------------------------------------------------------------//
      case PayLoadT::Ticker:
      //---------------------------------------------------------------------//
      {
        // XXX: For the moment, we ignore these msgs, because in HotSpotFX,
        // they are deliberately distorted:
        // (*) only Pxs are correct;
        // (*) Qtys are reported as 0;
        // (*) Timings are subject to random distirtions:
        // In any case, we currently do NOT subscribe to Trades info
        // in ITCH; so if this msg is received somehow, we always skip it:
        msgEnd = msgBegin + sizeof(Ticker);
        CHECK_MSG_END_HSFX(Ticker)
        break;
      }
      //---------------------------------------------------------------------//
      default:
      //---------------------------------------------------------------------//
      {
        // There are no other valid Payload types -- if one is encountered,
        // we cannot delimit the msg, so will have to terminate the session:
        LOG_ERROR(2,
          "EConnector_ITCH_HotSpotFX::ParseAndProcessSequencedPayLoad: "
          "Invalid PayLoadType={}", sdh->m_PayLoadType)
        return -1;
      }
    }

    //-----------------------------------------------------------------------//
    // Analyse the result:                                                   //
    //-----------------------------------------------------------------------//
    // Once we got here, the return value will be non-negative: the msg was ei-
    // ther successfully parsed and processed (res > 0), or was found to be in-
    // complete (res == 0) -- but at least, there was no error:
    assert(msgEnd >= msgBegin);
    int    res = int(msgEnd - msgBegin);
    return res;
  }

  //=========================================================================//
  // "ReadHandler":                                                          //
  //=========================================================================//
  // Call-Back entry point used by "TCP_Connector". Returns the number of bytes
  // "consumed" (ie successfully read and processed, in particular 0 if the msg
  // was incomplete), or (-1) if there was an error:
  //
  template<ITCH::DialectT::type D>
  inline int EConnector_ITCH_HotSpotFX<D>::ReadHandler
  (
    int             DEBUG_ONLY(a_fd),
    char const*     a_buff,     // Points  to the dynamic buffer of Reactor
    int             a_size,     // Size of the data chunk available in Buff
    utxx::time_val  a_ts_recv
  )
  {
    using namespace ITCH::HotSpotFX;
    utxx::time_val handlTS = utxx::now_utc();

    assert(a_fd >= 0 && a_buff != nullptr && a_size > 0 && a_fd == this->m_fd);

    //-----------------------------------------------------------------------//
    // Get the ITCH Msgs from the Byte Stream:                               //
    //-----------------------------------------------------------------------//
    char const* msgBegin = a_buff;
    char const* chunkEnd = a_buff + a_size;
    assert(msgBegin < chunkEnd);
    do
    {
      // Get the MsgType; the PayLoadType is not klnown yet:
      MsgT msgType(*msgBegin);
      char const* msgEnd = msgBegin;   // Not known yet
      try
      {
        switch (msgType)
        {
          //=================================================================//
          // Session-Level Msgs:                                             //
          //=================================================================//
          //-----------------------------------------------------------------//
          GET_MSG_HSFX(LogInAccepted)
          //-----------------------------------------------------------------//
            this->LogOnCompleted(a_ts_recv);
            break;

          //-----------------------------------------------------------------//
          GET_MSG_HSFX(LogInRejected)
          //-----------------------------------------------------------------//
          {
            LogInRejected const* tmsg =
              reinterpret_cast<LogInRejected const*>(msgBegin);

            // Full-Stop NOW:
            LOG_CRIT(1, "LogIn FAILED: {}", ToCStr(tmsg->m_Reason))
            this->template StopNow<true>("LogInRejected", a_ts_recv);
            return -1;
          }
          //-----------------------------------------------------------------//
          GET_MSG_HSFX(ServerHeartBeat)
          //-----------------------------------------------------------------//
            // Nothing specific: "TCP_Connector" monitors the Server liveness
            // using all msgs recvd:
            break;

          //-----------------------------------------------------------------//
          GET_MSG_HSFX(ErrorNotification)
          //-----------------------------------------------------------------//
          {
            ErrorNotification const* tmsg =
              reinterpret_cast<ErrorNotification const*>(msgBegin);

            // XXX: At the moment, we also perform a Full-Stop after a Server-
            // side error, because the integrity of MktData cannot be guarant-
            // eed anymore:
            LOG_CRIT(1,
              "Server-Side ErrorNotification: {}",
              ToCStr(tmsg->m_ErrExplanation))
            this->template StopNow<true>("ErrorNotification", a_ts_recv);
            return -1;
          }
          //-----------------------------------------------------------------//
          case MsgT::InstrsDir:
          //-----------------------------------------------------------------//
          {
            // This msg has a variable format. We just check its size and skip
            // it (but we should not get it at all, as yet):
            InstrsDir const* tmsg =
              reinterpret_cast<InstrsDir const*>(msgBegin);

            unsigned nPairs   = GetInt<unsigned>(tmsg->m_NCcyPairs);

            msgEnd = msgBegin + (offsetof(InstrsDir, m_Var) + 7 * nPairs + 1);
            CHECK_MSG_END_HSFX  (InstrsDir)
            break;
          }

          //=================================================================//
          // Application-Level Msgs:                                         //
          //=================================================================//
          // Actually, there is only 1 primary appl type ("SequencedData"), but
          // different PayLoads within it; and if the payload is empty, then it
          // is "EndOfSession" actually:
          //-----------------------------------------------------------------//
          case MsgT::SequencedData:
          //-----------------------------------------------------------------//
          {
            // We must have at least 1 more byte available in any case:
            CHECK_ONLY
            (
              if (utxx::unlikely(msgBegin + 1 >= chunkEnd))
                break;    // "msgEnd" remains NULL
            )
            // So examine the next byte:
            if (utxx::unlikely(msgBegin[1] == '\n'))
            {
              // Special Case: This is "EndOfSession", NOT "SequencedData"!!!
              // Full-Stop NOW:
              this->template StopNow<true>("EndOfSession", a_ts_recv);
              return -1;
            }

            // Generic Case: This is a "SequencedData":
            int res =
              m_withExtras
              ? ParseAndProcessSequencedPayLoad<true>
                  (msgBegin, chunkEnd, a_ts_recv, handlTS)
              : ParseAndProcessSequencedPayLoad<false>
                  (msgBegin, chunkEnd, a_ts_recv, handlTS);

            CHECK_ONLY
            (
              if (utxx::unlikely(res < 0))
                // We got an outright error -- stop reading now:
                return res;
            )
            // Otherwise:
            msgEnd = msgBegin + res;
            break;
          }

          //-----------------------------------------------------------------//
          default:
          //-----------------------------------------------------------------//
          {
            // Any other MsgTypes must not appear in the input:
            assert(msgEnd == nullptr);
            LOG_ERROR(2,
              "EConnector_ITCH_HotSpotFX::ReadHandler: Invalid MsgType={}",
              *msgBegin)
          }
        }
        // End of "MsgType" switch
      }
      // End of the "try" block
      //---------------------------------------------------------------------//
      // Exception Handling:                                                 //
      //---------------------------------------------------------------------//
      catch (EPollReactor::ExitRun const&)
      { throw; }    // This exception is propagated

      catch (std::exception const& exc)
      {
        // Exceptions while processing ITCH msgs: XXX: they would likely indic-
        // ate some serious logic error, and result in OrderBook inconsistenci-
        // es, so we better re-start the Connector:
        LOG_CRIT(1,
          "EConnector_ITCH_HotSpotFX::ReadHandler: Exception in MsgProcessor: "
          "MsgType={}: {}", char(msgType), exc.what())
        // FullStop=false:
        this->template StopNow<false>(exc.what(), a_ts_recv);
        return -1;
      }

      //---------------------------------------------------------------------//
      // To the next Msg?                                                    //
      //---------------------------------------------------------------------//
      // IMPORTANT: Stop immediately if the main socket has been closed:
      if (utxx::unlikely(this->IsInactive()))
        return -1;

      // If we got here, the following invariant must hold:
      assert(msgEnd >= msgBegin);

      CHECK_ONLY
      (
        if (utxx::unlikely(msgEnd == msgBegin))
          // The msg was incomplete -- so stop reading now and wait until more
          // data becomes available:
        break;
      )
      // Otherwise, continue to the next msg:
      msgBegin = msgEnd;
    }
    while(msgBegin < chunkEnd);
    // End of buffer reading loop

    //-----------------------------------------------------------------------//
    // All currently-available msgs have been read in and processed:         //
    //-----------------------------------------------------------------------//
    // The number of bytes consumed (successfully read and processed):
    int    consumed =  int(msgBegin - a_buff);
    assert(consumed >= 0);

    // Send the "End-of-Data-Chunk" Event, which will be used to invoke the
    // "OnOrderBookUpdate" Call-Backs on the subscribed Strategies  -- XXX:
    // but only if any data were consumed at all,    otherwise there was no
    // chunk to end-process!
    // Ie, the following method is NOT invoked if the "a_buff" only contained
    // an incomplete msg:
    if (consumed > 0)
    {
      this->template ProcessEndOfDataChunk
        <TCPI::IsMultiCast,    WithIncrUpdates, TCPI::WithOrdersLog,
         TCPI::WithRelaxedOBs, QT,              QR>
        ();
    }
    return consumed;
  }

  //=========================================================================//
  // Elementary Senders:                                                     //
  //=========================================================================//
  GENERATE_0ARG_SENDER_HSFX(SendHeartBeat,          ClientHeartBeat)
  GENERATE_0ARG_SENDER_HSFX(GracefulStopInProgress, LogOutReq)
  GENERATE_1ARG_SENDER_HSFX(SendMktDataSubscrReq,   MktDataSubscrReq)
  GENERATE_1ARG_SENDER_HSFX(SendMktSnapShotReq,     MktSnapShotReq)
  GENERATE_1ARG_SENDER_HSFX(SendMktDataUnSubscrReq, MktDataUnSubscrReq)

} // End namespace MAQUETTE
