/*
 * 本文件做什么：
 * 这些测试为 SampleConsensusModelStick 建立 RVV diagnostic（诊断）入口。
 * Phase 080 以后，公开入口已经有 production direct（真实生产路径）RVV 分流；
 * candidate（候选实现）测试继续保留为诊断回归，新增 production tests 直接覆盖
 * public entry（公开入口）语义和 fallback（回退路径）边界。
 */

#define SAC_MODEL_STICK_DONT_WARN_DEPRECATED

#include <pcl/test/gtest.h>

#include "impl/sac_model_stick_diagnostic.hpp"

#include <pcl/point_types.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

template <typename PointT>
class SampleConsensusModelStickAccess
  : public pcl::SampleConsensusModelStick<PointT>
{
  using Base = pcl::SampleConsensusModelStick<PointT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::countWithinDistanceStandard;
  using Base::error_sqr_dists_;
  using Base::getDistancesToModel;
  using Base::getDistancesToModelStandard;
  using Base::selectWithinDistance;
  using Base::selectWithinDistanceStandard;
  using Base::setIndices;
};

#if defined (__RVV10__)
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZI>::value);
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGB>::value);
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGBA>::value);
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGBNormal>::value);
#endif

static Eigen::VectorXf
stickCoefficients ()
{
  Eigen::VectorXf coeffs (7);
  coeffs << 1.0f, -2.0f, 0.5f, 3.0f, -0.5f, 1.5f, 0.10f;
  return coeffs;
}

template <typename PointT>
static typename pcl::PointCloud<PointT>::Ptr
makeStickDistanceCloudAs (const std::vector<float>& radial_offsets)
{
  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  cloud->resize (radial_offsets.size ());

  const Eigen::Vector3f p0 (1.0f, -2.0f, 0.5f);
  const Eigen::Vector3f p1 (3.0f, -0.5f, 1.5f);
  Eigen::Vector3f dir = p1 - p0;
  dir.normalize ();
  Eigen::Vector3f n1 (0.0f, 1.0f, -1.0f);
  n1 -= n1.dot (dir) * dir;
  n1.normalize ();
  Eigen::Vector3f n2 = dir.cross (n1);
  n2.normalize ();

  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float along_line = (static_cast<float> (i) - 3.0f) * 0.35f;
    const float side_mix = (i % 2 == 0) ? 0.25f : -0.15f;
    const Eigen::Vector3f pt =
        p0 + along_line * dir + radial_offsets[i] * (n1 + side_mix * n2).normalized ();
    (*cloud)[i].x = pt.x ();
    (*cloud)[i].y = pt.y ();
    (*cloud)[i].z = pt.z ();
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      (*cloud)[i].intensity = static_cast<float> (10 + i);
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGB> ||
                       std::is_same_v<PointT, pcl::PointXYZRGBA>)
      (*cloud)[i].rgba = static_cast<std::uint32_t> (0xff000000u | (i * 33u));
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBNormal>)
    {
      (*cloud)[i].rgba = static_cast<std::uint32_t> (0xff000000u | (i * 33u));
      (*cloud)[i].normal_x = 0.0f;
      (*cloud)[i].normal_y = 0.0f;
      (*cloud)[i].normal_z = 1.0f;
      (*cloud)[i].curvature = 0.0f;
    }
  }
  return cloud;
}

static pcl::PointCloud<pcl::PointXYZ>::Ptr
makeStickDistanceCloud (const std::vector<float>& radial_offsets)
{
  return makeStickDistanceCloudAs<pcl::PointXYZ> (radial_offsets);
}

template <typename PointT>
static void
expectPublicMatchesStandard (SampleConsensusModelStickAccess<PointT>& model,
                             const Eigen::VectorXf& coeffs,
                             const double threshold)
{
  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  const std::size_t standard_count = model.countWithinDistanceStandard (coeffs, threshold);
  EXPECT_EQ (standard_count, public_count);

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  pcl::Indices standard_inliers;
  model.selectWithinDistanceStandard (coeffs, threshold, standard_inliers);
  const std::vector<double> standard_errors = model.error_sqr_dists_;

  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], public_errors[i], 1e-6);

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> standard_distances;
  model.getDistancesToModelStandard (coeffs, standard_distances);

  ASSERT_EQ (standard_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < standard_distances.size (); ++i)
    EXPECT_NEAR (standard_distances[i], public_distances[i], 1e-5);
}

template <typename PointT>
static void
expectPointTypeMatchesStandard ()
{
  auto cloud = makeStickDistanceCloudAs<PointT> (
      {0.00f, 0.03f, 0.06f, 0.09f, 0.12f, 0.15f, 0.19f, 0.23f});
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = stickCoefficients ();

  SampleConsensusModelStickAccess<PointT> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setRadiusLimits (0.0, 0.10);

  expectPublicMatchesStandard (model, coeffs, 0.10);
}

