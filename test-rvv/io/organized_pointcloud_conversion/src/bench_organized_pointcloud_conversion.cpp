/*
 * 本文件做什么：
 * 这是 organized_pointcloud_conversion 首阶段 diagnostic benchmark（诊断性能测试）
 * 入口。Std build（标量构建）调用 PCL 当前 OrganizedConversion 标量路径；RVV build
 * 调用 test-only RVV candidate（测试专用 RVV 候选）。输出格式供
 * analyze_bench_compare.py 和 Evidence Doctor（证据体检）读取。
 *
 * 证据边界：
 * QEMU（仿真器）只用于构建或日志形状，不作为性能结论。真实性能只看板卡或目标硬件。
 */

#include "organized_pointcloud_conversion.h"

#include <pcl/compression/organized_pointcloud_conversion.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace opc = pcl::io::rvv_test::organized_pointcloud_conversion;

namespace {

constexpr float kFocalLength = 525.0f;
constexpr float kDisparityShift = 2.0f;
constexpr float kDisparityScale = 0.5f;
constexpr int kDefaultIterations = 30;
constexpr int kDefaultWarmupIterations = 5;

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

int
parseIntArg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter") {
      const std::string selected(argv[i + 1]);
      if (selected == requested || selected == "all")
        return true;
      if (!selected.empty() && selected.back() == '*') {
        const std::string prefix = selected.substr(0, selected.size() - 1);
        return requested.rfind(prefix, 0) == 0;
      }
      return false;
    }
  }
  return true;
}

void
runCase(const std::string& name,
        const int iterations,
        const int warmup_iterations,
        const std::function<std::uint64_t()>& fn)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i)
    checksum = opc::mixChecksum(checksum, fn() + static_cast<std::uint64_t>(i + 1));

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum = opc::mixChecksum(
        checksum, fn() + static_cast<std::uint64_t>(warmup_iterations + i + 1));
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(56) << name << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << '\n';
  doNotOptimize(checksum);
}

std::uint64_t
runConversion(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::vector<std::uint16_t> disparity;
#if defined(__RVV10__)
  opc::convertPointXYZCloudToDisparityDiagnostic(
      cloud, kFocalLength, kDisparityShift, kDisparityScale, disparity);
#else
  std::vector<std::uint8_t> unused_color;
  pcl::io::OrganizedConversion<pcl::PointXYZ>::convert(cloud,
                                                       kFocalLength,
                                                       kDisparityShift,
                                                       kDisparityScale,
                                                       false,
                                                       disparity,
                                                       unused_color);
#endif
  return opc::checksumDisparity(disparity);
}

template <typename PointT>
std::uint64_t
runProductionConversion(const pcl::PointCloud<PointT>& cloud)
{
  std::vector<std::uint16_t> disparity;
  std::vector<std::uint8_t> unused_color;
  pcl::io::OrganizedConversion<PointT>::convert(cloud,
                                                kFocalLength,
                                                kDisparityShift,
                                                kDisparityScale,
                                                false,
                                                disparity,
                                                unused_color);
  return opc::checksumDisparity(disparity);
}

std::uint64_t
runColorConversion(const pcl::PointCloud<pcl::PointXYZRGB>& cloud, const bool convert_to_mono)
{
  std::vector<std::uint16_t> disparity;
  std::vector<std::uint8_t> color;
#if defined(__RVV10__)
  opc::convertPointXYZRGBCloudToDisparityColorFusedDiagnostic(cloud,
                                                              kFocalLength,
                                                              kDisparityShift,
                                                              kDisparityScale,
                                                              convert_to_mono,
                                                              disparity,
                                                              color);
#else
  pcl::io::OrganizedConversion<pcl::PointXYZRGB>::convert(cloud,
                                                          kFocalLength,
                                                          kDisparityShift,
                                                          kDisparityScale,
                                                          convert_to_mono,
                                                          disparity,
                                                          color);
#endif
  return opc::mixChecksum(opc::checksumDisparity(disparity), opc::checksumBytes(color));
}

template <typename PointT>
std::uint64_t
runProductionColorConversion(const pcl::PointCloud<PointT>& cloud,
                             const bool convert_to_mono)
{
  std::vector<std::uint16_t> disparity;
  std::vector<std::uint8_t> color;
  pcl::io::OrganizedConversion<PointT>::convert(cloud,
                                                kFocalLength,
                                                kDisparityShift,
                                                kDisparityScale,
                                                convert_to_mono,
                                                disparity,
                                                color);
  return opc::mixChecksum(opc::checksumDisparity(disparity), opc::checksumBytes(color));
}

