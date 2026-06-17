#include "transformation_validation_euclidean_diag.hpp"

#include <pcl/test/gtest.h>

#include <cmath>
#include <limits>

namespace diag = pcl::registration::transformation_validation_euclidean_diag;

namespace {

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(std::size_t n)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 1021) - 510) * 0.01f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 7) % 1031) - 515) * 0.008f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 13) % 1033) - 516) * 0.006f;
  }
  return cloud;
}

diag::Matrix4f
makeTransform()
{
  diag::Matrix4f t = diag::Matrix4f::Identity();
  t(0, 0) = 0.9659258f;
  t(0, 1) = -0.2588190f;
  t(1, 0) = 0.2588190f;
  t(1, 1) = 0.9659258f;
  t(0, 3) = 0.12f;
  t(1, 3) = -0.04f;
  t(2, 3) = 0.08f;
  return t;
}

} // namespace

TEST(TransformationValidationEuclideanDiag, TransformCandidateMatchesScalar)
{
  const auto cloud = makeCloud(4096);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  pcl::PointCloud<pcl::PointXYZ> candidate;
  const auto transform = makeTransform();

  diag::transformPointXYZStd(*cloud, scalar, transform);
  diag::transformPointXYZCandidate(*cloud, candidate, transform);

  ASSERT_EQ(scalar.size(), candidate.size());
  for (std::size_t i = 0; i < scalar.size(); ++i) {
    EXPECT_NEAR(scalar[i].x, candidate[i].x, 1e-6f);
    EXPECT_NEAR(scalar[i].y, candidate[i].y, 1e-6f);
    EXPECT_NEAR(scalar[i].z, candidate[i].z, 1e-6f);
  }
}

TEST(TransformationValidationEuclideanDiag, SmallInputFallbackMatchesScalar)
{
  const auto cloud = makeCloud(17);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  pcl::PointCloud<pcl::PointXYZ> candidate;
  const auto transform = makeTransform();

  diag::transformPointXYZStd(*cloud, scalar, transform);
  diag::transformPointXYZCandidate(*cloud, candidate, transform);

  ASSERT_EQ(scalar.size(), candidate.size());
  for (std::size_t i = 0; i < scalar.size(); ++i) {
    EXPECT_FLOAT_EQ(scalar[i].x, candidate[i].x);
    EXPECT_FLOAT_EQ(scalar[i].y, candidate[i].y);
    EXPECT_FLOAT_EQ(scalar[i].z, candidate[i].z);
  }
}

TEST(TransformationValidationEuclideanDiag, FullValidationScoreMatchesScalar)
{
  const auto source = makeCloud(2048);
  const auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const auto transform = makeTransform();
  diag::transformPointXYZStd(*source, *target, transform);

  const double scalar = diag::validateTransformationStd(source, target, transform, 1.0);
  const double candidate = diag::validateTransformationCandidate(source, target, transform, 1.0);
  EXPECT_NEAR(scalar, candidate, 1e-12);
}

TEST(TransformationValidationEuclideanDiag, MaxRangeRejectsAllMatchesScalar)
{
  const auto source = makeCloud(1024);
  const auto target = makeCloud(1024);
  diag::Matrix4f transform = diag::Matrix4f::Identity();
  transform(0, 3) = 100.0f;

  const double scalar = diag::validateTransformationStd(source, target, transform, 1e-6);
  const double candidate = diag::validateTransformationCandidate(source, target, transform, 1e-6);
  EXPECT_EQ(scalar, std::numeric_limits<double>::max());
  EXPECT_EQ(candidate, scalar);
}
