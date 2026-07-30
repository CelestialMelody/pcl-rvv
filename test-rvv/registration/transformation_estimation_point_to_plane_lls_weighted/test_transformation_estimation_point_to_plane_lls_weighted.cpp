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
 *   weight load（权重加载）；source-indexed（源索引路径）和 dual-indices（双索引路径）
 *   用独立测试拆分单侧 / 双侧 gather（离散加载）；对应关系路径先展开 index/weight，
 *   再复用双侧 gather 形态。
 * - small fallback（小规模回退路径）和 invalid lane（无效向量通道）case 分别隔离规模
 *   gate（会导致回退的验收条件）和 finite mask（有限值掩码）。
 */

#include "test_support_teptplw.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

struct TEPTPLWDoubleNormalTarget {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  union {
    struct {
      double normal_x;
      double normal_y;
      double normal_z;
    };
    double normal[3];
  };
};

POINT_CLOUD_REGISTER_POINT_STRUCT(TEPTPLWDoubleNormalTarget,
                                  (float, x, x)
                                  (float, y, y)
                                  (float, z, z)
                                  (double, normal_x, normal_x)
                                  (double, normal_y, normal_y)
                                  (double, normal_z, normal_z))

namespace diag = pcl::registration::rvv_te_pt2plane_lls_weighted_diag;
namespace prod_detail = pcl::registration::detail;

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

pcl::PointCloud<pcl::PointXYZ>
copySourceAsXYZ(const pcl::PointCloud<pcl::PointNormal>& source)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.reserve(source.size());
  cloud.height = source.height;
  cloud.is_dense = source.is_dense;
  for (const auto& point : source) {
    pcl::PointXYZ copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
}

pcl::PointCloud<pcl::PointXYZINormal>
copyTargetAsXYZINormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  // PointXYZINormal 覆盖 target 只要求 xyz+normal f32 AoS，不要求 exact PointNormal。
  pcl::PointCloud<pcl::PointXYZINormal> cloud;
  cloud.reserve(target.size());
  cloud.height = target.height;
  cloud.is_dense = target.is_dense;
  for (const auto& point : target) {
    pcl::PointXYZINormal copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    copied.normal_x = point.normal_x;
    copied.normal_y = point.normal_y;
    copied.normal_z = point.normal_z;
    copied.intensity = 0.5f;
    copied.curvature = 0.0f;
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
}

