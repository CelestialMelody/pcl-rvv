/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 RBF component diagnostic（组件诊断）
 * 的 RVV helper 与标量参考链路在矩阵填充和体素求值上保持数值一致。
 *
 * 证据边界：
 * 测试没有调用真实 MarchingCubesRBF production dispatch（生产分流），也不证明
 * production 源码已经接入 RVV。它只证明 test-rvv candidate helper 在同一输入
 * 和同一 Eigen solve 边界下可作为后续 board bench（板卡性能测试）的对拍对象。
 */

#include "marching_cubes_rbf.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace mcrbf = pcl::surface::rvv_marching_cubes_rbf_support;

namespace {

mcrbf::RbfInput
makeProductionRoundedInput(const int points)
{
  mcrbf::RbfInput input = mcrbf::makeInput(points);
  for (std::size_t i = 0; i < input.x.size(); ++i) {
    input.x[i] = static_cast<double>(static_cast<float>(input.x[i]));
    input.y[i] = static_cast<double>(static_cast<float>(input.y[i]));
    input.z[i] = static_cast<double>(static_cast<float>(input.z[i]));
    input.nx[i] = static_cast<double>(static_cast<float>(input.nx[i]));
    input.ny[i] = static_cast<double>(static_cast<float>(input.ny[i]));
    input.nz[i] = static_cast<double>(static_cast<float>(input.nz[i]));
  }
  return input;
}

std::vector<double>
makeProductionReferenceGrid(const int points, const int resolution)
{
  const auto input = makeProductionRoundedInput(points);
  const auto centers = mcrbf::makeCenters(input);
  const auto rhs = mcrbf::makeRhs(centers.x.size(), input.off_surface_epsilon);
  mcrbf::Matrix matrix;
  mcrbf::fillMatrixSourceOrderScalar(centers, matrix);
  const mcrbf::Vector weights = matrix.fullPivLu().solve(rhs);
  return mcrbf::evaluateGridScalar(centers, weights, mcrbf::makeProductionGridSpec(resolution));
}

} // namespace

TEST(MarchingCubesRBFDiagnostic, MatrixFillMatchesScalarReference)
{
  const auto input = mcrbf::makeInput(18);
  const auto centers = mcrbf::makeCenters(input);
  mcrbf::Matrix reference;
  mcrbf::Matrix candidate;

  mcrbf::fillMatrixSourceOrderScalar(centers, reference);
  mcrbf::fillMatrixCandidate(centers, candidate);

  EXPECT_EQ(reference.rows(), candidate.rows());
  EXPECT_EQ(reference.cols(), candidate.cols());
  EXPECT_LT(mcrbf::maxAbsDiff(reference, candidate), 1.0e-10);
}

TEST(MarchingCubesRBFDiagnostic, VoxelEvaluationMatchesScalarReference)
{
  const auto input = mcrbf::makeInput(16);
  const auto centers = mcrbf::makeCenters(input);
  const auto rhs = mcrbf::makeRhs(centers.x.size(), input.off_surface_epsilon);

  mcrbf::Matrix matrix;
  mcrbf::fillMatrixSourceOrderScalar(centers, matrix);
  const mcrbf::Vector weights = matrix.fullPivLu().solve(rhs);

  mcrbf::GridSpec spec;
  spec.res_x = 12;
  spec.res_y = 12;
  spec.res_z = 12;
  spec.size_voxel = Eigen::Array3d(2.5 / 12.0, 2.5 / 12.0, 2.5 / 12.0);

  const auto reference = mcrbf::evaluateGridScalar(centers, weights, spec);
  const auto candidate = mcrbf::evaluateGridCandidate(centers, weights, spec);

  ASSERT_EQ(reference.size(), candidate.size());
  EXPECT_LT(mcrbf::maxAbsDiff(reference, candidate), 1.0e-7);
  EXPECT_EQ(mcrbf::countActiveCells(reference, spec), mcrbf::countActiveCells(candidate, spec));
}

TEST(MarchingCubesRBFDiagnostic, FullPipelineProducesStableChecksum)
{
  const auto first = mcrbf::runFullCandidatePipeline(18, 14);
  const auto second = mcrbf::runFullCandidatePipeline(18, 14);

  EXPECT_EQ(first.grid_values, second.grid_values);
  EXPECT_EQ(first.active_cells, second.active_cells);
  EXPECT_EQ(first.checksum, second.checksum);
}

TEST(MarchingCubesRBFProductionDirect, PointNormalVoxelizeMatchesScalarReference)
{
  const int points = 24;
  const int resolution = 14;
  const auto reference = makeProductionReferenceGrid(points, resolution);
  const auto production_grid =
      mcrbf::toDoubleGrid(mcrbf::runProductionVoxelizeGrid<pcl::PointNormal>(points, resolution));
  const auto spec = mcrbf::makeProductionGridSpec(resolution);

  ASSERT_EQ(reference.size(), production_grid.size());
  EXPECT_LT(mcrbf::maxAbsDiff(reference, production_grid), 1.0e-4);
  EXPECT_EQ(mcrbf::countActiveCells(reference, spec), mcrbf::countActiveCells(production_grid, spec));
}

TEST(MarchingCubesRBFProductionDirect, SmallPointNormalInputUsesStableFallback)
{
  const int points = 8;
  const int resolution = 10;
  const auto reference = makeProductionReferenceGrid(points, resolution);
  const auto production_grid =
      mcrbf::toDoubleGrid(mcrbf::runProductionVoxelizeGrid<pcl::PointNormal>(points, resolution));
  const auto spec = mcrbf::makeProductionGridSpec(resolution);

  ASSERT_EQ(reference.size(), production_grid.size());
  EXPECT_LT(mcrbf::maxAbsDiff(reference, production_grid), 1.0e-4);
  EXPECT_EQ(mcrbf::countActiveCells(reference, spec), mcrbf::countActiveCells(production_grid, spec));
}

TEST(MarchingCubesRBFProductionDirect, PointXYZINormalVoxelizeMatchesScalarReference)
{
  const int points = 24;
  const int resolution = 12;
  const auto reference = makeProductionReferenceGrid(points, resolution);
  const auto production_grid =
      mcrbf::toDoubleGrid(mcrbf::runProductionVoxelizeGrid<pcl::PointXYZINormal>(points, resolution));
  const auto spec = mcrbf::makeProductionGridSpec(resolution);

  ASSERT_EQ(reference.size(), production_grid.size());
  EXPECT_LT(mcrbf::maxAbsDiff(reference, production_grid), 1.0e-4);
  EXPECT_EQ(mcrbf::countActiveCells(reference, spec), mcrbf::countActiveCells(production_grid, spec));
}

TEST(MarchingCubesRBFProductionDirect, PointXYZRGBNormalVoxelizeMatchesScalarReference)
{
  const int points = 24;
  const int resolution = 12;
  const auto reference = makeProductionReferenceGrid(points, resolution);
  const auto production_grid =
      mcrbf::toDoubleGrid(mcrbf::runProductionVoxelizeGrid<pcl::PointXYZRGBNormal>(points, resolution));
  const auto spec = mcrbf::makeProductionGridSpec(resolution);

  ASSERT_EQ(reference.size(), production_grid.size());
  EXPECT_LT(mcrbf::maxAbsDiff(reference, production_grid), 1.0e-4);
  EXPECT_EQ(mcrbf::countActiveCells(reference, spec), mcrbf::countActiveCells(production_grid, spec));
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
