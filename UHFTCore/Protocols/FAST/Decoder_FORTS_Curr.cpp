// vim:ts=2:et
//===========================================================================//
//                 "Protocols/FAST/Decoder_FORTS_Curr.cpp":                  //
//    Types and Decoding Functions for FAST Messages for FORTS (All ACs)     //
//                       for the "Curr" FORTS FAST Ver                       //
//===========================================================================//
#include "Protocols/FAST/Decoder_FORTS_Curr.h"
#include "Protocols/FAST/LowLevel.hpp"
#include <utxx/error.hpp>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include "Protocols/FAST/Macros.h" // NB: Should be the last include

# ifdef  __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wshadow"
# endif

namespace MAQUETTE
{
namespace FAST
{
namespace FORTS
{
  //=========================================================================//
  // Static (Unused) Msg Flds:                                               //
  //=========================================================================//
  uint64_t SecurityDefinitionBase_Curr              ::s_SendingTime;
  uint64_t SecurityDefinitionUpdate<ProtoVerT::Curr>::s_SendingTime;
  uint64_t SecurityStatus          <ProtoVerT::Curr>::s_SendingTime;
  uint64_t TradingSessionStatus    <ProtoVerT::Curr>::s_SendingTime;
  uint64_t HeartBeat               <ProtoVerT::Curr>::s_SendingTime;
  uint64_t SequenceReset           <ProtoVerT::Curr>::s_SendingTime;
  uint64_t News                    <ProtoVerT::Curr>::s_SendingTime;
  uint64_t OrdersLogIncrRefresh    <ProtoVerT::Curr>::s_SendingTime;
  uint64_t OrdersLogSnapShot       <ProtoVerT::Curr>::s_SendingTime;

  //=========================================================================//
  // "SecurityDefinition::Decode":                                           //
  //=========================================================================//
  // IDF -- Instruments Definitions:
  //
  char const* SecurityDefinition<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the msg:
    // Zero-out the "plain vanilla" part of the msg:
    SecurityDefinitionBase_Curr::Init();

    // Alias for the msg being filled in:
    SecurityDefinition& msg = *this;

    // Clear the Vectors:
    m_MDFeedTypes.clear();
    m_Underlyings.clear();
    m_InstrLegs.clear();
    m_InstrAttribs.clear();
    m_Events.clear();

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    //-----------------------------------------------------------------------//
    // General Spec:                                                         //
    //-----------------------------------------------------------------------//
    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(  f1,   1,  false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp, s_SendingTime)

    // Total count of "SecurityDefinition" msgs:
    GET_INT(  f3,  f2,  false, OpT::NoOp, m_TotNumReports)
    GET_ASCII(f4,  f3,  false, OpT::NoOp, m_Symbol)
    GET_UTF8( f5,  f4,  true,  OpT::NoOp, m_SecurityDesc)

    // SecurityID: Primary Key. Skipping SecurityIDSource which is a const:
    GET_INT(  f6,  f5,  false, OpT::NoOp, m_SecurityID)
    GET_ASCII(f7,  f6,  true,  OpT::NoOp, m_SecurityAltID)
    GET_ASCII(f8,  f7,  true,  OpT::NoOp, m_SecurityAltIDSource)
    // Type of instrument:
    GET_ASCII(f9,  f8,  true,  OpT::NoOp, m_SecurityType)
    GET_ASCII(f10, f9,  true,  OpT::NoOp, m_CFICode)
    GET_DEC(  f11, f10, true,  OpT::NoOp, m_StrikePrice)
    GET_DEC(  f12, f11, true,  OpT::NoOp, m_ContractMultiplier)
    GET_INT(  f13, f12, true,  OpT::NoOp, m_SecurityTradingStatus)

