/*
 * 本文件做什么：
 * 这个 bench（性能测试）只通过真实 `OrganizedEdgeBase::compute()` 入口运行
 * organized edge detection（有组织点云边缘检测）的 depth label path（深度标签路径）。
 * 它不调用 test-only helper（测试专用 helper），因此可以用来验证 production
 * dispatch（生产分流）是否真的命中 RVV。
 *
 * 证据边界：
 * QEMU（仿真器）侧只用于编译、正确性和反汇编归属；性能结论必须来自板卡或目标硬件。
 */

#include <pcl/features/organized_edge_detection.h>
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

namespace
{
using Clock = std::chrono::steady_clock;

struct Options
{
  int iterations = 20;
  int warmup = 3;
  int width = 320;
  int height = 240;
  std::string case_filter = "all";
};

struct BenchResult
{
  double ms_per_iter = 0.0;
  double checksum = 0.0;
  std::size_t edge_count = 0;
};

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
  readStringArg(argc, argv, "--case-filter", options.case_filter);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  options.width = std::max(options.width, 16);
  options.height = std::max(options.height, 16);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  if (options.case_filter == "all" || options.case_filter == name)
    return true;

  std::size_t start = 0;
  while (start < options.case_filter.size())
  {
    const std::size_t end = options.case_filter.find(',', start);
    const std::string item = options.case_filter.substr(start, end - start);
    if (item == name)
      return true;
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return false;
}

template <typename PointT>
void
fillPointTypeExtras(PointT&, const std::size_t, const std::size_t)
{
}

void
fillPointTypeExtras(pcl::PointXYZI& point, const std::size_t row, const std::size_t col)
{
  point.intensity = static_cast<float>((row * 13 + col * 17) % 255) / 255.0f;
}

void
fillPointTypeExtras(pcl::PointXYZRGB& point, const std::size_t row, const std::size_t col)
{
  point.r = static_cast<std::uint8_t>((row * 3 + col * 5) % 255);
  point.g = static_cast<std::uint8_t>((row * 7 + col * 11) % 255);
  point.b = static_cast<std::uint8_t>((row * 13 + col * 17) % 255);
}

void
fillPointTypeExtras(pcl::PointXYZRGBNormal& point, const std::size_t row, const std::size_t col)
{
  point.r = static_cast<std::uint8_t>((row * 3 + col * 5) % 255);
  point.g = static_cast<std::uint8_t>((row * 7 + col * 11) % 255);
  point.b = static_cast<std::uint8_t>((row * 13 + col * 17) % 255);
  point.normal_x = 0.0f;
  point.normal_y = 0.0f;
  point.normal_z = 1.0f;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeDepthGrid(const std::size_t width, const std::size_t height, const bool with_invalids)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = false;
  cloud->points.resize(width * height);

  const std::size_t left = width / 4;
  const std::size_t right = width - left;
  const std::size_t top = height / 4;
  const std::size_t bottom = height - top;
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      PointT& point = (*cloud)[row * width + col];
      point.x = static_cast<float>(col);
      point.y = static_cast<float>(row);
      float depth = 2.0f + 0.00001f * static_cast<float>((row * 7 + col * 11) % 17);
      if (row >= top && row < bottom && col >= left && col < right)
        depth = 1.70f + 0.00002f * static_cast<float>((row * 5 + col * 3) % 13);
      point.z = depth;
      fillPointTypeExtras(point, row, col);
    }
  }

  if (with_invalids)
  {
    const std::size_t stride = std::max<std::size_t>(width / 17, 5);
    for (std::size_t row = 3; row + 3 < height; row += stride)
    {
      for (std::size_t col = 3; col + 3 < width; col += stride + 1)
      {
        (*cloud)[row * width + col].z = std::numeric_limits<float>::quiet_NaN();
        (*cloud)[row * width + col + 1].z = std::numeric_limits<float>::quiet_NaN();
      }
    }
  }
  return cloud;
}

double
checksumLabels(const pcl::PointCloud<pcl::Label>& labels)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < labels.size(); ++i)
    checksum += static_cast<double>((i % 104729U) + 1U) *
                static_cast<double>(labels[i].label + 1U);
  return checksum;
}

