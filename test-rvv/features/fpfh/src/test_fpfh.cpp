/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）给 FPFH（Fast Point Feature Histogram，快速点特征直方图）
 * topic 建立 scalar reference（标量参考链路）和 production-shaped diagnostic
 * （生产形态诊断）基线。后续 RVV candidate 必须继续通过这些对拍，不能只靠 bench 数字。
 *
 * 证据边界：
 * 本文件不修改 `features/include/pcl/features/impl/fpfh.hpp`。Std/RVV 两个构建都运行同一套
 * production header，用来验证当前 scaffold（脚手架）和未来候选的 correctness gate
 * （正确性验收条件）。
 */

#include "fpfh.h"

#include <pcl/test/gtest.h>
#include <pcl/features/fpfh.h>
#include <pcl/point_cloud.h>
#include <pcl/search/kdtree.h>

#include <cmath>
#include <limits>

namespace fpfh_test = pcl::features::rvv_test::fpfh;

namespace
{
using Estimator = pcl::FPFHEstimation<fpfh_test::PointT, fpfh_test::PointT, pcl::FPFHSignature33>;

void
expectMatrixNear(const Eigen::MatrixXf& actual, const Eigen::MatrixXf& expected, const float tolerance)
{
  ASSERT_EQ(actual.rows(), expected.rows());
  ASSERT_EQ(actual.cols(), expected.cols());
  for (Eigen::Index row = 0; row < actual.rows(); ++row)
    for (Eigen::Index col = 0; col < actual.cols(); ++col)
      EXPECT_NEAR(actual(row, col), expected(row, col), tolerance)
          << "row=" << row << " col=" << col;
}

void
expectVectorNear(const Eigen::VectorXf& actual, const Eigen::VectorXf& expected, const float tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (Eigen::Index i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "bin=" << i;
}
} // namespace

TEST(FPFHReference, ComputesSPFHHistogramLikeProductionHelper)
{
  const auto cloud = fpfh_test::makeFeatureCloud(9);
  const pcl::Indices neighborhood = fpfh_test::makeWrappedNeighborhood(*cloud, 40, 17);

  Estimator fpfh;
  constexpr int bins = 11;
  Eigen::MatrixXf expected(1, bins);
  Eigen::MatrixXf expected_f2(1, bins);
  Eigen::MatrixXf expected_f3(1, bins);
  Eigen::MatrixXf actual(1, bins);
  Eigen::MatrixXf actual_f2(1, bins);
  Eigen::MatrixXf actual_f3(1, bins);
  expected.setZero();
  expected_f2.setZero();
  expected_f3.setZero();
  actual.setZero();
  actual_f2.setZero();
  actual_f3.setZero();

  fpfh_test::computePointSPFHReference(*cloud, 40, 0, neighborhood, expected, expected_f2, expected_f3);
  fpfh.computePointSPFHSignature(*cloud, *cloud, 40, 0, neighborhood, actual, actual_f2, actual_f3);

  expectMatrixNear(actual, expected, 1e-5f);
  expectMatrixNear(actual_f2, expected_f2, 1e-5f);
  expectMatrixNear(actual_f3, expected_f3, 1e-5f);
}

TEST(FPFHReference, WeightsSPFHLikeProductionHelper)
{
  constexpr int rows = 13;
  constexpr int bins = 11;
  Eigen::MatrixXf hist_f1(rows, bins);
  Eigen::MatrixXf hist_f2(rows, bins);
  Eigen::MatrixXf hist_f3(rows, bins);
  for (int row = 0; row < rows; ++row)
  {
    for (int bin = 0; bin < bins; ++bin)
    {
      hist_f1(row, bin) = 0.25f + static_cast<float>((row + 1) * (bin + 2) % 17);
      hist_f2(row, bin) = 0.50f + static_cast<float>((row + 3) * (bin + 1) % 19);
      hist_f3(row, bin) = 0.75f + static_cast<float>((row + 5) * (bin + 4) % 23);
    }
  }

  const pcl::Indices indices = fpfh_test::makeSequentialIndices(rows);
  std::vector<float> dists(static_cast<std::size_t>(rows));
  for (std::size_t i = 0; i < dists.size(); ++i)
    dists[i] = i == 0 ? 0.0f : 0.35f + 0.17f * static_cast<float>(i);

  Eigen::VectorXf expected;
  Eigen::VectorXf actual;
  Estimator fpfh;
  fpfh_test::weightPointSPFHReference(hist_f1, hist_f2, hist_f3, indices, dists, expected);
  fpfh.weightPointSPFHSignature(hist_f1, hist_f2, hist_f3, indices, dists, actual);

  expectVectorNear(actual, expected, 1e-5f);
}

TEST(FPFHReference, WeightsSPFHWithRemappedRowsLikeProductionHelper)
{
  constexpr int rows = 17;
  constexpr int bins = 11;
  Eigen::MatrixXf hist_f1(rows, bins);
  Eigen::MatrixXf hist_f2(rows, bins);
  Eigen::MatrixXf hist_f3(rows, bins);
  for (int row = 0; row < rows; ++row)
  {
    for (int bin = 0; bin < bins; ++bin)
    {
      hist_f1(row, bin) = 0.10f + static_cast<float>((row + 4) * (bin + 1) % 13);
      hist_f2(row, bin) = 0.20f + static_cast<float>((row + 6) * (bin + 5) % 17);
      hist_f3(row, bin) = 0.30f + static_cast<float>((row + 8) * (bin + 7) % 19);
    }
  }

  // `computeFeature` 会把邻域 surface index 重映射到 SPFH matrix row；这个 case 证明生产
  // helper 的 RVV 分支能处理非连续但合法的 row indices，而不是只覆盖 dense sequential rows。
  const pcl::Indices indices = {0, 12, 4, 15, 2, 9, 6};
  std::vector<float> dists(indices.size());
  for (std::size_t i = 0; i < dists.size(); ++i)
    dists[i] = i == 0 ? 0.0f : 0.30f + 0.09f * static_cast<float>(i);

  Eigen::VectorXf expected;
  Eigen::VectorXf actual;
  Estimator fpfh;
  fpfh_test::weightPointSPFHReference(hist_f1, hist_f2, hist_f3, indices, dists, expected);
  fpfh.weightPointSPFHSignature(hist_f1, hist_f2, hist_f3, indices, dists, actual);

  expectVectorNear(actual, expected, 1e-5f);
}

TEST(FPFHCandidate, DenseRowsRVVWeightsSPFHLikeProductionHelper)
{
  constexpr int rows = 29;
  constexpr int bins = 11;
  Eigen::MatrixXf hist_f1(rows, bins);
  Eigen::MatrixXf hist_f2(rows, bins);
  Eigen::MatrixXf hist_f3(rows, bins);
  for (int row = 0; row < rows; ++row)
  {
    for (int bin = 0; bin < bins; ++bin)
    {
      hist_f1(row, bin) = 0.125f + static_cast<float>((row + 2) * (bin + 5) % 29);
      hist_f2(row, bin) = 0.375f + static_cast<float>((row + 7) * (bin + 3) % 31);
      hist_f3(row, bin) = 0.625f + static_cast<float>((row + 11) * (bin + 9) % 37);
    }
  }

  const pcl::Indices indices = fpfh_test::makeSequentialIndices(rows);
  std::vector<float> dists(static_cast<std::size_t>(rows));
  for (std::size_t i = 0; i < dists.size(); ++i)
    dists[i] = i == 0 ? 0.0f : 0.20f + 0.041f * static_cast<float>(i);

  Eigen::VectorXf expected;
  Eigen::VectorXf candidate;
  Estimator fpfh;
  fpfh.weightPointSPFHSignature(hist_f1, hist_f2, hist_f3, indices, dists, expected);
  fpfh_test::weightPointSPFHDenseRowsRVV(hist_f1, hist_f2, hist_f3, indices, dists, candidate);

  expectVectorNear(candidate, expected, 2e-4f);
}

TEST(FPFHPublicPath, ComputesFiniteDescriptorForSyntheticCloud)
{
  const auto cloud = fpfh_test::makeFeatureCloud(12);
  auto tree = pcl::search::KdTree<fpfh_test::PointT>::Ptr(new pcl::search::KdTree<fpfh_test::PointT>);

  Estimator fpfh;
  fpfh.setInputCloud(cloud);
  fpfh.setInputNormals(cloud);
  fpfh.setSearchMethod(tree);
  fpfh.setKSearch(24);

  pcl::PointCloud<pcl::FPFHSignature33> output;
  fpfh.compute(output);

  ASSERT_EQ(output.size(), cloud->size());
  ASSERT_TRUE(output.is_dense);
  for (const auto& descriptor : output)
  {
    for (const float value : descriptor.histogram)
      ASSERT_TRUE(std::isfinite(value));

    float f1_sum = 0.0f;
    float f2_sum = 0.0f;
    float f3_sum = 0.0f;
    for (int i = 0; i < 11; ++i)
    {
      f1_sum += descriptor.histogram[i];
      f2_sum += descriptor.histogram[i + 11];
      f3_sum += descriptor.histogram[i + 22];
    }
    EXPECT_NEAR(f1_sum, 100.0f, 1e-3f);
    EXPECT_NEAR(f2_sum, 100.0f, 1e-3f);
    EXPECT_NEAR(f3_sum, 100.0f, 1e-3f);
  }
}

TEST(FPFHPublicPath, MarksNoNeighborQueryAsNonDense)
{
  auto cloud = fpfh_test::makeFeatureCloud(10);
  (*cloud)[3].x += 1000.0f;
  (*cloud)[3].y += 1000.0f;
  (*cloud)[3].z += 1000.0f;
  auto surface = fpfh_test::CloudT::Ptr(new fpfh_test::CloudT);
  surface->reserve(cloud->size() - 1);
  for (std::size_t i = 0; i < cloud->size(); ++i)
  {
    if (i != 3)
      surface->push_back((*cloud)[i]);
  }
  surface->width = static_cast<std::uint32_t>(surface->size());
  surface->height = 1;
  surface->is_dense = true;

  Estimator fpfh;
  fpfh.setInputCloud(cloud);
  fpfh.setSearchSurface(surface);
  fpfh.setInputNormals(surface);
  fpfh.setSearchMethod(pcl::search::KdTree<fpfh_test::PointT>::Ptr(new pcl::search::KdTree<fpfh_test::PointT>));
  fpfh.setRadiusSearch(0.075);

  pcl::PointCloud<pcl::FPFHSignature33> output;
  fpfh.compute(output);

  ASSERT_EQ(output.size(), cloud->size());
  EXPECT_FALSE(output.is_dense);
  for (const float value : output[3].histogram)
    EXPECT_TRUE(std::isnan(value));
}
