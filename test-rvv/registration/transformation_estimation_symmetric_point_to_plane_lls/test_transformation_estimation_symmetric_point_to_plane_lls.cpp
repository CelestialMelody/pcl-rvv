/*
 * 本文件做什么：
 * 这些专项测试覆盖 symmetric point-to-plane LLS（对称点到平面线性最小二乘）的
 * scalar reference（标量参考链路）和 RVV candidate（RVV 候选链路）。main() 由
 * gtest 提供；Makefile 会分别构建 std 和 RVV 二进制。
 *
 * 阅读提示：
 * - 公开全云入口（full-cloud，source/target 按相同下标一一对应）和公开对应关系入口
 *   （correspondences，由 index_query/index_match 指定点对）都进入同一个
 *   ConstCloudIterator（常量点云迭代器）逐点 helper。
 * - symmetric LLS 比普通 point-to-plane LLS 多了 source normal（源法线）和 target
 *   normal（目标法线）的同向选择。测试会故意翻转部分 target normal，确保
 *   enforce_same_direction_normals gate（法线同向验收条件）被覆盖。
 * - RVV candidate 使用 stride load（跨步加载）或 gather（离散加载）读取 PointNormal
 *   字段，再把有效 lane（向量通道）压缩后交给标量尾段累加。
 */

#include "transformation_estimation_symmetric_point_to_plane_lls_diag.hpp"

#include <pcl/common/rvv_point_traits.h>
#include <pcl/common/transforms.h>
#include <pcl/field_traits.h>
#include <pcl/registration/transformation_estimation_symmetric_point_to_plane_lls.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace diag = pcl::registration::rvv_te_symmetric_pt2plane_lls_diag;

namespace {

static_assert(pcl::rvv::RVVXYZNormalFloatLayout<pcl::PointNormal>::value);
static_assert(pcl::rvv::RVVXYZNormalFloatLayout<pcl::PointXYZINormal>::value);
static_assert(!pcl::rvv::RVVXYZNormalFloatLayout<pcl::PointXYZ>::value);
static_assert(pcl::rvv::kRVVXYZNormalPointCompatible<pcl::PointXYZINormal>);

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
      point.z = 0.08f * x * x + 0.13f * x * y - 0.21f * y + 0.7f;
      point.normal_x = -0.16f * x - 0.13f * y;
      point.normal_y = -0.13f * x + 0.21f;
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
  transform.row(0) << 0.9952f, 0.0867f, 0.0452f, 0.0800f;
  transform.row(1) << -0.0876f, 0.9960f, 0.0170f, -0.1300f;
  transform.row(2) << -0.0435f, -0.0208f, 0.9988f, 0.2200f;
  transform.row(3) << 0.0000f, 0.0000f, 0.0000f, 1.0000f;
  return transform;
}

pcl::PointCloud<pcl::PointNormal>
makeTargetCloud(const pcl::PointCloud<pcl::PointNormal>& source, const bool flip_some_normals)
{
  // 使用 PCL 公共 transform helper 保持真实 PointNormal 内存布局和 normal 字段。
  pcl::PointCloud<pcl::PointNormal> target;
  pcl::transformPointCloudWithNormals(source, target, makeTransform());
  if (flip_some_normals) {
    for (std::size_t i = 5; i < target.size(); i += 17) {
      target[i].normal_x = -target[i].normal_x;
      target[i].normal_y = -target[i].normal_y;
      target[i].normal_z = -target[i].normal_z;
    }
  }
  return target;
}

template <typename PointT>
pcl::PointCloud<PointT>
copyAsGenericNormalCloud(const pcl::PointCloud<pcl::PointNormal>& source)
{
  // 泛型 normal 点类型测试只改变点结构布局，不改变几何输入。这样 public estimator
  // 与 PointNormal reference（参考链路）之间的差异可以归因到 traits/offset 分流。
  pcl::PointCloud<PointT> cloud;
  cloud.height = source.height;
  cloud.width = source.width;
  cloud.is_dense = source.is_dense;
  cloud.reserve(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    PointT point;
    point.x = source[i].x;
    point.y = source[i].y;
    point.z = source[i].z;
    point.normal_x = source[i].normal_x;
    point.normal_y = source[i].normal_y;
    point.normal_z = source[i].normal_z;
    if constexpr (pcl::traits::has_intensity<PointT>::value)
      point.intensity = static_cast<float>(i % 19) * 0.01f;
    if constexpr (pcl::traits::has_label<PointT>::value)
      point.label = static_cast<std::uint32_t>(i % 23);
    cloud.push_back(point);
  }
  cloud.width = cloud.size();
  return cloud;
}

