// vim:ts=2:et
//===========================================================================//
//                             "Basis/BaseTypes.hpp":                        //
//                      Common Types for MAQUETTE Componets                  //
//===========================================================================//
#pragma  once

#include "Basis/Macros.h"
#include "Basis/Maths.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/convert.hpp>
#include <utxx/enumv.hpp>
#include <iostream>
#include <utility>
#include <cstddef>
#include <cstring>
#include <climits>
#include <array>
#include <type_traits>
#include <cassert>

namespace MAQUETTE
{
  //=========================================================================//
  // Most Important Types:                                                   //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // The Most Important Type: The Empty Type:                                //
  //-------------------------------------------------------------------------//
  struct EmptyT {};

  //-------------------------------------------------------------------------//
  // "MQTEnvT": The Environment We Are Working In:                           //
  //-------------------------------------------------------------------------//
  UTXX_ENUMV(
  MQTEnvT,  (int, 0),
      (Prod, 1)     // Production
      (Test, 2)     // Test or UAT
      (Hist, 3)     // "Paper" env (on historical data, not real-time)
  );

  //-------------------------------------------------------------------------//
  // Fuzzy Boolean:                                                          //
  //-------------------------------------------------------------------------//
  enum class FuzzyBool
  {
    UNDEFINED = -1,
    False     =  0,
    True      =  1
  };

  //-------------------------------------------------------------------------//
  // PlaceHolders for Template MetaProgramming:                              //
  //-------------------------------------------------------------------------//
  // "BoolPlaceHolder" used to infer Boolean Template Params:
  template<bool BoolParam>
  struct BoolPlaceHolder{};

  // Any other PlaceHolder: Used to infer type T:
  template<typename T>
  struct PlaceHolder{};

  //=========================================================================//
  // Integral Types:                                                         //
  //=========================================================================//
  // NB: "SeqNum"s are signed because we need to do arithmetics with them:
  using SecID   = unsigned long;  // SecID proper, short string or hash
  using SeqNum  = long;           // Sequence Number of a Msg;  Signed!
  using OrderID = unsigned long;
  using UserID  = unsigned;

  //=========================================================================//
  // Fixed-Size Strings:                                                     //
  //=========================================================================//
  // XXX: They are represented as "std::array"s; cannot wrap them into a custom
  // class, or use a class derived from "std::array",  as this would cause pro-
  // hibitive problems with ODB code generation   (which we currently don't do,
  // anyway...):
  //-------------------------------------------------------------------------//
  // Templated version of "strncpy":                                         //
  //-------------------------------------------------------------------------//
  // "ForceZ": always 0-terminate the dest;
  // "N"     : dest size
  // "M"     : max  src size to be used (-1 if auto-decided):
  // Returns the number of bytes actuall written into "dest" (including the 0-
  //         terminator if applicable):
  //
  template<bool ForceZ, int M = -1, int N>
  inline void StrNCpy(char (&a_dest)[N], char const* a_src)
  {
    static_assert(N > 0, "StrNCpy: N");
    assert(a_src != nullptr);

    // "L" is the max number of chars to be copied from "src" to "dest" before
    //     possible padding:
    // M < 0 means that "M" is not specified, so use "D";
    //       otherwise, use min(M,D);
    // "D" is N-1 if zero-termination is required, and "N" otherwise:
    //
    constexpr int D = ForceZ ? (N-1) : N;
    static_assert(D >= 0, "StrNCpy: D");

    constexpr int L = (M < 0 || M > D) ? D : M;
    static_assert(0 <= L  && L <= D && (M < 0 || L <= M), "StrNCpy: L");

    int i = 0;
    for (; i < L; ++i)
    {
      char c = a_src[i];
      a_dest[i] = c;
      // If a 0-byte was wriiten, there is nothing to do anymore: For efficien-
      // cy, unlike the standard "strncpy", we do NOT 0-pad the tail:
      if (c == '\0')
        return;
    }
    // If we got here, we wrote i==L non-0 bytes. If "ForceZ" flag is set, inst-
    // all the 0-terminator (there must be room for it):
    assert(i == L);
    if (ForceZ)
    {
      assert(i < N);
      a_dest[i] = '\0';
    }
  }