std::uint64_t
runDecodeDisparityConversion(const std::vector<std::uint16_t>& disparity,
                             const std::size_t width,
                             const std::size_t height)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
#if defined(__RVV10__)
  opc::convertDisparityToPointXYZCloudDiagnostic(disparity,
                                                 width,
                                                 height,
                                                 kFocalLength,
                                                 kDisparityShift,
                                                 kDisparityScale,
                                                 cloud);
#else
  auto disparity_copy = disparity;
  std::vector<std::uint8_t> unused_color;
  pcl::io::OrganizedConversion<pcl::PointXYZ>::convert(disparity_copy,
                                                       unused_color,
                                                       false,
                                                       width,
                                                       height,
                                                       kFocalLength,
                                                       kDisparityShift,
                                                       kDisparityScale,
                                                       cloud);
#endif
  return opc::checksumPointXYZCloud(cloud);
}

template <typename PointT>
std::uint64_t
runFullEncodePointCloud(const pcl::PointCloud<PointT>& cloud,
                        const bool do_color_encoding,
                        const bool convert_to_mono)
{
  std::ostringstream compressed;
  opc::encodePointCloudShaped(cloud,
                              compressed,
                              do_color_encoding,
                              convert_to_mono,
                              1 /* Z_BEST_SPEED */);
  return opc::checksumStringBytes(compressed.str());
}

template <typename PointT>
std::uint64_t
runAnalyzeOrganizedCloud(const pcl::PointCloud<PointT>& cloud)
{
  float max_depth = 0.0f;
  float focal_length = 0.0f;
  opc::analyzeOrganizedCloudDiagnostic(cloud, max_depth, focal_length);
  return opc::checksumAnalyzeResult(max_depth, focal_length);
}

template <typename PointT>
std::uint64_t
runProductionAnalyzeOrganizedCloudDetail(const pcl::PointCloud<PointT>& cloud)
{
  float max_depth = 0.0f;
  float focal_length = 0.0f;
  pcl::io::organized_compression_detail::analyzeOrganizedCloud(
      cloud, max_depth, focal_length);
  return opc::checksumAnalyzeResult(max_depth, focal_length);
}

void
runCloudCase(const int argc,
             char** argv,
             const std::string& label,
             const std::size_t size,
             const opc::InvalidPattern invalid_pattern,
             const int iterations,
             const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makePointXYZCloud(size, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud]() { return runConversion(cloud); });
}

void
runAnalyzePointXYZCase(const int argc,
                       char** argv,
                       const std::string& label,
                       const std::size_t width,
                       const std::size_t height,
                       const opc::InvalidPattern invalid_pattern,
                       const int iterations,
                       const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makeOrganizedPointXYZCloud(width, height, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud]() { return runAnalyzeOrganizedCloud(cloud); });
}

void
runProductionAnalyzePointXYZCase(const int argc,
                                 char** argv,
                                 const std::string& label,
                                 const std::size_t width,
                                 const std::size_t height,
                                 const opc::InvalidPattern invalid_pattern,
                                 const int iterations,
                                 const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makeOrganizedPointXYZCloud(width, height, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud]() { return runProductionAnalyzeOrganizedCloudDetail(cloud); });
}

void
runProductionAnalyzePointXYZICase(const int argc,
                                  char** argv,
                                  const std::string& label,
                                  const std::size_t width,
                                  const std::size_t height,
                                  const opc::InvalidPattern invalid_pattern,
                                  const int iterations,
                                  const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  auto cloud = opc::makePointXYZICloud(width * height, invalid_pattern);
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud]() { return runProductionAnalyzeOrganizedCloudDetail(cloud); });
}

void
runColorCloudCase(const int argc,
                  char** argv,
                  const std::string& label,
                  const std::size_t size,
                  const opc::InvalidPattern invalid_pattern,
                  const bool convert_to_mono,
                  const int iterations,
                  const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makePointXYZRGBCloud(size, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud, convert_to_mono]() {
            return runColorConversion(cloud, convert_to_mono);
          });
}

void
runProductionCloudCase(const int argc,
                       char** argv,
                       const std::string& label,
                       const std::size_t size,
                       const opc::InvalidPattern invalid_pattern,
                       const int iterations,
                       const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makePointXYZCloud(size, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud]() { return runProductionConversion(cloud); });
}

void
runPointXYZIProductionCloudCase(const int argc,
                                char** argv,
                                const std::string& label,
                                const std::size_t size,
                                const opc::InvalidPattern invalid_pattern,
                                const int iterations,
                                const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makePointXYZICloud(size, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud]() { return runProductionConversion(cloud); });
}

