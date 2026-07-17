/*
 * 本文件做什么：
 * 这些专项测试覆盖 point-to-plane LLS normal-equation（法方程）构造的
 * scalar reference（标量参考链路）和 RVV candidate（RVV 候选链路）。
 * main() 由 gtest 提供；Makefile 会分别构建 std 和 RVV 二进制。
 *
 * 阅读提示：
 * - 大规模全云 case 证明连续 PointNormal 输入可以命中 RVV staging（暂存阶段）。
 * - correspondences（对应关系）case 证明 gather（按索引读取）路径能处理乱序和重复匹配。
 * - small fallback（小规模回退路径）和 invalid lane（无效 lane）case 分别隔离规模 gate
 *   和 finite mask（有限值掩码），避免用一个混合 case 误代表所有 fallback。
 */

#include "transformation_estimation_point_to_plane_lls_diag.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls.h>

#include <gtest/gtest.h>

#include <limits>

namespace diag = pcl::registration::rvv_te_pt2plane_lls_diag;

namespace {

pcl::PointCloud<pcl::PointNormal>
makeSurfaceCloud(const int grid_radius, const float step)
{
  // 生成上游 registration API 测试同类的二次曲面；法线来自解析偏导。
  // 这个曲面避免完全共面退化，让 6x6 normal-equation 更稳定。
  pcl::PointCloud<pcl::PointNormal> cloud;
  cloud.height = 1;
  cloud.is_dense = true;
  for (int ix = -grid_radius; ix <= grid_radius; ++ix) {
    for (int iy = -grid_radius; iy <= grid_radius; ++iy) {
      const float x = static_cast<float>(ix) * step;
      const float y = static_cast<float>(iy) * step;
      pcl::PointNormal point;
      point.x = x;
      point.y = y;
      point.z = 0.1f * x * x + 0.2f * x * y - 0.3f * y + 1.0f;
      point.normal_x = -0.2f * x - 0.2f * y;
      point.normal_y = -0.2f * x + 0.3f;
      point.normal_z = 1.0f;
      const float norm = std::sqrt(point.normal_x * point.normal_x +
                                   point.normal_y * point.normal_y +
                                   point.normal_z * point.normal_z);
      point.normal_x /= norm;
      point.normal_y /= norm;
      point.normal_z /= norm;
      cloud.push_back(point);
    }
  }
  cloud.width = cloud.size();
  return cloud;
}

Eigen::Matrix4f
makeTransform()
{
  // 这个矩阵沿用上游 registration API 测试的温和刚体变换，便于和公开 estimator 对齐。
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
  transform.row(0) << 0.9938f, 0.0988f, 0.0517f, 0.1000f;
  transform.row(1) << -0.0997f, 0.9949f, 0.0149f, -0.2000f;
  transform.row(2) << -0.0500f, -0.0200f, 0.9986f, 0.3000f;
  transform.row(3) << 0.0000f, 0.0000f, 0.0000f, 1.0000f;
  return transform;
}

pcl::PointCloud<pcl::PointNormal>
makeTargetCloud(const pcl::PointCloud<pcl::PointNormal>& source)
{
  // target 由 PCL 公共 transform helper 生成，让测试输入保持真实 PointNormal 布局。
  pcl::PointCloud<pcl::PointNormal> target;
  pcl::transformPointCloudWithNormals(source, target, makeTransform());
  return target;
}

pcl::Correspondences
makeShuffledCorrespondences(const std::size_t n)
{
  // 乱序和重复 index 是 correspondences gather path（对应关系索引读取路径）的语义重点。
  // 这里仍保持 source/target 成对来自同一个变换，只改变扫描顺序并重复少量样本。
  pcl::Correspondences correspondences;
  correspondences.reserve(n + n / 11);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  for (std::size_t i = 1; i < n; i += 4)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  for (std::size_t i = 7; i < n; i += 37)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  return correspondences;
}

void
expectMatrixNear(const Eigen::Matrix4f& actual,
                 const Eigen::Matrix4f& expected,
                 const float tolerance)
{
  // 测试矩阵逐元素报错，失败时能直接定位是旋转还是平移项偏离。
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(actual(row, col), expected(row, col), tolerance)
          << "row=" << row << " col=" << col;
}

} // namespace

// 这个测试验证 test-rvv 的标量诊断入口是否复刻当前公开 estimator 的输出。
// 如果失败，说明 diagnostic reference（诊断参考链路）已经不能作为后续 RVV 对拍基准。
TEST(TransformationEstimationPointToPlaneLLS, StdDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(14, 0.22f);
  const auto target = makeTargetCloud(source);

  pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                             pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_full(source, target, &stats);

  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

// 这个测试验证连续 PointNormal 全云输入下，RVV candidate（RVV 候选链路）与标量参考链路保持一致。
// 如果 RVV 构建没有命中 staging 或矩阵超差，说明 full-cloud 主路径证据或数值 gate 断了。
TEST(TransformationEstimationPointToPlaneLLS, FullCloudCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = diag::estimate_std_full(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_full(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 2e-4f);
}

// 这个测试验证 correspondences（对应关系）路径中的乱序、重复索引 gather（索引读取）不会改变求解结果。
// 如果失败，说明非连续访问语义、保序 staging 或对应关系路径的证据断了。
TEST(TransformationEstimationPointToPlaneLLS, CorrespondenceCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeShuffledCorrespondences(source.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_correspondences(source, target, correspondences, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_correspondences(
          source, target, correspondences, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试单独隔离小规模 fallback（回退路径）gate：输入太小时必须回到标量链路。
// 如果失败，说明规模阈值 gate 或 fallback 的 normal-equation 等价性断了。
TEST(TransformationEstimationPointToPlaneLLS, SmallInputFallsBackForIsolatedSizeGate)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);

  diag::AccumulationStats stats;
  const diag::NormalEquation candidate =
      diag::accumulate_candidate_full(source, target, &stats);
  const diag::NormalEquation reference = diag::accumulate_std_full(source, target);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(candidate.accepted_points, reference.accepted_points);
  EXPECT_NEAR((candidate.ata - reference.ata).norm(), 0.0, 1e-12);
  EXPECT_NEAR((candidate.atb - reference.atb).norm(), 0.0, 1e-12);
}

// 这个测试验证 NaN/Inf invalid lane（无效 lane）会被 finite mask（有限值掩码）剔除，并保持标量可见语义。
// 如果失败，说明 mask、vcompress（保序压缩）或压缩后标量 tail 的证据断了。
TEST(TransformationEstimationPointToPlaneLLS, InvalidLaneMaskMatchesStd)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, &std_stats);
  const diag::NormalEquation candidate_eq =
      diag::accumulate_candidate_full(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
}