pcl::Correspondences
makeCorrespondences(const std::size_t n)
{
  // 乱序、重复 index 共同覆盖对应关系索引读取路径
  //（correspondences gather path），也说明它和全云顺序扫描不是同一种数据流。
  pcl::Correspondences correspondences;
  correspondences.reserve(n + n / 13);
  for (std::size_t i = 0; i < n; i += 2)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0f);
  for (std::size_t i = 1; i < n; i += 5)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0f);
  for (std::size_t i = 11; i < n; i += 41)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0f);
  return correspondences;
}

pcl::Indices
makeSubsetIndices(const std::size_t n)
{
  pcl::Indices indices;
  indices.reserve(n / 2);
  for (std::size_t i = 0; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  return indices;
}

pcl::Indices
makeIndexedRvvRows(const std::size_t n)
{
  // 生成 source 侧有效但非连续、含重复的 index stream（索引流）。新增 indexed 消融用
  // 它覆盖 gather（离散加载）成本，同时避免非法 index 把 production 输入合同外的行为混进来。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 0; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  if (n > 10) {
    indices.push_back(10);
    indices.push_back(2);
  }
  for (std::size_t i = 3; i < n; i += 5)
    indices.push_back(static_cast<int>(i));
  for (std::size_t i = 11; i < n; i += 41)
    indices.push_back(static_cast<int>(i));
  return indices;
}

pcl::Indices
makeIndependentTargetIndexedRvvRows(const std::size_t n)
{
  // 双侧 indices 消融需要 target 侧有独立 index stream，不能直接复用 source indices。
  // 这里同样只生成有效 index，保留非连续和重复行，让测试能覆盖 row 配对顺序、重复 index
  // 和两侧 gather；非法 index 行为仍明确不属于本轮 correctness 合同。
  pcl::Indices indices;
  indices.reserve(n);
  for (std::size_t i = 1; i < n; i += 2)
    indices.push_back(static_cast<int>(i));
  if (n > 11) {
    indices.push_back(11);
    indices.push_back(3);
  }
  for (std::size_t i = 4; i < n; i += 5)
    indices.push_back(static_cast<int>(i));
  for (std::size_t i = 17; i < n; i += 37)
    indices.push_back(static_cast<int>(i));
  return indices;
}

template <typename PointT>
pcl::PointCloud<PointT>
copyIndexedCloud(const pcl::PointCloud<PointT>& cloud, const pcl::Indices& indices)
{
  pcl::PointCloud<PointT> subset;
  subset.height = 1;
  subset.is_dense = cloud.is_dense;
  subset.reserve(indices.size());
  for (const int index : indices)
    subset.push_back(cloud[static_cast<std::size_t>(index)]);
  subset.width = subset.size();
  return subset;
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

// 这个测试验证 test-rvv 标量诊断是否复刻公开全云 symmetric estimator。
// RVV 构建中公开 estimator 会先尝试 production direct（真实生产入口分流），因此这里的
// 容差同时覆盖“标量诊断 vs 生产 RVV”的 PI3 入口证据。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     StdDiagnosticMatchesPublicEstimatorWithNormalDirectionGate)
{
  const auto source = makeSurfaceCloud(14, 0.22f);
  const auto target = makeTargetCloud(source, true);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_full(source, target, true, &stats);

  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
#ifdef __RVV10__
  expectMatrixNear(diagnostic_matrix, public_matrix, 4e-3f);
#else
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
#endif
}

// 这个测试单独覆盖关闭同向法线 gate 的公开语义；target normal 不翻转，避免公式退化
// 干扰“setEnforceSameDirectionNormals(false)”本身的对拍。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     StdDiagnosticMatchesPublicEstimatorWithoutNormalDirectionGate)
{
  const auto source = makeSurfaceCloud(14, 0.22f);
  const auto target = makeTargetCloud(source, false);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setEnforceSameDirectionNormals(false);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_full(source, target, false, &stats);

  EXPECT_EQ(stats.accepted_points, source.size());
#ifdef __RVV10__
  expectMatrixNear(diagnostic_matrix, public_matrix, 4e-3f);
#else
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
#endif
}

// 这个测试是 PI3 production direct（真实生产入口命中 RVV 分流后的证据）主 case：
// 它调用公开 full-cloud estimator，而不是 test-rvv wrapper；再与同构 diagnostic candidate
// 对拍，证明 full-cloud `PointNormal` / `float` 入口已接入同一窄范围 RVV 语义。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectFullCloudMatchesDiagnosticCandidate)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source, true);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_full(source, target, true, &candidate_stats);

#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(public_matrix, candidate_matrix, 1e-4f);
}

