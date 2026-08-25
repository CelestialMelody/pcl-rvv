/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 organized edge detection（有组织点云边缘检测）
 * depth-label diagnostic（深度标签诊断）候选是否复刻 production `OrganizedEdgeBase::extractEdges()`
 * 的标签语义，并保持 `assignLabelIndices()` 的线性索引顺序。
 *
 * 证据边界：
 * 本文件只覆盖 test-only candidate（测试专用候选）。它不修改 production 头文件，也不证明
 * 真实 `OrganizedEdgeBase::compute()` 已经命中 RVV dispatch（RVV 分流）。
 */

#include "organized_edge_detection.h"

#include <gtest/gtest.h>

#include <pcl/features/organized_edge_detection.h>
#include <pcl/point_cloud.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/point_types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <vector>

namespace oed = pcl::features::rvv_test::organized_edge_detection;

namespace
{
std::vector<float>
makeFlatDepth(std::size_t width, std::size_t height, float depth)
{
  std::vector<float> z(width * height, depth);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
      z[row * width + col] += 0.0001f * static_cast<float>((row + col) % 3);
  }
  return z;
}

std::vector<float>
makeSquareDiscontinuity(std::size_t width, std::size_t height)
{
  std::vector<float> z = makeFlatDepth(width, height, 2.0f);
  const std::size_t left = width / 4;
  const std::size_t right = width - left;
  const std::size_t top = height / 4;
  const std::size_t bottom = height - top;
  for (std::size_t row = top; row < bottom; ++row)
  {
    for (std::size_t col = left; col < right; ++col)
      z[row * width + col] = 1.75f;
  }
  return z;
}

std::vector<float>
makeInvalidBoundary(std::size_t width, std::size_t height)
{
  std::vector<float> z = makeSquareDiscontinuity(width, height);
  z[3 * width + 3] = std::numeric_limits<float>::quiet_NaN();
  z[3 * width + 4] = std::numeric_limits<float>::quiet_NaN();
  z[4 * width + 3] = std::numeric_limits<float>::quiet_NaN();
  z[(height / 2) * width + (width / 2)] = std::numeric_limits<float>::infinity();
  return z;
}

void
expectSameLabelsAndIndices(const std::vector<float>& z,
                           std::size_t width,
                           std::size_t height,
                           int max_search_neighbors,
                           std::uint32_t edge_types)
{
  constexpr float depth_threshold = 0.02f;
  std::vector<std::uint32_t> expected(z.size(), 99U);
  std::vector<std::uint32_t> actual(z.size(), 77U);

  oed::computeDepthLabelsScalar(z.data(),
                                width,
                                height,
                                depth_threshold,
                                max_search_neighbors,
                                edge_types,
                                expected.data());
  oed::computeDepthLabelsRVV(z.data(),
                             width,
                             height,
                             depth_threshold,
                             max_search_neighbors,
                             edge_types,
                             actual.data());

  EXPECT_EQ(actual, expected);
  EXPECT_EQ(oed::assignLabelIndices(actual.data(), actual.size()),
            oed::assignLabelIndices(expected.data(), expected.size()));
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeCloudFromDepth(const std::vector<float>& z, const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = false;
  cloud->points.resize(z.size());
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      PointT& point = (*cloud)[row * width + col];
      point.x = static_cast<float>(col);
      point.y = static_cast<float>(row);
      point.z = z[row * width + col];
    }
  }
  return cloud;
}

std::vector<std::uint32_t>
labelBits(const pcl::PointCloud<pcl::Label>& labels)
{
  std::vector<std::uint32_t> bits(labels.size());
  for (std::size_t i = 0; i < labels.size(); ++i)
    bits[i] = labels[i].label;
  return bits;
}

std::vector<std::vector<std::size_t>>
labelIndexVectors(const std::vector<pcl::PointIndices>& label_indices)
{
  std::vector<std::vector<std::size_t>> indices(label_indices.size());
  for (std::size_t edge_type = 0; edge_type < label_indices.size(); ++edge_type)
  {
    indices[edge_type].reserve(label_indices[edge_type].indices.size());
    for (const int index : label_indices[edge_type].indices)
      indices[edge_type].push_back(static_cast<std::size_t>(index));
  }
  return indices;
}

