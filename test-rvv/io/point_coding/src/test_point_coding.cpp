/*
 * 本文件做什么：
 * 这是 point_coding 首阶段 correctness（正确性）测试。Std build 使用
 * test-only 标量参考链路；RVV build 在 __RVV10__ 下会走测试专用 RVV
 * candidate（RVV 候选）。这些测试只证明 leaf-level component ablation
 * （组件消融）与 PointCoding 的标量公式一致，不证明 production dispatch
 * （生产分流）已经存在。
 */

#include "point_coding.h"

#include <pcl/test/gtest.h>
#include <pcl/rvv_point_traits.h>

#include <cstdint>
#include <vector>

struct PointCodingTestXYZDouble
{
  double x{};
  double y{};
  double z{};
  float untouched{};
};

POINT_CLOUD_REGISTER_POINT_STRUCT(PointCodingTestXYZDouble,
                                  (double, x, x)(double, y, y)(double, z, z)(
                                      float, untouched, untouched))

namespace pcoding = pcl::io::rvv_test::point_coding;

TEST(PointCodingComponentAblation, EncodeCandidateMatchesScalarReference)
{
  const auto cloud = pcoding::makePointCloud(257);
  const pcl::Indices indices = {0, 3, 4, 7, 31, 63, 128, 129, 205, 256};
  const pcoding::ReferencePoint reference{1.25, -2.5, 0.75};

  std::vector<char> expected;
  std::vector<char> actual;
  pcoding::encodePointsScalar(cloud, indices, reference, 0.03125f, expected);
  pcoding::encodePointsCandidate(cloud, indices, reference, 0.03125f, actual);

  EXPECT_EQ(expected, actual);
}

TEST(PointCodingComponentAblation, EncodeCandidateClampsLikeScalarReference)
{
  const auto cloud = pcoding::makePointCloudWithClampCases();
  const pcl::Indices indices = {0, 1, 2, 3, 4, 5};
  const pcoding::ReferencePoint reference{0.0, 0.0, 0.0};

  std::vector<char> expected;
  std::vector<char> actual;
  pcoding::encodePointsScalar(cloud, indices, reference, 0.01f, expected);
  pcoding::encodePointsCandidate(cloud, indices, reference, 0.01f, actual);

  EXPECT_EQ(expected, actual);
}

TEST(PointCodingComponentAblation, DecodeCandidateMatchesScalarReference)
{
  const auto encoded = pcoding::makeEncodedDiffs(513);
  const pcoding::ReferencePoint reference{-4.0, 2.0, 1.5};

  std::vector<pcoding::PointXYZ> expected;
  std::vector<pcoding::PointXYZ> actual;
  pcoding::decodePointsScalar(encoded, reference, 0.0625f, expected);
  pcoding::decodePointsCandidate(encoded, reference, 0.0625f, actual);

  ASSERT_EQ(expected.size(), actual.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_FLOAT_EQ(expected[i].x, actual[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].y, actual[i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].z, actual[i].z) << "i=" << i;
  }
}

TEST(PointCodingProductionShapedScout, DecodeCandidateMatchesPointCodingObjectState)
{
  const std::size_t point_count = 513;
  const std::size_t begin_index = 5;
  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-4.0, 2.0, 1.5};

  const auto expected =
      pcoding::decodePointsProductionObject(encoded, reference, 0.0625f, begin_index);
  auto actual = pcoding::makeOutputCloudWithPadding(point_count, begin_index);
  pcoding::decodePointsCandidateToCloud(encoded, reference, 0.0625f, actual, begin_index);

  ASSERT_EQ(expected.size(), actual.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_FLOAT_EQ(expected[i].x, actual[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].y, actual[i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].z, actual[i].z) << "i=" << i;
  }
}

