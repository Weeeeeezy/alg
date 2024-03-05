// vim:ts=2:et
//===========================================================================//
//                      "Protocols/P2CGate/Volatility.h":                    //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace Volatility
{
  struct volat
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char volat[11]; // d16.5
    char theor_price[11]; // d16.5
    char theor_price_limit[11]; // d16.5
    char up_prem[11]; // d16.5
    char down_prem[11]; // d16.5
  };

  constexpr size_t sizeof_volat = 88;
  constexpr size_t volat_index = 0;

} // End namespace Volatility
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
