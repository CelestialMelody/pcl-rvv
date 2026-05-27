/**
 * bench_norms.cpp — pcl/common/norms.h 基准（主要范数 + selectNorm）
 *
 * 覆盖连续 float：裸指针与 std::vector<float>（与仓库内 NARF / MultiscaleFeaturePersistence 用法一致）。
 */
#include <pcl/common/norms.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

template <typename T>
inline void
doNotOptimize (const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile ("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

class Benchmarker
{
public:
  explicit Benchmarker (const std::string& name) : name_ (name) {}

  void
  run (const std::function<void ()>& func, int iterations = 30, int warmup = 4)
  {
    for (int i = 0; i < warmup; ++i)
      func ();
    const auto start = std::chrono::high_resolution_clock::now ();
    for (int i = 0; i < iterations; ++i)
      func ();
    const auto end = std::chrono::high_resolution_clock::now ();
    const double total_us = std::chrono::duration<double, std::micro> (end - start).count ();
    const double avg_us = total_us / iterations;
    std::cout << std::left << std::setw (56) << name_ << ": " << std::fixed << std::setprecision (4) << avg_us
              << " us/iter\n";
  }

private:
  std::string name_;
};

void
fillRandom (std::vector<float>& a, std::vector<float>& b, std::uint32_t seed)
{
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (-2.0f, 2.0f);
  for (std::size_t i = 0; i < a.size (); ++i)
  {
    a[i] = u (rng);
    b[i] = u (rng);
  }
}

} // namespace

int
main ()
{
  constexpr int kBenchIters = 30;
  constexpr int kWarmup = 4;
  const std::vector<int> dims = {8, 36, 128, 512, 4096};
  constexpr float kP1 = 0.7f;
  constexpr float kP2 = 1.3f;

  std::cout << "============================================================\n";
  std::cout << " PCL norms.hpp Benchmark\n";
  std::cout << " Iterations: " << kBenchIters << "\n";
  std::cout << " Dataset: norms dim={8,36,128,512,4096} iterations=" << kBenchIters << " unit=us/iter\n";
#if defined(__RVV10__)
  std::cout << " build: __RVV10__ defined\n";
#else
  std::cout << " build: __RVV10__ NOT defined\n";
#endif
  std::cout << "============================================================\n";

  for (int dim : dims)
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fillRandom (a, b, 42u + static_cast<std::uint32_t> (dim));
    float* pa = a.data ();
    float* pb = b.data ();

    std::cout << "\n--- dim=" << dim << " ---\n";

    const auto tag = [&dim] (const char* base) {
      return std::string (base) + ",dim=" + std::to_string (dim);
    };

    {
      Benchmarker bench (tag ("L1_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::L1_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("L2_Norm_SQR(float*)"));
      bench.run (
          [&] () {
            float s = pcl::L2_Norm_SQR (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("L2_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::L2_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("Linf_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::Linf_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("JM_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::JM_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("B_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::B_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("Sublinear_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::Sublinear_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("CS_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::CS_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("Div_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::Div_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("KL_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::KL_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("HIK_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::HIK_Norm (pa, pb, dim);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("PF_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::PF_Norm (pa, pb, dim, kP1, kP2);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
    {
      Benchmarker bench (tag ("K_Norm(float*)"));
      bench.run (
          [&] () {
            float s = pcl::K_Norm (pa, pb, dim, kP1, kP2);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }

    for (pcl::NormType nt :
         {pcl::L1,
          pcl::L2_SQR,
          pcl::L2,
          pcl::LINF,
          pcl::JM,
          pcl::B,
          pcl::SUBLINEAR,
          pcl::CS,
          pcl::DIV,
          pcl::KL,
          pcl::HIK})
    {
      const char* name = nullptr;
      switch (nt)
      {
        case pcl::L1:
          name = "L1";
          break;
        case pcl::L2_SQR:
          name = "L2_SQR";
          break;
        case pcl::L2:
          name = "L2";
          break;
        case pcl::LINF:
          name = "LINF";
          break;
        case pcl::JM:
          name = "JM";
          break;
        case pcl::B:
          name = "B";
          break;
        case pcl::SUBLINEAR:
          name = "SUBLINEAR";
          break;
        case pcl::CS:
          name = "CS";
          break;
        case pcl::DIV:
          name = "DIV";
          break;
        case pcl::KL:
          name = "KL";
          break;
        case pcl::HIK:
          name = "HIK";
          break;
        default:
          name = "?";
          break;
      }
      Benchmarker bench (
          tag ((std::string ("selectNorm(vec,") + name + ")").c_str ()));
      bench.run (
          [&] () {
            float s = pcl::selectNorm<std::vector<float>> (a, b, dim, nt);
            doNotOptimize (s);
          },
          kBenchIters, kWarmup);
    }
  }

  std::cout << "============================================================\n";

  return 0;
}
