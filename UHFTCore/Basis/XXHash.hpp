// vim:ts=2:et
//===========================================================================//
//                             "Basis/XXHash.hpp":                           //
//        C++ Wrapper for xxHash -- Extremely Fast Hash Algorithm            //
//===========================================================================//
#pragma once

#include <xxHash/xxhash.h>
#include <utxx/compiler_hints.hpp>
#include <string>
#include <cstring>

namespace MAQUETTE
{
  //=========================================================================//
  // 64- and 48-bit Hash Code Computations:                                  //
  //=========================================================================//
  constexpr unsigned long XXHSeed = 24459308806UL;

  inline unsigned long MkHashCode64(char const* a_str)
  {
    return
      (utxx::unlikely(a_str == nullptr || *a_str == '\0'))
      ? 0UL
      : XXH64(a_str, strlen(a_str), XXHSeed);
  }

  inline unsigned long MkHashCode64(std::string const& a_str)
  {
    return
      (utxx::unlikely(a_str.empty()))
      ? 0UL
      : XXH64(a_str.data(), a_str.size(), XXHSeed);
  }

  inline unsigned long To48(unsigned long a_code64)
    { return a_code64 & 0xFFFFFFFFFFFFUL; }
}
// End namespace MAQUETTE
