/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）先建立 RoPS（Rotational Projection
 * Statistics，旋转投影统计）descriptor component（描述子组件）的
 * same-chain（同构链路）正确性边界。本阶段只测 central moments（中心矩）
 * helper，不碰 production（生产源码），也不证明完整 `ROPSEstimation::compute()`
 * 已经命中 RVV。
 *
 * 证据边界：
 * production 私有 `computeCentralMoments()` 只作为 oracle（参考真值）使用。
 * test-only RVV candidate 若通过，只能证明分布矩阵后的中心矩组件可复刻
 * 标量语义；projection、distribution matrix、LRF 和完整 descriptor 仍需后续
 * phase 单独验证。
 */

#include "rops_estimation.h"

#include <Eigen/Core>

#include <pcl/register_point_struct.h>

#define PCL_RVV_ROPS_ENABLE_TEST_TRACE
#define private public
#include <pcl/features/rops_estimation.h>
#undef private

#include <gtest/gtest.h>
#include <pcl/search/kdtree.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace rops = pcl::features::rvv_test::rops;

struct RopsXYZDouble {
  double x;
  double y;
  double z;

  Eigen::Vector3f
  getVector3fMap() const
  {
    return Eigen::Vector3f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
  }
};

POINT_CLOUD_REGISTER_POINT_STRUCT(RopsXYZDouble,
                                  (double, x, x)
                                  (double, y, y)
                                  (double, z, z))

#if defined(__RVV10__) && defined(PCL_RVV_ROPS_ENABLE_TEST_TRACE)
extern "C" {
std::size_t pcl_rvv_rops_rotate_cloud_trace_hits = 0;
std::size_t pcl_rvv_rops_distribution_matrix_trace_hits = 0;
}
#endif

namespace {

pcl::PointCloud<pcl::PointXYZ>
makeDistributionCloud()
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 9;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(cloud.width);
  for (std::size_t i = 0; i < cloud.points.size(); ++i) {
    cloud.points[i].x = -1.0f + 0.25f * static_cast<float>(i);
    cloud.points[i].y = 0.5f + 0.375f * static_cast<float>((i * 3) % 7);
    cloud.points[i].z = -0.75f + 0.5f * static_cast<float>((i * 5) % 9);
  }
  cloud.points.back().x = 1.0f;
  cloud.points.back().y = 2.0f;
  cloud.points.back().z = 1.5f;
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>
makeRotateCloud()
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = 37;
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(cloud.width);
  for (std::size_t i = 0; i < cloud.points.size(); ++i) {
    const auto f = static_cast<float>(i);
    cloud.points[i].x = -1.25f + 0.071f * f;
    cloud.points[i].y = 0.85f - 0.043f * static_cast<float>((i * 5) % 23);
    cloud.points[i].z = -0.60f + 0.097f * static_cast<float>((i * 7) % 29);
  }
  return cloud;
}