template <typename PointT>
static void
expectSelectCandidateMatchesPublic (pcl_rvv_test::SampleConsensusModelStickDiagnostic<PointT>& model,
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
    EXPECT_NEAR (public_errors[i], candidate_errors[i], 1e-6);
}

template <typename PointT>
static void
expectDistancesCandidateMatchesPublic (const pcl_rvv_test::SampleConsensusModelStickDiagnostic<PointT>& model,
                                       const Eigen::VectorXf& coeffs)
{
  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances;
  model.getDistancesToModelCandidate (coeffs, candidate_distances);

  ASSERT_EQ (public_distances.size (), candidate_distances.size ());
  for (std::size_t i = 0; i < public_distances.size (); ++i)
    EXPECT_NEAR (public_distances[i], candidate_distances[i], 1e-6);
}

TEST (SampleConsensusModelStick, CountCandidateMatchesPublicEntryWithInnerOuterPenalty)
{
  // 这个 case 同时放入内圈、外圈和远外圈样本，保护 stick 特有的
  // `nr_i - nr_o` 语义。普通 line inlier count（直线内点计数）即使公式正确，
  // 也会在这个输入上返回不同结果。
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.03f, 0.05f, 0.07f, 0.11f, 0.13f, 0.16f, 0.21f, 0.23f, 0.30f});
  pcl::Indices indices = {7, 0, 4, 2, 9, 1, 6, 3, 8, 5};
  const Eigen::VectorXf coeffs = stickCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelStickDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  const std::size_t candidate_count = model.countWithinDistanceCandidate (coeffs, threshold);

  EXPECT_EQ (public_count, candidate_count);
}

TEST (SampleConsensusModelStick, CountCandidateReturnsZeroWhenOuterBandDominates)
{
  // 当外圈数量不少于内圈时，公开入口返回 0。这个边界是 stick topic
  // 独有的 correctness gate（正确性验收），不能从 line topic 迁移结论。
  auto cloud = makeStickDistanceCloud (
      {0.01f, 0.04f, 0.11f, 0.12f, 0.14f, 0.16f, 0.22f, 0.24f});
  pcl::Indices indices = {3, 0, 5, 1, 7, 2, 6, 4};
  const Eigen::VectorXf coeffs = stickCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelStickDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  const std::size_t candidate_count = model.countWithinDistanceCandidate (coeffs, threshold);

  EXPECT_EQ (0u, public_count);
  EXPECT_EQ (public_count, candidate_count);
}

TEST (SampleConsensusModelStick, CountPublicEntryPreservesInnerOuterPenalty)
{
  // 这个 production direct case 只调用公开入口。Std/RVV 两个构建都运行它；
  // RVV 构建是否真正命中生产 helper 由 check_production_asm 单独验证。
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.03f, 0.05f, 0.07f, 0.11f, 0.13f, 0.16f, 0.21f, 0.23f, 0.30f});
  pcl::Indices indices = {7, 0, 4, 2, 9, 1, 6, 3, 8, 5};
  const Eigen::VectorXf coeffs = stickCoefficients ();
  constexpr double threshold = 0.10;

  pcl::SampleConsensusModelStick<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  EXPECT_EQ (1u, model.countWithinDistance (coeffs, threshold));
}

TEST (SampleConsensusModelStick, SelectCandidateMatchesPublicEntryAndErrorDistances)
{
  // 这个 case 故意混入命中与未命中点，并打乱 indices 顺序。
  // 如果 RVV select 把输出顺序写成 lane 顺序而不是 indices 扫描顺序，
  // 或者把 error_sqr_dists_ 写错为索引位置，都会在这里暴露。
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.03f, 0.06f, 0.09f, 0.12f, 0.15f, 0.19f, 0.23f, 0.27f, 0.31f});
  pcl::Indices indices = {8, 1, 6, 3, 9, 0, 7, 2, 5, 4};
  const Eigen::VectorXf coeffs = stickCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelStickDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSelectCandidateMatchesPublic (model, coeffs, threshold);
}

TEST (SampleConsensusModelStick, SelectCandidateClearsStaleErrorDistancesWhenNoInliers)
{
  // 这个 case 先用一次有命中的 select 填充 error_sqr_dists_，再切换到
  // 没有命中的阈值。若 candidate 忘记清空 error_sqr_dists_，旧内容会残留。
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.05f, 0.10f, 0.14f, 0.18f, 0.22f, 0.26f, 0.30f});
  pcl::Indices indices = {7, 0, 5, 1, 6, 2, 4, 3};
  const Eigen::VectorXf coeffs = stickCoefficients ();

  pcl_rvv_test::SampleConsensusModelStickDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, 0.10, public_inliers);
  ASSERT_FALSE (model.error_sqr_dists_.empty ());

  pcl::Indices candidate_inliers;
  model.selectWithinDistanceCandidate (coeffs, 0.0, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;

  EXPECT_TRUE (candidate_inliers.empty ());
  EXPECT_TRUE (candidate_errors.empty ());
}

