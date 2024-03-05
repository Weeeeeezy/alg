// vim:ts=2:et
//===========================================================================//
//                        "Protocols/FAST/LowLevel.hpp":                     //
//          Low-Level Types and Operations for FAST Msgs Decoding            //
//===========================================================================//
#pragma  once

#include "Basis/Maths.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <type_traits>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace MAQUETTE
{
namespace FAST
{
  //=========================================================================//
  // Standard FAST Types:                                                    //
  //=========================================================================//
  typedef uint64_t PMap;  // XXX: Sufficient for small applications???
  typedef uint32_t TID;   // TemplateID

  //=========================================================================//
  // "Decimal" Struct:                                                       //
  //=========================================================================//
  struct Decimal
  {
    int         m_exp;
    long        m_mant;
    double      m_val;

    // Default Ctor: All 0s:
    Decimal()
    : m_exp (0),
      m_mant(0),
      m_val (0.0)
    {}

    // "Reset": Similar:
    void Reset()
    {
      m_exp  = 0;
      m_mant = 0;
      m_val  = 0.0;
    }

    // Non-Default Ctor:
    Decimal(int e, long m)
    : m_exp (e),
      m_mant(m),
      m_val (double(m) * Pow(10.0, double(e)))
    {}

    // "Set": Similar:
    void Set(int e, long m)
    {
      m_exp  = e;
      m_mant = m;
      m_val  = double(m) * Pow(10.0, double(e));
    }

    // Addition / Subtraction:  This is peculiar: It  is performed  on the
    // exponent and mantissa separately, and then the value is recomputed:
    //
    Decimal& operator+=(Decimal const& right)
    {
      m_exp  += right.m_exp;
      m_mant += right.m_mant;
      m_val   = double(m_mant) * Pow(10.0, double(m_exp));
      return *this;
    }

    Decimal operator+  (Decimal const& right) const
    {
      int  e = m_exp  + right.m_exp;
      long m = m_mant + right.m_mant;
      return Decimal(e, m);
    }

    Decimal& operator-=(Decimal const& right)
    {
      m_exp  -= right.m_exp;
      m_mant -= right.m_mant;
      m_val   = double(m_mant) * Pow(10.0, double(m_exp));
      return *this;
    }

    Decimal operator-  (Decimal const& right) const
    {
      int  e = m_exp  - right.m_exp;
      long m = m_mant - right.m_mant;
      return Decimal(e, m);
    }
  };

  //=========================================================================//
  // Extracting FAST Flds from an Input Buffer:                              //
  //=========================================================================//
  // NB:
  // (*) All functions return the new buffer position (after the item which has
  //     just been read in);
  // (*) If the "SkipVal" flag is set, we do not construct the actual  value,
  //     just skip to the end of the curr fld; the value returned is 0.
  // (*) However, in that case, the "a_is_null" flag is set correctly ANYWAY,
  //     but "a_is_neg" is not set (false).
  // (*) The end-ptr is always correct:
  //
  //=========================================================================//
  // "GetInteger":                                                           //
  //=========================================================================//
  // (Signed or unsigned, NULLable or non-NULLable Integer):
  //
  template<bool SkipVal, typename T>
  inline typename std::enable_if<std::is_integral_v<T>, char const*>::type
  GetInteger
  (
    bool        a_nullable,
    char const* a_buff,
    char const* a_end,
    T*          a_res,
    bool*       a_is_null,
    char const* a_fld_name,   // For diagnostics only
    bool*       a_is_neg      // For internal use only
  )
  {
    assert(a_buff != nullptr && a_end     != nullptr && a_buff < a_end       &&
           a_res  != nullptr && a_is_null != nullptr && a_fld_name != nullptr);

    // For safety, always initialise the outputs:
    *a_res     = 0;
    *a_is_null = false;       // IMPORTANT: Use this default!
    if (a_is_neg != nullptr)
      *a_is_neg  = false;

    // Here the result is accumulated from 7-bit bytes; maximum of (N) 7-bit
    // bytes for the size of type "T", where N=1+BitSz/7 (for the maximum N,
    // the highest-order byte would contribute less that 7 bit):
    //
    constexpr int BitSz = sizeof(T) * 8;

    // Scan the buffer. First, get the highest-order byte  (the input comes
    // in the big endian format). It is always present, and also determines
    // whether the value is NULL or negative:
    //
    char c0    = *a_buff;     // XXX: The above "assert" guarantees it's OK
    bool done  = bool(c0 & '\x80');

    // Extract the leading 7-bit value and initialise the result:
    c0        &= '\x7f';
    [[maybe_unused]] T lres  = T(c0);

    // Now process other bytes (if any).
    // NB: the value of "shift" as given before entering each iteration, must
    // then be decremented by 7 to get the actual left shift. The  orig value
    // may be in the range 1..6, in which case an additional left shift is to
    // be performed (potentially discarding the highest-order bits)   to make
    // the effect of 0 shift in the last byte:
    //
    char const* curr = a_buff + 1;

    for (; !done && curr < a_end; ++curr)
      if constexpr (!SkipVal)
      {
        // Generic Case:
        char c = *curr;
        done   = bool(c & '\x80');  // Check the stop bit
        c     &= '\x7f';
        lres <<= 7;
        lres  |= T(c);
      }
      else
        // Just evaluate the stop bit:
        done   = bool((*curr) & '\x80');

    // Arrived at the end?
    if (utxx::unlikely(!done))
      throw utxx::runtime_error
            ("FAST::GetInteger(", a_fld_name, "): Unterminated value");

    // If we really need a value: Manage its sign:
    bool neg = false;
    if constexpr (!SkipVal)
    {
      // For signed types, need to check  if the over-all value  is negative.
      // This is determined by the 6th (sign) bit of the leading 7-bit group:
      if constexpr (std::is_signed_v<T>)
      {
        neg = c0 & '\x40';

        // If negative, fill in the high part of "lres" ((BitSz-7*n) bits) with
        // "1"s:
        int  n7 = 7 * int(curr - a_buff);
        if (neg && n7 < BitSz)
        {
          // Make a mask of "n7" 1s, and invert it. Thus, the leading
          // (BitSz-7*n) bits appear to be 1, the trailing (7*n) bits are 0:
          T mask = ~((T(1) << n7) - 1);
          // Apply it:
          lres  |= mask;
        }
      }
    }

    // Check for NULL and adjust NULLable values. XXX: "IsNULL" flag is set cor-
    // ectly in ANY case (even in the "SkipVal" mode),  because the Caller may
    // still require it:
    if (a_nullable)
    {
      int n =  int(curr - a_buff);
      if (n == 1 && !c0)
        *a_is_null = true;
      else
      if constexpr (!SkipVal)
      {
        // Non-NULL: decrement the value (for signed types, only if it is non-
        // negative). IMPORTANT:  because of possible overflows, we must  NOT
        // compare "lres" with 0 (may get false equality): Use the "neg" flag
        // instead!!!
        if (std::is_unsigned_v<T> || !neg)
          lres--;
      }
    }

    // Invariant Test (but only AFTER the NULL adjustment): NB: "neg" being set
    // and the value being 0, IS ALLOWED:
    if constexpr (!SkipVal)
    {
      if (utxx::unlikely
         ((neg && lres > 0) || (std::is_signed_v<T> && !neg && lres < 0)))
        throw utxx::runtime_error
              ("FAST::GetInteger(", a_fld_name, "): neg=", neg, ", value=",
               lres);
      assert(curr <= a_end);

      // Store the results:
      *a_res = lres;
      if (a_is_neg != nullptr)
        *a_is_neg = neg;
    }

    // Return the advanced input ptr (in any case):
    return curr;
  }

  //=========================================================================//
  // "GetDecimal":                                                           //
  //=========================================================================//
  // XXX: Returned type is a "double" which is not ideal (rounding errors etc),
  // but OK for the moment. Using "new-style" 2-field encoding of Decimals:
  //
  template<bool SkipVal>
  inline char const* GetDecimal
  (
    bool        a_nullable,
    char const* a_buff,
    char const* a_end,
    Decimal*    a_res,
    bool*       a_is_null,
    char const* a_fld_name
  )
  {
    assert(a_buff != nullptr && a_end     != nullptr && a_buff < a_end       &&
           a_res  != nullptr && a_is_null != nullptr && a_fld_name != nullptr);

    // XXX: For safety, initialise the output:
    a_res->Reset();
    *a_is_null = false;

    int32_t expon = 0; // Exponent, actually in [-63 .. +63], NULLable
    int64_t mant  = 0; // Mantissa

    // NB: Even if the "expon" val itself is not required, we need its NULL flg.
    // But we can STILL apply the "SkipVal" flg, because the NULL flg is return-
    // ed correcyly anyway, as well as "exp_end":
    char const* exp_end =
      GetInteger<SkipVal>
        (a_nullable, a_buff, a_end, &expon, a_is_null, a_fld_name, nullptr);

    // Check for NULL -- in any case (because if the exponent is NULL, we stop
    // immediately):
    if (*a_is_null)
    {
      // Then the whole Decimal is NULL/0:
      assert(exp_end == a_buff + 1 && expon == 0);
      return exp_end;
    }

    // Check for a correct Exp:
    if (utxx::unlikely(!SkipVal && (expon < -63 || expon > 63)))
      throw utxx::runtime_error
            ("FAST::GetDecimal(", a_fld_name, "): Invalid Exponent=", expon);

    // Non-NULL: Get a mantissa (non-NULLable):
    char const* mant_end =
      GetInteger<SkipVal>
        (false, exp_end, a_end, &mant, a_is_null, a_fld_name, nullptr);

    // The resulting value:
    if constexpr (!SkipVal)
    {
      assert(!(*a_is_null));
      a_res->Set(expon, mant);
    }
    return mant_end;
  }

  //=========================================================================//
  // "GetOldDecimal":                                                        //
  //=========================================================================//
  // Like "GetDecimal" above, but using "old-style" 1-field encoding of Decim-
  // als:
  template<bool SkipVal>
  inline char const* GetOldDecimal
  (
    bool        a_nullable,
    char const* a_buff,
    char const* a_end,
    Decimal*    a_res,
    bool*       a_is_null,
    char const* a_fld_name
  )
  {
    assert(a_buff != nullptr && a_end     != nullptr && a_buff < a_end       &&
           a_res  != nullptr && a_is_null != nullptr && a_fld_name != nullptr);

    // XXX: For safety, initialise the output:
    a_res->Reset();
    *a_is_null = false;

    // Get the Exponent byte, it must have the stop bit set:
    signed char expon = *a_buff;
    ++a_buff;

    if (utxx::unlikely(!(expon & '\x80')))
      throw utxx::runtime_error
            ("FAST::GetOldDecimal(", a_fld_name, "): Unterminated ExpByte: ",
             expon);
    expon &= '\x7f';

    // Don't forget to propagate the sign (from bit 6 to bit 7):
    if (expon &  '\x40')
      expon   |= '\x80';

    // Check  for NULLability: if NULLable and the exp is NULL, then the whole
    // Decimal is NULL:
    if (a_nullable)
    {
      if (expon == 0)
      {
        if constexpr (!SkipVal)
          *a_is_null = true;  // *a_res remains NULL/0
        return a_buff;        // Stop now!
      }
      else
      if constexpr (!SkipVal)
      {
        // Adjust the "expon" value:
        if (expon >= 1)
          --expon;
      }
    }

    // Check the exponent range:
    if (utxx::unlikely(!SkipVal && (expon < -63 || expon > 63)))
      throw utxx::runtime_error
            ("FAST::GetOldDecimal(", a_fld_name, "): Invalid Exponent=", expon);

    // Now get the mantissa -- use the longest signed format available:
    int64_t     mant     = 0;
    char const* mant_end =
      GetInteger<SkipVal>
        (false, a_buff, a_end, &mant, a_is_null, a_fld_name, nullptr);

    // The resulting value:
    if constexpr (!SkipVal)
    {
      assert(!(*a_is_null));
      a_res->Set(expon, mant);
    }
    return mant_end;
  }

  //=========================================================================//
  // "GetASCII":                                                             //
  //=========================================================================//
  // For efficinecy, the Caller must provide the Output Buffer "a_res" which the
  // String is copied into; the string is then 0-terminated;  "a_act_len" is its
  // actual length (WITHOUT the terminating 0 -- just like "strlen");
  // NB: "a_act_len" may be smaller than the total number of bytes consumed from
  // the input stream because of treatment of 0-prefixes and NULL strings:
  //
  template<bool SkipVal>
  inline char const* GetASCII
  (
    bool        a_nullable,
    char const* a_buff,
    char const* a_end,
    char*       a_res,       // Output buffer for the string
    int         a_max_len,   // Size of "a_res" buffer -- max len incl '\0'
    int*        a_act_len,   // W/o the terminating 0
    bool*       a_is_null,
    char const* a_fld_name
  )
  {
    assert(a_buff != nullptr && a_end     != nullptr && a_buff < a_end       &&
           a_res  != nullptr && a_is_null != nullptr && a_fld_name != nullptr);

    // XXX: For safety, always initialise the outputs:
    *a_res     = '\0';
    *a_act_len = 0;     // 0-terminator is NOT counted!
    *a_is_null = false;

    // NB: "a_max_len" must be at least 2, including the terminating 0 (which is
    // NOT stored in the input stream):
    if (utxx::unlikely(a_max_len < 2))
      throw utxx::badarg_error
            ("FAST::GetASCII(", a_fld_name, "): Output buffer too short");

    // Scan the buffer; "n" is the total number of bytes consumed which is
    // limited by (a_max_len-1) because the last byte is for terminating 0:
    int  n     = 0;
    bool done  = false;
    int  stop  = std::min<int>(a_max_len - 1, int(a_end - a_buff));
    assert(stop >= 1);

    for (; n < stop && !done; ++n)
      if constexpr (!SkipVal)
      {
        // Generic Case:
        char c   = a_buff[n];
        done     = bool(c & '\x80'); // Check the stop bit
        c       &= '\x7f';           // Get the data bits
        a_res[n] = c;
      }
      else
        // Just evaluate the stop bit:
        done = bool(a_buff[n] & '\x80');

    // Arrived at the end?
    if (utxx::unlikely(!done))
      throw utxx::runtime_error
            ("FAST::GetASCII(", a_fld_name, "): String too long?");

    // We should have got at least 1 byte:
    assert(n >= 1);

    // The rest is done only if we are interested in the value:
    if constexpr (!SkipVal)
    {
      if (utxx::unlikely(a_res[0] == '\0'))
      {
        // String starts with 0:
        if (a_nullable && n == 1)
          *a_is_null = true;
        else
          // Check for "over-long" strings. XXX: This check is more strict than
          // necessary: we do not allow any non-0 chars followed by initial 0s:
          for (int i = 1; i < n; ++i)
            if (utxx::unlikely(a_res[i]))
              // Non-0 is encountered after a 0:
              throw utxx::runtime_error
                    ("FAST::GetASCII(", a_fld_name, "): Over-long string: ",
                     std::string(a_res, unsigned(n-i)));

        // XXX: We currently do not return any strings containing only '\0's be-
        // cause the rules of what is in the prefix or body are convoluted.  So
        // simply make such strings empty:
        assert(*a_act_len == 0);
      }
      else
      {
        *a_act_len = n;
        // Put the terminator:
        a_res[n] = '\0';
      }

      // Check: a NULL string is always empty:
      assert(!(*a_is_null) || *a_act_len == 0);
      assert(*a_act_len < a_max_len);
    }

    // In any case, return the ptr beyond the string read:
    assert(a_buff + n <= a_end);
    return a_buff + n;
  }

  //=========================================================================//
  // "GetASCIIDelta":                                                        //
  //=========================================================================//
  // The base value is supposed to be already installed in "a_res". The string
  // being read  is prefixed by the Subtraction Length which has effect as be-
  // low: FIXME: Not fully tested if "SkipVal" flag is set:
  //
  template<bool SkipVal>
  inline char const* GetASCIIDelta
  (
    bool        a_nullable,
    char const* a_buff,
    char const* a_end,
    char*       a_res,            // Output buffer for the string
    int         a_max_len,        // Size of "a_res" buffer -- incl '\0'
    int*        a_act_len,        // W/o the terminating 0
    bool*       a_is_null,
    char const* a_fld_name
  )
  {
    assert(a_buff != nullptr && a_end     != nullptr && a_buff < a_end       &&
           a_res  != nullptr && a_is_null != nullptr && a_fld_name != nullptr);

    // XXX: For safety, always initialise the outputs:
    *a_res     = '\0';
    *a_act_len = 0;
    *a_is_null = false;

    // First, get the Subtraction Length: It is never "SkipVal"ped, at least be-
    // cause we need the "is_neg" flag:
    int32_t sub_len = 0;
    bool    is_neg  = false;

    a_buff  = GetInteger<false>
              (a_nullable, a_buff, a_end, &sub_len, a_is_null, a_fld_name,
               &is_neg);

    if (utxx::unlikely(*a_is_null))
    {
      // Then the whole ASCII Delta is NULL:
      assert(*a_act_len == 0);
      return  a_buff;
    }

    // Now read in the new string and modify the existing one:
    int len = int(strnlen(a_res, size_t(a_max_len)));

    // NB: The sign of "sub_len" determines whether the curr "a_res" is trunca-
    // ted at back (+) or at front (-); because 0 can also be signed,  use the
    // "is_neg" flag to decide the sign:
    if (!is_neg)
    {
      // The existing string is to be truncated at BACK by "sub_len" bytes, the
      // new one comes straight after it:
      //
      assert(sub_len >= 0);
      int   off  = std::max<int>(len - sub_len, 0);
      char* from = a_res + off;

      // "a_max_len" needs to be adjusted,  so that the whole result fits  into
      // the buffer. The string we are getting is not NULLable -- only the Sub-
      // traction Length is. It is OK to "SkipVal" the actual appendix here, if
      // not used:
      int max_delta_len = std::max<int>(a_max_len - off, 0);
      a_buff =
        GetASCII<SkipVal>
          (false, a_buff, a_end, from, max_delta_len, a_act_len, a_is_null,
           a_fld_name);

      // Adjust "a_act_len" -- make it the total length of the string in "a_res"
      // (not just the delta):
      if constexpr (!SkipVal)
      {
        assert(!(*a_is_null));
        *a_act_len += off;
      }
    }
    else
    {
      // The existing string is to be truncated at FRONT by   |sub_len| bytes.
      // The new string is read into the start of "a_res", and then the rest of
      // the existing one concatenated. Because the length of the new string
      // is not known yet, we will have to make an on-stack copy of the curr
      // one's tail:
      assert(sub_len <= 0);

      // NB: In this case, "sub_len" uses an "excess-1" encoding, so if it is
      // negative, it must be incremented by 1  (and then the absolute value
      // used):
      // XXX: so if "sub_len" is already 0 and the "neg"  flag is set,  we do
      // NOT increment it? -- Probably not, as negative 0 is to be encoded as
      // (-1) -- see the FAST Standard, Section 6.3.7.3:
      //
      if (sub_len < 0)
        sub_len = abs(sub_len + 1);

      int  keep = std::max<int>(len - sub_len, 0);
      [[maybe_unused]] char tail[keep];
      if constexpr (!SkipVal)
      {
        int  cut  = len - keep;
        assert(cut >= 0);
        strncpy(tail, a_res + cut, size_t(keep));
      }

      // Read the new string in. Again, it is not NULLable:
      int max_delta_len = std::max<int>(a_max_len - keep, 0);
      a_buff =
        GetASCII<SkipVal>
          (false, a_buff, a_end, a_res, max_delta_len, a_act_len, a_is_null,
           a_fld_name);

      // Copy the saved tail of the original string:
      if constexpr (!SkipVal)
      {
        assert(!(*a_is_null));
        strncpy(a_res + (*a_act_len), tail, size_t(keep));
        *a_act_len += keep;
      }
    }

    // Install the 0-terminator. NB: "a_act_len" is the length W/O the 0-term:
    if constexpr (!SkipVal)
    {
      assert(*a_act_len  < a_max_len);
      a_res [*a_act_len] = '\0';
    }
    return  a_buff;
  }

  //=========================================================================//
  // "GetByteVec" (Incl a UTF-8 string):                                     //
  //=========================================================================//
  // Args and return value are similar to those for "GetASCII", but different
  // encoded string format. In this case, the encoded string is prepended  by
  // the length prefix, so the string length ("act_length") and the total num-
  // ber of bytes consumed (return values) are different:
  //
  template<bool SkipVal>
  inline char const* GetByteVec
  (
    bool        a_nullable,
    char const* a_buff,
    char const* a_end,
    char*       a_res,            // Output buffer for the string
    int         a_max_len,        // Size of the "a_res" buffer -- max str len
    int*        a_act_len,        // W/o the terminating 0
    bool*       a_is_null,
    char const* a_fld_name
  )
  {
    assert(a_buff    != nullptr && a_end      != nullptr && a_buff < a_end &&
           a_res     != nullptr && a_is_null  != nullptr &&
           a_act_len != nullptr && a_fld_name != nullptr);

    // XXX: For safety, always initialise the outputs:
    *a_res     = '\0';
    *a_act_len = 0;
    *a_is_null = false;

    if (utxx::unlikely((a_buff >= a_end || a_max_len <= 0)))
      throw utxx::badarg_error
            ("FAST::GetByteVec(", a_fld_name, "): End of input buffer");

    // Get the length as uint32, and the ptr to the string body which follows.
    // If the string is NULLable, then the length is also nullable, and it is
    // used to determine whether  the whole  string is a NULL. Obviously, the
    // "len" can NOT be "SkipVal"ed:
    uint32_t    len  = 0;
    char const* body =
      GetInteger<false>
        (a_nullable,   a_buff, a_end, &len, a_is_null, a_fld_name, nullptr);

    if (utxx::unlikely(*a_is_null))
    {
      // Nothing to get further -- the whole string is NULL:
      assert(*a_act_len == 0);
      assert(body == a_buff + 1);
      return body;
    }

    // Calculate the end of input:
    char const* body_end = body + len;

    if constexpr (!SkipVal)
    {
      // Check the length: We will STILL 0-terminate the sequence received so it
      // can be used as a C-style string (if does not contain other 0s), so res-
      // erve 1 byte for the termiantor:
      //
      if (utxx::unlikely(int(len) + 1 > a_max_len))
        throw utxx::runtime_error
              ("FAST::GetByteVec(", a_fld_name, "): Output buffer too short (",
               a_max_len, "), need ", len + 1);

      if (utxx::unlikely(body_end > a_end))
        throw utxx::runtime_error
              ("FAST::GetByteVec(", a_fld_name,
               "): End-of-string beyond buffer end by ", (body_end - a_end),
               " bytes");

      // If all checks are OK: Get the string body direct into "a_res":
      memcpy(a_res, body, len);

      // Install the 0-terminator just in case:
      assert(int(len) <= a_max_len - 1);
      *a_act_len = int(len);
      a_res[len] = '\0';
    }

    // In any case:
    return body_end;
  }

  //=========================================================================//
  // "PMap" and TemplateID ("TID") Management:                               //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Accessing "PMap" bits:                                                  //
  //-------------------------------------------------------------------------//
  // "bit_no" is from 0; bit_no=0 corresponds to the highest (63) bit of the
  // representation:
  //
  inline bool GetPMapBit(PMap a_pmap, int a_bit_no)
  {
    // NB: "PMap" implicitly extends to +oo with 0 bits, so:
    if (utxx::unlikely(a_bit_no > 62))
      return false;

    // Otherwise, test the given bit:
    return (a_pmap & (1UL << (63 - a_bit_no)));
  }

  //=========================================================================//
  // "PMap" and TemplateID ("TID") Management:                               //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Extracting "PMap" and TemplateID:                                       //
  //-------------------------------------------------------------------------//
  // NB: "PMap" bits are stored in the resulting "uint64" in the order of incr-
  // easing significance, from 0 to 62 (bit 63 is unused).
  // Also note: "tid" may be NULL, in which case we get only the "PMap" (useful
  // for processing Sequences):
  //
  inline char const* GetMsgHeader
  (
    char const* a_buff,
    char const* a_end,
    PMap*       a_pmap,
    TID*        a_tid,
    char const* a_grp_name
  )
  {
    assert(a_buff != nullptr && a_end != nullptr && a_buff < a_end &&
           a_pmap != nullptr);

    // "PMap" is encoded as a sequence of 7-bit groups. As the result is stored
    // in a 64-bit format, the maximum of 9 groups (making 63 bits) is  allowed
    // with this implementation.
    // NB: Obtaining "PMap" is similar to "GetInteger" BUT:
    // (*) no need to test the result for NULL or negative values;
    // (*) the result is always adjusted to the left (highest-order) bit bound:
    //
    PMap        lpm   = 0;
    char const* curr  = a_buff;
    int         shift = 57;
    bool        done  = false;

    for (; !done && curr < a_end; ++curr)
    {
      char c = *curr;
      done   = bool(c & '\x80');  // Check the stop bit
      c     &= '\x7f';
      lpm   |= (PMap(c) << shift);
      shift -= 7;
    }
    if (utxx::unlikely(!done || curr > a_buff + 9))
      throw utxx::runtime_error
            ("FAST::GetMsgHeader(", a_grp_name, "): PMap too large?");

    *a_pmap = lpm;
    if (utxx::unlikely(a_tid == nullptr))
      return curr;

    // Now get the Template ID.  It should by itself have a PMap entry, be non-
    // NULLable, and the corresp PMap bit (0) must be 1:
    //
    bool hasTID = GetPMapBit(*a_pmap, 0);

    if (utxx::unlikely(!hasTID))
      throw utxx::runtime_error
            ("FAST::GetMsgHeader(", a_grp_name,
             "): No TemplateID bit in PMap?");

    // NB: Here we are getting the TID: it is never "SkipVal"ed:
    bool tid_null = false;
    char const* msg_body  =
      GetInteger<false>
        (false, curr, a_end, a_tid, &tid_null, a_grp_name, nullptr);
    assert(!tid_null);

    return msg_body;
  }

  //=========================================================================//
  // The Type of FAST Message Operators:                                     //
  //=========================================================================//
  enum class OpT
  {
    NoOp    = 0,
    Const   = 1,
    Copy    = 2,
    Default = 3,
    Delta   = 4,
    Incr    = 5,
    Tail    = 6
  };

  //-------------------------------------------------------------------------//
  // "HasPMapBitF": Does this fld have a PMap bit?                           //
  //-------------------------------------------------------------------------//
  constexpr bool HasPMapBitF(OpT a_op, bool a_optional)
  {
    return
      (a_op == OpT::NoOp || a_op == OpT::Delta) // NoOp, Delta: never PMapBit
      ? false
      :
      (a_op == OpT::Const)                      // Const: PMapBit for optional
      ? a_optional
      : true;                                   // All others: always
  }

  //-------------------------------------------------------------------------//
  // "IsNullableF": Is this Fld NULLable?                                    //
  //-------------------------------------------------------------------------//
  constexpr bool IsNullableF(OpT a_op, bool a_optional)
  {
    return
      // Optional flds are almost always NULLable -- except for a "Const";
      // mandatory (non-optional) flds are never NULLable:
      a_optional
      ? (a_op != OpT::Const)
      : false;
  }

  //=========================================================================//
  // "FC": Field Counter:                                                    //
  //=========================================================================//
  // COMPILE-TIME class which automatically keeps track of "PMap" bits for
  // FAST msgs flds (ie, whether a bit is present or not, and if yes, what
  // its position is) and for NULLability flags. Each new "FC" object con-
  // structed is updated with next fld info:
  //
  class FC
  {
  private:
    //-----------------------------------------------------------------------//
    // Internal State:                                                       //
    //-----------------------------------------------------------------------//
    int  const m_pos;    // PMap Position ("virtual" -- if "m_hasPMB" is set)
    bool const m_has;    // This fld has PMap fld?
    bool const m_null;   // This fld is NULLable

    // Default Ctor is hidden, because we need to initialise "m_pos" with 1
    // for main PMaps and with 0 for sequence entry PMaps, so have to do it
    // explicitly:
    FC();

  public:
    //-----------------------------------------------------------------------//
    // Non-Default Ctors:                                                    //
    //-----------------------------------------------------------------------//
    // (1) Initialisation -- for the 1st fld of a msg (init=1), or the 1st fld
    //     of a sequence (init=0):
    //
    constexpr FC(int a_init, bool a_optional, OpT a_op)
    : m_pos (a_init),
      m_has (HasPMapBitF(a_op, a_optional)),
      m_null(IsNullableF(a_op, a_optional))
    {}

    // (2) Update for subsequent flds:
    //     "m_pos" is advanced if the "a_prev" object did indeed have a PMap
    //     bit:
    constexpr FC(FC const&   a_prev, bool a_optional, OpT a_op)
    : m_pos (a_prev.m_has ? (a_prev.m_pos + 1) : a_prev.m_pos),
      m_has (HasPMapBitF(a_op, a_optional)),
      m_null(IsNullableF(a_op, a_optional))
    {}

    //-----------------------------------------------------------------------//
    // Accessors:                                                            //
    //-----------------------------------------------------------------------//
    constexpr bool HasPMapBit() const { return m_has;  }
    constexpr int  PMapPos()    const { return m_has ? m_pos : (-1); }
    constexpr bool IsNullable() const { return m_null; }

    //-----------------------------------------------------------------------//
    // Testing whether the fld is present:                                   //
    //-----------------------------------------------------------------------//
    // This method is not a "constexpr" -- it uses the PMap which is only known
    // at run-time. The fld is present if it does not have a PMap bit associat-
    // ed with it at all, of if that PMap bit is 1:
    //
    bool IsPresent(PMap a_pmap) const
      { return (!m_has) || GetPMapBit(a_pmap, m_pos); }
  };
} // End namespace FAST
} // End namespace MAQUETTE
