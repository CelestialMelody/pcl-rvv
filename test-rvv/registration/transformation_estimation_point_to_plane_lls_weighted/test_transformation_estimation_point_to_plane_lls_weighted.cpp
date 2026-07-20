/*
 * 本文件做什么：
 * 这些专项测试覆盖 weighted point-to-plane LLS（带权重点到平面线性最小二乘）
 * 的 scalar reference（标量参考链路）和 RVV candidate（RVV 候选链路）。
 * main() 由 gtest 提供；Makefile 会分别构建 std 和 RVV 二进制。
 *
 * 阅读提示：
 * - 公开全云入口（full-cloud，source/target 按相同下标一一对应）需要先调用
 *   setCorrespondenceWeights（设置对应关系权重）。
 * - 公开对应关系入口（correspondences，由 index_query/index_match 指定点对）直接读取
 *   correspondence.weight 字段。
 * - production 源码把不同入口统一成 ConstCloudIterator（常量点云迭代器）逐点流；RVV
 *   诊断为了暴露取数成本，显式拆成全云顺序扫描和对应关系索引扫描。
 * - RVV candidate（RVV 候选链路）的全云路径使用 stride load（跨步加载）和连续
 *   weight load（权重加载）；对应关系路径先展开 index/weight，再用 gather（离散加载）。
 * - small fallback（小规模回退路径）和 invalid lane（无效向量通道）case 分别隔离规模
 *   gate（会导致回退的验收条件）和 finite mask（有限值掩码）。
 */

#include "transformation_estimation_point_to_plane_lls_weighted_diag.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace diag = pcl::registration::rvv_te_pt2plane_lls_weighted_diag;

namespace {

pcl::PointCloud<pcl::PointNormal>
makeSurfaceCloud(const int grid_radius, const float step)
{
  // 生成非平面的解析曲面；法线来自解析偏导，避免 6x6 normal-equation 退化。
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
      point.z = 0.11f * x * x + 0.17f * x * y - 0.24f * y + 0.9f;
      point.normal_x = -0.22f * x - 0.17f * y;
      point.normal_y = -0.17f * x + 0.24f;
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
  // 温和刚体变换让测试聚焦 normal-equation 构造，而不是大角度线性化误差。
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
  // 使用 PCL 公共 transform helper 保持真实 PointNormal 内存布局和 normal 字段。
  pcl::PointCloud<pcl::PointNormal> target;
  pcl::transformPointCloudWithNormals(source, target, makeTransform());
  return target;
}

std::vector<float>
makeWeights(const std::size_t n)
{
  // 权重是确定性周期序列，覆盖小于、等于和大于 1 的 normal 缩放，不依赖随机数。
  std::vector<float> weights;
  weights.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    weights.push_back(0.55f + 0.07f * static_cast<float>(i % 9));
  return weights;
}

pcl::Correspondences
makeWeightedCorrespondences(const std::size_t n)
{
  // 乱序、重复和不同 weight 值共同覆盖对应关系索引读取路径
  //（correspondences gather path），也说明它和全云顺序扫描不是同一种数据流。
  pcl::Correspondences correspondences;
  correspondences.reserve(n + n / 13);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.60f + 0.03f * (i % 7));
  for (std::size_t i = 1; i < n; i += 5)
    correspondences.emplace_back(
        static_cast<int>(i), static_cast<int>(i), 0.75f + 0.02f * (i % 5));
  for (std::size_t i = 11; i < n; i += 41)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.15f);
  return correspondences;
}

void
expectMatrixNear(const Eigen::Matrix4f& actual,
                 const Eigen::Matrix4f& expected,
                 const float tolerance)
{
  // 逐元素报错能直接指出旋转或平移项偏离。
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(actual(row, col), expected(row, col), tolerance)
          << "row=" << row << " col=" << col;
}

} // namespace

// 这个测试验证 test-rvv 标量诊断是否复刻公开全云带权估计入口
//（full-cloud weighted estimator）。
// 如果失败，后续 RVV candidate 就没有可信的 scalar reference（标量参考链路）。
TEST(TransformationEstimationPointToPlaneLLSWeighted, StdDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(14, 0.22f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_full(source, target, weights, &stats);

  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

// 这个测试验证公开对应关系入口（correspondences）直接使用 correspondence.weight 的语义。
// 它证明权重来源和全云 setCorrespondenceWeights 路径不同，不能混为一个入口。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     StdCorrespondencesMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeWeightedCorrespondences(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_correspondences(source, target, correspondences, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

// 这个测试验证连续 PointNormal 全云输入下，RVV 候选链路会命中带权公式暂存阶段
//（weighted staging）并与标量一致。如果 RVV 构建没有命中或矩阵超差，说明跨步加载、
// 权重加载或公式 gate 断了。
TEST(TransformationEstimationPointToPlaneLLSWeighted, FullCloudCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_full(source, target, weights, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_full(source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 3e-4f);
}

// 这个测试验证对应关系路径（correspondences）的乱序、重复 index 和 weight 字段。
// 如果失败，说明 gather（离散加载）、权重展开或 vcompress 后保序尾段的证据断了。
TEST(TransformationEstimationPointToPlaneLLSWeighted, CorrespondenceCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Correspondences correspondences = makeWeightedCorrespondences(source.size());

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
  expectMatrixNear(candidate_matrix, std_matrix, 4e-4f);
}

// 这个测试单独隔离小规模 fallback（回退路径）gate：输入太小时必须回到标量链路。
// 如果失败，说明规模阈值或 fallback normal-equation 等价性断了。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     SmallInputFallsBackForIsolatedSizeGate)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  diag::AccumulationStats stats;
  const diag::NormalEquation candidate =
      diag::accumulate_candidate_full(source, target, weights, &stats);
  const diag::NormalEquation reference = diag::accumulate_std_full(source, target, weights);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(candidate.accepted_points, reference.accepted_points);
  EXPECT_NEAR((candidate.ata - reference.ata).norm(), 0.0, 1e-12);
  EXPECT_NEAR((candidate.atb - reference.atb).norm(), 0.0, 1e-12);
}

// 这个测试验证 NaN/Inf invalid lane（无效 lane）会被 finite mask（有限值掩码）剔除。
// weight 本身不参与当前 production finite check，所以本 case 只放有限权重。
TEST(TransformationEstimationPointToPlaneLLSWeighted, InvalidLaneMaskMatchesStd)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  target[17].y = std::numeric_limits<float>::quiet_NaN();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation candidate_eq =
      diag::accumulate_candidate_full(source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
}
