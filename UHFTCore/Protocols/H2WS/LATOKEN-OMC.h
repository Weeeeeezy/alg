// vim:ts=2:et
//===========================================================================//
//                        "Protocols/H2WS/LATOKEN-OMC.h":                    //
//         LATOKEN OMC/STP Msg Formats and Parsers (for Kafka and WS)        //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/PxsQtys.h"
#include "Protocols/FIX/Msgs.h"
#include <string>
#include <cstdint>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <spdlog/spdlog.h>
#include <cassert>

namespace MAQUETTE
{
namespace LATOKEN
{
  //=========================================================================//
  // LATOKEN Qty Type (Fractional QtyA):                                     //
  //=========================================================================//
  constexpr static QtyTypeT QT   = QtyTypeT::QtyA;
  using                     QR   = double;
  using                     QtyN = Qty<QT,QR>;
}

namespace H2WS
{
namespace LATOKEN
{
  //=========================================================================//
  // Enums:                                                                  //
  //=========================================================================//
  // IMPORTANT: They must be consistent with LATOKEN ME/BE Internal Enums:
  //
  enum class MsgTypeT: unsigned
  {
    UNDEFINED     = 0,
    Resp          = 1, // Order Ack
    Trade         = 2  // Order Confirmed (accepted into ME): may not be a Fill!
  };

  enum class RespT: unsigned
  {
    UNDEFINED     = 0,
    OK            = 1,
    Error         = 2
  };

  enum class ReqT:  unsigned
  {
    UNDEFINED     = 0,
    Auth          = 1,
    NewOrder      = 2,
    CancelOrder   = 3
  };

  enum class FeeT:  unsigned // Fee Type
  {
    UNDEFINED     = 0,
    Locked        = 1,       // On NewOrder
    Released      = 2,       // On Cancel/Fail?
    Taken         = 3,       // Final settlment
    Credit        = 4
  };

  enum class OrdActionT: unsigned
  {
    UNDEFINED     = 0,
    NewOrder      = 1,
    CancelOrder   = 2,
    ReplaceOrder  = 3,  // Not yet implemented!
    Recovery      = 4,  // XXX: Are they sent via the WSTGW???
    RecoveryDone  = 5   //
  };

  // NB:
  // For Side,        we use FIX::SideT
  // For OrderTytype, we use FIX::OrderTypeT
  // For TiF,         we use FIX::TimeInForceT
  // For OrdStatus,   we use FIX::OrdStatusT

  //=========================================================================//
  // "MsgOMC": Synthetic OMC/STP LATOKEN Msg:                                //
  //=========================================================================//
  // This is a Union of Kafka (STP Trd and Bal) and WS (OMC) flds. It is also
  // suitable for MDC OrdersBook and FullOrdersLog:
  //
  struct MsgOMC
  {
    using QtyN = ::MAQUETTE::LATOKEN::QtyN;

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // Msg and Order Type Info:
    MsgTypeT          m_msgType;
    RespT             m_resp;
    ReqT              m_req;
    OrdActionT        m_action;
    FeeT              m_feeType;
    // Order Details:
    FIX::OrderTypeT   m_ordType;
    FIX::SideT        m_side;
    FIX::TimeInForceT m_tif;
    unsigned          m_secID;        // Same as PairID
    unsigned          m_ccyID;
    unsigned          m_userID;
    unsigned          m_makerUserID;
    unsigned          m_takerUserID;
    unsigned          m_userGroup;
    FIX::OrdStatusT   m_ordStatus;
    bool              m_result;       // (Balances only)
    // Qtys:
    QtyN              m_amount;       // Requested order amount
    QtyN              m_showAmt;      // Visible Iceberg amount
    QtyN              m_execQty;      // Qty filled
    QtyN              m_openQty;      // Remaining (open, unfilled) Qty
    QtyN              m_fee;
    QtyN              m_lockedByOrd;
    QtyN              m_lockedByWdr;
    // Pxs:
    PriceT            m_initPx;       // Px as in the order submitted
    PriceT            m_px;           // For unsufficient balances only?
    PriceT            m_execPx;       // Actual trade px
    PriceT            m_avgPx;        // Avg px for an Aggreesive Order(?)
    PriceT            m_bestBidPx;    // L1 OrderBook data
    PriceT            m_bestAskPx;    //  (Kafka only)
    // String data:
    ObjName           m_id;           // Any ID (Trade, Deposit, ...)
    ObjName           m_ordID;        // OrderID
    ObjName           m_cliOrdID;     // ClientOrderID
    ObjName           m_errMsg;       // 64 bytes are enough?
    ObjName           m_apiKey;       // ditto

