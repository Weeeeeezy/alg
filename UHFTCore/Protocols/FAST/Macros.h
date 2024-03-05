// vim:ts=2:et
//===========================================================================//
//                           "Protocols/FAST/Macros.h":                      //
//                       Some Macros for FAST Msgs Decoding                  //
//===========================================================================//
#pragma once
#include "Basis/BaseTypes.hpp"
#include <utxx/error.hpp>
#include <type_traits>

// XXX: These macros are a "poor man's" replacement of automatic FAST decoders
// generation from XML specs. Instead, we write code for decoding FAST msgs ma-
// nually, but in order to prevent lots of repetetive boiler-plate code ,  use
// these macros.
// They will produce valid code **ONLY** if placed in a correct local context;
// in particular, the following standardised variable names are used:
//    "a_buff"    -- the buffer being decoded
//    "a_end"     -- ptr to the end of "buff"
//    "msg"       -- the msg obejct being filled in (top-level or group entry)
//    "i"         -- the group entry index (0-based)
//    "prev"      -- ptr to th eprev group entry (NULL if i==0)
//    "a_pmap"    -- PMap of this FAST msg
//    "is_null"   -- output "bull" where the corresp flag is placed:
// NB: The "Get..." functions are templates parameterised by the "Skip" flag.
// Here this flag is set iff the corresp target fld is STATIC (and therefore,
// unused) fld of the Msg class. This is decided using "std::is_member_object_
// pointer" trait which returns FALSE for static members:
//
//===========================================================================//
// Macros for extracting FAST msg flds of different types:                   //
//===========================================================================//
//---------------------------------------------------------------------------//
// "GET_INT":                                                                //
//---------------------------------------------------------------------------//
// NB: In the following macros, "ThisFC"  is constructed with 1 of 2 possible
// ctors: if PrevFC is a name, then an "incremental" ctor is used; if it is a
// number, then the "initial" one is used:
//
#ifdef  GET_INT
#undef  GET_INT
#endif
#define GET_INT(ThisFC, PrevFC, Optional, Op, Fld) \
  constexpr FC ThisFC(  PrevFC, Optional, Op); \
  if (ThisFC.IsPresent(a_pmap)) \
  { \
    using MsgT = typename std::remove_reference <decltype(msg)>::type; \
    constexpr bool Skip = \
      !std::is_member_object_pointer_v<decltype(&MsgT::Fld)>; \
    a_buff = GetInteger<Skip>                      \
             (ThisFC.IsNullable(), a_buff, a_end, &msg.Fld, &is_null,  \
             #Fld,   nullptr);                     \
    if (utxx::unlikely(!Optional && is_null))      \
      throw utxx::runtime_error(__func__,          \
                                ": Missing Mandatory  Integer Fld: " #ThisFC); \
  }

//---------------------------------------------------------------------------//
// "GET_DEC":                                                                //
//---------------------------------------------------------------------------//
#ifdef  GET_DEC
#undef  GET_DEC
#endif
#define GET_DEC(ThisFC, PrevFC, Optional, Op, Fld)   \
  constexpr FC  ThisFC( PrevFC, Optional, Op); \
  if (ThisFC.IsPresent(a_pmap))  \
  { \
    using MsgT = typename std::remove_reference <decltype(msg)>::type; \
    constexpr bool Skip = \
      !std::is_member_object_pointer_v<decltype(&MsgT::Fld)>; \
    a_buff = GetOldDecimal<Skip> \
             (ThisFC.IsNullable(), a_buff, a_end, &msg.Fld, &is_null, #Fld);  \
    if (utxx::unlikely(!Optional && is_null)) \
      throw utxx::runtime_error(__func__,     \
                                ": Missing Mandatory Decimal Fld: " #ThisFC); \
  }
  // NB: Using old-style Decimals (1 bit in the PMap)

//---------------------------------------------------------------------------//
// "GET_ASCII":                                                              //
//---------------------------------------------------------------------------//
// NB: GET_ASCII has a VERY PECULIAR mode when Op=Delta:    It also requires a
// DELTA_ASCII macro before this one, to set the initial value for Delta;
// in the output, the space for 0-terminator is reserved automatically by
// "GetASCII[Delta]" -- no need to subtract 1 from sizeof(msg.Fld) explicitly:
//
#ifdef  GET_ASCII
#undef  GET_ASCII
#endif
#define GET_ASCII(ThisFC, PrevFC, Optional, Op, Fld) \
  constexpr FC    ThisFC( PrevFC, Optional, Op); \
  if (ThisFC.IsPresent(a_pmap))  \
  { \
    using MsgT = typename std::remove_reference <decltype(msg)>::type; \
    constexpr bool Skip = \
      !std::is_member_object_pointer_v<decltype(&MsgT::Fld)>; \
    a_buff = \
      (Op == OpT::Delta) \
      ? GetASCIIDelta<Skip>(ThisFC.IsNullable(), a_buff,   a_end,    msg.Fld, \
                            sizeof(msg.Fld),     &act_len, &is_null, #Fld)    \
      : GetASCII<Skip>     (ThisFC.IsNullable(), a_buff,   a_end,    msg.Fld, \
                            sizeof(msg.Fld),     &act_len, &is_null, #Fld);   \
    if (utxx::unlikely(!Optional && is_null)) \
      throw utxx::runtime_error(__func__,     \
                                ": Missing Mandatory ASCII Fld: " #ThisFC);   \
  }

//---------------------------------------------------------------------------//
// "GET_UTF8":                                                               //
//---------------------------------------------------------------------------//
#ifdef  GET_UTF8
#undef  GET_UTF8
#endif
#define GET_UTF8(ThisFC, PrevFC, Optional, Op, Fld) \
  constexpr FC   ThisFC( PrevFC, Optional, Op); \
  if (ThisFC.IsPresent(a_pmap)) \
  { \
    using MsgT = typename std::remove_reference <decltype(msg)>::type; \
    constexpr bool Skip = \
      !std::is_member_object_pointer_v<decltype(&MsgT::Fld)>; \
    a_buff = GetByteVec<Skip>(ThisFC.IsNullable(), a_buff, a_end, msg.Fld,    \
                              sizeof(msg.Fld)-1, &act_len, &is_null, #Fld);   \
    if (utxx::unlikely(!Optional && is_null)) \
      throw utxx::runtime_error(__func__,     \
                                ": Missing Mandatory UTF8 Fld: " #ThisFC);    \
  }

//===========================================================================//
// Field Operators Support (in Sequences only):                              //
//===========================================================================//
//---------------------------------------------------------------------------//
// "Copy":                                                                   //
//---------------------------------------------------------------------------//
#ifdef  COPY_INT
#undef  COPY_INT
#endif
#define COPY_INT(Fld) \
  else if (utxx::likely(i > 0)) msg.Fld = prev->Fld;

#ifdef  COPY_INT2
#undef  COPY_INT2
#endif
#define COPY_INT2(Fld, Init) \
  else msg.Fld = (utxx::likely(i > 0) ? prev->Fld : Init);

#ifdef  COPY_DEC
#undef  COPY_DEC
#endif
#define COPY_DEC(Fld) \
  else if (utxx::likely(i > 0)) msg.Fld = prev->Fld;

#ifdef  COPY_ASCII
#undef  COPY_ASCII
#endif
#define COPY_ASCII(Fld) \
  else if (utxx::likely(i > 0)) \
    StrNCpy<true>(msg.Fld, prev->Fld);

//---------------------------------------------------------------------------//
// "Delta":                                                                  //
//---------------------------------------------------------------------------//
#ifdef  DELTA_DEC
#undef  DELTA_DEC
#endif
#define DELTA_DEC(Fld) \
  if (utxx::likely(i > 0)) msg.Fld += prev->Fld;

#ifdef  DELTA_DEC0
#undef  DELTA_DEC0
#endif
#define DELTA_DEC0(Fld) \
  else msg.Fld = Decimal(); \
  if (utxx::likely(i > 0)) msg.Fld += prev->Fld;

// NB: The following is to be applied BEFORE the Fld is read:
#ifdef  DELTA_ASCII
#undef  DELTA_ASCII
#endif
#define DELTA_ASCII(Fld, Init) \
   StrNCpy<true>(msg.Fld, utxx::likely(i > 0) ? prev->Fld : Init);

//---------------------------------------------------------------------------//
// "Default":                                                                //
//---------------------------------------------------------------------------//
#ifdef  DEFAULT_INT
#undef  DEFAULT_INT
#endif
#define DEFAULT_INT(Fld, Init) \
  else msg.Fld = Init;

#ifdef  DEFAULT_ASCII
#undef  DEFAULT_ASCII
#endif
#define DEFAULT_ASCII(Fld, Init) \
  else StrNCpy<true>(msg.Fld, Init);

#ifdef  DEFAULT_UTF8
#undef  DEFAULT_UTF8
#endif
#define DEFAULT_UTF8(Fld, Init) \
  else StrNCpy<true>(msg.Fld, Init);

//---------------------------------------------------------------------------//
// Combined "Get" and "Copy" operations:                                     //
//---------------------------------------------------------------------------//
#ifdef  GET_INT_COPY
#undef  GET_INT_COPY
#endif
#define GET_INT_COPY(ThisFC,   PrevFC, Optional, Fld) \
  GET_INT(           ThisFC,   PrevFC, Optional, OpT::Copy,  Fld) \
  COPY_INT(Fld)

#ifdef  GET_INT2_COPY
#undef  GET_INT2_COPY
#endif
#define GET_INT2_COPY(ThisFC,  PrevFC, Optional, Fld, Init) \
  GET_INT(           ThisFC,   PrevFC, Optional, OpT::Copy,  Fld) \
  COPY_INT2(Fld, Init)

#ifdef  GET_DEC_COPY
#undef  GET_DEC_COPY
#endif
#define GET_DEC_COPY(ThisFC,   PrevFC, Optional, Fld) \
  GET_DEC(           ThisFC,   PrevFC, Optional, OpT::Copy, Fld)  \
  COPY_DEC(Fld)

#ifdef  GET_ASCII_COPY
#undef  GET_ASCII_COPY
#endif
#define GET_ASCII_COPY(ThisFC, PrevFC, Optional, Fld) \
  GET_ASCII(           ThisFC, PrevFC, Optional, OpT::Copy, Fld)  \
  COPY_ASCII(Fld)

// XXX:

//---------------------------------------------------------------------------//
// Combined "Get" and "Delta" operations:                                    //
//---------------------------------------------------------------------------//
// XXX: BEWARE: For Integer types, even if the target type is unsigned, Delta
// value itself must be signed!
// NB:
// (*) For Delta, "IsPresent" always returns "true", but we keep this expr as
//     it is evaluated at compile time:
// (*) "delta" is treated as "Skip"pable iff "msg.Fld" is such, and will be 0
//     in such cases:
//
#ifdef  GET_UINT2_DELTA
#undef  GET_UINT2_DELTA
#endif
#define GET_UINT2_DELTA(ThisFC,   PrevFC,  Optional, Fld, Init) \
  constexpr FC ThisFC(PrevFC, Optional, OpT::Delta); \
  if (ThisFC.IsPresent(a_pmap))  \
  { \
    using MsgT = typename std::remove_reference <decltype(msg)>::type; \
    constexpr bool Skip =        \
      !std::is_member_object_pointer_v<decltype(&MsgT::Fld)>; \
    std::remove_reference<decltype(msg.Fld)>::type delta = 0; \
    a_buff =                     \
      GetInteger<false>          \
        (ThisFC.IsNullable(), a_buff, a_end, &delta, &is_null, #Fld, nullptr); \
    if (utxx::unlikely(!Optional && is_null)) \
      throw utxx::runtime_error(__func__,     \
                               ": Missing Mandatory UIntDelta Fld: " #ThisFC); \
    msg.Fld = ((utxx::unlikely(i==0) ? (Init) : prev->Fld) + delta); \
  }

#ifdef  GET_UINT_DELTA
#undef  GET_UINT_DELTA
#endif
#define GET_UINT_DELTA(    ThisFC, PrevFC, Optional, Fld) \
        GET_UINT2_DELTA(   ThisFC, PrevFC, Optional, Fld, 0)

// NB: For signed types, it is somewhat simpler -- no int/uint conversions:
//
#ifdef  GET_INT2_DELTA
#undef  GET_INT2_DELTA
#endif
#define GET_INT2_DELTA(ThisFC, PrevFC,     Optional, Fld, Init) \
  constexpr FC ThisFC( PrevFC, Optional,   OpT::Delta);         \
  if (ThisFC.IsPresent(a_pmap))                \
  { \
    using MsgT = typename std::remove_reference <decltype(msg)>::type; \
    constexpr bool Skip = \
      !std::is_member_object_pointer_v<decltype(&MsgT::Fld)>;   \
    std::remove_reference<decltype(msg.Fld)>::type delta = 0;   \
    a_buff = \
      GetInteger<false>                        \
        (ThisFC.IsNullable(), a_buff, a_end, &delta, &is_null, #Fld, nullptr); \
    \
    if (utxx::unlikely(!Optional && is_null))  \
      throw utxx::runtime_error(__func__,      \
                                ": Missing Mandatory IntDelta Fld: " #ThisFC); \
    msg.Fld = ((utxx::unlikely(i==0) ? (Init) : prev->Fld) + delta);           \
  }

#ifdef  GET_INT_DELTA
#undef  GET_INT_DELTA
#endif
#define GET_INT_DELTA( ThisFC, PrevFC, Optional, Fld) \
        GET_INT2_DELTA(ThisFC, PrevFC, Optional, Fld, 0)