std::vector<pcl::Vertices>
makeFanTriangles(const std::size_t points)
{
  std::vector<pcl::Vertices> triangles;
  if (points < 3)
    return triangles;
  triangles.reserve(points - 2);
  for (std::size_t i = 1; i + 1 < points; ++i) {
    pcl::Vertices triangle;
    triangle.vertices = {0, static_cast<int>(i), static_cast<int>(i + 1)};
    triangles.emplace_back(triangle);
  }
  return triangles;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePublicRopsSurface(const std::size_t points)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(points);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(points);
  for (std::size_t i = 0; i < points; ++i) {
    const float fi = static_cast<float>(i);
    cloud->points[i].x = std::cos(fi * 0.31f) + 0.031f * fi;
    cloud->points[i].y = std::sin(fi * 0.29f) + 0.017f * static_cast<float>((i * 7) % 11);
    cloud->points[i].z = 0.11f * static_cast<float>((i * 5) % 13) + 0.002f * fi;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>
makeBenchShapedRotateCloud(const std::size_t points);

template <typename PointT>
pcl::PointCloud<PointT>
makeBenchShapedRotateCloudAs(const std::size_t points)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(points);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(points);
  for (std::size_t i = 0; i < points; ++i) {
    const float fi = static_cast<float>(i);
    cloud.points[i].x = -1.0f + 2.0f * static_cast<float>((i * 17) % 1024) / 1023.0f;
    cloud.points[i].y = 0.5f + 2.25f * static_cast<float>((i * 31 + 7) % 2048) / 2047.0f;
    cloud.points[i].z = -0.75f + 4.0f * static_cast<float>((i * 47 + 13) % 4096) / 4095.0f;
    cloud.points[i].x += 0.00003f * std::sin(fi * 0.013f);
    cloud.points[i].y += 0.00002f * std::cos(fi * 0.017f);
  }
  if (!cloud.points.empty()) {
    cloud.points.front().x = -1.0f;
    cloud.points.front().y = 0.5f;
    cloud.points.front().z = -0.75f;
    cloud.points.back().x = 1.0f;
    cloud.points.back().y = 2.75f;
    cloud.points.back().z = 3.25f;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>
makeBenchShapedRotateCloud(const std::size_t points)
{
  return makeBenchShapedRotateCloudAs<pcl::PointXYZ>(points);
}

std::vector<float>
computeProductionCentralMoments(const Eigen::MatrixXf& matrix)
{
  pcl::ROPSEstimation<pcl::PointXYZ, pcl::Histogram<135>> estimator;
  estimator.setNumberOfPartitionBins(static_cast<unsigned int>(matrix.rows()));
  std::vector<float> moments;
  estimator.computeCentralMoments(matrix, moments);
  return moments;
}

Eigen::MatrixXf
computeProductionDistributionMatrix(const unsigned int projection,
                                    const Eigen::Vector3f& min,
                                    const Eigen::Vector3f& max,
                                    const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                    const unsigned int bins)
{
  pcl::ROPSEstimation<pcl::PointXYZ, pcl::Histogram<135>> estimator;
  estimator.setNumberOfPartitionBins(bins);
  Eigen::MatrixXf matrix(bins, bins);
  estimator.getDistributionMatrix(projection, min, max, cloud, matrix);
  return matrix;
}

void
computeProductionRotateCloud(const pcl::PointXYZ& axis,
                             const float angle,
                             const pcl::PointCloud<pcl::PointXYZ>& cloud,
                             pcl::PointCloud<pcl::PointXYZ>& rotated_cloud,
                             Eigen::Vector3f& min,
                             Eigen::Vector3f& max)
{
  pcl::ROPSEstimation<pcl::PointXYZ, pcl::Histogram<135>> estimator;
  estimator.rotateCloud(axis, angle, cloud, rotated_cloud, min, max);
}

template <typename PointT>
void
computeProductionRotateCloudTyped(const PointT& axis,
                                  const float angle,
                                  const pcl::PointCloud<PointT>& cloud,
                                  pcl::PointCloud<PointT>& rotated_cloud,
                                  Eigen::Vector3f& min,
                                  Eigen::Vector3f& max)
{
  pcl::ROPSEstimation<PointT, pcl::Histogram<135>> estimator;
  estimator.rotateCloud(axis, angle, cloud, rotated_cloud, min, max);
}

template <typename PointT>
Eigen::MatrixXf
computeProductionDistributionMatrixTyped(const unsigned int projection,
                                         const Eigen::Vector3f& min,
                                         const Eigen::Vector3f& max,
                                         const pcl::PointCloud<PointT>& cloud,
                                         const unsigned int bins)
{
  pcl::ROPSEstimation<PointT, pcl::Histogram<135>> estimator;
  estimator.setNumberOfPartitionBins(bins);
  Eigen::MatrixXf matrix(bins, bins);
  estimator.getDistributionMatrix(projection, min, max, cloud, matrix);
  return matrix;
}

std::array<Eigen::MatrixXf, 3>
computeProductionRotateDistributionPipeline(const pcl::PointXYZ& axis,
                                            const float angle,
                                            const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                            const unsigned int bins,
                                            pcl::PointCloud<pcl::PointXYZ>& rotated_cloud,
                                            Eigen::Vector3f& min,
                                            Eigen::Vector3f& max)
{
  pcl::ROPSEstimation<pcl::PointXYZ, pcl::Histogram<135>> estimator;
  estimator.setNumberOfPartitionBins(bins);
  estimator.rotateCloud(axis, angle, cloud, rotated_cloud, min, max);

  std::array<Eigen::MatrixXf, 3> matrices;
  for (unsigned int projection = 0; projection < matrices.size(); ++projection) {
    matrices[projection].resize(bins, bins);
    estimator.getDistributionMatrix(projection, min, max, rotated_cloud, matrices[projection]);
  }
  return matrices;
}

void
expectMomentsNear(const std::vector<float>& actual, const std::vector<float>& expected)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], 1.0e-5f) << "moment=" << i;
}

