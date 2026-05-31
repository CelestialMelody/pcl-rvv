#include <pcl/PCLPointCloud2.h>
#include <pcl/common/common.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

pcl::PCLPointField
makeFloatField(const std::string& name, std::uint32_t offset)
{
  pcl::PCLPointField field;
  field.name = name;
  field.offset = offset;
  field.datatype = pcl::PCLPointField::FLOAT32;
  field.count = 1;
  return field;
}

void
writeFloat(std::vector<std::uint8_t>& data, std::size_t base, std::uint32_t offset, float value)
{
  std::memcpy(data.data() + base + offset, &value, sizeof(value));
}

pcl::PCLPointCloud2::Ptr
makeCloud(std::size_t n, std::uint32_t point_step, std::uint32_t x_off, std::uint32_t y_off, std::uint32_t z_off)
{
  auto cloud = pcl::make_shared<pcl::PCLPointCloud2>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->is_bigendian = false;
  cloud->point_step = point_step;
  cloud->row_step = point_step * cloud->width;
  cloud->fields = {makeFloatField("x", x_off), makeFloatField("y", y_off), makeFloatField("z", z_off)};
  cloud->data.assign(n * point_step, std::uint8_t{0});

  std::mt19937 rng(1337u + static_cast<std::uint32_t>(n + point_step));
  std::uniform_real_distribution<float> dist(-500.0f, 500.0f);
  for (std::size_t i = 0; i < n; ++i) {
    const float x = dist(rng) + static_cast<float>(i % 7) * 0.25f;
    const float y = dist(rng) - static_cast<float>(i % 11) * 0.5f;
    const float z = dist(rng) + static_cast<float>(i % 13) * 0.75f;
    const std::size_t base = i * point_step;
    writeFloat(cloud->data, base, x_off, x);
    writeFloat(cloud->data, base, y_off, y);
    writeFloat(cloud->data, base, z_off, z);
  }

  if (n > 0) {
    writeFloat(cloud->data, 0, x_off, -1000.0f);
    writeFloat(cloud->data, 0, y_off, 900.0f);
    writeFloat(cloud->data, 0, z_off, -800.0f);
    const std::size_t last = (n - 1) * point_step;
    writeFloat(cloud->data, last, x_off, 1200.0f);
    writeFloat(cloud->data, last, y_off, -1100.0f);
    writeFloat(cloud->data, last, z_off, 1300.0f);
  }

  return cloud;
}

Eigen::Vector4f
readXYZ(const pcl::PCLPointCloud2ConstPtr& cloud, int index)
{
  Eigen::Vector4f point(0.0f, 0.0f, 0.0f, 0.0f);
  const std::size_t base = static_cast<std::size_t>(index) * cloud->point_step;
  std::memcpy(&point[0], cloud->data.data() + base + cloud->fields[0].offset, sizeof(float));
  std::memcpy(&point[1], cloud->data.data() + base + cloud->fields[1].offset, sizeof(float));
  std::memcpy(&point[2], cloud->data.data() + base + cloud->fields[2].offset, sizeof(float));
  return point;
}

std::pair<Eigen::Vector4f, Eigen::Vector4f>
expectedMinMaxForIndices(const pcl::PCLPointCloud2ConstPtr& cloud, const pcl::Indices& indices)
{
  Eigen::Vector4f min_pt(std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         0.0f);
  Eigen::Vector4f max_pt(std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest(),
                         0.0f);
  for (const int index : indices) {
    const Eigen::Vector4f point = readXYZ(cloud, index);
    min_pt = min_pt.cwiseMin(point);
    max_pt = max_pt.cwiseMax(point);
  }
  min_pt[3] = 0.0f;
  max_pt[3] = 0.0f;
  return {min_pt, max_pt};
}

void
expectMinMax(const pcl::PCLPointCloud2ConstPtr& cloud,
             const Eigen::Vector4f& expected_min,
             const Eigen::Vector4f& expected_max)
{
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D<float>(cloud, 0, 1, 2, min_pt, max_pt);

  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(min_pt[i], expected_min[i]);
    EXPECT_FLOAT_EQ(max_pt[i], expected_max[i]);
  }
}

void
expectMinMax(const pcl::PCLPointCloud2ConstPtr& cloud,
             const pcl::Indices& indices,
             const Eigen::Vector4f& expected_min,
             const Eigen::Vector4f& expected_max)
{
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D<float>(cloud, indices, 0, 1, 2, min_pt, max_pt);

  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(min_pt[i], expected_min[i]);
    EXPECT_FLOAT_EQ(max_pt[i], expected_max[i]);
  }
}

