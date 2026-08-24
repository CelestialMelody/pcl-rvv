/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 color_coding topic 的测试专用
 * helper 与当前 `ColorCoding` 标量语义一致。测试使用手算的 byte average
 *（字节平均）、XOR diff（异或差分）和 decode（解码）期望值，避免用被测
 * 代码反推 expected（期望值）。
 *
 * 证据边界：
 * 这里是 component diagnostic（组件诊断），不是 production direct（真实
 * 生产路径证据）。通过这些测试只能说明隔离 helper 的整数语义一致。
 */

#include "color_coding.h"

#include <gtest/gtest.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/types.h>
#include <pcl/compression/color_coding.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cc = pcl::octree::rvv_color_coding_support;

namespace {

constexpr unsigned char kRgbaOffset = offsetof(cc::ColorPoint, rgba);

std::vector<cc::ColorPoint>
makeCloud()
{
  std::vector<cc::ColorPoint> cloud(5);
  cloud[0].rgba = cc::packColor(1, 2, 3, 99);
  cloud[1].rgba = cc::packColor(10, 20, 30, 99);
  cloud[2].rgba = cc::packColor(14, 25, 37, 99);
  cloud[3].rgba = cc::packColor(20, 31, 40, 99);
  cloud[4].rgba = cc::packColor(200, 201, 202, 99);
  return cloud;
}

std::vector<unsigned char>
asUnsigned(const std::vector<char>& values)
{
  std::vector<unsigned char> out;
  out.reserve(values.size());
  for (const char value : values)
    out.push_back(static_cast<unsigned char>(value));
  return out;
}

std::vector<std::uint32_t>
colorsOf(const std::vector<cc::ColorPoint>& cloud)
{
  std::vector<std::uint32_t> values;
  values.reserve(cloud.size());
  for (const auto& point : cloud)
    values.push_back(point.rgba & 0x00ffffffu);
  return values;
}

} // namespace

TEST(ColorCodingReference, EncodeAverageUsesIntegerAverageBeforeBitReduction)
{
  const auto cloud = makeCloud();
  const std::vector<std::uint32_t> indices{1, 2, 3};
  cc::EncodedColorData encoded;

  cc::encodeAverageOfPointsReference(cloud, indices, kRgbaOffset, 1, encoded);

  const std::vector<unsigned char> expected_average{7, 12, 17};
  EXPECT_EQ(expected_average, asUnsigned(encoded.average));
  EXPECT_TRUE(encoded.differential.empty());
}

TEST(ColorCodingReference, EncodePointsUsesXorDiffBeforeBitReduction)
{
  const auto cloud = makeCloud();
  const std::vector<std::uint32_t> indices{1, 2, 3};
  cc::EncodedColorData encoded;

  cc::encodePointsReference(cloud, indices, kRgbaOffset, 1, encoded);

  const std::vector<unsigned char> expected_average{7, 12, 17};
  const std::vector<unsigned char> expected_diff{2, 6, 30, 0, 0, 3, 13, 3, 5};
  EXPECT_EQ(expected_average, asUnsigned(encoded.average));
  EXPECT_EQ(expected_diff, asUnsigned(encoded.differential));
}

TEST(ColorCodingReference, DecodePointsRestoresShiftedAverageAndDiffSemantics)
{
  cc::EncodedColorData encoded;
  encoded.average = {static_cast<char>(7), static_cast<char>(12), static_cast<char>(17)};
  encoded.differential = {static_cast<char>(2),
                          static_cast<char>(6),
                          static_cast<char>(30),
                          static_cast<char>(0),
                          static_cast<char>(0),
                          static_cast<char>(3),
                          static_cast<char>(13),
                          static_cast<char>(3),
                          static_cast<char>(5)};
  std::vector<cc::ColorPoint> output(5);
  std::size_t avg_cursor = 0;
  std::size_t diff_cursor = 0;

  cc::decodePointsReference(encoded, avg_cursor, diff_cursor, output, 1, 4, kRgbaOffset, 1);

  const std::vector<std::uint32_t> expected{
      0, cc::packColor(10, 20, 30), cc::packColor(14, 24, 36),
      cc::packColor(20, 30, 40), 0};
  EXPECT_EQ(expected, colorsOf(output));
  EXPECT_EQ(3u, avg_cursor);
  EXPECT_EQ(9u, diff_cursor);
}

