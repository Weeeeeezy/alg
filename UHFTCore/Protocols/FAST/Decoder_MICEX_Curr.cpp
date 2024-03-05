// vim:ts=2:et
//===========================================================================//
//                 "Protocols/FAST/Decoder_MICEX_Curr.cpp":                  //
//  Types and Decoding Functions for FAST Messages for MICEX Equity and FX   //
//             for the "Curr" MICEX FAST Ver (as of 2017-Mar-30):            //
//===========================================================================//
#include "Protocols/FAST/Decoder_MICEX_Curr.h"
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
namespace MICEX
{
  using AssetClassT = ::MAQUETTE::MICEX::AssetClassT;

  //=========================================================================//
  // Static (Unused) Flds:                                                   //
  //=========================================================================//
  // In "MDEntryIncr":
  Decimal   MDEntryIncr ::s_Yield;
  char      MDEntryIncr ::s_OrdType[4];
  Decimal   MDEntryIncr ::s_TotalVolume;
  Decimal   MDEntryIncr ::s_Price;
  char      MDEntryIncr ::s_OrderStatus[4];
  char      MDEntryIncr ::s_TradingSessionSubID[16];

  // In "MDEntryTrade":
  Decimal   MDEntryTrade::s_Yield;
  char      MDEntryTrade::s_OrdType[4];
  Decimal   MDEntryTrade::s_TotalVolume;
  Decimal   MDEntryTrade::s_Price;
  char      MDEntryTrade::s_OrderStatus[4];
  char      MDEntryTrade::s_TradingSessionSubID[16];
  Decimal   MDEntryTrade::s_AccruedInterestAmt;
  char      MDEntryTrade::s_SettleType[4];
  Decimal   MDEntryTrade::s_TradeValue;
  int32_t   MDEntryTrade::s_PriceType;
  Decimal   MDEntryTrade::s_RepoToPx;
  Decimal   MDEntryTrade::s_BuyBackPx;
  uint32_t  MDEntryTrade::s_BuyBackDate;

  // In "MdEntrySnap":
  Decimal   MDEntrySnap ::s_Yield;
  char      MDEntrySnap ::s_OrderStatus[16];
  char      MDEntrySnap ::s_OrdType[4];
  Decimal   MDEntrySnap ::s_TotalVolume;
  char      MDEntrySnap ::s_TradingSessionSubID[16];

  // In "IncrementalRefresh":
  uint64_t  IncrementalRefresh  <ProtoVerT::Curr>::s_SendingTime;

  // In "TradesIncrement":
  uint64_t  TradesIncrement     <ProtoVerT::Curr>::s_SendingTime;

  // In "SnapShot":
  uint64_t  SnapShot            <ProtoVerT::Curr>::s_SendingTime;
  uint32_t  SnapShot            <ProtoVerT::Curr>::s_RouteFirst;
  int32_t   SnapShot            <ProtoVerT::Curr>::s_TradSesStatus;
  int32_t   SnapShot            <ProtoVerT::Curr>::s_MDSecurityTradingStatus;
  uint32_t  SnapShot            <ProtoVerT::Curr>::s_AuctionIndicator;

  // In "SecurityStatus":
  uint64_t  SecurityStatus      <ProtoVerT::Curr>::s_SendingTime;
  char      SecurityStatus      <ProtoVerT::Curr>::s_TradingSessionSubID[16];
  uint32_t  SecurityStatus      <ProtoVerT::Curr>::s_AuctionIndicator;

  // In "TradingSessionStatus":
  uint64_t  TradingSessionStatus<ProtoVerT::Curr>::s_SendingTime;

  // In "HeartBeat":
  uint64_t  HeartBeat           <ProtoVerT::Curr>::s_SendingTime;

