/*
 * 本文件做什么：
 * 这是 point_cloud_image_extractors 首阶段 production-shaped diagnostic
 * （生产形态诊断）bench 入口。Std build 使用标量参考链路；RVV build
 * 使用测试专用 candidate。输出格式沿用 test-rvv 共享 compare 脚本可解析的
 * label、ms/iter、Total Time 和 checksum。
 *
 * 证据边界：
 * QEMU 运行本文件只证明构建、日志形状和 checksum（校验和）口径；性能结论
 * 只能来自板卡或目标硬件。这里没有修改真实 `PointCloudImageExtractor`
 * production dispatch。
 */

#include "pcie.h"

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace pcie = pcl::io::rvv_test::point_cloud_image_extractors;

namespace {

constexpr int kDefaultIterations = 20;
constexpr int kDefaultWarmupIterations = 3;

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
      std::size_t start = 0;
      while (start <= selected.size()) {
        const std::size_t comma = selected.find(',', start);
        const std::string item = selected.substr(start, comma - start);
        if (item == requested)
          return true;
        if (comma == std::string::npos)
          break;
        start = comma + 1;
      }
      return false;
    }
  }
  return true;
}

template <typename T>
std::uint64_t
checksumValues(const std::vector<T>& values)
{
  std::uint64_t h = 1469598103934665603ull;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(values.data());
  for (std::size_t i = 0; i < values.size() * sizeof(T); ++i) {
    h ^= bytes[i];
    h *= 1099511628211ull;
  }
  return h;
}

template <typename Fill>
void
runByteCase(const int argc,
            char** argv,
            const std::string& label,
            const int iterations,
            const int warmup_iterations,
            Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  std::vector<std::uint8_t> output;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill(output);
    doNotOptimize(output.data());
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill(output);
    doNotOptimize(output.data());
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
  const std::uint64_t checksum =
      checksumValues(output) ^ static_cast<std::uint64_t>(iterations + warmup_iterations);

  std::cout << std::left << std::setw(44) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", bytes: " << output.size() << '\n';
  doNotOptimize(checksum);
}

