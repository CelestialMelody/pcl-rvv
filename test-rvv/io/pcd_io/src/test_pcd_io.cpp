/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 pcd_io 的 test-only（仅测试使用）
 * component ablation（组件消融）helper 是否能复刻 PCLPointCloud2
 * binary_compressed 路径里的 field layout conversion（字段布局转换）、
 * production-shaped writer payload，以及 reader shaped unpack + finite scan。
 *
 * 证据边界：
 * 这里没有修改 production（生产源码），也不证明 PCDReader / PCDWriter 的
 * public entry（公开入口）已经命中 RVV。测试只证明 pack/unpack 候选能和
 * 标量参考链路一致，供后续 QEMU、反汇编和板卡 bench 继续取证。
 */

#include "pcd_io.h"

#include <pcl/io/pcd_io.h>
#include <pcl/test/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace pcd = pcl::io::rvv_test::pcd_io;

namespace pcl::io::pcd_io_rvv_test {

enum class WriterCompressedPath {
  None,
  Std,
  Rvv,
};

void
resetWriterCompressedPath();

WriterCompressedPath
lastWriterCompressedPath();

} // namespace pcl::io::pcd_io_rvv_test

namespace {

pcl::PCLPointField
makeField(const std::string& name,
          const std::uint32_t offset,
          const std::uint8_t datatype,
          const std::uint32_t count = 1)
{
  pcl::PCLPointField field;
  field.name = name;
  field.offset = offset;
  field.datatype = datatype;
  field.count = count;
  return field;
}

pcl::PCLPointCloud2
makeCloud(const std::size_t point_count,
          const std::uint32_t point_step,
          const std::vector<pcl::PCLPointField>& pcl_fields,
          const std::vector<pcd::FieldLayout>& layouts)
{
  pcl::PCLPointCloud2 cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.point_step = point_step;
  cloud.row_step = point_step * cloud.width;
  cloud.is_dense = true;
  cloud.fields = pcl_fields;
  cloud.data = pcd::makeInterleavedCloud(point_count, point_step, layouts);
  return cloud;
}

std::vector<std::uint8_t>
compressedPayloadFromStream(const std::string& text)
{
  const std::string marker = "DATA binary_compressed\n";
  const auto pos = text.find(marker);
  EXPECT_NE(std::string::npos, pos);
  if (pos == std::string::npos)
    return {};

  const auto payload_begin = pos + marker.size();
  return std::vector<std::uint8_t>(text.begin() + static_cast<std::ptrdiff_t>(payload_begin),
                                   text.end());
}

std::vector<std::uint8_t>
writeCompressedPayload(const pcl::PCLPointCloud2& cloud)
{
  pcl::PCDWriter writer;
  std::ostringstream stream(std::ios::binary);
  const int status = writer.writeBinaryCompressed(stream, cloud);
  EXPECT_EQ(0, status);
  return compressedPayloadFromStream(stream.str());
}

} // namespace

TEST(PCDIOComponentAblation, PackCandidateMatchesScalarForFourByteFields)
{
  const std::size_t point_count = 257;
  const std::size_t point_step = 16;
  const std::vector<pcd::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcd::makeInterleavedCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcd::packFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcd::resetLastPath();
  pcd::packFieldsCandidate(cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcd::PathKind::PackRvv, pcd::lastPath());
#else
  EXPECT_EQ(pcd::PathKind::PackScalarFallback, pcd::lastPath());
#endif
}

TEST(PCDIOProductionWriter, PublicOstreamPayloadMatchesScalarOracleAndHitsRvvPath)
{
  const std::size_t point_count = 128;
  const std::uint32_t point_step = 16;
  const std::vector<pcd::FieldLayout> layouts = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = makeCloud(point_count,
                               point_step,
                               {makeField("x", 0, pcl::PCLPointField::FLOAT32),
                                makeField("y", 4, pcl::PCLPointField::FLOAT32),
                                makeField("z", 8, pcl::PCLPointField::FLOAT32),
                                makeField("intensity", 12, pcl::PCLPointField::FLOAT32)},
                               layouts);

  std::vector<std::uint8_t> expected;
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadScalar(
      cloud.data.data(), point_count, point_step, layouts, expected));

  pcl::io::pcd_io_rvv_test::resetWriterCompressedPath();
  const auto actual = writeCompressedPayload(cloud);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcl::io::pcd_io_rvv_test::WriterCompressedPath::Rvv,
            pcl::io::pcd_io_rvv_test::lastWriterCompressedPath());
