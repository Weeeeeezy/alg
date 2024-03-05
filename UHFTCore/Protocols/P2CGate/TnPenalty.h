// vim:ts=2:et
//===========================================================================//
//                        "Protocols/P2CGate/TnPenalty.h":                   //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace TnPenalty
{
  struct fee_all
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long time; // i8
    char p2login[65]; // c64
    signed int sess_id; // i4
    signed int points; // i4
    char fee[10]; // d16.2
  };

  constexpr size_t sizeof_fee_all = 120;
  constexpr size_t fee_all_index = 0;


  struct fee_tn
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long time; // i8
    char p2login[65]; // c64
    signed int sess_id; // i4
    signed int tn_type; // i4
    signed int err_code; // i4
    signed int count; // i4

  };

  constexpr size_t sizeof_fee_tn = 116;
  constexpr size_t fee_tn_index = 1;

} // End namespace TnPenalty
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
