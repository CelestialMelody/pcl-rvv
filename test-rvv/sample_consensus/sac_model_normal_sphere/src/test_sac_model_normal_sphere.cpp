/*
 * sac_model_normal_sphere correctness tests 的源码入口。
 * 夹具、测试专用候选和断言 helper 放在 include/，这里保留 gtest case，
 * 方便 reviewer 先看公开入口、候选入口和边界样本。
 */

#include <test_sac_model_normal_sphere.h>

using namespace pcl_rvv_normal_sphere_test_support;

namespace
{

template <typename PointT>
void
expectProductionRVVDetailHelpersForPointType ()
{
#if defined (__RVV10__)
  static_assert (pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value);
  static_assert (sizeof (pcl::index_t) == sizeof (std::int32_t));
  static_assert (std::is_signed_v<pcl::index_t>);

  auto cloud = makeLargeNormalSphereCloud<PointT> (32);
  auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
  pcl::Indices indices = makeShuffledNormalSphereIndices (cloud->size ());
  ASSERT_EQ (32u, indices.size ());
  ASSERT_TRUE (pcl::detail::canUseRVVNormalSphere (*cloud, *normals, indices));
  const Eigen::VectorXf coeffs = normalSphereCoefficients ();

  SampleConsensusModelNormalSphereAccess<PointT, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.35);

  pcl::Indices reference_inliers;
  model.selectWithinDistanceScalarReference (coeffs, 0.22, reference_inliers);
  const std::vector<double> reference_errors = model.error_sqr_dists_;
  const std::size_t reference_count =
      model.countWithinDistanceScalarReference (coeffs, 0.22);
  std::vector<double> reference_distances;
  model.getDistancesToModelScalarReference (coeffs, reference_distances);

  std::size_t rvv_count = 0;
  const bool count_hit = pcl::detail::countWithinDistanceRVVNormalSphere<PointT, pcl::Normal> (
      *cloud, *normals, indices, coeffs, 0.22, 0.35, rvv_count);
  ASSERT_TRUE (count_hit);
  EXPECT_EQ (reference_count, rvv_count);

  pcl::Indices rvv_inliers;
  std::vector<double> rvv_errors;
  const bool select_hit = pcl::detail::selectWithinDistanceRVVNormalSphere<PointT, pcl::Normal> (
      *cloud, *normals, indices, coeffs, 0.22, 0.35, rvv_inliers, rvv_errors);
  ASSERT_TRUE (select_hit);
  EXPECT_EQ (reference_inliers, rvv_inliers);
  ASSERT_EQ (reference_errors.size (), rvv_errors.size ());
  for (std::size_t i = 0; i < reference_errors.size (); ++i)
    EXPECT_NEAR (reference_errors[i], rvv_errors[i], 2e-4);

  std::vector<double> rvv_distances;
  const bool distances_hit = pcl::detail::getDistancesToModelRVVNormalSphere<PointT, pcl::Normal> (
      *cloud, *normals, indices, coeffs, 0.35, rvv_distances);
  ASSERT_TRUE (distances_hit);
  ASSERT_EQ (reference_distances.size (), rvv_distances.size ());
  for (std::size_t i = 0; i < reference_distances.size (); ++i)
    EXPECT_NEAR (reference_distances[i], rvv_distances[i], 2e-4);
#endif
}

template <typename PointT>
void
expectProductionPublicDispatchForPointType ()
{
  auto cloud = makeLargeNormalSphereCloud<PointT> (32);
  auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
  pcl::Indices indices = makeShuffledNormalSphereIndices (cloud->size ());
  const Eigen::VectorXf coeffs = normalSphereCoefficients ();

  SampleConsensusModelNormalSphereAccess<PointT, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.35);

  expectPublicNormalSphereOutputsNearReference (model, coeffs, 0.22, 2e-4);
}

} // namespace

TEST (SampleConsensusModelNormalSphere, PublicEntriesMatchReferenceOnNormalBoundaries)
{
  // 本测试覆盖球壳距离、normal angle（法线夹角）和 early continue（欧氏距离先验跳过）
  // 同时存在的样本。失败说明测试专用候选不能保持 production 标量入口的阈值语义。
  auto cloud = makeNormalSphereCloud<pcl::PointXYZ> ();
  auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
  pcl::Indices indices = {7, 4, 0, 2, 6, 1, 5, 3};
  const Eigen::VectorXf coeffs = normalSphereCoefficients ();

  SampleConsensusModelNormalSphereAccess<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.35);

  expectSameNormalSphereOutputs (model, coeffs, 0.22);
}

