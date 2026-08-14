/*
 * 本文件做什么：
 * 这些测试把 `IterativeClosestPoint::transformCloud` 形状的 test-only RVV candidate
 * 与标量 reference 对拍。它们覆盖动态字段 offset、in-place（原地写回）、NaN/Inf
 * 有限值 gate（会触发跳过写回的条件）和小规模 fallback（回退路径）。
 */

#include "test_icp.h"

#include <pcl/registration/icp.h>
#include <pcl/test/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>

namespace support = pcl::registration::rvv_icp_support;

namespace {

void
expectFloatSameOrNear(float candidate, float reference, float epsilon, const char* label, std::size_t i)
{
  if (std::isfinite(candidate) && std::isfinite(reference)) {
    EXPECT_NEAR(candidate, reference, epsilon) << label << " mismatch at " << i;
    return;
  }
  if (std::isnan(candidate) || std::isnan(reference)) {
    EXPECT_TRUE(std::isnan(candidate) && std::isnan(reference))
        << label << " NaN preservation mismatch at " << i;
    return;
  }
  EXPECT_FLOAT_EQ(candidate, reference) << label << " infinity mismatch at " << i;
}

template <typename PointT>
void
expectXYZNear(const pcl::PointCloud<PointT>& candidate,
              const pcl::PointCloud<PointT>& reference,
              float epsilon = 2e-5f)
{
  ASSERT_EQ(candidate.size(), reference.size());
  for (std::size_t i = 0; i < candidate.size(); ++i) {
    expectFloatSameOrNear(candidate[i].x, reference[i].x, epsilon, "x", i);
    expectFloatSameOrNear(candidate[i].y, reference[i].y, epsilon, "y", i);
    expectFloatSameOrNear(candidate[i].z, reference[i].z, epsilon, "z", i);
  }
}

void
expectNormalsNear(const pcl::PointCloud<pcl::PointNormal>& candidate,
                  const pcl::PointCloud<pcl::PointNormal>& reference,
                  float epsilon = 2e-5f)
{
  ASSERT_EQ(candidate.size(), reference.size());
  for (std::size_t i = 0; i < candidate.size(); ++i) {
    expectFloatSameOrNear(candidate[i].normal_x, reference[i].normal_x, epsilon, "normal_x", i);
    expectFloatSameOrNear(candidate[i].normal_y, reference[i].normal_y, epsilon, "normal_y", i);
    expectFloatSameOrNear(candidate[i].normal_z, reference[i].normal_z, epsilon, "normal_z", i);
  }
}

template <typename PointT, typename Scalar = float>
class ExposedICP : public pcl::IterativeClosestPoint<PointT, PointT, Scalar> {
  using Base = pcl::IterativeClosestPoint<PointT, PointT, Scalar>;

public:
  using Matrix4 = typename Base::Matrix4;
  using PointCloudSource = typename Base::PointCloudSource;

  void
  initializeSource(const PointCloudSource& input)
  {
    this->setInputSource(pcl::make_shared<PointCloudSource>(input));
  }

  void
  transformPublic(const PointCloudSource& input,
                  PointCloudSource& output,
                  const Matrix4& transform)
  {
    this->transformCloud(input, output, transform);
  }
};

support::FieldLayout
pointXYZILayout()
{
  return {offsetof(pcl::PointXYZI, x),
          offsetof(pcl::PointXYZI, y),
          offsetof(pcl::PointXYZI, z),
          0,
          0,
          0};
}

pcl::PointCloud<pcl::PointXYZI>
makePointXYZICloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 190.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 3) % 4093) - 2046) / 220.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 17) % 4091) - 2045) / 250.0f;
    cloud[i].intensity = static_cast<float>(i % 251) * 0.25f;
  }
  return cloud;
}

