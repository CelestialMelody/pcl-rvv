/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 pcd_io_templated_writer 的
 * test-only（仅测试使用）component ablation（组件消融）helper 是否能复刻
 * `PCDWriter::writeBinaryCompressed<PointT>` 的压缩前置字段布局转换。
 *
 * 证据边界：
 * 这里没有修改 production（生产源码），也不证明真实 `PCDWriter` public
 * entry（公开入口）已经命中 RVV。测试只证明候选布局转换能和标量参考链路
 * 一致，并且 RVV build 会在可覆盖的 4 字节字段布局下真实命中候选路径。
 */

#define PCL_RVV_PCD_WRITER_TEST_HOOK

#include "pcdtw.h"

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/test/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

namespace pcdtw = pcl::io::rvv_test::pcd_io_templated_writer;

struct PCDTWPointMixedField
{
  float x;
  float y;
  float z;
  std::uint16_t ring;
};

POINT_CLOUD_REGISTER_POINT_STRUCT(PCDTWPointMixedField,
                                  (float, x, x)
                                  (float, y, y)
	                                  (float, z, z)
	                                  (std::uint16_t, ring, ring))

struct PCDTWPointXYZRGBPadding
{
  float x;
  float y;
  float z;
  std::uint32_t rgba;
  std::uint32_t ignored_padding;
};

struct PCDTWPointXYZRGBCompact
{
  float x;
  float y;
  float z;
  std::uint32_t rgba;
};

POINT_CLOUD_REGISTER_POINT_STRUCT(PCDTWPointXYZRGBPadding,
                                  (float, x, x)
                                  (float, y, y)
                                  (float, z, z)
                                  (std::uint32_t, rgba, rgba))

POINT_CLOUD_REGISTER_POINT_STRUCT(PCDTWPointXYZRGBCompact,
                                  (float, x, x)
                                  (float, y, y)
                                  (float, z, z)
                                  (std::uint32_t, rgba, rgba))

namespace {

constexpr int kCompressedWriterScalarHook = 1;
constexpr int kCompressedWriterRvvHook = 2;
constexpr int kBinaryWriterNoneHook = 0;
constexpr int kBinaryWriterScalarHook = 1;
constexpr int kBinaryWriterMemcpyHook = 2;
constexpr int kBinaryWriterRvvHook = 3;

std::string
makeTempPcdPath()
{
  std::string pattern = "/tmp/pcdtw_public_XXXXXX.pcd";
  std::vector<char> path(pattern.begin(), pattern.end());
  path.push_back('\0');
  const int fd = mkstemps(path.data(), 4);
  EXPECT_NE(fd, -1);
  if (fd != -1)
    close(fd);
  return std::string(path.data());
}

std::vector<std::uint8_t>
readFileBytes(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  EXPECT_TRUE(file.good());
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>());
}

std::vector<pcdtw::FieldLayout>
fieldLayoutsFromPointType(const std::vector<pcl::PCLPointField>& fields)
{
  std::vector<pcdtw::FieldLayout> layouts;
  for (const auto& field : fields) {
    if (field.name == "_")
      continue;
    layouts.push_back({field.offset,
                       static_cast<std::size_t>(field.count) *
                           static_cast<std::size_t>(pcl::getFieldSize(field.datatype))});
  }
  return layouts;
}

std::vector<std::uint8_t>
decompressCompressedPcdPayload(const std::vector<std::uint8_t>& file_bytes)
{
  const std::string marker = "DATA binary_compressed\n";
  const auto begin = reinterpret_cast<const char*>(file_bytes.data());
  const auto found = std::search(begin, begin + file_bytes.size(), marker.begin(), marker.end());
  EXPECT_NE(found, begin + file_bytes.size());
  const auto payload_offset =
      static_cast<std::size_t>(found - begin) + marker.size();
  EXPECT_LE(payload_offset + 8, file_bytes.size());

  std::uint32_t compressed_size = 0;
  std::uint32_t data_size = 0;
  std::memcpy(&compressed_size, file_bytes.data() + payload_offset, sizeof(compressed_size));
  std::memcpy(&data_size, file_bytes.data() + payload_offset + 4, sizeof(data_size));
  EXPECT_LE(payload_offset + 8 + compressed_size, file_bytes.size());

  std::vector<std::uint8_t> unpacked(data_size);
  if (data_size == 0)
    return unpacked;
  const unsigned int decompressed_size =
      pcl::lzfDecompress(file_bytes.data() + payload_offset + 8,
                         compressed_size,
                         unpacked.data(),
                         data_size);
  EXPECT_EQ(data_size, decompressed_size);
  return unpacked;
}

