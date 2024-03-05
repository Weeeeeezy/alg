// vim:ts=2:et
//===========================================================================//
//                        "Protocols/FIX/UtilsMacros.hpp":                   //
//              Macros and Utils for Building and Parsing FIX Msgs           //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/TimeValUtils.hpp"
#include "Protocols/FIX/Msgs.h"
#include <utxx/convert.hpp>
#include <utxx/time_val.hpp>
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <type_traits>
#include <cstdlib>

//===========================================================================//
// "Elementary" Msg Sending Macros:                                          //
//===========================================================================//
// NB: Most macros use "curr" output buf ptr as context:
//
//---------------------------------------------------------------------------//
// SOH as String Literal:                                                    //
//---------------------------------------------------------------------------//
#ifdef  SOH
#undef  SOH
#endif
#define SOH "\x01"

//---------------------------------------------------------------------------//
// "STR_FLD": String:                                                        //
//---------------------------------------------------------------------------//
#ifdef  STR_FLD
#undef  STR_FLD
#endif
#define STR_FLD(Tag, Value)     \
{ \
  curr  = stpcpy (curr, Tag);   \
  curr  = stpcpy (curr, Value); \
  *curr = soh;                  \
  ++curr;                       \
}

//---------------------------------------------------------------------------//
// "INT_VAL", "INT_FLD" (including "unsigned" and/or "long"):                //
//---------------------------------------------------------------------------//
#ifdef  INT_VAL
#undef  INT_VAL
#endif
#define INT_VAL(Value)          \
  (void) utxx::itoa             \
    <typename std::remove_const \
    <typename std::remove_reference<decltype(Value)>::type>::type> \
         (Value, curr);         \

#ifdef  INT_FLD
#undef  INT_FLD
#endif
#define INT_FLD(Tag, Value)   \
{ \
  curr  = stpcpy (curr, Tag); \
  INT_VAL(Value);             \
  *curr = soh;                \
  ++curr;                     \
}

//---------------------------------------------------------------------------//
// "HEX12_FLD":                                                              //
//---------------------------------------------------------------------------//
// Will produce a 48-bit (12 hex digits) string:
#ifdef  HEX12_FLD
#undef  HEX12_FLD
#endif
#define HEX12_FLD(Tag, Value) \
{ \
  curr  = stpcpy (curr, Tag); \
  curr  = utxx::itoa16_right<unsigned long, 12>(curr, Value); \
  *curr = soh;                \
  ++curr;                     \
}

//---------------------------------------------------------------------------//
// "EXPL_FLD": Value is given as an immediate string operand:                //
//---------------------------------------------------------------------------//
#ifdef  EXPL_FLD
#undef  EXPL_FLD
#endif
#define EXPL_FLD(TagVal)     \
  curr  = stpcpy (curr, TagVal SOH);

//---------------------------------------------------------------------------//
// "CHAR_FLD" (for Enum values encoded as chars):                            //
//---------------------------------------------------------------------------//
#ifdef  CHAR_FLD
#undef  CHAR_FLD
#endif
#define CHAR_FLD(Tag, Enum)  \
{ \
  curr  = stpcpy(curr, Tag); \
  *curr = char(Enum);        \
  ++curr;                    \
  *curr = soh;               \
  ++curr;                    \
}

//---------------------------------------------------------------------------//
// "CHAR2_VAL":                                                              //
//---------------------------------------------------------------------------//
#ifdef  CHAR2_VAL
#undef  CHAR2_VAL
#endif
#define CHAR2_VAL(Enum)          \
{ \
  uint32_t val = uint32_t(Enum); \
  *curr = char(val & 0xFF);      \
  ++curr;                        \
  val >>= 8;                     \
  if (val != 0)                  \
  {                              \
    assert(val <= 0xFF);         \
    *curr = char(val);           \
    ++curr;                      \
  }                              \
}

