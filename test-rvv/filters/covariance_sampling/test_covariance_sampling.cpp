#include "covariance_sampling_diag.hpp"

#include <gtest/gtest.h>

namespace {

struct FixtureData {
  pcl::PointCloud<pcl::PointXYZ> cloud;
  pcl::PointCloud<pcl::Normal> normals;
  pcl::Indices identity;
  pcl::Indices shuffled;
};

FixtureData
makeFixture(std::size_t n)
{
  FixtureData data;
  data.cloud = pcl_rvv_filters_covariance_sampling::makePointCloud(n);
  data.normals = pcl_rvv_filters_covariance_sampling::makeNormalCloud(n);
  data.identity = pcl_rvv_filters_covariance_sampling::makeIndices(n, false);
  data.shuffled = pcl_rvv_filters_covariance_sampling::makeIndices(n, true);
  return data;
}

} // namespace

TEST(CovarianceSamplingDiag, ScaledPointsRVVMatchesScalarConditionInput)
{
  const auto data = makeFixture(512);
  std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>> std_scaled;
  std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>> rvv_scaled;
  Eigen::Vector3f std_centroid;
  Eigen::Vector3f rvv_centroid;
  double std_norm = 0.0;
  double rvv_norm = 0.0;

  pcl_rvv_filters_covariance_sampling::computeScaledPointsStd(
      data.cloud, data.identity, std_scaled, std_centroid, std_norm);
  const bool rvv_used = pcl_rvv_filters_covariance_sampling::computeScaledPointsRVV(
      data.cloud, data.identity, rvv_scaled, rvv_centroid, rvv_norm);

#if defined(__RVV10__) && defined(PCL_COVARIANCE_SAMPLING_RVV_DIAGNOSTIC)
  EXPECT_TRUE(rvv_used);
#else
  EXPECT_FALSE(rvv_used);
  pcl_rvv_filters_covariance_sampling::computeScaledPointsStd(
      data.cloud, data.identity, rvv_scaled, rvv_centroid, rvv_norm);
#endif

  ASSERT_EQ(std_scaled.size(), rvv_scaled.size());
  EXPECT_NEAR(std_centroid.x(), rvv_centroid.x(), 2e-5f);
  EXPECT_NEAR(std_centroid.y(), rvv_centroid.y(), 2e-5f);
  EXPECT_NEAR(std_centroid.z(), rvv_centroid.z(), 2e-5f);
  EXPECT_NEAR(std_norm, rvv_norm, 2e-5);
}

TEST(CovarianceSamplingDiag, FullDiagnosticIdentityIndicesMatchesScalarWithinTolerance)
{
  const auto data = makeFixture(512);
  const auto std_result = pcl_rvv_filters_covariance_sampling::runDiagnostic(
      data.cloud, data.normals, data.identity, 64, false);
  const auto rvv_result = pcl_rvv_filters_covariance_sampling::runDiagnostic(
      data.cloud, data.normals, data.identity, 64, true);

  EXPECT_EQ(std_result.sampled_count, 64u);
  EXPECT_EQ(rvv_result.sampled_count, 64u);
  EXPECT_NEAR(std_result.condition_number, rvv_result.condition_number, 5e-5);
}

TEST(CovarianceSamplingDiag, FullDiagnosticShuffledIndicesMatchesScalarWithinTolerance)
{
  const auto data = makeFixture(512);
  const auto std_result = pcl_rvv_filters_covariance_sampling::runDiagnostic(
      data.cloud, data.normals, data.shuffled, 96, false);
  const auto rvv_result = pcl_rvv_filters_covariance_sampling::runDiagnostic(
      data.cloud, data.normals, data.shuffled, 96, true);

  EXPECT_EQ(std_result.sampled_count, 96u);
  EXPECT_EQ(rvv_result.sampled_count, 96u);
  EXPECT_NEAR(std_result.condition_number, rvv_result.condition_number, 5e-5);
}

TEST(CovarianceSamplingDiag, SmallInputFallsBackToScalarHelper)
{
  const auto data = makeFixture(16);
  std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>> scaled;
  Eigen::Vector3f centroid;
  double norm = 0.0;
  EXPECT_FALSE(pcl_rvv_filters_covariance_sampling::computeScaledPointsRVV(
      data.cloud, data.identity, scaled, centroid, norm));

  std::vector<pcl_rvv_filters_covariance_sampling::Vector6d,
              Eigen::aligned_allocator<pcl_rvv_filters_covariance_sampling::Vector6d>> vectors;
  pcl_rvv_filters_covariance_sampling::computeScaledPointsStd(
      data.cloud, data.identity, scaled, centroid, norm);
  EXPECT_FALSE(pcl_rvv_filters_covariance_sampling::buildVectorsRVV(
      scaled, data.normals, data.identity, vectors));
}
