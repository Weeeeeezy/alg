#ifndef BLS_EXTERN_BLS_STRATEGIES_HPP_
#define BLS_EXTERN_BLS_STRATEGIES_HPP_

#include <cstdint>

#include <utxx/time_val.hpp>

#ifdef EXPORT_WINDOWS
#include "../lib/bls_signal.hpp"
#else
#include "bls_signal.hpp"
#endif // EXPORT_WINDOWS

// NB: Use only int64_t and double in parameter struct so that each parameter has the same size in
// bytes and we can access them via index
#define INT int64_t
#define DBL double

#define COMMON_PARAMS                                                                              \
  INT JTR_size;                                                                                    \
                                                                                                   \
  INT GRB_forward_size;                                                                            \
  INT GRB_reverse_size;                                                                            \
  INT GRB_gap_bricks;                                                                              \
  INT GRB_dynamic_shift_mode;                                                                      \
                                                                                                   \
  INT trade_direction;                                                                             \
  INT trading_mode;                                                                                \
  INT entry_order_type;                                                                            \
  INT force_market_exit;                                                                           \
  INT swap_exit_params;                                                                            \
  INT entry_adjust_ticks;                                                                          \
  INT stop_loss_ticks;                                                                             \
  DBL profit_target_multiple_of_stop_loss;

extern "C" {

struct BLSParams_HelixOSC {
  COMMON_PARAMS

  DBL cycle_length;
  DBL cycle_tolerance;
  INT delta_1;
  INT delta_2;
  DBL reversal_threshold;
  DBL phase;
  INT smooth;
  INT band_criterion;
};

struct BLSParams_RSXonRSX {
  COMMON_PARAMS

  DBL RSX_length;
  DBL blend;
  DBL phase;
  DBL top_band;
  DBL bottom_band;
};

struct BLSParams_VELonVEL {
  COMMON_PARAMS

  INT VEL_length;
  DBL blend;
  DBL lower_band;
  DBL upper_band;
};

struct BLSParams_JMACCX {
  COMMON_PARAMS

  INT DWMA_length;
  DBL top_line;
  DBL bottom_line;
};

struct BLSParams_JMAXover {
  COMMON_PARAMS

  DBL JMA_length;
  DBL JMA_phase;
  INT data_type;
  INT other_MA_type;
  INT other_MA_length;
};

struct BLSParams_Trend {
  COMMON_PARAMS

  INT tolerance;
};

struct BLSParams_GRBSystem {
  COMMON_PARAMS

  INT polarity;
};

struct Bar {
  double open, high, low, close;
  int64_t volume;
  utxx::time_val close_time;
};

void BLS_CreatePool(int num_instances);

void BLS_DestroyPool();

void BLS_GetLastClosedBar(uint64_t index, Bar *bar);

void BLS_GetCurrentBar(uint64_t index, Bar *bar);

bool BLS_Update(uint64_t index, double price, int64_t volume, utxx::time_val time,
                int market_position, BLSSignal *signal);

// Init functions for all strategies
// bar_type:
// 0: Time based bar
// 1: Range bar
// 2: Classic Renko bar
// 3: Mean Renko bar
// 4: Kase bar
// 5: Tick bar

void BLS_InitHelix(uint64_t index, int ticks_per_point, int bar_type, double bar_parameter,
                   BLSParams_HelixOSC helix_params);

void BLS_InitRSXonRSX(uint64_t index, int ticks_per_point, int bar_type, double bar_parameter,
                      BLSParams_RSXonRSX rsx_on_rsx_params);

void BLS_InitVELonVEL(uint64_t index, int ticks_per_point, int bar_type, double bar_parameter,
                      BLSParams_VELonVEL vel_on_vel_params);

void BLS_InitJMACCX(uint64_t index, int ticks_per_point, int bar_type, double bar_parameter,
                    BLSParams_JMACCX jma_ccx_params);

void BLS_InitJMAXover(uint64_t index, int ticks_per_point, int bar_type, double bar_parameter,
                      BLSParams_JMAXover jma_ccx_params);

void BLS_InitTrend(uint64_t index, int ticks_per_point, int bar_type, double bar_parameter,
                   BLSParams_Trend trend_params);

void BLS_InitGRBSystem(uint64_t index, int ticks_per_point, int bar_type, double bar_parameter,
                       BLSParams_GRBSystem grb_system_params);

} // extern "C"

#endif // BLS_EXTERN_BLS_STRATEGIES_HPP_