    //-----------------------------------------------------------------------//
    // EXAMPLE:                                                              //
    //-----------------------------------------------------------------------//
    // {
    //   "msgtp"         : "rspns"|"trd",
    //   "rspns"         : "ok",
    //   "rqst"          : "onew",
    //   "action"        : 1,
    //   "tradingPairId" : 29,
    //   "userId"        : 51983,
    //   "ug"            : 13,
    //   "cliOrderId"    : "cxc-185-shagov-test-01",
    //   "orderId"       : "1557232509.789310.wstg1@0029.1",
    //   "trdId"         : "1557232509.826227.2.ug13",
    //   "initPrice"     : "200",
    //   "avgPrice"      : "200",
    //   "execPrice"     : "200",
    //   "qty"           : "0.00001",
    //   "oqty"          : "0.00001",
    //   "amount"        : "0.00001",
    //   "side"          : "buy"|"sell",
    //   "type"          : "lmt"|"mkt" (Internally: Numerical!)
    //   "message"       : "insufficientBalance",
    //   "tif"           : 1,
    //   "stat"          : "rejected"
    // }
    //-----------------------------------------------------------------------//
    // Default Ctor: Zero-out the object:                                    //
    //-----------------------------------------------------------------------//
    MsgOMC()     { Reset(); }
    void Reset() { memset(this, '\0', sizeof(MsgOMC)); }
  };

