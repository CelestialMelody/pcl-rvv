/*
 * 本文件做什么：
 * 这些测试对拍 ISM diagnostic helper（诊断辅助函数）的 Std/RVV 同构链路。
 * 它们不调用 `trainISM()` 或 `findObjects()`，而是把源码中的局部公式抽成固定输入，
 * 用于证明 RVV 候选没有改变公式语义。
 */

#include "ism.h"

#include <cmath>
#include <Eigen/Core>
#include <gtest/gtest.h>
#include <pcl/recognition/impl/implicit_shape_model.hpp>

namespace ism = pcl::test::implicit_shape_model_rvv;

TEST(ImplicitShapeModelDiagnostics, DescriptorNearestClusterMatchesReference)
{
  const std::size_t dimensions = 153;
  const std::size_t clusters = 64;
  const auto descriptor = ism::makeDescriptor(dimensions);
  const auto centers = ism::makeClusterCenters(clusters, dimensions);

  const auto expected =
      ism::nearestClusterDistanceStd(descriptor.data(), centers.data(), clusters, dimensions);
  const auto actual =
      ism::nearestClusterDistanceCandidate(descriptor.data(), centers.data(), clusters, dimensions);

  EXPECT_EQ(actual.index, expected.index);
  EXPECT_NEAR(actual.distance, expected.distance, 1e-4f);
}

TEST(ImplicitShapeModelDiagnostics, DescriptorBatchAssignmentMatchesReference)
{
  const std::size_t dimensions = 153;
  const std::size_t descriptors = 97;
  const std::size_t clusters = 64;
  const auto descriptor_cloud = ism::makeDescriptorBatch(descriptors, dimensions);
  const auto centers = ism::makeClusterCenters(clusters, dimensions);

  const auto expected = ism::assignDescriptorBatchStd(
      descriptor_cloud.data(), centers.data(), descriptors, clusters, dimensions);
  const auto actual = ism::assignDescriptorBatchCandidate(
      descriptor_cloud.data(), centers.data(), descriptors, clusters, dimensions);

  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(actual[i], expected[i]) << "descriptor index " << i;
}

TEST(ImplicitShapeModelDiagnostics, ProductionNearestClusterHelperHandlesColumnMajorCenters)
{
  Eigen::VectorXf descriptor(153);
  Eigen::MatrixXf centers(64, 153);
  const auto descriptor_values = ism::makeDescriptor(153);
  const auto center_values = ism::makeClusterCenters(64, 153);

  for (std::size_t dim = 0; dim < descriptor_values.size(); ++dim)
    descriptor(static_cast<Eigen::Index>(dim)) = descriptor_values[dim];
  for (Eigen::Index row = 0; row < centers.rows(); ++row)
    for (Eigen::Index col = 0; col < centers.cols(); ++col)
      centers(row, col) = center_values[static_cast<std::size_t>(row) * 153u +
                                        static_cast<std::size_t>(col)];

  const unsigned int expected = pcl::ism::detail::findNearestClusterIndexStd(descriptor, centers, 64);
  const unsigned int actual = pcl::ism::detail::findNearestClusterIndex<153>(descriptor, centers, 64);

  EXPECT_EQ(actual, expected);
}

TEST(ImplicitShapeModelDiagnostics, ProductionNearestClusterHelperMatchesReferenceOnNon153Dimensions)
{
  Eigen::VectorXf descriptor(32);
  Eigen::MatrixXf centers(17, 32);
  const auto descriptor_values = ism::makeDescriptor(32);
  const auto center_values = ism::makeClusterCenters(17, 32);

  for (std::size_t dim = 0; dim < descriptor_values.size(); ++dim)
    descriptor(static_cast<Eigen::Index>(dim)) = descriptor_values[dim];
  for (Eigen::Index row = 0; row < centers.rows(); ++row)
    for (Eigen::Index col = 0; col < centers.cols(); ++col)
      centers(row, col) = center_values[static_cast<std::size_t>(row) * 32u +
                                        static_cast<std::size_t>(col)];

  const unsigned int expected = pcl::ism::detail::findNearestClusterIndexStd(descriptor, centers, 17);
  const unsigned int actual = pcl::ism::detail::findNearestClusterIndex<32>(descriptor, centers, 17);

  EXPECT_EQ(actual, expected);
}

TEST(ImplicitShapeModelDiagnostics, PublicFindObjectsChecksumIncludesPeakFingerprint)
{
  const ism::PublicFindObjectsResult votes_only{494u, 1u, 0.0, 0u};
  const ism::PublicFindObjectsResult with_peak_fingerprint{494u, 1u, 12.5, 7u};

  EXPECT_NE(ism::checksumPublicFindObjects(votes_only),
            ism::checksumPublicFindObjects(with_peak_fingerprint));
}

TEST(ImplicitShapeModelDiagnostics, PairwiseSigmaMaxDotMatchesReference)
{
  const auto points = ism::makeTrainingPoints(257);

  const float expected = ism::maxPairwiseDotSigmaStd(points.data(), points.size());
  const float actual = ism::maxPairwiseDotSigmaCandidate(points.data(), points.size());

  EXPECT_NEAR(actual, expected, 1e-4f);
}

TEST(ImplicitShapeModelDiagnostics, DensityWeightedSumMatchesReference)
{
  const auto distances = ism::makeSquaredDistances(4097);
  const auto strengths = ism::makeVoteStrengths(4097);
  const float sigma = 0.42f;

  const float expected =
      ism::densityWeightedSumStd(distances.data(), strengths.data(), distances.size(), sigma);
  const float actual =
      ism::densityWeightedSumCandidate(distances.data(), strengths.data(), distances.size(), sigma);

  EXPECT_NEAR(actual, expected, std::max(1e-3f, std::fabs(expected) * 3e-5f));
}
