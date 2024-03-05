// vim:ts=2:et
//===========================================================================//
//                            "Protocols/P2CGate/Pos.h":                     //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace Pos
{
  struct position
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    char client_code[8]; // c7
    signed int open_qty; // i4
    signed int buys_qty; // i4
    signed int sells_qty; // i4
    signed int pos; // i4
    char net_volume_rur[15]; // d26.2
    signed long long last_deal_id; // i8
    char waprice[11]; // d16.5
  };

  constexpr size_t sizeof_position = 88;
  constexpr size_t position_index = 0;


  struct sys_events
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long event_id; // i8
    signed int sess_id; // i4
    signed int event_type; // i4
    char message[65]; // c64
  };

  constexpr size_t sizeof_sys_events = 108;
  constexpr size_t sys_events_index = 1;

} // End namespace Pos
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
