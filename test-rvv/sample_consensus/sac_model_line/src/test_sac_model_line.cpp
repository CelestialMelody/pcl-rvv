/*
 * 本文件做什么：
 * 这些测试为 SampleConsensusModelLine 建立 RVV diagnostic（诊断）入口。
 * 当前测试验证 countWithinDistance、selectWithinDistance 和 getDistancesToModel
 * 的公开入口及测试专用 RVV candidate（候选实现）是否保持同一语义。
 * Phase 040 后，公开入口已经有真实 production dispatch（生产分流）；本测试
 * 仍不证明泛型点类型或其它 row source（行来源）已经覆盖。
 */

#include <pcl/test/gtest.h>

#include "impl/sac_model_line_diagnostic.hpp"

#include <pcl/point_types.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

static Eigen::VectorXf
lineCoefficients ()
{
  Eigen::VectorXf coeffs (6);
  coeffs << 1.0f, -2.0f, 0.5f, 2.0f, 1.0f, -0.5f;
  return coeffs;
}

static pcl::PointCloud<pcl::PointXYZ>::Ptr
makeLineDistanceCloud ()
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);
  cloud->resize (10);

  const Eigen::Vector3f p0 (1.0f, -2.0f, 0.5f);
  Eigen::Vector3f dir (2.0f, 1.0f, -0.5f);
  dir.normalize ();
  Eigen::Vector3f n1 (1.0f, -2.0f, 0.0f);
  n1 -= n1.dot (dir) * dir;
  n1.normalize ();
  Eigen::Vector3f n2 = dir.cross (n1);
  n2.normalize ();

  const float offsets[][2] = {
      {0.00f, 0.00f}, {0.02f, 0.00f}, {0.05f, 0.00f}, {0.08f, 0.00f},
      {0.12f, 0.00f}, {0.00f, 0.07f}, {0.03f, 0.04f}, {0.16f, 0.00f},
      {0.04f, 0.02f}, {0.20f, 0.02f}};

  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float along_line = (static_cast<float> (i) - 4.0f) * 0.35f;
    const Eigen::Vector3f pt =
        p0 + along_line * dir +
        offsets[i][0] * n1 + offsets[i][1] * n2;
    (*cloud)[i].x = pt.x ();
    (*cloud)[i].y = pt.y ();
    (*cloud)[i].z = pt.z ();
  }
  return cloud;
}

static pcl::PointCloud<pcl::PointXYZ>::Ptr
makeLineBenchScaleCloud (const std::size_t nr_points)
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);
  cloud->resize (nr_points);

  const Eigen::Vector3f p0 (1.0f, -2.0f, 0.5f);
  Eigen::Vector3f dir (2.0f, 1.0f, -0.5f);
  dir.normalize ();
  Eigen::Vector3f n1 (1.0f, -2.0f, 0.0f);
  n1 -= n1.dot (dir) * dir;
  n1.normalize ();
  Eigen::Vector3f n2 = dir.cross (n1);
  n2.normalize ();

  for (std::size_t i = 0; i < cloud->size (); ++i)
  {
    const float t = static_cast<float> (i % 8192) / 8192.0f;
    const float along_line =
        (static_cast<float> (i) - static_cast<float> (nr_points / 2)) * 0.001f;
    const float radial_a = 0.12f * std::sin (t * 37.0f);
    const float radial_b = 0.09f * std::cos (t * 29.0f);
    const Eigen::Vector3f pt = p0 + along_line * dir + radial_a * n1 + radial_b * n2;
    (*cloud)[i].x = pt.x ();
    (*cloud)[i].y = pt.y ();
    (*cloud)[i].z = pt.z ();
  }
  return cloud;
}

static void
expectDistancesNear (const std::vector<double>& expected,
                     const std::vector<double>& actual,
                     const double tolerance)
{
  ASSERT_EQ (expected.size (), actual.size ());
  for (std::size_t i = 0; i < expected.size (); ++i)
    EXPECT_NEAR (expected[i], actual[i], tolerance) << "at distance index " << i;
}