//---------------------------------------------------------------------------//
// "CHAR2_FLD" (for Enum values encoded as char or two chars):               //
//---------------------------------------------------------------------------//
#ifdef CHAR2_FLD
#undef CHAR2_FLD
#endif
#define CHAR2_FLD(Tag, Enum)     \
{ \
  curr  = stpcpy(curr, Tag);     \
  CHAR2_VAL(Enum)                \
  *curr = soh;                   \
  ++curr;                        \
}

//---------------------------------------------------------------------------//
// "DEC_VAL", "DEC_FLD":                                                     //
//---------------------------------------------------------------------------//
// XXX: Decimals are floating-point numbers ("double"s) internally, so we need
// to explicitly specify the decimal precision. The highest possible precision
// we are aware of, is 8 decimal points (if less is required, trailing 0s will
// not harm -- but XXX there is a risk that there might be trailing 9s!).  The
// maximum fld width is 17, ie like 99999999.12345678,  which is about the max
// relative precision of a "double" anyway.
// This is mostly for Pxs and Px-related data, and Fractional Qtys:
// TODO: Compile-time max precision here (will come from SecDefs):
//
#ifdef  DEC_VAL
#undef  DEC_VAL
#endif
#define DEC_VAL(Value) \
  curr += utxx::ftoa_left(Value, curr, 13, 5, true);

#ifdef  DEC_FLD
#undef  DEC_FLD
#endif
#define DEC_FLD(Tag, Value)  \
{ \
  curr  = stpcpy(curr, Tag); \
  DEC_VAL(Value)             \
  *curr = soh;               \
  ++curr;                    \
}

//---------------------------------------------------------------------------//
// "QTY_FLD":                                                                //
//---------------------------------------------------------------------------//
// We support both Long- and Double-valued Qtys:
//
#ifdef  QTY_FLD
#undef  QTY_FLD
#endif
#define QTY_FLD(Tag, Value)  \
{ \
  curr  = stpcpy(curr, Tag); \
  { \
    if constexpr(ProtocolFeatures<D>::s_hasFracQtys)  \
      DEC_VAL(double(QR(Value))) \
    else \
      INT_VAL(long  (QR(Value))) \
  } \
  *curr = soh;               \
  ++curr;                    \
}

//---------------------------------------------------------------------------//
// "MK_SYMBOL":                                                              //
//---------------------------------------------------------------------------//
// We support both Long- and Double-valued Qtys:
//
#ifdef  MK_SYMBOL
#undef  MK_SYMBOL
#endif
#define MK_SYMBOL(Instr)                        \
{                                               \
  if constexpr (D == DialectT::TT) {            \
    STR_FLD("55=", (Instr)->m_Symbol.data())    \
    INT_FLD("48=", (Instr)->m_SecID)            \
    EXPL_FLD("207=CME")                         \
  } else {                                      \
    if constexpr (ProtocolFeatures<D>::s_useSecIDInsteadOfSymbol) { \
      INT_FLD("48=", (Instr)->m_SecID)          \
      EXPL_FLD("22=8")                          \
    } else {                                    \
      STR_FLD("55=", (Instr)->m_Symbol.data())  \
    }                                           \
  }                                             \
}

//---------------------------------------------------------------------------//
// "MK_PARTIES":                                                             //
//---------------------------------------------------------------------------//
// Extra context var (apart from "curr"): "a_sess":
//
#ifdef  MK_PARTIES
#undef  MK_PARTIES
#endif
#define MK_PARTIES  \
  if (!IsEmpty(a_sess->m_partyID))             \
  { \
    /* Yes, the Party Group is required:  */   \
    EXPL_FLD("453=1")   /* NoPartyIDs = 1 */   \
    STR_FLD( "448=", a_sess->m_partyID.data()) \
    CHAR_FLD("447=", a_sess->m_partyIDSrc)     \
    INT_FLD( "452=", a_sess->m_partyRole)      \
  }

//===========================================================================//
// "Elementary" Msg Reading Macros:                                          //
//===========================================================================//
// NB: Context variables:
//     -- "curr" : at invocation time points to the beginning of the NEXT fld,
//        so it can be used as the end of this fld's value;
//     -- "var"  : ptr to the string value;
//     -- "tmsg" : ptr to the typed output FIX msg:
//
//---------------------------------------------------------------------------//
// "GET_CHAR":                                                               //
//---------------------------------------------------------------------------//
#ifdef  GET_CHAR
#undef  GET_CHAR
#endif
#define GET_CHAR(Tag, Fld, ...) \
  case Tag: \
    tmsg.Fld = val[0]; \
    __VA_ARGS__ ;      \
    break;