  //-------------------------------------------------------------------------//
  // Zeroing-Out:                                                            //
  //-------------------------------------------------------------------------//
  template<size_t N>
  inline void ZeroOut(std::array<char, N>* a_arr)
  {
    assert(a_arr != nullptr);
    memset(a_arr->data(), '\0', N);
  }

  //-------------------------------------------------------------------------//
  // Initialisation from a C string of known or unknown length:              //
  //-------------------------------------------------------------------------//
  // (KnownLen means that "strnlen" is not required):
  //
  template<size_t N, bool KnownLen=false>
  inline void InitFixedStrGen
  (
    std::array<char, N>* a_arr,
    char const*          a_cstr      = nullptr,   // NULL: empty
    size_t               a_len_bound = N-1
  )
  {
    static_assert(N > 0, "InitFixedStrGen");
    assert(a_arr != nullptr);

    // "len" is an upper bound for the number of non-0 chars to be copied over;
    // we initialize it to 0 because the src string "a_cstr" may be NULL:
    size_t len    = 0;
    char*  targ   = const_cast<char*>(a_arr->data());
    assert(targ  != nullptr);
    char*  tLimit = targ + N;

    if (utxx::likely(a_cstr != nullptr))
    {
      // Need to copy some data over:
      // NB: If "a_cstr" does not fit into the (N-1)-char format, it is trunca-
      // ted (ie only the first N-1 chars are used);   if "len_bound" is given,
      // we will never look beyond that number of chars. NB: at least 1 byte is
      // always reserved for the 0-terminator:
      len = std::min<size_t>(N-1, a_len_bound);

      // Copy the data over; use "stpncpy" to get the actual end of writing and
      // and fill the remaining bytes with 0s:
      char*  end  = stpncpy(targ, a_cstr, len);
      assert(end != nullptr  && targ <= end && end <= tLimit);
      memset(end, '\0', size_t(tLimit - end));
    }
    else
      // No data to copy -- zero-out the whole "targ":
      memset(targ, '\0', N);

    // For extra safety, explicitly 0-terminate the "targ" even if "memset" did
    // not do it:
    assert(len <= N-1);
    targ[len] = '\0';
  }

  //-------------------------------------------------------------------------//
  // As above, but in the form of an assignment:                             //
  //-------------------------------------------------------------------------//
  template<size_t N, bool KnownLen>
  inline std::array<char, N> MkFixedStrGen
  (
    char const* a_cstr      = nullptr,            // NULL: empty
    size_t      a_len_bound = N-1)
  {
    static_assert   (N > 0, "MkFixedStrGen");
    std::array<char, N> res;
    InitFixedStrGen <N, KnownLen>(&res, a_cstr, a_len_bound);
    return res;
  }

  //-------------------------------------------------------------------------//
  // Initialisation from "std::string":                                      //
  //-------------------------------------------------------------------------//
  // In this case, KnownLen=true: It is assumed that the string arg does NOT
  // contain any garbage:
  //
  template<size_t N>
  inline void InitFixedStr(std::array<char, N>* a_arr, std::string const& a_str)
    { InitFixedStrGen<N, true>(a_arr, a_str.data(), a_str.size()); }

  template<size_t N>
  inline std::array<char, N> MkFixedStr(std::string const& a_str)
    { return MkFixedStrGen<N, true>(a_str.data(),   a_str.size()); }

  //-------------------------------------------------------------------------//
  // Initialisation from a C Ptr:                                            //
  //-------------------------------------------------------------------------//
  // If the string length is unknown, the upper bound is set to (N-1):
  //
  template<size_t  N>
  inline void InitFixedStr
  (
    std::array<char, N>* a_arr,
    char const*          a_cstr      = nullptr,
    size_t               a_len_bound = N-1
  )
  { InitFixedStrGen<N, false>(a_arr, a_cstr, a_len_bound); }

