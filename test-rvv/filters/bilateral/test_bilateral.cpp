#include "bilateral_diag.hpp"

#include <pcl/filters/bilateral.h>

#include <gtest/gtest.h>

#include <cmath>
#include <utility>

namespace {

TEST(BilateralDiagnostic, ScalarFormulaMatchesManualTwoNeighbors)
{
  auto cloud = pcl_rvv_filters_bilateral::makeCloud(4, 1, false);
  cloud[0].intensity = 10.0f;
  cloud[1].intensity = 14.0f;
  cloud[2].intensity = 18.0f;
  const pcl::Indices neighbors{1, 2};
  const std::vector<float> squared_distances{0.25f, 1.0f};
  const double sigma_s = 2.0;
  const double sigma_r = 8.0;

  const double w0 = std::exp(-(0.5 * 0.5) / (2.0 * sigma_s * sigma_s)) *
                    std::exp(-(4.0 * 4.0) / (2.0 * sigma_r * sigma_r));
  const double w1 = std::exp(-(1.0 * 1.0) / (2.0 * sigma_s * sigma_s)) *
                    std::exp(-(8.0 * 8.0) / (2.0 * sigma_r * sigma_r));
  const double expected = (w0 * 14.0 + w1 * 18.0) / (w0 + w1);

  EXPECT_NEAR(pcl_rvv_filters_bilateral::computePointWeightStd(
                  cloud, 0, neighbors, squared_distances, sigma_s, sigma_r),
              expected,
              1e-12);
}

TEST(BilateralDiagnostic, RVVStagingMatchesScalarWeight)
{
  auto cloud = pcl_rvv_filters_bilateral::makeCloud(64, 1, false);
  pcl::Indices neighbors;
  std::vector<float> squared_distances;
  for (int i = 0; i < 48; ++i) {
    neighbors.push_back(i);
    squared_distances.push_back(static_cast<float>(i + 1) * 0.01f);
  }
  const double std_weight = pcl_rvv_filters_bilateral::computePointWeightStd(
      cloud, 3, neighbors, squared_distances, 1.5, 12.0);
  const double rvv_weight = pcl_rvv_filters_bilateral::computePointWeightRVV(
      cloud, 3, neighbors, squared_distances, 1.5, 12.0);

  EXPECT_NEAR(rvv_weight, std_weight, 1e-5);
}

TEST(BilateralDiagnostic, RVVExpWeightMatchesScalarWithinApproximation)
{
  auto cloud = pcl_rvv_filters_bilateral::makeCloud(64, 1, false);
  pcl::Indices neighbors;
  std::vector<float> squared_distances;
  for (int i = 0; i < 48; ++i) {
    neighbors.push_back(i);
    squared_distances.push_back(static_cast<float>(i + 1) * 0.01f);
  }
  const double std_weight = pcl_rvv_filters_bilateral::computePointWeightStd(
      cloud, 3, neighbors, squared_distances, 1.5, 12.0);
  const double rvv_weight = pcl_rvv_filters_bilateral::computePointWeightExpRVV(
      cloud, 3, neighbors, squared_distances, 1.5, 12.0);

  EXPECT_NEAR(rvv_weight, std_weight, 2e-5);
}

TEST(BilateralDiagnostic, FullDiagnosticMatchesScalar)
{
  const auto cloud = pcl_rvv_filters_bilateral::makeCloud(48, 32, false);
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const auto std_out = pcl_rvv_filters_bilateral::filterStd(cloud, indices, 0.09, 18.0);
  const auto rvv_out = pcl_rvv_filters_bilateral::filterRVV(cloud, indices, 0.09, 18.0);

  ASSERT_EQ(std_out.size(), rvv_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i)
    EXPECT_NEAR(std_out[i].intensity, rvv_out[i].intensity, 2e-4f) << i;
}

TEST(BilateralDiagnostic, FullExpDiagnosticMatchesScalar)
{
  const auto cloud = pcl_rvv_filters_bilateral::makeCloud(48, 32, false);
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const auto std_out = pcl_rvv_filters_bilateral::filterStd(cloud, indices, 0.09, 18.0);
  const auto rvv_out = pcl_rvv_filters_bilateral::filterExpRVV(cloud, indices, 0.09, 18.0);
  const auto stats = pcl_rvv_filters_bilateral::compareCloudIntensity(std_out, rvv_out);

  ASSERT_EQ(std_out.size(), rvv_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i)
    EXPECT_NEAR(std_out[i].intensity, rvv_out[i].intensity, 3e-4f) << i;
  EXPECT_TRUE(pcl_rvv_filters_bilateral::errorWithinTolerance(stats, 3e-4f, 2e-5f, 8e-5))
      << "max_abs=" << stats.max_abs << " max_rel=" << stats.max_rel << " rmse=" << stats.rmse;
}

TEST(BilateralDiagnostic, FullExpErrorBudgetAcrossParameters)
{
  const auto cloud = pcl_rvv_filters_bilateral::makeHighContrastCloud(40, 32);
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const std::pair<double, double> params[] = {
      {0.06, 8.0},
      {0.09, 18.0},
      {0.14, 32.0},
  };

  for (const auto& param : params) {
    const auto std_out = pcl_rvv_filters_bilateral::filterStd(cloud, indices, param.first, param.second);
    const auto rvv_out = pcl_rvv_filters_bilateral::filterExpRVV(cloud, indices, param.first, param.second);
    const auto stats = pcl_rvv_filters_bilateral::compareCloudIntensity(std_out, rvv_out);
    EXPECT_TRUE(pcl_rvv_filters_bilateral::errorWithinTolerance(stats, 8e-4f, 5e-5f, 2e-4))
        << "sigma_s=" << param.first << " sigma_r=" << param.second
        << " max_abs=" << stats.max_abs << " max_rel=" << stats.max_rel
        << " rmse=" << stats.rmse << " p99_abs=" << stats.p99_abs;
  }
}

TEST(BilateralDiagnostic, ProductionFilterMatchesScalarWithinApproximation)
{
  const auto cloud = pcl_rvv_filters_bilateral::makeCloud(48, 32, false);
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const auto std_out = pcl_rvv_filters_bilateral::filterStd(cloud, indices, 0.09, 18.0);
  const auto production_out = pcl_rvv_filters_bilateral::filterProduction(cloud, 0.09, 18.0);

  const auto stats = pcl_rvv_filters_bilateral::compareCloudIntensity(std_out, production_out);
  EXPECT_TRUE(pcl_rvv_filters_bilateral::errorWithinTolerance(stats, 3e-4f, 2e-5f, 8e-5))
      << "max_abs=" << stats.max_abs << " max_rel=" << stats.max_rel << " rmse=" << stats.rmse;
}

TEST(BilateralDiagnostic, ProductionFilterErrorBudgetAcrossParameters)
{
  const auto cloud = pcl_rvv_filters_bilateral::makeHighContrastCloud(40, 32);
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const std::pair<double, double> params[] = {
      {0.06, 8.0},
      {0.09, 18.0},
      {0.14, 32.0},
  };

  for (const auto& param : params) {
    const auto std_out = pcl_rvv_filters_bilateral::filterStd(cloud, indices, param.first, param.second);
    const auto production_out =
        pcl_rvv_filters_bilateral::filterProduction(cloud, param.first, param.second);
    const auto stats = pcl_rvv_filters_bilateral::compareCloudIntensity(std_out, production_out);
    EXPECT_TRUE(pcl_rvv_filters_bilateral::errorWithinTolerance(stats, 8e-4f, 5e-5f, 2e-4))
        << "sigma_s=" << param.first << " sigma_r=" << param.second
        << " max_abs=" << stats.max_abs << " max_rel=" << stats.max_rel
        << " rmse=" << stats.rmse << " p99_abs=" << stats.p99_abs;
  }
}

TEST(BilateralDiagnostic, NonDenseInvalidCentersAreSkipped)
{
  const auto cloud = pcl_rvv_filters_bilateral::makeCloud(64, 24, true);
  const auto indices = pcl_rvv_filters_bilateral::makeIndices(cloud.size(), false);
  const auto std_out = pcl_rvv_filters_bilateral::filterStd(cloud, indices, 0.09, 18.0);
  const auto rvv_out = pcl_rvv_filters_bilateral::filterRVV(cloud, indices, 0.09, 18.0);

  ASSERT_EQ(std_out.size(), rvv_out.size());
  for (std::size_t i = 0; i < std_out.size(); ++i)
    EXPECT_NEAR(std_out[i].intensity, rvv_out[i].intensity, 2e-4f) << i;
}

TEST(BilateralDiagnostic, SmallNeighborListFallsBack)
{
  const auto cloud = pcl_rvv_filters_bilateral::makeCloud(8, 1, false);
  const pcl::Indices neighbors{0, 1, 2};
  const std::vector<float> squared_distances{0.0f, 0.1f, 0.2f};
  std::vector<pcl_rvv_filters_bilateral::NeighborFeature> features;
  EXPECT_FALSE(pcl_rvv_filters_bilateral::stageNeighborFeaturesRVV(
      cloud, 1, neighbors, squared_distances, features));
}

} // namespace
