/*
 * 本文件做什么：
 * 这是 TrajkovicKeypoint3D FOUR_CORNERS response map（四邻域响应图）的
 * benchmark（性能测试）入口。默认 diagnostic mode（诊断模式）只度量测试专用
 * response helper；public mode（公开入口模式）调用真实 `compute()`，用于验证
 * production dispatch（生产分流逻辑）接入后是否仍有板卡收益。
 */

#include "trajkovic_3d.h"

#include <pcl/keypoints/trajkovic_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace t3d = pcl::keypoints::rvv_test::trajkovic_3d;

namespace
{
using Clock = std::chrono::steady_clock;
using Detector = pcl::TrajkovicKeypoint3D<pcl::PointXYZ, pcl::PointXYZI, pcl::Normal>;

struct Options
{
  int iterations = 20;
  int warmup = 3;
  int width = 320;
  int height = 240;
  int half_window = 1;
  float first_threshold = 0.00046f;
  float second_threshold = 0.0005f;
  std::string mode = "diagnostic";
  std::string case_filter = "all";
};

struct Dataset
{
  std::size_t width = 0;
  std::size_t height = 0;
  std::vector<pcl::PointXYZ> points;
  std::vector<pcl::Normal> normals;
};

bool
readIntArg(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
readFloatArg(const int argc, char** argv, const char* key, float& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = std::strtof(argv[i + 1], nullptr);
      return true;
    }
  }
  return false;
}

bool
readStringArg(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = argv[i + 1];
      return true;
    }
  }
  return false;
}

Options
parseOptions(const int argc, char** argv)
{
  Options options;
  readIntArg(argc, argv, "--iterations", options.iterations);
  readIntArg(argc, argv, "--warmup", options.warmup);
  readIntArg(argc, argv, "--width", options.width);
  readIntArg(argc, argv, "--height", options.height);
  readIntArg(argc, argv, "--half-window", options.half_window);
  readFloatArg(argc, argv, "--first-threshold", options.first_threshold);
  readFloatArg(argc, argv, "--second-threshold", options.second_threshold);
  readStringArg(argc, argv, "--mode", options.mode);
  readStringArg(argc, argv, "--case-filter", options.case_filter);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  options.width = std::max(options.width, 16);
  options.height = std::max(options.height, 16);
  options.half_window = std::max(options.half_window, 1);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  if (options.case_filter == "all")
    return true;
  std::size_t start = 0;
  while (start <= options.case_filter.size())
  {
    const std::size_t comma = options.case_filter.find(',', start);
    const std::string token = options.case_filter.substr(start, comma - start);
    if (token == name)
      return true;
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return false;
}

Dataset
makeDataset(const std::size_t width, const std::size_t height, const bool include_invalid)
{
  Dataset data;
  data.width = width;
  data.height = height;
  data.points.resize(width * height);
  data.normals.resize(width * height);

  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      const std::size_t index = row * width + col;
      pcl::PointXYZ& point = data.points[index];
      point.x = static_cast<float>(col) * 0.01f;
      point.y = static_cast<float>(row) * 0.02f;
      point.z = 1.0f + static_cast<float>((col * 7 + row * 13) % 31) * 0.001f;

      const float nx = 0.07f * static_cast<float>(static_cast<int>(col % 9) - 4);
      const float ny = 0.05f * static_cast<float>(static_cast<int>(row % 7) - 3);
      const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
      pcl::Normal& normal = data.normals[index];
      normal.normal_x = nx;
      normal.normal_y = ny;
      normal.normal_z = nz;
      normal.curvature = 0.0f;
    }
  }

  if (include_invalid && width > 80 && height > 80)
  {
    for (std::size_t row = 17; row + 17 < height; row += 37)
    {
      for (std::size_t col = 19; col + 19 < width; col += 43)
      {
        const std::size_t index = row * width + col;
        if (((row + col) % 3) == 0)
          data.points[index].z = std::numeric_limits<float>::quiet_NaN();
        else if (((row + col) % 3) == 1)
          data.normals[index].normal_x = std::numeric_limits<float>::quiet_NaN();
        else
          data.normals[index].normal_z = std::numeric_limits<float>::infinity();
      }
    }
  }
  return data;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeCloud(const std::vector<PointT>& points, const std::size_t width, const std::size_t height, const bool is_dense)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = is_dense;
  cloud->points.assign(points.begin(), points.end());
  return cloud;
}

