/*
 * 本文件做什么：
 * 这是 ISM topic 的 diagnostic bench（诊断性能测试）入口。Std build 运行标量参考链路，
 * RVV build 运行测试专用 RVV candidate。输出采用共享 analyzer 能解析的 Dataset、
 * Iterations、计时行和 checksum（校验和）格式，后续可生成 repeated board summary
 * 和 Evidence Doctor（证据体检）报告。
 *
 * 证据边界：
 * 这个 bench 只证明三个局部公式是否值得继续，不包含 feature estimator、VoxelGrid、
 * KMeans、radiusSearch、`nth_element` 或完整 vote list 状态。正向结果只能进入下一阶段
 * production-shaped diagnostic（生产形态诊断），不能直接写成 production adoption（生产采纳）。
 */

#include "ism.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <Eigen/Core>
#include <pcl/features/feature.h>
#include <pcl/features/impl/feature.hpp>
#include <pcl/point_types.h>
#include <pcl/recognition/impl/implicit_shape_model.hpp>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace ism = pcl::test::implicit_shape_model_rvv;

namespace
{

std::uint64_t
checksumDoubleForPublicFingerprint(const double value)
{
  union
  {
    double d;
    std::uint64_t u;
  } bits{value};
  return bits.u;
}

class SyntheticIsmFeature
: public pcl::FeatureFromNormals<pcl::PointXYZ, pcl::Normal, pcl::Histogram<153>>
{
  using Base = pcl::FeatureFromNormals<pcl::PointXYZ, pcl::Normal, pcl::Histogram<153>>;

  void
  computeFeature(pcl::PointCloud<pcl::Histogram<153>>& output) override
  {
    const std::size_t count = this->input_ ? this->input_->size() : 0u;
    output.clear();
    output.resize(count);
    for (std::size_t point = 0; point < count; ++point)
    {
      const bool zero_descriptor = (point % 29u) == 0u;
      for (std::size_t dim = 0; dim < 153u; ++dim)
      {
        output[point].histogram[dim] =
            zero_descriptor
                ? 0.0f
                : 0.18f +
                      static_cast<float>((point * 43u + dim * 37u + 19u) % 251u) * 0.0027f;
      }
    }
  }

public:
  using Ptr = pcl::shared_ptr<SyntheticIsmFeature>;

  SyntheticIsmFeature()
  {
    // 测试专用 estimator 不做邻域搜索，但 Feature::initCompute() 仍要求 K 或 radius 非零。
    this->setKSearch(1);
  }
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

int
parseIntArg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::string(argv[i]) == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::string(argv[i]) != "--case-filter")
      continue;
    const std::string selected(argv[i + 1]);
    if (selected == "all")
      return true;
    std::size_t begin = 0;
    while (begin <= selected.size())
    {
      const std::size_t end = selected.find(',', begin);
      const auto token = selected.substr(begin, end == std::string::npos ? end : end - begin);
      if (token == requested)
        return true;
      if (end == std::string::npos)
        break;
      begin = end + 1;
    }
    return false;
  }
  return true;
}

