/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）为 VFH（Viewpoint Feature Histogram，
 * 视点特征直方图）建立 test-only scalar reference（测试专用标量参考链路）。
 * Phase 000 已完成 same-chain（同构链路）对拍；后续如果进入 PI1/PI2，
 * 仍要先保持这两个 reference gate（参考验收门）不变，不能只靠 bench（性能测试）数字。
 *
 * 证据边界：
 * 本文件不修改 production 头文件。Std/RVV 两个构建都运行 public compute
 * reference gate（公开入口参考验收）；RVV 构建额外要求第一阶段候选 helper
 * 真实返回成功并接近标量 reference。
 */

#include "vfh.h"

#include <pcl/features/vfh.h>
#include <pcl/test/gtest.h>

namespace vfh_test = pcl::features::rvv_test::vfh;

namespace
{
using Estimator = pcl::VFHEstimation<vfh_test::PointT, vfh_test::PointT, pcl::VFHSignature308>;

void
expectSignatureNear(const vfh_test::Signature& actual,
                    const vfh_test::Signature& expected,
                    const float tolerance)
{
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "bin=" << i;
}

float
signatureSum(const vfh_test::Signature& signature)
{
  float sum = 0.0f;
  for (const float value : signature)
    sum += value;
  return sum;
}

vfh_test::Signature
computePublicVFH(const vfh_test::CloudT::Ptr& cloud, const vfh_test::VFHOptions& options)
{
  Estimator estimator;
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
  if (output.size() != 1u)
    return signature;
  std::copy(std::begin(output[0].histogram), std::end(output[0].histogram), signature.begin());
  return signature;
}

vfh_test::Signature
computePublicVFHScalarFallback(const vfh_test::CloudT::Ptr& cloud,
                               const pcl::Indices& indices,
                               const vfh_test::VFHOptions& options)
{
  const Eigen::Vector4f normal = vfh_test::computeNormalCentroidReference(*cloud, indices);

  Estimator estimator;
  estimator.setInputCloud(cloud);
  estimator.setInputNormals(cloud);
  estimator.setSearchSurface(cloud);
  estimator.setViewPoint(options.viewpoint.x(), options.viewpoint.y(), options.viewpoint.z());
  estimator.setNormalizeBins(options.normalize_bins);
  estimator.setNormalizeDistance(options.normalize_distances);
  estimator.setFillSizeComponent(options.size_component);
  estimator.setNormalToUse(normal.head<3>());
  estimator.setUseGivenNormal(true);

  pcl::PointCloud<pcl::VFHSignature308> output;
  estimator.compute(output);

  vfh_test::Signature signature{};
  if (output.size() != 1u)
    return signature;
  std::copy(std::begin(output[0].histogram), std::end(output[0].histogram), signature.begin());
  return signature;
}
} // namespace

TEST(VFHReference, MatchesPublicComputeDefaultDescriptor)
{
  const auto cloud = vfh_test::makeFeatureCloud(9);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  const vfh_test::VFHOptions options;

  const auto expected = computePublicVFH(cloud, options);
  const auto reference = vfh_test::computeVFHSignatureReference(*cloud, indices, options);

  EXPECT_EQ(reference.size(), expected.size());
  expectSignatureNear(reference, expected, 2e-3f);
  EXPECT_NEAR(signatureSum(reference), signatureSum(expected), 1e-3f);
  EXPECT_GT(signatureSum(reference), 300.0f);
}

TEST(VFHReference, MatchesPublicComputeWithSizeComponent)
{
  const auto cloud = vfh_test::makeFeatureCloud(8);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  vfh_test::VFHOptions options;
  options.size_component = true;
  options.normalize_distances = true;

  const auto expected = computePublicVFH(cloud, options);
  const auto reference = vfh_test::computeVFHSignatureReference(*cloud, indices, options);

  EXPECT_EQ(reference.size(), expected.size());
  expectSignatureNear(reference, expected, 2e-3f);
  EXPECT_NEAR(signatureSum(reference), signatureSum(expected), 1e-3f);
  EXPECT_GT(signatureSum(reference), 300.0f);
}

TEST(VFHCandidate, CentroidSPFHRVVMatchesReference)
{
  const auto cloud = vfh_test::makeFeatureCloud(12);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  const vfh_test::VFHOptions options;
  const auto expected = vfh_test::computeVFHSignatureReference(*cloud, indices, options);

  vfh_test::Signature candidate{};
#if defined(__RVV10__)
  ASSERT_TRUE(vfh_test::computeVFHSignatureCentroidSPFHRVV(*cloud, indices, candidate, options));
  expectSignatureNear(candidate, expected, 2e-3f);
#else
  EXPECT_FALSE(vfh_test::computeVFHSignatureCentroidSPFHRVV(*cloud, indices, candidate, options));
  (void)expected;
#endif
}

