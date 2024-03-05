#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace MAQUETTE {
namespace BLS {

class Histogram {
public:
  Histogram(const std::vector<int> &input_values) {
    assert(input_values.size() > 0);
    auto values = input_values;

    // the bin_width_ is the smallest difference between unique values, sort all
    // values so we can find it
    std::stable_sort(values.begin(), values.end());
    bin_width_ = std::numeric_limits<int>::max();
    bool got_a_diff = false;
    int min = values[0];
    int max = values[0];
    for (size_t i = 1; i < values.size(); ++i) {
      min = std::min(min, values[i]);
      max = std::max(max, values[i]);
      int diff = values[i] - values[i - 1];
      if (diff > 0.0) {
        bin_width_ = std::min(bin_width_, diff);
        got_a_diff = true;
      }
    }

    if (!got_a_diff) {
      // all values are the same
      bin_width_ = 0;
      lower_edges_ = std::vector<int>(1);
      counts_ = std::vector<int>(1);

      lower_edges_[0] = values[0];
      counts_[0] = int(values.size());
      return;
    }

    size_t num_bins = size_t(std::ceil((max + bin_width_ - min) / bin_width_));
    num_bins = std::max(num_bins, size_t(1));

    lower_edges_.resize(num_bins);
    counts_.resize(num_bins);

    for (size_t i = 0; i < num_bins; ++i) {
      lower_edges_[i] = min + int(i) * bin_width_;
      counts_[i] = 0;
    }

    // get counts, histogram bins include the lower edge and not the upper edge
    for (size_t i = 0; i < values.size(); ++i) {
      int idx = (values[i] - min) / bin_width_;
      if ((idx < 0) || (idx >= int(num_bins))) {
        printf(
            "min = %i, max = %i, bin_width = %i, num_bins = %lu, values[%lu] "
            "= %i, idx = %i, values.size() = %lu\n",
            min, max, bin_width_, num_bins, i, values[i], idx, values.size());
        throw std::runtime_error("Index error in Histogram");
      }

      counts_[size_t(idx)] += 1;
    }
  }

  auto bin_width() const { return bin_width_; }
  const auto &lower_edges() const { return lower_edges_; }
  const auto &counts() const { return counts_; }

private:
  int bin_width_;
  std::vector<int> lower_edges_, counts_;
};

} // namespace BLS
} // namespace MAQUETTE
