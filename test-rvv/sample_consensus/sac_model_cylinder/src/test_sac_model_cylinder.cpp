/*
 * 本文件做什么：
 * 这些测试为 SampleConsensusModelCylinder 建立 RVV diagnostic（诊断）入口。
 * 当前阶段只验证测试专用 candidate（候选实现）能否复刻公开 count/select
 * 入口的 cylinder 距离公式、weighted euclid early gate（欧氏距离早停门禁）
 * 和 inlier/error 输出顺序。生产接入后，新增 public entry（公开入口）
 * 对 Standard helper（标量 helper）的对拍和 fallback（回退路径）测试。
 */

#include <pcl/test/gtest.h>

#include "sac_model_cylinder.h"

#include <pcl/point_types.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <memory>
#include <type_traits>
#include <vector>

static Eigen::VectorXf
cylinderCoefficients ()
{
  Eigen::VectorXf coeffs (7);
  coeffs << 0.20f, -0.30f, 0.10f, 0.0f, 0.0f, 1.0f, 1.0f;
  return coeffs;
}

static void
rotateAroundZ (Eigen::Vector3f& normal, const float radians)
{
  const float c = std::cos (radians);
  const float s = std::sin (radians);
  normal = Eigen::Vector3f (c * normal.x () - s * normal.y (),
                            s * normal.x () + c * normal.y (),
                            normal.z ());
}

template <typename PointT>
static void
setXYZFields (PointT& point, const Eigen::Vector3f& value)
{
  point.x = value.x ();
  point.y = value.y ();
  point.z = value.z ();
}

template <typename PointNT>
static void
setNormalFields (PointNT& normal_point, const Eigen::Vector3f& value)
{
  normal_point.normal_x = value.x ();
  normal_point.normal_y = value.y ();
  normal_point.normal_z = value.z ();
}

template <typename PointT, typename PointNT>
static void
appendCylinderPoint (pcl::PointCloud<PointT>& cloud,
                     pcl::PointCloud<PointNT>& normals,
                     const float angle,
                     const float z,
                     const float radial_delta,
                     const float normal_angle)
{
  const Eigen::Vector3f axis_point (0.20f, -0.30f, 0.10f);
  const float radius = 1.0f + radial_delta;
  const Eigen::Vector3f radial (std::cos (angle), std::sin (angle), 0.0f);
  const Eigen::Vector3f point = axis_point + Eigen::Vector3f (0.0f, 0.0f, z) + radius * radial;

  PointT pt;
  setXYZFields (pt, point);
  cloud.push_back (pt);

  Eigen::Vector3f normal = radial;
  rotateAroundZ (normal, normal_angle);
  normal.normalize ();

  PointNT nt;
  setNormalFields (nt, normal);
  nt.curvature = 0.0f;
  normals.push_back (nt);
}

static pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCylinderCloud ()
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  (void) normals;
  return cloud;
}

template <typename PointT, typename PointNT>
static void
fillCylinderInputs (pcl::PointCloud<PointT>& cloud,
                    pcl::PointCloud<PointNT>& normals)
{
  const std::vector<float> radial_delta = {
      0.00f, 0.03f, 0.07f, 0.10f, 0.16f, -0.02f, 0.24f, -0.09f};
  const std::vector<float> normal_delta = {
      0.00f, 0.03f, 0.05f, 0.12f, 0.01f, 0.18f, 0.04f, 0.22f};
  for (std::size_t i = 0; i < radial_delta.size (); ++i)
  {
    appendCylinderPoint (cloud,
                         normals,
                         static_cast<float> (i) * 0.63f,
                         static_cast<float> (i) * 0.17f,
                         radial_delta[i],
                         normal_delta[i]);
  }
}