// 小输入必须从真实公开入口回到原标量 helper；这个 case 防止 PI2 分流越过规模 gate。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source, true);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_candidate_full(source, target, true, &stats);
  EXPECT_FALSE(stats.used_rvv);
  expectMatrixNear(public_matrix, diagnostic_matrix, 1e-4f);
}

// invalid lane 通过真实公开入口覆盖 source/target xyz 和合成 normal 的 finite mask。
// RVV 构建应与同构 candidate 一致；std 构建则继续证明原标量语义。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectInvalidLaneMatchesDiagnosticCandidate)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source, true);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  source[17].normal_x = std::numeric_limits<float>::quiet_NaN();

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_candidate_full(source, target, true, &stats);
#ifdef __RVV10__
  EXPECT_TRUE(stats.used_rvv);
#else
  EXPECT_FALSE(stats.used_rvv);
#endif
  expectMatrixNear(public_matrix, diagnostic_matrix, 1e-4f);
}

// 这个测试覆盖本轮新增的泛型 normal 点类型 production direct：PointXYZINormal 满足
// RVV Generic Point Type Strategy（泛型点类型策略）的 xyz + normal float 字段 gate。
// 如果 RVV 构建失败或输出偏离，说明 traits offset、stride 或 normal 字段映射不成立。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectPointXYZINormalFullCloudMatchesPointNormalReference)
{
  const auto source_ref = makeSurfaceCloud(28, 0.12f);
  const auto target_ref = makeTargetCloud(source_ref, true);
  const auto source = copyAsGenericNormalCloud<pcl::PointXYZINormal>(source_ref);
  const auto target = copyAsGenericNormalCloud<pcl::PointXYZINormal>(target_ref);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointXYZINormal,
      pcl::PointXYZINormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  const Eigen::Matrix4f reference_matrix =
      diag::estimate_std_full(source_ref, target_ref, true);
#ifdef __RVV10__
  expectMatrixNear(public_matrix, reference_matrix, 4e-3f);
#else
  expectMatrixNear(public_matrix, reference_matrix, 1e-4f);
#endif
}

// source/target 的布局 gate 必须分别计算。这个 mixed layout case 让 source 使用
// PointXYZINormal、target 使用 PointNormal，防止 production RVV 错把一侧 offset 复用到另一侧。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectMixedNormalLayoutsMatchPointNormalReference)
{
  const auto source_ref = makeSurfaceCloud(28, 0.12f);
  const auto target_ref = makeTargetCloud(source_ref, true);
  const auto source = copyAsGenericNormalCloud<pcl::PointXYZINormal>(source_ref);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointXYZINormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target_ref, public_matrix);

  const Eigen::Matrix4f reference_matrix =
      diag::estimate_std_full(source_ref, target_ref, true);
#ifdef __RVV10__
  expectMatrixNear(public_matrix, reference_matrix, 4e-3f);
#else
  expectMatrixNear(public_matrix, reference_matrix, 1e-4f);
#endif
}

// `Scalar=double` 是 PI1 明确排除的生产边界。该测试保证模板实例仍可编译运行，
// 且输出保持上游标量估计器的几何精度。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ScalarDoubleFallbackStillMatchesGroundTruth)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const Eigen::Matrix4f transform = makeTransform();
  pcl::PointCloud<pcl::PointNormal> target;
  pcl::transformPointCloudWithNormals(source, target, transform);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal,
      double>
      estimator;
  Eigen::Matrix4d public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(public_matrix(row, col),
                  static_cast<double>(transform(row, col)),
                  1e-2)
          << "row=" << row << " col=" << col;
}

