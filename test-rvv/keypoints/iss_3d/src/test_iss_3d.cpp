/*
 * 本文件验证 ISSKeypoint3D 的 scatter matrix（散布矩阵）局部核。
 * Phase 000 只证明 test-only component ablation（测试专用组件消融）边界；
 * 它不证明 searchForNeighbors（邻域搜索）、EVD（特征值分解）、NMS（非极大值抑制）
 * 或 production dispatch（生产分流）已经可采纳。
 */

#include "iss_3d.h"

#include <pcl/keypoints/iss_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <vector>

namespace iss = pcl::keypoints::rvv_test::iss_3d;

namespace
{
std::vector<pcl::PointXYZ>
makeCloud(const std::size_t n)
{
  std::vector<pcl::PointXYZ> points(n);
  for (std::size_t i = 0; i < n; ++i)
  {
    points[i].x = static_cast<float>((static_cast<int>(i % 17) - 8) * 0.03125f);
    points[i].y = static_cast<float>((static_cast<int>((i * 7) % 19) - 9) * 0.026f);
    points[i].z = static_cast<float>(1.0f + static_cast<int>((i * 11) % 23) * 0.013f);
  }
  return points;
}

std::vector<int>
makeNeighbors(const std::size_t count, const std::size_t modulo)
{
  std::vector<int> indices(count);
  for (std::size_t i = 0; i < count; ++i)
    indices[i] = static_cast<int>((i * 37 + 5) % modulo);
  return indices;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloudPtr(const std::size_t n)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  const auto points = makeCloud(n);
  cloud->points.assign(points.begin(), points.end());
  return cloud;
}

void
expectNearMatrix(const Eigen::Matrix3d& lhs, const Eigen::Matrix3d& rhs, const double tolerance)
{
  for (int row = 0; row < 3; ++row)
    for (int col = 0; col < 3; ++col)
      EXPECT_NEAR(lhs(row, col), rhs(row, col), tolerance) << "row=" << row << " col=" << col;
}

class ISSScatterProbe : public pcl::ISSKeypoint3D<pcl::PointXYZ, pcl::PointXYZ>
{
public:
  bool initialize()
  {
    return this->initCompute();
  }

  void scatterAt(const int index, Eigen::Matrix3d& scatter)
  {
    this->getScatterMatrix(index, scatter);
  }
};

Eigen::Matrix3d
referenceScatterForRadius(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& cloud,
                          pcl::search::KdTree<pcl::PointXYZ>& tree,
                          const int current_index,
                          const double radius)
{
  pcl::Indices nn_indices;
  std::vector<float> nn_distances;
  tree.radiusSearch(current_index, radius, nn_indices, nn_distances);

  Eigen::Matrix3d scatter = Eigen::Matrix3d::Zero();
  const auto& center = (*cloud)[current_index];
  for (const int n_idx : nn_indices)
  {
    const auto& point = (*cloud)[n_idx];
    const double dx = point.x - center.x;
    const double dy = point.y - center.y;
    const double dz = point.z - center.z;
    scatter(0, 0) += dx * dx;
    scatter(0, 1) += dx * dy;
    scatter(0, 2) += dx * dz;
    scatter(1, 0) += dy * dx;
    scatter(1, 1) += dy * dy;
    scatter(1, 2) += dy * dz;
    scatter(2, 0) += dz * dx;
    scatter(2, 1) += dz * dy;
    scatter(2, 2) += dz * dz;
  }
  return scatter;
}
} // namespace

TEST(ISS3DScatterDiagnostic, CandidateMatchesScalarForIndexedNeighbors)
{
  const auto points = makeCloud(257);
  const auto neighbors = makeNeighbors(193, points.size());
  Eigen::Matrix3d scalar = Eigen::Matrix3d::Zero();
  Eigen::Matrix3d candidate = Eigen::Matrix3d::Zero();

  iss::computeScatterMatrixStd(points.data(), 17, neighbors.data(), neighbors.size(), scalar);
  iss::computeScatterMatrixCandidate(points.data(), 17, neighbors.data(), neighbors.size(), candidate);

  expectNearMatrix(scalar, candidate, 1e-3);
}

TEST(ISS3DScatterDiagnostic, CandidateHandlesTailNeighborCount)
{
  const auto points = makeCloud(131);
  const auto neighbors = makeNeighbors(73, points.size());
  Eigen::Matrix3d scalar = Eigen::Matrix3d::Zero();
  Eigen::Matrix3d candidate = Eigen::Matrix3d::Zero();

  iss::computeScatterMatrixStd(points.data(), 42, neighbors.data(), neighbors.size(), scalar);
  iss::computeScatterMatrixCandidate(points.data(), 42, neighbors.data(), neighbors.size(), candidate);

  expectNearMatrix(scalar, candidate, 1e-3);
}

TEST(ISS3DScatterDiagnostic, EmptyNeighborSetKeepsZeroMatrix)
{
  const auto points = makeCloud(16);
  Eigen::Matrix3d candidate = Eigen::Matrix3d::Constant(42.0);

  iss::computeScatterMatrixCandidate(points.data(), 3, nullptr, 0, candidate);

  expectNearMatrix(Eigen::Matrix3d::Zero(), candidate, 0.0);
}

TEST(ISS3DProductionScatter, GetScatterMatrixMatchesIndependentRadiusReference)
{
  const double radius = 0.55;
  const int current_index = 64;
  auto cloud = makeCloudPtr(257);
  auto tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZ>>();
  tree->setInputCloud(cloud);

  ISSScatterProbe detector;
  detector.setSearchMethod(tree);
  detector.setInputCloud(cloud);
  detector.setSalientRadius(radius);
  detector.setNonMaxRadius(radius);
  detector.setMinNeighbors(1);
  ASSERT_TRUE(detector.initialize());

  Eigen::Matrix3d production = Eigen::Matrix3d::Zero();
  detector.scatterAt(current_index, production);
  const Eigen::Matrix3d reference = referenceScatterForRadius(cloud, *tree, current_index, radius);

  expectNearMatrix(reference, production, 1e-8);
}
