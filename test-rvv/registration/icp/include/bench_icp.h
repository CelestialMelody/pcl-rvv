/*
 * 本文件做什么：
 * ICP bench 专用聚合入口。bench 源码使用这里的 helper 生成输入、运行 production transformCloud 并输出
 * `analyze_bench_compare.py` 可解析的日志形状。
 */

#pragma once

#include "icp.h"

#include <pcl/registration/icp.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace pcl::registration::rvv_icp_bench {

namespace support = pcl::registration::rvv_icp_support;

constexpr int kIterations = 20;
constexpr int kWarmupIterations = 3;
constexpr std::size_t kBannerWidth = 96;

template <typename PointT>
class ExposedICP : public pcl::IterativeClosestPoint<PointT, PointT> {
  using Base = pcl::IterativeClosestPoint<PointT, PointT>;

public:
  using Matrix4 = typename Base::Matrix4;
  using PointCloudSource = typename Base::PointCloudSource;

  void
  initializeSource(const PointCloudSource& input)
  {
    this->setInputSource(pcl::make_shared<PointCloudSource>(input));
  }

  void
  transformPublic(const PointCloudSource& input,
                  PointCloudSource& output,
                  const Matrix4& transform)
  {
    this->transformCloud(input, output, transform);
  }
};

inline void
printBanner(char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

class Benchmarker {
public:
  explicit Benchmarker(std::string name) : name_(std::move(name)) {}

  void
  run(const std::function<void()>& func,
      int iterations = kIterations,
      int warmup = kWarmupIterations) const
  {
    for (int i = 0; i < warmup; ++i)
      func();

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto stop = std::chrono::steady_clock::now();
    const double total_ms =
        std::chrono::duration<double, std::milli>(stop - start).count();
    std::cout << std::left << std::setw(64) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void
  setChecksum(std::uint64_t checksum) const
  {
    checksum_ = checksum;
  }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

inline void
benchPointXYZ(const std::string& name, std::size_t size)
{
  const auto input = support::makePointXYZCloud(size);
  auto output = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointXYZ> icp;
  icp.initializeSource(input);
  Benchmarker bench(name);
  bench.run([&]() {
    icp.transformPublic(input, output, transform);
    bench.setChecksum(support::checksumXYZ(output));
    doNotOptimize(output);
  });
}

inline void
benchPointNormal(const std::string& name, std::size_t size)
{
  const auto input = support::makePointNormalCloud(size);
  auto output = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointNormal> icp;
  icp.initializeSource(input);
  Benchmarker bench(name);
  bench.run([&]() {
    icp.transformPublic(input, output, transform);
    bench.setChecksum(support::checksumXYZNormal(output));
    doNotOptimize(output);
  });
}

inline int
run_icp_bench()
{
  printBanner('=');
  std::cout << "PCL registration/icp transformCloud diagnostic benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout
      << "Dataset: synthetic PointXYZ and PointNormal clouds; production transformCloud full-cloud\n";
  std::cout << "Iterations: " << kIterations << '\n';
  std::cout << "Warmup Iterations: " << kWarmupIterations << '\n';
  printBanner('-');

  benchPointXYZ("icp transform-cloud xyz 64K", 64 * 1024);
  benchPointXYZ("icp transform-cloud xyz 256K", 256 * 1024);
  benchPointNormal("icp transform-cloud xyz-normal 64K", 64 * 1024);
  benchPointNormal("icp transform-cloud xyz-normal 256K", 256 * 1024);

  printBanner('=');
  return 0;
}

} // namespace pcl::registration::rvv_icp_bench
