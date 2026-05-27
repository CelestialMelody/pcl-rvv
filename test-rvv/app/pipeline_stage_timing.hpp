#pragma once

#include <cassert>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace pcl_test_rvv_app {

inline double
chronoDurationMs (std::chrono::high_resolution_clock::time_point a,
                  std::chrono::high_resolution_clock::time_point b)
{
  return std::chrono::duration<double, std::milli> (b - a).count ();
}

/** 在各阶段 `record` 后，对 `iterations` 次计时迭代求平均每迭代毫秒数。 */
class StageAccumulator
{
public:
  explicit StageAccumulator (std::initializer_list<const char*> stage_names)
  {
    for (const char* s : stage_names)
    {
      names_.emplace_back (s);
      sum_ms_.push_back (0.0);
    }
  }

  std::size_t
  size () const
  {
    return names_.size ();
  }

  void
  record (std::size_t index, double ms)
  {
    assert (index < sum_ms_.size ());
    sum_ms_[index] += ms;
  }

  void
  printSummary (int iterations, std::ostream& os = std::cout) const
  {
    if (iterations <= 0 || names_.empty ())
      return;
    double total = 0.0;
    for (double v : sum_ms_)
      total += v;
    const double inv = 1.0 / static_cast<double> (iterations);
    os << "--- 分阶段耗时（平均每迭代 ms；百分比为相对本节各阶段之和）；标签可多行，续行与首行左对齐 ---\n";

    auto first_line = [] (std::string_view s) -> std::string_view {
      auto p = s.find ('\n');
      return (p == std::string_view::npos) ? s : s.substr (0, p);
    };

    std::size_t col = 54u;
    for (const auto& nm : names_)
      col = std::max<std::size_t> (col, first_line (nm).size ());
    col = std::min<std::size_t> (col + 4u, 120u);
    const int icol = static_cast<int> (col);

    for (std::size_t i = 0; i < names_.size (); ++i)
    {
      const double avg = sum_ms_[i] * inv;
      const double pct = (total > 1e-9) ? (100.0 * sum_ms_[i] / total) : 0.0;
      const std::string& nm = names_[i];
      std::string_view line0 = first_line (nm);
      os << std::left << std::setw (icol) << line0 << ": " << std::fixed << std::setprecision (4) << avg
         << " ms/iter  (" << std::setprecision (1) << pct << "% of staged)\n";
      const auto p = nm.find ('\n');
      if (p != std::string::npos && p + 1 < nm.size ())
      {
        std::string_view rest = std::string_view (nm).substr (p + 1);
        os << rest;
        if (rest.back () != '\n')
          os << '\n';
      }
    }
    os << std::left << std::setw (icol) << "staged_sum" << ": " << std::fixed << std::setprecision (4) << (total * inv)
       << " ms/iter\n";
  }

private:
  std::vector<std::string> names_;
  std::vector<double> sum_ms_;
};

template <typename Fn>
void
runTimedBenchmarkWithStages (const std::string& total_label, Fn&& once, StageAccumulator& stages, int iterations,
                             int warmup, int label_width)
{
  for (int i = 0; i < warmup; ++i)
    once (nullptr);
  const auto t0 = std::chrono::high_resolution_clock::now ();
  for (int i = 0; i < iterations; ++i)
    once (&stages);
  const auto t1 = std::chrono::high_resolution_clock::now ();
  const double total_ms = std::chrono::duration<double, std::milli> (t1 - t0).count ();
  const double avg_ms = total_ms / static_cast<double> (iterations);
  std::cout << std::left << std::setw (label_width) << total_label << ": " << std::fixed << std::setprecision (4)
            << avg_ms << " ms/iter\n";
  stages.printSummary (iterations);
}

} // namespace pcl_test_rvv_app