    GET_ASCII(f14, f13, true,  OpT::NoOp, m_Currency)
    GET_ASCII(f15, f14, false, OpT::NoOp, m_MarketSegmentID)
    GET_INT(  f16, f15, true,  OpT::NoOp, m_TradingSessionID)
    GET_INT(  f17, f16, true,  OpT::NoOp, m_ExchangeTradingSessionID)
    GET_DEC(  f18, f17, true,  OpT::NoOp, m_Volatility)

    // IMPORTANT NOTICE:
    // All Sequences below, do not require a "PMap"s for their entries,  as all
    // flds are "NoOp"s; thus, even if a fld is optional, this affects it null-
    // ability, not presence in the encoded msg (which is always ON). So do NOT
    // extract local "PMap"s from the input stream:
    //-----------------------------------------------------------------------//
    // Sequence: "MDFeedTypes":                                              //
    //-----------------------------------------------------------------------//
    GET_INT(  f19, f18, false, OpT::NoOp, m_NoMDFeedTypes)

    if (utxx::unlikely(m_NoMDFeedTypes  > m_MDFeedTypes.capacity()))
      throw utxx::runtime_error
            ("FAST::FORTS::SecurityDefinition::Decode: Too many MDFeedTypes");

    for (int i = 0; i < int(m_NoMDFeedTypes); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      // No local "PMap" here:
      MDFeedType msg;

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      GET_ASCII(s0,  0, false, OpT::NoOp, m_MDFeedType)
      GET_INT(  s1, s0, true,  OpT::NoOp, m_MarketDepth)
      GET_INT(  s2, s1, true,  OpT::NoOp, m_MDBookType)

      m_MDFeedTypes.push_back(msg);
    }
    //-----------------------------------------------------------------------//
    // Sequence: "Underlyings":                                              //
    //-----------------------------------------------------------------------//
    // NB: The sequence itself is optional. If not present, "m_NoUnderlyings"
    // will remain 0:
    GET_INT(  f20, f19, true,  OpT::NoOp, m_NoUnderlyings)

    if (utxx::unlikely(m_NoUnderlyings  > m_Underlyings.capacity()))
      throw utxx::runtime_error
            ("FAST::FORTS::SecurityDefinition::Decode: Too many Underlyings");

    for (int i = 0; i < int(m_NoUnderlyings); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      // No local "PMap" here:
      Underlying msg;

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      GET_ASCII(s0,  0, false, OpT::NoOp, m_UnderlyingSymbol)
      GET_INT(  s1, s0, true,  OpT::NoOp, m_UnderlyingSecurityID)

      m_Underlyings.push_back(msg);
    }
    //-----------------------------------------------------------------------//
    // Back to the Main Msg: Price Constraints:                              //
    //-----------------------------------------------------------------------//
    GET_DEC(  f21, f20,  true,  OpT::NoOp, m_HighLimitPx)
    GET_DEC(  f22, f21,  true,  OpT::NoOp, m_LowLimitPx)
    GET_DEC(  f23, f22,  true,  OpT::NoOp, m_MinPriceIncrement)
    GET_DEC(  f24, f23,  true,  OpT::NoOp, m_MinPriceIncrementAmount)
    GET_DEC(  f25, f24,  true,  OpT::NoOp, m_InitialMarginOnBuy)
    GET_DEC(  f26, f25,  true,  OpT::NoOp, m_InitialMarginOnSell)
    GET_DEC(  f27, f26,  true,  OpT::NoOp, m_InitialMarginSyntetic)
    GET_ASCII(f28, f27,  true,  OpT::NoOp, m_QuotationList)
    GET_DEC(  f29, f28,  true,  OpT::NoOp, m_TheorPrice)
    GET_DEC(  f30, f29,  true,  OpT::NoOp, m_TheorPriceLimit)

    //-----------------------------------------------------------------------//
    // Sequence: "InstrumentLegs":                                           //
    //-----------------------------------------------------------------------//
    // NB: The sequence itself is optional. If not present, "m_NoLegs" will
    // remain 0:
    GET_INT(  f31, f30, true, OpT::NoOp, m_NoLegs)