template <typename PointT, typename PointNT>
static void
fillBenchShapedCylinderInputs (pcl::PointCloud<PointT>& cloud,
                               pcl::PointCloud<PointNT>& normals,
                               const std::size_t nr_points)
{
  cloud.reserve (nr_points);
  normals.reserve (nr_points);
  for (std::size_t i = 0; i < nr_points; ++i)
  {
    const float phase = static_cast<float> (i % 8192) / 8192.0f;
    const float angle = phase * 6.28318530718f + static_cast<float> (i % 7) * 0.013f;
    const float z = (static_cast<float> (i) - static_cast<float> (nr_points / 2)) * 0.0007f;
    const int bucket = static_cast<int> (i % 8);
    const float radial_delta =
        (bucket <= 2) ? (0.010f * static_cast<float> (bucket)) :
        (bucket <= 4) ? (0.075f + 0.010f * static_cast<float> (bucket - 3)) :
                        (0.210f + 0.015f * static_cast<float> (bucket - 5));
    const float normal_angle =
        (bucket <= 2) ? (0.010f * static_cast<float> (bucket)) :
        (bucket <= 4) ? (0.040f + 0.010f * static_cast<float> (bucket - 3)) :
                        (0.180f + 0.020f * static_cast<float> (bucket - 5));
    appendCylinderPoint (cloud, normals, angle, z, radial_delta, normal_angle);
  }
}

static pcl::Indices
makeShuffledAdjacentIndices (const std::size_t nr_points)
{
  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);
  return indices;
}

static void
expectSelectCandidateMatchesPublic (pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal>& model,
                                    const Eigen::VectorXf& coeffs,
                                    const double threshold)
{
  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  pcl::Indices candidate_inliers;
  model.selectWithinDistanceCandidate (coeffs, threshold, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;

  ASSERT_EQ (public_inliers, candidate_inliers);
  ASSERT_EQ (public_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < public_errors.size (); ++i)
    EXPECT_NEAR (public_errors[i], candidate_errors[i], 1e-5);
}

template <typename PointT, typename PointNT>
static std::size_t
countStandardDirect (const pcl::PointCloud<PointT>& cloud,
                     const pcl::PointCloud<PointNT>& normals,
                     const pcl::Indices& indices,
                     const Eigen::VectorXf& coeffs,
                     const double threshold,
                     const double normal_weight)
{
  return pcl::detail::countWithinDistanceStandardCylinder<PointT, PointNT> (
      cloud, normals, indices, coeffs, threshold, normal_weight);
}

template <typename PointT, typename PointNT>
static void
selectStandardDirect (const pcl::PointCloud<PointT>& cloud,
                      const pcl::PointCloud<PointNT>& normals,
                      const pcl::Indices& indices,
                      const Eigen::VectorXf& coeffs,
                      const double threshold,
                      const double normal_weight,
                      pcl::Indices& inliers,
                      std::vector<double>& errors)
{
  pcl::detail::selectWithinDistanceStandardCylinder<PointT, PointNT> (
      cloud, normals, indices, coeffs, threshold, normal_weight, inliers, errors);
}

template <typename PointT, typename PointNT>
static void
getDistancesStandardDirect (const pcl::PointCloud<PointT>& cloud,
                            const pcl::PointCloud<PointNT>& normals,
                            const pcl::Indices& indices,
                            const Eigen::VectorXf& coeffs,
                            const double normal_weight,
                            std::vector<double>& distances)
{
  pcl::detail::getDistancesToModelStandardCylinder<PointT, PointNT> (
      cloud, normals, indices, coeffs, normal_weight, distances);
}

template <typename PointT, typename PointNT>
static void
expectPublicEntriesMatchStandardForType (const std::size_t nr_points)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<PointNT>> ();
  if (nr_points == 8)
    fillCylinderInputs (*cloud, *normals);
  else
    fillBenchShapedCylinderInputs (*cloud, *normals, nr_points);
  const pcl::Indices indices =
      (nr_points == 8) ? pcl::Indices {6, 1, 7, 0, 5, 2, 4, 3}
                       : makeShuffledAdjacentIndices (nr_points);
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.10;
  constexpr double normal_weight = 0.25;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<PointT, PointNT> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (normal_weight);

  EXPECT_EQ (countStandardDirect (*cloud, *normals, indices, coeffs, threshold, normal_weight),
             model.countWithinDistance (coeffs, threshold));

  pcl::Indices standard_inliers;
  std::vector<double> standard_errors;
  selectStandardDirect (*cloud,
                        *normals,
                        indices,
                        coeffs,
                        threshold,
                        normal_weight,
                        standard_inliers,
                        standard_errors);

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], public_errors[i], 1e-5);

  std::vector<double> standard_distances;
  getDistancesStandardDirect (*cloud, *normals, indices, coeffs, normal_weight, standard_distances);

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  ASSERT_EQ (standard_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < standard_distances.size (); ++i)
    EXPECT_NEAR (standard_distances[i], public_distances[i], 1e-5);
}

