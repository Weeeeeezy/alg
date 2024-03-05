// vim:ts=2:et:sw=2
//===========================================================================//
//                               "EventTypes.h":                             //
//   Common Types for Exchanging Data between in Connectors and Strategies   //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_EVENTTYPES_H
#define MAQUETTE_STRATEGYADAPTOR_EVENTTYPES_H

#include "Common/DateTime.h"
#include "Connectors/OrderMgmtTypes.h"
#include <cstring>
#include <climits>
#include <array>
#include <cassert>

namespace Events
{
  //=========================================================================//
  // Integral Types:                                                         //
  //=========================================================================//
  using SecID     = unsigned long;    // SecID proper, short string or hash
  using SeqNum    = unsigned long;    // Sequence Number of a Trade
  using ClientRef = long;
  using OrderID   = MAQUETTE::OrderID;
  using HashT     = size_t;           // General hash value
  using AccCrypt  = unsigned long;    // Cryptographic Account ID/Key

  constexpr SecID     InvalidSecID     = ULONG_MAX;  // 0 can be valid!
  constexpr SeqNum    InvalidSecNum    = 0;
  constexpr ClientRef InvalidClientRef = -1;         // 0 usually valid
  constexpr OrderID   InvalidOrderID   = 0;
  constexpr HashT     InvalidHashT     = 0;
  constexpr AccCrypt  InvalidAccCrypt  = 0;

  //=========================================================================//
  // Fixed-Size Strings:                                                     //
  //=========================================================================//
  // XXX: They are represented as "std::array"s; cannot wrap them into a custom
  // class, or use a class derived from "std::array", as this would cause proh-
  // ibitive problems with ODB code generation:
  //
  //-------------------------------------------------------------------------//
  // Initialisation from a C string of known or unknown length:              //
  //-------------------------------------------------------------------------//
  // (KnownLen means that "strnlen" is not required):
  //
  template<size_t N, bool KnownLen=false>
  void InitFixedStrGen
    (std::array<char, N>* arr, char const* cstr, int len_bound = int(N)-1)
  {
    assert(arr != NULL && cstr != NULL && len_bound >= 0);

    // XXX: If "cstr" does not fit into the (N-1)-char format, it is truncated
    // (ie only the first N-1 chars are used);  if "len_bound" is given,  this
    // Init never looks beyond the "str_len" chars. NB: at least 1 byte is al-
    // ways reserved for 0-terminator:
    //
    int len = std::min<int>(int(N)-1, len_bound);
    if (!KnownLen)
      // Then "len_bound" really was an upper bound only, so need to get the
      // actual len dynamically (but still within the limits as above):
      len = strnlen(cstr, len);

    // Copy the data over:
    char*   targ = const_cast<char*>(arr->data());
    strncpy(targ,  cstr, len);

    // IMPORTANT: Do not allow any garbage at the end:
    // Zero-out the rest (there is always at least 1 byte set to 0, which will
    // act as a C string terminator):
    memset (targ + len,  '\0', N - len);
    assert (targ[N-1] == '\0');
  }

  //
  // As above, but in the form of an assignment:
  //
  template<size_t  N, bool KnownLen>
  std::array<char, N>
    MkFixedStrGen(char const* cstr, int len_bound = int(N)-1)
  {
    std::array<char, N> res;
    InitFixedStrGen<N, KnownLen>(&res, cstr, len_bound);
    return res;
  }

  //-------------------------------------------------------------------------//
  // Initialisation from "std::string":                                      //
  //-------------------------------------------------------------------------//
  // In this case, KnownLen=true: It is assumed that the string arg does NOT
  // contain any garbage:
  //
  template<size_t  N>
  void InitFixedStr(std::array<char, N>& arr, std::string const& str)
    { InitFixedStrGen<N, true>(&arr, str.c_str(), str.size()); }

  template<size_t  N>
  std::array<char, N> MkFixedStr(std::string const& str)
    { return MkFixedStrGen<N, true>(str.c_str(), str.size()); }

  //-------------------------------------------------------------------------//
  // Initialisation from a C Ptr:                                            //
  //-------------------------------------------------------------------------//
  // If the string length is unknown, the upper bound is set to (N-1):
  //
  template<size_t  N>
  void InitFixedStr
    (std::array<char, N>& arr, char const* cstr, int len_bound = int(N)-1)
    { InitFixedStrGen<N, false>(&arr, cstr, len_bound); }

  template<size_t  N>
  std::array<char, N> MkFixedStr(char const* cstr, int len_bound = int(N)-1)
    { return MkFixedStrGen<N, false>(cstr, len_bound); }

  //-------------------------------------------------------------------------//
  // Initialisation from char[M]:                                            //
  //-------------------------------------------------------------------------//
  // KnownLen = false (will need a dynamic "strnlen") as there could be garba-
  // ge at the end of the input buffer. NB: "buff" does NOT need to be 0-term-
  // inated; the constructed "arr" will always be:
  //
  template<size_t  N, size_t M>
  void InitFixedStr(std::array<char, N>& arr, char const (&buff)[M])
    { InitFixedStrGen<N, false>(arr, buff, M);   }

  template<size_t  N, size_t M>
  std::array<char, N> MkFixedStr(char const (&buff)[M])
    { return MkFixedStrGen<N, false>(buff, M); }