TEST (SampleConsensusModelLine, CountCandidateMatchesPublicEntryOnShuffledIndices)
{
  // 这个测试用乱序 indices（索引子集）保护 direct indexed row source
  // （直接索引行来源）。阈值附近样本会暴露 `< threshold^2` 与 `<=` 写错、
  // 非单位方向系数归一化遗漏，以及 RVV float 公式与公开入口不一致的问题。
  auto cloud = makeLineDistanceCloud ();
  pcl::Indices indices = {7, 0, 4, 2, 9, 1, 6, 3, 8, 5};
  const Eigen::VectorXf coeffs = lineCoefficients ();
  constexpr double threshold = 0.08;

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  const std::size_t candidate_count = model.countWithinDistanceCandidate (coeffs, threshold);

  EXPECT_EQ (public_count, candidate_count);
}

TEST (SampleConsensusModelLine, SelectCandidateMatchesPublicEntryOnShuffledIndices)
{
  // selectWithinDistance（筛选阈值内点）比 count 多了输出顺序和
  // error_sqr_dists_（平方距离缓存）语义。这个测试会卡住漏写
  // vcompress（向量压缩）保序、误差数组对应关系或调用前清理的错误。
  auto cloud = makeLineDistanceCloud ();
  pcl::Indices indices = {7, 0, 4, 2, 9, 1, 6, 3, 8, 5};
  const Eigen::VectorXf coeffs = lineCoefficients ();
  constexpr double threshold = 0.08;

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  pcl::Indices public_inliers = {99, 100};
  model.error_sqr_dists_ = {1.0, 2.0};
  model.selectWithinDistance (coeffs, threshold, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;

  pcl::Indices candidate_inliers = {101};
  model.error_sqr_dists_ = {3.0};
  model.selectWithinDistanceCandidate (coeffs, threshold, candidate_inliers);
  const std::vector<double> candidate_errors = model.error_sqr_dists_;

  ASSERT_EQ (public_inliers, candidate_inliers);
  ASSERT_EQ (public_errors.size (), candidate_errors.size ());
  for (std::size_t i = 0; i < public_errors.size (); ++i)
    EXPECT_NEAR (public_errors[i], candidate_errors[i], 1e-6);
}

TEST (SampleConsensusModelLine, SelectCandidateMatchesPublicEntryOnBenchScaleInput)
{
  // 这个 case 固化 Phase 010 board checksum mismatch（校验和不一致）的复现：
  // 小样本通过不够，bench 规模输入必须同样保持 inliers 顺序和误差数组一致。
  constexpr std::size_t nr_points = 65536;
  auto cloud = makeLineBenchScaleCloud (nr_points);
  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);
  const Eigen::VectorXf coeffs = lineCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

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

TEST (SampleConsensusModelLine, GetDistancesCandidateMatchesPublicEntryOnShuffledIndices)
{
  // getDistancesToModel（计算到模型的距离）写 dense distance output
  // （连续距离输出），这里用乱序 indices 保护输出顺序、方向归一化和 sqrt
  // （平方根）语义。候选如果写成平方距离、漏掉 resize 或按点云原顺序输出，
  // 这个测试会失败。
  auto cloud = makeLineDistanceCloud ();
  pcl::Indices indices = {7, 0, 4, 2, 9, 1, 6, 3, 8, 5};
  const Eigen::VectorXf coeffs = lineCoefficients ();

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances = {42.0, 43.0};
  model.getDistancesToModelCandidate (coeffs, candidate_distances);

  expectDistancesNear (public_distances, candidate_distances, 1e-6);
}

TEST (SampleConsensusModelLine, GetDistancesCandidateMatchesPublicEntryOnBenchScaleInput)
{
  // Phase 020 的 bench-scale（性能测试规模）correctness 会覆盖多个 VL chunk
  // （可变向量长度分块）和相邻交换后的 direct indexed row source（直接索引行来源）。
  // 小样本能验证语义，bench 规模能卡住 chunk tail（分块尾段）和 dense store 边界。
  constexpr std::size_t nr_points = 65536;
  auto cloud = makeLineBenchScaleCloud (nr_points);
  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);
  const Eigen::VectorXf coeffs = lineCoefficients ();

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances;
  model.getDistancesToModelCandidate (coeffs, candidate_distances);

  expectDistancesNear (public_distances, candidate_distances, 2e-6);
}