TEST (SampleConsensusModelCylinder, CountCandidateMatchesPublicEntryWithEarlyEuclidGate)
{
  // 这个 case 同时放入半径差过大、法线角度过大和双条件都通过的点。
  // 如果 candidate 忘记 weighted euclid early gate（欧氏项早停），或把
  // normal angle（法线夹角）权重顺序写错，计数会和公开入口不同。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  fillCylinderInputs (*cloud, *normals);
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.25);

  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  const std::size_t candidate_count = model.countWithinDistanceCandidate (coeffs, threshold);

  EXPECT_EQ (public_count, candidate_count);
}

TEST (SampleConsensusModelCylinder, CountPublicEntryMatchesProductionStandardHelper)
{
  // 生产接入后，这个测试直接比较 public entry（公开入口）与生产 Standard
  // helper（标量 helper）。在 RVV 构建下 public entry 应命中 production RVV
  // dispatch；若 RVV 公式、early gate 或 fallback 边界出错，会和 Standard
  // helper 的同边界结果不一致。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  fillCylinderInputs (*cloud, *normals);
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.10;
  constexpr double normal_weight = 0.25;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (normal_weight);

  EXPECT_EQ (countStandardDirect (*cloud, *normals, indices, coeffs, threshold, normal_weight),
             model.countWithinDistance (coeffs, threshold));
}

TEST (SampleConsensusModelCylinder, SelectCandidatePreservesOrderAndErrorDistances)
{
  // selectWithinDistance 需要按 indices_ 扫描顺序写 inliers，并把通过完整
  // cylinder distance（圆柱距离）的误差写入 error_sqr_dists_。这个测试保护
  // `vcompress` 后续可能引入的保序写回边界。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  fillCylinderInputs (*cloud, *normals);
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.25);

  expectSelectCandidateMatchesPublic (model, coeffs, threshold);
}

TEST (SampleConsensusModelCylinder, SelectPublicEntryMatchesProductionStandardHelper)
{
  // 这个 production direct（真实生产路径）测试保护 select 的两个副作用：
  // 保序写入 inliers，并同步写 error_sqr_dists_。RVV `vcompress` 写回不能
  // 改变 `indices_` 扫描顺序。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  fillCylinderInputs (*cloud, *normals);
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.10;
  constexpr double normal_weight = 0.25;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (normal_weight);

  pcl::Indices standard_inliers;
  std::vector<double> standard_errors;
  selectStandardDirect (*cloud,
                        *normals,
                        indices,
                        coeffs,
                        threshold,
                        normal_weight,
                        standard_inliers,
                        standard_errors);

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], public_errors[i], 1e-5);
}

TEST (SampleConsensusModelCylinder, SelectCandidateClearsStaleStateWhenNoInliers)
{
  // 先用常规阈值填充输出，再用 0 阈值验证 candidate 清空旧状态。
  // production select 的副作用包含 inliers 和 error_sqr_dists_ 两个容器，
  // diagnostic helper 必须同时复刻。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  fillCylinderInputs (*cloud, *normals);
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = cylinderCoefficients ();

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.25);

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, 0.10, public_inliers);
  ASSERT_FALSE (model.error_sqr_dists_.empty ());

  pcl::Indices candidate_inliers;
  model.selectWithinDistanceCandidate (coeffs, 0.0, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;

  EXPECT_TRUE (candidate_inliers.empty ());
  EXPECT_TRUE (candidate_errors.empty ());
}

