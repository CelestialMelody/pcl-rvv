#include <pcl/filters/shadowpoints.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void
expectTrue(bool condition, const std::string& message)
{
  if (!condition)
    throw std::runtime_error(message);
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(std::size_t n)
{
  auto cloud = std::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = false;
  cloud->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 211) - 105) / 10.0f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 7) % 223) - 111) / 13.0f;
    (*cloud)[i].z = (i % 5 == 0) ? 0.0f : static_cast<float>(static_cast<int>((i * 13) % 197) - 98) / 17.0f;
  }
  if (n > 90)
    (*cloud)[90].x = std::numeric_limits<float>::quiet_NaN();
  return cloud;
}

pcl::PointCloud<pcl::PointNormal>::Ptr
makeNormals(std::size_t n)
{
  auto normals = std::make_shared<pcl::PointCloud<pcl::PointNormal>>();
  normals->width = static_cast<std::uint32_t>(n);
  normals->height = 1;
  normals->is_dense = false;
  normals->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*normals)[i].normal_x = (i % 3 == 0) ? 0.0f : 0.18f;
    (*normals)[i].normal_y = (i % 4 == 0) ? 0.0f : -0.11f;
    (*normals)[i].normal_z = (i % 2 == 0) ? 1.0f : 0.42f;
  }
  if (n > 90)
    (*normals)[90].normal_z = 1.0f;
  return normals;
}

pcl::PointCloud<pcl::Normal>::Ptr
makePlainNormals(std::size_t n)
{
  auto normals = std::make_shared<pcl::PointCloud<pcl::Normal>>();
  normals->width = static_cast<std::uint32_t>(n);
  normals->height = 1;
  normals->is_dense = true;
  normals->points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    (*normals)[i].normal_x = 0.0f;
    (*normals)[i].normal_y = 0.0f;
    (*normals)[i].normal_z = (i % 2 == 0) ? 1.0f : 0.25f;
  }
  return normals;
}

pcl::IndicesPtr
makeSubset(std::size_t n)
{
  auto indices = std::make_shared<pcl::Indices>();
  indices->reserve(n / 3);
  for (std::size_t i = 1; i < n; i += 3)
    indices->push_back(static_cast<int>(i));
  return indices;
}

template <typename PointT, typename NormalT>
pcl::Indices
filterIndices(const typename pcl::PointCloud<PointT>::Ptr& cloud,
              const typename pcl::PointCloud<NormalT>::Ptr& normals,
              bool negative = false,
              bool extract_removed = false,
              const pcl::IndicesPtr& subset = {})
{
  pcl::ShadowPoints<PointT, NormalT> filter(extract_removed);
  filter.setInputCloud(cloud);
  filter.setNormals(normals);
  filter.setThreshold(0.1f);
  filter.setNegative(negative);
  if (subset)
    filter.setIndices(subset);

  pcl::Indices indices;
  filter.filter(indices);
  if (extract_removed) {
    const auto removed = filter.getRemovedIndices();
    expectTrue(removed != nullptr, "removed indices pointer is null");
    expectTrue(indices.size() + removed->size() == (subset ? subset->size() : cloud->size()),
               "kept + removed count mismatch");
  }
  return indices;
}

pcl::Indices
referenceIndices(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                 const pcl::PointCloud<pcl::PointNormal>& normals,
                 bool negative,
                 const pcl::Indices& input_indices)
{
  pcl::Indices out;
  for (const int idx : input_indices) {
    const auto& pt = cloud[idx];
    const auto& normal = normals[idx];
    const float val = std::abs(normal.normal_x * pt.x + normal.normal_y * pt.y + normal.normal_z * pt.z);
    if ((val >= 0.1f) ^ negative)
      out.push_back(idx);
  }
  return out;
}

pcl::Indices
identityIndices(std::size_t n)
{
  pcl::Indices indices(n);
  for (std::size_t i = 0; i < n; ++i)
    indices[i] = static_cast<int>(i);
  return indices;
}

void
expectEqual(const pcl::Indices& lhs, const pcl::Indices& rhs, const std::string& label)
{
  expectTrue(lhs.size() == rhs.size(), label + " size mismatch");
  for (std::size_t i = 0; i < lhs.size(); ++i)
    expectTrue(lhs[i] == rhs[i], label + " value mismatch at " + std::to_string(i));
}

void
testFullCloudIndices()
{
  const auto cloud = makeCloud(4096);
  const auto normals = makeNormals(cloud->size());
  const auto got = filterIndices<pcl::PointXYZ, pcl::PointNormal>(cloud, normals);
  const auto ref = referenceIndices(*cloud, *normals, false, identityIndices(cloud->size()));
  expectEqual(got, ref, "full cloud indices");
}

void
testNegativeAndRemoved()
{
  const auto cloud = makeCloud(4096);
  const auto normals = makeNormals(cloud->size());
  const auto got = filterIndices<pcl::PointXYZ, pcl::PointNormal>(cloud, normals, true, true);
  const auto ref = referenceIndices(*cloud, *normals, true, identityIndices(cloud->size()));
  expectEqual(got, ref, "negative indices");
}

void
testSubsetFallback()
{
  const auto cloud = makeCloud(2048);
  const auto normals = makeNormals(cloud->size());
  const auto subset = makeSubset(cloud->size());
  const auto got = filterIndices<pcl::PointXYZ, pcl::PointNormal>(cloud, normals, false, true, subset);
  const auto ref = referenceIndices(*cloud, *normals, false, *subset);
  expectEqual(got, ref, "subset fallback");
}

void
testNormalTypeFallback()
{
  const auto cloud = makeCloud(2048);
  const auto normals = makePlainNormals(cloud->size());
  const auto got = filterIndices<pcl::PointXYZ, pcl::Normal>(cloud, normals);
  expectTrue(!got.empty(), "plain Normal fallback produced no points");
}

void
testCloudOutputFallback()
{
  const auto cloud = makeCloud(257);
  const auto normals = makeNormals(cloud->size());
  pcl::ShadowPoints<pcl::PointXYZ, pcl::PointNormal> filter(true);
  filter.setInputCloud(cloud);
  filter.setNormals(normals);
  filter.setThreshold(0.1f);
  filter.setKeepOrganized(true);

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);
  expectTrue(output.size() == cloud->size(), "keep organized output size mismatch");
  expectTrue(filter.getRemovedIndices()->size() + filterIndices<pcl::PointXYZ, pcl::PointNormal>(cloud, normals).size() == cloud->size(),
             "cloud output removed count mismatch");
}

void
testRvvThenFallbackSequence()
{
  const auto cloud = makeCloud(4096);
  const auto normals = makeNormals(cloud->size());
  const auto main_path = filterIndices<pcl::PointXYZ, pcl::PointNormal>(cloud, normals);
  expectTrue(!main_path.empty(), "main path produced no points");

  const auto subset = makeSubset(cloud->size());
  const auto fallback = filterIndices<pcl::PointXYZ, pcl::PointNormal>(cloud, normals, false, false, subset);
  const auto ref = referenceIndices(*cloud, *normals, false, *subset);
  expectEqual(fallback, ref, "fallback after main path");
}

} // namespace

int
main()
{
  try {
    testFullCloudIndices();
    testNegativeAndRemoved();
    testSubsetFallback();
    testNormalTypeFallback();
    testCloudOutputFallback();
    testRvvThenFallbackSequence();
  } catch (const std::exception& e) {
    std::cerr << "[FAIL] " << e.what() << '\n';
    return 1;
  }

  std::cout << "[PASS] filters/shadowpoints RVV tests\n";
  return 0;
}