TEST(VFHCandidate, SPFHAndViewpointRVVMatchesReference)
{
  const auto cloud = vfh_test::makeFeatureCloud(12);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  const vfh_test::VFHOptions options;
  const auto expected = vfh_test::computeVFHSignatureReference(*cloud, indices, options);

  vfh_test::Signature candidate{};
#if defined(__RVV10__)
  ASSERT_TRUE(vfh_test::computeVFHSignatureSPFHAndViewpointRVV(*cloud, indices, candidate, options));
  expectSignatureNear(candidate, expected, 2e-3f);
#else
  EXPECT_FALSE(vfh_test::computeVFHSignatureSPFHAndViewpointRVV(*cloud, indices, candidate, options));
  (void)expected;
#endif
}

TEST(VFHCandidate, CentroidsSPFHAndViewpointRVVMatchesReference)
{
  const auto cloud = vfh_test::makeFeatureCloud(12);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  const vfh_test::VFHOptions options;
  const auto expected = vfh_test::computeVFHSignatureReference(*cloud, indices, options);

  vfh_test::Signature candidate{};
#if defined(__RVV10__)
  ASSERT_TRUE(vfh_test::computeVFHSignatureCentroidsSPFHAndViewpointRVV(*cloud, indices, candidate, options));
  expectSignatureNear(candidate, expected, 2e-3f);
#else
  EXPECT_FALSE(vfh_test::computeVFHSignatureCentroidsSPFHAndViewpointRVV(*cloud, indices, candidate, options));
  (void)expected;
#endif
}

TEST(VFHProductionRVV, DefaultPublicBoundaryHelperMatchesPublicCompute)
{
  const auto cloud = vfh_test::makeFeatureCloud(12);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  const vfh_test::VFHOptions options;
  const auto expected = computePublicVFHScalarFallback(cloud, indices, options);

#if defined(__RVV10__)
  vfh_test::Signature signature{};
  ASSERT_TRUE((pcl::detail::computeVFHSignatureRVV<vfh_test::PointT,
                                                   vfh_test::PointT,
                                                   pcl::VFHSignature308>(
      *cloud,
      *cloud,
      indices,
      options.viewpoint,
      options.normalize_bins,
      options.normalize_distances,
      options.size_component,
      signature.data())));
  expectSignatureNear(signature, expected, 2e-3f);
#else
  (void)indices;
  (void)expected;
#endif
}

TEST(VFHProductionRVV, HelperMatchesScalarForOverRangeNormalAngles)
{
  const auto cloud = vfh_test::makeFeatureCloud(12);
  for (std::size_t i = 0; i < cloud->size(); i += 5)
  {
    (*cloud)[i].normal_x *= 1.8f;
    (*cloud)[i].normal_y *= 1.8f;
    (*cloud)[i].normal_z *= 1.8f;
  }
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  const vfh_test::VFHOptions options;
  const auto expected = computePublicVFHScalarFallback(cloud, indices, options);

#if defined(__RVV10__)
  vfh_test::Signature signature{};
  ASSERT_TRUE((pcl::detail::computeVFHSignatureRVV<vfh_test::PointT,
                                                   vfh_test::PointT,
                                                   pcl::VFHSignature308>(
      *cloud,
      *cloud,
      indices,
      options.viewpoint,
      options.normalize_bins,
      options.normalize_distances,
      options.size_component,
      signature.data())));
  expectSignatureNear(signature, expected, 2e-3f);
#else
  (void)indices;
  (void)expected;
#endif
}

TEST(VFHProductionRVV, HelperRejectsSizeComponentFallbackBoundary)
{
  const auto cloud = vfh_test::makeFeatureCloud(12);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  vfh_test::VFHOptions options;
  options.size_component = true;

#if defined(__RVV10__)
  vfh_test::Signature signature{};
  EXPECT_FALSE((pcl::detail::computeVFHSignatureRVV<vfh_test::PointT,
                                                    vfh_test::PointT,
                                                    pcl::VFHSignature308>(
      *cloud,
      *cloud,
      indices,
      options.viewpoint,
      options.normalize_bins,
      options.normalize_distances,
      options.size_component,
      signature.data())));
#else
  (void)cloud;
  (void)indices;
#endif
}

TEST(VFHProductionRVV, HelperRejectsNormalizeBinsFallbackBoundary)
{
  const auto cloud = vfh_test::makeFeatureCloud(12);
  const auto indices = vfh_test::makeSequentialIndices(cloud->size());
  vfh_test::VFHOptions options;
  options.normalize_bins = false;

#if defined(__RVV10__)
  vfh_test::Signature signature{};
  EXPECT_FALSE((pcl::detail::computeVFHSignatureRVV<vfh_test::PointT,
                                                    vfh_test::PointT,
                                                    pcl::VFHSignature308>(
      *cloud,
      *cloud,
      indices,
      options.viewpoint,
      options.normalize_bins,
      options.normalize_distances,
      options.size_component,
      signature.data())));
#else
  (void)cloud;
  (void)indices;
#endif
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