// `Scalar=double` 对泛型 normal 点类型也必须保持标量；本轮只扩大 float full-cloud。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ScalarDoublePointXYZINormalFallbackStillMatchesGroundTruth)
{
  const auto source_ref = makeSurfaceCloud(16, 0.18f);
  const Eigen::Matrix4f transform = makeTransform();
  pcl::PointCloud<pcl::PointNormal> target_ref;
  pcl::transformPointCloudWithNormals(source_ref, target_ref, transform);
  const auto source = copyAsGenericNormalCloud<pcl::PointXYZINormal>(source_ref);
  const auto target = copyAsGenericNormalCloud<pcl::PointXYZINormal>(target_ref);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointXYZINormal,
      pcl::PointXYZINormal,
      double>
      estimator;
  Eigen::Matrix4d public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(public_matrix(row, col),
                  static_cast<double>(transform(row, col)),
                  1e-2)
          << "row=" << row << " col=" << col;
}

// 这个测试覆盖 production SourceIndexedRowSource（source 索引 + target 全云）公开入口。
// target 在进入入口前已压成与 indices 行数相同的紧凑 cloud，因此 public API 的 row 配对
// 仍是 source_indices[row] 对 target[row]。RVV 构建中它应与 test-rvv candidate 对齐；
// std 构建中则自然落回原 ConstCloudIterator 标量路径。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectSourceIndicesMatchesCandidate)
{
  const auto source = makeSurfaceCloud(8, 0.16f);
  const auto target_full = makeTargetCloud(source, true);
  const pcl::Indices indices = makeIndexedRvvRows(source.size());
  const pcl::PointCloud<pcl::PointNormal> target_subset =
      copyIndexedCloud(target_full, indices);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, indices, target_subset, public_matrix);

  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_source_indices(
      source, indices, target_subset, true, &candidate_stats);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(public_matrix, candidate_matrix, 4e-3f);
}

// 泛型 normal 点类型也走同一 SourceIndexedRowSource production policy。这个 case 证明
// source gather 使用 source 布局 offset，target stride 使用 target 布局 offset，没有退回
// PointNormal 专用假设。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectPointXYZINormalSourceIndicesMatchesPointNormalReference)
{
  const auto source_ref = makeSurfaceCloud(8, 0.16f);
  const auto target_ref_full = makeTargetCloud(source_ref, true);
  const auto source = copyAsGenericNormalCloud<pcl::PointXYZINormal>(source_ref);
  const auto target_full =
      copyAsGenericNormalCloud<pcl::PointXYZINormal>(target_ref_full);
  const pcl::Indices indices = makeIndexedRvvRows(source_ref.size());
  const auto target = copyIndexedCloud(target_full, indices);
  const auto target_ref = copyIndexedCloud(target_ref_full, indices);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointXYZINormal,
      pcl::PointXYZINormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, indices, target, public_matrix);

  const Eigen::Matrix4f reference_matrix =
      diag::estimate_std_source_indices(source_ref, indices, target_ref, true);
#ifdef __RVV10__
  expectMatrixNear(public_matrix, reference_matrix, 4e-3f);
#else
  expectMatrixNear(public_matrix, reference_matrix, 1e-4f);
#endif
}

// source-indexed production 也必须保住 `Scalar=double` fallback（回退路径）。
// 这个 case 调用新接入的 `cloud_src + indices_src + cloud_tgt` overload，而不是
// full-cloud overload，防止 RVV 分流越过 `Scalar=float` gate。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ScalarDoubleSourceIndicesFallbackStillMatchesReference)
{
  const auto source = makeSurfaceCloud(8, 0.16f);
  const auto target_full = makeTargetCloud(source, true);
  const pcl::Indices indices = makeIndexedRvvRows(source.size());
  const pcl::PointCloud<pcl::PointNormal> target_subset =
      copyIndexedCloud(target_full, indices);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal,
      double>
      estimator;
  Eigen::Matrix4d public_matrix;
  estimator.estimateRigidTransformation(
      source, indices, target_subset, public_matrix);

  const Eigen::Matrix4f reference_matrix =
      diag::estimate_std_source_indices(source, indices, target_subset, true);
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(public_matrix(row, col),
                  static_cast<double>(reference_matrix(row, col)),
                  1e-3)
          << "row=" << row << " col=" << col;
}