void
runDescriptorCase(const int argc,
                  char** argv,
                  const int iterations,
                  const int warmup_iterations)
{
  const std::string label = "descriptor_cluster_distance";
  if (!caseEnabled(argc, argv, label))
    return;

  const std::size_t dimensions = 153;
  const std::size_t clusters = static_cast<std::size_t>(parseIntArg(argc, argv, "--clusters", 184));
  const auto descriptor = ism::makeDescriptor(dimensions);
  const auto centers = ism::makeClusterCenters(clusters, dimensions);
  ism::DistanceResult result{0, 0.0f};

  const double ms = timeKernel(
      [&] {
        result = ism::nearestClusterDistanceCandidate(
            descriptor.data(), centers.data(), clusters, dimensions);
      },
      iterations,
      warmup_iterations);
  const auto checksum = ism::checksumDiagnostic(result, 0.0f, 0.0f);

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(6) << ms << " ms/iter\n";
  std::cout << "  Total Time: " << (ms * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", dimensions: " << dimensions
            << ", clusters: " << clusters << ", best_index: " << result.index << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
}

void
runDescriptorBatchCase(const int argc,
                       char** argv,
                       const int iterations,
                       const int warmup_iterations)
{
  const std::string label = "descriptor_batch_assignment";
  if (!caseEnabled(argc, argv, label))
    return;

  const std::size_t dimensions = 153;
  const std::size_t descriptors =
      static_cast<std::size_t>(parseIntArg(argc, argv, "--descriptors", 512));
  const std::size_t clusters = static_cast<std::size_t>(parseIntArg(argc, argv, "--clusters", 184));
  const auto descriptor_cloud = ism::makeDescriptorBatch(descriptors, dimensions);
  const auto centers = ism::makeClusterCenters(clusters, dimensions);
  std::vector<int> assignments;

  const double ms = timeKernel(
      [&] {
        assignments = ism::assignDescriptorBatchCandidate(
            descriptor_cloud.data(), centers.data(), descriptors, clusters, dimensions);
      },
      iterations,
      warmup_iterations);
  const auto checksum = ism::checksumAssignments(assignments);
  const auto assigned =
      static_cast<std::size_t>(std::count_if(assignments.begin(), assignments.end(), [](const int value) {
        return value >= 0;
      }));

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(6) << ms << " ms/iter\n";
  std::cout << "  Total Time: " << (ms * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", descriptors: " << descriptors
            << ", clusters: " << clusters << ", assigned: " << assigned << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
}

void
runSigmaCase(const int argc, char** argv, const int iterations, const int warmup_iterations)
{
  const std::string label = "sigma_pairwise_max_dot";
  if (!caseEnabled(argc, argv, label))
    return;

  const std::size_t points = static_cast<std::size_t>(parseIntArg(argc, argv, "--points", 768));
  const auto training_points = ism::makeTrainingPoints(points);
  float sigma = 0.0f;

  const double ms = timeKernel(
      [&] { sigma = ism::maxPairwiseDotSigmaCandidate(training_points.data(), training_points.size()); },
      iterations,
      warmup_iterations);
  const auto checksum = ism::checksumDiagnostic({0, 0.0f}, sigma, 0.0f);

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(6) << ms << " ms/iter\n";
  std::cout << "  Total Time: " << (ms * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", points: " << points
            << ", sigma: " << sigma << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
}

void
runDensityCase(const int argc, char** argv, const int iterations, const int warmup_iterations)
{
  const std::string label = "vote_density_gaussian_sum";
  if (!caseEnabled(argc, argv, label))
    return;

  const std::size_t votes = static_cast<std::size_t>(parseIntArg(argc, argv, "--votes", 8192));
  const auto distances = ism::makeSquaredDistances(votes);
  const auto strengths = ism::makeVoteStrengths(votes);
  const float sigma = 0.42f;
  float density = 0.0f;

  const double ms = timeKernel(
      [&] {
        density = ism::densityWeightedSumCandidate(
            distances.data(), strengths.data(), distances.size(), sigma);
      },
      iterations,
      warmup_iterations);
  const auto checksum = ism::checksumDiagnostic({0, 0.0f}, sigma, density);

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(6) << ms << " ms/iter\n";
  std::cout << "  Total Time: " << (ms * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", votes: " << votes
            << ", sigma: " << sigma << ", density: " << density << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
}

pcl::features::ISMModel::Ptr
makeDeterministicModel(const std::size_t clusters)
{
  pcl::features::ISMModel::Ptr model(new pcl::features::ISMModel);
  model->number_of_classes_ = 1;
  model->number_of_visual_words_ = static_cast<unsigned int>(clusters);
  model->number_of_clusters_ = static_cast<unsigned int>(clusters);
  model->descriptors_dimension_ = 153;
  model->clusters_centers_.resize(static_cast<Eigen::Index>(clusters), 153);
  const auto centers = ism::makeClusterCenters(clusters, 153);
  for (Eigen::Index row = 0; row < model->clusters_centers_.rows(); ++row)
    for (Eigen::Index col = 0; col < model->clusters_centers_.cols(); ++col)
      model->clusters_centers_(row, col) =
          centers[static_cast<std::size_t>(row) * 153u + static_cast<std::size_t>(col)];

  model->clusters_.resize(clusters);
  model->classes_.resize(clusters, 0);
  model->learned_weights_.resize(clusters, 1.0f);
  model->statistical_weights_.assign(1, std::vector<float>(clusters, 1.0f));
  model->sigmas_.assign(1, 1.0f);
  model->directions_to_center_.resize(static_cast<Eigen::Index>(clusters), 3);
  model->directions_to_center_.setZero();
  for (std::size_t cluster = 0; cluster < clusters; ++cluster)
    model->clusters_[cluster].push_back(static_cast<unsigned int>(cluster));
  return model;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePublicEntryCloud(const std::size_t count)
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
  cloud->resize(count);
  cloud->width = static_cast<std::uint32_t>(count);
  cloud->height = 1;
  cloud->is_dense = true;
  for (std::size_t i = 0; i < count; ++i)
  {
    (*cloud)[i].x = static_cast<float>(i) * 0.25f;
    (*cloud)[i].y = static_cast<float>((i * 7u) % 37u) * 0.05f;
    (*cloud)[i].z = static_cast<float>((i * 11u) % 43u) * 0.04f;
  }
  return cloud;
}

pcl::PointCloud<pcl::Normal>::Ptr
makePublicEntryNormals(const std::size_t count)
{
  pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
  normals->resize(count);
  normals->width = static_cast<std::uint32_t>(count);
  normals->height = 1;
  normals->is_dense = true;
  for (auto& normal : *normals)
  {
    normal.normal_x = 0.0f;
    normal.normal_y = 1.0f;
    normal.normal_z = 0.0f;
  }
  return normals;
}

ism::PublicFindObjectsResult
summarizePublicFindObjects(const pcl::features::ISMVoteList<pcl::PointXYZ>::Ptr& votes,
                           const std::size_t class_of_interest)
{
  ism::PublicFindObjectsResult result{votes ? votes->getNumberOfVotes() : 0u, class_of_interest, 0.0, 0u};
  if (!votes || result.votes == 0u)
    return result;

  std::vector<pcl::ISMPeak, Eigen::aligned_allocator<pcl::ISMPeak> > peaks;
  votes->findStrongestPeaks(peaks, static_cast<int>(class_of_interest), 0.05, 1.0);
  std::uint64_t fingerprint = 1469598103934665603ull;
  fingerprint = ism::mixChecksum(fingerprint, static_cast<std::uint64_t>(peaks.size()));
  for (const auto& peak : peaks)
  {
    fingerprint = ism::mixChecksum(fingerprint, ism::checksumFloat(peak.x));
    fingerprint = ism::mixChecksum(fingerprint, ism::checksumFloat(peak.y));
    fingerprint = ism::mixChecksum(fingerprint, ism::checksumFloat(peak.z));
    fingerprint = ism::mixChecksum(fingerprint, checksumDoubleForPublicFingerprint(peak.density));
    fingerprint = ism::mixChecksum(fingerprint, static_cast<std::uint64_t>(peak.class_id + 1));
  }
  result.strongest_peak_density = peaks.empty() ? 0.0 : peaks.front().density;
  result.peak_fingerprint = fingerprint;
  return result;
}

void
runPublicFindObjectsCase(const int argc,
                         char** argv,
                         const int iterations,
                         const int warmup_iterations)
{
  const std::string label = "public_find_objects_descriptor_assignment";
  if (!caseEnabled(argc, argv, label))
    return;

  const std::size_t descriptors =
      static_cast<std::size_t>(parseIntArg(argc, argv, "--descriptors", 512));
  const std::size_t clusters = static_cast<std::size_t>(parseIntArg(argc, argv, "--clusters", 184));
  const auto model = makeDeterministicModel(clusters);
  const auto cloud = makePublicEntryCloud(descriptors);
  const auto normals = makePublicEntryNormals(descriptors);
  pcl::ism::ImplicitShapeModelEstimation<153, pcl::PointXYZ, pcl::Normal> estimator;
  estimator.setSamplingSize(0.01f);
  estimator.setNumberOfClusters(static_cast<unsigned int>(clusters));
  estimator.setFeatureEstimator(SyntheticIsmFeature::Ptr(new SyntheticIsmFeature));
  pcl::features::ISMVoteList<pcl::PointXYZ>::Ptr votes;

  const double ms = timeKernel(
      [&] { votes = estimator.findObjects(model, cloud, normals, 0); },
      iterations,
      warmup_iterations);
  const ism::PublicFindObjectsResult result = summarizePublicFindObjects(votes, 0u);
  const auto checksum = ism::checksumPublicFindObjects(result);

  std::cout << std::left << std::setw(36) << label << ": " << std::fixed
            << std::setprecision(6) << ms << " ms/iter\n";
  std::cout << "  Total Time: " << (ms * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", descriptors: " << descriptors
            << ", clusters: " << clusters << ", votes: " << result.votes
            << ", strongest_peak_density: " << result.strongest_peak_density
            << ", peak_fingerprint: " << result.peak_fingerprint << '\n';
  std::cout << label << " checksum: " << checksum << '\n';
}

} // namespace

int
main(int argc, char** argv)
{
  const int actual_iterations = parseIntArg(argc, argv, "--iterations", 100);
  const int warmup_iterations = parseIntArg(argc, argv, "--warmup-iterations", 5);

  std::cout << "Dataset: synthetic ISM local formula diagnostic\n";
  std::cout << "Iterations: " << actual_iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  std::cout << "Build Path: " << ism::pathName() << '\n';

  runDescriptorCase(argc, argv, actual_iterations, warmup_iterations);
  runDescriptorBatchCase(argc, argv, actual_iterations, warmup_iterations);
  runPublicFindObjectsCase(argc, argv, actual_iterations, warmup_iterations);
  runSigmaCase(argc, argv, actual_iterations, warmup_iterations);
  runDensityCase(argc, argv, actual_iterations, warmup_iterations);
  return 0;
}