    if (utxx::unlikely(m_NoLegs  >  m_InstrLegs.capacity()))
      throw utxx::runtime_error
            ("FAST::FORTS::SecurityDefinition::Decode: Too many InstrLegs");

    for (int i = 0; i < int(m_NoLegs); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      // No local "PMap" here:
      InstrLeg msg;

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      GET_ASCII(s0,  0, false, OpT::NoOp, m_LegSymbol)
      GET_INT(  s1, s0, false, OpT::NoOp, m_LegSecurityID)
      GET_DEC(  s2, s1, false, OpT::NoOp, m_LegRatioQty)

      m_InstrLegs.push_back(msg);
    }
    //-----------------------------------------------------------------------//
    // Sequence: "InstrAttribs":                                             //
    //-----------------------------------------------------------------------//
    // NB: The sequence itself is optional. If not present, "m_NoLegs" will
    // remain 0:
    GET_INT(f32, f31, true, OpT::NoOp, m_NoInstrAttribs)

    if (utxx::unlikely(m_NoInstrAttribs > m_InstrAttribs.capacity()))
      throw utxx::runtime_error
            ("FAST::FORTS::SecurityDefinition::Decode: Too many InstrAttribs");

    for (int i = 0; i < int(m_NoInstrAttribs); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      // No local "PMap" here:
      InstrAttrib msg;

      GET_INT(  s0,  0, false, OpT::NoOp, m_InstrAttribType)
      GET_ASCII(s1, s0, false, OpT::NoOp, m_InstrAttribValue)

      m_InstrAttribs.push_back(msg);
    }
    //-----------------------------------------------------------------------//
    // Back to the Main Msg: Underlying info:                                //
    //-----------------------------------------------------------------------//
    GET_DEC(  f33, f32, true, OpT::NoOp, m_UnderlyingQty)
    GET_ASCII(f34, f33, true, OpT::NoOp, m_UnderlyingCurrency)

    //-----------------------------------------------------------------------//
    // Sequence: "Events":                                                   //
    //-----------------------------------------------------------------------//
    // NB: The sequence itself is optional. If not present, "m_NoEvents" will
    // remain 0:
    GET_INT(  f35, f34, true, OpT::NoOp, m_NoEvents)

    if (utxx::unlikely(m_NoEvents > m_Events.capacity()))
      throw utxx::runtime_error
            ("FAST::FORTS::SecurityDefinition::Decode: Too many Events");

    for (int i = 0; i < int(m_NoEvents);  ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      // No local "PMap" here:
      Event msg;

      GET_INT(  s0,  0, false, OpT::NoOp, m_EventType)
      GET_INT(  s1, s0, false, OpT::NoOp, m_EventDate)
      GET_INT(  s2, s1, false, OpT::NoOp, m_EventTime)

      m_Events.push_back(msg);
    }
    //-----------------------------------------------------------------------//
    // Back to the Main Msg:                                                 //
    //-----------------------------------------------------------------------//
    GET_INT(  f36, f35, true,  OpT::NoOp, m_MaturityDate)
    GET_INT(  f37, f36, true,  OpT::NoOp, m_MaturityTime)

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "SecurityDefinitionUpdate::Decode":                                     //
  //=========================================================================//
  char const* SecurityDefinitionUpdate<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the msg:
    Init();

    // Alias for the msg being filled in:
    SecurityDefinitionUpdate& msg = *this;

    bool is_null = false;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    // XXX: Although all flfd are "NoOp", the top-level PMap  is still present
    // (unlike Sequences/Groups, where local "PMap" is absent in such cases --
    // because TID must still be listed in the top-level "PMap!):
    //
    GET_INT(  f1,   1,  false, OpT::NoOp,  m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp,  s_SendingTime)
    GET_INT(  f3,  f2,  false, OpT::NoOp,  m_SecurityID)
    GET_DEC(  f4,  f3,  true,  OpT::NoOp,  m_Volatility)
    GET_DEC(  f5,  f4,  true,  OpT::NoOp,  m_TheorPrice)
    GET_DEC(  f6,  f5,  true,  OpT::NoOp,  m_TheorPriceLimit)

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "SecurityStatus::Decode":                                               //
  //=========================================================================//
  char const* SecurityStatus<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the msg:
    Init();