void
runProductionColorCloudCase(const int argc,
                            char** argv,
                            const std::string& label,
                            const std::size_t size,
                            const opc::InvalidPattern invalid_pattern,
                            const bool convert_to_mono,
                            const int iterations,
                            const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makePointXYZRGBCloud(size, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud, convert_to_mono]() {
            return runProductionColorConversion(cloud, convert_to_mono);
          });
}

void
runPointXYZRGBAProductionColorCloudCase(const int argc,
                                        char** argv,
                                        const std::string& label,
                                        const std::size_t size,
                                        const opc::InvalidPattern invalid_pattern,
                                        const bool convert_to_mono,
                                        const int iterations,
                                        const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makePointXYZRGBACloud(size, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud, convert_to_mono]() {
            return runProductionColorConversion(cloud, convert_to_mono);
          });
}

void
runDecodeDisparityCase(const int argc,
                       char** argv,
                       const std::string& label,
                       const std::size_t width,
                       const std::size_t height,
                       const opc::InvalidPattern invalid_pattern,
                       const int iterations,
                       const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto disparity = opc::makeDisparityImage(width, height, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&disparity, width, height]() {
            return runDecodeDisparityConversion(disparity, width, height);
          });
}

void
runFullEncodePointXYZCase(const int argc,
                          char** argv,
                          const std::string& label,
                          const std::size_t width,
                          const std::size_t height,
                          const opc::InvalidPattern invalid_pattern,
                          const int iterations,
                          const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makeOrganizedPointXYZCloud(width, height, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud]() { return runFullEncodePointCloud(cloud, false, false); });
}

void
runFullEncodePointXYZRGBCase(const int argc,
                             char** argv,
                             const std::string& label,
                             const std::size_t width,
                             const std::size_t height,
                             const opc::InvalidPattern invalid_pattern,
                             const bool convert_to_mono,
                             const int iterations,
                             const int warmup_iterations)
{
  if (!caseEnabled(argc, argv, label))
    return;

  const auto cloud = opc::makeOrganizedPointXYZRGBCloud(width, height, invalid_pattern);
  runCase(label,
          iterations,
          warmup_iterations,
          [&cloud, convert_to_mono]() {
            return runFullEncodePointCloud(cloud, true, convert_to_mono);
          });
}

}  // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "Dataset: synthetic organized PointXYZ clouds for cloud-to-disparity\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build: RVV diagnostic candidate (__RVV10__)\n";
#else
  std::cout << "Build: scalar OrganizedConversion path\n";