//---------------------------------------------------------------------------//
// "GET_BOOL":                                                               //
//---------------------------------------------------------------------------//
#ifdef  GET_BOOL_VAL
#undef  GET_BOOL_VAL
#endif
#define GET_BOOL_VAL(Targ)  \
  Targ =    \
    (val[0] == 'Y') \
    ? true  \
    :       \
    (val[0] == 'N') \
    ? false \
    : throw utxx::runtime_error("FIX(Read): Invalid Bool: ", tag, '=', val);

#ifdef  GET_BOOL
#undef  GET_BOOL
#endif
#define GET_BOOL(Tag, Fld, ...) \
  case Tag: \
    GET_BOOL_VAL(tmsg.Fld)  \
    __VA_ARGS__ ; \
    break;

//---------------------------------------------------------------------------//
// "GET_NAT", "GET_POS":                                                     //
//---------------------------------------------------------------------------//
// NB: The value fld is delimited by (curr-1) where there is a '\0' terminator,
//     hence TillEOL=true in "utxx::fast_atoi":
//
#ifdef  GET_NAT_VAL
#undef  GET_NAT_VAL
#endif
#define GET_NAT_VAL(Targ) \
  Targ = 0; \
  assert(*(curr - 1) == '\0'); \
  if (utxx::unlikely  \
     (utxx::fast_atoi \
        <typename std::remove_reference<decltype(Targ)>::type, true> \
        (val, curr - 1, Targ) == nullptr)) \
    throw utxx::runtime_error("FIX(Read): Invalid Nat: ", tag, '=', val);

#ifdef  GET_POS_VAL
#undef  GET_POS_VAL
#endif
#define GET_POS_VAL(Targ) \
  Targ = 0; \
  assert(*(curr - 1) == '\0'); \
  if (utxx::unlikely \
    (utxx::fast_atoi \
      <typename std::remove_reference<decltype(Targ)>::type, true> \
      (val, curr - 1, Targ) == nullptr || (Targ) == 0)) \
    throw utxx::runtime_error("FIX(Read): Invalid Pos: ", tag, '=', val);

#ifdef  GET_NAT
#undef  GET_NAT
#endif
#define GET_NAT(Tag, Fld, ...) \
  case Tag:        \
    GET_NAT_VAL(tmsg.Fld) \
    __VA_ARGS__ ;  \
    break;

#ifdef  GET_POS
#undef  GET_POS
#endif
#define GET_POS(Tag, Fld, ...) \
  case Tag:        \
    GET_POS_VAL(tmsg.Fld) \
    __VA_ARGS__ ;  \
    break;

//---------------------------------------------------------------------------//
// "STRTOD": Different Impls in Checked and UnChecked Modes:                 //
//---------------------------------------------------------------------------//
#ifdef  GET_DBL_VAL
#undef  GET_DBL_VAL
#endif
#if     UNCHECKED_MODE
#define GET_DBL_VAL strtod(val, nullptr)
#else
inline  double StrToD(char const* a_fld_val, char const* a_fld_end)
{
  assert(a_fld_val != nullptr && a_fld_end != nullptr && a_fld_val < a_fld_end);
  char*  end = nullptr;
  double res = strtod(a_fld_val,  &end);
  if (utxx::unlikely (end != a_fld_end))
    throw utxx::runtime_error("FIX(Read): Invalid Dbl Fld: ", a_fld_val);
  return res;
}
#define GET_DBL_VAL StrToD(val, curr - 1) // Because FldEnd is (curr - 1)
#endif

//---------------------------------------------------------------------------//
// "GET_PX":                                                                 //
//---------------------------------------------------------------------------//
#ifdef  GET_PX
#undef  GET_PX
#endif
#define GET_PX(Tag, Fld, ...)   \
  case Tag: \
    tmsg.Fld = PriceT(GET_DBL_VAL); \
    __VA_ARGS__ ; \
    break;

