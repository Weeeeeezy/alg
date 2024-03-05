// vim:ts=2:et
//===========================================================================//
//                          "Protocols/P2Cgate/Aggr.h":                      //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace Aggr
{
  struct orders_aggr
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    char price[11]; // d16.5
    signed long long volume; // i8
    struct cg_time_t moment; // t
    signed char dir; // i1
  };

  constexpr size_t sizeof_orders_aggr = 60;
  constexpr size_t orders_aggr_index = 0;

} // End namespace Aggr
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