// source-indexed mixed layout：source 使用 PointXYZINormal 触发 gather offset，target 使用
// PointNormal 触发 stride offset。它补齐 source gather 与 target stride 分别使用两侧 traits
// 的 production direct 证据。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectMixedNormalLayoutsSourceIndicesMatchPointNormalReference)
{
  const auto source_ref = makeSurfaceCloud(8, 0.16f);
  const auto target_ref_full = makeTargetCloud(source_ref, true);
  const auto source = copyAsGenericNormalCloud<pcl::PointXYZINormal>(source_ref);
  const pcl::Indices indices = makeIndexedRvvRows(source_ref.size());
  const auto target_ref = copyIndexedCloud(target_ref_full, indices);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointXYZINormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, indices, target_ref, public_matrix);

  const Eigen::Matrix4f reference_matrix =
      diag::estimate_std_source_indices(source_ref, indices, target_ref, true);
#ifdef __RVV10__
  expectMatrixNear(public_matrix, reference_matrix, 4e-3f);
#else
  expectMatrixNear(public_matrix, reference_matrix, 1e-4f);
#endif
}

// source-indexed public invalid lane case：在有效 index stream 内注入 source NaN 和
// compact target Inf，证明 production path 的 finite mask 与标量 reference 一样跳过
// 对应 row。它仍不覆盖负数或越界 index 行为。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectSourceIndicesInvalidLaneMatchesReference)
{
  auto source = makeSurfaceCloud(8, 0.16f);
  const auto target_full = makeTargetCloud(source, true);
  const pcl::Indices indices = makeIndexedRvvRows(source.size());
  auto target_subset = copyIndexedCloud(target_full, indices);
  ASSERT_GE(indices.size(), std::size_t{4});
  ASSERT_GE(target_subset.size(), std::size_t{4});

  source[static_cast<std::size_t>(indices[1])].x =
      std::numeric_limits<float>::quiet_NaN();
  target_subset[2].normal_z = std::numeric_limits<float>::infinity();

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, indices, target_subset, public_matrix);

  diag::AccumulationStats reference_stats;
  const Eigen::Matrix4f reference_matrix = diag::estimate_std_source_indices(
      source, indices, target_subset, true, &reference_stats);
  EXPECT_LT(reference_stats.accepted_points, reference_stats.input_points);
#ifdef __RVV10__
  expectMatrixNear(public_matrix, reference_matrix, 4e-3f);
#else
  expectMatrixNear(public_matrix, reference_matrix, 1e-4f);
#endif
}

// 这个测试单独隔离 source-indexed 的规模 gate。输入行数不足时，公开入口必须继续使用
// 原 iterator 标量语义；它不证明非法 index 行为，只覆盖有效 index stream。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     SourceIndicesOverloadSmallInputFallsBackToScalar)
{
  const auto source = makeSurfaceCloud(3, 0.16f);
  const auto target = makeTargetCloud(source, true);
  const pcl::Indices indices = makeSubsetIndices(source.size());
  const pcl::PointCloud<pcl::PointNormal> target_subset =
      copyIndexedCloud(target, indices);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, indices, target_subset, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_source_indices(source, indices, target_subset, true, &stats);
  EXPECT_FALSE(stats.used_rvv);
  expectMatrixNear(public_matrix, diagnostic_matrix, 1e-4f);
}

// 双侧 indexed 入口仍然不接 production RVV。它继续走 iterator，避免 source-indexed
// policy 意外覆盖 target index stream 或 correspondence 语义。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     SourceAndTargetIndicesOverloadRemainsScalarFallback)
{
  const auto source = makeSurfaceCloud(8, 0.16f);
  const auto target = makeTargetCloud(source, true);
  pcl::Indices source_indices = makeIndexedRvvRows(source.size());
  pcl::Indices target_indices = makeIndependentTargetIndexedRvvRows(target.size());
  const std::size_t paired_rows = std::min(source_indices.size(), target_indices.size());
  source_indices.resize(paired_rows);
  target_indices.resize(paired_rows);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, public_matrix);

  const Eigen::Matrix4f diagnostic_matrix = diag::estimate_std_dual_indices(
      source, source_indices, target, target_indices, true);
  expectMatrixNear(public_matrix, diagnostic_matrix, 1e-4f);
}