  template<size_t  N>
  inline std::array<char, N> MkFixedStr
  (
    char const*          a_cstr      = nullptr,
    size_t               a_len_bound = N-1
  )
  { return MkFixedStrGen<N, false>(a_cstr,   a_len_bound); }

  //-------------------------------------------------------------------------//
  // Initialisation from char[M]:                                            //
  //-------------------------------------------------------------------------//
  // KnownLen = false (will need a dynamic "strnlen") as there could be garba-
  // ge at the end of the input buffer. NB: "buff" does NOT need to be 0-term-
  // inated; the constructed "arr" will always be:
  //
  template<size_t  N, size_t M>
  inline void InitFixedStr(std::array<char, N>* a_arr, char const (&a_buff)[M])
    { InitFixedStrGen<N, false>(a_arr, a_buff, M);   }

  template<size_t  N, size_t M>
  inline std::array<char, N> MkFixedStr(char const (&a_buff)[M])
    { return MkFixedStrGen<N, false>  (a_buff, M);   }

  //-------------------------------------------------------------------------//
  // Initialisation from another "std::array<char,M>":                       //
  //-------------------------------------------------------------------------//
  // Similar to the C Ptr case above because the actual length of "a_src" is
  // not known (it may potentially contain trailing garbage):
  //
  template<size_t  N, size_t M>
  inline void InitFixedStr
  (
    std::array<char, N>*       a_arr,
    std::array<char, M> const& a_src
  )
  { InitFixedStrGen<N, false>(a_arr, a_src.data(), M); }

  template<size_t  N, size_t M>
  inline std::array<char, N> MkFixedStr(std::array<char, M> const& a_src)
  { return MkFixedStrGen<N, false>(a_src.data(),   M); }

  //-------------------------------------------------------------------------//
  // Conversion to "std::string":                                            //
  //-------------------------------------------------------------------------//
  template<size_t  N>
  inline std::string ToString
  (
    std::array<char, N> const& a_arr,
    char                       a_trim = '\0'
  )
  {
    // Drop the trailing 0s (if any):
    // By our construction, the last byte of "data" should be a 0-terminator,
    // but be conservative -- "data" may have been altered by direct access,
    // so enforce the terminator explicitly:
    // NB: The resulting string can be shorter than (N-1); any trailing garbage
    // (though unlikely) is NOT copied into "str" (XXX: but NOT removed from
    // "arr" in this case):
    //
    char const* p = a_arr.data(), *end = p + N;
    for (; p != end && *p && *p != a_trim; ++p);
    return std::string  (a_arr.data(), size_t(p - a_arr.data()));
  }

  //-------------------------------------------------------------------------//
  // Conversion to C Strings (mutable or non-mutable):                       //
  //-------------------------------------------------------------------------//
  template<size_t N>
  inline char const* ToCStr(std::array<char, N> const& a_arr)
  {
    // XXX: In this case, we may need to alter the "arr" to re-inforce the
    // 0-terminator; if there is an earlier 0-terminator already,  and the
    // garbage beyond it, the latter is NOT removed:
    //
    char* str = const_cast<char*>(a_arr.data());
    str[N-1]  = '\0';
    return a_arr.data();
  }

  // XXX: DANGEROUS: returns a mutable C String ptr, so the arg must not be
  // "const" either:
  template<size_t N>
  inline char* ToCStr(std::array<char, N>* a_arr)
  {
    assert(a_arr != nullptr);
    // Same policy on 0-termination and traling garbage as above:
    char*  str = const_cast<char*>(a_arr->data());
    str[N-1]   = '\0';
    return str;
  }

  //-------------------------------------------------------------------------//
  // "IsEmpty" Test, "Length":                                               //
  //-------------------------------------------------------------------------//
  template<size_t N>
  inline bool IsEmpty(std::array<char, N> const& a_arr)
    { return *(a_arr.data()) == '\0'; }

