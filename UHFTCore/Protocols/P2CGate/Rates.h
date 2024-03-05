// vim:ts=2:et
//===========================================================================//
//                         "Protocols/P2CGate/Rates.h":                      //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace Rates
{
  struct curr_online
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int rate_id; // i4
    char value[11]; // d16.5
    struct cg_time_t moment; // t
  };

  constexpr size_t sizeof_curr_online = 52;
  constexpr size_t curr_online_index = 0;

} // End namespace Rates
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