TEST (SampleConsensusModelNormalSphere, PointXYZILayoutMatchesReference)
{
  // `PointXYZI` 证明 source cloud（源点云）的 x/y/z AoS layout（结构数组布局）
  // 可以和独立 normal cloud 一起进入诊断候选；它不代表所有自定义点型已经被批准。
  auto cloud = makeNormalSphereCloud<pcl::PointXYZI> ();
  auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
  pcl::Indices indices = {6, 2, 4, 0, 7, 3, 5, 1};
  const Eigen::VectorXf coeffs = normalSphereCoefficients ();

  SampleConsensusModelNormalSphereAccess<pcl::PointXYZI, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.35);

  expectSameNormalSphereOutputs (model, coeffs, 0.22);
}

TEST (SampleConsensusModelNormalSphere, PointXYZRGBAndRGBALayoutsMatchReference)
{
  // RGB/RGBA 点型的颜色字段不参与 normal-sphere 距离公式。本测试只证明
  // registered xyz float AoS layout（注册的 xyz float 结构数组布局）能和独立
  // `pcl::Normal` cloud 一起进入测试专用候选，不代表所有自定义点型已覆盖。
  {
    auto cloud = makeNormalSphereCloud<pcl::PointXYZRGB> ();
    auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
    pcl::Indices indices = {6, 2, 4, 0, 7, 3, 5, 1};
    const Eigen::VectorXf coeffs = normalSphereCoefficients ();

    SampleConsensusModelNormalSphereAccess<pcl::PointXYZRGB, pcl::Normal> model (cloud);
    model.setInputNormals (normals);
    model.setIndices (std::make_shared<std::vector<int>> (indices));
    model.setNormalDistanceWeight (0.35);

    expectSameNormalSphereOutputs (model, coeffs, 0.22);
  }

  {
    auto cloud = makeNormalSphereCloud<pcl::PointXYZRGBA> ();
    auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
    pcl::Indices indices = {1, 5, 3, 7, 0, 4, 2, 6};
    const Eigen::VectorXf coeffs = normalSphereCoefficients ();

    SampleConsensusModelNormalSphereAccess<pcl::PointXYZRGBA, pcl::Normal> model (cloud);
    model.setInputNormals (normals);
    model.setIndices (std::make_shared<std::vector<int>> (indices));
    model.setNormalDistanceWeight (0.35);

    expectSameNormalSphereOutputs (model, coeffs, 0.22);
  }
}

TEST (SampleConsensusModelNormalSphere, DegenerateCenterDirectionKeepsScalarBoundary)
{
  // 点落在球心时，`n_dir = p - center` 的方向退化。标量 `getAngle3D`
  // 不把它修正成有效法线，本测试保护 RVV 候选不要扩大 inlier 语义。
  auto cloud = makeNormalSphereCloud<pcl::PointXYZ> ();
  (*cloud)[5].x = 1.0f;
  (*cloud)[5].y = -2.0f;
  (*cloud)[5].z = 0.5f;
  auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
  pcl::Indices indices = {5, 0, 1, 2, 3, 4, 6, 7};
  const Eigen::VectorXf coeffs = normalSphereCoefficients ();

  SampleConsensusModelNormalSphereAccess<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.95);

  expectSameNormalSphereOutputs (model, coeffs, 4.0);
}

TEST (SampleConsensusModelNormalSphere, VCompressSelectCandidatePreservesOrderAndErrors)
{
  // 这个测试只在 RVV 构建下验证 `vcompress`（RVV 保序压缩）实现族。
  // 它证明 select 输出顺序和 double error 写回仍与标量参考一致，但不代表 production
  // 公开入口已经切换到该候选。
#if defined (__RVV10__)
  auto cloud = makeNormalSphereCloud<pcl::PointXYZ> ();
  auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
  pcl::Indices indices = {7, 4, 0, 2, 6, 1, 5, 3};
  const Eigen::VectorXf coeffs = normalSphereCoefficients ();

  SampleConsensusModelNormalSphereAccess<pcl::PointXYZ, pcl::Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.35);

  pcl::Indices reference_inliers;
  model.selectWithinDistanceScalarReference (coeffs, 0.22, reference_inliers);
  const std::vector<double> reference_errors = model.error_sqr_dists_;

  pcl::Indices compressed_inliers;
  model.selectWithinDistanceVCompressCandidate (coeffs, 0.22, compressed_inliers);
  const std::vector<double> compressed_errors = model.error_sqr_dists_;

  ASSERT_EQ (reference_inliers, compressed_inliers);
  ASSERT_EQ (reference_errors.size (), compressed_errors.size ());
  for (std::size_t i = 0; i < reference_errors.size (); ++i)
    EXPECT_NEAR (reference_errors[i], compressed_errors[i], 1e-4);
