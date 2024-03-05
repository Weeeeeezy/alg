// vim:ts=2:et
//===========================================================================//
//                           "Protocols/P2Cgate/Fee.h":                      //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace Fee
{
  struct adjusted_fee
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long id_deal; // i8
    struct cg_time_t moment; // t
    char code_buy[8]; // c7
    char code_sell[8]; // c7
    char initial_fee_buy[15]; // d26.2
    char initial_fee_sell[15]; // d26.2
    char adjusted_fee_buy[15]; // d26.2
    char adjusted_fee_sell[15]; // d26.2
    signed long long id_repo; // i8
    signed long long id_deal_multileg; // i8
  };

  constexpr size_t sizeof_adjusted_fee = 136;
  constexpr size_t adjusted_fee_index = 0;


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

} // End namespace Fee
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
