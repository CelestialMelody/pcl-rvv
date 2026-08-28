/*
 * 本文件做什么：
 * 这些测试专门覆盖 SampleConsensusModelPlane 的 RVV 生产分流（production dispatch，
 * 真实公开入口选择 RVV 或标量路径）和 protected helper（受保护辅助函数）的直接输出。
 * QEMU 运行只证明 correctness（正确性）和日志形状，不代表板卡性能。
 */

#include <pcl/test/gtest.h>

#include <pcl/point_types.h>
#include <pcl/sample_consensus/sac_model_plane.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <type_traits>
#include <vector>

template <typename PointT>
class SampleConsensusModelPlaneAccess
  : public pcl::SampleConsensusModelPlane<PointT>
{
  using Base = pcl::SampleConsensusModelPlane<PointT>;

public:
  using Base::Base;
  using Base::countWithinDistance;
  using Base::countWithinDistanceStandard;
  using Base::getDistancesToModel;
  using Base::getDistancesToModelStandard;
  using Base::selectWithinDistance;
  using Base::selectWithinDistanceStandard;
  using Base::setIndices;
  using Base::error_sqr_dists_;
#if defined (__RVV10__)
  using Base::countWithinDistanceRVV;
  using Base::getDistancesToModelRVV;
  using Base::selectWithinDistanceRVV;
#endif
};

#if defined (__RVV10__)
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZI>::value);
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGB>::value);
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZRGBA>::value);
static_assert (pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZINormal>::value);
#endif

static Eigen::VectorXf
zPlaneCoefficients ()
{
  Eigen::VectorXf coeffs (4);
  coeffs << 0.0f, 0.0f, 1.0f, 0.0f;
  return coeffs;
}

template <typename PointT>
static typename pcl::PointCloud<PointT>::Ptr
makePlaneDispatchCloud ()
{
  typename pcl::PointCloud<PointT>::Ptr cloud (new pcl::PointCloud<PointT>);
  const double z_values[] = {-0.070, 0.010, 0.020, 0.060, -0.030, 0.049, 0.051, 0.000};
  cloud->resize (sizeof (z_values) / sizeof (z_values[0]));
  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    (*cloud)[i].x = static_cast<float> (i);
    (*cloud)[i].y = static_cast<float> (i % 3);
    (*cloud)[i].z = z_values[i];
    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
      (*cloud)[i].intensity = static_cast<float> (10 + i);
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZINormal>)
    {
      (*cloud)[i].intensity = static_cast<float> (10 + i);
      (*cloud)[i].normal_x = 0.0f;
      (*cloud)[i].normal_y = 0.0f;
      (*cloud)[i].normal_z = 1.0f;
      (*cloud)[i].curvature = 0.0f;
    }
  }
  return cloud;
}

template <typename PointT>
static void
expectSamePlaneOutputs (SampleConsensusModelPlaneAccess<PointT>& model,
                        const Eigen::VectorXf& coeffs,
                        const double threshold)
{
  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  pcl::Indices standard_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t standard_count =
      model.selectWithinDistanceStandard (coeffs, threshold, standard_inliers);
  standard_inliers.resize (standard_count);
  model.error_sqr_dists_.resize (standard_count);
  const std::vector<double> standard_errors = model.error_sqr_dists_;
  std::vector<double> standard_distances;
  model.getDistancesToModelStandard (coeffs, standard_distances);

  ASSERT_EQ (standard_count, public_count);
  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < standard_errors.size (); ++i)
    EXPECT_NEAR (standard_errors[i], public_errors[i], 1e-6);

  ASSERT_EQ (standard_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < standard_distances.size (); ++i)
    EXPECT_NEAR (standard_distances[i], public_distances[i], 1e-6);
}

