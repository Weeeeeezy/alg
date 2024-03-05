// vim:ts=2:et
//===========================================================================//
//                        "Protocols/P2Cgate/FutInfo.h":                     //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace FutInfo
{
  struct delivery_report
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    struct cg_time_t date; // t
    char client_code[8]; // c7
    char type[3]; // c2
    signed int isin_id; // i4
    signed int pos; // i4
    signed int pos_excl; // i4
    signed int pos_unexec; // i4
    signed char unexec; // i1
    char settl_pair[13]; // c12
    char asset_code[26]; // c25
    char issue_code[26]; // c25
    char oblig_rur[10]; // d16.2
    signed long long oblig_qty; // i8
    char fulfil_rur[10]; // d16.2
    signed long long fulfil_qty; // i8
    signed int step; // i4
    signed int sess_id; // i4
    signed int id_gen; // i4
  };

  constexpr size_t sizeof_delivery_report = 180;
  constexpr size_t delivery_report_index = 0;


  struct fut_rejected_orders
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long order_id; // i8
    signed int sess_id; // i4
    char client_code[8]; // c7
    struct cg_time_t moment; // t
    struct cg_time_t moment_reject; // t
    signed int isin_id; // i4
    signed char dir; // i1
    signed int amount; // i4
    char price[11]; // d16.5
    struct cg_time_t date_exp; // t
    signed long long id_ord1; // i8
    signed int ret_code; // i4
    char ret_message[256]; // c255
    char comment[21]; // c20
    char login_from[21]; // c20
    signed int ext_id; // i4
  };

  constexpr size_t sizeof_fut_rejected_orders = 416;
  constexpr size_t fut_rejected_orders_index = 1;


  struct fut_intercl_info
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    char client_code[8]; // c7
    char vm_intercl[10]; // d16.2
  };

  constexpr size_t sizeof_fut_intercl_info = 48;
  constexpr size_t fut_intercl_info_index = 2;


  struct fut_bond_registry
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int bond_id; // i4
    char small_name[26]; // c25
    char short_isin[26]; // c25
    char name[76]; // c75
    struct cg_time_t date_redempt; // t
    char nominal[11]; // d16.5
    signed char bond_type; // i1
    signed short year_base; // i2
  };

  constexpr size_t sizeof_fut_bond_registry = 180;
  constexpr size_t fut_bond_registry_index = 3;


  struct fut_bond_isin
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    signed int bond_id; // i4
    char coeff_conversion[5]; // d5.4
  };

  constexpr size_t sizeof_fut_bond_isin = 40;
  constexpr size_t fut_bond_isin_index = 4;


  struct fut_bond_nkd
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int bond_id; // i4
    struct cg_time_t date; // t
    char nkd[11]; // d16.7
    signed char is_cupon; // i1
  };

  constexpr size_t sizeof_fut_bond_nkd = 52;
  constexpr size_t fut_bond_nkd_index = 5;


  struct fut_bond_nominal
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int bond_id; // i4
    struct cg_time_t date; // t
    char nominal[11]; // d16.5
    char face_value[11]; // d16.5
    char coupon_nominal[7]; // d8.5
    signed char is_nominal; // i1
  };

  constexpr size_t sizeof_fut_bond_nominal = 68;
  constexpr size_t fut_bond_nominal_index = 6;


  struct usd_online
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed long long id; // i8
    char rate[10]; // d16.4
    struct cg_time_t moment; // t
  };

  constexpr size_t sizeof_usd_online = 52;
  constexpr size_t usd_online_index = 7;


  struct fut_vcb
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char code_vcb[26]; // c25
    char name[76]; // c75
    char exec_type[2]; // c1
    char curr[4]; // c3
    char exch_pay[10]; // d16.2
    signed char exch_pay_scalped; // i1
    char clear_pay[10]; // d16.2
    signed char clear_pay_scalped; // i1
    char sell_fee[6]; // d7.3
    char buy_fee[6]; // d7.3
    char trade_scheme[2]; // c1
    char section[51]; // c50
    char exch_pay_spot[11]; // d16.5
    char client_code[8]; // c7
    char exch_pay_spot_repo[11]; // d16.5
    signed int rate_id; // i4
  };

  constexpr size_t sizeof_fut_vcb = 256;
  constexpr size_t fut_vcb_index = 8;


  struct session
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    struct cg_time_t begin; // t
    struct cg_time_t end; // t
    signed int state; // i4
    signed int opt_sess_id; // i4
    struct cg_time_t inter_cl_begin; // t
    struct cg_time_t inter_cl_end; // t
    signed int inter_cl_state; // i4
    signed char eve_on; // i1
    struct cg_time_t eve_begin; // t
    struct cg_time_t eve_end; // t
    signed char mon_on; // i1
    struct cg_time_t mon_begin; // t
    struct cg_time_t mon_end; // t
    struct cg_time_t pos_transfer_begin; // t
    struct cg_time_t pos_transfer_end; // t
  };

  constexpr size_t sizeof_session = 144;
  constexpr size_t session_index = 9;


  struct multileg_dict
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    signed int isin_id_leg; // i4
    signed int qty_ratio; // i4
  };

  constexpr size_t sizeof_multileg_dict = 40;
  constexpr size_t multileg_dict_index = 10;


  struct fut_sess_contents
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    signed int isin_id; // i4
    char short_isin[26]; // c25
    char isin[26]; // c25
    char name[76]; // c75
    signed int inst_term; // i4
    char code_vcb[26]; // c25
    signed char is_limited; // i1
    char limit_up[11]; // d16.5
    char limit_down[11]; // d16.5
    char old_kotir[11]; // d16.5
    char buy_deposit[10]; // d16.2
    char sell_deposit[10]; // d16.2
    signed int roundto; // i4
    char min_step[11]; // d16.5
    signed int lot_volume; // i4
    char step_price[11]; // d16.5
    struct cg_time_t d_pg; // t
    signed char is_spread; // i1
    char coeff[7]; // d9.6
    struct cg_time_t d_exp; // t
    signed char is_percent; // i1
    char percent_rate[5]; // d6.2
    char last_cl_quote[11]; // d16.5
    signed int signs; // i4
    signed char is_trade_evening; // i1
    signed int ticker; // i4
    signed int state; // i4
    signed char price_dir; // i1
    signed int multileg_type; // i4
    signed int legs_qty; // i4
    char step_price_clr[11]; // d16.5
    char step_price_interclr[11]; // d16.5
    char step_price_curr[11]; // d16.5
    struct cg_time_t d_start; // t
    char exch_pay[11]; // d16.5
    char pctyield_coeff[11]; // d16.5
    char pctyield_total[11]; // d16.5
  };

  constexpr size_t sizeof_fut_sess_contents = 432;
  constexpr size_t fut_sess_contents_index = 11;


  struct fut_instruments
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int isin_id; // i4
    char short_isin[26]; // c25
    char isin[26]; // c25
    char name[76]; // c75
    signed int inst_term; // i4
    char code_vcb[26]; // c25
    signed char is_limited; // i1
    char old_kotir[11]; // d16.5
    signed int roundto; // i4
    char min_step[11]; // d16.5
    signed int lot_volume; // i4
    char step_price[11]; // d16.5
    struct cg_time_t d_pg; // t
    signed char is_spread; // i1
    char coeff[7]; // d9.6
    struct cg_time_t d_exp; // t
    signed char is_percent; // i1
    char percent_rate[5]; // d6.2
    char last_cl_quote[11]; // d16.5
    signed int signs; // i4
    char volat_min[13]; // d20.15
    char volat_max[13]; // d20.15
    signed char price_dir; // i1
    signed int multileg_type; // i4
    signed int legs_qty; // i4
    char step_price_clr[11]; // d16.5
    char step_price_interclr[11]; // d16.5
    char step_price_curr[11]; // d16.5
    struct cg_time_t d_start; // t
    signed char is_limit_opt; // i1
    char limit_up_opt[5]; // d5.2
    char limit_down_opt[5]; // d5.2
    char adm_lim[11]; // d16.5
    char adm_lim_offmoney[11]; // d16.5
    signed char apply_adm_limit; // i1
    char pctyield_coeff[11]; // d16.5
    char pctyield_total[11]; // d16.5
    char exec_name[2]; // c1
  };

  constexpr size_t sizeof_fut_instruments = 424;
  constexpr size_t fut_instruments_index = 12;


  struct diler
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char client_code[8]; // c7
    char name[201]; // c200
    char rts_code[51]; // c50
    char transfer_code[8]; // c7
    signed int status; // i4
  };

  constexpr size_t sizeof_diler = 296;
  constexpr size_t diler_index = 13;


  struct investr
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char client_code[8]; // c7
    char name[201]; // c200
    signed int status; // i4
  };

  constexpr size_t sizeof_investr = 240;
  constexpr size_t investr_index = 14;


  struct fut_sess_settl
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int sess_id; // i4
    struct cg_time_t date_clr; // t
    char isin[26]; // c25
    signed int isin_id; // i4
    char settl_price[11]; // d16.5
  };

  constexpr size_t sizeof_fut_sess_settl = 80;
  constexpr size_t fut_sess_settl_index = 15;


  struct sys_messages
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int msg_id; // i4
    struct cg_time_t moment; // t
    char lang_code[9]; // c8
    signed char urgency; // i1
    signed char status; // i1
    char text[256]; // c255
    char message_body[4001]; // c4000
  };

  constexpr size_t sizeof_sys_messages = 4308;
  constexpr size_t sys_messages_index = 16;


  struct fut_settlement_account
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char code[8]; // c7
    signed char type; // i1
    char settlement_account[13]; // c12
  };

  constexpr size_t sizeof_fut_settlement_account = 48;
  constexpr size_t fut_settlement_account_index = 17;


  struct fut_margin_type
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char code[13]; // c12
    signed char type; // i1
    signed char margin_type; // i1
  };

  constexpr size_t sizeof_fut_margin_type = 40;
  constexpr size_t fut_margin_type_index = 18;


  struct prohibition
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int prohib_id; // i4
    char client_code[8]; // c7
    signed int initiator; // i4
    char section[51]; // c50
    char code_vcb[26]; // c25
    signed int isin_id; // i4
    signed int priority; // i4
    signed long long group_mask; // i8
    signed int type; // i4
    signed int is_legacy; // i4
  };

  constexpr size_t sizeof_prohibition = 144;
  constexpr size_t prohibition_index = 19;


  struct rates
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    signed int rate_id; // i4
    char curr_base[16]; // c15
    char curr_coupled[16]; // c15
    char radius[11]; // d16.5
  };

  constexpr size_t sizeof_rates = 72;
  constexpr size_t rates_index = 20;


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
  constexpr size_t sys_events_index = 21;

} // End namespace FutInfo
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