void
expectMatrixNear(const Eigen::MatrixXf& actual, const Eigen::MatrixXf& expected)
{
  ASSERT_EQ(actual.rows(), expected.rows());
  ASSERT_EQ(actual.cols(), expected.cols());
  for (int i = 0; i < actual.rows(); ++i)
    for (int j = 0; j < actual.cols(); ++j)
      EXPECT_NEAR(actual(i, j), expected(i, j), 1.0e-6f) << "cell=(" << i << "," << j << ")";
}

void
expectCloudAndBoundsNear(const pcl::PointCloud<pcl::PointXYZ>& actual_cloud,
                         const Eigen::Vector3f& actual_min,
                         const Eigen::Vector3f& actual_max,
                         const pcl::PointCloud<pcl::PointXYZ>& expected_cloud,
                         const Eigen::Vector3f& expected_min,
                         const Eigen::Vector3f& expected_max)
{
  ASSERT_EQ(actual_cloud.size(), expected_cloud.size());
  for (std::size_t i = 0; i < actual_cloud.size(); ++i) {
    EXPECT_NEAR(actual_cloud[i].x, expected_cloud[i].x, 2.0e-5f) << "point=" << i << " x";
    EXPECT_NEAR(actual_cloud[i].y, expected_cloud[i].y, 2.0e-5f) << "point=" << i << " y";
    EXPECT_NEAR(actual_cloud[i].z, expected_cloud[i].z, 2.0e-5f) << "point=" << i << " z";
  }

  for (int axis = 0; axis < 3; ++axis) {
    EXPECT_NEAR(actual_min(axis), expected_min(axis), 2.0e-5f) << "min axis=" << axis;
    EXPECT_NEAR(actual_max(axis), expected_max(axis), 2.0e-5f) << "max axis=" << axis;
  }
}

template <typename ActualPointT, typename ExpectedPointT>
void
expectXYZCloudAndBoundsNear(const pcl::PointCloud<ActualPointT>& actual_cloud,
                            const Eigen::Vector3f& actual_min,
                            const Eigen::Vector3f& actual_max,
                            const pcl::PointCloud<ExpectedPointT>& expected_cloud,
                            const Eigen::Vector3f& expected_min,
                            const Eigen::Vector3f& expected_max,
                            const float tolerance)
{
  ASSERT_EQ(actual_cloud.size(), expected_cloud.size());
  for (std::size_t i = 0; i < actual_cloud.size(); ++i) {
    EXPECT_NEAR(actual_cloud[i].x, expected_cloud[i].x, tolerance) << "point=" << i << " x";
    EXPECT_NEAR(actual_cloud[i].y, expected_cloud[i].y, tolerance) << "point=" << i << " y";
    EXPECT_NEAR(actual_cloud[i].z, expected_cloud[i].z, tolerance) << "point=" << i << " z";
  }

  for (int axis = 0; axis < 3; ++axis) {
    EXPECT_NEAR(actual_min(axis), expected_min(axis), tolerance) << "min axis=" << axis;
    EXPECT_NEAR(actual_max(axis), expected_max(axis), tolerance) << "max axis=" << axis;
  }
}

