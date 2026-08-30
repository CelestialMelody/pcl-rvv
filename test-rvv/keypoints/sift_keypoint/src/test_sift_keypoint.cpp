/*
 * 本文件做什么：
 * 这些 gtest 验证 SIFT scale-space diagnostic helper 的 scalar reference 和
 * RVV candidate 是否在同一 synthetic neighborhood batch 上对拍一致。公共
 * `SIFTKeypoint` smoke 只负责证明当前生产源码还能处理测试用 organized cloud，
 * 不把它当作 RVV 采纳证据。
 */

#include "sift_keypoint.h"

#include <algorithm>
#include <pcl/keypoints/sift_keypoint.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace sift = pcl::keypoints::rvv_test::sift_keypoint;

namespace
{
pcl::PointCloud<pcl::PointXYZI>::Ptr
makeSmokeCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->points.resize(width * height);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      const std::size_t index = row * width + col;
      cloud->points[index].x = static_cast<float>(col) * 0.01f;
      cloud->points[index].y = static_cast<float>(row) * 0.01f;
      cloud->points[index].z = 1.0f + 0.02f * static_cast<float>((row + col) % 5);
      cloud->points[index].intensity =
          0.7f + 0.2f * std::sin(0.13f * static_cast<float>(row)) +
          0.1f * std::cos(0.17f * static_cast<float>(col));
    }
  }
  return cloud;
}

void
expectResponsesNear(const sift::ScaleSpaceResponse& lhs,
                    const sift::ScaleSpaceResponse& rhs,
                    const float tolerance)
{
  ASSERT_EQ(lhs.rows, rhs.rows);
  ASSERT_EQ(lhs.cols, rhs.cols);
  ASSERT_EQ(lhs.dog.size(), rhs.dog.size());
  for (std::size_t i = 0; i < lhs.dog.size(); ++i)
    EXPECT_NEAR(lhs.dog[i], rhs.dog[i], tolerance) << "index=" << i;
}

sift::ScaleSpaceResponse
makeExtremaFixture()
{
  sift::ScaleSpaceResponse response;
  response.rows = 6;
  response.cols = 5;
  response.dog.assign(response.rows * response.cols, -0.1f);

  response.dog[sift::indexOf(3, 2, response.cols)] = 0.8f;
  response.dog[sift::indexOf(3, 1, response.cols)] = 0.2f;
  response.dog[sift::indexOf(3, 3, response.cols)] = 0.2f;
  response.dog[sift::indexOf(2, 2, response.cols)] = 0.3f;
  response.dog[sift::indexOf(4, 2, response.cols)] = 0.3f;

  response.dog[sift::indexOf(1, 2, response.cols)] = 0.1f;
  response.dog[sift::indexOf(5, 2, response.cols)] = 0.1f;
  return response;
}

} // namespace

TEST(SiftScaleSpace, SyntheticCaseHasSortedNeighborhoodAndScales)
{
  const auto data = sift::makeSyntheticScaleSpaceCase(11, 17, 3);

  ASSERT_EQ(data.scales.size(), 6u);
  for (std::size_t i = 1; i < data.scales.size(); ++i)
    EXPECT_GT(data.scales[i], data.scales[i - 1]);

  ASSERT_EQ(data.points.size(), 11u);
  for (const auto& point : data.points)
  {
    ASSERT_EQ(point.neighborhood.distance_sqr.size(), 17u);
    ASSERT_EQ(point.neighborhood.value.size(), 17u);
    for (std::size_t i = 1; i < point.neighborhood.distance_sqr.size(); ++i)
      EXPECT_LE(point.neighborhood.distance_sqr[i - 1], point.neighborhood.distance_sqr[i]);
  }
}

TEST(SiftScaleSpace, CandidateMatchesScalarReferenceForSyntheticBatch)
{
  const auto data = sift::makeSyntheticScaleSpaceCase(23, 29, 3);
  const auto scalar = sift::computeScaleSpaceScalar(data);
  std::size_t rvv_chunks = 0;
  const auto candidate = sift::computeScaleSpaceCandidate(data, &rvv_chunks);

  expectResponsesNear(candidate, scalar, 1e-5f);
#if defined(__RVV10__) && defined(__riscv_vector)
  EXPECT_GT(rvv_chunks, 0u);
#else
  EXPECT_EQ(rvv_chunks, 0u);
#endif
  EXPECT_NE(sift::checksumResponses(candidate), 0u);
}

TEST(SiftScaleSpace, CandidatePreservesScalarEarlyBreakForUnsortedNeighborhood)
{
  sift::ScaleSpaceCase data;
  data.nr_scales_per_octave = 1;
  data.extrema_radius = 1;
  data.contrast_threshold = 0.0f;
  data.scales = {0.1f, 0.2f, 0.3f};
  data.points.resize(1);
  data.points[0].neighborhood.distance_sqr = {0.01f, 0.5f, 0.02f};
  data.points[0].neighborhood.value = {1.0f, 100.0f, 10.0f};

  const auto scalar = sift::computeScaleSpaceScalar(data);
  std::size_t rvv_chunks = 0;
  const auto candidate = sift::computeScaleSpaceCandidate(data, &rvv_chunks);

  expectResponsesNear(candidate, scalar, 1e-5f);
}

TEST(SiftScaleSpace, ExtremaScanFindsStableInteriorPoints)
{
  const auto response = makeExtremaFixture();
  const auto extrema = sift::findScaleSpaceExtremaScalar(response, 1, 0.05f);

  EXPECT_FALSE(extrema.empty());
  EXPECT_NE(sift::checksumIndices(extrema), 0u);
  EXPECT_NE(std::find(extrema.begin(), extrema.end(), 3), extrema.end());
  for (const int index : extrema)
  {
    EXPECT_GE(index, 0);
    EXPECT_LT(static_cast<std::size_t>(index), response.rows);
  }
}

TEST(SiftScaleSpace, PublicSiftKeypointAcceptsSyntheticOrganizedCloud)
{
  const auto cloud = makeSmokeCloud(32, 24);
  pcl::SIFTKeypoint<pcl::PointXYZI, pcl::PointWithScale> detector;
  detector.setSearchMethod(pcl::make_shared<pcl::search::KdTree<pcl::PointXYZI>>());
  detector.setInputCloud(cloud);
  detector.setScales(0.02f, 3, 3);
  detector.setMinimumContrast(0.0f);

  pcl::PointCloud<pcl::PointWithScale> output;
  detector.compute(output);

  EXPECT_EQ(output.width, output.size());
  EXPECT_EQ(output.height, 1u);
  EXPECT_EQ(output.header, cloud->header);
}