std::vector<std::uint8_t>
extractBinaryPcdPayload(const std::vector<std::uint8_t>& file_bytes)
{
  const std::string marker = "DATA binary\n";
  const auto begin = reinterpret_cast<const char*>(file_bytes.data());
  const auto found = std::search(begin, begin + file_bytes.size(), marker.begin(), marker.end());
  EXPECT_NE(found, begin + file_bytes.size());
  const auto payload_offset =
      static_cast<std::size_t>(found - begin) + marker.size();
  EXPECT_LE(payload_offset, file_bytes.size());
  return std::vector<std::uint8_t>(file_bytes.begin() + static_cast<std::ptrdiff_t>(payload_offset),
                                   file_bytes.end());
}

template <typename PointT>
pcl::PointCloud<PointT>
makePublicWriterCloud(const std::size_t point_count)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    cloud.points[i].x = static_cast<float>(i) * 0.25f;
    cloud.points[i].y = static_cast<float>(i % 17) - 3.0f;
    cloud.points[i].z = static_cast<float>(i % 29) + 0.5f;
  }
  return cloud;
}

template <>
pcl::PointCloud<pcl::PointXYZRGBA>
makePublicWriterCloud<pcl::PointXYZRGBA>(const std::size_t point_count)
{
  pcl::PointCloud<pcl::PointXYZRGBA> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.25f;
    point.y = static_cast<float>(i % 17) - 3.0f;
    point.z = static_cast<float>(i % 29) + 0.5f;
    point.r = static_cast<std::uint8_t>((i * 3) & 0xffu);
    point.g = static_cast<std::uint8_t>((i * 5) & 0xffu);
    point.b = static_cast<std::uint8_t>((i * 7) & 0xffu);
    point.a = 255;
  }
  return cloud;
}

template <>
pcl::PointCloud<PCDTWPointXYZRGBCompact>
makePublicWriterCloud<PCDTWPointXYZRGBCompact>(const std::size_t point_count)
{
  pcl::PointCloud<PCDTWPointXYZRGBCompact> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.25f;
    point.y = static_cast<float>(i % 17) - 3.0f;
    point.z = static_cast<float>(i % 29) + 0.5f;
    point.rgba = (static_cast<std::uint32_t>((i * 3) & 0xffu) << 16) |
                 (static_cast<std::uint32_t>((i * 5) & 0xffu) << 8) |
                 static_cast<std::uint32_t>((i * 7) & 0xffu);
  }
  return cloud;
}

template <>
pcl::PointCloud<PCDTWPointXYZRGBPadding>
makePublicWriterCloud<PCDTWPointXYZRGBPadding>(const std::size_t point_count)
{
  pcl::PointCloud<PCDTWPointXYZRGBPadding> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.25f;
    point.y = static_cast<float>(i % 17) - 3.0f;
    point.z = static_cast<float>(i % 29) + 0.5f;
    point.rgba = (static_cast<std::uint32_t>((i * 3) & 0xffu) << 16) |
                 (static_cast<std::uint32_t>((i * 5) & 0xffu) << 8) |
                 static_cast<std::uint32_t>((i * 7) & 0xffu);
    point.ignored_padding = 0xa5a50000u | static_cast<std::uint32_t>(i & 0xffffu);
  }
  return cloud;
}