  template<size_t N>
  inline int  Length (std::array<char, N> const& a_arr)
  {
    size_t len = strlen(a_arr.data());
    assert(len < N);  // At least 1 byte reserved for '\0'
    return int(len);
  }

  //-------------------------------------------------------------------------//
  // "IsEQ" Test:                                                            //
  //-------------------------------------------------------------------------//
  // C-style comparison (

  //=========================================================================//
  // Specific Fixed-Size Strings Instances:                                  //
  //=========================================================================//
  // NB:  Because there are currently no class wrappers around the following
  // array types, our default ctors cannot be specified; USE "= {0}" INITIAL-
  // ISERS to create empty objects!

  //-------------------------------------------------------------------------//
  // "Ccy":                                                                  //
  //-------------------------------------------------------------------------//
  // XXX: Originally,  it used to be ISO 4217 3-letter Currency Code.
  // With the advent of Crypto-Ccys, the size has been extended to 7 symbols +
  // the 0-terminator, so 8 bytes total:
  //
  constexpr size_t CcySz = 8;
  using  Ccy     = std::array<char, CcySz>;

  inline Ccy MkCcy(std::string const& a_str)
    { return MkFixedStr<CcySz>(a_str); }

  inline Ccy MkCcy(char const* a_str = nullptr)
    { return MkFixedStr<CcySz>(a_str); }

  //-------------------------------------------------------------------------//
  // "SymKey":                                                               //
  //-------------------------------------------------------------------------//
  // Up to 15 data chars, sufficient in most cases for Exchange Symbols etc. In
  // particular, any Symbol of the form "CcyA/CcyB" would fit into this fmt:
  //
  constexpr size_t    SymKeySz = 16;
  using  SymKey     = std::array<char, SymKeySz>;

  inline SymKey MkSymKey(std::string const& a_str)
    { return MkFixedStr<SymKeySz>(a_str); }

  inline SymKey MkSymKey(char const* a_str = nullptr)
    { return MkFixedStr<SymKeySz>(a_str); }

  //-------------------------------------------------------------------------//
  // "Credential":                                                           //
  //-------------------------------------------------------------------------//
  // Up to 47 data chars, for CompIDs etc (SymKey may sometimes be insufficient
  // for that). XXX: Cannot use Size=36, there are examples exceeding it!
  //
  constexpr size_t    CredentialSz = 48;
  using  Credential = std::array<char, CredentialSz>;

  inline Credential MkCredential(std::string const& a_str)
    { return MkFixedStr<CredentialSz>(a_str); }

  inline Credential MkCredential(char const* a_str = nullptr)
    { return MkFixedStr<CredentialSz>(a_str); }

  //-------------------------------------------------------------------------//
  // "ObjName":                                                              //
  //-------------------------------------------------------------------------//
  // Some persistent object name (eg Connector or Strategy name) -- up to 63
  // data chars:
  //
  constexpr size_t     ObjNameSz = 64;
  using  ObjName     = std::array<char, ObjNameSz>;

  inline ObjName MkObjName(std::string const& a_str)
    { return MkFixedStr<ObjNameSz>(a_str); }

  inline ObjName MkObjName(char const* a_str = nullptr)
    { return MkFixedStr<ObjNameSz>(a_str); }

  //-------------------------------------------------------------------------//
  // "ErrTxt":                                                               //
  //-------------------------------------------------------------------------//
  // Text of an error msg communicated from Connector back to the Client. Up to
  // 255 data bytes are allowed:
  //
  constexpr size_t ErrTxtSz  = 256;
  using  ErrTxt     = std::array<char, ErrTxtSz>;

  inline ErrTxt MkErrTxt(std::string const& a_str)
    { return MkFixedStr<ErrTxtSz>(a_str); }

  inline ErrTxt MkErrTxt(char const* a_str = nullptr)
    { return MkFixedStr<ErrTxtSz>(a_str); }