//---------------------------------------------------------------------------//
// "GET_QTY":                                                                //
//---------------------------------------------------------------------------//
// Both Integral and Fractional Qtys are supported:
#ifdef  GET_QTY
#undef  GET_QTY
#endif
#define GET_QTY(Tag, Fld, ...)       \
  case Tag: \
    if constexpr (ProtocolFeatures<D>::s_hasFracQtys)  \
    { \
      /* Qty can be Fractional: */   \
      double dQty = GET_DBL_VAL;     \
      /* NB: The following check also catches NaNs: */ \
      CHECK_ONLY \
      ( \
      if (utxx::unlikely(!(dQty >= 0.0))) \
        throw utxx::runtime_error("FIX(Read): Invalid Qty: ", tag, '=', val); \
      ) \
      tmsg.Fld = QtyN(dQty); \
    } \
    else \
    {    \
      /* Qty is a Natural number: */ \
      long lQty = -1;                \
      GET_NAT_VAL(lQty)              \
      tmsg.Fld  = QtyN(lQty);        \
    }    \
    __VA_ARGS__ ;                    \
    break;

//---------------------------------------------------------------------------//
// "GET_STR":                                                                //
//---------------------------------------------------------------------------//
// Target is a fixed-size array of "char"s. Just for extra safety, '\0'-termi-
// nate it (it should be '\0' at the end already):
#ifdef  GET_STR
#undef  GET_STR
#endif
#define GET_STR(Tag, Fld, ...)    \
  case Tag: \
    StrNCpy<true>(tmsg.Fld, val); \
    __VA_ARGS__ ; \
    break;

//---------------------------------------------------------------------------//
// "GET_SYM", "GET_CRD" (Credential), "GET_CCY":                             //
//---------------------------------------------------------------------------//
// Similar to "GET_STR", but the Target is "std::array<char, N>" instead:
//
#ifdef  GET_SYM_VAL
#undef  GET_SYM_VAL
#endif
#define GET_SYM_VAL(Targ) InitFixedStr<SymKeySz>(&(Targ), val);

#ifdef  GET_SYM
#undef  GET_SYM
#endif
#define GET_SYM(Tag, Fld, ...) \
  case Tag: \
    GET_SYM_VAL(tmsg.Fld) \
    __VA_ARGS__ ;    \
    break;

#ifdef  GET_CRD_VAL
#undef  GET_CRD_VAL
#endif
#define GET_CRD_VAL(Targ) InitFixedStr<CredentialSz>(&(Targ), val);

#ifdef  GET_CRD
#undef  GET_CRD
#endif
#define GET_CRD(Tag, Fld, ...) \
  case Tag: \
    GET_CRD_VAL(tmsg.Fld) \
    __VA_ARGS__ ;    \
    break;

#ifdef  GET_CCY_VAL
#undef  GET_CCY_VAL
#endif
#define GET_CCY_VAL(Targ) InitFixedStr<CcySz>(&(Targ), val);

#ifdef  GET_CCY
#undef  GET_CCY
#endif
#define GET_CCY(Tag, Fld, ...) \
  case Tag: \
    GET_CCY_VAL(tmsg.Fld) \
    __VA_ARGS__ ;    \
    break;

//---------------------------------------------------------------------------//
// "GET_ENUM":                                                               //
//---------------------------------------------------------------------------//
// (1) With "char" Enum encoding:
//
#ifdef  GET_ENUM_CHAR
#undef  GET_ENUM_CHAR
#endif
#define GET_ENUM_CHAR(Tag, Fld, EnumType, ...) \
  case Tag: \
    tmsg.Fld = EnumType(val[0]); \
    __VA_ARGS__ ; \
    break;