#else
  EXPECT_EQ(pcl::io::pcd_io_rvv_test::WriterCompressedPath::Std,
            pcl::io::pcd_io_rvv_test::lastWriterCompressedPath());
#endif
}

TEST(PCDIOProductionWriter, PublicOstreamFallsBackForMixedFieldSizes)
{
  const std::size_t point_count = 64;
  const std::uint32_t point_step = 18;
  const std::vector<pcd::FieldLayout> layouts = {
      {0, 4},
      {4, 2},
      {8, 1},
      {12, 4},
  };
  const auto cloud = makeCloud(point_count,
                               point_step,
                               {makeField("x", 0, pcl::PCLPointField::FLOAT32),
                                makeField("ring", 4, pcl::PCLPointField::UINT16),
                                makeField("label", 8, pcl::PCLPointField::UINT8),
                                makeField("intensity", 12, pcl::PCLPointField::FLOAT32)},
                               layouts);

  std::vector<std::uint8_t> expected;
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadScalar(
      cloud.data.data(), point_count, point_step, layouts, expected));

  pcl::io::pcd_io_rvv_test::resetWriterCompressedPath();
  const auto actual = writeCompressedPayload(cloud);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(pcl::io::pcd_io_rvv_test::WriterCompressedPath::Std,
            pcl::io::pcd_io_rvv_test::lastWriterCompressedPath());
}

TEST(PCDIOProductionWriter, PublicOstreamFallsBackForUnalignedPointStep)
{
  const std::size_t point_count = 48;
  const std::uint32_t point_step = 18;
  const std::vector<pcd::FieldLayout> layouts = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = makeCloud(point_count,
                               point_step,
                               {makeField("x", 0, pcl::PCLPointField::FLOAT32),
                                makeField("y", 4, pcl::PCLPointField::FLOAT32),
                                makeField("z", 8, pcl::PCLPointField::FLOAT32),
                                makeField("intensity", 12, pcl::PCLPointField::FLOAT32)},
                               layouts);

  std::vector<std::uint8_t> expected;
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadScalar(
      cloud.data.data(), point_count, point_step, layouts, expected));

  pcl::io::pcd_io_rvv_test::resetWriterCompressedPath();
  const auto actual = writeCompressedPayload(cloud);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(pcl::io::pcd_io_rvv_test::WriterCompressedPath::Std,
            pcl::io::pcd_io_rvv_test::lastWriterCompressedPath());
}

TEST(PCDIOProductionWriter, PublicOstreamFallsBackForUnalignedFieldOffset)
{
  const std::size_t point_count = 48;
  const std::uint32_t point_step = 20;
  const std::vector<pcd::FieldLayout> layouts = {
      {2, 4},
      {6, 4},
      {10, 4},
      {14, 4},
  };
  const auto cloud = makeCloud(point_count,
                               point_step,
                               {makeField("x", 2, pcl::PCLPointField::FLOAT32),
                                makeField("y", 6, pcl::PCLPointField::FLOAT32),
                                makeField("z", 10, pcl::PCLPointField::FLOAT32),
                                makeField("intensity", 14, pcl::PCLPointField::FLOAT32)},
                               layouts);

  std::vector<std::uint8_t> expected;
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadScalar(
      cloud.data.data(), point_count, point_step, layouts, expected));

  pcl::io::pcd_io_rvv_test::resetWriterCompressedPath();
  const auto actual = writeCompressedPayload(cloud);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(pcl::io::pcd_io_rvv_test::WriterCompressedPath::Std,
            pcl::io::pcd_io_rvv_test::lastWriterCompressedPath());
}

TEST(PCDIOProductionWriter, PublicOstreamIgnoresPaddingField)
{
  const std::size_t point_count = 96;
  const std::uint32_t point_step = 24;
  const std::vector<pcd::FieldLayout> layouts = {
      {0, 4},
      {4, 4},
      {8, 4},
      {16, 4},
  };
  const auto cloud = makeCloud(point_count,
                               point_step,
                               {makeField("x", 0, pcl::PCLPointField::FLOAT32),
                                makeField("y", 4, pcl::PCLPointField::FLOAT32),
                                makeField("z", 8, pcl::PCLPointField::FLOAT32),
                                makeField("_", 12, pcl::PCLPointField::UINT32),
                                makeField("intensity", 16, pcl::PCLPointField::FLOAT32)},
                               layouts);

  std::vector<std::uint8_t> expected;
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadScalar(
      cloud.data.data(), point_count, point_step, layouts, expected));

  pcl::io::pcd_io_rvv_test::resetWriterCompressedPath();
  const auto actual = writeCompressedPayload(cloud);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcl::io::pcd_io_rvv_test::WriterCompressedPath::Rvv,
            pcl::io::pcd_io_rvv_test::lastWriterCompressedPath());