  //-------------------------------------------------------------------------//
  // "LiteralEnum":                                                          //
  //-------------------------------------------------------------------------//
  // Allows us to create enums with multi-char encodings (actually encoded as
  // uint32_t, hence up to 4 chars can be used):
  // LiteralEnum('q','w','e') == LiteralEnum("qwe") == LiteralEnum("qwe",3)
  // length-as-param version is for dynamic use:
  //
  inline constexpr uint32_t LiteralEnum(char a)
    { return static_cast<uint32_t>(a);  }

  inline constexpr uint32_t LiteralEnum(char a, char b)
    { return LiteralEnum(a)       | (LiteralEnum(b) << 8);  }

  inline constexpr uint32_t LiteralEnum(char a, char b, char c)
    { return LiteralEnum(a, b)    | (LiteralEnum(c) << 16); }

  inline constexpr uint32_t LiteralEnum(char a, char b, char c, char d)
    { return LiteralEnum(a, b, c) | (LiteralEnum(d) << 24); }

  template
  <
    int     Length,
    class = typename std::enable_if<2 <= Length && Length <= 5>::type
  >
  inline constexpr uint32_t LiteralEnum(char const (&a_string)[Length])
  {
    switch (Length)
    {
      case 2:  return LiteralEnum(a_string[0]);
      case 3:  return LiteralEnum(a_string[0], a_string[1]);
      case 4:  return LiteralEnum(a_string[0], a_string[1], a_string[2]);
      case 5:  return LiteralEnum(a_string[0], a_string[1], a_string[2],
                                  a_string[3]);
    }
  }

  constexpr uint32_t InvalidLiteralEnumValue = 0;

  inline uint32_t LiteralEnum(char const* a_string, int a_length)
  {
    switch (a_length)
    {
      case 1:  return LiteralEnum(a_string[0]);
      case 2:  return LiteralEnum(a_string[0], a_string[1]);
      case 3:  return LiteralEnum(a_string[0], a_string[1], a_string[2]);
      case 4:  return LiteralEnum(a_string[0], a_string[1], a_string[2],
                                  a_string[3]);
      default: return InvalidLiteralEnumValue;
    }
  }

  //=========================================================================//
  // "ExchOrdID" Class:                                                      //
  //=========================================================================//
  // Exchange-Assigned OrderID: Can be a String or an unsigned long (OrderID);
  // in the latter case, we still keep its stringified form for output:
  //
  class ExchOrdID
  {
  public:
    //-----------------------------------------------------------------------//
    // Size for the String rep of "ExchOrdID":                               //
    //-----------------------------------------------------------------------//
    constexpr static size_t StrSz = 64;

  private:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    // XXX: This object is 40 bytes long. This is not ideal, but acceptable:
    //
    OrderID m_i;        // 0 if this ID is not numerical
    char    m_s[StrSz]; // Always '\0'-terminated

  public:
    //-----------------------------------------------------------------------//
    // Ctors:                                                                //
    //-----------------------------------------------------------------------//
    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    // Zeroes-out the object (NB: "IsNum" will be "false"). The result is the
    // same as constructing "ExchOrdID" from  an empty str (see below):
    //
    ExchOrdID(): m_i(0) { memset(m_s, '\0', StrSz); }

    //-----------------------------------------------------------------------//
    // Constructing from an OrderID:                                         //
    //-----------------------------------------------------------------------//
    explicit ExchOrdID(OrderID a_val)
    : m_i    (a_val)
    {
      if (m_i != 0)
        // This is a real non-0 OrdID, so the "ExchOrdID" will be treated as a
        // Number -- install its string rep as well (XXX: minor inefficiency):
        (void) utxx::itoa_left(m_s, a_val);
      else
        // This is not a valid OrdID, so the "ExchOrdID" will be of string type
        // and will be empty -- must install an empty string as well:
        m_s[0] = '\0';
    }