void
expectCloudAndBoundsWithinBenchTolerance(const pcl::PointCloud<pcl::PointXYZ>& actual_cloud,
                                         const Eigen::Vector3f& actual_min,
                                         const Eigen::Vector3f& actual_max,
                                         const pcl::PointCloud<pcl::PointXYZ>& expected_cloud,
                                         const Eigen::Vector3f& expected_min,
                                         const Eigen::Vector3f& expected_max)
{
  ASSERT_EQ(actual_cloud.size(), expected_cloud.size());
  float max_abs_error = 0.0f;
  for (std::size_t i = 0; i < actual_cloud.size(); ++i) {
    max_abs_error = std::max(max_abs_error, std::abs(actual_cloud[i].x - expected_cloud[i].x));
    max_abs_error = std::max(max_abs_error, std::abs(actual_cloud[i].y - expected_cloud[i].y));
    max_abs_error = std::max(max_abs_error, std::abs(actual_cloud[i].z - expected_cloud[i].z));
  }
  for (int axis = 0; axis < 3; ++axis) {
    max_abs_error = std::max(max_abs_error, std::abs(actual_min(axis) - expected_min(axis)));
    max_abs_error = std::max(max_abs_error, std::abs(actual_max(axis) - expected_max(axis)));
  }
  EXPECT_LE(max_abs_error, 2.0e-5f);
}

template <typename PointT>
void
expectTypedProductionPipelineMatchesPointXYZReference()
{
  const pcl::PointCloud<pcl::PointXYZ> reference_cloud = makeBenchShapedRotateCloud(257);
  const pcl::PointCloud<PointT> typed_cloud = makeBenchShapedRotateCloudAs<PointT>(257);
  const pcl::PointXYZ reference_axis{0.57735026f, 0.57735026f, 0.57735026f};
  PointT typed_axis;
  typed_axis.x = reference_axis.x;
  typed_axis.y = reference_axis.y;
  typed_axis.z = reference_axis.z;
  constexpr unsigned int kBins = 5;

  pcl::PointCloud<pcl::PointXYZ> expected_cloud;
  pcl::PointCloud<PointT> actual_cloud;
  Eigen::Vector3f expected_min;
  Eigen::Vector3f expected_max;
  Eigen::Vector3f actual_min;
  Eigen::Vector3f actual_max;
  rops::rotateCloudStd(reference_axis, 33.0f, reference_cloud, expected_cloud, expected_min, expected_max);
  computeProductionRotateCloudTyped(typed_axis, 33.0f, typed_cloud, actual_cloud, actual_min, actual_max);

  expectXYZCloudAndBoundsNear(
      actual_cloud, actual_min, actual_max, expected_cloud, expected_min, expected_max, 2.0e-5f);

  for (unsigned int projection = 0; projection < 3; ++projection) {
    Eigen::MatrixXf expected_matrix(kBins, kBins);
    rops::getDistributionMatrixStd(
        projection, expected_min, expected_max, expected_cloud, expected_matrix);
    const Eigen::MatrixXf actual_matrix = computeProductionDistributionMatrixTyped(
        projection, actual_min, actual_max, actual_cloud, kBins);
    expectMatrixNear(actual_matrix, expected_matrix);
  }
}

} // namespace

TEST(RopsCentralMomentsRVV, MatchesProductionHelperOnDenseMatrix)
{
  Eigen::MatrixXf matrix(5, 5);
  matrix << 0.02f, 0.00f, 0.06f, 0.00f, 0.04f,
            0.00f, 0.08f, 0.00f, 0.12f, 0.00f,
            0.10f, 0.00f, 0.16f, 0.00f, 0.06f,
            0.00f, 0.14f, 0.00f, 0.10f, 0.00f,
            0.04f, 0.00f, 0.02f, 0.00f, 0.06f;

  const std::vector<float> expected = computeProductionCentralMoments(matrix);
  std::vector<float> actual;
  rops::computeCentralMomentsRVV(matrix, actual);

  expectMomentsNear(actual, expected);
}

TEST(RopsCentralMomentsRVV, HandlesZeroMatrixLikeProductionHelper)
{
  const Eigen::MatrixXf matrix = Eigen::MatrixXf::Zero(5, 5);

  const std::vector<float> expected = computeProductionCentralMoments(matrix);
  std::vector<float> actual;
  rops::computeCentralMomentsRVV(matrix, actual);

  expectMomentsNear(actual, expected);
}

