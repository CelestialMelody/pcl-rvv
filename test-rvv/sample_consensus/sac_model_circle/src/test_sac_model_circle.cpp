/*
 * 本文件做什么：
 * 这些测试为 SampleConsensusModelCircle2D 建立独立 RVV topic 的 correctness
 * （正确性）入口。它们覆盖已有 countWithinDistance RVV path（计数 RVV 路径），
 * 并先为 selectWithinDistance production dispatch（生产分流）写 RED 测试：
 * RVV 构建下应能直接调用 protected RVV helper，并和 public entry（公开入口）
 * 与 Standard helper（标量辅助函数）保持 inliers 顺序和精确误差一致。
 */

#include <pcl/test/gtest.h>

#include "sac_model_circle_test_support.h"

#include <pcl/point_types.h>

#include <memory>
#include <type_traits>
#include <vector>

template <typename PointT>
using SampleConsensusModelCircle2DAccess =
    pcl_rvv_test::sac_model_circle::SampleConsensusModelCircleAccess<PointT>;

static Eigen::VectorXf
circleCoefficients ()
{
  Eigen::VectorXf coeffs (3);
  coeffs << 0.25f, -0.20f, 1.00f;
  return coeffs;
}

template <typename PointT>
static typename pcl::PointCloud<PointT>::Ptr
makeCircleDispatchCloud ()
{
  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  const float xy[][2] = {
      {1.25f, -0.20f},  // on the circle
      {0.25f, 0.80f},   // on the circle
      {-0.75f, -0.20f}, // on the circle
      {0.25f, -1.20f},  // on the circle
      {1.31f, -0.20f},  // outside but inside threshold shell
      {1.42f, -0.20f},  // outside threshold
      {0.25f, 0.65f},   // inside but inside threshold shell
      {0.25f, 0.50f},   // inside threshold
      {-0.60f, 0.33f},  // asymmetric interior point
      {1.05f, -0.85f}   // asymmetric shell point
  };
  cloud->resize (sizeof (xy) / sizeof (xy[0]));
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    (*cloud)[i].x = xy[i][0];
    (*cloud)[i].y = xy[i][1];
    (*cloud)[i].z = static_cast<float> (i % 3);
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      (*cloud)[i].intensity = static_cast<float> (10 + i);
  }
  return cloud;
}

template <typename PointT>
static void
expectSameCircleOutputs (SampleConsensusModelCircle2DAccess<PointT>& model,
                         const Eigen::VectorXf& coeffs,
                         const double threshold)
{
  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);

  pcl::Indices standard_inliers;
  model.error_sqr_dists_.clear ();
  model.selectWithinDistanceStandard (coeffs, threshold, standard_inliers);
  const std::vector<double> standard_errors = model.error_sqr_dists_;
  const std::size_t standard_count =
      model.countWithinDistanceStandard (coeffs, threshold);

  ASSERT_EQ (standard_count, public_count);
  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], public_errors[i], 1e-6);
}

TEST (SampleConsensusModelCircle2D, PublicSelectWithinDistanceMatchesDirectRVVForSupportedLayout)
{
  // 乱序 indices（索引）保护输出顺序：RVV mask（掩码）只能改变筛选方式，
  // 不能改变 inlier 写回顺序或误差数组对应关系。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {9, 1, 5, 0, 7, 3, 2, 8, 4, 6};
  const Eigen::VectorXf coeffs = circleCoefficients ();
  constexpr double threshold = 0.08;

  SampleConsensusModelCircle2DAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSameCircleOutputs (model, coeffs, threshold);

#if defined (__RVV10__)
  pcl::Indices rvv_inliers;
  model.error_sqr_dists_.clear ();
  model.selectWithinDistanceRVV (coeffs, threshold, rvv_inliers);
  const std::vector<double> rvv_errors = model.error_sqr_dists_;

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  ASSERT_EQ (model.countWithinDistanceRVV (coeffs, threshold), rvv_inliers.size ());
  ASSERT_EQ (rvv_inliers, public_inliers);
  ASSERT_EQ (rvv_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < rvv_errors.size (); ++i)
    EXPECT_NEAR (rvv_errors[i], public_errors[i], 1e-6);
#endif
}

