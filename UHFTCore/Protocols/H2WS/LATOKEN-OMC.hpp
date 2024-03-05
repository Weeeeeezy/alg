// vim:ts=2:et
//===========================================================================//
//                      "Protocols/H2WS/LATOKEN-OMC.hpp":                    //
//        LATOKEN OMC/STP Msg Formats and Parsers (for Kafka and WS)         //
//===========================================================================//
#pragma once

#include "Protocols/H2WS/LATOKEN-OMC.h"
#include <utxx/convert.hpp>

namespace MAQUETTE
{
namespace H2WS
{
namespace LATOKEN
{
  //=========================================================================//
  // Utils:                                                                  //
  //=========================================================================//
  //=========================================================================//
  // "HandlerOMC::s_Keys":                                                   //
  //=========================================================================//
  template<bool IsKafkaOrMDA>
  typename HandlerOMC<IsKafkaOrMDA>::KeyMap const
  HandlerOMC<IsKafkaOrMDA>::s_Keys
  {
    { "",                   NextValT::UNDEFINED   },
    { "msgtp",              NextValT::MsgType     },
    { "rspns",              NextValT::Resp        },
    { "rqst",               NextValT::Req         },
    { "action",             NextValT::Action      },
    { "type",               NextValT::OrdType     },
    { "side",               NextValT::Side        },
    { "tif",                NextValT::TiF         },
    // NB: Both "tradingPairId" and "sec" are mapped to the same MsgOMC fld
    // ("m_secID"):
    { "tradingPairId",      NextValT::SecID       },
    { "sec",                NextValT::SecID       },

    { "currencyId",         NextValT::CcyID       },
    { "userId",             NextValT::UserID      },
    { "userIdMaker",        NextValT::MakerUserID },
    { "userIdTaker",        NextValT::TakerUserID },
    { "ug",                 NextValT::UserGroup   },
    { "stat",               NextValT::OrdStatus   },
    { "amount",             NextValT::Amount      },
    { "showAmt",            NextValT::ShowAmt     },
    { "qty",                NextValT::ExecQty     },
    { "oqty",               NextValT::OpenQty     },
    { "initPrice",          NextValT::InitPx      },
    { "price",              NextValT::Price       },
    { "execPrice",          NextValT::ExecPx      },
    { "avgPrice",           NextValT::AvgPx       },
    { "bestBidPx",          NextValT::BestBidPx   },
    { "bestAskPx",          NextValT::BestAskPx   },
    { "orderId",            NextValT::OrdID       },
    { "cliOrderId",         NextValT::CliOrdID    },
    // NB: Both "trId" and "id" are mapped to the same MsgOMC fld ("m_id"):
    { "trdId",              NextValT::ID          },
    { "id",                 NextValT::ID          },

    { "fee",                NextValT::Fee         },
    { "feetp",              NextValT::FeeType     },
    { "lockedByOrder",      NextValT::LockedByOrd },
    { "lockedByWithdrawal", NextValT::LockedByWdr },
    { "result",             NextValT::Result      },
    { "message",            NextValT::ErrMsg      },
    { "apikey",             NextValT::APIKey      }
  };

  //=========================================================================//
  // "HandlerOMC": Non-Default Ctor and "Reset":                             //
  //=========================================================================//
  template<bool IsKafkaOrMDA>
  inline HandlerOMC<IsKafkaOrMDA>::HandlerOMC
  (
    spdlog::logger* a_logger,
    int             a_debug_level
  )
  : m_logger       (a_logger),
    m_debugLevel   (a_debug_level)
  {
    Reset();
    assert(m_logger != nullptr);
  }

  template<bool IsKafkaOrMDA>
  inline void HandlerOMC<IsKafkaOrMDA>::Reset()
  {
    m_state = HStateT ::ExpectObjectStart;
    memset(m_key.data(), '\0', sizeof(m_key));
    m_next  = NextValT::UNDEFINED;
    m_msg.Reset();
  }