// (2) With "int" Enum encoding; NB: We only use Natural values for Enum
//     encoding (in particular, NEVER (-1) for UNDEFINED):
//
#ifdef  GET_ENUM_INT
#undef  GET_ENUM_INT
#endif
#define GET_ENUM_INT(Tag, Fld, EnumType, ...)  \
  case Tag:        \
  { \
    int c = 0;     \
    GET_NAT_VAL(c);         \
    tmsg.Fld = EnumType(c); \
    __VA_ARGS__ ;  \
    break;         \
  }

//---------------------------------------------------------------------------//
// "GET_TIME", "GET_DATE_TIME":                                              //
//---------------------------------------------------------------------------//
#ifdef  GET_TIME
#undef  GET_TIME
#endif
#define GET_TIME(Tag, Fld, ...)       \
  case Tag: \
    tmsg.Fld = TimeToTimeValFIX(val); \
    __VA_ARGS__ ; \
    break;

#ifdef  GET_DATE_TIME
#undef  GET_DATE_TIME
#endif
#define GET_DATE_TIME(Tag, Fld, ...) \
  case Tag:       \
    tmsg.Fld = DateTimeToTimeValFIX(val); \
    __VA_ARGS__ ; \
    break;

//---------------------------------------------------------------------------//
// "GET_REQ_ID":                                                             //
//---------------------------------------------------------------------------//
// NB: Remove a special ReqID prefix if it is used. The prefix is supposed to
// be of constant size, which is pre-computed for efficiency (0 if no prefix):
//
#ifdef  GET_REQ_ID
#undef  GET_REQ_ID
#endif
#define GET_REQ_ID(Tag, Fld, ...) \
  case Tag: \
    val += m_reqIDPfxSz;          \
    GET_NAT_VAL(tmsg.Fld);        \
    __VA_ARGS__ ;                 \
   break;