TEST (SampleConsensusModelCircle2D, SelectFullRVVErrorTailCandidateMatchesDirectRVV)
{
  // Phase 080 只比较 selectWithinDistance 的误差写回实现族。candidate
  // 是测试专用 helper；它必须保持 inlier 顺序，并在 `1e-6` 误差预算内
  // 对齐当前 production RVV baseline（生产 RVV 基线）。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {9, 1, 5, 0, 7, 3, 2, 8, 4, 6};
  const Eigen::VectorXf coeffs = circleCoefficients ();
  constexpr double threshold = 0.08;

  SampleConsensusModelCircle2DAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  pcl::Indices candidate_inliers;
  model.error_sqr_dists_.clear ();
  model.selectWithinDistanceFullRVVErrorTailCandidate (
      coeffs, threshold, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;

  ASSERT_EQ (public_inliers, candidate_inliers);
  ASSERT_EQ (public_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < public_errors.size (); ++i)
    EXPECT_NEAR (public_errors[i], candidate_errors[i], 1e-6);

#if defined (__RVV10__)
  pcl::Indices rvv_inliers;
  model.error_sqr_dists_.clear ();
  model.selectWithinDistanceRVV (coeffs, threshold, rvv_inliers);
  const std::vector<double> rvv_errors = model.error_sqr_dists_;

  ASSERT_EQ (rvv_inliers, candidate_inliers);
  ASSERT_EQ (rvv_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < rvv_errors.size (); ++i)
    EXPECT_NEAR (rvv_errors[i], candidate_errors[i], 1e-6);
#endif
}

TEST (SampleConsensusModelCircle2D, PointXYZILayoutMatchesStandardPath)
{
  // 这个 case 证明 select/count 的 traits gate（点类型字段准入）不只覆盖 exact
  // PointXYZ。它仍只证明 correctness，不给 PointXYZI 写板卡性能结论。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZI> ();
  pcl::Indices indices = {6, 4, 8, 2, 3, 7, 0, 5, 1, 9};
  const Eigen::VectorXf coeffs = circleCoefficients ();

  SampleConsensusModelCircle2DAccess<pcl::PointXYZI> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSameCircleOutputs (model, coeffs, 0.08);
}

TEST (SampleConsensusModelCircle2D, CloudOnlyIdentityIndicesMatchStandardPath)
{
  // cloud-only 构造会生成 identity indices（恒等索引）。本阶段 production
  // 路径仍使用 indexed gather（离散加载），但默认整云入口必须和显式索引路径同样正确。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZ> ();
  const Eigen::VectorXf coeffs = circleCoefficients ();

  SampleConsensusModelCircle2DAccess<pcl::PointXYZ> model (cloud);

  expectSameCircleOutputs (model, coeffs, 0.08);
}

TEST (SampleConsensusModelCircle2D, ExplicitEmptyIndicesMatchStandardPath)
{
  // 显式空 indices（索引子集）保护输出清理语义：public entry、Standard helper
  // 和后续 RVV helper 都不能留下调用前的 inliers 或误差。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZ> ();
  const Eigen::VectorXf coeffs = circleCoefficients ();

  SampleConsensusModelCircle2DAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> ());

  pcl::Indices public_inliers = {3, 4, 5};
  model.error_sqr_dists_ = {1.0, 2.0};
  model.selectWithinDistance (coeffs, 0.08, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, 0.08);

  pcl::Indices standard_inliers = {8, 9};
  model.error_sqr_dists_ = {4.0};
  model.selectWithinDistanceStandard (coeffs, 0.08, standard_inliers);
  const std::vector<double> standard_errors = model.error_sqr_dists_;
  const std::size_t standard_count =
      model.countWithinDistanceStandard (coeffs, 0.08);

  EXPECT_EQ (0u, public_count);
  EXPECT_EQ (standard_count, public_count);
  EXPECT_TRUE (public_inliers.empty ());
  EXPECT_EQ (standard_inliers, public_inliers);
  EXPECT_TRUE (public_errors.empty ());
  EXPECT_EQ (standard_errors, public_errors);
}

TEST (SampleConsensusModelCircle2D, GetDistancesCandidateMatchesPublicPath)
{
  // getDistancesToModel（逐点距离写回）候选仍是 test-only diagnostic：
  // 它必须先证明和公开标量入口逐项一致，不能直接被当成 production dispatch。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {9, 1, 5, 0, 7, 3, 2, 8, 4, 6};
  const Eigen::VectorXf coeffs = circleCoefficients ();

  SampleConsensusModelCircle2DAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances;
  model.getDistancesToModelScalarSqrtCandidate (coeffs, candidate_distances);

  ASSERT_EQ (public_distances.size (), candidate_distances.size ());
  for (std::size_t i = 0; i < public_distances.size (); ++i)
    EXPECT_NEAR (public_distances[i], candidate_distances[i], 1e-6);
}

TEST (SampleConsensusModelCircle2D, GetDistancesFullRVVCandidateMatchesPublicPath)
{
  // Phase 050 的 full-RVV candidate（完整 RVV 候选）把 sqrt、abs 和
  // float-to-double 写回都留在 RVV 链路中。本测试只证明数值能对齐 public
  // getDistancesToModel，不证明 production dispatch 或板卡收益。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {9, 1, 5, 0, 7, 3, 2, 8, 4, 6};
  const Eigen::VectorXf coeffs = circleCoefficients ();

  SampleConsensusModelCircle2DAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances;
  model.getDistancesToModelFullRVVCandidate (coeffs, candidate_distances);

  ASSERT_EQ (public_distances.size (), candidate_distances.size ());
  for (std::size_t i = 0; i < public_distances.size (); ++i)
    EXPECT_NEAR (public_distances[i], candidate_distances[i], 1e-6);
}

TEST (SampleConsensusModelCircle2D, PublicGetDistancesMatchesStandardAndDirectRVV)
{
  // Phase 060 的 production probe（生产探针）要求公开入口真的命中
  // getDistancesToModelRVV，同时 Standard helper 继续作为 fallback 参考链路。
  auto cloud = makeCircleDispatchCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {9, 1, 5, 0, 7, 3, 2, 8, 4, 6};
  const Eigen::VectorXf coeffs = circleCoefficients ();

  SampleConsensusModelCircle2DAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::vector<double> standard_distances;
  model.getDistancesToModelStandard (coeffs, standard_distances);

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  ASSERT_EQ (standard_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < standard_distances.size (); ++i)
    EXPECT_NEAR (standard_distances[i], public_distances[i], 1e-6);

#if defined (__RVV10__)
  std::vector<double> rvv_distances;
  model.getDistancesToModelRVV (coeffs, rvv_distances);

  ASSERT_EQ (standard_distances.size (), rvv_distances.size ());
  for (std::size_t i = 0; i < standard_distances.size (); ++i)
    EXPECT_NEAR (standard_distances[i], rvv_distances[i], 1e-6);
#endif
}
