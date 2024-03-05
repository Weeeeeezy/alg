#pragma once

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace MAQUETTE {
namespace BLS {

// Applies weighted 3-point median filter across a time series. Weights are
// 1-2-1.
std::vector<double> WeightedMedian3(const std::vector<double> &x) {
  std::array<double, 4> buffer;
  std::vector<double> res(x.size());

  // weights: 1-2-1
  for (int i = 0; i < int(x.size()); ++i) {
    buffer[0] = x[size_t(std::max(0, i - 1))];
    buffer[1] = x[size_t(i)];
    buffer[2] = x[size_t(i)];
    buffer[3] = x[size_t(std::min(int(x.size() - 1), i + 1))];

    std::partial_sort(buffer.begin(), buffer.begin() + 3, buffer.end());

    res[size_t(i)] = 0.5 * (buffer[1] + buffer[2]);
  }

  return res;
}

// Applies 5-point weighted median filter across a time series.  Weights are
// 1-2-3-2-1.
std::vector<double> WeightedMedian5(const std::vector<double> &x) {
  std::array<double, 9> buffer;
  std::vector<double> res(x.size());

  // weights: 1-2-3-2-1
  for (int i = 0; i < int(x.size()); ++i) {
    buffer[0] = x[size_t(std::max(0, i - 2))];
    buffer[1] = x[size_t(std::max(0, i - 1))];
    buffer[2] = x[size_t(std::max(0, i - 1))];
    buffer[3] = x[size_t(i)];
    buffer[4] = x[size_t(i)];
    buffer[5] = x[size_t(i)];
    buffer[6] = x[size_t(std::min(int(x.size() - 1), i + 1))];
    buffer[7] = x[size_t(std::min(int(x.size() - 1), i + 1))];
    buffer[8] = x[size_t(std::min(int(x.size() - 1), i + 2))];

    std::partial_sort(buffer.begin(), buffer.begin() + 5, buffer.end());

    res[size_t(i)] = buffer[4];
  }

  return res;
}

// 3-pole EMA filter.  Smooths vector SERIES.
// lag: decimal number of delay in time slices >= 0
// init: initial value of EMA filter before starting, defaults to x[0]
std::vector<double> EMA3(const std::vector<double> &x, double lag,
                         double init) {
  if (lag < 0.0)
    throw std::invalid_argument("Lag cannot be negative");

  const double a = 3.0 / (3.0 + lag);
  const double b = 1.0 - a;
  std::vector<double> res(x.size());

  double e1 = init;
  double e2 = e1;
  double e3 = e1;

  for (size_t i = 0; i < x.size(); ++i) {
    e1 = a * x[i] + b * e1;
    e2 = a * e1 + b * e2;
    e3 = a * e2 + b * e3;

    res[i] = e3;
  }

  return res;
}

std::vector<double> EMA3(const std::vector<double> &x, double lag) {
  return EMA3(x, lag, x[0]);
}

// Zero-lag 6-pole low-pass block filter applied to a time series (XRW = X-ray
// window) Reasonable performance at end points lag: controls smoothness. Larger
// is smoother.  Range: any value >= 0
std::vector<double> XRW(const std::vector<double> &x, double lag) {
  const size_t N = x.size();
  std::vector<double> res(N);

  // make a reverse copy of x
  auto rev = x;
  std::reverse(rev.begin(), rev.end());

  // PASS 1  -- get preliminary filter endpoints and slopes
  auto fwd_1 = EMA3(x, lag);
  auto bkw_1 = EMA3(rev, lag);

  double slope_lhs = bkw_1[N - 1] - bkw_1[N - 2];
  double slope_rhs = fwd_1[N - 1] - fwd_1[N - 2];

  // PASS 2  -- Bidirectional pass, part 1.  Eval starting point using filter
  // lag and slope
  auto fwd_2 = EMA3(x, lag, bkw_1[N - 1] + (2.0 * lag + 1.0) * slope_lhs);
  auto bkw_2 = EMA3(rev, lag, fwd_1[N - 1] + (2.0 * lag + 1.0) * slope_rhs);

  slope_lhs = bkw_2[N - 1] - bkw_2[N - 2];
  slope_rhs = fwd_2[N - 1] - fwd_2[N - 2];

  // PASS 3  -- Bidirectional pass, part 2.  Eval starting point using filter
  // lag and slope.
  std::reverse(fwd_2.begin(), fwd_2.end());
  std::reverse(bkw_2.begin(), bkw_2.end());

  auto fwd_3 = EMA3(bkw_2, lag, bkw_2[0] + (lag + 1.0) * slope_lhs);
  auto bkw_3 = EMA3(fwd_2, lag, fwd_2[0] + (lag + 1.0) * slope_rhs);

  // combine forward-reverse and reverse-forward filters
  std::reverse(bkw_3.begin(), bkw_3.end());

  for (size_t i = 0; i < N; ++i)
    res[i] = 0.5 * (fwd_3[i] + bkw_3[i]);

  return res;
}

// Finds all segments in the count_ array with values > 0. Returns the first and
// last index of longest segment such segment.  If 2 or more segments have same
// longest length, then that segment with largest sum of counts is chosen.
// enhance == true enables contrast enhancement prior to analysis
std::array<size_t, 2> FindStrongSubrange(const std::vector<double> &x,
                                         bool enhance) {
  std::vector<double> dat(x.size());
  for (size_t i = 0; i < dat.size(); ++i)
    dat[i] = std::max(0.0, double(x[i]));

  if (enhance) {
    // histogram bins are converted to neural stimulus vector. Initialize row of
    // neural cells and eval global cell inhibition. Let cells interact for 40
    // cycles, then output result.
    double cell_decay = 0.8 / double(x.size());
    std::vector<double> stimulus(dat.size());
    double sum = 0.0;
    for (int i = 0; i < int(dat.size()); ++i) {
      // stimulus is dat_i + 0.5 * (dat_{i-1} + dat_{i+1}) extending values at
      // boundaries
      stimulus[size_t(i)] =
          dat[size_t(i)] +
          0.5 * (dat[size_t(std::max(0, i - 1))] +
                 dat[size_t(std::min(int(dat.size() - 1), i + 1))]);
      sum += stimulus[size_t(i)];
    }

    for (size_t i = 0; i < dat.size(); ++i) {
      stimulus[i] /= sum;
      dat[i] = stimulus[i];
    }

    for (int k = 0; k < 40; ++k) {
      double sum1 = 0.0;
      for (size_t i = 0; i < dat.size(); ++i) {
        dat[i] = std::max(0.0, dat[i] + stimulus[i] - cell_decay);
        sum1 += dat[i];
      }

      for (size_t i = 0; i < dat.size(); ++i)
        dat[i] /= sum1;
    }
  }

  // find start and end index of longest segment of non-zero values
  // these are the values for the longest segment
  size_t max_start = 0;
  size_t max_end = 0;
  size_t max_length = 0;
  double max_value_sum = 0.0;

  // these are the values for the current segment found
  size_t current_start = 0;
  size_t current_end = 0;
  // length of the current segment, if 0, we are not currently in a non-zero
  // segment
  size_t current_length = 0;
  double current_value_sum = 0.0;

  for (size_t i = 0; i < dat.size(); ++i) {
    if (dat[i] > 0.0) {
      if (current_length == 0) {
        // this is the start of a new non-zero segment
        current_start = i;
        current_end = i;
        current_length = 1;
        current_value_sum = dat[i];
      } else {
        // this value is part of the current non-zero segment
        current_end = i;
        current_length += 1;
        current_value_sum += dat[i];
      }
    } else {
      // current value is 0
      if (current_length == 0) {
        // we are not currently in a non-zero segment, so don't do anything
      } else {
        // this is the first non-zero value after a non-zero segment, check if
        // this segment is the longest (if it's the same length as the previous,
        // take this if it's value_sum is greater)
        if ((current_length > max_length) ||
            ((current_length == max_length) &&
             (current_value_sum > max_value_sum))) {
          // accept this segment as the max segment
          max_start = current_start;
          max_end = current_end;
          max_length = current_length;
          max_value_sum = current_value_sum;
        }

        // reset current segment
        current_start = 0;
        current_end = 0;
        current_length = 0;
        current_value_sum = 0.0;
      }
    }
  }

  // need to check current segment if the last value is non-zero
  if ((current_length > max_length) ||
      ((current_length == max_length) && (current_value_sum > max_value_sum))) {
    max_start = current_start;
    max_end = current_end;
  }

  return {{max_start, max_end}};
}

std::array<size_t, 2> FindStrongSubrange(const std::vector<int> &x,
                                         bool enhance) {
  std::vector<double> dx(x.size());
  for (size_t i = 0; i < x.size(); ++i)
    dx[i] = double(x[i]);

  return FindStrongSubrange(dx, enhance);
}

} // namespace BLS
} // namespace MAQUETTE
