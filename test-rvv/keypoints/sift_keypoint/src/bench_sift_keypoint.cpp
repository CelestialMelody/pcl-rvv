/*
 * 本文件做什么：
 * 这是 SIFT scale-space topic 的 benchmark 入口。diagnostic mode 跑
 * synthetic neighborhood batch，对拍 `computeScaleSpace()` 里的 Gaussian
 * 权重与局部规约；public mode 跑真实 `SIFTKeypoint::compute()`，用于看
 * production direct（真实生产入口）是否仍有板卡收益。
 *
 * 证据边界：
 * diagnostic mode 不测 `radiusSearch`、`nearestKSearch` 或完整 public SIFT 入口。
 * public mode 会覆盖完整 public compute()，但仍只是 benchmark，不替代 correctness
 * gtest 或正式 doc-rvv 收口。
 */

#include "sift_keypoint.h"

#include <pcl/keypoints/sift_keypoint.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace sift = pcl::keypoints::rvv_test::sift_keypoint;

namespace
{
using PublicDetector = pcl::SIFTKeypoint<pcl::PointXYZI, pcl::PointWithScale>;

struct Options
{
  int iterations = 50;
  int warmup_iterations = 5;
  std::string case_filter = "all";
  std::string mode = "diagnostic";
  bool emit_public_trace = false;
  std::size_t public_trace_limit = std::numeric_limits<std::size_t>::max();
};

template <typename Fn>
double
timeKernel(Fn&& fn, const int iterations, const int warmup_iterations)
{
  for (int i = 0; i < warmup_iterations; ++i)
    fn();
  const auto begin = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    fn();
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(end - begin).count() /
         static_cast<double>(iterations);
}

bool
caseEnabled(const std::string& filter, const std::string& label)
{
  return filter == "all" || filter == label || filter.find(label) != std::string::npos;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr
makeSmokeCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->points.resize(width * height);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      const std::size_t index = row * width + col;
      cloud->points[index].x = static_cast<float>(col) * 0.01f;
      cloud->points[index].y = static_cast<float>(row) * 0.01f;
      cloud->points[index].z = 1.0f + 0.02f * static_cast<float>((row + col) % 5);
      cloud->points[index].intensity =
          0.7f + 0.2f * std::sin(0.13f * static_cast<float>(row)) +
          0.1f * std::cos(0.17f * static_cast<float>(col));
    }
  }
  return cloud;
}