template <>
pcl::PointCloud<PCDTWPointMixedField>
makePublicWriterCloud<PCDTWPointMixedField>(const std::size_t point_count)
{
  pcl::PointCloud<PCDTWPointMixedField> cloud;
  cloud.width = static_cast<std::uint32_t>(point_count);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(point_count);
  for (std::size_t i = 0; i < point_count; ++i) {
    auto& point = cloud.points[i];
    point.x = static_cast<float>(i) * 0.5f;
    point.y = static_cast<float>(i % 13);
    point.z = static_cast<float>(i % 23);
    point.ring = static_cast<std::uint16_t>(i % 1024);
  }
  return cloud;
}

template <typename PointT>
void
expectCompressedPublicWriterMatchesScalarPack(const pcl::PointCloud<PointT>& cloud)
{
  std::vector<pcl::PCLPointField> pcl_fields;
  pcl::getFields<PointT>(pcl_fields);
  const auto layouts = fieldLayoutsFromPointType(pcl_fields);
  std::vector<std::uint8_t> expected;
  pcdtw::packFieldsScalar(reinterpret_cast<const std::uint8_t*>(cloud.points.data()),
                          cloud.size(),
                          sizeof(PointT),
                          layouts,
                          expected);

  const std::string path = makeTempPcdPath();
  pcl::io::detail::pcl_rvv_pcd_writer_compressed_reset_test_hook();
  pcl::PCDWriter writer;
  EXPECT_EQ(0, writer.writeBinaryCompressed<PointT>(path, cloud));
  const auto unpacked = decompressCompressedPcdPayload(readFileBytes(path));
  std::remove(path.c_str());
  EXPECT_EQ(expected, unpacked);
}

template <typename PointT>
void
expectBinaryPublicWriterMatchesScalarPack(const pcl::PointCloud<PointT>& cloud)
{
  std::vector<pcl::PCLPointField> pcl_fields;
  pcl::getFields<PointT>(pcl_fields);
  const auto layouts = fieldLayoutsFromPointType(pcl_fields);
  std::vector<std::uint8_t> expected;
  pcdtw::packBinaryFieldsScalar(reinterpret_cast<const std::uint8_t*>(cloud.points.data()),
                                cloud.size(),
                                sizeof(PointT),
                                layouts,
                                expected);

  const std::string path = makeTempPcdPath();
  pcl::io::detail::pcl_rvv_pcd_writer_binary_reset_test_hook();
  pcl::PCDWriter writer;
  EXPECT_EQ(0, writer.writeBinary<PointT>(path, cloud));
  const auto payload = extractBinaryPcdPayload(readFileBytes(path));
  std::remove(path.c_str());
  EXPECT_EQ(expected, payload);
}

template <typename PointT>
std::vector<std::uint8_t>
packBinaryFieldsForIndices(const pcl::PointCloud<PointT>& cloud, const pcl::Indices& indices)
{
  std::vector<pcl::PCLPointField> pcl_fields;
  pcl::getFields<PointT>(pcl_fields);
  const auto layouts = fieldLayoutsFromPointType(pcl_fields);
  std::vector<std::uint8_t> expected;
  expected.reserve(indices.size() * pcdtw::packedBinarySize(1, layouts));
  for (const auto index : indices) {
    for (const auto& field : layouts) {
      const auto* first =
          reinterpret_cast<const std::uint8_t*>(&cloud[index]) + field.offset;
      expected.insert(expected.end(), first, first + static_cast<std::ptrdiff_t>(field.size));
    }
  }
  return expected;
}

} // namespace

TEST(PCDTemplatedWriterComponentAblation,
     PackCandidateMatchesScalarForPointXYZRgbLikeLayout)
{
  const std::size_t point_count = 257;
  const std::size_t point_step = 16;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packFieldsCandidate(cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcdtw::PathKind::PackRvv, pcdtw::lastPath());
#else
  EXPECT_EQ(pcdtw::PathKind::PackScalarFallback, pcdtw::lastPath());
#endif
}

TEST(PCDTemplatedWriterComponentAblation, CandidateHandlesTrailingPointPadding)
{
  const std::size_t point_count = 193;
  const std::size_t point_step = 20;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packFieldsCandidate(cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcdtw::PathKind::PackRvv, pcdtw::lastPath());
#else
  EXPECT_EQ(pcdtw::PathKind::PackScalarFallback, pcdtw::lastPath());
#endif
}