  //=========================================================================//
  // "HandlerOMC":                                                           //
  //=========================================================================//
  // Data Handler for LATOKEN Trd and Bal Msgs, based on RpdJson.
  // Taken from:
  // https://github.com/Tencent/rapidjson/blob/master/example/messagereader/
  //        messagereader.cpp
  // http://rapidjson.org/md_doc_sax.html
  //
  // "IsKafkaOrMDA" is "true"  for Kafka Msg Handlers and WSMDA,
  //                   "false" for WSTGW Msg Handlers
  // (their encodings are somewhat different):
  //
  template<bool IsKafkaOrMDA>
  class HandlerOMC final:
    public rapidjson::BaseReaderHandler
          <rapidjson::UTF8<>, HandlerOMC<IsKafkaOrMDA>>
  {
  private:
    //-----------------------------------------------------------------------//
    // Types:                                                                //
    //-----------------------------------------------------------------------//
    using                 QtyN = MAQUETTE::LATOKEN::QtyN;
    constexpr static bool IsWS = !IsKafkaOrMDA;

    // Internal state of this Handler:
    enum class HStateT:  unsigned
    {
      // NB: No UNDEFINED here:
      ExpectObjectStart     = 0,
      ExpectNameOrObjectEnd = 1,
      ExpectValue           = 2
    };

    //-----------------------------------------------------------------------//
    // Which Value to expect (based on the Key):                             //
    //-----------------------------------------------------------------------//
    // NB: This enum should correspond to "MsgOMC" flds above:
    //
    enum class NextValT
    {
      UNDEFINED   =  0,
      MsgType     =  1, // "msgtp"             : String
      Resp        =  2, // "rspns"             : String
      Req         =  3, // "rqst"              : String
      Action      =  4, // "action"            : UInt
      OrdType     =  5, // "type"              : String
      Side        =  6, // "side"              : String
      TiF         =  7, // "tif"               : UInt
      SecID       =  8, // "tradingPairId"     : UInt
      CcyID       =  9, // "currencyId"        : Uint   (Balances only)
      UserID      = 10, // "userId"            : UInt
      MakerUserID = 11, // "userIdMaker"       : UInt
      TakerUserID = 12, // "userIDTaker"       : UInt
      UserGroup   = 13, // "ug"                : UInt
      OrdStatus   = 14, // "stat"              : String
      Amount      = 15, // "amount"            : String -> Double -> QtyN
      ShowAmt     = 16, // "showAmt"           : String -> Double -> QtyN
      ExecQty     = 17, // "qty"               : String -> Double -> QtyN
      OpenQty     = 18, // "oqty"              : String -> Double -> QtyN
      InitPx      = 19, // "initPrice"         : String -> Double -> PriceT
      Price       = 20, // "price"             : String -> Double -> PriceT
      ExecPx      = 21, // "execPrice"         : String -> Double -> PriceT
      AvgPx       = 22, // "avgPrice"          : String -> Double -> PriceT
      BestBidPx   = 23, // "bestBidPx"         : String -> Double -> PriceT
      BestAskPx   = 24, // "bestAskPx"         : String -> Double -> PriceT
      OrdID       = 25, // "orderId"           : String
      CliOrdID    = 26, // "cliOrderId"        : String
      TrdID       = 27, // "trdId"             : String
      ID          = 28, // "id"                : String (Balances only)
      Fee         = 29, // "fee"               : String -> Double -> QtyN (Bal)
      FeeType     = 30, // "feetp"             : UInt   (Balances only)
      LockedByOrd = 31, // "lockedByOrder"     : String -> Double -> QtyN (Bal)
      LockedByWdr = 32, // "lockedByWithdrawal": String -> Double -> QtyN (Bal)
      Result      = 33, // "result"            : UInt   (balances only:   bool)
      ErrMsg      = 34, // "message"           : String
      APIKey      = 35  // "apikey"            : String
    };

    using  KeyMap = std::map<char const*, NextValT, CStrLessT>;
    static KeyMap const  s_Keys;

    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    mutable HStateT  m_state;     // Parser state
    mutable ObjName  m_key;       // Curr Key (to which NextValT corresponds)
    mutable NextValT m_next;      // Where the next value will come to
    mutable MsgOMC   m_msg;       // Struct to be filled in
    spdlog::logger*  m_logger;    // Ptr NOT owned
    int              m_debugLevel;

    // Default Ctor is deleted -- we must always set the Logger & DebugLevel:
    HandlerOMC() =   delete;

  public:
    //-----------------------------------------------------------------------//
    // Non-Default Ctor, Accessor:                                           //
    //-----------------------------------------------------------------------//
    HandlerOMC(spdlog::logger* a_logger, int a_debug_level);

    // The Dtor is auto-generated

    // Accessor to the "MsgOMC" obj:
    MsgOMC const& GetMsg() const { return m_msg; }

    // Resetting the Handler before next object starts:
    void Reset();

    //-----------------------------------------------------------------------//
    // Call-Backs from "RpdJson::Parser":                                    //
    //-----------------------------------------------------------------------//
    // "StartObject": Handling Object Start:
    bool StartObject();

    // "EndObject": Handling Object End:
    bool EndObject(rapidjson::SizeType);

    // "String": Handling a String value in the input. NB: Double vals are also
    // encoded as Strings:
    bool String(char const* a_buff, rapidjson::SizeType a_buff_len, bool);

    // "Uint": Handling a UInt value in the input:
    bool Uint(unsigned a_u);

    //-----------------------------------------------------------------------//
    // Handlers for other types: They do not occur:                          //
    //-----------------------------------------------------------------------//
    // XXX: So return "false" in all those cases?
    //
    bool Int64(int64_t)   { return false; }
    bool Uint64(uint64_t) { return false; }
    bool Double(double)   { return false; }
    bool Default()        { return false; }
  };
} // End namespace LATOKEN
} // End namespace WS
} // End namespace MAQUETTE
