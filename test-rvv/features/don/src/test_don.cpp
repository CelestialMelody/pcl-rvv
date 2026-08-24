/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 DON（Difference of Normals，
 * 法线差分）逐点热点的 test-only（仅测试使用）RVV candidate 是否和
 * 手工期望值一致。它们先约束 normal 差、非有限值置零和 curvature 写回，
 * 再让后续实现可以在同一合同下替换为 RVV 链路。
 *
 * 证据边界：
 * 本文件不修改 production（生产源码），也不证明真实公开入口已经命中 RVV。
 * 它只证明当前 topic 的 candidate helper 能复刻 `computeFeature()` 中的
 * 逐点计算语义，后续仍需要 production-shaped diagnostic（生产形态诊断）、
 * 反汇编归属和板卡性能证据。
 */

#include "don.h"

#include <pcl/features/don.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace don = pcl::features::rvv_test::don;

namespace {

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeInputCloud(const std::size_t count)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(count);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    (*cloud)[i].x = static_cast<float>(i) * 0.25f;
    (*cloud)[i].y = static_cast<float>(i % 11) - 3.0f;
    (*cloud)[i].z = static_cast<float>(i % 7) + 0.5f;
  }
  return cloud;
}

pcl::PointCloud<pcl::Normal>::Ptr
makeNormals(const std::size_t count, const float bias)
{
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>>();
  normals->width = static_cast<std::uint32_t>(count);
  normals->height = 1;
  normals->is_dense = false;
  normals->points.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    (*normals)[i].normal_x = bias + static_cast<float>(i % 13) * 0.125f;
    (*normals)[i].normal_y = bias * 0.5f + static_cast<float>(i % 17) * 0.0625f;
    (*normals)[i].normal_z = bias * 0.25f - static_cast<float>(i % 19) * 0.03125f;
  }
  return normals;
}

} // namespace

TEST(DonRVV, ComputesNormalDifferenceZeroesInvalidAndWritesCurvature)
{
  std::vector<pcl::Normal> small(4);
  std::vector<pcl::Normal> large(4);
  std::vector<pcl::Normal> out(4);

  small[0].normal_x = 3.0f;
  small[0].normal_y = 1.0f;
  small[0].normal_z = -1.0f;
  large[0].normal_x = 1.0f;
  large[0].normal_y = -1.0f;
  large[0].normal_z = 1.0f;

  small[1].normal_x = -2.0f;
  small[1].normal_y = 4.0f;
  small[1].normal_z = 8.0f;
  large[1].normal_x = 2.0f;
  large[1].normal_y = 2.0f;
  large[1].normal_z = 2.0f;

  small[2].normal_x = std::numeric_limits<float>::quiet_NaN();
  small[2].normal_y = 1.0f;
  small[2].normal_z = 1.0f;
  large[2].normal_x = 0.0f;
  large[2].normal_y = 1.0f;
  large[2].normal_z = 1.0f;

  small[3].normal_x = 7.0f;
  small[3].normal_y = -5.0f;
  small[3].normal_z = std::numeric_limits<float>::infinity();
  large[3].normal_x = 1.0f;
  large[3].normal_y = -1.0f;
  large[3].normal_z = 0.0f;

  don::computeDoNRVV(small.data(), large.data(), out.data(), out.size());

  EXPECT_FLOAT_EQ(out[0].normal_x, 1.0f);
  EXPECT_FLOAT_EQ(out[0].normal_y, 1.0f);
  EXPECT_FLOAT_EQ(out[0].normal_z, -1.0f);
  EXPECT_NEAR(out[0].curvature, std::sqrt(3.0f), 1e-6f);

  EXPECT_FLOAT_EQ(out[1].normal_x, -2.0f);
  EXPECT_FLOAT_EQ(out[1].normal_y, 1.0f);
  EXPECT_FLOAT_EQ(out[1].normal_z, 3.0f);
  EXPECT_NEAR(out[1].curvature, std::sqrt(14.0f), 1e-6f);

  EXPECT_FLOAT_EQ(out[2].normal_x, 0.0f);
  EXPECT_FLOAT_EQ(out[2].normal_y, 0.0f);
  EXPECT_FLOAT_EQ(out[2].normal_z, 0.0f);
  EXPECT_FLOAT_EQ(out[2].curvature, 0.0f);

  EXPECT_FLOAT_EQ(out[3].normal_x, 0.0f);
  EXPECT_FLOAT_EQ(out[3].normal_y, 0.0f);
  EXPECT_FLOAT_EQ(out[3].normal_z, 0.0f);
  EXPECT_FLOAT_EQ(out[3].curvature, 0.0f);
}

