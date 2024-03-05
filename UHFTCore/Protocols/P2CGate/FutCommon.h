// vim:ts=2:et
//===========================================================================//
//                        "Protocols/P2CGate/FutCommon.h":                   //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace FutCommon
{
  struct common
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char best_sell[11]; // d16.5
    signed int amount_sell; // i4
    char best_buy[11]; // d16.5
    signed int amount_buy; // i4
    char price[11]; // d16.5
    char trend[11]; // d16.5
    signed int amount; // i4
    struct cg_time_t deal_time; // t
    char min_price[11]; // d16.5
    char max_price[11]; // d16.5
    char avr_price[11]; // d16.5
    char old_kotir[11]; // d16.5
    signed int deal_count; // i4
    signed int contr_count; // i4
    char capital[15]; // d26.2
    signed int pos; // i4
    struct cg_time_t mod_time; // t
    char cur_kotir[11]; // d16.5
    char cur_kotir_real[11]; // d16.5
    signed int orders_sell_qty; // i4
    signed int orders_sell_amount; // i4
    signed int orders_buy_qty; // i4
    signed int orders_buy_amount; // i4
    char open_price[11]; // d16.5
    char close_price[11]; // d16.5
    struct cg_time_t local_time; // t
  };

  constexpr size_t sizeof_common = 256;
  constexpr size_t common_index = 0;

} // End namespace FutCommon
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