std::size_t
countEdges(const std::vector<pcl::PointIndices>& label_indices)
{
  std::size_t total = 0;
  for (const pcl::PointIndices& indices : label_indices)
    total += indices.indices.size();
  return total;
}

template <typename PointT>
BenchResult
runProductionCase(const Options& options,
                  const std::size_t width,
                  const std::size_t height,
                  const bool with_invalids)
{
  const auto cloud = makeDepthGrid<PointT>(width, height, with_invalids);
  pcl::OrganizedEdgeBase<PointT, pcl::Label> detector;
  detector.setInputCloud(cloud);
  detector.setDepthDisconThreshold(0.02f);
  detector.setMaxSearchNeighbors(12);
  detector.setEdgeType(pcl::OrganizedEdgeBase<PointT, pcl::Label>::EDGELABEL_NAN_BOUNDARY |
                       pcl::OrganizedEdgeBase<PointT, pcl::Label>::EDGELABEL_OCCLUDING |
                       pcl::OrganizedEdgeBase<PointT, pcl::Label>::EDGELABEL_OCCLUDED);

  pcl::PointCloud<pcl::Label> labels;
  std::vector<pcl::PointIndices> label_indices;
  double checksum = 0.0;
  std::size_t edges = 0;

  auto invoke = [&]() {
    detector.compute(labels, label_indices);
    const double local_checksum = checksumLabels(labels);
    const std::size_t local_edges = countEdges(label_indices);
    doNotOptimize(local_checksum);
    doNotOptimize(local_edges);
    checksum += local_checksum;
    edges += local_edges;
  };

  for (int i = 0; i < options.warmup; ++i)
    invoke();

  const auto t0 = Clock::now();
  for (int i = 0; i < options.iterations; ++i)
    invoke();
  const auto t1 = Clock::now();

  return {std::chrono::duration<double, std::milli>(t1 - t0).count() /
              static_cast<double>(options.iterations),
          checksum,
          edges};
}

void
printCase(const std::string& name, const BenchResult& result)
{
  std::cout << std::left << std::setw(40) << name << ": " << std::fixed
            << std::setprecision(4) << result.ms_per_iter << " ms / iter\n";
  std::cout << name << " checksum: " << result.checksum << "\n";
  std::cout << name << " edge-count: " << result.edge_count << "\n";
}

template <typename PointT>
void
runCase(const Options& options,
        const std::string& name,
        const std::size_t width,
        const std::size_t height,
        const bool with_invalids)
{
  if (!caseEnabled(options, name))
    return;
  printCase(name, runProductionCase<PointT>(options, width, height, with_invalids));
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  std::cout << "Dataset: production OrganizedEdgeBase<PointT, Label> width=" << options.width
            << " height=" << options.height << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif

  runCase<pcl::PointXYZ>(options,
                         "prod_depth_finite_320x240",
                         static_cast<std::size_t>(options.width),
                         static_cast<std::size_t>(options.height),
                         false);
  runCase<pcl::PointXYZ>(options, "prod_depth_finite_641x481_tail", 641, 481, false);
  runCase<pcl::PointXYZ>(options,
                         "prod_depth_nan_boundary_320x240",
                         static_cast<std::size_t>(options.width),
                         static_cast<std::size_t>(options.height),
                         true);
  runCase<pcl::PointXYZI>(options,
                          "prod_depth_pointxyzi_finite_320x240",
                          static_cast<std::size_t>(options.width),
                          static_cast<std::size_t>(options.height),
                          false);
  runCase<pcl::PointXYZRGB>(options,
                            "prod_depth_pointxyzrgb_finite_320x240",
                            static_cast<std::size_t>(options.width),
                            static_cast<std::size_t>(options.height),
                            false);
  runCase<pcl::PointXYZRGBNormal>(options,
                                  "prod_depth_pointxyzrgbnormal_finite_320x240",
                                  static_cast<std::size_t>(options.width),
                                  static_cast<std::size_t>(options.height),
                                  false);
  runCase<pcl::PointXYZRGB>(options,
                            "prod_depth_pointxyzrgb_nan_boundary_320x240",
                            static_cast<std::size_t>(options.width),
                            static_cast<std::size_t>(options.height),
                            true);
  return 0;
}