TEST(ColorCodingReference, SetDefaultColorWritesWhiteWithoutAlpha)
{
  std::vector<cc::ColorPoint> output(4);

  cc::setDefaultColorReference(output, 1, 3, kRgbaOffset);

  const std::vector<std::uint32_t> expected{
      0, cc::packColor(255, 255, 255), cc::packColor(255, 255, 255), 0};
  EXPECT_EQ(expected, colorsOf(output));
}

TEST(ColorCodingCandidate, CandidateMatchesReferenceForAverageEncodeDecodeAndDefault)
{
  const auto cloud = makeCloud();
  const std::vector<std::uint32_t> indices{1, 2, 3};
  cc::EncodedColorData reference;
  cc::EncodedColorData candidate;

  cc::encodeAverageOfPointsReference(cloud, indices, kRgbaOffset, 1, reference);
  ASSERT_TRUE(cc::encodeAverageOfPointsCandidate(cloud, indices, kRgbaOffset, 1, candidate));
  EXPECT_EQ(reference.average, candidate.average);
  EXPECT_EQ(reference.differential, candidate.differential);

  reference = {};
  candidate = {};
  cc::encodePointsReference(cloud, indices, kRgbaOffset, 1, reference);
  ASSERT_TRUE(cc::encodePointsCandidate(cloud, indices, kRgbaOffset, 1, candidate));
  EXPECT_EQ(reference.average, candidate.average);
  EXPECT_EQ(reference.differential, candidate.differential);

  std::vector<cc::ColorPoint> ref_output(5);
  std::vector<cc::ColorPoint> cand_output(5);
  std::size_t ref_avg_cursor = 0;
  std::size_t ref_diff_cursor = 0;
  std::size_t cand_avg_cursor = 0;
  std::size_t cand_diff_cursor = 0;
  cc::decodePointsReference(
      reference, ref_avg_cursor, ref_diff_cursor, ref_output, 1, 4, kRgbaOffset, 1);
  ASSERT_TRUE(cc::decodePointsCandidate(
      candidate, cand_avg_cursor, cand_diff_cursor, cand_output, 1, 4, kRgbaOffset, 1));
  EXPECT_EQ(colorsOf(ref_output), colorsOf(cand_output));

  cc::setDefaultColorReference(ref_output, 0, ref_output.size(), kRgbaOffset);
  ASSERT_TRUE(cc::setDefaultColorCandidate(cand_output, 0, cand_output.size(), kRgbaOffset));
  EXPECT_EQ(colorsOf(ref_output), colorsOf(cand_output));
}

/*
 * 这个测试是 phase 020 的 production-shaped diagnostic（生产形态诊断）
 * RED gate。它使用真实 PCL RGBA 点类型，而不是测试专用 `ColorPoint`，
 * 用来证明后续 helper 至少能处理 production 侧相同的字段 offset 与
 * AoS（结构数组）stride。它仍然不是 production direct（真实生产入口）
 * 证据，因为没有通过 `OctreePointCloudCompression` 公开入口。
 */
