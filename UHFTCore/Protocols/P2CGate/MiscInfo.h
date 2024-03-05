// vim:ts=2:et
//===========================================================================//
//                          "Protocols/P2CGate/MiscInfo.h":                  //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace MiscInfo
{
  struct volat_coeff
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    char a[10]; // d16.10
    char b[10]; // d16.10
    char c[10]; // d16.10
    char d[10]; // d16.10
    char e[10]; // d16.10
    char s[10]; // d16.10
  };

  constexpr size_t sizeof_volat_coeff = 88;
  constexpr size_t volat_coeff_index = 0;

} // End namespace MiscInfo
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