template <typename PointT>
void
expectProductionComputeMatchesScalarReference(const std::vector<float>& z,
                                              const std::size_t width,
                                              const std::size_t height,
                                              const int max_search_neighbors,
                                              const std::uint32_t edge_types)
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value,
                "Phase 020 point-type expansion only covers xyz AoS float point types");

  constexpr float depth_threshold = 0.02f;
  std::vector<std::uint32_t> expected(z.size(), 0U);
  oed::computeDepthLabelsScalar(z.data(),
                                width,
                                height,
                                depth_threshold,
                                max_search_neighbors,
                                edge_types,
                                expected.data());

  pcl::OrganizedEdgeBase<PointT, pcl::Label> detector;
  detector.setInputCloud(makeCloudFromDepth<PointT>(z, width, height));
  detector.setDepthDisconThreshold(depth_threshold);
  detector.setMaxSearchNeighbors(max_search_neighbors);
  detector.setEdgeType(static_cast<int>(edge_types));

  pcl::PointCloud<pcl::Label> labels;
  std::vector<pcl::PointIndices> label_indices;
  detector.compute(labels, label_indices);

  EXPECT_EQ(labelBits(labels), expected);
  EXPECT_EQ(labelIndexVectors(label_indices),
            oed::assignLabelIndices(expected.data(), expected.size()));
}
} // namespace

TEST(OrganizedEdgeDetectionDepthLabelsRVV, MatchesScalarForDepthStepAndTailWidth)
{
  expectSameLabelsAndIndices(makeSquareDiscontinuity(37, 19),
                             37,
                             19,
                             8,
                             oed::kOccluding | oed::kOccluded);
}

TEST(OrganizedEdgeDetectionDepthLabelsRVV, MatchesScalarForNanBoundarySearch)
{
  expectSameLabelsAndIndices(makeInvalidBoundary(41, 23),
                             41,
                             23,
                             12,
                             oed::kNanBoundary | oed::kOccluding | oed::kOccluded);
}

TEST(OrganizedEdgeDetectionDepthLabelsRVV, HonorsDisabledEdgeTypeBits)
{
  expectSameLabelsAndIndices(makeInvalidBoundary(29, 17), 29, 17, 9, oed::kNanBoundary);
}

TEST(OrganizedEdgeDetectionDepthLabelsRVV, ProductionComputeMatchesScalarReference)
{
  expectProductionComputeMatchesScalarReference<pcl::PointXYZ>(
      makeInvalidBoundary(43, 25),
      43,
      25,
      12,
      oed::kNanBoundary | oed::kOccluding | oed::kOccluded);
}

TEST(OrganizedEdgeDetectionDepthLabelsRVV, ProductionComputeMatchesScalarReferenceForPointXYZI)
{
  expectProductionComputeMatchesScalarReference<pcl::PointXYZI>(
      makeSquareDiscontinuity(47, 29), 47, 29, 10, oed::kOccluding | oed::kOccluded);
}

TEST(OrganizedEdgeDetectionDepthLabelsRVV, ProductionComputeMatchesScalarReferenceForPointXYZRGB)
{
  expectProductionComputeMatchesScalarReference<pcl::PointXYZRGB>(
      makeInvalidBoundary(43, 27),
      43,
      27,
      12,
      oed::kNanBoundary | oed::kOccluding | oed::kOccluded);
}

TEST(OrganizedEdgeDetectionDepthLabelsRVV,
     ProductionComputeMatchesScalarReferenceForPointXYZRGBNormal)
{
  expectProductionComputeMatchesScalarReference<pcl::PointXYZRGBNormal>(
      makeSquareDiscontinuity(49, 31), 49, 31, 10, oed::kOccluding | oed::kOccluded);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