TEST(PCDTemplatedWriterComponentAblation, CandidateFallsBackForMixedFieldSizes)
{
  const std::size_t point_count = 31;
  const std::size_t point_step = 18;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 2},
      {8, 1},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packFieldsCandidate(cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(pcdtw::PathKind::PackScalarFallback, pcdtw::lastPath());
}

TEST(PCDTemplatedWriterCompressedDiagnostic,
     PackAndCompressCandidateMatchesScalarPayload)
{
  const std::size_t point_count = 4096;
  const std::size_t point_step = 16;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  ASSERT_TRUE(pcdtw::packAndCompressScalar(
      cloud.data(), point_count, point_step, fields, expected));
  pcdtw::resetLastPath();
  ASSERT_TRUE(pcdtw::packAndCompressCandidate(
      cloud.data(), point_count, point_step, fields, actual));

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcdtw::PathKind::PackRvv, pcdtw::lastPath());
#else
  EXPECT_EQ(pcdtw::PathKind::PackScalarFallback, pcdtw::lastPath());
#endif
}

TEST(PCDTemplatedWriterProductionDirect,
     CompressedPublicWriterMatchesScalarPackAndHitsRvvPath)
{
  const auto cloud = makePublicWriterCloud<pcl::PointXYZRGBA>(2049);

  expectCompressedPublicWriterMatchesScalarPack(cloud);
#if defined(__RVV10__)
  EXPECT_EQ(kCompressedWriterRvvHook,
            pcl::io::detail::pcl_rvv_pcd_writer_compressed_last_test_hook());
#else
  EXPECT_EQ(kCompressedWriterScalarHook,
            pcl::io::detail::pcl_rvv_pcd_writer_compressed_last_test_hook());
#endif
}

TEST(PCDTemplatedWriterProductionDirect,
     CompressedPublicWriterFallsBackForMixedFieldSizes)
{
  const auto cloud = makePublicWriterCloud<PCDTWPointMixedField>(129);

  expectCompressedPublicWriterMatchesScalarPack(cloud);
  EXPECT_EQ(kCompressedWriterScalarHook,
            pcl::io::detail::pcl_rvv_pcd_writer_compressed_last_test_hook());
}

TEST(PCDTemplatedWriterProductionDirect,
     BinaryPublicWriterCompactMatchesScalarAndUsesMemcpyPath)
{
  const auto cloud = makePublicWriterCloud<PCDTWPointXYZRGBCompact>(1025);

  expectBinaryPublicWriterMatchesScalarPack(cloud);
#if defined(__RVV10__)
  EXPECT_EQ(kBinaryWriterMemcpyHook,
            pcl::io::detail::pcl_rvv_pcd_writer_binary_last_test_hook());
#else
  EXPECT_EQ(kBinaryWriterScalarHook,
            pcl::io::detail::pcl_rvv_pcd_writer_binary_last_test_hook());
#endif
}

TEST(PCDTemplatedWriterProductionDirect,
     BinaryPublicWriterPaddingMatchesScalarAndUsesSegmentPath)
{
  const auto cloud = makePublicWriterCloud<PCDTWPointXYZRGBPadding>(1025);

  expectBinaryPublicWriterMatchesScalarPack(cloud);
#if defined(__RVV10__)
  EXPECT_EQ(kBinaryWriterRvvHook,
            pcl::io::detail::pcl_rvv_pcd_writer_binary_last_test_hook());
#else
  EXPECT_EQ(kBinaryWriterScalarHook,
            pcl::io::detail::pcl_rvv_pcd_writer_binary_last_test_hook());
#endif
}

TEST(PCDTemplatedWriterProductionDirect,
     BinaryPublicWriterFallsBackForMixedFieldSizes)
{
  const auto cloud = makePublicWriterCloud<PCDTWPointMixedField>(129);

  expectBinaryPublicWriterMatchesScalarPack(cloud);
  EXPECT_EQ(kBinaryWriterScalarHook,
            pcl::io::detail::pcl_rvv_pcd_writer_binary_last_test_hook());
}

