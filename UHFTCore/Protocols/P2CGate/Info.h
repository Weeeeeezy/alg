// vim:ts=2:et
//===========================================================================//
//                           "Protocols/P2CGate/Info.h":                     //
//===========================================================================//
#pragma once
#include <cgate.h>
#pragma pack(push, 4)

namespace MAQUETTE
{
namespace P2CGate
{
namespace Info
{
  struct base_contracts_params
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char code_vcb[26]; // c25
    char code_mcs[26]; // c25
    signed char volat_num; // i1
    signed char points_num; // i1
    double subrisk_step; // f
    signed char is_percent; // i1
    char percent_rate[11]; // d16.5
    char currency_volat[11]; // d16.5
    signed char is_usd; // i1
    double usd_rate_curv_radius; // f
    double somc; // f
  };

  constexpr size_t sizeof_base_contracts_params = 128;
  constexpr size_t base_contracts_params_index = 0;


  struct futures_params
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char isin[26]; // c25
    signed int isin_id; // i4
    char code_vcb[26]; // c25
    double limit; // f
    char settl_price[11]; // d16.5
    signed char spread_aspect; // i1
    signed char subrisk; // i1
    double step_price; // f
    char base_go[15]; // d26.2
    struct cg_time_t exp_date; // t
    signed char spot_signs; // i1
    char settl_price_real[11]; // d16.5
    double min_step; // f
  };

  constexpr size_t sizeof_futures_params = 164;
  constexpr size_t futures_params_index = 1;


  struct virtual_futures_params
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char isin[26]; // c25
    char isin_base[26]; // c25
    signed char is_net_positive; // i1
    double volat_range; // f
    double t_squared; // f
    double max_addrisk; // f
    double a; // f
    double b; // f
    double c; // f
    double d; // f
    double e; // f
    double s; // f
    struct cg_time_t exp_date; // t
    signed char fut_type; // i1
    signed char use_null_volat; // i1
    signed int exp_clearings_bf; // i4
    signed int exp_clearings_cc; // i4
  };

  constexpr size_t sizeof_virtual_futures_params = 172;
  constexpr size_t virtual_futures_params_index = 2;


  struct options_params
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char isin[26]; // c25
    signed int isin_id; // i4
    char isin_base[26]; // c25
    char strike[11]; // d16.5
    signed char opt_type; // i1
    char settl_price[11]; // d16.5
    char base_go_sell[15]; // d26.2
    char synth_base_go[15]; // d26.2
    char base_go_buy[15]; // d26.2
  };

  constexpr size_t sizeof_options_params = 152;
  constexpr size_t options_params_index = 3;


  struct broker_params
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char broker_code[8]; // c7
    char code_vcb[26]; // c25
    signed int limit_spot_sell; // i4
    signed int used_limit_spot_sell; // i4
  };

  constexpr size_t sizeof_broker_params = 68;
  constexpr size_t broker_params_index = 4;


  struct client_params
  {
    signed long long replID; // i8
    signed long long replRev; // i8
    signed long long replAct; // i8
    char client_code[8]; // c7
    char code_vcb[26]; // c25
    char coeff_go[11]; // d16.5
    signed int limit_spot_sell; // i4
    signed int used_limit_spot_sell; // i4
  };

  constexpr size_t sizeof_client_params = 80;
  constexpr size_t client_params_index = 5;


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
  constexpr size_t sys_events_index = 6;

} // End namespace Info
} // End namaepace P2CGate
} // End namespace MAQUETTE
#pragma pack(pop)
