#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/segmentation/approximate_progressive_morphological_filter.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

struct BenchConfig {
  std::size_t cloud_size = 262144;
  int max_window_size = 9;
  int half_size = 4;
  int iterations = 8;
  int warmup_iterations = 2;
};

BenchConfig
parseArgs(int argc, char** argv)
{
  BenchConfig config;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string key = argv[i];
    const int value = std::atoi(argv[i + 1]);
    if (key == "--size")
      config.cloud_size = static_cast<std::size_t>(value);
    else if (key == "--max-window")
      config.max_window_size = value;
    else if (key == "--half")
      config.half_size = value;
    else if (key == "--iterations")
      config.iterations = value;
    else if (key == "--warmup")
      config.warmup_iterations = value;
  }
  config.max_window_size = std::max(config.max_window_size, 2 * config.half_size + 1);
  return config;
}

template <typename PointT>
void
fillExtraFields(PointT&, const std::size_t)
{
}

void
fillExtraFields(pcl::PointXYZI& point, const std::size_t i)
{
  point.intensity = static_cast<float>(i % 31);
}

void
fillExtraFields(pcl::PointXYZRGB& point, const std::size_t i)
{
  point.r = static_cast<std::uint8_t>((i * 3) % 251);
  point.g = static_cast<std::uint8_t>((i * 5) % 251);
  point.b = static_cast<std::uint8_t>((i * 7) % 251);
}

void
fillExtraFields(pcl::PointXYZRGBA& point, const std::size_t i)
{
  point.r = static_cast<std::uint8_t>((i * 3) % 251);
  point.g = static_cast<std::uint8_t>((i * 5) % 251);
  point.b = static_cast<std::uint8_t>((i * 7) % 251);
  point.a = static_cast<std::uint8_t>(255 - (i % 17));
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeCloud(const std::size_t n, const bool with_invalid)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = !with_invalid;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 1021) - 510) * 0.125f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 37) % 1019) - 509) * 0.125f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 53) % 4093) - 2046) * 0.004f;
    fillExtraFields((*cloud)[i], i);
  }
  if (with_invalid) {
    for (std::size_t i = 31; i < n; i += 4096) {
      (*cloud)[i].x = std::numeric_limits<float>::quiet_NaN();
      if (i + 17 < n)
        (*cloud)[i + 17].z = std::numeric_limits<float>::infinity();
    }
  }
  return cloud;
}

template <typename PointT>
std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t sum = 1469598103934665603ull;
  for (const int index : indices) {
    sum ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(index));
    sum *= 1099511628211ull;
  }
  return sum;
}

template <typename PointT>
double
runFilter(const typename pcl::PointCloud<PointT>::ConstPtr& cloud, const BenchConfig& config)
{
  pcl::ApproximateProgressiveMorphologicalFilter<PointT> filter;
  filter.setInputCloud(cloud);
  filter.setMaxWindowSize(config.max_window_size);
  filter.setSlope(0.35f);
  filter.setMaxDistance(0.8f);
  filter.setInitialDistance(0.18f);
  filter.setCellSize(1.0f);
  filter.setBase(2.0f);
  filter.setExponential(true);
  filter.setNumberOfThreads(1);

  pcl::Indices ground;
  filter.extract(ground);
  return static_cast<double>(checksumIndices<PointT>(ground));
}

template <typename Fn>
double
timeCase(const BenchConfig& config, const std::string& label, Fn&& fn)
{
  double last_checksum = 0.0;
  for (int i = 0; i < config.warmup_iterations; ++i)
    last_checksum += fn();

  const auto begin = std::chrono::steady_clock::now();
  for (int i = 0; i < config.iterations; ++i)
    last_checksum += fn();
  const auto end = std::chrono::steady_clock::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - begin).count();
  std::cout << label << " : " << (elapsed / static_cast<double>(config.iterations)) << " ms / iter\n";
  std::cout << "Checksum " << label << " : " << last_checksum << "\n";
  return last_checksum;
}

template <typename PointT>
void
runPointTypeCases(const BenchConfig& config,
                  const std::string& dense_label,
                  const std::string& non_dense_label,
                  volatile double& sink)
{
  const auto dense_cloud = makeCloud<PointT>(config.cloud_size, false);
  const auto sparse_cloud = makeCloud<PointT>(config.cloud_size, true);

  sink += timeCase(config, dense_label, [&]() {
    return runFilter<PointT>(dense_cloud, config);
  });
  sink += timeCase(config, non_dense_label, [&]() {
    return runFilter<PointT>(sparse_cloud, config);
  });
}

} // namespace

int
main(int argc, char** argv)
{
  const BenchConfig config = parseArgs(argc, argv);
  std::cout << "Dataset: synthetic APMF production public, cloud=" << config.cloud_size
            << ", max_window=" << config.max_window_size << "\n";
  std::cout << "Iterations: " << config.iterations << "\n";
  std::cout << "Warmup Iterations: " << config.warmup_iterations << "\n";

  volatile double sink = 0.0;

  runPointTypeCases<pcl::PointXYZ>(
      config, "apmf production public dense", "apmf production public non-dense", sink);
  runPointTypeCases<pcl::PointXYZI>(
      config,
      "apmf production public PointXYZI dense",
      "apmf production public PointXYZI non-dense",
      sink);
  runPointTypeCases<pcl::PointXYZRGB>(
      config,
      "apmf production public PointXYZRGB dense",
      "apmf production public PointXYZRGB non-dense",
      sink);
  runPointTypeCases<pcl::PointXYZRGBA>(
      config,
      "apmf production public PointXYZRGBA dense",
      "apmf production public PointXYZRGBA non-dense",
      sink);

  return sink == 0.123 ? 1 : 0;
}