TEST (SampleConsensusModelPlane, PublicEntriesMatchDirectRVVForSupportedLayout)
{
  // 这个测试验证 public entry（公开入口）和 protected RVV helper 的输出一致。
  // 输入 indices 故意乱序，确保 select 的 vcompress 写回保持生产顺序。
  auto cloud = makePlaneDispatchCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {7, 4, 0, 2, 6, 1, 5, 3};
  const Eigen::VectorXf coeffs = zPlaneCoefficients ();
  constexpr double threshold = 0.05;

  SampleConsensusModelPlaneAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSamePlaneOutputs (model, coeffs, threshold);

#if defined (__RVV10__)
  pcl::Indices rvv_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t rvv_count = model.selectWithinDistanceRVV (coeffs, threshold, rvv_inliers);
  rvv_inliers.resize (rvv_count);
  model.error_sqr_dists_.resize (rvv_count);
  const std::vector<double> rvv_errors = model.error_sqr_dists_;
  std::vector<double> rvv_distances;
  model.getDistancesToModelRVV (coeffs, rvv_distances);

  pcl::Indices public_inliers;
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  ASSERT_EQ (rvv_count, model.countWithinDistanceRVV (coeffs, threshold));
  ASSERT_EQ (rvv_inliers, public_inliers);
  ASSERT_EQ (rvv_errors.size (), public_errors.size ());
  for (std::size_t i = 0; i < rvv_errors.size (); ++i)
    EXPECT_NEAR (rvv_errors[i], public_errors[i], 1e-6);
  ASSERT_EQ (rvv_distances.size (), public_distances.size ());
  for (std::size_t i = 0; i < rvv_distances.size (); ++i)
    EXPECT_NEAR (rvv_distances[i], public_distances[i], 1e-6);
#endif
}

TEST (SampleConsensusModelPlane, PointXYZILayoutMatchesStandardPath)
{
  // 这个测试把 production dispatch（生产分流）扩展到另一个 registered float xyz
  // 点型。它证明 traits gate 不只对 exact PointXYZ 成立，但仍不代表所有自定义点型
  // 和所有布局都已经有板卡性能证据。
  auto cloud = makePlaneDispatchCloud<pcl::PointXYZI> ();
  pcl::Indices indices = {6, 2, 4, 0, 7, 3, 5, 1};
  const Eigen::VectorXf coeffs = zPlaneCoefficients ();

  SampleConsensusModelPlaneAccess<pcl::PointXYZI> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSamePlaneOutputs (model, coeffs, 0.05);
}

template <typename PointT>
static void
expectPointTypeMatchesStandardForIndices (const pcl::Indices& indices)
{
  auto cloud = makePlaneDispatchCloud<PointT> ();
  const Eigen::VectorXf coeffs = zPlaneCoefficients ();

  SampleConsensusModelPlaneAccess<PointT> model (cloud);
  if (!indices.empty ())
    model.setIndices (std::make_shared<std::vector<int>> (indices));

  expectSamePlaneOutputs (model, coeffs, 0.05);
}

TEST (SampleConsensusModelPlane, AdditionalAoSPointTypesMatchStandardPath)
{
  // 这个测试覆盖常见 registered float xyz AoS 点型。当前算法只读取 x/y/z，
  // 不构造完整 PointT 输出，因此 RGB、intensity 和 normal 字段不参与距离语义。
  const pcl::Indices shuffled_indices = {6, 2, 4, 0, 7, 3, 5, 1};

  expectPointTypeMatchesStandardForIndices<pcl::PointXYZRGB> (shuffled_indices);
  expectPointTypeMatchesStandardForIndices<pcl::PointXYZRGBA> (shuffled_indices);
  expectPointTypeMatchesStandardForIndices<pcl::PointXYZINormal> (shuffled_indices);
}

TEST (SampleConsensusModelPlane, CloudOnlyIdentityIndicesMatchStandardPath)
{
  // cloud-only 构造会生成 identity indices（恒等索引）。这个 case 保护默认整云
  // 公开入口，避免后续把 identity fast path（恒等索引快速路径）只留在 bench 中验证。
  auto cloud = makePlaneDispatchCloud<pcl::PointXYZ> ();
  const Eigen::VectorXf coeffs = zPlaneCoefficients ();

  SampleConsensusModelPlaneAccess<pcl::PointXYZ> model (cloud);

  expectSamePlaneOutputs (model, coeffs, 0.05);
}

