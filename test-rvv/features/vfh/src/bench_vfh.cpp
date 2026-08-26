#include "vfh.h"

#include <pcl/features/vfh.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace vfh_test = pcl::features::rvv_test::vfh;

namespace
{
struct BenchConfig
{
  int side = 80;
  int iterations = 8;
  int warmup_iterations = 2;
};

BenchConfig
parseArgs(int argc, char** argv)
{
  BenchConfig config;
  for (int i = 1; i + 1 < argc; i += 2)
  {
    const std::string key = argv[i];
    const int value = std::atoi(argv[i + 1]);
    if (key == "--side")
      config.side = value;
    else if (key == "--iterations")
      config.iterations = value;
    else if (key == "--warmup")
      config.warmup_iterations = value;
  }
  return config;
}

double
checksumSignature(const vfh_test::Signature& signature)
{
  double sum = 0.0;
  for (std::size_t i = 0; i < signature.size(); ++i)
    sum += static_cast<double>(signature[i]) * static_cast<double>(i + 1);
  return sum;
}

double
computePublicChecksum(const vfh_test::CloudT::Ptr& cloud, const vfh_test::VFHOptions& options)
{
  pcl::VFHEstimation<vfh_test::PointT, vfh_test::PointT, pcl::VFHSignature308> estimator;
  estimator.setInputCloud(cloud);
  estimator.setInputNormals(cloud);
  estimator.setSearchSurface(cloud);
  estimator.setViewPoint(options.viewpoint.x(), options.viewpoint.y(), options.viewpoint.z());
  estimator.setNormalizeBins(options.normalize_bins);
  estimator.setNormalizeDistance(options.normalize_distances);
  estimator.setFillSizeComponent(options.size_component);

  pcl::PointCloud<pcl::VFHSignature308> output;
  estimator.compute(output);
  vfh_test::Signature signature{};
  if (!output.empty())
    std::copy(std::begin(output[0].histogram), std::end(output[0].histogram), signature.begin());
  return checksumSignature(signature);
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
  std::cout << label << ": " << (elapsed / static_cast<double>(config.iterations)) << " ms / iter\n";
  std::cout << label << " checksum: " << last_checksum << "\n";
  return last_checksum;
}
} // namespace

int
main(int argc, char** argv)
{
  const BenchConfig config = parseArgs(argc, argv);
  const auto cloud = vfh_test::makeFeatureCloud(config.side);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  const vfh_test::VFHOptions options;

  std::cout << "Dataset: synthetic VFH dense PointNormal cloud, side=" << config.side
            << ", points=" << cloud->size() << "\n";
  std::cout << "Iterations: " << config.iterations << "\n";
  std::cout << "Warmup Iterations: " << config.warmup_iterations << "\n";

  volatile double sink = 0.0;
  sink += timeCase(config, "component_vfh_reference", [&]() {
    return checksumSignature(vfh_test::computeVFHSignatureReference(*cloud, indices, options));
  });
  sink += timeCase(config, "candidate_vfh_centroid_spfh_rvv", [&]() {
    vfh_test::Signature signature{};
    if (vfh_test::computeVFHSignatureCentroidSPFHRVV(*cloud, indices, signature, options))
      return checksumSignature(signature);
    return checksumSignature(vfh_test::computeVFHSignatureReference(*cloud, indices, options));
  });
  sink += timeCase(config, "candidate_vfh_spfh_viewpoint_rvv", [&]() {
    vfh_test::Signature signature{};
    if (vfh_test::computeVFHSignatureSPFHAndViewpointRVV(*cloud, indices, signature, options))
      return checksumSignature(signature);
    return checksumSignature(vfh_test::computeVFHSignatureReference(*cloud, indices, options));
  });
  sink += timeCase(config, "candidate_vfh_centroids_spfh_viewpoint_rvv", [&]() {
    vfh_test::Signature signature{};
    if (vfh_test::computeVFHSignatureCentroidsSPFHAndViewpointRVV(*cloud, indices, signature, options))
      return checksumSignature(signature);
    return checksumSignature(vfh_test::computeVFHSignatureReference(*cloud, indices, options));
  });
  sink += timeCase(config, "production_vfh_compute_default", [&]() {
    return computePublicChecksum(cloud, options);
  });

  return sink == 0.123 ? 1 : 0;
}