#endif

  runCloudCase(argc,
               argv,
               "pointxyz_disparity_dense_307k",
               640u * 480u,
               opc::InvalidPattern::finite_only,
               iterations,
               warmup_iterations);
  runCloudCase(argc,
               argv,
               "pointxyz_disparity_mixed_invalid_307k",
               640u * 480u,
               opc::InvalidPattern::mixed_invalid,
               iterations,
               warmup_iterations);
  runCloudCase(argc,
               argv,
               "pointxyz_disparity_dense_1m",
               1024u * 1024u,
               opc::InvalidPattern::finite_only,
               iterations,
               warmup_iterations);
  runAnalyzePointXYZCase(argc,
                         argv,
                         "analyze_pointxyz_organized_dense_307k",
                         640u,
                         480u,
                         opc::InvalidPattern::finite_only,
                         iterations,
                         warmup_iterations);
  runAnalyzePointXYZCase(argc,
                         argv,
                         "analyze_pointxyz_organized_mixed_invalid_307k",
                         640u,
                         480u,
                         opc::InvalidPattern::mixed_invalid,
                         iterations,
                         warmup_iterations);
  runProductionAnalyzePointXYZCase(argc,
                                   argv,
                                   "production_analyze_detail_pointxyz_dense_307k",
                                   640u,
                                   480u,
                                   opc::InvalidPattern::finite_only,
                                   iterations,
                                   warmup_iterations);
  runProductionAnalyzePointXYZCase(argc,
                                   argv,
                                   "production_analyze_detail_pointxyz_mixed_invalid_307k",
                                   640u,
                                   480u,
                                   opc::InvalidPattern::mixed_invalid,
                                   iterations,
                                   warmup_iterations);
  runProductionAnalyzePointXYZICase(argc,
                                    argv,
                                    "production_analyze_detail_pointxyzi_mixed_invalid_307k",
                                    640u,
                                    480u,
                                    opc::InvalidPattern::mixed_invalid,
                                    iterations,
                                    warmup_iterations);
  runColorCloudCase(argc,
                    argv,
                    "pointxyzrgb_disparity_rgb_dense_307k",
                    640u * 480u,
                    opc::InvalidPattern::finite_only,
                    false,
                    iterations,
                    warmup_iterations);
  runColorCloudCase(argc,
                    argv,
                    "pointxyzrgb_disparity_mono_dense_307k",
                    640u * 480u,
                    opc::InvalidPattern::finite_only,
                    true,
                    iterations,
                    warmup_iterations);
  runColorCloudCase(argc,
                    argv,
                    "pointxyzrgb_disparity_rgb_mixed_invalid_307k",
                    640u * 480u,
                    opc::InvalidPattern::mixed_invalid,
                    false,
                    iterations,
                    warmup_iterations);
  runDecodeDisparityCase(argc,
                         argv,
                         "pointxyz_decode_disparity_dense_307k",
                         640u,
                         480u,
                         opc::InvalidPattern::finite_only,
                         iterations,
                         warmup_iterations);
  runDecodeDisparityCase(argc,
                         argv,
                         "pointxyz_decode_disparity_mixed_invalid_307k",
                         640u,
                         480u,
                         opc::InvalidPattern::mixed_invalid,
                         iterations,
                         warmup_iterations);
  runDecodeDisparityCase(argc,
                         argv,
                         "pointxyz_decode_disparity_dense_1m",
                         1024u,
                         1024u,
                         opc::InvalidPattern::finite_only,
                         iterations,
                         warmup_iterations);
  runProductionCloudCase(argc,
                         argv,
                         "production_pointxyz_disparity_dense_307k",
                         640u * 480u,
                         opc::InvalidPattern::finite_only,
                         iterations,
                         warmup_iterations);
  runProductionCloudCase(argc,
                         argv,
                         "production_pointxyz_disparity_mixed_invalid_307k",
                         640u * 480u,
                         opc::InvalidPattern::mixed_invalid,
                         iterations,
                         warmup_iterations);
  runPointXYZIProductionCloudCase(argc,
                                  argv,
                                  "production_pointxyzi_disparity_dense_307k",
                                  640u * 480u,
                                  opc::InvalidPattern::finite_only,
                                  iterations,
                                  warmup_iterations);
  runPointXYZIProductionCloudCase(argc,
                                  argv,
                                  "production_pointxyzi_disparity_mixed_invalid_307k",
                                  640u * 480u,
                                  opc::InvalidPattern::mixed_invalid,
                                  iterations,
                                  warmup_iterations);
  runProductionColorCloudCase(argc,
                              argv,
                              "production_pointxyzrgb_disparity_rgb_dense_307k",
                              640u * 480u,
                              opc::InvalidPattern::finite_only,
                              false,
                              iterations,
                              warmup_iterations);
  runProductionColorCloudCase(argc,
                              argv,
                              "production_pointxyzrgb_disparity_mono_dense_307k",
                              640u * 480u,
                              opc::InvalidPattern::finite_only,
                              true,
                              iterations,
                              warmup_iterations);
  runProductionColorCloudCase(argc,
                              argv,
                              "production_pointxyzrgb_disparity_rgb_mixed_invalid_307k",
                              640u * 480u,
                              opc::InvalidPattern::mixed_invalid,
                              false,
                              iterations,
                              warmup_iterations);
  runPointXYZRGBAProductionColorCloudCase(argc,
                                          argv,
                                          "production_pointxyzrgba_disparity_rgb_dense_307k",
                                          640u * 480u,
                                          opc::InvalidPattern::finite_only,
                                          false,
                                          iterations,
                                          warmup_iterations);
  runPointXYZRGBAProductionColorCloudCase(argc,
                                          argv,
                                          "production_pointxyzrgba_disparity_rgb_mixed_invalid_307k",
                                          640u * 480u,
                                          opc::InvalidPattern::mixed_invalid,
                                          false,
                                          iterations,
                                          warmup_iterations);
  runFullEncodePointXYZCase(argc,
                            argv,
                            "production_full_pointxyz_encode_dense_307k",
                            640u,
                            480u,
                            opc::InvalidPattern::finite_only,
                            iterations,
                            warmup_iterations);
  runFullEncodePointXYZRGBCase(argc,
                               argv,
                               "production_full_pointxyzrgb_encode_rgb_dense_307k",
                               640u,
                               480u,
                               opc::InvalidPattern::finite_only,
                               false,
                               iterations,
                               warmup_iterations);
  runFullEncodePointXYZRGBCase(argc,
                               argv,
                               "production_full_pointxyzrgb_encode_mono_dense_307k",
                               640u,
                               480u,
                               opc::InvalidPattern::finite_only,
                               true,
                               iterations,
                               warmup_iterations);

  return 0;
}