pcl::PointCloud<TEPTPLWDoubleNormalTarget>
copyTargetAsDoubleNormal(const pcl::PointCloud<pcl::PointNormal>& target)
{
  // 这个点型已注册 PCL fields，但 normal 是 double；f32 normal layout gate 应回退。
  pcl::PointCloud<TEPTPLWDoubleNormalTarget> cloud;
  cloud.reserve(target.size());
  cloud.height = target.height;
  cloud.is_dense = target.is_dense;
  for (const auto& point : target) {
    TEPTPLWDoubleNormalTarget copied;
    copied.x = point.x;
    copied.y = point.y;
    copied.z = point.z;
    copied.normal_x = point.normal_x;
    copied.normal_y = point.normal_y;
    copied.normal_z = point.normal_z;
    copied.normal[0] = static_cast<double>(point.normal_x);
    copied.normal[1] = static_cast<double>(point.normal_y);
    copied.normal[2] = static_cast<double>(point.normal_z);
    cloud.push_back(copied);
  }
  cloud.width = cloud.size();
  return cloud;
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

pcl::Indices
makeSourceIndices(const std::size_t n)
{
  // 只生成有效索引，覆盖乱序和重复 row。valid-index-only（只覆盖有效索引）是本轮
  // indexed diagnostic 合同；非法 index 不被写成 production 语义。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    indices.push_back(static_cast<int>((i * 37 + 11) % n));
  return indices;
}

pcl::Indices
makeTargetIndices(const std::size_t n)
{
  // target 侧使用另一条 index stream，避免 dual-indices 退化成 source-indexed 的变体。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; ++i)
    indices.push_back(static_cast<int>((i * 19 + 5) % n));
  return indices;
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

void
expectNormalEquationWithinBudget(const diag::NormalEquation& actual,
                                 const diag::NormalEquation& expected,
                                 const double ata_abs_tolerance,
                                 const double ata_rel_tolerance,
                                 const double atb_abs_tolerance,
                                 const double atb_rel_tolerance)
{
  // block-reduction（分块规约）会改变跨 lane 累加树，所以这里同时给绝对预算和
  // 相对预算。绝对预算保护 near-cancellation（近抵消）样本，相对预算保护 scale
  // stress（尺度压力）样本，二者都检查 accepted_points 与 ATA/ATb。
  const double ata_norm = std::max(expected.ata.norm(), 1.0);
  const double atb_norm = std::max(expected.atb.norm(), 1.0);
  EXPECT_EQ(actual.accepted_points, expected.accepted_points);
  EXPECT_LE((actual.ata - expected.ata).norm(),
            ata_abs_tolerance + ata_rel_tolerance * ata_norm);
  EXPECT_LE((actual.atb - expected.atb).norm(),
            atb_abs_tolerance + atb_rel_tolerance * atb_norm);
}

void
expectProductionEquationWithinBudget(
    const prod_detail::PointToPlaneLLSWeightedNormalEquation& actual,
    const prod_detail::PointToPlaneLLSWeightedNormalEquation& expected,
    const double ata_abs_tolerance,
    const double ata_rel_tolerance,
    const double atb_abs_tolerance,
    const double atb_rel_tolerance)
{
  // production block-reduction 也通过 float lane partial sums 再落到 double
  // normal-equation，因此这里复用 PI1 的 accepted_points、ATA/ATb 双预算。
  const double ata_norm = std::max(expected.ata.norm(), 1.0);
  const double atb_norm = std::max(expected.atb.norm(), 1.0);
  EXPECT_EQ(actual.accepted_points, expected.accepted_points);
  EXPECT_LE((actual.ata - expected.ata).norm(),
            ata_abs_tolerance + ata_rel_tolerance * ata_norm);
  EXPECT_LE((actual.atb - expected.atb).norm(),
            atb_abs_tolerance + atb_rel_tolerance * atb_norm);
}

Eigen::Matrix4f
solveProductionEquation(prod_detail::PointToPlaneLLSWeightedNormalEquation eq)
{
  Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
  prod_detail::solvePointToPlaneLLSWeightedNormalEquation(eq, matrix);
  return matrix;
}

} // namespace

// 这个测试验证 test-rvv 标量诊断是否复刻公开全云带权估计入口的 fallback 标量语义。
// 大规模 full-cloud public overload 在 RVV build 可能命中 production 分流，所以这里用
// n < 64 的小样本专门保护 scalar reference（标量参考链路）。
TEST(TransformationEstimationPointToPlaneLLSWeighted, StdDiagnosticMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(3, 0.22f);
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

// 这个测试闭合 source-indexed（源索引路径）的公开入口：权重来自
// setCorrespondenceWeights，row k 使用 source[indices[k]] 和 target[k]。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     StdSourceIndexedMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, source_indices, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_source_indices(source, source_indices, target, weights, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-6f);
}

// 这个测试闭合 dual-indices（双索引路径）的公开入口：两条 index stream 决定取点，
// 但连续 weights[k] 仍按 row 序号推进。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     StdDualIndicesMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const pcl::Indices target_indices = makeTargetIndices(target.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix = diag::estimate_std_dual_indices(
      source, source_indices, target, target_indices, weights, &stats);

  EXPECT_EQ(stats.input_points, source_indices.size());
  EXPECT_EQ(stats.accepted_points, source_indices.size());
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

// 这个 A/B 测试把 current vcompress + fixed buffer + tail 与 full-cloud
// block-reduction 分开。block 仅改变跨 lane reduction tree，不改变 weighted lane formula。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionMatchesStdWithinBudget)
{
  auto source = makeSurfaceCloud(28, 0.12f);
  auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());
  source[7].x = std::numeric_limits<float>::quiet_NaN();
  target[19].normal_z = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats current_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation current_eq =
      diag::accumulate_candidate_full(source, target, weights, &current_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  EXPECT_NEAR((current_eq.ata - std_eq.ata).norm(), 0.0, 5e-2);
  EXPECT_NEAR((current_eq.atb - std_eq.atb).norm(), 0.0, 5e-2);
  EXPECT_NEAR((block_eq.ata - std_eq.ata).norm(), 0.0, 2e-1);
  EXPECT_NEAR((block_eq.atb - std_eq.atb).norm(), 0.0, 2e-1);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   2e-3f);
}

// 这个测试构造 near-cancellation（近抵消）样本：target 沿 normal 做很小的正负扰动，
// 让 ATb 中的贡献互相抵消。它专门保护 block-reduction 改变规约树后的矩阵预算。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionNearCancellationStressMatchesStd)
{
  auto source = makeSurfaceCloud(31, 0.055f);
  auto target = source;
  std::vector<float> weights;
  weights.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
    const float eps = sign * (2.0e-4f + 1.0e-5f * static_cast<float>(i % 7));
    target[i].x += eps * target[i].normal_x;
    target[i].y += eps * target[i].normal_y;
    target[i].z += eps * target[i].normal_z;
    weights.push_back(0.25f + 0.05f * static_cast<float>(i % 11));
  }

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  expectNormalEquationWithinBudget(block_eq, std_eq, 5e-2, 2e-5, 5e-4, 2e-4);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   4e-3f);
}