TEST(RopsCentralMomentsRVV, MatchesProductionHelperOnNonDefaultBins)
{
  Eigen::MatrixXf matrix(7, 7);
  for (int i = 0; i < matrix.rows(); ++i)
    for (int j = 0; j < matrix.cols(); ++j)
      matrix(i, j) = static_cast<float>(((i + 3) * (j + 5)) % 17) * 0.001f;

  const std::vector<float> expected = computeProductionCentralMoments(matrix);
  std::vector<float> actual;
  rops::computeCentralMomentsRVV(matrix, actual);

  expectMomentsNear(actual, expected);
}

TEST(RopsDistributionMatrixRVV, MatchesProductionHelperForAllProjections)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeDistributionCloud();
  const Eigen::Vector3f min(-1.0f, 0.5f, -0.75f);
  const Eigen::Vector3f max(1.0f, 2.75f, 3.25f);
  constexpr unsigned int kBins = 5;

  for (unsigned int projection = 0; projection < 3; ++projection) {
    const Eigen::MatrixXf expected =
        computeProductionDistributionMatrix(projection, min, max, cloud, kBins);
    Eigen::MatrixXf actual(kBins, kBins);
    rops::getDistributionMatrixRVV(projection, min, max, cloud, actual);
    expectMatrixNear(actual, expected);
  }
}

TEST(RopsRotateCloudRVV, MatchesProductionHelperForCanonicalAndTiltedAxes)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeRotateCloud();
  const std::vector<pcl::PointXYZ> axes = {
      pcl::PointXYZ{1.0f, 0.0f, 0.0f},
      pcl::PointXYZ{0.0f, 1.0f, 0.0f},
      pcl::PointXYZ{0.0f, 0.0f, 1.0f},
      pcl::PointXYZ{0.57735026f, 0.57735026f, 0.57735026f}};
  const std::vector<float> angles = {22.5f, 45.0f, 67.5f, 33.0f};

  for (std::size_t i = 0; i < axes.size(); ++i) {
    pcl::PointCloud<pcl::PointXYZ> expected_cloud;
    pcl::PointCloud<pcl::PointXYZ> actual_cloud;
    Eigen::Vector3f expected_min;
    Eigen::Vector3f expected_max;
    Eigen::Vector3f actual_min;
    Eigen::Vector3f actual_max;

    computeProductionRotateCloud(
        axes[i], angles[i], cloud, expected_cloud, expected_min, expected_max);
    rops::rotateCloudRVV(axes[i], angles[i], cloud, actual_cloud, actual_min, actual_max);

    expectCloudAndBoundsNear(
        actual_cloud, actual_min, actual_max, expected_cloud, expected_min, expected_max);
  }
}

TEST(RopsRotateCloudRVV, MatchesProductionHelperOnBenchShapedCloud)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeBenchShapedRotateCloud(1024);
  const pcl::PointXYZ axis{0.57735026f, 0.57735026f, 0.57735026f};

  pcl::PointCloud<pcl::PointXYZ> expected_cloud;
  pcl::PointCloud<pcl::PointXYZ> actual_cloud;
  Eigen::Vector3f expected_min;
  Eigen::Vector3f expected_max;
  Eigen::Vector3f actual_min;
  Eigen::Vector3f actual_max;

  computeProductionRotateCloud(axis, 33.0f, cloud, expected_cloud, expected_min, expected_max);
  rops::rotateCloudRVV(axis, 33.0f, cloud, actual_cloud, actual_min, actual_max);

  expectCloudAndBoundsWithinBenchTolerance(
      actual_cloud, actual_min, actual_max, expected_cloud, expected_min, expected_max);
}