//===========================================================================//
// "Higher-Level" Msg Reading Macros:                                        //
//===========================================================================//
//---------------------------------------------------------------------------//
// "GENERATE_PARSER_COMMON":                                                 //
//---------------------------------------------------------------------------//
// Common Macro to generate Reader Functions which fill in Typed FIX Msgs from
// a Text FIX Msg!
//
#define GENERATE_PARSER(MsgType, SpecificCases, ...) \
  template                       \
  <                              \
    DialectT::type D,            \
    bool            IsServer,    \
    typename        SessMgr,     \
    typename        Processor    \
  >                              \
  inline void \
  ProtoEngine<D, IsServer, SessMgr, Processor>::Parse##MsgType \
  ( \
    char*           a_msg_body,  \
    char const*     a_body_end   \
  ) \
  const \
  { \
    assert(a_msg_body != nullptr && a_msg_body < a_body_end); \
    auto& tmsg         = m_##MsgType;   \
    tmsg.Init();  \
    char*  curr        = a_msg_body;    \
    \
    __VA_ARGS__ ; \
    \
    while (curr < a_body_end) \
    { \
      int         tag = 0;    \
      char const* val = nullptr;                     \
      curr = ParseFld(curr, a_body_end, &tag, &val); \
      assert(val != nullptr); \
      \
      switch (tag) \
      { \
        case 34:   \
          GET_POS_VAL( tmsg.m_MsgSeqNum)    \
          break;   \
        case 43:   \
          GET_BOOL_VAL(tmsg.m_PossDupFlag)  \
          break;   \
        case 49:   \
          GET_CRD_VAL( tmsg.m_SenderCompID) \
          break;   \
        case 50:   \
          GET_CRD_VAL( tmsg.m_SenderSubID)  \
          break;   \
        case 52:   \
          tmsg.m_SendingTime     = DateTimeToTimeValFIX(val); \
          break;   \
        case 56:   \
          GET_CRD_VAL( tmsg.m_TargetCompID) \
          break;   \
        case 57:   \
          GET_CRD_VAL( tmsg.m_TargetSubID) \
          break;   \
        case 97:   \
          GET_BOOL_VAL(tmsg.m_PossResend)   \
          break;   \
        case 122:  \
          tmsg.m_OrigSendingTime = DateTimeToTimeValFIX(val); \
          break;   \
        \
        SpecificCases \
        \
        default: ; \
          CHECK_ONLY \
          ( \
            if (utxx::unlikely(m_sessMgr->m_debugLevel >= 4)) \
              m_sessMgr->m_logger->warn            \
                ("FIX::Parse" #MsgType ": Unrecognised Tag={}", tag); \
          ) \
      } \
    } \
  }

//----------------------------------------------------------------------------//
// "GET_MSG_COMMON":                                                          //
//----------------------------------------------------------------------------//
#ifdef  GET_MSG_COMMON
#undef  GET_MSG_COMMON
#endif

// XXX: UnTerminated "case" and "if" here!
// They are completed in GET_MSG and GET_SESS_MSG:
//
#define GET_MSG_COMMON(MsgType) \
  case MsgT::MsgType:           \
    Parse##MsgType(a_msg_body, a_body_end);  \
    m_##MsgType.m_MsgType   = MsgT::MsgType; \
    m_##MsgType.m_OrigMsgSz = a_total_len;   \
    if (utxx::likely            \
       (m_sessMgr != nullptr && \
       (!IsServer || \
        m_sessMgr->template CheckFIXSession<MsgT::MsgType>  \
                     (a_fd, &m_##MsgType, &a_curr_sess)) ))

//---------------------------------------------------------------------------//
// "GET_MSG":                                                                //
//---------------------------------------------------------------------------//
// Parse and Process the (Typed) Application-Level Msg:
// (*) for Server-Side, we invoke the Processor only if the FIXSession was suc-
//     cessfully verified for this Msg;
// (*) for Client-Side, "CheckFIXSession" is not called because there is only 1
//     Sesion anyway -- optimisation feature:
//
#ifdef  GET_MSG
#undef  GET_MSG
#endif

#define GET_MSG( MsgType) \
  GET_MSG_COMMON(MsgType) \
      m_processor->Process \
        (m_##MsgType, a_curr_sess, a_ts_recv, a_ts_handl); \
    return &m_##MsgType;

//---------------------------------------------------------------------------//
// "GET_SESS_MSG":                                                           //
//---------------------------------------------------------------------------//
// Similar to "GET_MSG" above, but for Session-Level Msgs which are processed
// by the SessMgr rather than  by the  Processor;  unlike the latter, SessMgr
// must always be non-NULL:
// NB: Unlike "GET_MSG", here we do NOT pass "handlTS" to "Process"  methods:
//
#ifdef  GET_SESS_MSG
#undef  GET_SESS_MSG
#endif

#define GET_SESS_MSG(MsgType) \
  GET_MSG_COMMON(MsgType)     \
      m_sessMgr->Process \
        (m_##MsgType, a_curr_sess, a_ts_recv); \
    return &m_##MsgType;

//===========================================================================//
namespace MAQUETTE
{
namespace FIX
{
  //=========================================================================//
  // Msg Sending Utils:                                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "soh" -- FIX Msg Fld Separator:                                         //
  //-------------------------------------------------------------------------//
  constexpr char soh = '\x01';

  //-------------------------------------------------------------------------//
  // "MkPrefixStr":                                                          //
  //-------------------------------------------------------------------------//
  // Generates the "8=...|" Fld:
  //
  constexpr char const* MkPrefixStr(ApplVerIDT a_ver)
  {
    switch (a_ver)
    {
      case ApplVerIDT::FIX27: return "8=FIX.2.7" SOH;
      case ApplVerIDT::FIX30: return "8=FIX.3.0" SOH;
      case ApplVerIDT::FIX40: return "8=FIX.4.0" SOH;
      case ApplVerIDT::FIX41: return "8=FIX.4.1" SOH;
      case ApplVerIDT::FIX42: return "8=FIX.4.2" SOH;
      case ApplVerIDT::FIX43: return "8=FIX.4.3" SOH;
      case ApplVerIDT::FIX44: return "8=FIX.4.4" SOH;
      default:
        // NB: In all other cases, the "trasport" layet is not the FIX protocol
        // itself, but FIXT.1.1:
        return "8=FIXT.1.1" SOH;
    }
  }

  //-------------------------------------------------------------------------//
  // "MsgCheckSum":                                                          //
  //-------------------------------------------------------------------------//
  // NB: This function is used by both Sending and Receiving sides:
  //
  inline int MsgCheckSum(char const* start, char const* end)
  {
    unsigned int cs = 0;
    for (char const* curr = start; curr < end; curr++)
      cs += unsigned(*curr);

    // Take the result modulo 256, ie the lowest-order byte:
    return int(cs & 0xff);
  }

  //-------------------------------------------------------------------------//
  // "CompleteMsg":                                                          //
  //-------------------------------------------------------------------------//
  // Completes the msg by installing CheckSum and MsgLength:
  // Returns the Total Length (incl the CheckSum fld and the final SOH):
  //
  inline int CompleteMsg(char*  a_buff,  char* a_msg_body,    char* a_curr)
  {
    assert(a_buff != nullptr && a_buff < a_msg_body && a_msg_body < a_curr);

    // MsgLen is counted between the Body start and the body end (NOT including
    // the last 10=... fld):
    int msgLen = int(a_curr - a_msg_body);
    assert(0 < msgLen && msgLen <= 8192);

    // Install "msgLen" into the "XXXX" placeholder:
    (void) utxx::itoa_right<int, 4, char>(a_msg_body - 5, msgLen, '0');

    // Now compute the checksum and install it:
    int cs = MsgCheckSum(a_buff, a_curr);
    assert(0 <= cs && cs <= 255);

    a_curr  = stpcpy(a_curr, "10=");
    (void) utxx::itoa_right<int, 3, char>(a_curr, cs, '0');
    a_curr += 3;
    *a_curr = soh;

    ++a_curr;         // Beyond the last char written
    * a_curr = '\0';  // For extra safety, place a terminator there
    return int(a_curr - a_buff);
  }

  //-------------------------------------------------------------------------//
  // "OutputDateFld": Incl Tag and SOH:                                      //
  //-------------------------------------------------------------------------//
  inline char* OutputDateFld
    (char const* a_tag, int a_year, int a_month, int a_day, char* a_curr)
  {
    assert(a_tag != nullptr     && a_curr != nullptr);

    if (utxx::likely(a_year > 0 && a_month > 0 &&  a_day > 0))
    {
      // XXX: No more checks for date validity are performed:
      a_curr  = stpcpy       (a_curr, a_tag);
      a_curr  = OutputDateFIX(a_year, a_month, a_day, a_curr);
      *a_curr = soh;
      ++a_curr;
    }
    return a_curr;
  }

  //=========================================================================//
  // Msg Reading Utils:                                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "ParseFld":                                                             //
  //-------------------------------------------------------------------------//
  // NB: This function 0-terminates the "a_curr" fld,  fills in its Tag  and
  // Value (the latter as a ptr to string value -- no type conversions), and
  // returns a ptr to the beginning of the next fld:
  //
  inline char* ParseFld
  (
    char*         a_curr,
    char const*   a_body_end,
    int*          a_tag,
    char const**  a_str_val
  )
  {
    assert(a_curr    != nullptr && a_curr < a_body_end && a_tag != nullptr &&
           a_str_val != nullptr);

    // Get the Tag: TillEOL=false, as the Tag ends with the '=':
    char const* eq = utxx::fast_atoi<int, false>(a_curr, a_body_end, *a_tag);
    CHECK_ONLY
    (
      if (utxx::unlikely(eq == nullptr || *eq != '=' || *a_tag <= 0))
        throw utxx::runtime_error("FIX::ParseFld: Invalid Tag: ", a_curr);
    )
    // Now find SOH and replace it with 0, to delimit the Value (and the Fld):
    char* val = const_cast <char*>(eq + 1);
    char* end = static_cast<char*>(memchr(val, soh, size_t(a_body_end - val)));
    CHECK_ONLY
    (
      if (end == nullptr || end == val)
        throw utxx::runtime_error("FIX::ParseFld: Invalid Val: ", a_curr);
    )
    *end       = '\0';
    *a_str_val = val;

    // Return a ptr beyond this fld:
    return (end + 1);
  }
} // End namespace FIX
} // End namespace MAQUETTE