// 这个测试把坐标和权重拉开到更宽尺度，确认 block-reduction 在大 ATA/ATb 量级下
// 仍保持可解释的相对误差预算。它不把该预算写成 production 语义，只服务 PI1 风险评估。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionScaleStressMatchesStd)
{
  auto source = makeSurfaceCloud(33, 0.21f);
  for (auto& point : source) {
    point.x *= 8.0f;
    point.y *= 8.0f;
    point.z *= 4.0f;
  }
  const auto target = makeTargetCloud(source);
  std::vector<float> weights;
  weights.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float scale = (i % 3 == 0) ? 0.015f : ((i % 3 == 1) ? 3.5f : 48.0f);
    weights.push_back(scale + 0.125f * static_cast<float>(i % 5));
  }

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  expectNormalEquationWithinBudget(block_eq, std_eq, 4e4, 3e-5, 1e4, 3e-5);
  expectMatrixNear(diag::solve_normal_equation(block_eq),
                   diag::solve_normal_equation(std_eq),
                   8e-3f);
}

// 这个测试同时放入非有限 point/normal 和非有限 weight。point/normal 非有限的 lane
// 必须被剔除；point/normal 有限但 weight 非有限的 lane 必须保留并污染法方程。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     FullCloudBlockReductionPreservesNonFiniteWeightSemantics)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_y = std::numeric_limits<float>::infinity();
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats block_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation block_eq =
      diag::accumulate_candidate_full_block_reduction(
          source, target, weights, &block_stats);

  EXPECT_EQ(block_stats.input_points, std_stats.input_points);
  EXPECT_EQ(block_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(block_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(block_stats.used_rvv);
#else
  EXPECT_FALSE(block_stats.used_rvv);
#endif
  EXPECT_FALSE(std::isfinite(std_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(block_eq.ata.norm()));
}

// 这个测试验证 source-indexed（源索引路径）：第 k 行来自 source[indices[k]]、
// target[k] 和 weights[k]。它把单侧 gather 与连续权重读取分开审查。
TEST(TransformationEstimationPointToPlaneLLSWeighted, SourceIndexedCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_source_indices(source, source_indices, target, weights, &std_stats);
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_source_indices(
      source, source_indices, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-4f);
}

// 这个测试验证 dual-indices（双索引路径）：source 和 target 都来自独立 index stream，
// weight 仍按 row 序号读取。它不能替代 correspondences 的 query/match/weight 展开证据。
TEST(TransformationEstimationPointToPlaneLLSWeighted, DualIndicesCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(34, 0.095f);
  const auto target = makeTargetCloud(source);
  const pcl::Indices source_indices = makeSourceIndices(source.size());
  const pcl::Indices target_indices = makeTargetIndices(source.size());
  const std::vector<float> weights = makeWeights(source_indices.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = diag::estimate_std_dual_indices(
      source, source_indices, target, target_indices, weights, &std_stats);
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_dual_indices(
      source, source_indices, target, target_indices, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 6e-4f);
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

// 这个测试锁住 weight finite semantics（权重有限性语义）：production 只检查点和
// normal 是否有限，不检查 weight。非有限权重应参与计算并污染法方程，而不是被 mask 跳过。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     NonFiniteWeightsAreNotMaskedWhenPointsAreFinite)
{
  const auto source = makeSurfaceCloud(18, 0.16f);
  const auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, weights, &std_stats);
  const diag::NormalEquation candidate_eq =
      diag::accumulate_candidate_full(source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_FALSE(std::isfinite(std_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(candidate_eq.ata.norm()));
}

// production direct：真实 public full-cloud overload 使用连续 weights_，RVV build
// 应命中 full-cloud f32 AoS layout-gated block-reduction；std build 则自然保留标量路径。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPublicOverloadMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 2e-3f);
}

// production direct normal-equation：比只比较 matrix 更早捕获 block-reduction 的
// reduction tree、finite mask 或 weight 语义回归。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudNormalEquationMatchesStdWithinBudget)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 2e-1, 2e-5, 2e-1, 2e-5);
  expectMatrixNear(solveProductionEquation(candidate_eq),
                   solveProductionEquation(std_eq),
                   2e-3f);
}

// production numeric stress：尺度压力样本覆盖大 ATA/ATb 量级下的 accepted_points、
// normal-equation 和 public matrix 预算。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudScaleStressMatchesStdWithinBudget)
{
  auto source = makeSurfaceCloud(33, 0.21f);
  for (auto& point : source) {
    point.x *= 8.0f;
    point.y *= 8.0f;
    point.z *= 4.0f;
  }
  const auto target = makeTargetCloud(source);
  std::vector<float> weights;
  weights.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float scale = (i % 3 == 0) ? 0.015f : ((i % 3 == 1) ? 3.5f : 48.0f);
    weights.push_back(scale + 0.125f * static_cast<float>(i % 5));
  }

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 4e4, 3e-5, 1e4, 3e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 8e-3f);
}

// production non-finite semantics：point/normal 非有限 lane 被剔除；weight 非有限但
// point/normal 有限时仍参与计算并污染法方程，保持原标量合同。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPreservesNonFiniteWeightSemantics)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source);
  std::vector<float> weights = makeWeights(source.size());
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_y = std::numeric_limits<float>::infinity();
  weights[5] = std::numeric_limits<float>::quiet_NaN();
  weights[31] = std::numeric_limits<float>::infinity();

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  EXPECT_FALSE(std::isfinite(std_eq.ata.norm()));
  EXPECT_FALSE(std::isfinite(candidate_eq.ata.norm()));
}