support::FieldLayout
pointXYZINormalLayout()
{
  return {offsetof(pcl::PointXYZINormal, x),
          offsetof(pcl::PointXYZINormal, y),
          offsetof(pcl::PointXYZINormal, z),
          offsetof(pcl::PointXYZINormal, normal_x),
          offsetof(pcl::PointXYZINormal, normal_y),
          offsetof(pcl::PointXYZINormal, normal_z)};
}

pcl::PointCloud<pcl::PointXYZINormal>
makePointXYZINormalCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZINormal> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float a = static_cast<float>((i % 991) + 1) * 0.0027f;
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 205.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 9) % 4093) - 2046) / 235.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 15) % 4091) - 2045) / 265.0f;
    cloud[i].normal_x = std::cos(a) * 0.71f;
    cloud[i].normal_y = std::sin(a) * 0.59f;
    cloud[i].normal_z = 0.19f + std::cos(a * 0.5f) * 0.37f;
    cloud[i].intensity = static_cast<float>(i % 257) * 0.125f;
    cloud[i].curvature = static_cast<float>(i % 127) * 0.03125f;
  }
  return cloud;
}

} // namespace

// PointXYZ 全云顺序扫描是 `source_has_normals_ == false` 的主分支。失败说明 RVV 候选
// 的矩阵乘法、mask store（带掩码写回）或动态 offset 映射与 production 标量语义不一致。
TEST(IterativeClosestPointTransformCloudRVV, PointXYZMatchesScalar)
{
  const auto input = support::makePointXYZCloud(4096);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  pcl::PointCloud<pcl::PointXYZ> candidate;
  const auto transform = support::makeRigidTransform();

  const auto scalar_stats =
      support::transformCloudStd(input, scalar, transform, support::pointXYZLayout(), false);
  const auto candidate_stats = support::transformCloudCandidate(
      input, candidate, transform, support::pointXYZLayout(), false);

  EXPECT_EQ(candidate_stats.input_points, scalar_stats.input_points);
  EXPECT_EQ(candidate_stats.xyz_written, scalar_stats.xyz_written);
  EXPECT_EQ(candidate_stats.normals_written, 0u);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectXYZNear(candidate, scalar);
}

// PointNormal 覆盖 source_has_normals_ 分支：XYZ 有限时先写 xyz，normal 有限时再独立旋转法线。
// 这条测试保护“normal 非法不能回滚已经写出的 xyz”这一 production 细节。
TEST(IterativeClosestPointTransformCloudRVV, PointNormalMatchesScalarAndFiniteBranches)
{
  auto input = support::makePointNormalCloud(4096);
  input[7].x = std::numeric_limits<float>::quiet_NaN();
  input[19].normal_y = std::numeric_limits<float>::infinity();
  input[31].z = -std::numeric_limits<float>::infinity();

  pcl::PointCloud<pcl::PointNormal> scalar;
  pcl::PointCloud<pcl::PointNormal> candidate;
  const auto transform = support::makeRigidTransform();

  const auto scalar_stats =
      support::transformCloudStd(input, scalar, transform, support::pointNormalLayout(), true);
  const auto candidate_stats = support::transformCloudCandidate(
      input, candidate, transform, support::pointNormalLayout(), true);

  EXPECT_EQ(candidate_stats.input_points, scalar_stats.input_points);
  EXPECT_EQ(candidate_stats.xyz_written, scalar_stats.xyz_written);
  EXPECT_EQ(candidate_stats.normals_written, scalar_stats.normals_written);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectXYZNear(candidate, scalar);
  expectNormalsNear(candidate, scalar);
  EXPECT_FLOAT_EQ(candidate[19].x, scalar[19].x);
  EXPECT_FLOAT_EQ(candidate[19].normal_y, input[19].normal_y);
}