#else
  SUCCEED ();
#endif
}

TEST (SampleConsensusModelNormalSphere, ProductionRVVDetailHelpersMatchPublicReference)
{
  // 这个测试是 Phase 060 的 production direct（真实生产路径证据）RED gate。
  // 它要求 production 内部 RVV helper 在四种代表性 source 点型、真实 indices 和真实 normal cloud
  // 上返回 true，并与公开入口标量参考保持输出一致。若 helper 不存在、未接入或
  // fallback 到标量，本测试会失败，而不是把未命中的 production RVV 当成通过。
#if defined (__RVV10__)
  expectProductionRVVDetailHelpersForPointType<pcl::PointXYZ> ();
  expectProductionRVVDetailHelpersForPointType<pcl::PointXYZI> ();
  expectProductionRVVDetailHelpersForPointType<pcl::PointXYZRGB> ();
  expectProductionRVVDetailHelpersForPointType<pcl::PointXYZRGBA> ();
#else
  SUCCEED ();
#endif
}

TEST (SampleConsensusModelNormalSphere, ProductionPublicEntriesMatchReferenceOnLargeInputs)
{
  // 这个测试走真实 public entry（公开入口），不是直接调用 detail helper。
  // RVV 构建下它覆盖大输入 dispatch；Std 构建下同一断言保护抽出的标量 fallback。
  expectProductionPublicDispatchForPointType<pcl::PointXYZ> ();
  expectProductionPublicDispatchForPointType<pcl::PointXYZI> ();
  expectProductionPublicDispatchForPointType<pcl::PointXYZRGB> ();
  expectProductionPublicDispatchForPointType<pcl::PointXYZRGBA> ();
}

TEST (SampleConsensusModelNormalSphere, ProductionFallbacksKeepPublicReferenceSemantics)
{
  // 非 `pcl::Normal` normal 点型和小规模输入都必须自然回退标量路径。
  // 这保护 Phase 060 的窄 production gate，避免把代表点型证据误写成完整泛型覆盖。
  const Eigen::VectorXf coeffs = normalSphereCoefficients ();

  {
    auto cloud = makeLargeNormalSphereCloud<pcl::PointXYZ> (32);
    auto normals = makeNormalSphereNormals<pcl::PointNormal> (*cloud);
    pcl::Indices indices = makeShuffledNormalSphereIndices (cloud->size ());
#if defined (__RVV10__)
    ASSERT_FALSE (pcl::detail::canUseRVVNormalSphere (*cloud, *normals, indices));
#endif
    SampleConsensusModelNormalSphereAccess<pcl::PointXYZ, pcl::PointNormal> model (cloud);
    model.setInputNormals (normals);
    model.setIndices (std::make_shared<std::vector<int>> (indices));
    model.setNormalDistanceWeight (0.35);
    expectPublicNormalSphereOutputsNearReference (model, coeffs, 0.22, 1e-6);
  }

  {
    auto cloud = makeNormalSphereCloud<pcl::PointXYZ> ();
    auto normals = makeNormalSphereNormals<pcl::Normal> (*cloud);
    pcl::Indices indices = makeShuffledNormalSphereIndices (cloud->size ());
#if defined (__RVV10__)
    ASSERT_FALSE (pcl::detail::canUseRVVNormalSphere (*cloud, *normals, indices));
#endif
    SampleConsensusModelNormalSphereAccess<pcl::PointXYZ, pcl::Normal> model (cloud);
    model.setInputNormals (normals);
    model.setIndices (std::make_shared<std::vector<int>> (indices));
    model.setNormalDistanceWeight (0.35);
    expectPublicNormalSphereOutputsNearReference (model, coeffs, 0.22, 1e-6);
  }
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