  //-------------------------------------------------------------------------//
  // Conversion to "std::string":                                            //
  //-------------------------------------------------------------------------//
  template<size_t  N>
  std::string ToString(std::array<char, N> const& arr, char trim = '\0')
  {
    // Drop the trailing 0s (if any):
    // By our construction, the last byte of "data" should be a 0-terminator,
    // but be conservative -- "data" may have been altered by direct access,
    // so enforce the terminator explicitly:
    // NB: The resulting string can be shorter than (N-1); any trailing garbage
    // (though unlikely) is NOT copied into "str" (XXX: but NOT removed from
    // "arr" in this case):
    //
    const char* p = arr.data(), *end = p + N;
    for (; p != end && *p && *p != trim; ++p);
    return std::string  (arr.data(), p - arr.data());
  }

  //-------------------------------------------------------------------------//
  // Conversion to C Strings (mutable or non-mutable):                       //
  //-------------------------------------------------------------------------//
  template<size_t N>
  char const* ToCStr(std::array<char, N> const& arr)
  {
    // XXX: In this case, we may need to alter the "arr" to re-inforce the
    // 0-terminator; if there is an earlier 0-terminator already,  and the
    // garbage beyond it, the latter is NOT removed:
    //
    char* str = const_cast<char*>(arr.data());
    str[N-1]  = '\0';
    return arr.data();
  }

  // XXX: DANGEROUS: returns a mutable C String ptr, so the arg must not be
  // "const" either:
  template<size_t N>
  char* ToCStr(std::array<char, N>* arr)
  {
    assert(arr != NULL);
    // Same policy on 0-termination and traling garbage as above:
    char*  str = const_cast<char*>(arr->data());
    str[N-1]   = '\0';
    return str;
  }

  //-------------------------------------------------------------------------//
  // "IsEmpty" Test:                                                         //
  //-------------------------------------------------------------------------//
  template<size_t N>
  bool IsEmpty(std::array<char, N> const& arr)
    { return *(arr.data()) == '\0'; }

  //-------------------------------------------------------------------------//
  // "SymKey":                                                               //
  //-------------------------------------------------------------------------//
  // Up to 15 data chars, sufficient in most cases:
  //
  constexpr size_t SymKeySz = 16;
  using     SymKey = std::array<char, SymKeySz>;

  // All 0s
  inline SymKey const& EmptySymKey() { static SymKey s_key; return s_key; }

  inline SymKey MkSymKey(std::string const& a_str)
  {
    SymKey k; auto p = utxx::copy(k.data(), SymKeySz, a_str);
    memset(p, '\0', k.data() + SymKeySz - p); // Zero-out trailing spaces
    return k;
  }

  //=========================================================================//
  // Specific Fixed-Size Strings Instances:                                  //
  //=========================================================================//
  // XXX: Still need to use raw "array" types for ODB code generation, since
  // ODB does not know how to handle "FixedStr"...

  //-------------------------------------------------------------------------//
  // "Ccy": ISO 4217 3-letter Currency Code:                                 //
  //-------------------------------------------------------------------------//
  constexpr size_t CcySz = 4;
  using     Ccy    = std::array<char, CcySz>; // Still has 1 byte for 0-terminator!
  inline Ccy const&  EmptyCcy() { static Ccy s_ccy; return s_ccy; }  // All 0s

  inline Ccy MkCcy(std::string const& a_str)
    { return MkFixedStr<CcySz>(a_str); }

  //-------------------------------------------------------------------------//
  // "SymKey":                                                               //
  //-------------------------------------------------------------------------//
  // Up to 15 data chars, sufficient in most cases:
  //

  //-------------------------------------------------------------------------//
  // "ObjName":                                                              //
  //-------------------------------------------------------------------------//
  // Some persistent object name (eg Connector or Strategy name) -- up to 63
  // data chars:
  //
  constexpr size_t ObjNameSz = 64;
  typedef std::array<char, ObjNameSz> ObjName;
  ObjName const& EmptyObjName();      // All 0s

  inline ObjName MkObjName(std::string const& str)
    { return MkFixedStr<ObjNameSz>(str); }

  //-------------------------------------------------------------------------//
  // "ErrTxt":                                                               //
  //-------------------------------------------------------------------------//
  // Text of an error msg communicated from Connector back to the Client. Up to
  // 255 data bytes are allowed:
  //
  constexpr size_t ErrTxtSz  = 256;
  typedef std::array<char, ErrTxtSz>  ErrTxt;
  ErrTxt const&  EmptyErrTxt();       // All 0s

  inline ErrTxt MkErrTxt(std::string const& str)
    { return MkFixedStr<ErrTxtSz>(str); }

  //=========================================================================//
  // ODB Mappings Support:                                                   //
  //=========================================================================//
# ifdef ODB_COMPILER
  // XXX: See the definitions of these types in "StrategyAdaptor/EventTypes.h".
  // The sizes must be kept consistent MANUALLY!!!
  //
# pragma db value (Events::Ccy)        type("VARCHAR(3)")
# pragma db value (Events::ObjName)    type("VARCHAR(63)")
# pragma db value (Events::ErrTxt)     type("VARCHAR(255)")
# pragma db value (Arbalete::DateTime) type("DATETIME(6)")
# endif  // ODB_COMPILER
}
#endif  // MAQUETTE_STRATEGYADAPTOR_EVENTTYPES_H