std::uint64_t
checksumResponses(const std::vector<float>& responses)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const float value : responses)
  {
    const auto quantized = static_cast<std::int64_t>(std::llround(static_cast<double>(value) * 100000.0));
    std::uint64_t bits = 0;
    std::memcpy(&bits, &quantized, sizeof(bits));
    hash ^= bits;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::uint64_t
checksumOutput(const pcl::PointCloud<pcl::PointXYZI>& output, const pcl::PointIndices& indices)
{
  std::uint64_t hash = 1469598103934665603ull;
  hash ^= static_cast<std::uint64_t>(output.size());
  hash *= 1099511628211ull;
  for (std::size_t i = 0; i < output.size(); ++i)
  {
    const auto intensity = static_cast<std::int64_t>(std::llround(static_cast<double>(output[i].intensity) * 100000.0));
    std::uint64_t bits = 0;
    std::memcpy(&bits, &intensity, sizeof(bits));
    hash ^= bits;
    hash *= 1099511628211ull;
    const auto index = static_cast<std::uint64_t>(indices.indices[i]);
    hash ^= index + 0x9e3779b97f4a7c15ull + (hash << 6U) + (hash >> 2U);
    hash *= 1099511628211ull;
  }
  return hash;
}

template <typename Fn>
double
timeCase(const Options& options, Fn&& fn, std::uint64_t& checksum)
{
  for (int i = 0; i < options.warmup; ++i)
    checksum ^= fn();
  const auto begin = Clock::now();
  for (int i = 0; i < options.iterations; ++i)
    checksum ^= fn();
  const auto end = Clock::now();
  return std::chrono::duration<double, std::milli>(end - begin).count() /
         static_cast<double>(options.iterations);
}

void
runDiagnosticCase(const Options& options,
                  const std::string& label,
                  const Detector::ComputationMethod method,
                  const std::size_t width,
                  const std::size_t height,
                  const int half_window,
                  const bool include_invalid)
{
  if (!caseEnabled(options, label))
    return;

  const Dataset data = makeDataset(width, height, include_invalid);
  const t3d::ResponseConfig config{data.width, data.height, half_window, options.first_threshold};
  std::vector<float> response(data.width * data.height, 0.0f);
  std::uint64_t checksum = 0;

  const double ms = timeCase(options, [&] {
#if defined(__RVV10__)
    if (method == Detector::FOUR_CORNERS)
      t3d::computeFourCornersResponseRVV(data.points.data(), data.normals.data(), config, response.data());
    else
      t3d::computeEightCornersResponseRVV(data.points.data(), data.normals.data(), config, response.data());
#else
    if (method == Detector::FOUR_CORNERS)
      t3d::computeFourCornersResponseStd(data.points.data(), data.normals.data(), config, response.data());
    else
      t3d::computeEightCornersResponseStd(data.points.data(), data.normals.data(), config, response.data());
#endif
    return checksumResponses(response);
  }, checksum);

  std::cout << "Dataset: synthetic Trajkovic 3D "
            << (method == Detector::FOUR_CORNERS ? "FOUR_CORNERS" : "EIGHT_CORNERS")
            << " response diagnostic width=" << width << " height=" << height
            << " half_window=" << half_window
            << " include_invalid=" << (include_invalid ? "true" : "false") << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";
  std::cout << label << ": " << std::fixed << std::setprecision(6) << ms << " ms / iter\n";
  std::cout << label << " checksum: " << checksum << "\n";
}

void
runPublicCase(const Options& options,
              const std::string& label,
              const Detector::ComputationMethod method,
              const std::size_t width,
              const std::size_t height,
              const bool include_invalid)
{
  if (!caseEnabled(options, label))
    return;

  const Dataset data = makeDataset(width, height, include_invalid);
  auto cloud = makeCloud(data.points, data.width, data.height, !include_invalid);
  auto normals = makeCloud(data.normals, data.width, data.height, !include_invalid);
  std::uint64_t checksum = 0;

  const double ms = timeCase(options, [&] {
    Detector detector(Detector::FOUR_CORNERS,
                      static_cast<int>(options.half_window * 2 + 1),
                      options.first_threshold,
                      options.second_threshold);
    detector.setNumberOfThreads(1);
    detector.setInputCloud(cloud);
    detector.setNormals(normals);
    pcl::PointCloud<pcl::PointXYZI> output;
    detector.compute(output);
    return checksumOutput(output, *detector.getKeypointsIndices());
  }, checksum);

  std::cout << "Dataset: synthetic Trajkovic 3D "
            << (method == Detector::FOUR_CORNERS ? "FOUR_CORNERS" : "EIGHT_CORNERS")
            << " public compute width=" << width << " height=" << height
            << " half_window=" << options.half_window
            << " include_invalid=" << (include_invalid ? "true" : "false") << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";
  std::cout << label << ": " << std::fixed << std::setprecision(6) << ms << " ms / iter\n";
  std::cout << label << " checksum: " << checksum << "\n";
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  if (options.mode == "public")
  {
    runPublicCase(options,
                  "public_four_corners_320x240",
                  Detector::FOUR_CORNERS,
                  static_cast<std::size_t>(options.width),
                  static_cast<std::size_t>(options.height),
                  false);
    runPublicCase(options,
                  "public_four_corners_invalid_320x240",
                  Detector::FOUR_CORNERS,
                  static_cast<std::size_t>(options.width),
                  static_cast<std::size_t>(options.height),
                  true);
    runPublicCase(options, "public_four_corners_641x481_tail", Detector::FOUR_CORNERS, 641, 481, true);
    runPublicCase(options,
                  "public_eight_corners_320x240",
                  Detector::EIGHT_CORNERS,
                  static_cast<std::size_t>(options.width),
                  static_cast<std::size_t>(options.height),
                  false);
    runPublicCase(options,
                  "public_eight_corners_invalid_320x240",
                  Detector::EIGHT_CORNERS,
                  static_cast<std::size_t>(options.width),
                  static_cast<std::size_t>(options.height),
                  true);
    runPublicCase(options, "public_eight_corners_641x481_tail", Detector::EIGHT_CORNERS, 641, 481, true);
    return 0;
  }

  runDiagnosticCase(options,
                    "four_corners_320x240",
                    Detector::FOUR_CORNERS,
                    static_cast<std::size_t>(options.width),
                    static_cast<std::size_t>(options.height),
                    options.half_window,
                    false);
  runDiagnosticCase(options,
                    "four_corners_invalid_320x240",
                    Detector::FOUR_CORNERS,
                    static_cast<std::size_t>(options.width),
                    static_cast<std::size_t>(options.height),
                    options.half_window,
                    true);
  runDiagnosticCase(options, "four_corners_641x481_tail", Detector::FOUR_CORNERS, 641, 481, options.half_window, true);
  runDiagnosticCase(options,
                    "eight_corners_320x240",
                    Detector::EIGHT_CORNERS,
                    static_cast<std::size_t>(options.width),
                    static_cast<std::size_t>(options.height),
                    options.half_window,
                    false);
  runDiagnosticCase(options,
                    "eight_corners_invalid_320x240",
                    Detector::EIGHT_CORNERS,
                    static_cast<std::size_t>(options.width),
                    static_cast<std::size_t>(options.height),
                    options.half_window,
                    true);
  runDiagnosticCase(options, "eight_corners_641x481_tail", Detector::EIGHT_CORNERS, 641, 481, options.half_window, true);
  return 0;
}