TEST(ColorCodingProductionShaped, PointXYZRGBAMatchesComponentSemantics)
{
  std::vector<pcl::PointXYZRGBA> cloud(5);
  cloud[0].rgba = cc::packColor(1, 2, 3, 99);
  cloud[1].rgba = cc::packColor(10, 20, 30, 99);
  cloud[2].rgba = cc::packColor(14, 25, 37, 99);
  cloud[3].rgba = cc::packColor(20, 31, 40, 99);
  cloud[4].rgba = cc::packColor(200, 201, 202, 99);
  const std::vector<std::uint32_t> indices{1, 2, 3};
  constexpr unsigned char rgba_offset = offsetof(pcl::PointXYZRGBA, rgba);

  cc::EncodedColorData avg_reference;
  cc::EncodedColorData avg_candidate;
  cc::encodeAverageOfPointsReferencePointVector(
      cloud, indices, rgba_offset, 1, avg_reference);
  ASSERT_TRUE(cc::encodeAverageOfPointsCandidatePointVector(
      cloud, indices, rgba_offset, 1, avg_candidate));
  EXPECT_EQ(avg_reference.average, avg_candidate.average);
  EXPECT_TRUE(avg_candidate.differential.empty());

  cc::EncodedColorData reference;
  cc::EncodedColorData candidate;
  cc::encodePointsReferencePointVector(cloud, indices, rgba_offset, 1, reference);
  ASSERT_TRUE(cc::encodePointsCandidatePointVector(cloud, indices, rgba_offset, 1, candidate));
  EXPECT_EQ(reference.average, candidate.average);
  EXPECT_EQ(reference.differential, candidate.differential);

  std::vector<pcl::PointXYZRGBA> ref_output(5);
  std::vector<pcl::PointXYZRGBA> cand_output(5);
  std::size_t ref_avg_cursor = 0;
  std::size_t ref_diff_cursor = 0;
  std::size_t cand_avg_cursor = 0;
  std::size_t cand_diff_cursor = 0;
  cc::decodePointsReferencePointVector(
      reference, ref_avg_cursor, ref_diff_cursor, ref_output, 1, 4, rgba_offset, 1);
  ASSERT_TRUE(cc::decodePointsCandidatePointVector(
      candidate, cand_avg_cursor, cand_diff_cursor, cand_output, 1, 4, rgba_offset, 1));
  EXPECT_EQ(ref_output[1].rgba & 0x00ffffffu, cand_output[1].rgba & 0x00ffffffu);
  EXPECT_EQ(ref_output[2].rgba & 0x00ffffffu, cand_output[2].rgba & 0x00ffffffu);
  EXPECT_EQ(ref_output[3].rgba & 0x00ffffffu, cand_output[3].rgba & 0x00ffffffu);

  /*
   * phase 050 的 RED gate：staged-store decode（先连续写 scratch，再
   * 标量写回 AoS 点云）只用于 production-shaped implementation-shape
   * audit。它要证明新形态仍保持 byte stream 语义，不代表 production
   * direct 证据。
   */
  std::vector<pcl::PointXYZRGBA> staged_output(5);
  std::vector<std::uint32_t> scratch;
  std::size_t staged_avg_cursor = 0;
  std::size_t staged_diff_cursor = 0;
  ASSERT_TRUE(cc::decodePointsCandidatePointVectorStagedStore(
      candidate,
      staged_avg_cursor,
      staged_diff_cursor,
      staged_output,
      1,
      4,
      rgba_offset,
      1,
      scratch));
  EXPECT_EQ(ref_output[1].rgba & 0x00ffffffu, staged_output[1].rgba & 0x00ffffffu);
  EXPECT_EQ(ref_output[2].rgba & 0x00ffffffu, staged_output[2].rgba & 0x00ffffffu);
  EXPECT_EQ(ref_output[3].rgba & 0x00ffffffu, staged_output[3].rgba & 0x00ffffffu);
  EXPECT_EQ(3u, staged_avg_cursor);
  EXPECT_EQ(9u, staged_diff_cursor);

  cc::setDefaultColorReferencePointVector(ref_output, 0, ref_output.size(), rgba_offset);
  ASSERT_TRUE(
      cc::setDefaultColorCandidatePointVector(cand_output, 0, cand_output.size(), rgba_offset));
  EXPECT_EQ(ref_output[0].rgba & 0x00ffffffu, cand_output[0].rgba & 0x00ffffffu);
  EXPECT_EQ(ref_output[4].rgba & 0x00ffffffu, cand_output[4].rgba & 0x00ffffffu);
}

/*
 * phase 080 的 no-adoption closeout（不采纳收尾）测试：这个测试改用
 * 真实 `ColorCoding<pcl::PointXYZRGBA>` 公开方法，只验证完整回滚后
 * Std / RVV 构建都保持同一标量语义，不再要求任何 production RVV
 * dispatch（生产分流）命中。
 */
