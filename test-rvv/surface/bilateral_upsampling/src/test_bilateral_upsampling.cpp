#include "bilateral_upsampling.h"

#include <gtest/gtest.h>
#include <pcl/point_types.h>
#include <pcl/surface/bilateral_upsampling.h>

#include <limits>
#include <type_traits>

namespace diag = pcl_rvv_surface_bilateral_upsampling;

namespace {

Eigen::Matrix3f
makeProductionProjection()
{
  Eigen::Matrix3f projection;
  projection << 500.0f, 0.0f, 320.0f,
                0.0f, 500.0f, 240.0f,
                0.0f, 0.0f, 1.0f;
  return projection;
}

template <typename PointT>
pcl::PointCloud<PointT>
makePclCloud(const std::vector<diag::RgbPoint>& src, int width, int height)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = false;
  cloud.resize(src.size());
  for (std::size_t i = 0; i < src.size(); ++i) {
    cloud[i].x = src[i].x;
    cloud[i].y = src[i].y;
    cloud[i].z = src[i].z;
    cloud[i].r = src[i].r;
    cloud[i].g = src[i].g;
    cloud[i].b = src[i].b;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
      cloud[i].a = src[i].a;
  }
  return cloud;
}

template <typename PointT>
std::vector<diag::RgbPoint>
fromPclCloud(const pcl::PointCloud<PointT>& cloud)
{
  std::vector<diag::RgbPoint> out(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    out[i].x = cloud[i].x;
    out[i].y = cloud[i].y;
    out[i].z = cloud[i].z;
    out[i].r = cloud[i].r;
    out[i].g = cloud[i].g;
    out[i].b = cloud[i].b;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
      out[i].a = cloud[i].a;
  }
  return out;
}

template <typename PointInT, typename PointOutT = PointInT>
diag::ErrorStats
runProductionPublicEntryOnCloud(std::vector<diag::RgbPoint> diag_cloud,
                                int width,
                                int height,
                                int window)
{
  const auto tables = diag::computeTables(window, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(diag_cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);

  auto input = makePclCloud<PointInT>(diag_cloud, width, height);
  auto input_ptr = input.makeShared();
  pcl::BilateralUpsampling<PointInT, PointOutT> upsampling;
  upsampling.setInputCloud(input_ptr);
  upsampling.setWindowSize(window);
  upsampling.setSigmaDepth(0.5f);
  upsampling.setSigmaColor(15.0f);
  upsampling.setProjectionMatrix(makeProductionProjection());
  pcl::PointCloud<PointOutT> actual_cloud;
  upsampling.process(actual_cloud);
  return diag::compareClouds(expected, fromPclCloud(actual_cloud));
}

template <typename PointInT, typename PointOutT = PointInT>
diag::ErrorStats
runProductionPublicEntry(int width, int height, int window, bool holes)
{
  return runProductionPublicEntryOnCloud<PointInT, PointOutT>(
      diag::makeCloud(width, height, holes), width, height, window);
}

} // namespace

TEST(BilateralUpsamplingDiagnostic, DepthAndColorTablesMatchFormula)
{
  const auto tables = diag::computeTables(3, 0.5f, 15.0f);
  EXPECT_EQ(tables.diameter, 7);
  EXPECT_NEAR(diag::depthWeight(tables, 0, 0), 1.0f, 1e-6f);
  EXPECT_NEAR(tables.rgb[0], 1.0f, 1e-6f);
  EXPECT_LT(diag::depthWeight(tables, 3, 0), diag::depthWeight(tables, 1, 0));
  EXPECT_LT(tables.rgb[90], tables.rgb[10]);
}