// 这个 production direct case 覆盖关闭同向法线 gate 后的真实公开入口。
// 如果失败，说明 production RVV dispatch 没有正确传递 estimator 对象状态。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     ProductionDirectFullCloudWithoutNormalDirectionGateMatchesCandidate)
{
  const auto source = makeSurfaceCloud(30, 0.11f);
  const auto target = makeTargetCloud(source, false);

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  estimator.setEnforceSameDirectionNormals(false);
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, public_matrix);

  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_full(source, target, false, &candidate_stats);

#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(public_matrix, candidate_matrix, 1e-4f);
}

// 这个测试验证公开对应关系入口（correspondences）与标量诊断的语义一致。
// 它覆盖乱序、重复 index 和同向法线选择，不证明 RVV gather 性能。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     StdCorrespondencesMatchesPublicEstimator)
{
  const auto source = makeSurfaceCloud(16, 0.18f);
  const auto target = makeTargetCloud(source, true);
  const pcl::Correspondences correspondences = makeCorrespondences(source.size());

  pcl::registration::TransformationEstimationSymmetricPointToPlaneLLS<
      pcl::PointNormal,
      pcl::PointNormal>
      estimator;
  Eigen::Matrix4f public_matrix;
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);

  diag::AccumulationStats stats;
  const Eigen::Matrix4f diagnostic_matrix =
      diag::estimate_std_correspondences(source, target, correspondences, true, &stats);

  EXPECT_EQ(stats.input_points, correspondences.size());
  expectMatrixNear(diagnostic_matrix, public_matrix, 1e-4f);
}