TEST(ColorCodingProductionDirect, PointXYZRGBAPublicMethodsMatchExpectedAfterFullRollback)
{
  using PointT = pcl::PointXYZRGBA;
  auto cloud = pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
  cloud->resize(64);
  pcl::Indices indices;
  indices.reserve(cloud->size());
  for (std::size_t i = 0; i < cloud->size(); ++i) {
    (*cloud)[i].rgba = cc::packColor(10, 20, 30, 99);
    indices.push_back(static_cast<pcl::index_t>(i));
  }
  constexpr unsigned char rgba_offset = offsetof(PointT, rgba);
  pcl::octree::ColorCoding<PointT> coder;
  coder.setBitDepth(8);

  coder.encodeAverageOfPoints(indices, rgba_offset, cloud);
  const std::vector<unsigned char> expected_average{10, 20, 30};
  EXPECT_EQ(expected_average, asUnsigned(coder.getAverageDataVector()));
  EXPECT_TRUE(coder.getDifferentialDataVector().empty());

  coder.initializeEncoding();
  coder.encodePoints(indices, rgba_offset, cloud);
  const std::vector<unsigned char> expected_diff(indices.size() * 3, 0);
  EXPECT_EQ(expected_average, asUnsigned(coder.getAverageDataVector()));
  EXPECT_EQ(expected_diff, asUnsigned(coder.getDifferentialDataVector()));

  auto output = pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
  output->resize(64);
  coder.setDefaultColor(output, 0, output->size(), rgba_offset);
  for (const auto& point : *output)
    EXPECT_EQ(cc::packColor(255, 255, 255), point.rgba & 0x00ffffffu);
}

/*
 * 完整回滚后，这个测试保留小规模和非 exact `PointXYZRGBA` 点型样本，
 * 用来防止 no-adoption closeout 意外改变公开方法的标量语义。
 */
TEST(ColorCodingProductionDirect, ScalarSemanticsRemainForSmallAndNonExactPointTypes)
{
  {
    using PointT = pcl::PointXYZRGBA;
    auto cloud = pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
    cloud->resize(3);
    for (std::size_t i = 0; i < cloud->size(); ++i)
      (*cloud)[i].rgba = cc::packColor(10, 20, 30, 99);
    const pcl::Indices indices{0, 1, 2};
    constexpr unsigned char rgba_offset = offsetof(PointT, rgba);
    pcl::octree::ColorCoding<PointT> coder;
    coder.setBitDepth(8);

    coder.encodeAverageOfPoints(indices, rgba_offset, cloud);
    EXPECT_EQ((std::vector<unsigned char>{10, 20, 30}),
              asUnsigned(coder.getAverageDataVector()));

    auto output = pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
    output->resize(3);
    coder.setDefaultColor(output, 0, output->size(), rgba_offset);
    for (const auto& point : *output)
      EXPECT_EQ(cc::packColor(255, 255, 255), point.rgba & 0x00ffffffu);
  }

  {
    using PointT = pcl::PointXYZRGB;
    auto cloud = pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
    cloud->resize(64);
    pcl::Indices indices;
    indices.reserve(cloud->size());
    for (std::size_t i = 0; i < cloud->size(); ++i) {
      (*cloud)[i].rgba = cc::packColor(10, 20, 30, 99);
      indices.push_back(static_cast<pcl::index_t>(i));
    }
    constexpr unsigned char rgba_offset = offsetof(PointT, rgba);
    pcl::octree::ColorCoding<PointT> coder;
    coder.setBitDepth(8);

    coder.encodePoints(indices, rgba_offset, cloud);
    EXPECT_EQ((std::vector<unsigned char>{10, 20, 30}),
              asUnsigned(coder.getAverageDataVector()));
    EXPECT_EQ(std::vector<unsigned char>(indices.size() * 3, 0),
              asUnsigned(coder.getDifferentialDataVector()));

    auto output = pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
    output->resize(64);
    coder.setDefaultColor(output, 0, output->size(), rgba_offset);
    for (const auto& point : *output)
      EXPECT_EQ(cc::packColor(255, 255, 255), point.rgba & 0x00ffffffu);
  }
}