TEST(BilateralUpsamplingDiagnostic, ScalarProducesNanWhenWindowHasNoFiniteDepth)
{
  auto cloud = diag::makeCloud(5, 5, false);
  for (auto& p : cloud)
    p.z = std::numeric_limits<float>::quiet_NaN();

  const auto tables = diag::computeTables(2, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> out;
  diag::processScalar(cloud, 5, 5, tables, diag::makeSimpleUnprojection(), out);

  EXPECT_TRUE(std::isnan(out[12].z));
  EXPECT_EQ(out[12].r, cloud[12].r);
  EXPECT_EQ(out[12].g, cloud[12].g);
  EXPECT_EQ(out[12].b, cloud[12].b);
}

TEST(BilateralUpsamplingDiagnostic, CandidateMatchesScalarOnDenseCloud)
{
  const int width = 32;
  const int height = 24;
  const auto cloud = diag::makeCloud(width, height, false);
  const auto tables = diag::computeTables(3, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  std::vector<diag::RgbPoint> actual;

  diag::processScalar(cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  diag::processCandidate(cloud, width, height, tables, diag::makeSimpleUnprojection(), actual);
  const auto stats = diag::compareClouds(expected, actual);

  EXPECT_TRUE(diag::errorWithinTolerance(stats, 4e-6f, 1e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz;
}

TEST(BilateralUpsamplingDiagnostic, CandidateMatchesScalarWithNanHoles)
{
  const int width = 40;
  const int height = 28;
  const auto cloud = diag::makeCloud(width, height, true);
  const auto tables = diag::computeTables(4, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  std::vector<diag::RgbPoint> actual;

  diag::processScalar(cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  diag::processCandidate(cloud, width, height, tables, diag::makeSimpleUnprojection(), actual);
  const auto stats = diag::compareClouds(expected, actual);

  EXPECT_TRUE(diag::errorWithinTolerance(stats, 6e-6f, 2e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz
      << " nan_points=" << stats.nan_points;
}

TEST(BilateralUpsamplingDiagnostic, DirectDepthCandidateMatchesScalarWithNanHoles)
{
  const int width = 40;
  const int height = 28;
  const auto cloud = diag::makeCloud(width, height, true);
  const auto tables = diag::computeTables(4, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  std::vector<diag::RgbPoint> actual;

  diag::processScalar(cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  diag::processDirectDepthCandidate(cloud, width, height, tables, diag::makeSimpleUnprojection(), actual);
  const auto stats = diag::compareClouds(expected, actual);

  EXPECT_TRUE(diag::errorWithinTolerance(stats, 6e-6f, 2e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz;
}

TEST(BilateralUpsamplingDiagnostic, DirectDepthCandidateSkipsInfiniteDepth)
{
  const int width = 40;
  const int height = 28;
  auto cloud = diag::makeCloud(width, height, false);
  cloud[3 * width + 5].z = std::numeric_limits<float>::infinity();
  cloud[9 * width + 12].z = -std::numeric_limits<float>::infinity();
  cloud[17 * width + 31].z = std::numeric_limits<float>::infinity();
  const auto tables = diag::computeTables(4, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  std::vector<diag::RgbPoint> actual;

  diag::processScalar(cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  diag::processDirectDepthCandidate(cloud, width, height, tables, diag::makeSimpleUnprojection(), actual);
  const auto stats = diag::compareClouds(expected, actual);

  EXPECT_TRUE(diag::errorWithinTolerance(stats, 6e-6f, 2e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz
      << " finite_points=" << stats.finite_points << " nan_points=" << stats.nan_points;
}

TEST(BilateralUpsamplingDiagnostic, WindowBoundaryKeepsProductionLoopShape)
{
  const int width = 8;
  const int height = 7;
  const auto cloud = diag::makeCloud(width, height, false);
  const auto tables = diag::computeTables(3, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  std::vector<diag::RgbPoint> actual;

  diag::processScalar(cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  diag::processCandidate(cloud, width, height, tables, diag::makeSimpleUnprojection(), actual);

  const std::size_t corners[] = {0, 7, 48, 55};
  for (const auto i : corners) {
    EXPECT_NEAR(expected[i].x, actual[i].x, 6e-6f) << i;
    EXPECT_NEAR(expected[i].y, actual[i].y, 6e-6f) << i;
    EXPECT_NEAR(expected[i].z, actual[i].z, 6e-6f) << i;
  }
}

TEST(BilateralUpsamplingDiagnostic, DirectDepthWindowBoundaryKeepsProductionLoopShape)
{
  const int width = 8;
  const int height = 7;
  const auto cloud = diag::makeCloud(width, height, false);
  const auto tables = diag::computeTables(3, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  std::vector<diag::RgbPoint> actual;

  diag::processScalar(cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  diag::processDirectDepthCandidate(cloud, width, height, tables, diag::makeSimpleUnprojection(), actual);

  const std::size_t corners[] = {0, 7, 48, 55};
  for (const auto i : corners) {
    EXPECT_NEAR(expected[i].x, actual[i].x, 6e-6f) << i;
    EXPECT_NEAR(expected[i].y, actual[i].y, 6e-6f) << i;
    EXPECT_NEAR(expected[i].z, actual[i].z, 6e-6f) << i;
  }
}


TEST(BilateralUpsamplingProductionPublic, PointXYZRGBMatchesReferenceWithNanHoles)
{
  const auto stats = runProductionPublicEntry<pcl::PointXYZRGB>(40, 28, 4, true);
  EXPECT_TRUE(diag::errorWithinTolerance(stats, 8e-6f, 3e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz
      << " nan_points=" << stats.nan_points;
}

TEST(BilateralUpsamplingProductionPublic, PointXYZRGBAMatchesReferenceDense)
{
  const auto stats = runProductionPublicEntry<pcl::PointXYZRGBA>(32, 24, 3, false);
  EXPECT_TRUE(diag::errorWithinTolerance(stats, 8e-6f, 3e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz;
}

TEST(BilateralUpsamplingProductionPublic, PointXYZRGBSkipsInfiniteDepth)
{
  const int width = 40;
  const int height = 28;
  auto cloud = diag::makeCloud(width, height, false);
  cloud[2 * width + 3].z = std::numeric_limits<float>::infinity();
  cloud[11 * width + 20].z = -std::numeric_limits<float>::infinity();
  cloud[24 * width + 7].z = std::numeric_limits<float>::infinity();

  const auto stats =
      runProductionPublicEntryOnCloud<pcl::PointXYZRGB>(cloud, width, height, 4);
  EXPECT_TRUE(diag::errorWithinTolerance(stats, 8e-6f, 3e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz
      << " finite_points=" << stats.finite_points << " nan_points=" << stats.nan_points;
}

TEST(BilateralUpsamplingProductionPublic, PointXYZRGBToPointXYZRGBAMatchesReferenceWithNanHoles)
{
  const auto stats =
      runProductionPublicEntry<pcl::PointXYZRGB, pcl::PointXYZRGBA>(40, 28, 4, true);
  EXPECT_TRUE(diag::errorWithinTolerance(stats, 8e-6f, 3e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz
      << " nan_points=" << stats.nan_points;
}

TEST(BilateralUpsamplingProductionPublic, PointXYZRGBAToPointXYZRGBMatchesReferenceDense)
{
  const auto stats =
      runProductionPublicEntry<pcl::PointXYZRGBA, pcl::PointXYZRGB>(32, 24, 3, false);
  EXPECT_TRUE(diag::errorWithinTolerance(stats, 8e-6f, 3e-6f))
      << "max_abs_xyz=" << stats.max_abs_xyz << " rmse_xyz=" << stats.rmse_xyz;
}