TEST(PointCodingProductionShapedScout, DecodeCandidateMatchesMultiLeafObjectState)
{
  const auto fixture = pcoding::makeMultiLeafDecodeCase(1537);
  const auto expected = pcoding::decodePointsProductionObjectMultiLeaf(fixture, 0.0625f);
  auto actual =
      pcoding::makeOutputCloudWithPadding(fixture.encoded.size() / 3, fixture.begin_index);
  pcoding::decodePointsCandidateMultiLeafToCloud(fixture, 0.0625f, actual);

  ASSERT_EQ(expected.size(), actual.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_FLOAT_EQ(expected[i].x, actual[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].y, actual[i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].z, actual[i].z) << "i=" << i;
  }
}

TEST(PointCodingProductionDirect, DecodePointXYZMatchesIndependentScalarReference)
{
  const std::size_t point_count = 513;
  const std::size_t begin_index = 5;
  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-4.0, 2.0, 1.5};

  auto expected = pcoding::makeOutputCloudWithPadding(point_count, begin_index);
  pcoding::decodePointsScalarToCloud(encoded, reference, 0.0625f, expected, begin_index);

  pcl::octree::PointCoding<pcoding::PointXYZ> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = encoded;
  coder.initializeDecoding();
  auto actual = pcl::PointCloud<pcoding::PointXYZ>::Ptr(new pcl::PointCloud<pcoding::PointXYZ>(
      pcoding::makeOutputCloudWithPadding(point_count, begin_index)));
  coder.decodePoints(actual,
                     pcoding::asArray(reference),
                     static_cast<pcl::uindex_t>(begin_index),
                     static_cast<pcl::uindex_t>(begin_index + point_count));

  ASSERT_EQ(expected.size(), actual->size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_FLOAT_EQ(expected[i].x, (*actual)[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].y, (*actual)[i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[i].z, (*actual)[i].z) << "i=" << i;
  }
}

TEST(PointCodingProductionDirect, DecodePointXYZKeepsDoubleReferenceRounding)
{
  const std::size_t point_count = 3;
  const std::size_t begin_index = 1;
  const std::vector<char> encoded = {
      static_cast<char>(57), static_cast<char>(58), static_cast<char>(59),
      static_cast<char>(60), static_cast<char>(61), static_cast<char>(62),
      static_cast<char>(63), static_cast<char>(64), static_cast<char>(65)};
  const pcoding::ReferencePoint reference{
      -777.6720136988527, 127.37500000000003, 0.10000000000000002};
  constexpr float resolution = 0.6911351479762546f;

  auto expected = pcoding::makeOutputCloudWithPadding(point_count, begin_index);
  pcoding::decodePointsScalarToCloud(encoded, reference, resolution, expected, begin_index);

  pcl::octree::PointCoding<pcoding::PointXYZ> coder;
  coder.setPrecision(resolution);
  coder.getDifferentialDataVector() = encoded;
  coder.initializeDecoding();
  auto actual = pcl::PointCloud<pcoding::PointXYZ>::Ptr(new pcl::PointCloud<pcoding::PointXYZ>(
      pcoding::makeOutputCloudWithPadding(point_count, begin_index)));
  coder.decodePoints(actual,
                     pcoding::asArray(reference),
                     static_cast<pcl::uindex_t>(begin_index),
                     static_cast<pcl::uindex_t>(begin_index + point_count));

  for (std::size_t i = 0; i < point_count; ++i) {
    EXPECT_FLOAT_EQ(expected[begin_index + i].x, (*actual)[begin_index + i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[begin_index + i].y, (*actual)[begin_index + i].y) << "i=" << i;
    EXPECT_FLOAT_EQ(expected[begin_index + i].z, (*actual)[begin_index + i].z) << "i=" << i;
  }
}

TEST(PointCodingProductionDirect, DecodePointXYZIKeepsExtraFieldWithTraitsGate)
{
  static_assert(pcl::rvv::kRVVXYZAoSPointCompatible<pcl::PointXYZI>,
                "PointXYZI should exercise the Phase 070 traits-gated RVV decode path.");
  const std::size_t point_count = 129;
  const std::size_t begin_index = 3;
  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-1.0, 0.25, 2.0};

  pcl::PointCloud<pcl::PointXYZI> actual_cloud;
  actual_cloud.resize(begin_index + point_count + 2);
  actual_cloud.width = static_cast<std::uint32_t>(actual_cloud.size());
  actual_cloud.height = 1;
  actual_cloud.is_dense = true;
  for (std::size_t i = 0; i < actual_cloud.size(); ++i) {
    actual_cloud[i].x = -1000.0f;
    actual_cloud[i].y = 1000.0f;
    actual_cloud[i].z = 17.0f;
    actual_cloud[i].intensity = static_cast<float>(i);
  }

  pcl::octree::PointCoding<pcl::PointXYZI> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = encoded;
  coder.initializeDecoding();
  auto actual = pcl::PointCloud<pcl::PointXYZI>::Ptr(new pcl::PointCloud<pcl::PointXYZI>(actual_cloud));
  coder.decodePoints(actual,
                     pcoding::asArray(reference),
                     static_cast<pcl::uindex_t>(begin_index),
                     static_cast<pcl::uindex_t>(begin_index + point_count));

  for (std::size_t i = 0; i < point_count; ++i) {
    const auto diff_x = static_cast<unsigned char>(encoded[i * 3 + 0]);
    const auto diff_y = static_cast<unsigned char>(encoded[i * 3 + 1]);
    const auto diff_z = static_cast<unsigned char>(encoded[i * 3 + 2]);
    const auto& point = (*actual)[begin_index + i];
    EXPECT_FLOAT_EQ(static_cast<float>(reference.x + (diff_x + 0.5) * 0.0625f), point.x)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>(reference.y + (diff_y + 0.5) * 0.0625f), point.y)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>(reference.z + (diff_z + 0.5) * 0.0625f), point.z)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>(begin_index + i), point.intensity) << "i=" << i;
  }
}