// In-place 形态对应 production 注释里的 “cloud_in equal to cloud_out”。这条测试证明
// 候选在读取输入字段后再写回，不会因为同一对象写回而污染本 chunk 后续 lane。
TEST(IterativeClosestPointTransformCloudRVV, InPlaceMatchesOutOfPlace)
{
  auto in_place = support::makePointNormalCloud(2048);
  const auto input = in_place;
  pcl::PointCloud<pcl::PointNormal> out_of_place;
  const auto transform = support::makeRigidTransform();

  support::transformCloudCandidate(
      input, out_of_place, transform, support::pointNormalLayout(), true);
  support::transformCloudCandidate(
      in_place, in_place, transform, support::pointNormalLayout(), true);

  expectXYZNear(in_place, out_of_place);
  expectNormalsNear(in_place, out_of_place);
}

// 小规模输入必须走标量 fallback。失败说明规模 gate 可能把测试噪声很高的小数组送进 RVV。
TEST(IterativeClosestPointTransformCloudRVV, SmallInputFallsBack)
{
  const auto input = support::makePointXYZCloud(17);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  pcl::PointCloud<pcl::PointXYZ> candidate;
  const auto transform = support::makeRigidTransform();

  support::transformCloudStd(input, scalar, transform, support::pointXYZLayout(), false);
  const auto stats = support::transformCloudCandidate(
      input, candidate, transform, support::pointXYZLayout(), false);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectXYZNear(candidate, scalar, 0.0f);
}

// Offset gate（字段偏移验收）不满足时必须回退。这里故意传入错误 layout，候选不能误走
// hard-coded PointXYZ RVV path；实际输出只要求与同一 layout 下的标量 reference 一致。
TEST(IterativeClosestPointTransformCloudRVV, MismatchedRuntimeOffsetsFallback)
{
  const auto input = support::makePointXYZCloud(256);
  support::FieldLayout shifted = support::pointXYZLayout();
  std::swap(shifted.x, shifted.y);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  pcl::PointCloud<pcl::PointXYZ> candidate;
  const auto transform = support::makeRigidTransform();

  support::transformCloudStd(input, scalar, transform, shifted, false);
  const auto stats = support::transformCloudCandidate(input, candidate, transform, shifted, false);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectXYZNear(candidate, scalar, 0.0f);
}

// Production direct（真实生产路径）测试：通过 ICP 的 protected transformCloud 入口对拍
// test reference，保护生产 dispatch 不改变有限值 gate、字段写回和 caller 预置 output 的语义。
TEST(IterativeClosestPointTransformCloudRVV, ProductionDirectPointXYZMatchesScalar)
{
  const auto input = support::makePointXYZCloud(4096);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  auto production = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointXYZ> icp;
  icp.initializeSource(input);

  support::transformCloudStd(input, scalar, transform, support::pointXYZLayout(), false);
  icp.transformPublic(input, production, transform);

  expectXYZNear(production, scalar);
}

// Normal 分支的 production direct 证据：XYZ 非有限跳过整点，normal 非有限只跳过 normal 写回，
// 不能回滚已经写出的 XYZ。
TEST(IterativeClosestPointTransformCloudRVV, ProductionDirectPointNormalFiniteBranches)
{
  auto input = support::makePointNormalCloud(4096);
  input[7].x = std::numeric_limits<float>::quiet_NaN();
  input[19].normal_y = std::numeric_limits<float>::infinity();
  input[31].z = -std::numeric_limits<float>::infinity();

  pcl::PointCloud<pcl::PointNormal> scalar;
  auto production = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointNormal> icp;
  icp.initializeSource(input);

  support::transformCloudStd(input, scalar, transform, support::pointNormalLayout(), true);
  icp.transformPublic(input, production, transform);

  expectXYZNear(production, scalar);
  expectNormalsNear(production, scalar);
  EXPECT_FLOAT_EQ(production[19].x, scalar[19].x);
  EXPECT_FLOAT_EQ(production[19].normal_y, input[19].normal_y);
}