    // Alias for the msg being filled in:
    SecurityStatus& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    // Again,  the top-level PMap is still required although all flds are NoOps:
    GET_INT(  f1,   1,  false, OpT::NoOp,  m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp,  s_SendingTime)
    GET_INT(  f3,  f2,  false, OpT::NoOp,  m_SecurityID)
    GET_ASCII(f4,  f3,  false, OpT::NoOp,  m_Symbol)

    GET_INT(  f5,  f4,  true,  OpT::NoOp,  m_SecurityTradingStatus)
    GET_DEC(  f6,  f5,  true,  OpT::NoOp,  m_HighLimitPx)
    GET_DEC(  f7,  f6,  true,  OpT::NoOp,  m_LowLimitPx)
    GET_DEC(  f8,  f7,  true,  OpT::NoOp,  m_InitialMarginOnBuy)
    GET_DEC(  f9,  f8,  true,  OpT::NoOp,  m_InitialMarginOnSell)
    GET_DEC(  f10, f9,  true,  OpT::NoOp,  m_InitialMarginSyntetic)

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "HeartBeat::Decode":                                                    //
  //=========================================================================//
  char const* HeartBeat<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    HeartBeat msg;
    bool is_null = false;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(  f1,   1,  false, OpT::NoOp,  m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp,  s_SendingTime)

    return a_buff;
  }


  //=========================================================================//
  // "SequenceReset::Decode":                                                //
  //=========================================================================//
  char const* SequenceReset<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    SequenceReset msg;
    bool is_null = false;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(  f1,   1,  false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp, s_SendingTime)
    GET_INT(  f3,  f2,  false, OpT::NoOp, m_NewSeqNo)

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "TradingSessionStatus::Decode":                                         //
  //=========================================================================//
  char const* TradingSessionStatus<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the msg:
    Init();

    // Alias for the msg being filled in:
    TradingSessionStatus& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(  f1,   1,  false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp, s_SendingTime)
    GET_INT(  f3,  f2,  false, OpT::NoOp, m_TradSesOpenTime)
    GET_INT(  f4,  f3,  false, OpT::NoOp, m_TradSesCloseTime)
    GET_INT(  f5,  f4,  true,  OpT::NoOp, m_TradSesIntermClearingStartTime)
    GET_INT(  f6,  f5,  true,  OpT::NoOp, m_TradSesIntermClearingEndTime)
    GET_INT(  f7,  f6,  false, OpT::NoOp, m_TradingSessionID)
    GET_INT(  f8,  f7,  true,  OpT::NoOp, m_ExchangeTradingSessionID)
    GET_INT(  f9,  f8,  false, OpT::NoOp, m_TradSesStatus)
    GET_ASCII(f10, f9,  false, OpT::NoOp, m_MarketSegmentID)
    GET_INT(  f11, f10, true,  OpT::NoOp, m_TradSesEvent)

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "News::Decode": TODO: Not Implemented Yet!                              //
  //=========================================================================//

  //=========================================================================//
  // "OrdersLogIncrRefresh::Decode":                                         //
  //=========================================================================//
  char const* OrdersLogIncrRefresh<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later);  but LastFragment is 1 by default:
    this->InitFixed();