#else
  EXPECT_EQ(pcl::io::pcd_io_rvv_test::WriterCompressedPath::Std,
            pcl::io::pcd_io_rvv_test::lastWriterCompressedPath());
#endif
}

TEST(PCDIOComponentAblation, UnpackCandidateMatchesScalarWithTrailingPadding)
{
  const std::size_t point_count = 193;
  const std::size_t point_step = 20;
  const std::vector<pcd::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcd::makeInterleavedCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> packed;
  pcd::packFieldsScalar(cloud.data(), point_count, point_step, fields, packed);

  std::vector<std::uint8_t> expected(point_count * point_step, 0xcc);
  std::vector<std::uint8_t> actual(point_count * point_step, 0xcc);
  pcd::unpackFieldsScalar(packed.data(), point_count, point_step, fields, expected);
  pcd::resetLastPath();
  pcd::unpackFieldsCandidate(packed.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcd::PathKind::UnpackRvv, pcd::lastPath());
#else
  EXPECT_EQ(pcd::PathKind::UnpackScalarFallback, pcd::lastPath());
#endif
}

TEST(PCDIOComponentAblation, CandidateFallsBackForMixedFieldSizes)
{
  const std::size_t point_count = 31;
  const std::size_t point_step = 18;
  const std::vector<pcd::FieldLayout> fields = {
      {0, 4},
      {4, 2},
      {8, 1},
      {12, 4},
  };
  const auto cloud = pcd::makeInterleavedCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcd::packFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcd::resetLastPath();
  pcd::packFieldsCandidate(cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(pcd::PathKind::PackScalarFallback, pcd::lastPath());
}

TEST(PCDIOWriterShapedDiagnostic, CompressedPayloadCandidateMatchesScalar)
{
  const std::size_t point_count = 128;
  const std::size_t point_step = 16;
  const std::vector<pcd::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcd::makeInterleavedCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadScalar(
      cloud.data(), point_count, point_step, fields, expected));
  pcd::resetLastPath();
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadCandidate(
      cloud.data(), point_count, point_step, fields, actual));

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcd::PathKind::PackRvv, pcd::lastPath());
#else
  EXPECT_EQ(pcd::PathKind::PackScalarFallback, pcd::lastPath());
#endif
}

TEST(PCDIOReaderShapedDiagnostic, CompressedReaderBodyCandidateMatchesScalarAndDenseFlag)
{
  const std::size_t point_count = 128;
  const std::size_t point_step = 16;
  const std::vector<pcd::FieldLayout> fields = {
      {0, 4, pcd::FiniteKind::Float32},
      {4, 4, pcd::FiniteKind::Float32},
      {8, 4, pcd::FiniteKind::Float32},
      {12, 4, pcd::FiniteKind::None},
  };
  auto cloud = pcd::makeInterleavedFloatCloud(point_count, point_step, fields);
  pcd::writeFloat32(cloud, point_step, fields[1].offset, 17, std::numeric_limits<float>::quiet_NaN());

  std::vector<std::uint8_t> payload;
  ASSERT_TRUE(pcd::makeCompressedWriterPayloadScalar(
      cloud.data(), point_count, point_step, fields, payload));

  std::vector<std::uint8_t> expected(point_count * point_step, 0xcc);
  std::vector<std::uint8_t> actual(point_count * point_step, 0xcc);
  bool expected_dense = true;
  bool actual_dense = true;
  ASSERT_TRUE(pcd::readCompressedBodyScalar(
      payload.data(), payload.size(), point_count, point_step, fields, expected, expected_dense));
  pcd::resetLastPath();
  ASSERT_TRUE(pcd::readCompressedBodyCandidate(
      payload.data(), payload.size(), point_count, point_step, fields, actual, actual_dense));

  EXPECT_EQ(expected, actual);
  EXPECT_FALSE(expected_dense);
  EXPECT_FALSE(actual_dense);
#if defined(__RVV10__)
  EXPECT_EQ(pcd::PathKind::UnpackRvv, pcd::lastPath());
#else
  EXPECT_EQ(pcd::PathKind::UnpackScalarFallback, pcd::lastPath());
#endif
}