TEST(PointCodingProductionDirect, DecodePointXYZRGBKeepsColorFieldsWithTraitsGate)
{
  static_assert(pcl::rvv::kRVVXYZAoSPointCompatible<pcl::PointXYZRGB>,
                "PointXYZRGB should exercise the Phase 070 traits-gated RVV decode path.");
  const std::size_t point_count = 257;
  const std::size_t begin_index = 4;
  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{2.0, -0.5, 4.0};

  pcl::PointCloud<pcl::PointXYZRGB> actual_cloud;
  actual_cloud.resize(begin_index + point_count + 2);
  actual_cloud.width = static_cast<std::uint32_t>(actual_cloud.size());
  actual_cloud.height = 1;
  actual_cloud.is_dense = true;
  for (std::size_t i = 0; i < actual_cloud.size(); ++i) {
    actual_cloud[i].x = -1000.0f;
    actual_cloud[i].y = 1000.0f;
    actual_cloud[i].z = 17.0f;
    actual_cloud[i].r = static_cast<std::uint8_t>((i * 3) & 0xff);
    actual_cloud[i].g = static_cast<std::uint8_t>((i * 5) & 0xff);
    actual_cloud[i].b = static_cast<std::uint8_t>((i * 7) & 0xff);
  }

  pcl::octree::PointCoding<pcl::PointXYZRGB> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = encoded;
  coder.initializeDecoding();
  auto actual =
      pcl::PointCloud<pcl::PointXYZRGB>::Ptr(new pcl::PointCloud<pcl::PointXYZRGB>(actual_cloud));
  coder.decodePoints(actual,
                     pcoding::asArray(reference),
                     static_cast<pcl::uindex_t>(begin_index),
                     static_cast<pcl::uindex_t>(begin_index + point_count));

  for (std::size_t i = 0; i < point_count; ++i) {
    const auto diff_x = static_cast<unsigned char>(encoded[i * 3 + 0]);
    const auto diff_y = static_cast<unsigned char>(encoded[i * 3 + 1]);
    const auto diff_z = static_cast<unsigned char>(encoded[i * 3 + 2]);
    const auto& point = (*actual)[begin_index + i];
    EXPECT_FLOAT_EQ(static_cast<float>(reference.x + (diff_x + 0.5) * 0.0625f), point.x)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>(reference.y + (diff_y + 0.5) * 0.0625f), point.y)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>(reference.z + (diff_z + 0.5) * 0.0625f), point.z)
        << "i=" << i;
    EXPECT_EQ(actual_cloud[begin_index + i].r, point.r) << "i=" << i;
    EXPECT_EQ(actual_cloud[begin_index + i].g, point.g) << "i=" << i;
    EXPECT_EQ(actual_cloud[begin_index + i].b, point.b) << "i=" << i;
  }
}

