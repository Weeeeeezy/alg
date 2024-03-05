// vim:ts=2:et
//===========================================================================//
//                         "Protocols/P2CGate/FutTrade.h":                   //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace FutTrade
{
  struct orders_log
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long id_ord; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    signed int amount; // i4
    signed int amount_rest; // i4
    signed long long id_deal; // i8
    signed long long xstatus; // i8
    signed int status; // i4
    char price[11]; // d16.5
    struct cg_time_t moment; // t
    signed char dir; // i1
    signed char action; // i1
    char deal_price[11]; // d16.5
    char client_code[8]; // c7
    char login_from[21]; // c20
    char comment[21]; // c20
    signed char hedge; // i1
    signed char trust; // i1
    signed int ext_id; // i4
    char broker_to[8]; // c7
    char broker_to_rts[8]; // c7
    char broker_from_rts[8]; // c7
    struct cg_time_t date_exp; // t
    signed long long id_ord1; // i8
    struct cg_time_t local_stamp; // t
  };

  constexpr size_t sizeof_orders_log = 216;
  constexpr size_t orders_log_index = 0;


  struct multileg_orders_log
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long id_ord; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    signed int amount; // i4
    signed int amount_rest; // i4
    signed long long id_deal; // i8
    signed long long xstatus; // i8
    signed int status; // i4
    char price[11]; // d16.5
    struct cg_time_t moment; // t
    signed char dir; // i1
    signed char action; // i1
    char deal_price[11]; // d16.5
    char rate_price[11]; // d16.5
    char swap_price[11]; // d16.5
    char client_code[8]; // c7
    char login_from[21]; // c20
    char comment[21]; // c20
    signed char hedge; // i1
    signed char trust; // i1
    signed int ext_id; // i4
    char broker_to[8]; // c7
    char broker_to_rts[8]; // c7
    char broker_from_rts[8]; // c7
    struct cg_time_t date_exp; // t
    signed long long id_ord1; // i8
    struct cg_time_t local_stamp; // t
  };

  constexpr size_t sizeof_multileg_orders_log = 240;
  constexpr size_t multileg_orders_log_index = 1;


  struct deal
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    signed long long id_deal; // i8
    signed long long id_deal_multileg; // i8
    signed long long id_repo; // i8
    signed int pos; // i4
    signed int amount; // i4
    signed long long id_ord_buy; // i8
    signed long long id_ord_sell; // i8
    char price[11]; // d16.5
    struct cg_time_t moment; // t
    signed char nosystem; // i1
    signed long long xstatus_buy; // i8
    signed long long xstatus_sell; // i8
    signed int status_buy; // i4
    signed int status_sell; // i4
    signed int ext_id_buy; // i4
    signed int ext_id_sell; // i4
    char code_buy[8]; // c7
    char code_sell[8]; // c7
    char comment_buy[21]; // c20
    char comment_sell[21]; // c20
    signed char trust_buy; // i1
    signed char trust_sell; // i1
    signed char hedge_buy; // i1
    signed char hedge_sell; // i1
    char fee_buy[15]; // d26.2
    char fee_sell[15]; // d26.2
    char login_buy[21]; // c20
    char login_sell[21]; // c20
    char code_rts_buy[8]; // c7
    char code_rts_sell[8]; // c7
  };

  constexpr size_t sizeof_deal = 288;
  constexpr size_t deal_index = 2;


  struct multileg_deal
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    signed int isin_id_rd; // i4
    signed int isin_id_rb; // i4
    signed int isin_id_repo; // i4
    signed int duration; // i4
    signed long long id_deal; // i8
    signed long long id_deal_rd; // i8
    signed long long id_deal_rb; // i8
    signed long long id_ord_buy; // i8
    signed long long id_ord_sell; // i8
    signed int amount; // i4
    char price[11]; // d16.5
    char rate_price[11]; // d16.5
    char swap_price[11]; // d16.5
    char buyback_amount[10]; // d16.2
    struct cg_time_t moment; // t
    signed char nosystem; // i1
    signed long long xstatus_buy; // i8
    signed long long xstatus_sell; // i8
    signed int status_buy; // i4
    signed int status_sell; // i4
    signed int ext_id_buy; // i4
    signed int ext_id_sell; // i4
    char code_buy[8]; // c7
    char code_sell[8]; // c7
    char comment_buy[21]; // c20
    char comment_sell[21]; // c20
    signed char trust_buy; // i1
    signed char trust_sell; // i1
    signed char hedge_buy; // i1
    signed char hedge_sell; // i1
    char login_buy[21]; // c20
    char login_sell[21]; // c20
    char code_rts_buy[8]; // c7
    char code_rts_sell[8]; // c7
  };

  constexpr size_t sizeof_multileg_deal = 300;
  constexpr size_t multileg_deal_index = 3;


  struct heartbeat
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    struct cg_time_t server_time; // t
  };

  constexpr size_t sizeof_heartbeat = 36;
  constexpr size_t heartbeat_index = 4;


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
  constexpr size_t sys_events_index = 5;


  struct user_deal
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    signed long long id_deal; // i8
    signed long long id_deal_multileg; // i8
    signed long long id_repo; // i8
    signed int pos; // i4
    signed int amount; // i4
    signed long long id_ord_buy; // i8
    signed long long id_ord_sell; // i8
    char price[11]; // d16.5
    struct cg_time_t moment; // t
    signed char nosystem; // i1
    signed long long xstatus_buy; // i8
    signed long long xstatus_sell; // i8
    signed int status_buy; // i4
    signed int status_sell; // i4
    signed int ext_id_buy; // i4
    signed int ext_id_sell; // i4
    char code_buy[8]; // c7
    char code_sell[8]; // c7
    char comment_buy[21]; // c20
    char comment_sell[21]; // c20
    signed char trust_buy; // i1
    signed char trust_sell; // i1
    signed char hedge_buy; // i1
    signed char hedge_sell; // i1
    char fee_buy[15]; // d26.2
    char fee_sell[15]; // d26.2
    char login_buy[21]; // c20
    char login_sell[21]; // c20
    char code_rts_buy[8]; // c7
    char code_rts_sell[8]; // c7
  };

  constexpr size_t sizeof_user_deal = 288;
  constexpr size_t user_deal_index = 6;


  struct user_multileg_deal
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    signed long long id_deal; // i8
    signed int isin_id_rd; // i4
    signed int isin_id_rb; // i4
    signed int isin_id_repo; // i4
    signed int duration; // i4
    signed long long id_deal_rd; // i8
    signed long long id_deal_rb; // i8
    signed long long id_ord_buy; // i8
    signed long long id_ord_sell; // i8
    signed int amount; // i4
    char price[11]; // d16.5
    char rate_price[11]; // d16.5
    char swap_price[11]; // d16.5
    char buyback_amount[10]; // d16.2
    struct cg_time_t moment; // t
    signed char nosystem; // i1
    signed long long xstatus_buy; // i8
    signed long long xstatus_sell; // i8
    signed int status_buy; // i4
    signed int status_sell; // i4
    signed int ext_id_buy; // i4
    signed int ext_id_sell; // i4
    char code_buy[8]; // c7
    char code_sell[8]; // c7
    char comment_buy[21]; // c20
    char comment_sell[21]; // c20
    signed char trust_buy; // i1
    signed char trust_sell; // i1
    signed char hedge_buy; // i1
    signed char hedge_sell; // i1
    char login_buy[21]; // c20
    char login_sell[21]; // c20
    char code_rts_buy[8]; // c7
    char code_rts_sell[8]; // c7
  };

  constexpr size_t sizeof_user_multileg_deal = 300;
  constexpr size_t user_multileg_deal_index = 7;

} // End namespace FutTrade
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