// production fallback：小规模输入即使满足 PointNormal/float/full-cloud，也必须回到
// 原 iterator 标量路径，避免 dispatch 成本进入 tiny case。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto default_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &stats);
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(public_matrix, solveProductionEquation(default_eq), 1e-3f);
}

// production gate：size / weights / VL / byte-offset predicate 是显式窄门；不满足
// 任何一项都不能进入 RVV helper。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPredicateGatesAreNarrow)
{
  constexpr std::size_t kMaxRows =
      std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointNormal);
  EXPECT_TRUE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
               pcl::PointXYZ,
               pcl::PointXYZINormal>(64, 64, 64, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(63, 63, 63, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 63, 64, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 64, 63, 64)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(64, 64, 64, 65)));
  EXPECT_FALSE((prod_detail::canUsePointToPlaneLLSWeightedFullCloudRVV<
                pcl::PointNormal,
                pcl::PointNormal>(kMaxRows + 1, kMaxRows + 1, kMaxRows + 1, 64)));
}

// production generic source：PointXYZ source 只提供 xyz 字段，target normal 仍来自
// PointNormal。这个 public overload 应命中泛型 source layout gate。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPointXYZSourceMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = makeTargetCloud(source_normal);
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      pcl::PointNormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 2e-1, 2e-5, 2e-1, 2e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 2e-3f);
}

// production generic target：PointXYZINormal target 证明 target gate 需要 xyz+normal
// f32 AoS layout，不要求 exact PointNormal；额外 intensity 字段不参与 weighted 公式。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudPointXYZToPointXYZINormalMatchesStdWithinBudget)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsXYZINormal(makeTargetCloud(source_normal));
  const std::vector<float> weights = makeWeights(source.size());

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      pcl::PointXYZINormal>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats std_stats;
  prod_detail::PointToPlaneLLSWeightedFullCloudStats candidate_stats;
  const auto std_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudStd(
          source, target, weights, &std_stats);
  const auto candidate_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectProductionEquationWithinBudget(candidate_eq, std_eq, 2e-1, 2e-5, 2e-1, 2e-5);
  expectMatrixNear(public_matrix, solveProductionEquation(std_eq), 2e-3f);
}

// production target layout fallback：这个 target 已注册 xyz/normal fields，但 normal
// 字段是 double，不满足 f32 normal layout，因此 RVV helper 必须拒绝并回到标量。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudDoubleNormalTargetFallsBackToScalar)
{
  const auto source_normal = makeSurfaceCloud(28, 0.12f);
  const auto source = copySourceAsXYZ(source_normal);
  const auto target = copyTargetAsDoubleNormal(makeTargetCloud(source_normal));
  const std::vector<float> weights = makeWeights(source.size());
  static_assert(!pcl::rvv::RVVXYZNormalFloatLayout<TEPTPLWDoubleNormalTarget>::value);

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointXYZ,
      TEPTPLWDoubleNormalTarget>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4f public_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  prod_detail::PointToPlaneLLSWeightedFullCloudStats stats;
  const auto default_eq =
      prod_detail::buildPointToPlaneLLSWeightedFullCloudDefault(
          source, target, weights, &stats);
  EXPECT_FALSE(stats.used_rvv);
  expectMatrixNear(public_matrix, solveProductionEquation(default_eq), 1e-6f);
}

// production Scalar gate：输出 Scalar=double 不在本轮 RVV 范围内，即使点类型满足
// f32 AoS layout gate，也必须保留原标量路径。
TEST(TransformationEstimationPointToPlaneLLSWeighted,
     ProductionFullCloudScalarDoubleFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source);
  const std::vector<float> float_weights = makeWeights(source.size());
  std::vector<double> weights;
  weights.reserve(float_weights.size());
  for (const float weight : float_weights)
    weights.push_back(static_cast<double>(weight));

  pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
      pcl::PointNormal,
      pcl::PointNormal,
      double>
      estimator;
  estimator.setCorrespondenceWeights(weights);
  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);
  EXPECT_TRUE(public_matrix.allFinite());
}