  //=========================================================================//
  // "TradesIncrement::Decode" (TLR, EQ, ID=2521):                           //
  //=========================================================================//
  template <>
  char const* TradesIncrement<ProtoVerT::Curr>::Decode<AssetClassT::EQ>
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later):
    InitFixed();

    // Alias for the msg being filled in:
    TradesIncrement& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(f1,  1, false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(f2, f1, false, OpT::NoOp, s_SendingTime)
    GET_INT(f3, f2, false, OpT::NoOp, m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the Sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::MICEX::", __func__, "<Curr,EQ>: Too many MDEs: ",
             m_NoMDEntries);
    // Do clean-up:
    InitVar();

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      MDEntry& msg = m_MDEntries[i];

      // NB: No "PMap"s for sequence entries, since all operations are "NoOp"s:
      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      //
      GET_INT(   s0,   0, true,  OpT::NoOp,  m_MDUpdateAction)

      GET_ASCII( s1,  s0, false, OpT::NoOp,  m_MDEntryType)  // XXX !!!
      GET_ASCII( s2,  s1, true,  OpT::NoOp,  m_MDEntryID)

      GET_ASCII( s3,  s2, true,  OpT::NoOp,  m_Symbol)
      GET_INT(   s4,  s3, true,  OpT::NoOp,  m_RptSeq)
      GET_INT(   s5,  s4, true,  OpT::NoOp,  m_MDEntryDate)
      GET_INT(   s6,  s5, true,  OpT::NoOp,  m_MDEntryTime)
      GET_INT(   s7,  s6, true,  OpT::NoOp,  m_OrigTime)

      GET_ASCII( s8,  s7, true,  OpT::NoOp,  m_OrderSide)
      GET_DEC(   s9,  s8, true,  OpT::NoOp,  m_MDEntryPx)
      GET_DEC(  s10,  s9, true,  OpT::NoOp,  m_MDEntrySize)
      GET_DEC(  s11, s10, true,  OpT::NoOp,  s_AccruedInterestAmt)
      GET_DEC(  s12, s11, true,  OpT::NoOp,  s_TradeValue)
      GET_DEC(  s13, s12, true,  OpT::NoOp,  s_Yield)

      GET_INT(  s14, s13, true,  OpT::NoOp,  m_SettlDate)
      GET_ASCII(s15, s14, true,  OpT::NoOp,  s_SettleType)
      GET_DEC(  s16, s15, true,  OpT::NoOp,  s_Price)
      GET_INT(  s17, s16, true,  OpT::NoOp,  s_PriceType)

      GET_DEC(  s18, s17, true,  OpT::NoOp,  s_RepoToPx)
      GET_DEC(  s19, s18, true,  OpT::NoOp,  s_BuyBackPx)
      GET_INT(  s20, s19, true,  OpT::NoOp,  s_BuyBackDate)

      GET_ASCII(s21, s20, true,  OpT::NoOp,  m_TradingSessionID)
      GET_ASCII(s22, s21, true,  OpT::NoOp,  s_TradingSessionSubID)
      GET_ASCII(s23, s22, true,  OpT::NoOp,  m_RefOrderID)
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "TradesIncrement::Decode" (TLR, FX, ID=3611):                           //
  //=========================================================================//
  template <>
  char const*
  TradesIncrement<ProtoVerT::Curr>::Decode<AssetClassT::FX>
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later):
    InitFixed();

    // Alias for the msg being filled in:
    TradesIncrement& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(f1,  1, false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(f2, f1, false, OpT::NoOp, s_SendingTime)
    GET_INT(f3, f2, false, OpT::NoOp, m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the Sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::MICEX::", __func__, "<Curr,FX>: Too many MDEs: ",
             m_NoMDEntries);
    // Do clean-up:
    InitVar();

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      MDEntry& msg = m_MDEntries[i];

      // NB: No "PMap"s for sequence entries, since all operations are "NoOp"s:
      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      //
      GET_INT(   s0,   0, true,  OpT::NoOp, m_MDUpdateAction)
      GET_ASCII( s1,  s0, false, OpT::NoOp, m_MDEntryType)  // XXX !!!
      GET_ASCII( s2,  s1, true,  OpT::NoOp, m_MDEntryID)
      GET_ASCII( s3,  s2, true,  OpT::NoOp, m_Symbol)
      GET_INT(   s4,  s3, true,  OpT::NoOp, m_RptSeq)
      GET_INT(   s5,  s4, true,  OpT::NoOp, m_MDEntryDate)

      GET_INT(   s6,  s5, true,  OpT::NoOp, m_MDEntryTime)
      GET_INT(   s7,  s6, true,  OpT::NoOp, m_OrigTime)
      GET_ASCII( s8,  s7, true,  OpT::NoOp, m_OrderSide)

      GET_DEC(   s9,  s8, true,  OpT::NoOp, m_MDEntryPx)
      GET_DEC(  s10,  s9, true,  OpT::NoOp, m_MDEntrySize)
      GET_DEC(  s11, s10, true,  OpT::NoOp, s_TradeValue)
      GET_INT(  s12, s11, true,  OpT::NoOp, m_SettlDate)
      GET_ASCII(s13, s12, true,  OpT::NoOp, s_SettleType)
      GET_DEC(  s14, s13, true,  OpT::NoOp, s_Price)
      GET_INT(  s15, s14, true,  OpT::NoOp, s_PriceType)

      GET_DEC(  s16, s15, true,  OpT::NoOp, s_RepoToPx)
      GET_DEC(  s17, s16, true,  OpT::NoOp, s_BuyBackPx)
      GET_INT(  s18, s17, true,  OpT::NoOp, s_BuyBackDate)

      GET_ASCII(s19, s18, true,  OpT::NoOp, m_TradingSessionID)
      GET_ASCII(s20, s19, true,  OpT::NoOp, s_TradingSessionSubID)
      GET_ASCII(s21, s20, true,  OpT::NoOp, m_RefOrderID)
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "IncrementalRefresh::Decode" (OLR, EQ, ID=2520):                        //
  //=========================================================================//
  template <>
  char const*
  IncrementalRefresh<ProtoVerT::Curr>::Decode<AssetClassT::EQ>
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later):
    InitFixed();

    // Alias for the msg being filled in:
    IncrementalRefresh& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(f1,  1, false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(f2, f1, false, OpT::NoOp, s_SendingTime)
    GET_INT(f3, f2, false, OpT::NoOp, m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the Sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::MICEX::", __func__, "<Curr,EQ>: Too many MDEs: ",
             m_NoMDEntries);
    // Do clean-up:
    InitVar();

    // The previous entry:
    MDEntry const* prev = nullptr;

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      MDEntry& msg = m_MDEntries[i];

      // Get the PMap for this Sequence entry (it is OK to over-write the
      // original "a_pmap") -- required because of "COPY" entries:
      a_buff = GetMsgHeader(a_buff, a_end, &a_pmap, nullptr, "2520");

      // NB: No "PMap"s for sequence entries, since all operations are "NoOp"s:
      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      //
      GET_INT(        s0,   0, true,  OpT::NoOp, m_MDUpdateAction)
      GET_ASCII_COPY( s1,  s0, true,             m_MDEntryType)
      GET_ASCII(      s2,  s1, true,  OpT::NoOp, m_MDEntryID)
      GET_ASCII_COPY( s3,  s2, true,             m_Symbol)
      GET_INT(        s4,  s3, true,  OpT::NoOp, m_RptSeq)

      GET_INT_COPY(   s5,  s4, true,             m_MDEntryDate)
      GET_INT_COPY(   s6,  s5, true,             m_MDEntryTime)
      GET_INT_COPY(   s7,  s6, true,             m_OrigTime)

      GET_DEC_COPY(   s8,  s7, true,             m_MDEntryPx)
      GET_DEC_COPY(   s9,  s8, true,             m_MDEntrySize)
      GET_DEC_COPY(  s10,  s9, true,             s_Yield)

      GET_ASCII_COPY(s11, s10, true,             s_OrderStatus)
      GET_ASCII_COPY(s12, s11, true,             s_OrdType)
      GET_DEC_COPY(  s13, s12, true,             s_TotalVolume)

      GET_ASCII_COPY(s14, s13, true,             m_TradingSessionID)
      GET_ASCII_COPY(s15, s14, true,             s_TradingSessionSubID)

      prev = &msg;
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "IncrenmentalRefresh::Decode" (OLR, FX, ID=3610):                       //
  //=========================================================================//
  template <>
  char const*
  IncrementalRefresh<ProtoVerT::Curr>::Decode<AssetClassT::FX>
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later):
    InitFixed();

    // Alias for the msg being filled in:
    IncrementalRefresh& msg = *this;

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(f1,  1, false, OpT::NoOp, m_MsgSeqNum)
    GET_INT(f2, f1, false, OpT::NoOp, s_SendingTime)
    GET_INT(f3, f2, false, OpT::NoOp, m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the Sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::MICEX::", __func__, "<Curr,FX>: Too many MDEs: ",
             m_NoMDEntries);
    // Do clean-up:
    InitVar();

    // The previous entry:
    MDEntry const* prev = nullptr;

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      MDEntry& msg = m_MDEntries[i];

      // Get the PMap for this Sequence entry (it is OK to over-write the
      // original "a_pmap") -- required because of "COPY" entries:
      a_buff = GetMsgHeader(a_buff, a_end, &a_pmap, nullptr, "3610");

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      //
      GET_INT_COPY(   s0,   0, true,             m_MDUpdateAction)
      GET_ASCII_COPY( s1,  s0, true,             m_MDEntryType)
      GET_ASCII(      s2,  s1, true,  OpT::NoOp, m_MDEntryID)
      GET_ASCII_COPY( s3,  s2, true,             m_Symbol)
      GET_INT(        s4,  s3, true,  OpT::NoOp, m_RptSeq)

      GET_INT_COPY(   s5,  s4, true,             m_MDEntryDate)
      GET_INT_COPY(   s6,  s5, true,             m_MDEntryTime)
      GET_INT_COPY(   s7,  s6, true,             m_OrigTime)

      GET_DEC_COPY(   s8,  s7, true,             m_MDEntryPx)
      GET_DEC_COPY(   s9,  s8, true,             m_MDEntrySize)
      GET_ASCII_COPY(s10,  s9, true,             s_OrderStatus)

      GET_ASCII_COPY(s11, s10, true,             m_TradingSessionID)
      GET_ASCII_COPY(s12, s11, true,             s_TradingSessionSubID)

      prev = &msg;
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "SnapShot::Decode" (OLS, EQ, ID=2510):                                  //
  //=========================================================================//
  template <>
  char const*
  SnapShot<ProtoVerT::Curr>::Decode<AssetClassT::EQ>
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later):
    InitFixed();

    // Alias for the msg being filled in:
    SnapShot& msg = *this;

    bool is_null  = false;
    int  act_len  = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(   f1,   1, false, OpT::NoOp,  m_MsgSeqNum)
    GET_INT(   f2,  f1, false, OpT::NoOp,  s_SendingTime)
    GET_INT(   f3,  f2, true,  OpT::NoOp,  m_LastMsgSeqNumProcessed)
    GET_INT(   f4,  f3, false, OpT::NoOp,  m_RptSeq)
    GET_INT(   f5,  f4, true,  OpT::NoOp,  m_LastFragment)
    GET_INT(   f6,  f5, true,  OpT::NoOp,  s_RouteFirst)
    GET_INT(   f7,  f6, true,  OpT::NoOp,  s_TradSesStatus)
    GET_ASCII( f8,  f7, true,  OpT::NoOp,  m_TradingSessionID)
    GET_ASCII( f9,  f8, false, OpT::NoOp,  m_Symbol)
    GET_INT(  f10,  f9, true,  OpT::NoOp,  s_MDSecurityTradingStatus)
    GET_INT(  f11, f10, true,  OpT::NoOp,  s_AuctionIndicator)
    GET_INT(  f12, f11, false, OpT::NoOp,  m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::MICEX::", __func__, "<Curr,EQ>: Too many MDEs: ",
             m_NoMDEntries);
    // Do clean-up:
    InitVar();

    // The previous entry:
    MDEntry const* prev = nullptr;

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      MDEntry& msg = m_MDEntries[i];

      // Get the PMap for this Sequence entry (it is OK to over-write the
      // original "a_pmap") -- required because of "COPY" entries:
      a_buff = GetMsgHeader(a_buff, a_end, &a_pmap, nullptr, "2510");

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      //
      GET_ASCII_COPY(s0,    0, true,              m_MDEntryType)
      GET_ASCII(     s1,   s0, true,  OpT::NoOp,  m_MDEntryID)
      GET_INT_COPY(  s2,   s1, true,              m_MDEntryDate)
      GET_INT_COPY(  s3,   s2, true,              m_MDEntryTime)
      GET_INT_COPY(  s4,   s3, true,              m_OrigTime)
      GET_DEC_COPY(  s5,   s4, true,              m_MDEntryPx)
      GET_DEC_COPY(  s6,   s5, true,              m_MDEntrySize)
      GET_DEC_COPY(  s7,   s6, true,              s_Yield)
      GET_ASCII_COPY(s8,   s7, true,              s_OrderStatus)
      GET_ASCII_COPY(s9,   s8, true,              s_OrdType)
      GET_DEC_COPY(  s10,  s9, true,              s_TotalVolume)
      GET_ASCII_COPY(s11, s10, true,              s_TradingSessionSubID)

      prev = &msg;
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "SnapShot::Decode" (OLS, FX, ID=3600):                                  //
  //=========================================================================//
  template <>
  char const*
  SnapShot<ProtoVerT::Curr>::Decode<AssetClassT::FX>
  (
    char const*     a_buff,
    char const*     a_end,
    PMap            a_pmap
  )
  {
    assert(a_buff != nullptr && a_buff < a_end);

    // Clean-up the fixed part of the msg (the variable part will be done
    // later):
    InitFixed();

    // Alias for the msg being filled in:
    SnapShot& msg = *this;

    bool is_null  = false;
    int  act_len  = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(   f1,   1, false, OpT::NoOp,  m_MsgSeqNum)
    GET_INT(   f2,  f1, false, OpT::NoOp,  s_SendingTime)
    GET_INT(   f3,  f2, true,  OpT::NoOp,  m_LastMsgSeqNumProcessed)
    GET_INT(   f4,  f3, false, OpT::NoOp,  m_RptSeq)
    GET_INT(   f5,  f4, true,  OpT::NoOp,  m_LastFragment)
    GET_INT(   f6,  f5, true,  OpT::NoOp,  s_RouteFirst)
    GET_INT(   f7,  f6, true,  OpT::NoOp,  s_TradSesStatus)
    GET_ASCII( f8,  f7, true,  OpT::NoOp,  m_TradingSessionID)
    GET_ASCII( f9,  f8, false, OpT::NoOp,  m_Symbol)
    GET_INT(  f10,  f9, true,  OpT::NoOp,  s_MDSecurityTradingStatus)
    GET_INT(  f11, f10, false, OpT::NoOp,  m_NoMDEntries)

    //-----------------------------------------------------------------------//
    // Now do the sequence of MDEntries:                                     //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_NoMDEntries > MaxMDEs))
      throw utxx::runtime_error
            ("FAST::MICEX::", __func__, "<Curr,FX>: Too many MDEs: ",
             m_NoMDEntries);
    // Do clean-up:
    InitVar();

    // The previous entry:
    MDEntry const* prev = nullptr;

    for (int i = 0; i < int(m_NoMDEntries); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      MDEntry& msg = m_MDEntries[i];

      // Get the PMap for this Sequence entry (it is OK to over-write the
      // original "a_pmap") -- required because of "COPY" entries:
      a_buff = GetMsgHeader(a_buff, a_end, &a_pmap, nullptr, "3600");

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      //
      GET_ASCII_COPY(s0,   0,  true,              m_MDEntryType)
      GET_ASCII(     s1,  s0,  true,  OpT::NoOp,  m_MDEntryID)
      GET_INT_COPY(  s2,  s1,  true,              m_MDEntryDate)
      GET_INT_COPY(  s3,  s2,  true,              m_MDEntryTime)
      GET_INT_COPY(  s4,  s3,  true,              m_OrigTime)
      GET_DEC_COPY(  s5,  s4,  true,              m_MDEntryPx)
      GET_DEC_COPY(  s6,  s5,  true,              m_MDEntrySize)
      GET_ASCII_COPY(s7,  s6,  true,              s_OrderStatus)
      GET_ASCII_COPY(s8,  s7,  true,              s_TradingSessionSubID)

      prev = &msg;
    }
    // Sequence done!

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "SecurityDefinition::Decode" (ID=2115):                                 //
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
    m_InstrAttribs.clear();
    m_MarketSegments.clear();

    bool is_null = false;
    int  act_len = 0;

    // Skip mandatory consts -- they are never present in the input stream:

    //-----------------------------------------------------------------------//
    // General Spec:                                                         //
    //-----------------------------------------------------------------------//
    // "f1" is the initial FC for the msg; starts with 1 due to TID bit:
    GET_INT(   f1,   1, false, OpT::Incr,    m_MsgSeqNum)
    GET_INT(   f2,  f1, false, OpT::NoOp,    m_SendingTime)
    GET_ASCII( f3,  f2, false, OpT::Default, m_MessageEncoding)
    DEFAULT_ASCII(m_MessageEncoding, "UTF-8")
    GET_INT(   f4,  f3, true,  OpT::NoOp,    m_TotNumReports)

    GET_ASCII( f5,  f4, true,  OpT::NoOp,    m_Symbol)
    GET_UTF8(  f6,  f5, true,  OpT::NoOp,    m_SecurityID)
    GET_UTF8(  f7,  f6, true,  OpT::NoOp,    m_SecurityIDSource)

    GET_INT(   f8,  f7, true,  OpT::NoOp,    m_Product)
    GET_UTF8(  f9,  f8, true,  OpT::NoOp,    m_CFICode)
    GET_UTF8( f10,  f9, true,  OpT::NoOp,    m_SecurityType)

    GET_INT(  f11, f10, true,  OpT::NoOp,    m_MaturityDate)
    GET_INT(  f12, f11, true,  OpT::NoOp,    m_SettlDate)
    GET_ASCII(f13, f12, true,  OpT::NoOp,    m_SettleType)

    GET_DEC(  f14, f13, true,  OpT::NoOp,    m_OrigIssueAmt)
    GET_INT(  f15, f14, true,  OpT::NoOp,    m_CouponPaymentDate)
    GET_DEC(  f16, f15, true,  OpT::NoOp,    m_CouponRate)
    GET_INT(  f17, f16, true,  OpT::NoOp,    m_SettlFixingDate)
    GET_DEC(  f18, f17, true,  OpT::NoOp,    m_DividendNetPx)
    GET_UTF8( f19, f18, true,  OpT::NoOp,    m_SecurityDesc)
    GET_UTF8( f20, f19, true,  OpT::NoOp,    m_EncodedSecurityDesc)
    GET_UTF8( f21, f20, true,  OpT::NoOp,    m_QuoteText)

    //-----------------------------------------------------------------------//
    // Sequence: "InstrAttribs":                                             //
    //-----------------------------------------------------------------------//
    // NB: The sequence itself is optional. If not present, "m_NoInstrAttribs"
    // will remain 0:
    GET_INT(  f22, f21, true,  OpT::NoOp,    m_NoInstrAttribs)

    // Fill in the "InstrAttribs" vector:
    if (utxx::unlikely(m_NoInstrAttribs > m_InstrAttribs.capacity()))
      throw utxx::runtime_error
            ("FAST::MICEX::SecurityDefinition::Decode: Too many InstrAttribs");

    for (int i = 0; i < int(m_NoInstrAttribs); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      InstrumentAttrib msg;

      GET_INT(  s0,  0, false, OpT::NoOp,    m_InstrAttribType)
      GET_UTF8( s1, s0, true,  OpT::NoOp,    m_InstrAttribValue)

      m_InstrAttribs.push_back(msg);
    }
    //
    // Back to the Main Msg:
    //
    GET_ASCII(f23, f22, true,  OpT::NoOp,    m_Currency)

    //-----------------------------------------------------------------------//
    // Sequence: "MarketSegments":                                           //
    //-----------------------------------------------------------------------//
    // NB: The sequence itself is optional. If not present, "m_NoMarketSegments"
    // will remain 0:
    GET_INT(  f24, f23, true,  OpT::NoOp,    m_NoMarketSegments)

    // Fill in the "MarketSegments" vector:
    if (utxx::unlikely(m_NoMarketSegments > m_MarketSegments.capacity()))
      throw utxx::runtime_error
            ("FAST::MICEX::SecurityDefinition::Decode: "
             "Too many MarketSegments");

    for (int i = 0; i < int(m_NoMarketSegments); ++i)
    {
      // For re-using the above macros, re-define what the "msg" locally is:
      MarketSegment  msg;
      MarketSegment& segm = msg;     // Alias, to be used in the Sub-Sequence

      // "s0" is the initial FC for the Sequence (no TID, so starts with 0):
      GET_DEC(  s0,  0, true,  OpT::NoOp,    m_RoundLot)

      //---------------------------------------------------------------------//
      // Sub-Sequence: "SessionRules":                                       //
      //---------------------------------------------------------------------//
      // NB: The sequence itself is optional. If not present, "m_NoTradingSes-
      // sionRules" will remain 0:
      //
      GET_INT(  s1, s0, true,  OpT::NoOp,    m_NoTradingSessionRules)

      // Fill in the "SessionRules" vector:
      segm.m_SessionRules.clear();
      if (utxx::unlikely
         (segm.m_NoTradingSessionRules > segm.m_SessionRules.capacity()))
        throw utxx::runtime_error
              ("FAST::MICEX::SecurityDefinition::Decode: "
               "Too many SessionRules");

      for (int j = 0; j < int(segm.m_NoTradingSessionRules); ++j)
      {
        // For re-using the above macros, re-define what the "msg" locally is:
        SessionRule msg;

        // "t0" the initial FC for the Sub-Sequence (no TID, starts with 0):
        GET_ASCII(t0,  0, false, OpT::NoOp,  m_TradingSessionID)
        GET_ASCII(t1, t0, true,  OpT::NoOp,  m_TradingSessionSubID)
        GET_INT(  t2, t1, true,  OpT::NoOp,  m_SecurityTradingStatus)
        GET_INT(  t3, t2, true,  OpT::NoOp,  m_OrderNote)

        segm.m_SessionRules.push_back(msg);
      }
      m_MarketSegments.push_back(segm);
    }
    //-----------------------------------------------------------------------//
    // Back to the Main Msg:                                                 //
    //-----------------------------------------------------------------------//
    GET_ASCII(f25, f24, true,  OpT::NoOp,    m_SettlCurrency)
    GET_INT(  f26, f25, true,  OpT::NoOp,    m_PriceType)
    GET_ASCII(f27, f26, true,  OpT::NoOp,    m_StateSecurityID)
    GET_UTF8( f28, f27, true,  OpT::NoOp,    m_EncodedShortSecurityDesc)
    GET_UTF8( f29, f28, true,  OpT::NoOp,    m_MarketCode)

    GET_DEC(  f30, f29, true,  OpT::NoOp,    m_MinPriceIncrement)
    GET_DEC(  f31, f30, true,  OpT::NoOp,    m_MktShareLimit)
    GET_DEC(  f32, f31, true,  OpT::NoOp,    m_MktShareThreshold)
    GET_DEC(  f33, f32, true,  OpT::NoOp,    m_MaxOrdersVolume)
    GET_DEC(  f34, f33, true,  OpT::NoOp,    m_PriceMvmLimit)
    GET_DEC(  f35, f34, true,  OpT::NoOp,    m_FaceValue)
    GET_DEC(  f36, f35, true,  OpT::NoOp,    m_BaseSwapPx)
    GET_DEC(  f37, f36, true,  OpT::NoOp,    m_RepoToPx)
    GET_DEC(  f38, f37, true,  OpT::NoOp,    m_BuyBackPx)
    GET_INT(  f39, f38, true,  OpT::NoOp,    m_BuyBackDate)
    GET_DEC(  f40, f39, true,  OpT::NoOp,    m_NoSharesIssued)
    GET_DEC(  f41, f40, true,  OpT::NoOp,    m_HighLimit)
    GET_DEC(  f42, f41, true,  OpT::NoOp,    m_LowLimit)
    GET_INT(  f43, f42, true,  OpT::NoOp,    m_NumOfDaysToMaturity)

    //-----------------------------------------------------------------------//
    // Return the curr buff ptr:                                             //
    //-----------------------------------------------------------------------//
    return a_buff;
  }

  //=========================================================================//
  // "SecurityStatus::Decode" (ID=2106):                                     //
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
    GET_INT(  f1,   1,  false,  OpT::NoOp,  m_MsgSeqNum)
    GET_INT(  f2,  f1,  false,  OpT::NoOp,  s_SendingTime)
    GET_ASCII(f3,  f2,  false,  OpT::NoOp,  m_Symbol)
    GET_ASCII(f4,  f3,  true,   OpT::NoOp,  m_TradingSessionID)
    GET_ASCII(f5,  f4,  true,   OpT::NoOp,  s_TradingSessionSubID)
    GET_INT(  f6,  f5,  true,   OpT::NoOp,  m_SecurityTradingStatus)
    GET_INT(  f7,  f6,  true,   OpT::NoOp,  s_AuctionIndicator)

    return a_buff;
  }

  //=========================================================================//
  // "TradingSessionStatus::Decode" (ID=2107):                               //
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

    GET_INT(  f3,  f2,  false, OpT::NoOp, m_TradSesStatus)
    GET_ASCII(f4,  f3,  true,  OpT::NoOp, m_Text)
    GET_ASCII(f5,  f4,  false, OpT::NoOp, m_TradingSessionID)

    // Return the curr buff ptr:
    return a_buff;
  }

  //=========================================================================//
  // "HeartBeat::Decode" (ID=2108):                                          //
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

    // XXX: The msg is currently ignored -- there is no call-back for it!
    return a_buff;
  }
} // End namespace MICEX
} // End namespace FAST
} // End namespace MAQUETTE

# ifdef  __clang__
# pragma clang diagnostic pop
# endif
