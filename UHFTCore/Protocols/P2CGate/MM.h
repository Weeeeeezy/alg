// vim:ts=2:et
//===========================================================================//
//                            "Protocols/P2CGate/MM.h":                      //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace MM
{
  struct fut_MM_info
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char spread[11]; // d16.5
    char price_edge_sell[11]; // d16.5
    signed int amount_sells; // i4
    char price_edge_buy[11]; // d16.5
    signed int amount_buys; // i4
    char mm_spread[11]; // d16.5
    signed int mm_amount; // i4
    signed char spread_sign; // i1
    signed char amount_sign; // i1
    char percent_time[5]; // d6.2
    struct cg_time_t period_start; // t
    struct cg_time_t period_end; // t
    char client_code[8]; // c7
    signed int active_sign; // i4
    signed int agmt_id; // i4
    char fulfil_min[5]; // d6.2
    char fulfil_partial[5]; // d6.2
    char fulfil_total[5]; // d6.2
    signed char is_fulfil_min; // i1
    signed char is_fulfil_partial; // i1
    signed char is_fulfil_total; // i1
    signed char is_rf; // i1
    signed int id_group; // i4
  };

  constexpr size_t sizeof_fut_MM_info = 160;
  constexpr size_t fut_MM_info_index = 0;


  struct opt_MM_info
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int sess_id; // i4
    char spread[11]; // d16.5
    char price_edge_sell[11]; // d16.5
    signed int amount_sells; // i4
    char price_edge_buy[11]; // d16.5
    signed int amount_buys; // i4
    char mm_spread[11]; // d16.5
    signed int mm_amount; // i4
    signed char spread_sign; // i1
    signed char amount_sign; // i1
    char percent_time[5]; // d6.2
    struct cg_time_t period_start; // t
    struct cg_time_t period_end; // t
    char client_code[8]; // c7
    char cstrike_offset[11]; // d16.5
    signed int active_sign; // i4
    signed int agmt_id; // i4
    char fulfil_min[5]; // d6.2
    char fulfil_partial[5]; // d6.2
    char fulfil_total[5]; // d6.2
    signed char is_fulfil_min; // i1
    signed char is_fulfil_partial; // i1
    signed char is_fulfil_total; // i1
    signed char is_rf; // i1
    signed int id_group; // i4
  };

  constexpr size_t sizeof_opt_MM_info = 172;
  constexpr size_t opt_MM_info_index = 1;


  struct cs_mm_rule
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    char client_code[5]; // c4
    signed int isin_id; // i4
  };

  constexpr size_t sizeof_cs_mm_rule = 40;
  constexpr size_t cs_mm_rule_index = 2;


  struct mm_agreement_filter
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int agmt_id; // i4
    char agreement[51]; // c50
    char client_code[8]; // c7
    signed char is_fut; // i1
  };

  constexpr size_t sizeof_mm_agreement_filter = 88;
  constexpr size_t mm_agreement_filter_index = 3;

} // End namespace MM
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