std::uint64_t
checksumPublicOutput(const pcl::PointCloud<pcl::PointWithScale>& output)
{
  std::uint64_t hash = 1469598103934665603ull;
  hash ^= static_cast<std::uint64_t>(output.size());
  hash *= 1099511628211ull;
  for (const auto& point : output)
  {
    const auto qx = static_cast<std::int64_t>(std::llround(static_cast<double>(point.x) * 100000.0));
    const auto qy = static_cast<std::int64_t>(std::llround(static_cast<double>(point.y) * 100000.0));
    const auto qz = static_cast<std::int64_t>(std::llround(static_cast<double>(point.z) * 100000.0));
    const auto qs = static_cast<std::int64_t>(std::llround(static_cast<double>(point.scale) * 100000.0));
    const std::array<std::int64_t, 4> values{qx, qy, qz, qs};
    for (const auto value : values)
    {
      std::uint64_t bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      hash ^= bits;
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

void
printPublicTrace(const std::string& label,
                 const pcl::PointCloud<pcl::PointWithScale>& output,
                 const std::size_t trace_limit)
{
  const std::size_t limit = std::min<std::size_t>(output.size(), trace_limit);
  std::cout << "public_trace_" << label << "_count=" << output.size()
            << " emitted=" << limit << "\n";
  for (std::size_t i = 0; i < limit; ++i)
  {
    const auto& point = output[i];
    std::cout << "public_trace_" << label
              << " index=" << i
              << " x=" << std::fixed << std::setprecision(9) << point.x
              << " y=" << point.y
              << " z=" << point.z
              << " scale=" << point.scale << "\n";
  }
}

void
runCase(const std::string& label,
        const std::size_t point_count,
        const std::size_t neighbor_count,
        const int nr_scales_per_octave,
        const int iterations,
        const int warmup_iterations)
{
  const auto input = sift::makeSyntheticScaleSpaceCase(point_count, neighbor_count, nr_scales_per_octave);
  const auto expected = sift::computeScaleSpaceScalar(input);
  std::size_t rvv_chunks = 0;
  const double ms = timeKernel(
      [&] {
        std::size_t local_chunks = 0;
        const auto actual = sift::computeScaleSpaceCandidate(input, &local_chunks);
        rvv_chunks = local_chunks;
        (void)actual;
      },
      iterations,
      warmup_iterations);
  const auto actual = sift::computeScaleSpaceCandidate(input, &rvv_chunks);
  const auto extrema = sift::findScaleSpaceExtremaScalar(actual, input.extrema_radius, input.contrast_threshold);
  float max_abs_error = 0.0f;
  for (std::size_t i = 0; i < actual.dog.size(); ++i)
    max_abs_error = std::max(max_abs_error, std::fabs(actual.dog[i] - expected.dog[i]));

  std::cout << label << ": " << std::fixed << std::setprecision(6) << ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << sift::checksumResponses(actual)
            << " iterations=" << iterations
            << " warmup=" << warmup_iterations
            << " rvv_chunks=" << rvv_chunks << "\n";
  std::cout << "extrema_" << label << "=" << sift::checksumIndices(extrema) << "\n";
  std::cout << "  max_abs_error: " << std::setprecision(9) << max_abs_error
            << ", tolerance_pass: " << (max_abs_error <= 1e-5f ? "yes" : "no") << "\n";
}

void
runPublicCase(const std::string& label,
              const std::size_t width,
              const std::size_t height,
              const int iterations,
              const int warmup_iterations,
              const bool emit_public_trace,
              const std::size_t public_trace_limit)
{
  const auto cloud = makeSmokeCloud(width, height);
  PublicDetector detector;
  detector.setSearchMethod(pcl::make_shared<pcl::search::KdTree<pcl::PointXYZI>>());
  detector.setInputCloud(cloud);
  detector.setScales(0.02f, 3, 3);
  detector.setMinimumContrast(0.0f);

  pcl::PointCloud<pcl::PointWithScale> output;
  const double ms = timeKernel(
      [&] {
        detector.compute(output);
      },
      iterations,
      warmup_iterations);
  detector.compute(output);

  std::cout << "Dataset: SIFTKeypoint public compute; width=" << width
            << "; height=" << height << "; mode=public\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  std::cout << label << ": " << std::fixed << std::setprecision(6) << ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << checksumPublicOutput(output)
            << " iterations=" << iterations
            << " warmup=" << warmup_iterations
            << " rvv_chunks=0\n";
  std::cout << "keypoints_" << label << "=" << output.size() << "\n";
  if (emit_public_trace)
    printPublicTrace(label, output, public_trace_limit);
}

} // namespace

int
main(int argc, char** argv)
{
  Options options;

  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--iterations" && i + 1 < argc)
      options.iterations = std::atoi(argv[++i]);
    else if (arg == "--warmup-iterations" && i + 1 < argc)
      options.warmup_iterations = std::atoi(argv[++i]);
    else if (arg == "--case-filter" && i + 1 < argc)
      options.case_filter = argv[++i];
    else if (arg == "--mode" && i + 1 < argc)
      options.mode = argv[++i];
    else if (arg == "--emit-public-trace")
      options.emit_public_trace = true;
    else if (arg == "--public-trace-limit" && i + 1 < argc)
      options.public_trace_limit = static_cast<std::size_t>(std::strtoull(argv[++i], nullptr, 10));
  }

  if (options.mode == "public")
  {
    if (caseEnabled(options.case_filter, "public_sift_keypoint_320x240"))
      runPublicCase("public_sift_keypoint_320x240",
                    320,
                    240,
                    options.iterations,
                    options.warmup_iterations,
                    options.emit_public_trace,
                    options.public_trace_limit);
    if (caseEnabled(options.case_filter, "public_sift_keypoint_641x481"))
      runPublicCase("public_sift_keypoint_641x481",
                    641,
                    481,
                    options.iterations,
                    options.warmup_iterations,
                    options.emit_public_trace,
                    options.public_trace_limit);
  }
  else
  {
    if (caseEnabled(options.case_filter, "sift_scale_space_96_points"))
      runCase("sift_scale_space_96_points", 96, 29, 3, options.iterations, options.warmup_iterations);
    if (caseEnabled(options.case_filter, "sift_scale_space_192_points"))
      runCase("sift_scale_space_192_points", 192, 33, 3, options.iterations, options.warmup_iterations);
    if (caseEnabled(options.case_filter, "sift_scale_space_tail_113_points"))
      runCase("sift_scale_space_tail_113_points", 113, 25, 3, options.iterations, options.warmup_iterations);
  }

  return 0;
}
