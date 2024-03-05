// vim:ts=2
//===========================================================================//
//                            "Basis/Base64.cpp":                            //
//             "UnChecked" Base64 Functions: No Memory Allocation            //
//===========================================================================//
// NB: Implementation borrowed from UTXX, but all buffers are made static:
//
#include "Basis/Base64.h"
#include "Basis/Maths.hpp"
#include <utxx/compiler_hints.hpp>
#include <cassert>

namespace MAQUETTE
{
  //=========================================================================//
  // "Base64EnCodeUnChecked":                                                //
  //=========================================================================//
  // Assume that "a_dst" has sufficient size to store the output. Returns the
  // actual number of bytes stoted:
  //
  int Base64EnCodeUnChecked
  (
    uint8_t const* a_src,
    int            a_len,
    char*          a_dst,
    bool           a_eq_trail,
    bool           a_url_mode
  )
  {
    assert(a_src != nullptr && a_len >= 0 && a_dst != nullptr);

    //-----------------------------------------------------------------------//
    // EnCoding Tables (0: StdMode; 1: URLMode):                             //
    //-----------------------------------------------------------------------//
    constexpr static char encTables[2][65]
    {
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", // STD
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"  // URL
    };
    char const (&encTable)[65] = encTables[a_url_mode];

    //-----------------------------------------------------------------------//
    // EnCoding Loop:                                                        //
    //-----------------------------------------------------------------------//
    int    i = 0;   // Src idx
    int    j = 0;   // Dst idx
    while (i < a_len)
    {
      uint8_t  c1 =  a_src[i++];
      if (utxx::unlikely(i == a_len))
      {
        a_dst[j++] = encTable[ c1 >> 2 ];
        a_dst[j++] = encTable[(c1 & 0x3) << 4];
        if (a_eq_trail)
        {
          a_dst[j++] = '=';
          a_dst[j++] = '=';
        }
        break;
      }
      uint8_t c2 = a_src[i++];
      if (utxx::unlikely(i == a_len))
      {
        a_dst[j++] = encTable[  c1 >> 2 ];
        a_dst[j++] = encTable[((c1 & 0x3) << 4) | ((c2 & 0xf0) >> 4)];
        a_dst[j++] = encTable[ (c2 & 0xf) << 2];
        if (a_eq_trail)
          a_dst[j++] = '=';
        break;
      }
      uint8_t c3 = a_src[i++];
      a_dst[j++] = encTable[  c1 >> 2 ];
      a_dst[j++] = encTable[((c1 & 0x3) << 4) | ((c2 & 0xf0) >> 4)];
      a_dst[j++] = encTable[((c2 & 0xf) << 2) | ((c3 & 0xc0) >> 6)];
      a_dst[j++] = encTable[  c3 & 0x3f];
    }
    // Check the sizes:
    assert(i == a_len && j <= ((((a_len << 2) / 3) + 3) & ~3));
    return j;
  }
  //=========================================================================//
  // "Base64DeCodeUnChecked":                                                //
  //=========================================================================//
  // Assume that "a_dst" has sufficient size to store the output. Returns the
  // actual number of bytes stoted:
  //
  int Base64DeCodeUnChecked
  (
    char const*    a_src,
    int            a_len,
    uint8_t*       a_dst,
    bool           a_url_mode
  )
  {
    assert(a_src != nullptr && a_len >= 0 && a_dst != nullptr);

    //-----------------------------------------------------------------------//
    // DeCoding Tables (0: StdMode; 1: URLMode):                             //
    //-----------------------------------------------------------------------//
    constexpr static int8_t decTables[2][128]
    {
      // STD:
      {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
      },
      // URL:
      {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
      }
    };
    int8_t const (&decTable)[128] = decTables[a_url_mode];

    //-----------------------------------------------------------------------//
    // DeCoding Loop:                                                        //
    //-----------------------------------------------------------------------//
    // NB: The 0x7f mask is used to make sure the input chars are in 0..127:
    int    i = 0;
    int    j = 0;
    while (i < a_len)
    {
      int8_t c1 = -1;
      do     c1 = decTable[a_src[i++] & 0x7f];
      while(i < a_len && c1 == -1);

      if (utxx::unlikely(i == a_len || c1 == -1))
        break;

      int8_t c2 = -1;
      do     c2 = decTable[a_src[i++] & 0x7f];
      while (i < a_len && c2 == -1);

      if (utxx::unlikely(i == a_len || c2 == -1))
        break;

      a_dst[j++] =
        static_cast<uint8_t>(((c1 << 2) | ((c2 & 0x30) >> 4)) & 0xff);

      int8_t c3 = -1;
      do
      {
        c3 = a_src[i++] & 0x7f;
        if (c3 == 61)   // '=' found, we are at the end!
          goto Done;
        c3 = decTable[c3];
      }
      while (i < a_len && c3 == -1);

      if (utxx::unlikely(i == a_len || c3 == -1))
        break;

      a_dst[j++] =
        static_cast<uint8_t>((((c2 & 0xf) << 4) | ((c3 & 0x3c) >> 2)) & 0xff);

      int8_t c4 = -1;
      do
      {
        c4 = a_src[i++] & 0x7f;
        if (c4 == 61)   // '=' found, we are at the end!
          goto Done;
        c4 = decTable[c4];
      }
      while (i < a_len && c4 == -1);

      if (utxx::unlikely(c4 == -1))
        break;

      a_dst[j++] =
        static_cast<uint8_t>((((c3 & 0x03) << 6) | c4) & 0xff);
    }
  Done: ;
    assert(i <= a_len && j <= int(Ceil(float(a_len) * 0.75f)));
    return j;
  }
} // End namespace MAQUETTE