TEST (SampleConsensusModelCylinder, GetDistancesPublicEntryMatchesProductionStandardHelper)
{
  // getDistancesToModel dense output（连续输出）没有 count/select 的 early gate
  // 短路：每个 index 都要读取 normal 并写一个 double 距离。这个测试保护
  // Phase 030 的 RVV dense store（连续写回）路径与生产 Standard helper 等价。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  fillCylinderInputs (*cloud, *normals);
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double normal_weight = 0.25;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (normal_weight);

  std::vector<double> standard_distances;
  getDistancesStandardDirect (*cloud, *normals, indices, coeffs, normal_weight, standard_distances);

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  ASSERT_EQ (standard_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < standard_distances.size (); ++i)
    EXPECT_NEAR (standard_distances[i], public_distances[i], 1e-5);
}

TEST (SampleConsensusModelCylinder, GetDistancesBenchShapedPublicEntryMatchesStandardHelper)
{
  // 这个 case 使用和 bench 相同的 shuffled adjacent pairs（相邻交换索引）
  // 输入形态，防止 dense distance path（连续距离路径）只在小手工样本上
  // 通过。bench checksum 只保护输出规模，数值等价由这里承担。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  constexpr std::size_t nr_points = 4096;
  fillBenchShapedCylinderInputs (*cloud, *normals, nr_points);
  const pcl::Indices indices = makeShuffledAdjacentIndices (nr_points);
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double normal_weight = 0.25;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (normal_weight);

  std::vector<double> standard_distances;
  getDistancesStandardDirect (*cloud, *normals, indices, coeffs, normal_weight, standard_distances);

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  ASSERT_EQ (standard_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < standard_distances.size (); ++i)
    EXPECT_NEAR (standard_distances[i], public_distances[i], 1e-5);
}

TEST (SampleConsensusModelCylinder, PublicEntryFallsBackWhenNormalsDoNotCoverInput)
{
  // 原 production 标量路径会先检查 weighted euclid early gate（欧氏项早停），
  // 只有可能成为内点时才读取 normal cloud。RVV 路径会按 chunk 预取 normal，
  // 所以 normal 数量不足时必须回退到 Standard helper，保留这个懒读取边界。
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> ();
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>> ();
  pcl::PointXYZ far_point;
  far_point.x = 4.0f;
  far_point.y = -0.3f;
  far_point.z = 0.1f;
  cloud->push_back (far_point);

  pcl::Indices indices = {0};
  const Eigen::VectorXf coeffs = cylinderCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelCylinderDiagnostic<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.25);

  EXPECT_EQ (model.countWithinDistance (coeffs, threshold), 0u);

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  EXPECT_TRUE (public_inliers.empty ());
  EXPECT_TRUE (model.error_sqr_dists_.empty ());
}

TEST (SampleConsensusModelCylinder, AdditionalSourcePointTypesMatchStandardPath)
{
  // Phase 040 覆盖更宽 source stride（源点步长）：PointXYZI 和
  // PointXYZRGB 的 xyz 字段仍由 traits gate（字段特征门控）定位。
  expectPublicEntriesMatchStandardForType<pcl::PointXYZI, pcl::Normal> (8);
  expectPublicEntriesMatchStandardForType<pcl::PointXYZRGB, pcl::Normal> (8);
}

TEST (SampleConsensusModelCylinder, AdditionalNormalPointTypesMatchStandardPath)
{
  // normal cloud（法线点云）可以是只含法线的 pcl::Normal，也可以是
  // 同时带 xyz 的 PointNormal；RVV normal offset 不能假设字段从 0 开始。
  expectPublicEntriesMatchStandardForType<pcl::PointXYZ, pcl::PointNormal> (8);
}

TEST (SampleConsensusModelCylinder, AdditionalPointTypesBenchShapedMatchStandardPath)
{
  // 小样本保护 near-threshold（近阈值）语义；这个 case 使用 bench-shaped
  // shuffled indices（性能样本形态的乱序索引）保护更长 stride 和 dense
  // output 在实际计时输入附近仍与 Standard helper 对齐。
  constexpr std::size_t nr_points = 4096;
  expectPublicEntriesMatchStandardForType<pcl::PointXYZI, pcl::Normal> (nr_points);
  expectPublicEntriesMatchStandardForType<pcl::PointXYZRGB, pcl::Normal> (nr_points);
  expectPublicEntriesMatchStandardForType<pcl::PointXYZ, pcl::PointNormal> (nr_points);
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