TEST (SampleConsensusModelLine, GetDistancesCandidatePreservesOutputForInvalidModel)
{
  // production 入口在 model coefficients（模型系数）无效时直接返回，不改写
  // distances。candidate 也必须保持这个副作用边界，否则 fallback 语义会变。
  auto cloud = makeLineDistanceCloud ();
  pcl::Indices indices = {0, 1, 2};
  Eigen::VectorXf invalid_coeffs (6);
  invalid_coeffs << 1.0f, -2.0f, 0.5f, 0.0f, 0.0f, 0.0f;

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::vector<double> public_distances = {7.0, 8.0};
  model.getDistancesToModel (invalid_coeffs, public_distances);

  std::vector<double> candidate_distances = {7.0, 8.0};
  model.getDistancesToModelCandidate (invalid_coeffs, candidate_distances);

  EXPECT_EQ (public_distances, candidate_distances);
}

TEST (SampleConsensusModelLine, GetDistancesVFSqrtCandidateMatchesPublicEntryOnBenchScaleInput)
{
  // Phase 030 验证 vfsqrt（RVV 向量平方根）候选是否仍满足公开入口距离语义。
  // 这个测试会卡住平方根前后顺序写回、tail（尾段）处理或 dense output
  // （连续输出）长度不一致。
  constexpr std::size_t nr_points = 65536;
  auto cloud = makeLineBenchScaleCloud (nr_points);
  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  for (std::size_t i = 1; i < indices.size (); i += 4)
    std::swap (indices[i - 1], indices[i]);
  const Eigen::VectorXf coeffs = lineCoefficients ();

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances;
  model.getDistancesToModelVFSqrtCandidate (coeffs, candidate_distances);

  expectDistancesNear (public_distances, candidate_distances, 2e-6);
}

TEST (SampleConsensusModelLine, PublicEntriesPreserveIdentityIndices)
{
  // Phase 070 的 identity-index fast path（恒等索引快速路径）只应该改变
  // RVV load family（加载实现族），不能改变 count/select/getDistances 的公开语义。
  constexpr std::size_t nr_points = 257;
  auto cloud = makeLineBenchScaleCloud (nr_points);
  pcl::Indices indices (nr_points);
  std::iota (indices.begin (), indices.end (), 0);
  const Eigen::VectorXf coeffs = lineCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud, true);
  model.setIndices (std::make_shared<std::vector<int>> (indices));

  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  const std::size_t candidate_count = model.countWithinDistanceCandidate (coeffs, threshold);
  EXPECT_EQ (public_count, candidate_count);

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

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances;
  model.getDistancesToModelVFSqrtCandidate (coeffs, candidate_distances);

  expectDistancesNear (public_distances, candidate_distances, 2e-6);
}

TEST (SampleConsensusModelLine, CloudOnlyIdentityIndicesPreservePublicEntries)
{
  // cloud-only 构造会让 SampleConsensusModel 自动使用整云 identity indices
  // （恒等索引）。这个 case 保护默认整云调用形态，而不是只验证显式 setIndices。
  constexpr std::size_t nr_points = 257;
  auto cloud = makeLineBenchScaleCloud (nr_points);
  const Eigen::VectorXf coeffs = lineCoefficients ();
  constexpr double threshold = 0.10;

  pcl_rvv_test::SampleConsensusModelLineDiagnostic<pcl::PointXYZ> model (cloud, true);

  const std::size_t public_count = model.countWithinDistance (coeffs, threshold);
  const std::size_t candidate_count = model.countWithinDistanceCandidate (coeffs, threshold);
  EXPECT_EQ (public_count, candidate_count);

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

  std::vector<double> public_distances;
  model.getDistancesToModel (coeffs, public_distances);

  std::vector<double> candidate_distances;
  model.getDistancesToModelVFSqrtCandidate (coeffs, candidate_distances);

  expectDistancesNear (public_distances, candidate_distances, 2e-6);
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