TEST(RopsCombinedRotateDistributionRVV, MatchesProductionHelperPipeline)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeBenchShapedRotateCloud(257);
  constexpr unsigned int kBins = 5;
  const std::vector<pcl::PointXYZ> axes = {
      pcl::PointXYZ{1.0f, 0.0f, 0.0f},
      pcl::PointXYZ{0.0f, 1.0f, 0.0f},
      pcl::PointXYZ{0.0f, 0.0f, 1.0f},
      pcl::PointXYZ{0.57735026f, 0.57735026f, 0.57735026f}};
  const std::vector<float> angles = {22.5f, 45.0f, 67.5f, 33.0f};

  for (std::size_t i = 0; i < axes.size(); ++i) {
    pcl::PointCloud<pcl::PointXYZ> expected_cloud;
    pcl::PointCloud<pcl::PointXYZ> actual_cloud;
    Eigen::Vector3f expected_min;
    Eigen::Vector3f expected_max;
    Eigen::Vector3f actual_min;
    Eigen::Vector3f actual_max;
    const std::array<Eigen::MatrixXf, 3> expected_matrices =
        computeProductionRotateDistributionPipeline(
            axes[i], angles[i], cloud, kBins, expected_cloud, expected_min, expected_max);
    std::array<Eigen::MatrixXf, 3> actual_matrices;

    rops::rotateCloudAndDistributionMatricesRVV(
        axes[i], angles[i], cloud, kBins, actual_cloud, actual_min, actual_max, actual_matrices);

    expectCloudAndBoundsWithinBenchTolerance(
        actual_cloud, actual_min, actual_max, expected_cloud, expected_min, expected_max);
    for (std::size_t projection = 0; projection < expected_matrices.size(); ++projection)
      expectMatrixNear(actual_matrices[projection], expected_matrices[projection]);
  }
}

#if defined(__RVV10__) && defined(PCL_RVV_ROPS_ENABLE_TEST_TRACE)
TEST(RopsProductionRVV, ProductionHelpersUseRVVForBenchShapedPointXYZ)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeBenchShapedRotateCloud(257);
  const pcl::PointXYZ axis{0.57735026f, 0.57735026f, 0.57735026f};
  constexpr unsigned int kBins = 5;

  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  pcl::PointCloud<pcl::PointXYZ> rotated_cloud;
  Eigen::Vector3f min;
  Eigen::Vector3f max;
  const std::array<Eigen::MatrixXf, 3> matrices =
      computeProductionRotateDistributionPipeline(axis, 33.0f, cloud, kBins, rotated_cloud, min, max);

  ASSERT_EQ(matrices.size(), 3u);
  EXPECT_GE(pcl_rvv_rops_rotate_cloud_trace_hits, cloud.size());
  EXPECT_GE(pcl_rvv_rops_distribution_matrix_trace_hits, cloud.size() * matrices.size());
}

TEST(RopsProductionRVV, ProductionHelpersMatchScalarReferenceWhenRVVPathIsHit)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeBenchShapedRotateCloud(257);
  const pcl::PointXYZ axis{0.57735026f, 0.57735026f, 0.57735026f};
  constexpr unsigned int kBins = 5;

  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  pcl::PointCloud<pcl::PointXYZ> expected_cloud;
  pcl::PointCloud<pcl::PointXYZ> actual_cloud;
  Eigen::Vector3f expected_min;
  Eigen::Vector3f expected_max;
  Eigen::Vector3f actual_min;
  Eigen::Vector3f actual_max;
  rops::rotateCloudStd(axis, 33.0f, cloud, expected_cloud, expected_min, expected_max);
  computeProductionRotateCloud(axis, 33.0f, cloud, actual_cloud, actual_min, actual_max);

  expectCloudAndBoundsWithinBenchTolerance(
      actual_cloud, actual_min, actual_max, expected_cloud, expected_min, expected_max);
  EXPECT_GE(pcl_rvv_rops_rotate_cloud_trace_hits, cloud.size());

  for (unsigned int projection = 0; projection < 3; ++projection) {
    Eigen::MatrixXf expected_matrix(kBins, kBins);
    Eigen::MatrixXf actual_matrix(kBins, kBins);
    rops::getDistributionMatrixStd(
        projection, expected_min, expected_max, expected_cloud, expected_matrix);
    actual_matrix = computeProductionDistributionMatrix(
        projection, actual_min, actual_max, actual_cloud, kBins);
    expectMatrixNear(actual_matrix, expected_matrix);
  }
  EXPECT_GE(pcl_rvv_rops_distribution_matrix_trace_hits, cloud.size() * 3u);
}

