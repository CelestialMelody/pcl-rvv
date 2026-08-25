/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）为 PFH（Point Feature Histogram，点特征直方图）
 * 建立 test-only scalar reference（测试专用标量参考链路）。后续 RVV candidate
 * （候选实现）必须先通过这些 same-chain（同构链路）对拍，不能只靠 bench 数字。
 *
 * 证据边界：
 * 本文件不修改 production 头文件。Std/RVV 两个构建都运行同一套测试，用来验证
 * PFH topic scaffold（脚手架）和后续候选的 correctness gate（正确性验收条件）。
 */

#include "pfh.h"

#include <pcl/features/pfh.h>
#include <pcl/point_cloud.h>
#include <pcl/search/kdtree.h>
#include <pcl/test/gtest.h>

#include <cmath>
#include <limits>

namespace pfh_test = pcl::features::rvv_test::pfh;

namespace
{
using Estimator = pcl::PFHEstimation<pfh_test::PointT, pfh_test::PointT, pcl::PFHSignature125>;
using XYZNormalEstimator = pcl::PFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::PFHSignature125>;

void
expectVectorNear(const Eigen::VectorXf& actual, const Eigen::VectorXf& expected, const float tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (Eigen::Index i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "bin=" << i;
}
} // namespace

TEST(PFHReference, ComputesPointPFHHistogramLikeProductionHelper)
{
  const auto cloud = pfh_test::makeFeatureCloud(9);
  const pcl::Indices neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, 40, 17);

  Eigen::VectorXf expected;
  Eigen::VectorXf actual(125);
  Estimator pfh;
  pfh_test::computePointPFHReference(*cloud, neighborhood, 5, expected);
  pfh.computePointPFHSignature(*cloud, *cloud, neighborhood, 5, actual);

  expectVectorNear(actual, expected, 1e-5f);
}

TEST(PFHCandidate, PairBatchRVVComputesHistogramCloseToProductionHelper)
{
  const auto cloud = pfh_test::makeFeatureCloud(10);
  const pcl::Indices neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, 52, 24);

  Eigen::VectorXf expected(125);
  Eigen::VectorXf candidate;
  Estimator pfh;
  pfh.computePointPFHSignature(*cloud, *cloud, neighborhood, 5, expected);
  pfh_test::computePointPFHSignaturePairBatchRVV(*cloud, neighborhood, 5, candidate);

  expectVectorNear(candidate, expected, 2e-3f);
}

TEST(PFHCandidate, DirectAoSRVVComputesHistogramCloseToProductionHelper)
{
  const auto cloud = pfh_test::makeFeatureCloud(10);
  const pcl::Indices neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, 52, 24);

  Eigen::VectorXf expected(125);
  Eigen::VectorXf candidate;
  Estimator pfh;
  pfh.computePointPFHSignature(*cloud, *cloud, neighborhood, 5, expected);
  pfh_test::computePointPFHSignatureDirectAoSRVV(*cloud, neighborhood, 5, candidate);

  expectVectorNear(candidate, expected, 2e-3f);
}

#if defined(__RVV10__)
TEST(PFHProductionRVV, DirectAoSHelperComputesPointNormalProductionHistogram)
{
  const auto cloud = pfh_test::makeFeatureCloud(10);
  const pcl::Indices neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, 52, 24);

  Eigen::VectorXf expected(125);
  Eigen::VectorXf candidate;
  Estimator pfh;
  pfh.setUseInternalCache(true);
  pfh.computePointPFHSignature(*cloud, *cloud, neighborhood, 5, expected);

  ASSERT_TRUE(pcl::detail::computePointPFHSignatureDirectAoSRVV(
      *cloud, *cloud, neighborhood, 5, candidate));
  expectVectorNear(candidate, expected, 2e-3f);
}

TEST(PFHProductionRVV, DirectAoSHelperComputesPointXYZNormalProductionHistogram)
{
  const auto [cloud, normals] = pfh_test::makeXYZAndNormalClouds(10);
  const pcl::Indices neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, 52, 24);

  Eigen::VectorXf expected(125);
  Eigen::VectorXf candidate;
  XYZNormalEstimator pfh;
  pfh.setUseInternalCache(true);
  pfh.computePointPFHSignature(*cloud, *normals, neighborhood, 5, expected);

  ASSERT_TRUE(pcl::detail::computePointPFHSignatureDirectAoSRVV(
      *cloud, *normals, neighborhood, 5, candidate));
  expectVectorNear(candidate, expected, 2e-3f);
}

TEST(PFHProductionRVV, DirectAoSHelperFallsBackForPointXYZNormalUnsupportedGates)
{
  const auto [cloud, normals] = pfh_test::makeXYZAndNormalClouds(6);
  const pcl::Indices small_neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, 18, 3);
  const pcl::Indices neighborhood = pfh_test::makeWrappedNeighborhood(*cloud, 18, 8);
  Eigen::VectorXf candidate;

  EXPECT_FALSE(pcl::detail::computePointPFHSignatureDirectAoSRVV(
      *cloud, *normals, small_neighborhood, 5, candidate));
  EXPECT_FALSE(pcl::detail::computePointPFHSignatureDirectAoSRVV(
      *cloud, *normals, neighborhood, 4, candidate));
}
#endif