TEST(DonRVV, HelperMatchesProductionEstimatorForOrderedNormalCloud)
{
  constexpr std::size_t kPoints = 41;
  const auto input = makeInputCloud(kPoints);
  const auto small = makeNormals(kPoints, 1.0f);
  const auto large = makeNormals(kPoints, -0.5f);
  (*small)[7].normal_y = std::numeric_limits<float>::quiet_NaN();
  (*large)[19].normal_z = std::numeric_limits<float>::infinity();

  pcl::DifferenceOfNormalsEstimation<pcl::PointXYZ, pcl::Normal, pcl::Normal> estimator;
  estimator.setInputCloud(input);
  estimator.setNormalScaleSmall(small);
  estimator.setNormalScaleLarge(large);

  pcl::PointCloud<pcl::Normal> production_output;
  production_output.resize(kPoints);
  ASSERT_TRUE(estimator.initCompute());
  estimator.computeFeature(production_output);

  std::vector<pcl::Normal> helper_output(kPoints);
  don::computeDoNRVV(small->points.data(), large->points.data(), helper_output.data(), helper_output.size());

  ASSERT_EQ(production_output.size(), helper_output.size());
  for (std::size_t i = 0; i < helper_output.size(); ++i) {
    EXPECT_FLOAT_EQ(production_output[i].normal_x, helper_output[i].normal_x) << "i=" << i;
    EXPECT_FLOAT_EQ(production_output[i].normal_y, helper_output[i].normal_y) << "i=" << i;
    EXPECT_FLOAT_EQ(production_output[i].normal_z, helper_output[i].normal_z) << "i=" << i;
    EXPECT_FLOAT_EQ(production_output[i].curvature, helper_output[i].curvature) << "i=" << i;
  }
}

TEST(DonRVV, HandlesTailSizesThatDoNotMatchVectorLength)
{
  std::vector<pcl::Normal> small(37);
  std::vector<pcl::Normal> large(37);
  std::vector<pcl::Normal> out(37);

  for (std::size_t i = 0; i < small.size(); ++i) {
    small[i].normal_x = static_cast<float>(i) * 0.25f + 1.0f;
    small[i].normal_y = static_cast<float>(i % 5) - 2.0f;
    small[i].normal_z = static_cast<float>(i % 7) * 0.5f;
    large[i].normal_x = static_cast<float>(i) * 0.125f;
    large[i].normal_y = -1.0f;
    large[i].normal_z = 0.25f;
  }

  don::computeDoNRVV(small.data(), large.data(), out.data(), out.size());

  for (std::size_t i = 0; i < out.size(); ++i) {
    const float expected_x = (small[i].normal_x - large[i].normal_x) * 0.5f;
    const float expected_y = (small[i].normal_y - large[i].normal_y) * 0.5f;
    const float expected_z = (small[i].normal_z - large[i].normal_z) * 0.5f;
    EXPECT_FLOAT_EQ(out[i].normal_x, expected_x) << "i=" << i;
    EXPECT_FLOAT_EQ(out[i].normal_y, expected_y) << "i=" << i;
    EXPECT_FLOAT_EQ(out[i].normal_z, expected_z) << "i=" << i;
    EXPECT_NEAR(out[i].curvature,
                std::sqrt(expected_x * expected_x + expected_y * expected_y + expected_z * expected_z),
                1e-6f)
        << "i=" << i;
  }
}

TEST(DonProductionDirect, ComputesExactNormalPublicEntrySemantics)
{
  constexpr std::size_t kPoints = 43;
  const auto input = makeInputCloud(kPoints);
  const auto small = makeNormals(kPoints, 2.0f);
  const auto large = makeNormals(kPoints, -1.0f);
  (*small)[3].normal_x = std::numeric_limits<float>::quiet_NaN();
  (*large)[17].normal_z = std::numeric_limits<float>::infinity();

  pcl::DifferenceOfNormalsEstimation<pcl::PointXYZ, pcl::Normal, pcl::Normal> estimator;
  estimator.setInputCloud(input);
  estimator.setNormalScaleSmall(small);
  estimator.setNormalScaleLarge(large);
  pcl::Feature<pcl::PointXYZ, pcl::Normal>& feature = estimator;

  pcl::PointCloud<pcl::Normal> output;
  feature.compute(output);
  ASSERT_EQ(output.size(), kPoints);

  std::vector<pcl::Normal> expected(kPoints);
  don::computeDoNScalar(small->points.data(), large->points.data(), expected.data(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_FLOAT_EQ(output[i].normal_x, expected[i].normal_x) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].normal_y, expected[i].normal_y) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].normal_z, expected[i].normal_z) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].curvature, expected[i].curvature) << "i=" << i;
  }
}

TEST(DonProductionDirect, FallsBackForNonExactOutputPointType)
{
  constexpr std::size_t kPoints = 29;
  const auto input = makeInputCloud(kPoints);
  const auto small = makeNormals(kPoints, 0.25f);
  const auto large = makeNormals(kPoints, -0.75f);

  pcl::DifferenceOfNormalsEstimation<pcl::PointXYZ, pcl::Normal, pcl::PointNormal> estimator;
  estimator.setInputCloud(input);
  estimator.setNormalScaleSmall(small);
  estimator.setNormalScaleLarge(large);
  pcl::Feature<pcl::PointXYZ, pcl::PointNormal>& feature = estimator;

  pcl::PointCloud<pcl::PointNormal> output;
  feature.compute(output);
  ASSERT_EQ(output.size(), kPoints);

  std::vector<pcl::Normal> expected(kPoints);
  don::computeDoNScalar(small->points.data(), large->points.data(), expected.data(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_FLOAT_EQ(output[i].normal_x, expected[i].normal_x) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].normal_y, expected[i].normal_y) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].normal_z, expected[i].normal_z) << "i=" << i;
    EXPECT_FLOAT_EQ(output[i].curvature, expected[i].curvature) << "i=" << i;
  }
}

int
main(int argc, char** argv)
{
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