  //=========================================================================//
  // "HandlerOMC::StartObject": Handling Object Start:                       //
  //=========================================================================//
  template<bool IsKafkaOrMDA>
  inline bool HandlerOMC<IsKafkaOrMDA>::StartObject()
  {
    if (m_state == HStateT::ExpectObjectStart)
    {
      m_state = HStateT::ExpectNameOrObjectEnd;
      return true;
    }
    else
      return false;
  }

  //=========================================================================//
  // "HandlerOMC::EndObject": Handling Object End:                           //
  //=========================================================================//
  template<bool IsKafkaOrMDA>
  inline bool HandlerOMC<IsKafkaOrMDA>::EndObject(rapidjson::SizeType)
    { return (m_state ==  HStateT::ExpectNameOrObjectEnd); }

  //=========================================================================//
  // "HandlerOMC::String": Handling a String value in the input:             //
  //=========================================================================//
  // NB: Double vals are also encoded as Strings:
  //
  template<bool IsKafkaOrMDA>
  inline bool HandlerOMC<IsKafkaOrMDA>::String
    (char const* a_buff, rapidjson::SizeType a_buff_len, bool)
  {
    assert (a_buff != nullptr);
    switch (m_state)
    {
      //---------------------------------------------------------------------//
      case HStateT::ExpectNameOrObjectEnd:
      //---------------------------------------------------------------------//
      {
        // Determine the next value to be read in,   based on the Key  in  the
        // Buffer. NB: If the Key is not in the list below, it will be ignored,
        // and "m_next" set to "UNDEFINED":
        //
        m_next = NextValT::UNDEFINED;

        // XXX: 0-terminate the string, then restore the original char:
        char* buff = const_cast<char*>(a_buff);
        char  was  = buff[a_buff_len];

        buff[a_buff_len] = '\0';
        auto it  = s_Keys.find(buff);

        if (utxx::likely(it != s_Keys.end()))
        {
          // Memoise "m_key" and "m_next":
          InitFixedStrGen(&m_key, buff);
          m_next = it->second;
        }
        else
          // XXX: We currenly produce an error if an unknown Key was found:
          LOG_ERROR(2, "LATOKEN::HandlerOMC: UnExpected Key={}", buff)

        buff[a_buff_len] = was;
        m_state = HStateT::ExpectValue;
        return true;
      }
      //---------------------------------------------------------------------//
      case HStateT::ExpectValue:
      //---------------------------------------------------------------------//
        // Fill in the corresp fld of "m_msg" from the raw String value:
        switch (m_next)
        {
          //-----------------------------------------------------------------//
          // Enums:                                                          //
          //-----------------------------------------------------------------//
          case NextValT::MsgType:
            m_msg.m_msgType =
              (!strncmp("rspns", a_buff, a_buff_len))
              ? MsgTypeT::Resp :
              (!strncmp("trd",   a_buff, a_buff_len))
              ? MsgTypeT::Trade
              : MsgTypeT::UNDEFINED;
            break;

          case NextValT::Resp:
            // NB: UNDEFINED is treated as "Error" here:
            m_msg.m_resp =
              (!strncmp("ok",   a_buff, a_buff_len))
              ? RespT::OK : RespT::Error;
            break;

          case NextValT::Req:
            m_msg.m_req  =
              (!strncmp("auth",  a_buff, a_buff_len))
              ? ReqT::Auth     :
              (!strncmp("onew",  a_buff, a_buff_len))
              ? ReqT::NewOrder :
              (!strncmp("ocncl", a_buff, a_buff_len))
              ? ReqT::CancelOrder
              : ReqT::UNDEFINED;
            break;

          case NextValT::OrdType:
            // FIXME: No Iceberg support here yet!
            // NB: It is a String for WS only; for Kafka, it is a UInt:
            if (IsWS)
              m_msg.m_ordType =
                (!strncmp("lmt",  a_buff, a_buff_len))
                ? FIX::OrderTypeT::Limit :
                (!strncmp("mkt",  a_buff, a_buff_len))
                ? FIX::OrderTypeT::Market
                : FIX::OrderTypeT::UNDEFINED;
            break;

          case NextValT::Side:
            // NB: It is a String for WS only; for Kafka, it is a UInt:
            if (IsWS)
              m_msg.m_side =
                (!strncmp("buy",  a_buff, a_buff_len))
                ? FIX::SideT::Buy :
                (!strncmp("sell", a_buff, a_buff_len))
                ? FIX::SideT::Sell
                : FIX::SideT::UNDEFINED;
            break;

          case NextValT::OrdStatus:
            // NB: It is a String for WS only; for Kafka, it is a UInt:
            if (IsWS)
              m_msg.m_ordStatus =
                // XXX: "accepted" is translated into "New", rather than into
                // an obscure "AcceptedForBidding":
                (!strncmp("accepted",       a_buff, a_buff_len))
                ? FIX::OrdStatusT::New :
                (!strncmp("partial_filled", a_buff, a_buff_len))
                ? FIX::OrdStatusT::PartiallyFilled :
                (!strncmp("filled",         a_buff, a_buff_len))
                ? FIX::OrdStatusT::Filled :
                (!strncmp("cancelled",      a_buff, a_buff_len))
                ? FIX::OrdStatusT::Cancelled :
                (!strncmp("rejected",       a_buff, a_buff_len))
                ? FIX::OrdStatusT::Rejected
                : FIX::OrdStatusT::UNDEFINED;
            break;

          //-----------------------------------------------------------------//
          // Qtys and Pxs:                                                   //
          //-----------------------------------------------------------------//
          case NextValT::Amount:
            m_msg.m_amount      = QtyN  (strtod(a_buff, nullptr));
            break;

          case NextValT::ShowAmt:
            m_msg.m_showAmt     = QtyN  (strtod(a_buff, nullptr));
            break;

          case NextValT::ExecQty:
            m_msg.m_execQty     = QtyN  (strtod(a_buff, nullptr));
            break;

          case NextValT::OpenQty:
            m_msg.m_openQty     = QtyN  (strtod(a_buff, nullptr));
            break;

          case NextValT::Fee:
            m_msg.m_fee         = QtyN  (strtod(a_buff, nullptr));
            break;

          case NextValT::LockedByOrd:
            m_msg.m_lockedByOrd = QtyN  (strtod(a_buff, nullptr));
            break;

          case NextValT::LockedByWdr:
            m_msg.m_lockedByWdr = QtyN  (strtod(a_buff, nullptr));
            break;

          case NextValT::InitPx:
            m_msg.m_initPx      = PriceT(strtod(a_buff, nullptr));
            break;

          case NextValT::Price:
            m_msg.m_px          = PriceT(strtod(a_buff, nullptr));
            break;

          case NextValT::ExecPx:
            m_msg.m_execPx      = PriceT(strtod(a_buff, nullptr));
            break;

          case NextValT::AvgPx:
            m_msg.m_avgPx       = PriceT(strtod(a_buff, nullptr));
            break;

          case NextValT::BestBidPx:
            // In fact, BestBidPx is currently sent in Kafka msgs only:
            m_msg.m_bestBidPx   = PriceT(strtod(a_buff, nullptr));
            break;

          case NextValT::BestAskPx:
            // In fact, BestAskPx is currently sent in Kafka msgs only:
            m_msg.m_bestAskPx   = PriceT(strtod(a_buff, nullptr));
            break;

          //-----------------------------------------------------------------//
          // String IDs:                                                     //
          //-----------------------------------------------------------------//
          // NB: "a_buff_len"
          case NextValT::OrdID:
            InitFixedStrGen(&m_msg.m_ordID,     a_buff, a_buff_len);
            break;

          case NextValT::CliOrdID:
            InitFixedStrGen(&m_msg.m_cliOrdID,  a_buff, a_buff_len);
            break;

          case NextValT::ID:
            // Any ID (incl Trade, Deposit, Withdrawal etc):
            InitFixedStrGen(&m_msg.m_id,        a_buff, a_buff_len);
            break;

          case NextValT::ErrMsg:
            InitFixedStrGen(&m_msg.m_errMsg,    a_buff, a_buff_len);
            break;

          case NextValT::APIKey:
            InitFixedStrGen(&m_msg.m_apiKey,    a_buff, a_buff_len);
            break;

          //-----------------------------------------------------------------//
          // Misc:                                                           //
          //-----------------------------------------------------------------//
          // XXX: For some reason, "result" is a Boolean string ("0" or "1"):
          case NextValT::Result:
          {
            unsigned b = 0;
            (void) utxx::fast_atoi(a_buff, a_buff_len, b);
            m_msg.m_result  = bool(b);
            break;
          }

          //-----------------------------------------------------------------//
          default:
          //-----------------------------------------------------------------//
            // There must be no more String Flds. In case of any unexpected
            // String value, log a warning but continue:
            LOG_WARN(2,
              "LATOKEN::HandlerOMC: UnExpected String for Key={}, NextValT={}",
              m_key.data(), int(m_next))
        }
        m_state = HStateT::ExpectNameOrObjectEnd;
        m_next  = NextValT::UNDEFINED;    // Just for safety...
        return true;

      //---------------------------------------------------------------------//
      default:
      //---------------------------------------------------------------------//
        // Any other state is not possible:
        return false;
    }
  }