// 泛型 XYZ gate 覆盖非 exact PointXYZ 的 AoS 点型。intensity 不参与 transformCloud 写回，
// output 已由 caller 预置时必须保持原值。
TEST(IterativeClosestPointTransformCloudRVV, ProductionDirectPointXYZIGenericXYZGate)
{
  const auto input = makePointXYZICloud(4096);
  pcl::PointCloud<pcl::PointXYZI> scalar;
  auto production = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointXYZI> icp;
  icp.initializeSource(input);

  support::transformCloudStd(input, scalar, transform, pointXYZILayout(), false);
  icp.transformPublic(input, production, transform);

  expectXYZNear(production, scalar);
  for (std::size_t i = 0; i < input.size(); ++i)
    EXPECT_FLOAT_EQ(production[i].intensity, input[i].intensity) << "intensity at " << i;
}

// 泛型 normal gate 覆盖 `PointNormal` 之外的 xyz+normal AoS 点型。非 transform 字段
// 由 caller 预置，production RVV 路径只能写回 XYZ 和 normal。
TEST(IterativeClosestPointTransformCloudRVV, ProductionDirectPointXYZINormalGenericNormalGate)
{
  auto input = makePointXYZINormalCloud(4096);
  input[5].normal_z = std::numeric_limits<float>::quiet_NaN();
  input[13].y = std::numeric_limits<float>::infinity();

  pcl::PointCloud<pcl::PointXYZINormal> scalar;
  auto production = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointXYZINormal> icp;
  icp.initializeSource(input);

  support::transformCloudStd(input, scalar, transform, pointXYZINormalLayout(), true);
  icp.transformPublic(input, production, transform);

  expectXYZNear(production, scalar);
  ASSERT_EQ(production.size(), scalar.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    expectFloatSameOrNear(
        production[i].normal_x, scalar[i].normal_x, 2e-5f, "normal_x", i);
    expectFloatSameOrNear(
        production[i].normal_y, scalar[i].normal_y, 2e-5f, "normal_y", i);
    expectFloatSameOrNear(
        production[i].normal_z, scalar[i].normal_z, 2e-5f, "normal_z", i);
    EXPECT_FLOAT_EQ(production[i].intensity, input[i].intensity) << "intensity at " << i;
    EXPECT_FLOAT_EQ(production[i].curvature, input[i].curvature) << "curvature at " << i;
  }
}

// 小规模输入在 production dispatch 下仍应自然回到标量路径；这里用零误差预算保护 fallback。
TEST(IterativeClosestPointTransformCloudRVV, ProductionDirectSmallInputFallback)
{
  const auto input = support::makePointXYZCloud(17);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  auto production = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointXYZ> icp;
  icp.initializeSource(input);

  support::transformCloudStd(input, scalar, transform, support::pointXYZLayout(), false);
  icp.transformPublic(input, production, transform);

  expectXYZNear(production, scalar, 0.0f);
}

// Scalar=double 仍走 production 标量路径，但 transformCloud 内部按原实现 cast<float>。
// 这条测试防止 RVV gate 越过 Scalar 类型边界。
TEST(IterativeClosestPointTransformCloudRVV, ProductionDirectScalarDoubleFallback)
{
  const auto input = support::makePointXYZCloud(4096);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  auto production = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointXYZ, double> icp;
  icp.initializeSource(input);

  support::transformCloudStd(input, scalar, transform, support::pointXYZLayout(), false);
  icp.transformPublic(input, production, transform.cast<double>());

  expectXYZNear(production, scalar, 0.0f);
}

// Production direct in-place 保护 cloud_in == cloud_out 的公开注释语义。
TEST(IterativeClosestPointTransformCloudRVV, ProductionDirectInPlaceMatchesOutOfPlace)
{
  auto in_place = support::makePointNormalCloud(2048);
  const auto input = in_place;
  pcl::PointCloud<pcl::PointNormal> out_of_place = input;
  const auto transform = support::makeRigidTransform();
  ExposedICP<pcl::PointNormal> icp;
  icp.initializeSource(input);

  icp.transformPublic(input, out_of_place, transform);
  icp.transformPublic(in_place, in_place, transform);

  expectXYZNear(in_place, out_of_place);
  expectNormalsNear(in_place, out_of_place);
}