TEST (SampleConsensusModelStick, SelectPublicEntryPreservesOrderAndErrorDistances)
{
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.03f, 0.06f, 0.09f, 0.12f, 0.15f, 0.19f, 0.23f, 0.27f, 0.31f});
  pcl::Indices indices = {8, 1, 6, 3, 9, 0, 7, 2, 5, 4};
  const Eigen::VectorXf coeffs = stickCoefficients ();
  constexpr double threshold = 0.10;

  pcl::SampleConsensusModelStick<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices inliers;
  model.selectWithinDistance (coeffs, threshold, inliers);

  const pcl::Indices expected_inliers = {1, 3, 0, 2};
  ASSERT_EQ (expected_inliers, inliers);
  ASSERT_EQ (expected_inliers.size (), model.error_sqr_dists_.size ());
  EXPECT_NEAR (0.0009, model.error_sqr_dists_[0], 1e-5);
  EXPECT_NEAR (0.0081, model.error_sqr_dists_[1], 1e-5);
}

TEST (SampleConsensusModelStick, SelectPublicEntryClearsStaleStateWhenNoInliers)
{
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.05f, 0.10f, 0.14f, 0.18f, 0.22f, 0.26f, 0.30f});
  pcl::Indices indices = {7, 0, 5, 1, 6, 2, 4, 3};
  const Eigen::VectorXf coeffs = stickCoefficients ();

  pcl::SampleConsensusModelStick<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices inliers;
  model.selectWithinDistance (coeffs, 0.10, inliers);
  ASSERT_FALSE (model.error_sqr_dists_.empty ());

  model.selectWithinDistance (coeffs, 0.0, inliers);
  EXPECT_TRUE (inliers.empty ());
  EXPECT_TRUE (model.error_sqr_dists_.empty ());
}

TEST (SampleConsensusModelStick, GetDistancesCandidateMatchesPublicDirectionCoefficientSemantics)
{
  // getDistancesToModel 把系数 3-5 当方向向量，而 count/select 把它们当第二端点。
  // 如果 candidate 复用 count/select 的端点解释，本 case 会和公开入口距离不同。
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.03f, 0.06f, 0.09f, 0.12f, 0.15f, 0.18f, 0.21f});
  pcl::Indices indices = {6, 1, 7, 0, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = stickCoefficients ();

  pcl_rvv_test::SampleConsensusModelStickDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectDistancesCandidateMatchesPublic (model, coeffs);
}

TEST (SampleConsensusModelStick, GetDistancesCandidatePreservesPenaltyAndDenseIndexedOrder)
{
  // 这个 case 设置较小 radius_max_，让公开入口同时产生普通距离和 2x penalty。
  // 输出是 dense distances，位置必须对应 indices_ 扫描顺序，不是原始点云下标。
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.04f, 0.08f, 0.12f, 0.16f, 0.20f, 0.24f, 0.28f});
  pcl::Indices indices = {7, 0, 6, 1, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = stickCoefficients ();

  pcl_rvv_test::SampleConsensusModelStickDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setRadiusLimits (0.0, 0.10);

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);
  ASSERT_EQ (indices.size (), public_distances.size ());
  ASSERT_TRUE (std::any_of (public_distances.begin (), public_distances.end (), [](double value) {
    return value > 0.20;
  }));

  expectDistancesCandidateMatchesPublic (model, coeffs);
}

TEST (SampleConsensusModelStick, GetDistancesPublicEntryPreservesDirectionPenaltyAndOrder)
{
  auto cloud = makeStickDistanceCloud (
      {0.00f, 0.04f, 0.08f, 0.12f, 0.16f, 0.20f, 0.24f, 0.28f});
  pcl::Indices indices = {7, 0, 6, 1, 5, 2, 4, 3};
  const Eigen::VectorXf coeffs = stickCoefficients ();

  pcl::SampleConsensusModelStick<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setRadiusLimits (0.0, 0.10);

  std::vector<double> distances;
  model.getDistancesToModel (coeffs, distances);

  ASSERT_EQ (indices.size (), distances.size ());
  EXPECT_NEAR (2.2016206, distances.front (), 1e-5);
  EXPECT_NEAR (1.4142270, distances[1], 1e-5);
  EXPECT_TRUE (std::any_of (distances.begin (), distances.end (), [](double value) {
    return value > 0.20;
  }));
}

TEST (SampleConsensusModelStick, AdditionalAoSPointTypesMatchStandardPath)
{
  // 当前 production gate 是 traits-based（基于字段特征），不是 exact PointXYZ。
  // 这里用 PCL 显式实例常见点型验证 public entry 和 Standard helper 的三入口输出一致。
  expectPointTypeMatchesStandard<pcl::PointXYZI> ();
  expectPointTypeMatchesStandard<pcl::PointXYZRGB> ();
  expectPointTypeMatchesStandard<pcl::PointXYZRGBA> ();
  expectPointTypeMatchesStandard<pcl::PointXYZRGBNormal> ();
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
