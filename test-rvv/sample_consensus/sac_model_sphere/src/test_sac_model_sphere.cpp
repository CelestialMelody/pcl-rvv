/*
 * sac_model_sphere correctness tests 的源码入口。
 * 夹具、access wrapper 和断言 helper 放在 include/，这里保留 gtest case，
 * 方便 reviewer 先看被测公开入口和断言边界。
 */

#include <test_sac_model_sphere.h>

using namespace pcl_rvv_sphere_test_support;

TEST (SampleConsensusModelSphere, PublicEntriesMatchReferenceOnShellBoundaries)
{
  // 这个测试故意包含内外边界等号点、球壳内部点和空心区域点。
  // 如果 RVV mask 使用严格 < 或 >，边界点会被错误丢弃。
  auto cloud = makeSphereShellCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {7, 4, 0, 2, 6, 1, 5, 3};
  const Eigen::VectorXf coeffs = sphereCoefficients ();

  SampleConsensusModelSphereAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSameSphereOutputs (model, coeffs, 0.25);
}

TEST (SampleConsensusModelSphere, PointXYZILayoutMatchesReference)
{
  // 这个 case（用例）证明测试专用候选和已有 count RVV gate 不只覆盖 exact PointXYZ。
  // 它仍不代表所有自定义点型、字段 offset 或 layout 已经被批准。
  auto cloud = makeSphereShellCloud<pcl::PointXYZI> ();
  pcl::Indices indices = {6, 2, 4, 0, 7, 3, 5, 1};
  const Eigen::VectorXf coeffs = sphereCoefficients ();

  SampleConsensusModelSphereAccess<pcl::PointXYZI> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSameSphereOutputs (model, coeffs, 0.25);
}

TEST (SampleConsensusModelSphere, PointXYZRGBAndRGBALayoutsMatchReference)
{
  // RGB/RGBA 点型把颜色字段放在 x/y/z 之后。本测试只证明 registered
  // xyz float layout 的 correctness，不把性能结论外推到所有自定义点型。
  {
    auto cloud = makeSphereShellCloud<pcl::PointXYZRGB> ();
    pcl::Indices indices = {6, 2, 4, 0, 7, 3, 5, 1};
    const Eigen::VectorXf coeffs = sphereCoefficients ();

    SampleConsensusModelSphereAccess<pcl::PointXYZRGB> model (cloud);
    model.setIndices (std::make_shared<std::vector<int>> (indices));

    expectSameSphereOutputs (model, coeffs, 0.25);
  }

  {
    auto cloud = makeSphereShellCloud<pcl::PointXYZRGBA> ();
    pcl::Indices indices = {1, 5, 3, 7, 0, 4, 2, 6};
    const Eigen::VectorXf coeffs = sphereCoefficients ();

    SampleConsensusModelSphereAccess<pcl::PointXYZRGBA> model (cloud);
    model.setIndices (std::make_shared<std::vector<int>> (indices));

    expectSameSphereOutputs (model, coeffs, 0.25);
  }
}

TEST (SampleConsensusModelSphere, DiagnosticCandidateMatchesPublicEntries)
{
  // 本测试把 select 和 getDistances 的测试专用 RVV candidate 与公开入口对拍。
  // 公开入口目前仍是标量；失败说明候选数据流或输出合同不能进入后续生产探针。
  auto cloud = makeSphereShellCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {0, 1, 2, 3, 4, 5, 6, 7};
  const Eigen::VectorXf coeffs = sphereCoefficients ();

  SampleConsensusModelSphereAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, 2.50, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  pcl::Indices candidate_inliers;
  model.selectWithinDistanceCandidate (coeffs, 2.50, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;
  std::vector<double> candidate_distances;
  model.getDistancesToModelCandidate (coeffs, candidate_distances);

  ASSERT_EQ (public_inliers, candidate_inliers);
  ASSERT_EQ (public_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < public_errors.size (); ++i)
    EXPECT_NEAR (public_errors[i], candidate_errors[i], 1e-6);
  ASSERT_EQ (public_distances.size (), candidate_distances.size ());
  for (std::size_t i = 0; i < public_distances.size (); ++i)
    EXPECT_NEAR (public_distances[i], candidate_distances[i], 1e-6);
}

TEST (SampleConsensusModelSphere, ProductionSelectWithinDistanceMatchesStandardHelper)
{
  // 这个测试直接对拍 public entry（公开入口）和显式 Standard helper。
  // RVV 构建下 public entry 会命中新接入的 production dispatch；Std 构建下同一测试覆盖自然 fallback。
  auto cloud = makeSphereShellCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {7, 4, 0, 2, 6, 1, 5, 3};
  const Eigen::VectorXf coeffs = sphereCoefficients ();

  SampleConsensusModelSphereAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices standard_inliers;
  model.selectWithinDistanceStandard (coeffs, 0.25, standard_inliers);
  const std::vector<double> standard_errors = model.error_sqr_dists_;

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, 0.25, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], public_errors[i], 1e-6);

#if defined (__RVV10__)
  pcl::Indices direct_rvv_inliers;
  model.selectWithinDistanceRVV (coeffs, 0.25, direct_rvv_inliers);
  const std::vector<double> direct_rvv_errors = model.error_sqr_dists_;

  ASSERT_EQ (standard_inliers, direct_rvv_inliers);
  ASSERT_EQ (standard_errors.size (), direct_rvv_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], direct_rvv_errors[i], 1e-6);
#endif
}

TEST (SampleConsensusModelSphere, VCompressSelectCandidateMatchesStandardHelper)
{
  // 这个测试只在 RVV 构建下验证 `vcompress` 候选。它是实现族消融，不代表
  // production 已经替换为 vcompress 路径。
#if defined (__RVV10__)
  auto cloud = makeSphereShellCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {7, 4, 0, 2, 6, 1, 5, 3};
  const Eigen::VectorXf coeffs = sphereCoefficients ();

  SampleConsensusModelSphereAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices standard_inliers;
  model.selectWithinDistanceStandard (coeffs, 0.25, standard_inliers);
  const std::vector<double> standard_errors = model.error_sqr_dists_;

  pcl::Indices compressed_inliers;
  model.selectWithinDistanceVCompressCandidate (coeffs, 0.25, compressed_inliers);
  const std::vector<double> compressed_errors = model.error_sqr_dists_;

  ASSERT_EQ (standard_inliers, compressed_inliers);
  ASSERT_EQ (standard_errors.size (), compressed_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], compressed_errors[i], 1e-6);
#else
  SUCCEED ();
#endif
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