    //-----------------------------------------------------------------------//
    // Constructing from C-Strings:                                          //
    //-----------------------------------------------------------------------//
    explicit ExchOrdID(char const* a_str, int a_N)
    {
      // XXX: On MICEX, there is a special value "NONE" -- treat it as empty:
      if (utxx::unlikely
         (a_str == nullptr || a_N == 0 || *a_str == '\0' ||
         (a_N   >= 5       && strcmp(a_str, "NONE") == 0)))
      {
        // This ExchOrdID is of string type, and is empty. Zero it out complet-
        // ely:
        m_i    = 0;
        memset(m_s, '\0', StrSz);
        return;
      }
      // Generic Case:
      // NB: "a_str" may actually represent a number, so try "atoi"  (TillEOL =
      // false); the latter flag means we traverse the string until '\0' (which
      // should be the 1st non-numeric char), but do not have to traverse all N
      // bytes:
      char const* limit  = a_str + a_N;
      char const* end    = utxx::fast_atoi<OrderID, false>(a_str, limit, m_i);

      // "sEnd" is for the target ("m_s"): the first unusied byte:
      char*       sLimit = m_s   + StrSz;
      char*       sEnd   = nullptr;

      // Because TillEOL was "false", "end" should always be non-NULL:
      assert(end != nullptr && a_str <= end && end <= limit);

      if (end == limit || *end == '\0')
      {
        // Yes, it was a digits-only string, non-0, so actually a Number. But it
        // may still be empty:
        if (utxx::unlikely(m_i == 0 || end == a_str))
        {
          m_i    = 0; // Assert it once again
          m_s[0] = '\0';
          return;
        }
        // Otherwise: A valid number. Its stringified rep should always fit in
        // "m_s" because "m_s" is large enough to hold any number,   but still
        // limit it explicitly:
        size_t  len = std::min<size_t>(size_t(end - a_str), StrSz);

        // NB: Use "stpncpy", so we know where we stopped:
        sEnd = stpncpy(m_s, a_str, len);
      }
      else
      {
        // No, it is just a generic string (or we got m_i==0, so this ExchOrdID
        // is empty):
        m_i = 0;

        // Copy the string over   (XXX: silently truncate if it does not fit in
        // "m_s", though this should not happen):
        // Again, use "stpncpy" to ensure that the unused part of "m_s" is fil-
        // led with 0s:
        sEnd = stpncpy(m_s, a_str, std::min<size_t>(size_t(a_N), StrSz));
      }
      // IMPORTANT: Zero-out any garbage at the end of "m_s":
      assert(sEnd != nullptr  &&  m_s <= sEnd  &&  sEnd <= sLimit);
      memset(sEnd, '\0', size_t(sLimit - sEnd));

      // For extra safety, explicitly 0-terminate "m_s" even if "memset" above
      // did not write anything:
      m_s[StrSz-1] = '\0';
    }

    //-----------------------------------------------------------------------//
    // Constructing from static char arrays:                                 //
    //-----------------------------------------------------------------------//
    template<int    N>
    explicit ExchOrdID(char const (&a)[N])
    : ExchOrdID(&(a[0]),          N)  {}

    template<size_t N>
    explicit ExchOrdID(std::array<char, N> const& a_arr)
    : ExchOrdID(a_arr.data(), int(N)) {}

    // Dtor is trivial

    //-----------------------------------------------------------------------//
    // Copying:                                                              //
    //-----------------------------------------------------------------------//
    // Copy Ctor and Assignment Operator are auto-generated:
    ExchOrdID           (ExchOrdID const&) = default;
    ExchOrdID& operator=(ExchOrdID const&) = default;

    //-----------------------------------------------------------------------//
    // Equality:                                                             //
    //-----------------------------------------------------------------------//
    bool operator==     (ExchOrdID const& a_rhs) const
    {
      // Positive if:
      // (*) Both numeric IDs are equal and non-0 (so really numeric);
      // (*) both numeric IDs are 0 and the strings are equal (incl empty):
      return
        (m_i != 0)
        ? (m_i == a_rhs.m_i)
        : (a_rhs.m_i == 0 && strcmp(m_s, a_rhs.m_s) == 0);
    }

    bool operator!=     (ExchOrdID const& a_rhs) const
      { return !(*this == a_rhs); }

