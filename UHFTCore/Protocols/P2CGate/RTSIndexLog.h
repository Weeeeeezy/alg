// vim:ts=2:et
//===========================================================================//
//                      "Protocols/P2CGate/RTSIndexLog.h":                   //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace RTSIndexLog
{
  struct rts_index_log
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char name[26]; // c25
    struct cg_time_t moment; // t
    char value[11]; // d18.4
    char prev_close_value[11]; // d18.4
    char open_value[11]; // d18.4
    char max_value[11]; // d18.4
    char min_value[11]; // d18.4
    char usd_rate[7]; // d10.4
    char cap[11]; // d18.4
    char volume[11]; // d18.4
  };

  constexpr size_t sizeof_rts_index_log = 144;
  constexpr size_t rts_index_log_index = 0;

} // End namespace RTSIndexLog
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