TEST(RopsProductionRVV, SmallCloudFallsBackToScalarPath)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeBenchShapedRotateCloud(8);
  const pcl::PointXYZ axis{1.0f, 0.0f, 0.0f};
  constexpr unsigned int kBins = 5;

  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  pcl::PointCloud<pcl::PointXYZ> expected_cloud;
  pcl::PointCloud<pcl::PointXYZ> actual_cloud;
  Eigen::Vector3f expected_min;
  Eigen::Vector3f expected_max;
  Eigen::Vector3f actual_min;
  Eigen::Vector3f actual_max;
  rops::rotateCloudStd(axis, 22.5f, cloud, expected_cloud, expected_min, expected_max);
  computeProductionRotateCloud(axis, 22.5f, cloud, actual_cloud, actual_min, actual_max);

  expectCloudAndBoundsNear(actual_cloud, actual_min, actual_max, expected_cloud, expected_min, expected_max);
  Eigen::MatrixXf expected_matrix(kBins, kBins);
  Eigen::MatrixXf actual_matrix = computeProductionDistributionMatrix(0, actual_min, actual_max, actual_cloud, kBins);
  rops::getDistributionMatrixStd(0, expected_min, expected_max, expected_cloud, expected_matrix);
  expectMatrixNear(actual_matrix, expected_matrix);
  EXPECT_EQ(pcl_rvv_rops_rotate_cloud_trace_hits, 0u);
  EXPECT_EQ(pcl_rvv_rops_distribution_matrix_trace_hits, 0u);
}

TEST(RopsProductionRVV, NonDenseInputsFallBackAtEachProductionHelper)
{
  pcl::PointCloud<pcl::PointXYZ> cloud = makeBenchShapedRotateCloud(257);
  cloud.is_dense = false;
  const pcl::PointXYZ axis{0.0f, 1.0f, 0.0f};

  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  pcl::PointCloud<pcl::PointXYZ> rotated_cloud;
  Eigen::Vector3f min;
  Eigen::Vector3f max;
  computeProductionRotateCloud(axis, 45.0f, cloud, rotated_cloud, min, max);
  EXPECT_EQ(pcl_rvv_rops_rotate_cloud_trace_hits, 0u);
  EXPECT_FALSE(rotated_cloud.is_dense);

  pcl_rvv_rops_distribution_matrix_trace_hits = 0;
  Eigen::MatrixXf expected_matrix(5, 5);
  rops::getDistributionMatrixStd(0, min, max, rotated_cloud, expected_matrix);
  const Eigen::MatrixXf actual_matrix = computeProductionDistributionMatrix(0, min, max, rotated_cloud, 5);
  expectMatrixNear(actual_matrix, expected_matrix);
  EXPECT_EQ(pcl_rvv_rops_distribution_matrix_trace_hits, 0u);
}

TEST(RopsProductionRVV, NonDenseSurfaceFallsBackThroughPublicCompute)
{
  constexpr std::size_t kPoints = 64;
  auto cloud = makePublicRopsSurface(kPoints);
  cloud->is_dense = false;
  auto indices = pcl::make_shared<pcl::Indices>();
  indices->push_back(0);

  pcl::search::KdTree<pcl::PointXYZ>::Ptr search_method(new pcl::search::KdTree<pcl::PointXYZ>);
  search_method->setInputCloud(cloud);

  pcl::ROPSEstimation<pcl::PointXYZ, pcl::Histogram<135>> estimator;
  estimator.setSearchMethod(search_method);
  estimator.setInputCloud(cloud);
  estimator.setSearchSurface(cloud);
  estimator.setIndices(indices);
  estimator.setTriangles(makeFanTriangles(kPoints));
  estimator.setRadiusSearch(10.0);
  estimator.setSupportRadius(10.0f);
  estimator.setNumberOfPartitionBins(5);
  estimator.setNumberOfRotations(1);

  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  pcl::PointCloud<pcl::Histogram<135>> output;
  estimator.compute(output);

  ASSERT_EQ(output.size(), indices->size());
  EXPECT_EQ(pcl_rvv_rops_rotate_cloud_trace_hits, 0u);
  EXPECT_EQ(pcl_rvv_rops_distribution_matrix_trace_hits, 0u);
}