    //-----------------------------------------------------------------------//
    // Emptiness Test:                                                       //
    //-----------------------------------------------------------------------//
    // The emptiness condition is m_s[0] == '\0', which implies m_i==0:
    bool IsEmpty() const
    {
      assert (m_s[0] != '\0' || m_i == 0);
      return (m_s[0] == '\0');
    }

    //-----------------------------------------------------------------------//
    // "ToString":                                                           //
    //-----------------------------------------------------------------------//
    constexpr char const* ToString() const { return m_s; }

    //-----------------------------------------------------------------------//
    // "GetOrderID" (if not a number, returns 0):                            //
    //-----------------------------------------------------------------------//
    constexpr bool    IsNum()        const { return (m_i != 0); }
    constexpr OrderID GetOrderID()   const { return  m_i;       }
  };

  //=========================================================================//
  // String Comparators:                                                     //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "CStrLessT": Comparison of CStrings:                                    //
  //-------------------------------------------------------------------------//
  // "<" for C Strings: We deliberately use this comparator instead of using
  // "std::string", in order to avoid memory allocations:
  //
  struct CStrLessT
  {
    bool operator()(char const* a_left, char const* a_right) const
      { return (strcmp(a_left, a_right) < 0); }
  };

  //-------------------------------------------------------------------------//
  // "FStrLessT": Comparison of Fixed Strings:                               //
  //-------------------------------------------------------------------------//
  template<size_t N>
  struct FStrLessT
  {
    bool operator()(std::array<char, N> const& a_left,
                    std::array<char, N> const& a_right) const
      { return (strncmp(a_left.data(), a_right.data(), N) < 0); }
  };

  //-------------------------------------------------------------------------//
  // Simple comparison of Fixed Strings for Equality:                        //
  //-------------------------------------------------------------------------//
  template<size_t N>
  bool IsEQ(std::array<char, N> const& a_left,
            std::array<char, N> const& a_right)
    { return (strncmp(a_left.data(), a_right.data(), N) == 0);  }

  //-------------------------------------------------------------------------//
  // "EStrLessT": Comparison of "ExchOrdID"s:                                //
  //-------------------------------------------------------------------------//
  struct EStrLessT
  {
    bool operator()(ExchOrdID const& a_left, ExchOrdID const& a_right) const
      { return (strcmp(a_left.ToString(), a_right.ToString()) < 0); }
  };

  //=========================================================================//
  // "UserData" (to be stored in various Control Blocks):                    //
  //=========================================================================//
  // XXX: This is just a 64-byte placeholder with "double" and "long" align-
  // ment. The user would typically implement a specific type  and cast its
  // ptr from/into UserData ptr:
  //
  struct UserData
  {
    // Alignment:
    double        m_alignD  [0];
    long          m_alignL  [0];
    // Data Space:
    mutable char  m_userData[64];

    // Default Ctor:
    UserData() { memset(this, '\0', sizeof(UserData)); }

    // Generic Setter:
    template<typename T>
    void Set(T const& a_user_data)
    {
      static_assert(sizeof(T) <= sizeof(UserData),
                    "UserData::Set: Arg type size too large");
      // NB: The following is required to avoid strict-aliasing warning in GCC:
      char* userData = m_userData;

      *(reinterpret_cast<T*>(userData)) = a_user_data;
    }

    // Generic Getter (XXX: Beware: there is no enforcement for getting same
    // type as the one which was Set!);
    // NB: although this method rerurns a NON-CONST ref, it is declated "const"
    // because "m_userData" is "mutable":
    template<typename T>
    T& Get() const
    {
      static_assert(sizeof(T) <= sizeof(UserData),
                   "UserData::Get Res type size too large");
      // NB: The following is required to avoid strict-aliasing warning in GCC:
      char* userData = m_userData;

      return *(reinterpret_cast<T*>(userData));
    }
  };