TEST(PCDTemplatedWriterProductionDirect,
     BinaryIndicesOverloadKeepsExistingScalarBehavior)
{
  const auto cloud = makePublicWriterCloud<PCDTWPointXYZRGBCompact>(8);
  const pcl::Indices indices = {6, 1, 3};
  const auto expected = packBinaryFieldsForIndices(cloud, indices);

  const std::string path = makeTempPcdPath();
  pcl::io::detail::pcl_rvv_pcd_writer_binary_reset_test_hook();
  pcl::PCDWriter writer;
  EXPECT_EQ(0, writer.writeBinary<PCDTWPointXYZRGBCompact>(path, cloud, indices));
  const auto payload = extractBinaryPcdPayload(readFileBytes(path));
  std::remove(path.c_str());

  EXPECT_EQ(expected, payload);
  EXPECT_EQ(kBinaryWriterNoneHook,
            pcl::io::detail::pcl_rvv_pcd_writer_binary_last_test_hook());
}

TEST(PCDTemplatedWriterBinaryComponentAblation,
     PackBinaryCandidateMatchesScalarForTrailingPadding)
{
  const std::size_t point_count = 257;
  const std::size_t point_step = 20;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packBinaryFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packBinaryFieldsCandidate(cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcdtw::PathKind::BinaryRvv, pcdtw::lastPath());
#else
  EXPECT_EQ(pcdtw::PathKind::BinaryScalarFallback, pcdtw::lastPath());
#endif
}

TEST(PCDTemplatedWriterBinaryComponentAblation,
     PackBinaryCandidateFallsBackForMixedFieldSizes)
{
  const std::size_t point_count = 47;
  const std::size_t point_step = 18;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 2},
      {8, 1},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packBinaryFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packBinaryFieldsCandidate(cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(pcdtw::PathKind::BinaryScalarFallback, pcdtw::lastPath());
}

TEST(PCDTemplatedWriterBinaryTupleSegmentDiagnostic,
     CompactLayoutUsesMemcpyFastPath)
{
  const std::size_t point_count = 257;
  const std::size_t point_step = 16;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packBinaryFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packBinaryFieldsTupleCandidate(
      cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcdtw::PathKind::BinaryTupleMemcpy, pcdtw::lastPath());
#else
  EXPECT_EQ(pcdtw::PathKind::BinaryTupleScalarFallback, pcdtw::lastPath());
#endif
}

TEST(PCDTemplatedWriterBinaryTupleSegmentDiagnostic,
     PaddingLayoutUsesSegmentStorePath)
{
  const std::size_t point_count = 193;
  const std::size_t point_step = 20;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 4},
      {8, 4},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packBinaryFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packBinaryFieldsTupleCandidate(
      cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
#if defined(__RVV10__)
  EXPECT_EQ(pcdtw::PathKind::BinaryTupleSegmentRvv, pcdtw::lastPath());
#else
  EXPECT_EQ(pcdtw::PathKind::BinaryTupleScalarFallback, pcdtw::lastPath());
#endif
}

TEST(PCDTemplatedWriterBinaryTupleSegmentDiagnostic,
     FallsBackForMixedFieldSizes)
{
  const std::size_t point_count = 47;
  const std::size_t point_step = 18;
  const std::vector<pcdtw::FieldLayout> fields = {
      {0, 4},
      {4, 2},
      {8, 1},
      {12, 4},
  };
  const auto cloud = pcdtw::makePointMajorCloud(point_count, point_step, fields);

  std::vector<std::uint8_t> expected;
  std::vector<std::uint8_t> actual;
  pcdtw::packBinaryFieldsScalar(cloud.data(), point_count, point_step, fields, expected);
  pcdtw::resetLastPath();
  pcdtw::packBinaryFieldsTupleCandidate(
      cloud.data(), point_count, point_step, fields, actual);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(pcdtw::PathKind::BinaryTupleScalarFallback, pcdtw::lastPath());
}