TEST(RopsProductionRVV, DistributionHelperRejectsInvalidInputsBeforeScalarFallback)
{
  const pcl::PointCloud<pcl::PointXYZ> cloud = makeBenchShapedRotateCloud(257);
  const Eigen::Vector3f min(-1.0f, 0.5f, -0.75f);
  const Eigen::Vector3f max(1.0f, 2.75f, 3.25f);
  Eigen::MatrixXf matrix = Eigen::MatrixXf::Constant(5, 5, -1.0f);

  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  EXPECT_FALSE(pcl::detail::rops::getDistributionMatrixRVV(3, min, max, cloud, 5, matrix));
  EXPECT_FALSE(pcl::detail::rops::getDistributionMatrixRVV(0, min, max, cloud, 0, matrix));
  EXPECT_FALSE(pcl::detail::rops::getDistributionMatrixRVV(
      0, Eigen::Vector3f(1.0f, 0.5f, -0.75f), max, cloud, 5, matrix));
  EXPECT_FALSE(pcl::detail::rops::getDistributionMatrixRVV(
      0, min, Eigen::Vector3f(1.0f, 0.5f, 3.25f), cloud, 5, matrix));
  EXPECT_FLOAT_EQ(matrix(0, 0), -1.0f);
  EXPECT_EQ(pcl_rvv_rops_distribution_matrix_trace_hits, 0u);
}

TEST(RopsProductionRVV, NonFloatXYZLayoutFallsBackToScalarHelpers)
{
  const pcl::PointCloud<pcl::PointXYZ> reference_cloud = makeBenchShapedRotateCloud(257);
  const pcl::PointCloud<RopsXYZDouble> double_xyz_cloud =
      makeBenchShapedRotateCloudAs<RopsXYZDouble>(257);
  const pcl::PointXYZ reference_axis{0.57735026f, 0.57735026f, 0.57735026f};
  const RopsXYZDouble double_xyz_axis{
      reference_axis.x, reference_axis.y, reference_axis.z};
  constexpr unsigned int kBins = 5;

  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  pcl::PointCloud<pcl::PointXYZ> expected_cloud;
  pcl::PointCloud<RopsXYZDouble> actual_cloud;
  Eigen::Vector3f expected_min;
  Eigen::Vector3f expected_max;
  Eigen::Vector3f actual_min;
  Eigen::Vector3f actual_max;
  rops::rotateCloudStd(reference_axis, 33.0f, reference_cloud, expected_cloud, expected_min, expected_max);
  computeProductionRotateCloudTyped(
      double_xyz_axis, 33.0f, double_xyz_cloud, actual_cloud, actual_min, actual_max);

  expectXYZCloudAndBoundsNear(
      actual_cloud, actual_min, actual_max, expected_cloud, expected_min, expected_max, 2.0e-5f);
  EXPECT_EQ(pcl_rvv_rops_rotate_cloud_trace_hits, 0u);

  for (unsigned int projection = 0; projection < 3; ++projection) {
    Eigen::MatrixXf expected_matrix(kBins, kBins);
    rops::getDistributionMatrixStd(
        projection, expected_min, expected_max, expected_cloud, expected_matrix);
    const Eigen::MatrixXf actual_matrix = computeProductionDistributionMatrixTyped(
        projection, actual_min, actual_max, actual_cloud, kBins);
    expectMatrixNear(actual_matrix, expected_matrix);
  }
  EXPECT_EQ(pcl_rvv_rops_distribution_matrix_trace_hits, 0u);
}

TEST(RopsProductionRVV, PointXYZITraitsGateUsesRVVAndMatchesScalarReference)
{
  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  expectTypedProductionPipelineMatchesPointXYZReference<pcl::PointXYZI>();

  EXPECT_GE(pcl_rvv_rops_rotate_cloud_trace_hits, 257u);
  EXPECT_GE(pcl_rvv_rops_distribution_matrix_trace_hits, 257u * 3u);
}

TEST(RopsProductionRVV, PointNormalTraitsGateUsesRVVAndMatchesScalarReference)
{
  pcl_rvv_rops_rotate_cloud_trace_hits = 0;
  pcl_rvv_rops_distribution_matrix_trace_hits = 0;

  expectTypedProductionPipelineMatchesPointXYZReference<pcl::PointNormal>();

  EXPECT_GE(pcl_rvv_rops_rotate_cloud_trace_hits, 257u);
  EXPECT_GE(pcl_rvv_rops_distribution_matrix_trace_hits, 257u * 3u);
}
#endif