  //=========================================================================//
  // "HandlerOMC::Uint": Handling a UInt value in the input:                 //
  //=========================================================================//
  template<bool IsKafkaOrMDA>
  inline bool HandlerOMC<IsKafkaOrMDA>::Uint(unsigned a_u)
  {
    if (m_state == HStateT::ExpectValue)
    {
      switch (m_next)
      {
        case NextValT::Action:
          // Here we can do a direct conversion, because "OrdActionT" is a
          // LATOKEN-specific type:
          m_msg.m_action      = OrdActionT(a_u);
          break;

        case NextValT::UserID:
          m_msg.m_userID      = a_u;
          break;

        case NextValT::MakerUserID:
          m_msg.m_makerUserID = a_u;
          break;

        case NextValT::TakerUserID:
          m_msg.m_takerUserID = a_u;
          break;

        case NextValT::UserGroup:
          m_msg.m_userGroup   = a_u;
          break;

        case NextValT::SecID:
          m_msg.m_secID       = a_u;
          break;

        case NextValT::CcyID:
          m_msg.m_ccyID       = a_u;
          break;

        case NextValT::FeeType:
          // Again, here we use direct encoding:
          m_msg.m_feeType     = FeeT(a_u);
          break;

        case NextValT::TiF:
          // Convert LATOKEN TiF encoding into a standard FIX one -- they are
          // NOT directly compatible:
          switch (a_u)
          {
            case 1:
              m_msg.m_tif = FIX::TimeInForceT::GoodTillCancel;
              break;
            case 2:
              m_msg.m_tif = FIX::TimeInForceT::Day;
              break;
            case 3:
              m_msg.m_tif = FIX::TimeInForceT::ImmedOrCancel;
              break;
            case 4:
              m_msg.m_tif = FIX::TimeInForceT::FillOrKill;
              break;
            default:
              m_msg.m_tif = FIX::TimeInForceT::UNDEFINED;
          }
          break;

        case NextValT::Side:
          // It is a UInt for Kafka only (for WS, it is a String). Translate
          // LATOKEN encoding into the standard FIX encoding:
          if (IsKafkaOrMDA)
            switch (a_u)
            {
              case 1:
                m_msg.m_side = FIX::SideT::Buy;
                break;
              case 2:
                m_msg.m_side = FIX::SideT::Sell;
                break;
              default:
                m_msg.m_side = FIX::SideT::UNDEFINED;
            }
          break;

        case NextValT::OrdType:
          // It is a UInt for Kafka only (for WS, it is a String). Translate
          // LATOKEN encoding into the standard FIX encoding:
          if (IsKafkaOrMDA)
            switch (a_u)
            {
              case 1:
              case 3:
                // NB: Limit and IcebergLimit types are treated similarly:
                m_msg.m_ordType = FIX::OrderTypeT::Limit;
                break;
              case 2:
                m_msg.m_ordType = FIX::OrderTypeT::Market;
                break;
              default:
                m_msg.m_ordType = FIX::OrderTypeT::UNDEFINED;
            }
          break;

        case NextValT::OrdStatus:
          // It is a UInt for Kafka only (for WS, it is a String). Translate
          // LATOKEN encoding into the standard FIX encoding:
          if (IsKafkaOrMDA)
            switch (a_u)
            {
              case 1:
                // Again, "Accepted" actually means "New":
                m_msg.m_ordStatus = FIX::OrdStatusT::New;
                break;
              case 2:
                m_msg.m_ordStatus = FIX::OrdStatusT::PartiallyFilled;
                break;
              case 3:
                m_msg.m_ordStatus = FIX::OrdStatusT::Filled;
                break;
              case 4:
                m_msg.m_ordStatus = FIX::OrdStatusT::DoneForDay;    // UNUSED
                break;
              case 5:
                m_msg.m_ordStatus = FIX::OrdStatusT::Cancelled;
                break;
              case 6:
                m_msg.m_ordStatus = FIX::OrdStatusT::Replaced;      // UNUSED
                break;
              case 7:
                m_msg.m_ordStatus = FIX::OrdStatusT::PendingCancel; // UNUSED
                break;
              case 8:
                m_msg.m_ordStatus = FIX::OrdStatusT::Stopped;       // UNUSED
                break;
              case 9:
                m_msg.m_ordStatus = FIX::OrdStatusT::Rejected;
                break;
              default:
                m_msg.m_ordStatus = FIX::OrdStatusT::UNDEFINED;
            }
          break;

        default:
          // There must be no more UInt Flds. In case of any unexpected UInt
          // value, log a warning but continue:
          LOG_WARN(2,
            "LATOKEN::HandlerOMC: UnExpected UInt for Key={}, NextValT={}",
            m_key.data(), int(m_next))
      }
      m_state = HStateT::ExpectNameOrObjectEnd;
      m_next  = NextValT::UNDEFINED;    // Just for safety...
    }
    return true;
  }