pcl::PCLPointCloud2::Ptr
makeCloudWithDistance(std::size_t n)
{
  auto cloud = makeCloud(n, 20, 0, 4, 8);
  cloud->fields.push_back(makeFloatField("distance", 12));
  for (std::size_t i = 0; i < n; ++i) {
    const float distance = static_cast<float>(i % 101) * 0.01f;
    writeFloat(cloud->data, i * cloud->point_step, 12, distance);
  }

  if (n > 16) {
    writeFloat(cloud->data, 10 * cloud->point_step, 0, -2000.0f);
    writeFloat(cloud->data, 10 * cloud->point_step, 4, -2100.0f);
    writeFloat(cloud->data, 10 * cloud->point_step, 8, -2200.0f);
    writeFloat(cloud->data, 10 * cloud->point_step, 12, 0.5f);

    writeFloat(cloud->data, 11 * cloud->point_step, 0, 2300.0f);
    writeFloat(cloud->data, 11 * cloud->point_step, 4, 2400.0f);
    writeFloat(cloud->data, 11 * cloud->point_step, 8, 2500.0f);
    writeFloat(cloud->data, 11 * cloud->point_step, 12, 1.5f);
  }
  return cloud;
}

std::pair<Eigen::Vector4f, Eigen::Vector4f>
expectedMinMaxForDistance(const pcl::PCLPointCloud2ConstPtr& cloud,
                          float min_distance,
                          float max_distance,
                          bool limit_negative)
{
  Eigen::Vector4f min_pt(std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         0.0f);
  Eigen::Vector4f max_pt(std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest(),
                         0.0f);
  for (std::size_t i = 0; i < cloud->width * cloud->height; ++i) {
    float distance = 0.0f;
    std::memcpy(&distance, cloud->data.data() + i * cloud->point_step + cloud->fields[3].offset, sizeof(float));
    if (limit_negative == (distance < max_distance && distance > min_distance))
      continue;
    const Eigen::Vector4f point = readXYZ(cloud, static_cast<int>(i));
    min_pt = min_pt.cwiseMin(point);
    max_pt = max_pt.cwiseMax(point);
  }
  min_pt[3] = 0.0f;
  max_pt[3] = 0.0f;
  return {min_pt, max_pt};
}

void
expectMinMaxDistance(const pcl::PCLPointCloud2ConstPtr& cloud,
                     float min_distance,
                     float max_distance,
                     bool limit_negative)
{
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D<float, float>(cloud,
                                 0,
                                 1,
                                 2,
                                 "distance",
                                 min_distance,
                                 max_distance,
                                 min_pt,
                                 max_pt,
                                 limit_negative);
  const auto expected = expectedMinMaxForDistance(cloud, min_distance, max_distance, limit_negative);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(min_pt[i], expected.first[i]);
    EXPECT_FLOAT_EQ(max_pt[i], expected.second[i]);
  }
}

} // namespace

TEST(VoxelGridGetMinMax3D, DenseFloatPackedXYZ)
{
  const auto cloud = makeCloud(4096, 16, 0, 4, 8);
  expectMinMax(cloud,
               Eigen::Vector4f(-1000.0f, -1100.0f, -800.0f, 0.0f),
               Eigen::Vector4f(1200.0f, 900.0f, 1300.0f, 0.0f));
}

TEST(VoxelGridGetMinMax3D, DenseFloatPaddedXYZ)
{
  const auto cloud = makeCloud(8193, 32, 4, 16, 24);
  expectMinMax(cloud,
               Eigen::Vector4f(-1000.0f, -1100.0f, -800.0f, 0.0f),
               Eigen::Vector4f(1200.0f, 900.0f, 1300.0f, 0.0f));
}

TEST(VoxelGridGetMinMax3D, DenseFloatIndices)
{
  const auto cloud = makeCloud(4096, 16, 0, 4, 8);
  pcl::Indices indices;
  indices.reserve(1024);
  for (int i = 1; i < 2049; i += 2)
    indices.push_back(i);
  indices.push_back(0);

  const auto expected = expectedMinMaxForIndices(cloud, indices);
  expectMinMax(cloud, indices, expected.first, expected.second);
}

TEST(VoxelGridGetMinMax3D, DenseFloatDistanceField)
{
  const auto cloud = makeCloudWithDistance(4096);
  expectMinMaxDistance(cloud, 0.25f, 0.75f, false);
}

TEST(VoxelGridGetMinMax3D, DenseFloatDistanceFieldLimitNegative)
{
  const auto cloud = makeCloudWithDistance(4096);
  expectMinMaxDistance(cloud, 0.25f, 0.75f, true);
}

TEST(VoxelGridGetMinMax3D, SmallDenseCloudUsesSameSemantics)
{
  const auto cloud = makeCloud(7, 20, 0, 8, 16);
  expectMinMax(cloud,
               Eigen::Vector4f(-1000.0f, -1100.0f, -800.0f, 0.0f),
               Eigen::Vector4f(1200.0f, 900.0f, 1300.0f, 0.0f));
}

TEST(VoxelGridGetMinMax3D, NonDenseKeepsFiniteFiltering)
{
  auto cloud = makeCloud(128, 16, 0, 4, 8);
  cloud->is_dense = false;
  writeFloat(cloud->data, 3 * cloud->point_step, 0, -9999.0f);
  writeFloat(cloud->data, 3 * cloud->point_step, 4, std::numeric_limits<float>::quiet_NaN());
  writeFloat(cloud->data, 3 * cloud->point_step, 8, 9999.0f);

  expectMinMax(cloud,
               Eigen::Vector4f(-1000.0f, -1100.0f, -800.0f, 0.0f),
               Eigen::Vector4f(1200.0f, 900.0f, 1300.0f, 0.0f));
}