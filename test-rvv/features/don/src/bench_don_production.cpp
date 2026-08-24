/*
 * 本文件做什么：
 * 这个 bench（性能测试）只通过真实
 * `DifferenceOfNormalsEstimation::computeFeature()` 入口运行 DON
 * （Difference of Normals，法线差分）逐点热点。它不包含 test-only
 * helper，因此反汇编 gate（验收条件）可以区分“生产路径真的出现 RVV
 * 指令”和“只有诊断 helper 出现 RVV 指令”。
 *
 * 证据边界：
 * QEMU 侧只用它编译和 dump asm（反汇编）；真实性能必须使用板卡或目标硬件
 * repeated board summary（重复板卡摘要）。
 */

#include <pcl/features/don.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

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

struct Args {
  std::size_t points = 262144;
  int iterations = 30;
  int warmup = 4;
  std::string case_filter = "all";
};

struct BenchResult {
  double avg_us = 0.0;
  std::uint64_t checksum = 0;
};

Args
parseArgs(int argc, char** argv)
{
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (key == "--points" && i + 1 < argc)
      args.points = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
    else if (key == "--iterations" && i + 1 < argc)
      args.iterations = std::atoi(argv[++i]);
    else if (key == "--warmup-iterations" && i + 1 < argc)
      args.warmup = std::atoi(argv[++i]);
    else if (key == "--case-filter" && i + 1 < argc)
      args.case_filter = argv[++i];
  }
  return args;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeInputCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(count);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    (*cloud)[i].x = static_cast<float>(i) * 0.25f;
    (*cloud)[i].y = static_cast<float>(i % 11) - 3.0f;
    (*cloud)[i].z = static_cast<float>(i % 7) + 0.5f;
  }
  return cloud;
}

pcl::PointCloud<pcl::Normal>::Ptr
makeNormals(const std::size_t count, const float bias)
{
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>>();
  normals->width = static_cast<std::uint32_t>(count);
  normals->height = 1;
  normals->is_dense = false;
  normals->points.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    (*normals)[i].normal_x = bias + static_cast<float>(i % 97) * 0.03125f;
    (*normals)[i].normal_y = bias * 0.5f + static_cast<float>(i % 53) * 0.0625f;
    (*normals)[i].normal_z = bias * 0.25f - static_cast<float>(i % 31) * 0.125f;
  }
  if (count > 9)
    (*normals)[9].normal_x = std::numeric_limits<float>::quiet_NaN();
  if (count > 31)
    (*normals)[31].normal_z = std::numeric_limits<float>::infinity();
  return normals;
}

std::uint64_t
checksum(const pcl::PointCloud<pcl::Normal>& output)
{
  std::uint64_t seed = 1469598103934665603ull;
  for (const auto& normal : output) {
    const auto qx = static_cast<std::int64_t>(normal.normal_x * 1000000.0f);
    const auto qy = static_cast<std::int64_t>(normal.normal_y * 1000000.0f);
    const auto qz = static_cast<std::int64_t>(normal.normal_z * 1000000.0f);
    const auto qc = static_cast<std::int64_t>(normal.curvature * 1000000.0f);
    seed ^= static_cast<std::uint64_t>(qx) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint64_t>(qy) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint64_t>(qz) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint64_t>(qc) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  }
  return seed;
}

BenchResult
measureProductionDoN(const Args& args)
{
  const auto input = makeInputCloud(args.points);
  const auto small = makeNormals(args.points, 1.0f);
  const auto large = makeNormals(args.points, -0.5f);
  pcl::DifferenceOfNormalsEstimation<pcl::PointXYZ, pcl::Normal, pcl::Normal> estimator;
  estimator.setInputCloud(input);
  estimator.setNormalScaleSmall(small);
  estimator.setNormalScaleLarge(large);
  pcl::Feature<pcl::PointXYZ, pcl::Normal>& feature = estimator;

  pcl::PointCloud<pcl::Normal> output;
  output.resize(args.points);
  for (int i = 0; i < args.warmup; ++i) {
    feature.compute(output);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < args.iterations; ++i) {
    feature.compute(output);
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_us = std::chrono::duration<double, std::micro>(end - start).count();
  doNotOptimize(output.data());
  const std::uint64_t fingerprint = checksum(output);
  doNotOptimize(fingerprint);
  return {total_us / static_cast<double>(std::max(args.iterations, 1)), fingerprint};
}

} // namespace

int
main(int argc, char** argv)
{
  const Args args = parseArgs(argc, argv);
  std::cout << "============================================================\n";
  std::cout << " PCL DON production Benchmark\n";
  std::cout << " Iterations: " << args.iterations << "\n";
  std::cout << " Warmup Iterations: " << args.warmup << "\n";
  std::cout << " Dataset: don_production_direct points=" << args.points
            << " iterations=" << args.iterations << " unit=us/iter\n";
#if defined(__RVV10__)
  std::cout << " build: __RVV10__ defined\n";
#else
  std::cout << " build: __RVV10__ NOT defined\n";
#endif
  std::cout << "============================================================\n";

  if (args.case_filter == "all" || args.case_filter == "don_production_direct") {
    const BenchResult result = measureProductionDoN(args);
    std::cout << std::left << std::setw(56)
              << "don_production_direct,points=" + std::to_string(args.points) << ": " << std::fixed
              << std::setprecision(4) << result.avg_us << " us/iter\n";
    std::cout << " Checksum: " << result.checksum << "\n";
  }
  return 0;
}