  //=========================================================================//
  // "MkTSFromMsgID:                                                         //
  //=========================================================================//
  // Most Msgs contain an ID whose prefix can be interpreted as a microsecond-
  // resolution TimeStamp. If the ID is not such, empty TimeStamp is returned:
  //
  inline utxx::time_val MkTSFromMsgID(char const* a_id)
  {
    assert(a_id != nullptr);

    // Find the first two '.'s in TradeID, they delimit the seconds from Epoch
    // and milliseconds in the TimeStamp:
    // NB: TillEOL=false here: '.'s are the delimiters:
    long        sec  = -1;
    long        usec = -1;
    char const* dot1 =
      utxx::fast_atoi<long, false>(a_id,     sizeof(SymKey), sec);

    char const* dot2 = nullptr;
    if (utxx::likely(dot1 != nullptr && *dot1 == '.' && sec > 0))
      dot2 =
      utxx::fast_atoi<long, false>(dot1 + 1, sizeof(SymKey), usec);

    if (utxx::unlikely(dot2 == nullptr || *dot2 != '.' || usec < 0))
      return utxx::time_val();
    else
    {
      // We can now create the ExchTS:
      assert(sec > 0 && usec >= 0);
      return utxx::time_val(sec, usec);
    }
    __builtin_unreachable();
  }
} // End namespace LATOKEN
} // End namespace WS
} // End namespace MAQUETTE