TEST(PointCodingProductionDirect, DecodeDoubleXYZFallsBackToScalarSemantics)
{
  static_assert(!pcl::rvv::kRVVXYZAoSPointCompatible<PointCodingTestXYZDouble>,
                "double x/y/z fields are registered but not single-float xyz, so they should fall back.");
  const std::size_t point_count = 65;
  const std::size_t begin_index = 2;
  const auto encoded = pcoding::makeEncodedDiffs(point_count);
  const pcoding::ReferencePoint reference{-3.0, 1.0, 0.5};

  pcl::PointCloud<PointCodingTestXYZDouble> actual_cloud;
  actual_cloud.resize(begin_index + point_count + 2);
  actual_cloud.width = static_cast<std::uint32_t>(actual_cloud.size());
  actual_cloud.height = 1;
  actual_cloud.is_dense = true;
  for (std::size_t i = 0; i < actual_cloud.size(); ++i) {
    actual_cloud[i].x = -1000.0f;
    actual_cloud[i].y = 1000.0f;
    actual_cloud[i].z = 17.0f;
    actual_cloud[i].untouched = static_cast<float>(i * 2);
  }

  pcl::octree::PointCoding<PointCodingTestXYZDouble> coder;
  coder.setPrecision(0.0625f);
  coder.getDifferentialDataVector() = encoded;
  coder.initializeDecoding();
  auto actual = pcl::PointCloud<PointCodingTestXYZDouble>::Ptr(
      new pcl::PointCloud<PointCodingTestXYZDouble>(actual_cloud));
  coder.decodePoints(actual,
                     pcoding::asArray(reference),
                     static_cast<pcl::uindex_t>(begin_index),
                     static_cast<pcl::uindex_t>(begin_index + point_count));

  for (std::size_t i = 0; i < point_count; ++i) {
    const auto diff_x = static_cast<unsigned char>(encoded[i * 3 + 0]);
    const auto diff_y = static_cast<unsigned char>(encoded[i * 3 + 1]);
    const auto diff_z = static_cast<unsigned char>(encoded[i * 3 + 2]);
    const auto& point = (*actual)[begin_index + i];
    EXPECT_FLOAT_EQ(static_cast<float>(reference.x + (diff_x + 0.5) * 0.0625f), point.x)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>(reference.y + (diff_y + 0.5) * 0.0625f), point.y)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>(reference.z + (diff_z + 0.5) * 0.0625f), point.z)
        << "i=" << i;
    EXPECT_FLOAT_EQ(static_cast<float>((begin_index + i) * 2), point.untouched) << "i=" << i;
  }
}

TEST(PointCodingPublicRoundtripFeasibility, RoundtripSmokeProducesFiniteOutput)
{
  // 这个 smoke（小型验证）只证明公开 Octree 压缩/解压入口能在
  // test-rvv 中构造。Phase 060/070 后，RVV build 的解码阶段已有
  // production dispatch（生产分流）；这个 gtest 仍只做 correctness，
  // 性能结论必须来自 Phase 080 的板卡 repeated benchmark。
  const auto input = pcoding::makePointCloud(128);
  const auto result = pcoding::runPublicOctreeRoundtrip(input);

  EXPECT_EQ(input.size(), result.input_size);
  EXPECT_EQ(input.size(), result.output_size);
  EXPECT_EQ(1u, result.output_height);
  EXPECT_GT(result.compressed_bytes, 0u);
  EXPECT_TRUE(result.all_finite_xyz);
  EXPECT_NE(0u, result.checksum);
}