    // Alias for the msg being filled in:
    OrdersLogIncrRefresh& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(  f1,   1,  false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp, s_SendingTime)
    GET_INT(  f3,  f2,  false, OpT::NoOp, m_LastFragment)
    GET_INT(  f4,  f3,  false, OpT::NoOp, m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the Sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::FORTS", __func__, "<Curr>: Too many MDEs: ", m_NoMDEntries);
    // Do clean-up:
    InitVar();

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is.
      // NB: No local PMap for the Sequence -- all entries are NoOp:
      MDEntry& msg = m_MDEntries[i];

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      GET_INT(  s0,   0,  false, OpT::NoOp, m_MDUpdateAction)
      GET_ASCII(s1,  s0,  false, OpT::NoOp, m_MDEntryType)
      GET_INT(  s2,  s1,  true,  OpT::NoOp, m_MDEntryID)
      GET_INT(  s3,  s2,  true,  OpT::NoOp, m_SecurityID)
      GET_INT(  s4,  s3,  true,  OpT::NoOp, m_RptSeq)
      GET_INT(  s5,  s4,  true,  OpT::NoOp, m_MDEntryDate)
      GET_INT(  s6,  s5,  false, OpT::NoOp, m_MDEntryTime)
      GET_DEC(  s7,  s6,  true,  OpT::NoOp, m_MDEntryPx)
      GET_INT(  s8,  s7,  true,  OpT::NoOp, m_MDEntrySize)
      GET_DEC(  s9,  s8,  true,  OpT::NoOp, m_LastPx)
      GET_INT(  s10, s9,  true,  OpT::NoOp, m_LastQty)
      GET_INT(  s11, s10, true,  OpT::NoOp, m_TradeID)
      GET_INT(  s12, s11, true,  OpT::NoOp, m_ExchangeTradingSessionID)
      GET_INT(  s13, s12, true,  OpT::NoOp, m_MDFlags)
      GET_INT(  s14, s13, true,  OpT::NoOp, m_Revision)
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "OrdersLogSnapShot::Decode":                                            //
  //=========================================================================//
  char const* OrdersLogSnapShot<ProtoVerT::Curr>::Decode
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later);  but LastFragment is 1 by default:
    this->InitFixed();

    // Alias for the msg being filled in:
    OrdersLogSnapShot& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(  f1,   1,  false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(  f2,  f1,  false, OpT::NoOp, s_SendingTime)
    GET_INT(  f3,  f2,  false, OpT::NoOp, m_LastMsgSeqNumProcessed)
    GET_INT(  f4,  f3,  true,  OpT::NoOp, m_RptSeq)
    GET_INT(  f5,  f4,  false, OpT::NoOp, m_LastFragment)
    GET_INT(  f6,  f5,  false, OpT::NoOp, m_RouteFirst)
    GET_INT(  f7,  f6,  false, OpT::NoOp, m_ExchangeTradingSessionID)
    GET_INT(  f8,  f7,  true,  OpT::NoOp, m_SecurityID)
    GET_INT(  f9,  f8,  false, OpT::NoOp, m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the Sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::FORTS", __func__, "<Curr>: Too many MDEs: ", m_NoMDEntries);
    // Do clean-up:
    InitVar();

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is.
      // NB: No local PMap for the Sequence -- all entries are NoOp:
      MDEntry& msg = m_MDEntries[i];

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      GET_ASCII(s0,   0,  false, OpT::NoOp, m_MDEntryType)
      GET_INT(  s1,  s0,  true,  OpT::NoOp, m_MDEntryID)
      GET_INT(  s2,  s1,  true,  OpT::NoOp, m_MDEntryDate)
      GET_INT(  s3,  s2,  false, OpT::NoOp, m_MDEntryTime)
      GET_DEC(  s4,  s3,  true,  OpT::NoOp, m_MDEntryPx)
      GET_INT(  s5,  s4,  true,  OpT::NoOp, m_MDEntrySize)
      GET_INT(  s6,  s5,  true,  OpT::NoOp, m_TradeID)
      GET_INT(  s7,  s6,  true,  OpT::NoOp, m_MDFlags)
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

} // End namespace FORTS
} // End namespace FAST
} // End namespace MAQUETTE
# ifdef  __clang__
# pragma clang diagnostic pop
# endif