TEST (SampleConsensusModelPlane, ExplicitEmptyIndicesMatchStandardPath)
{
  // 显式空 indices（索引子集）和不调用 setIndices 的默认整云路径不同。
  // 这个 case 保护 public entry（公开入口）在空子集下不会读取点云，也不会留下旧输出。
  auto cloud = makePlaneDispatchCloud<pcl::PointXYZ> ();
  const Eigen::VectorXf coeffs = zPlaneCoefficients ();

  SampleConsensusModelPlaneAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> ());

  pcl::Indices public_inliers = {3, 4, 5};
  model.error_sqr_dists_ = {1.0, 2.0};
  model.selectWithinDistance (coeffs, 0.05, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (coeffs, 0.05);
  std::vector<double> public_distances = {9.0};
  model.getDistancesToModel (coeffs, public_distances);

  pcl::Indices standard_inliers = {8, 9};
  model.error_sqr_dists_ = {4.0};
  const std::size_t standard_count =
      model.selectWithinDistanceStandard (coeffs, 0.05, standard_inliers);
  standard_inliers.resize (standard_count);
  model.error_sqr_dists_.resize (standard_count);
  const std::vector<double> standard_errors = model.error_sqr_dists_;
  std::vector<double> standard_distances = {7.0};
  model.getDistancesToModelStandard (coeffs, standard_distances);

  EXPECT_EQ (0u, public_count);
  EXPECT_EQ (standard_count, public_count);
  EXPECT_TRUE (public_inliers.empty ());
  EXPECT_EQ (standard_inliers, public_inliers);
  EXPECT_TRUE (public_errors.empty ());
  EXPECT_EQ (standard_errors, public_errors);
  EXPECT_TRUE (public_distances.empty ());
  EXPECT_EQ (standard_distances, public_distances);
}

TEST (SampleConsensusModelPlane, AdditionalAoSPointTypesIdentityMatchStandardPath)
{
  // identity indices（恒等索引）会命中 select/count 的 strided load 分支。
  // 这里只证明代表性点型的 correctness，不给这些点型写板卡性能结论。
  expectPointTypeMatchesStandardForIndices<pcl::PointXYZI> ({});
  expectPointTypeMatchesStandardForIndices<pcl::PointXYZRGB> ({});
  expectPointTypeMatchesStandardForIndices<pcl::PointXYZRGBA> ({});
  expectPointTypeMatchesStandardForIndices<pcl::PointXYZINormal> ({});
}

TEST (SampleConsensusModelPlane, SelectHelperResizesEmptyOutputBuffers)
{
  // 直接调用 helper 时，空 inliers 和 error_sqr_dists_ 也必须先 resize 再写入。
  // 这保护后续 scalar tail（标量尾段）复用和测试 wrapper，不依赖 public entry 预分配。
  auto cloud = makePlaneDispatchCloud<pcl::PointXYZ> ();
  pcl::Indices indices = {0, 1, 2, 3};
  const Eigen::VectorXf coeffs = zPlaneCoefficients ();

  SampleConsensusModelPlaneAccess<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices standard_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t standard_count =
      model.selectWithinDistanceStandard (coeffs, 0.08, standard_inliers);
  ASSERT_EQ (indices.size (), standard_count);
  ASSERT_EQ (indices.size (), standard_inliers.size ());
  ASSERT_EQ (indices.size (), model.error_sqr_dists_.size ());

#if defined (__RVV10__)
  pcl::Indices rvv_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t rvv_count = model.selectWithinDistanceRVV (coeffs, 0.08, rvv_inliers);
  ASSERT_EQ (indices.size (), rvv_count);
  ASSERT_EQ (indices.size (), rvv_inliers.size ());
  ASSERT_EQ (indices.size (), model.error_sqr_dists_.size ());
#endif
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
