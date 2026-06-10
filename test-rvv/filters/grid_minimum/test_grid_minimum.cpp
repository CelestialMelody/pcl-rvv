#include "grid_minimum_diag.hpp"

#include <pcl/filters/grid_minimum.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <limits>

namespace {

pcl::PointCloud<pcl::PointXYZ>
makeCloud(std::size_t n, bool with_invalid = false)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = !with_invalid;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) * 0.0031f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) * 0.0029f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) * 0.0027f;
  }
  if (with_invalid) {
    cloud[3].x = std::numeric_limits<float>::quiet_NaN();
    cloud[67].y = std::numeric_limits<float>::infinity();
    cloud[129].z = -std::numeric_limits<float>::infinity();
  }
  return cloud;
}

pcl::Indices
makeIndices(std::size_t n)
{
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    indices.push_back(static_cast<int>(i));
  return indices;
}

pcl::Indices
makeSubset(std::size_t n)
{
  pcl::Indices indices;
  indices.reserve(n / 2);
  for (std::size_t i = 1; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  return indices;
}

void
expectSameCells(const std::vector<pcl_rvv_filters_grid_minimum::GridCell>& actual,
                const std::vector<pcl_rvv_filters_grid_minimum::GridCell>& expected)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_EQ(actual[i], expected[i]) << "cell i=" << i;
}

} // namespace

TEST(GridMinimumDiag, ScalarFormulaCoversNegativeFloorAndGridId)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 4;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points = {pcl::PointXYZ(-0.01f, -0.50f, 0.30f),
                  pcl::PointXYZ(0.49f, 0.50f, 0.20f),
                  pcl::PointXYZ(1.01f, 1.49f, 0.10f),
                  pcl::PointXYZ(-1.01f, 1.01f, 0.40f)};
  const pcl::Indices indices = makeIndices(cloud.size());

  const auto cells =
      pcl_rvv_filters_grid_minimum::computeGridCellsStd(cloud, indices, 2.0f, -3, -1, 6);

  ASSERT_EQ(cells.size(), 4u);
  EXPECT_EQ(cells[0].ix, 2);
  EXPECT_EQ(cells[0].iy, 0);
  EXPECT_EQ(cells[0].idx, 2u);
  EXPECT_EQ(cells[1].ix, 3);
  EXPECT_EQ(cells[1].iy, 2);
  EXPECT_EQ(cells[1].idx, 15u);
  EXPECT_EQ(cells[3].ix, 0);
  EXPECT_EQ(cells[3].iy, 3);
  EXPECT_EQ(cells[3].idx, 18u);
}

TEST(GridMinimumDiag, RVVGridCellsMatchScalarFullIndices)
{
  const auto cloud = makeCloud(1024);
  const pcl::Indices indices = makeIndices(cloud.size());
  const float inverse_resolution = 8.0f;
  Eigen::Vector4f min_p;
  Eigen::Vector4f max_p;
  pcl_rvv_filters_grid_minimum::computeBoundsStd(cloud, indices, min_p, max_p);
  int min_b0 = 0;
  int min_b1 = 0;
  int div_x = 0;
  pcl_rvv_filters_grid_minimum::computeGridShape(
      min_p, max_p, inverse_resolution, min_b0, min_b1, div_x);
  const auto expected = pcl_rvv_filters_grid_minimum::computeGridCellsStd(
      cloud, indices, inverse_resolution, min_b0, min_b1, div_x);

#if defined(__RVV10__)
  std::vector<pcl_rvv_filters_grid_minimum::GridCell> actual;
  ASSERT_TRUE(pcl_rvv_filters_grid_minimum::computeGridCellsRVV(
      cloud, indices, inverse_resolution, min_b0, min_b1, div_x, actual));
  expectSameCells(actual, expected);
#else
  EXPECT_FALSE(expected.empty());
#endif
}

TEST(GridMinimumDiag, RVVGridCellsMatchScalarSubsetAndInvalidSkip)
{
  const auto cloud = makeCloud(2048, true);
  const pcl::Indices indices = makeSubset(cloud.size());
  const float inverse_resolution = 8.0f;
  Eigen::Vector4f min_p;
  Eigen::Vector4f max_p;
  pcl_rvv_filters_grid_minimum::computeBoundsStd(cloud, indices, min_p, max_p);
  int min_b0 = 0;
  int min_b1 = 0;
  int div_x = 0;
  pcl_rvv_filters_grid_minimum::computeGridShape(
      min_p, max_p, inverse_resolution, min_b0, min_b1, div_x);
  const auto expected = pcl_rvv_filters_grid_minimum::computeGridCellsStd(
      cloud, indices, inverse_resolution, min_b0, min_b1, div_x);

#if defined(__RVV10__)
  std::vector<pcl_rvv_filters_grid_minimum::GridCell> actual;
  ASSERT_TRUE(pcl_rvv_filters_grid_minimum::computeGridCellsRVV(
      cloud, indices, inverse_resolution, min_b0, min_b1, div_x, actual));
  expectSameCells(actual, expected);
#else
  EXPECT_LT(expected.size(), indices.size());
#endif
}

TEST(GridMinimumDiag, FullDiagnosticMatchesScalar)
{
  const auto cloud = makeCloud(4096, true);
  const pcl::Indices indices = makeIndices(cloud.size());
  const auto expected = pcl_rvv_filters_grid_minimum::gridMinimumPointXYZStd(cloud, indices, 8.0f);

#if defined(__RVV10__)
  pcl::Indices actual;
  ASSERT_TRUE(pcl_rvv_filters_grid_minimum::gridMinimumPointXYZRVV(
      cloud, indices, 8.0f, actual));
  EXPECT_EQ(actual, expected);
#else
  EXPECT_FALSE(expected.empty());
#endif
}

TEST(GridMinimumDiag, RVVMainThenFallbackOrderDoesNotPolluteRounding)
{
  const auto cloud = makeCloud(1024);
  const pcl::Indices indices = makeIndices(cloud.size());
  const auto fallback_expected =
      pcl_rvv_filters_grid_minimum::gridMinimumPointXYZStd(cloud, indices, 5.0f);

#if defined(__RVV10__)
  pcl::Indices rvv_out;
  ASSERT_TRUE(pcl_rvv_filters_grid_minimum::gridMinimumPointXYZRVV(
      cloud, indices, 8.0f, rvv_out));
  const auto fallback_actual =
      pcl_rvv_filters_grid_minimum::gridMinimumPointXYZStd(cloud, indices, 5.0f);
  EXPECT_EQ(fallback_actual, fallback_expected);

  std::vector<pcl_rvv_filters_grid_minimum::GridCell> small_cells;
  const pcl::Indices small_indices = makeIndices(16);
  EXPECT_FALSE(pcl_rvv_filters_grid_minimum::computeGridCellsRVV(
      cloud, small_indices, 8.0f, -1, -1, 8, small_cells));
#else
  EXPECT_FALSE(fallback_expected.empty());
#endif
}

TEST(GridMinimumDiag, ProductionGridMinimumStillRuns)
{
  auto cloud = makeCloud(2048, true).makeShared();
  pcl::GridMinimum<pcl::PointXYZ> filter(0.125f);
  filter.setInputCloud(cloud);

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);
  const pcl::Indices indices = makeIndices(cloud->size());
  const auto expected = pcl_rvv_filters_grid_minimum::gridMinimumPointXYZStd(*cloud, indices, 8.0f);

  ASSERT_EQ(output.size(), expected.size());
  EXPECT_EQ(output.height, 1u);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