  //=========================================================================//
  // Empty Objs of the Above Types:                                          //
  //=========================================================================//
  // XXX: "extern const" is formally an undefined behaviour in C++ (???):
  extern Ccy        const EmptyCcy;
  extern SymKey     const EmptySymKey;
  extern Credential const EmptyCredential;
  extern ObjName    const EmptyObjName;
  extern ErrTxt     const EmptyErrTxt;
  extern ExchOrdID  const EmptyExchOrdID;

  //=========================================================================//
  // Length of ConstExpr String Literals:                                    //
  //=========================================================================//
  template<int Len>
  constexpr int ConstStrLen(char const (&)[Len])
  {
    static_assert(Len >= 1, "ConstStrLen");  // Because includes terminating 0
    return (Len - 1);
  }

  constexpr int ConstStrLen(char const* a_str)
    { return (*a_str == '\0') ? 0 : ConstStrLen(a_str + 1) + 1; }

  //=========================================================================//
  // Types Borrowed from the FIX Protocol:                                   //
  //=========================================================================//
  // The following types are so fundamental that we use them for the abstract
  // Order Mgmt as well, hence XXX they are specified here at top level:
  //
  namespace FIX
  {
    //-----------------------------------------------------------------------//
    // "SideT":                                                              //
    //-----------------------------------------------------------------------//
    enum class SideT            // Encoded as "char"
    {
      UNDEFINED       = '\0',
      Buy             = '1',
      Sell            = '2'
    };

    //-----------------------------------------------------------------------//
    // "OrderTypeT":                                                         //
    //-----------------------------------------------------------------------//
    enum class OrderTypeT       // Encoded as "char"
    {
      UNDEFINED             = '\0',
      Market                = '1',
      Limit                 = '2',
      Stop                  = '3',
      StopLimit             = '4',
      // NB: '5' is not used anymore!
      WithOrWithout         = '6',
      LimitWithOrWO         = '8',
      OnBasis               = '9',
      PrevQuoted            = 'D',
      PrevIndicated         = 'E',
      ForexSwap             = 'G',
      Funari                = 'I',
      MarketIfTouched       = 'J',
      MarketWithLeftOver    = 'K',
      PrevFundValPoint      = 'L',
      NextFundValPoint      = 'M',
      Pegged                = 'P',
      CounterOrderSelection = 'Q',
      WVAP                  = 'W'
    };

    //------------------------------------------------------------------------//
    // "TimeInForceT":                                                        //
    //------------------------------------------------------------------------//
    enum class TimeInForceT         // Encoded as "char"
    {
      UNDEFINED           = '\0',
      Day                 = '0',
      GoodTillCancel      = '1',    // Good Till Cancel
      AtTheOpening        = '2',
      ImmedOrCancel       = '3',    // aka "Fill & Kill"
      FillOrKill          = '4',
      GoodTillX           = '5',    // Good Till Crossing
      GoodTillDate        = '6',
      AtTheClose          = '7'
    };
  }

  //-------------------------------------------------------------------------//
  // Types of Linear (Delta1) Financial Instruments:                         //
  //-------------------------------------------------------------------------//
  // XXX: Non-Linear Instruments (such as Options) require a separate -- and a
  // much more complex -- typology:
  //
  enum Delta1T
  {
    Spot    = 0,
    Futures = 1,
    Margin  = 2,
    Swap    = 3
  };

  //=========================================================================//
  // Misc:                                                                   //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "Delete0", "DeleteArr0":                                                //
  //-------------------------------------------------------------------------//
  // De-allocation with NULLifying the corresp ptrs (even if they were non-mu-
  // table):
  template<typename T>
  inline void Delete0(T* const& a_ptr)
  {
    if (a_ptr != nullptr)
      delete a_ptr;
    const_cast<T*&>(a_ptr) = nullptr;
  }

  template<typename T>
  inline void DeleteArr0(T* const& a_ptr)
  {
    if (a_ptr != nullptr)
      delete[] a_ptr;
    const_cast<T*&>(a_ptr) = nullptr;
  }
} // End namespace MAQUETTE
