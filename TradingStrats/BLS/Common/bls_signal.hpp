#ifndef BLS_STRATEGIES_SIGNAL_HPP_
#define BLS_STRATEGIES_SIGNAL_HPP_

extern "C" {

struct BLSSignal {
  bool market_entry; // true if enter at market
  bool market_exit;  // true if exit at market
  bool long_pos;     // true long, false short
  bool stop;         // true use stop entry, false use limit entry

  // if any price is 0, that order is disabled

  double entry_price; // the entry price

  // stop loss for short and long positions
  double stop_loss_short, stop_loss_long;

  // profit target for short and long positions
  double profit_target_short, profit_target_long;

  void Reset() {
    market_entry = false;
    market_exit = false;
    long_pos = false;
    stop = false;

    entry_price = 0.0;
    stop_loss_short = 0.0;
    stop_loss_long = 0.0;
    profit_target_short = 0.0;
    profit_target_long = 0.0;
  }
};

} // extern "C"

#endif // BLS_STRATEGIES_SIGNAL_HPP_