template <typename Fill>
void
runU16Case(const int argc,
           char** argv,
           const std::string& label,
           const int iterations,
           const int warmup_iterations,
           Fill fill)
{
  if (!caseEnabled(argc, argv, label))
    return;

  std::vector<std::uint16_t> output;
  for (int i = 0; i < warmup_iterations; ++i) {
    fill(output);
    doNotOptimize(output.data());
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    fill(output);
    doNotOptimize(output.data());
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
  const std::uint64_t checksum =
      checksumValues(output) ^ static_cast<std::uint64_t>(iterations + warmup_iterations);

  std::cout << std::left << std::setw(44) << label << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << ", pixels: " << output.size() << '\n';
  doNotOptimize(checksum);
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);

  std::cout << "PointCloudImageExtractors diagnostic bench\n";
  std::cout << "Dataset: synthetic_organized_clouds_640x480\n";
  std::cout << "Iterations: " << iterations << ", Warmup: " << warmup_iterations << '\n';
#if defined(__RVV10__)
  std::cout << "Build path: RVV candidate (__RVV10__ enabled)\n";
#else
  std::cout << "Build path: scalar reference (__RVV10__ disabled)\n";
#endif

  const auto rgb_cloud = pcie::makeRgbCloud<pcl::PointXYZRGB>(640, 480);
  const auto rgba_cloud = pcie::makeRgbCloud<pcl::PointXYZRGBA>(640, 480);
  const auto scaling_cloud = pcie::makeScalingCloud(640, 480);
  const auto normal_cloud = pcie::makeNormalCloud(640, 480);
  const auto label_cloud = pcie::makeLabelCloud(640, 480);

  runByteCase(argc,
              argv,
              "rgb_unpack_pointxyzrgb_640x480",
              iterations,
              warmup_iterations,
              [&](std::vector<std::uint8_t>& output) {
                pcie::extractRgbCandidate(rgb_cloud, output);
              });
  runByteCase(argc,
              argv,
              "rgb_unpack_pointxyzrgba_640x480",
              iterations,
              warmup_iterations,
              [&](std::vector<std::uint8_t>& output) {
                pcie::extractRgbCandidate(rgba_cloud, output);
              });
  runByteCase(argc,
              argv,
              "rgb_segment_store_pointxyzrgb_640x480",
              iterations,
              warmup_iterations,
              [&](std::vector<std::uint8_t>& output) {
                pcie::extractRgbSegmentStoreCandidate(rgb_cloud, output);
              });
  runByteCase(argc,
              argv,
              "rgb_segment_store_pointxyzrgba_640x480",
              iterations,
              warmup_iterations,
              [&](std::vector<std::uint8_t>& output) {
                pcie::extractRgbSegmentStoreCandidate(rgba_cloud, output);
              });
  runU16Case(argc,
             argv,
             "scaling_full_range_intensity_640x480",
             iterations,
             warmup_iterations,
             [&](std::vector<std::uint16_t>& output) {
               pcie::extractScalingCandidate(scaling_cloud, pcie::ScalingMode::FullRange, 1.0f, output);
             });
  runU16Case(argc,
             argv,
             "scaling_full_range_reduction_intensity_640x480",
             iterations,
             warmup_iterations,
             [&](std::vector<std::uint16_t>& output) {
               pcie::extractScalingFullRangeReductionCandidate(scaling_cloud, output);
             });
  runU16Case(argc,
             argv,
             "scaling_fixed_factor_intensity_640x480",
             iterations,
             warmup_iterations,
             [&](std::vector<std::uint16_t>& output) {
               pcie::extractScalingCandidate(
                   scaling_cloud, pcie::ScalingMode::FixedFactor, 1234.0f, output);
             });
  runByteCase(argc,
              argv,
              "normal_field_pointnormal_640x480",
              iterations,
              warmup_iterations,
              [&](std::vector<std::uint8_t>& output) {
                pcie::extractNormalCandidate(normal_cloud, output);
              });
  runU16Case(argc,
             argv,
             "label_mono16_pointxyzl_640x480",
             iterations,
             warmup_iterations,
             [&](std::vector<std::uint16_t>& output) {
               pcie::extractLabelMono16Candidate(label_cloud, output);
             });
  runByteCase(argc,
              argv,
              "production_rgb_pointxyzrgb_640x480",
              iterations,
              warmup_iterations,
              [&](std::vector<std::uint8_t>& output) {
                pcl::PCLImage image;
                pcl::io::PointCloudImageExtractorFromRGBField<pcl::PointXYZRGB> extractor;
                extractor.extract(rgb_cloud, image);
                output = image.data;
              });
  runByteCase(argc,
              argv,
              "production_rgb_pointxyzrgba_640x480",
              iterations,
              warmup_iterations,
              [&](std::vector<std::uint8_t>& output) {
                pcl::PCLImage image;
                pcl::io::PointCloudImageExtractorFromRGBField<pcl::PointXYZRGBA> extractor;
                extractor.extract(rgba_cloud, image);
                output = image.data;
              });
  runU16Case(argc,
             argv,
             "production_scaling_full_range_intensity_640x480",
             iterations,
             warmup_iterations,
             [&](std::vector<std::uint16_t>& output) {
               pcl::PCLImage image;
               pcl::io::PointCloudImageExtractorFromIntensityField<pcl::PointXYZI> extractor(
                   pcl::io::PointCloudImageExtractorWithScaling<pcl::PointXYZI>::SCALING_FULL_RANGE);
               extractor.extract(scaling_cloud, image);
               output.resize(image.data.size() / sizeof(std::uint16_t));
               std::memcpy(output.data(), image.data.data(), image.data.size());
             });
  runU16Case(argc,
             argv,
             "production_label_mono16_pointxyzl_640x480",
             iterations,
             warmup_iterations,
             [&](std::vector<std::uint16_t>& output) {
               pcl::PCLImage image;
               pcl::io::PointCloudImageExtractorFromLabelField<pcl::PointXYZL> extractor;
               extractor.setColorMode(extractor.COLORS_MONO);
               extractor.extract(label_cloud, image);
               output.resize(image.data.size() / sizeof(std::uint16_t));
               std::memcpy(output.data(), image.data.data(), image.data.size());
             });

  return 0;
}