// 这个测试验证连续 PointNormal 全云输入下，RVV 候选链路会命中 symmetric formula
// staging（对称公式暂存阶段）并与标量一致。
TEST(TransformationEstimationSymmetricPointToPlaneLLS, FullCloudCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(28, 0.12f);
  const auto target = makeTargetCloud(source, true);

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_full(source, target, true, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_full(source, target, true, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 4e-3f);
}

// 这个测试验证关闭同向法线 gate 时，RVV candidate 直接使用 n1 + n2 的路径。
// 如果失败，说明 enforce=false 分支和默认分支被混在了一起。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     FullCloudCandidateWithoutNormalDirectionGateMatchesStd)
{
  const auto source = makeSurfaceCloud(30, 0.11f);
  const auto target = makeTargetCloud(source, false);

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_full(source, target, false, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      diag::estimate_candidate_full(source, target, false, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 4e-3f);
}

// 这个测试验证对应关系路径（correspondences）的乱序和重复 index。
// 如果失败，说明 gather（离散加载）、法线选择或 vcompress 后保序尾段的证据断了。
TEST(TransformationEstimationSymmetricPointToPlaneLLS, CorrespondenceCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target = makeTargetCloud(source, true);
  const pcl::Correspondences correspondences = makeCorrespondences(source.size());

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_correspondences(source, target, correspondences, true, &std_stats);
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_correspondences(
      source, target, correspondences, true, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-3f);
}

// 这个测试是本轮 source 单侧 indexed 消融的 correctness（正确性）gate，也是
// test-rvv-only `SourceIndexedRowSource` policy 的调用者。source 侧按 indices gather，
// target 侧是紧凑全云顺序扫描；如果失败，说明单侧 gather + target stride load（跨步加载）
// 这条诊断数据流无法与标量 same-chain 对齐。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     SourceIndicesCandidateMatchesStd)
{
  const auto source = makeSurfaceCloud(32, 0.10f);
  const auto target_full = makeTargetCloud(source, true);
  const pcl::Indices source_indices = makeIndexedRvvRows(source.size());
  const pcl::PointCloud<pcl::PointNormal> target =
      copyIndexedCloud(target_full, source_indices);

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix =
      diag::estimate_std_source_indices(source, source_indices, target, true, &std_stats);
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_source_indices(
      source, source_indices, target, true, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-3f);
}

// 这个测试是双侧 indexed 消融的 correctness gate。它直接传两个 index vector，
// 不从 pcl::Correspondence 展开 query/match，也没有 correspondence.weight。
// 因此失败只说明双侧 gather 诊断链路断开，不涉及 correspondence parsing 成本。
TEST(TransformationEstimationSymmetricPointToPlaneLLS, DualIndicesCandidateMatchesStd)
{
  auto source = makeSurfaceCloud(32, 0.10f);
  auto target = makeTargetCloud(source, true);
  const pcl::Indices source_indices = makeIndexedRvvRows(source.size());
  const pcl::Indices target_indices = makeIndependentTargetIndexedRvvRows(target.size());

  ASSERT_FALSE(source_indices.empty());
  ASSERT_FALSE(target_indices.empty());
  EXPECT_NE(source_indices, target_indices);
  ASSERT_GE(source_indices.size(), std::size_t{12});
  ASSERT_GE(target_indices.size(), std::size_t{12});
  EXPECT_NE(std::find(source_indices.begin() + 2, source_indices.end(), source_indices[1]),
            source_indices.end());
  EXPECT_NE(std::find(target_indices.begin() + 2, target_indices.end(), target_indices[1]),
            target_indices.end());

  source[static_cast<std::size_t>(source_indices[1])].x =
      std::numeric_limits<float>::quiet_NaN();
  target[static_cast<std::size_t>(target_indices[2])].normal_z =
      std::numeric_limits<float>::infinity();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const Eigen::Matrix4f std_matrix = diag::estimate_std_dual_indices(
      source, source_indices, target, target_indices, true, &std_stats);
  const Eigen::Matrix4f candidate_matrix = diag::estimate_candidate_dual_indices(
      source, source_indices, target, target_indices, true, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_LT(std_stats.accepted_points, std_stats.input_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
#endif
  expectMatrixNear(candidate_matrix, std_matrix, 5e-3f);
}

// 这个测试单独隔离小规模 fallback（回退路径）gate：输入太小时必须回到标量链路。
// 如果失败，说明规模阈值或 fallback normal-equation 等价性断了。
TEST(TransformationEstimationSymmetricPointToPlaneLLS,
     SmallInputFallsBackForIsolatedSizeGate)
{
  const auto source = makeSurfaceCloud(3, 0.30f);
  const auto target = makeTargetCloud(source, true);

  diag::AccumulationStats stats;
  const diag::NormalEquation candidate =
      diag::accumulate_candidate_full(source, target, true, &stats);
  const diag::NormalEquation reference = diag::accumulate_std_full(source, target, true);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_EQ(candidate.accepted_points, reference.accepted_points);
  EXPECT_NEAR((candidate.ata - reference.ata).norm(), 0.0f, 1e-6f);
  EXPECT_NEAR((candidate.atb - reference.atb).norm(), 0.0f, 1e-6f);
}

// 这个测试验证 NaN/Inf invalid lane（无效 lane）会被 finite mask（有限值掩码）剔除。
// source normal 出错也通过合成后的 n 被剔除，匹配 production 的 n.array().isFinite() 检查。
TEST(TransformationEstimationSymmetricPointToPlaneLLS, InvalidLaneMaskMatchesStd)
{
  auto source = makeSurfaceCloud(20, 0.14f);
  auto target = makeTargetCloud(source, true);
  source[3].x = std::numeric_limits<float>::quiet_NaN();
  target[9].normal_z = std::numeric_limits<float>::infinity();
  source[17].normal_x = std::numeric_limits<float>::quiet_NaN();

  diag::AccumulationStats std_stats;
  diag::AccumulationStats candidate_stats;
  const diag::NormalEquation std_eq =
      diag::accumulate_std_full(source, target, true, &std_stats);
  const diag::NormalEquation candidate_eq =
      diag::accumulate_candidate_full(source, target, true, &candidate_stats);

  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
  EXPECT_EQ(candidate_eq.accepted_points, std_eq.accepted_points);
  EXPECT_NEAR((candidate_eq.ata - std_eq.ata).norm(), 0.0f, 8e-2f);
  EXPECT_NEAR((candidate_eq.atb - std_eq.atb).norm(), 0.0f, 8e-2f);
}
