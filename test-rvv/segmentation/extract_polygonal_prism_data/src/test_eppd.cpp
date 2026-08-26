#include <pcl/test/gtest.h>
#include <pcl/PointIndices.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/segmentation/extract_polygonal_prism_data.h>

#include <eppd.h>

#include <vector>

namespace {

using pcl::PointCloud;
using pcl::PointXYZ;

PointCloud<PointXYZ>
makeGridCloud()
{
  PointCloud<PointXYZ> cloud;
  cloud.reserve(36);
  for (int y = -3; y <= 2; ++y) {
    for (int x = -3; x <= 2; ++x) {
      const float z = ((x + y) % 3 == 0) ? 0.25f : 0.02f;
      cloud.push_back(PointXYZ(static_cast<float>(x), static_cast<float>(y), z));
    }
  }
  return cloud;
}

std::vector<int>
makeOrderedIndices(std::size_t n)
{
  std::vector<int> indices(n);
  for (std::size_t i = 0; i < n; ++i) {
    indices[i] = static_cast<int>(i);
  }
  return indices;
}

std::vector<PointXYZ>
makeSquare()
{
  return {
      PointXYZ(-1.5f, -1.5f, 0.0f),
      PointXYZ(1.5f, -1.5f, 0.0f),
      PointXYZ(1.5f, 1.5f, 0.0f),
      PointXYZ(-1.5f, 1.5f, 0.0f),
  };
}

std::vector<PointXYZ>
makeTiltedTriangle()
{
  return {
      PointXYZ(0.0f, 0.0f, 0.0f),
      PointXYZ(2.0f, 0.0f, 0.0f),
      PointXYZ(0.0f, 2.0f, 0.0f),
  };
}

PointCloud<PointXYZ>::Ptr
makeProductionCloud(std::size_t n)
{
  auto cloud = PointCloud<PointXYZ>::Ptr(new PointCloud<PointXYZ>);
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const int x_code = static_cast<int>((i * 17) % 97) - 48;
    const int y_code = static_cast<int>((i * 29) % 89) - 44;
    (*cloud)[i].x = static_cast<float>(x_code) * 0.05f;
    (*cloud)[i].y = static_cast<float>(y_code) * 0.05f;
    (*cloud)[i].z = (i % 11 == 0) ? 0.16f : 0.02f;
  }
  return cloud;
}

PointCloud<PointXYZ>::Ptr
makeProductionHull()
{
  auto hull = PointCloud<PointXYZ>::Ptr(new PointCloud<PointXYZ>);
  hull->push_back(PointXYZ(-1.6f, -1.6f, 0.0f));
  hull->push_back(PointXYZ(1.6f, -1.6f, 0.0f));
  hull->push_back(PointXYZ(1.6f, 1.6f, 0.0f));
  hull->push_back(PointXYZ(-1.6f, 1.6f, 0.0f));
  hull->width = static_cast<std::uint32_t>(hull->size());
  hull->height = 1;
  hull->is_dense = true;
  return hull;
}

PointCloud<PointXYZ>::Ptr
makeNestedProductionHull()
{
  auto hull = PointCloud<PointXYZ>::Ptr(new PointCloud<PointXYZ>);
  const auto add_point = [&hull](float x, float y) {
    hull->push_back(PointXYZ(x, y, 0.0f));
  };
  add_point(-1.6f, -1.6f);
  add_point(1.6f, -1.6f);
  add_point(1.6f, 1.6f);
  add_point(-1.6f, 1.6f);
  add_point(-0.35f, -0.35f);
  add_point(0.35f, -0.35f);
  add_point(0.35f, 0.35f);
  add_point(-0.35f, 0.35f);
  hull->width = static_cast<std::uint32_t>(hull->size());
  hull->height = 1;
  hull->is_dense = true;
  return hull;
}

std::vector<pcl::Vertices>
makeNestedProductionPolygons()
{
  pcl::Vertices outer;
  outer.vertices = {0, 1, 2, 3};
  pcl::Vertices inner;
  inner.vertices = {4, 5, 6, 7};
  return {outer, inner};
}

std::vector<int>
makeIndexedSubset(std::size_t n)
{
  std::vector<int> indices(n);
  if (n == 0) {
    return indices;
  }
  const std::size_t step = (n % 2 == 0) ? (n - 1) : (n - 2);
  for (std::size_t i = 0; i < n; ++i) {
    indices[i] = static_cast<int>((i * step + 1) % n);
  }
  return indices;
}

template <typename PointT>
class TestableExtractPolygonalPrismDataT
: public pcl::ExtractPolygonalPrismData<PointT> {
public:
#ifdef __RVV10__
  using pcl::ExtractPolygonalPrismData<PointT>::segmentRvv;
#endif
  using pcl::ExtractPolygonalPrismData<PointT>::segmentStd;
};

using TestableExtractPolygonalPrismData =
    TestableExtractPolygonalPrismDataT<PointXYZ>;

TestableExtractPolygonalPrismData
makeConfiguredPrism(const PointCloud<PointXYZ>::Ptr& cloud,
                    const std::vector<int>& indices)
{
  TestableExtractPolygonalPrismData prism;
  prism.setInputCloud(cloud);
  prism.setIndices(std::make_shared<std::vector<int>>(indices));
  prism.setInputPlanarHull(makeProductionHull());
  prism.setHeightLimits(-0.05, 0.10);
  prism.setViewPoint(0.0f, 0.0f, 2.0f);
  return prism;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeProductionCloudT(std::size_t n)
{
  auto cloud = typename pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const int x_code = static_cast<int>((i * 17) % 97) - 48;
    const int y_code = static_cast<int>((i * 29) % 89) - 44;
    (*cloud)[i].x = static_cast<float>(x_code) * 0.05f;
    (*cloud)[i].y = static_cast<float>(y_code) * 0.05f;
    (*cloud)[i].z = (i % 11 == 0) ? 0.16f : 0.02f;
  }
  return cloud;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeProductionHullT()
{
  auto hull = typename pcl::PointCloud<PointT>::Ptr(new pcl::PointCloud<PointT>);
  const auto add_point = [&hull](float x, float y) {
    PointT point;
    point.x = x;
    point.y = y;
    point.z = 0.0f;
    hull->push_back(point);
  };
  add_point(-1.6f, -1.6f);
  add_point(1.6f, -1.6f);
  add_point(1.6f, 1.6f);
  add_point(-1.6f, 1.6f);
  hull->width = static_cast<std::uint32_t>(hull->size());
  hull->height = 1;
  hull->is_dense = true;
  return hull;
}

template <typename PointT>
TestableExtractPolygonalPrismDataT<PointT>
makeConfiguredPrismT(const typename pcl::PointCloud<PointT>::Ptr& cloud,
                     const std::vector<int>& indices)
{
  TestableExtractPolygonalPrismDataT<PointT> prism;
  prism.setInputCloud(cloud);
  prism.setIndices(std::make_shared<std::vector<int>>(indices));
  prism.setInputPlanarHull(makeProductionHullT<PointT>());
  prism.setHeightLimits(-0.05, 0.10);
  prism.setViewPoint(0.0f, 0.0f, 2.0f);
  return prism;
}

template <typename PointT>
void
expectSegmentRvvMatchesSegmentStdForPointType()
{
  const auto cloud = makeProductionCloudT<PointT>(160);
  const auto indices = makeOrderedIndices(cloud->size());

  auto std_prism = makeConfiguredPrismT<PointT>(cloud, indices);
  pcl::PointIndices expected;
  std_prism.segmentStd(expected);

  auto rvv_prism = makeConfiguredPrismT<PointT>(cloud, indices);
  pcl::PointIndices actual;
  ASSERT_TRUE(rvv_prism.segmentRvv(actual));

  EXPECT_EQ(actual.indices, expected.indices);
}

} // namespace

// 这个测试先锁定投影后扫描段的基础语义：高度范围必须用原始点的 z，
// 二维 polygon 判定使用投影点的 x/y，输出 indices（索引）必须保持标量扫描顺序。
TEST(ExtractPolygonalPrismDataRvvDiagnostic, KeepsScalarOrderForSquarePrism)
{
  const auto cloud = makeGridCloud();
  const auto indices = makeOrderedIndices(cloud.size());
  const std::vector<std::vector<PointXYZ>> polygons = {makeSquare()};

  const auto out = eppd::segmentPolygonalPrismReference(
      cloud.points, indices, polygons, -0.05f, 0.10f);

  EXPECT_EQ(out, (std::vector<int>{14, 15, 20, 22, 27, 28}));
}

// 这个测试覆盖 concave hull（凹包）在 production 中使用的 XOR 语义。两个 ring
// polygon 按奇偶关系排除洞内点，失败时说明后续 RVV candidate 不能接近真实 segment 行为。
TEST(ExtractPolygonalPrismDataRvvDiagnostic, AppliesXorAcrossNestedPolygons)
{
  const std::vector<PointXYZ> cloud = {
      PointXYZ(0.0f, 0.0f, 0.0f),
      PointXYZ(0.6f, 0.0f, 0.0f),
      PointXYZ(2.0f, 0.0f, 0.0f),
      PointXYZ(0.0f, 0.0f, 0.2f),
  };
  const std::vector<int> indices = {0, 1, 2, 3};
  const std::vector<PointXYZ> outer = {
      PointXYZ(-1.0f, -1.0f, 0.0f),
      PointXYZ(1.0f, -1.0f, 0.0f),
      PointXYZ(1.0f, 1.0f, 0.0f),
      PointXYZ(-1.0f, 1.0f, 0.0f),
  };
  const std::vector<PointXYZ> inner = {
      PointXYZ(-0.25f, -0.25f, 0.0f),
      PointXYZ(0.25f, -0.25f, 0.0f),
      PointXYZ(0.25f, 0.25f, 0.0f),
      PointXYZ(-0.25f, 0.25f, 0.0f),
  };

  const auto out = eppd::segmentPolygonalPrismReference(
      cloud, indices, {outer, inner}, -0.05f, 0.10f);

  EXPECT_EQ(out, (std::vector<int>{1}));
}

#ifdef __RVV10__
// 这个测试故意把平面系数和投影坐标都换成非 XY 退化形态，逼迫 helper 不能再
// 只靠 z 范围或 fixed XY 语义通过。失败时说明当前 RVV candidate 还停留在 Phase 000。
TEST(ExtractPolygonalPrismDataRvvDiagnostic, FullScanNeedsPlaneAndProjectedCoordinates)
{
  const std::vector<PointXYZ> cloud = {
      PointXYZ(0.0f, 0.0f, 1.0f),
      PointXYZ(1.0f, 0.0f, 1.0f),
      PointXYZ(0.0f, 1.0f, 1.0f),
      PointXYZ(1.0f, 1.0f, 1.0f),
  };
  const std::vector<int> indices = {0, 1, 2, 3};
  const std::vector<PointXYZ> polygon = makeTiltedTriangle();
  const std::vector<std::vector<PointXYZ>> polygons = {polygon};

  const Eigen::Vector4f model_coefficients(0.5f, 0.5f, 0.0f, 0.0f);
  const std::vector<PointXYZ> projected_points = {
      PointXYZ(0.25f, 0.25f, 0.0f),
      PointXYZ(1.25f, 0.25f, 0.0f),
      PointXYZ(0.25f, 1.25f, 0.0f),
      PointXYZ(1.25f, 1.25f, 0.0f),
  };

  const auto expected = std::vector<int>{0, 1, 2};
  eppd::CandidatePath path = eppd::CandidatePath::ReferenceFallback;
  const auto actual = eppd::segmentPolygonalPrismRvvFullScanCandidate(
      cloud,
      indices,
      polygons,
      projected_points,
      model_coefficients,
      -0.1f,
      0.6f,
      0,
      1,
      &path);

  EXPECT_EQ(actual, expected);
  EXPECT_EQ(path, eppd::CandidatePath::RvvDense);
}

// production `segment` 使用 indices 中的源点编号访问 input cloud，同时 projected
// points 仍按扫描顺序排列。这个测试防止 indexed 路径把 lane id 当成 source index 输出。
TEST(ExtractPolygonalPrismDataRvvDiagnostic, FullScanIndexedPathKeepsSourceIndices)
{
  const std::vector<PointXYZ> cloud = {
      PointXYZ(-5.0f, -5.0f, 0.0f),
      PointXYZ(0.0f, 0.0f, 0.0f),
      PointXYZ(2.0f, 0.0f, 0.0f),
      PointXYZ(0.0f, 2.0f, 0.0f),
      PointXYZ(3.0f, 3.0f, 0.0f),
  };
  const std::vector<int> indices = {1, 2, 3};
  const std::vector<std::vector<PointXYZ>> polygons = {makeTiltedTriangle()};
  const std::vector<PointXYZ> projected_points = {
      PointXYZ(0.25f, 0.25f, 0.0f),
      PointXYZ(1.25f, 0.25f, 0.0f),
      PointXYZ(0.25f, 1.25f, 0.0f),
  };
  const Eigen::Vector4f model_coefficients(0.5f, -0.5f, 0.0f, 0.0f);

  const auto expected = eppd::segmentPolygonalPrismFullScanReference(
      cloud, indices, polygons, projected_points, model_coefficients, -0.1f, 1.1f, 0, 1);
  eppd::CandidatePath path = eppd::CandidatePath::ReferenceFallback;
  const auto actual = eppd::segmentPolygonalPrismRvvFullScanCandidate(
      cloud,
      indices,
      polygons,
      projected_points,
      model_coefficients,
      -0.1f,
      1.1f,
      0,
      1,
      &path);

  EXPECT_EQ(actual, expected);
  EXPECT_EQ(actual, (std::vector<int>{1, 2}));
  EXPECT_EQ(path, eppd::CandidatePath::RvvIndexed);
}

// production direct（真实生产入口）测试先要求生产类暴露清晰的 Std/RVV helper
// 分层。RED 阶段这些 helper 不存在，RVV 构建应编译失败；GREEN 阶段则验证
// 真实 `segment` 准备、`projectPoints` 和输出写回都能保持标量语义。
TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvMatchesSegmentStdForDenseSinglePolygon)
{
  const auto cloud = makeProductionCloud(160);
  const auto indices = makeOrderedIndices(cloud->size());

  auto std_prism = makeConfiguredPrism(cloud, indices);
  pcl::PointIndices expected;
  std_prism.segmentStd(expected);

  auto rvv_prism = makeConfiguredPrism(cloud, indices);
  pcl::PointIndices actual;
  ASSERT_TRUE(rvv_prism.segmentRvv(actual));

  EXPECT_EQ(actual.indices, expected.indices);
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvMatchesSegmentStdForIndexedSinglePolygon)
{
  const auto cloud = makeProductionCloud(160);
  const auto indices = makeIndexedSubset(cloud->size());

  auto std_prism = makeConfiguredPrism(cloud, indices);
  pcl::PointIndices expected;
  std_prism.segmentStd(expected);

  auto rvv_prism = makeConfiguredPrism(cloud, indices);
  pcl::PointIndices actual;
  ASSERT_TRUE(rvv_prism.segmentRvv(actual));

  EXPECT_EQ(actual.indices, expected.indices);
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvMatchesSegmentStdForNestedPolygons)
{
  const auto cloud = makeProductionCloud(160);
  const auto indices = makeOrderedIndices(cloud->size());

  auto std_prism = makeConfiguredPrism(cloud, indices);
  std_prism.setInputPlanarHull(makeNestedProductionHull());
  std_prism.setPolygons(makeNestedProductionPolygons());
  pcl::PointIndices expected;
  std_prism.segmentStd(expected);

  auto rvv_prism = makeConfiguredPrism(cloud, indices);
  rvv_prism.setInputPlanarHull(makeNestedProductionHull());
  rvv_prism.setPolygons(makeNestedProductionPolygons());
  pcl::PointIndices actual;
  ASSERT_TRUE(rvv_prism.segmentRvv(actual));

  EXPECT_EQ(actual.indices, expected.indices);
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvMatchesSegmentStdForIndexedNestedPolygons)
{
  const auto cloud = makeProductionCloud(160);
  const auto indices = makeIndexedSubset(cloud->size());

  auto std_prism = makeConfiguredPrism(cloud, indices);
  std_prism.setInputPlanarHull(makeNestedProductionHull());
  std_prism.setPolygons(makeNestedProductionPolygons());
  pcl::PointIndices expected;
  std_prism.segmentStd(expected);

  auto rvv_prism = makeConfiguredPrism(cloud, indices);
  rvv_prism.setInputPlanarHull(makeNestedProductionHull());
  rvv_prism.setPolygons(makeNestedProductionPolygons());
  pcl::PointIndices actual;
  ASSERT_TRUE(rvv_prism.segmentRvv(actual));

  EXPECT_EQ(actual.indices, expected.indices);
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvDeclinesDegeneratePolygons)
{
  const auto cloud = makeProductionCloud(160);
  const auto indices = makeOrderedIndices(cloud->size());

  auto rvv_prism = makeConfiguredPrism(cloud, indices);
  rvv_prism.setPolygons({pcl::Vertices{}, pcl::Vertices{}});

  pcl::PointIndices output;
  EXPECT_FALSE(rvv_prism.segmentRvv(output));
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvDeclinesSmallInputs)
{
  const auto cloud = makeProductionCloud(16);
  const auto indices = makeOrderedIndices(cloud->size());
  auto rvv_prism = makeConfiguredPrism(cloud, indices);

  pcl::PointIndices output;
  EXPECT_FALSE(rvv_prism.segmentRvv(output));
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvMatchesSegmentStdForPointXYZI)
{
  expectSegmentRvvMatchesSegmentStdForPointType<pcl::PointXYZI>();
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvMatchesSegmentStdForPointXYZRGB)
{
  expectSegmentRvvMatchesSegmentStdForPointType<pcl::PointXYZRGB>();
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvMatchesSegmentStdForPointXYZRGBA)
{
  expectSegmentRvvMatchesSegmentStdForPointType<pcl::PointXYZRGBA>();
}

TEST(ExtractPolygonalPrismDataProductionRvv, SegmentRvvDeclinesPointXYZINormal)
{
  const auto cloud = makeProductionCloudT<pcl::PointXYZINormal>(160);
  const auto indices = makeOrderedIndices(cloud->size());
  auto rvv_prism = makeConfiguredPrismT<pcl::PointXYZINormal>(cloud, indices);

  pcl::PointIndices output;
  EXPECT_FALSE(rvv_prism.segmentRvv(output));
}
#endif

#ifdef __RVV10__
// 这个测试只在 RVV 构建中启用，用同一份输入比较 reference path（参考链路）
// 和 RVV path（实际执行 RVV intrinsic 的链路）。它失败时说明候选不能作为后续 bench 证据。
TEST(ExtractPolygonalPrismDataRvvDiagnostic, RvvCandidateMatchesReferenceForSquarePrism)
{
  const auto cloud = makeGridCloud();
  const auto indices = makeOrderedIndices(cloud.size());
  const std::vector<std::vector<PointXYZ>> polygons = {makeSquare()};

  const auto expected = eppd::segmentPolygonalPrismReference(
      cloud.points, indices, polygons, -0.05f, 0.10f);
  const auto actual = eppd::segmentPolygonalPrismRvvCandidate(
      cloud.points, indices, polygons, -0.05f, 0.10f);

  EXPECT_EQ(actual, expected);
}

// 这个测试保留 test-only candidate（测试专用候选）的多 polygon XOR 对拍；
// production direct（真实生产入口）覆盖在上面的 nested polygon 测试中。
TEST(ExtractPolygonalPrismDataRvvDiagnostic, RvvCandidateFallsBackForNestedPolygons)
{
  const std::vector<PointXYZ> cloud = {
      PointXYZ(0.0f, 0.0f, 0.0f),
      PointXYZ(0.6f, 0.0f, 0.0f),
      PointXYZ(2.0f, 0.0f, 0.0f),
      PointXYZ(0.0f, 0.0f, 0.2f),
  };
  const std::vector<int> indices = {0, 1, 2, 3};
  const std::vector<PointXYZ> outer = {
      PointXYZ(-1.0f, -1.0f, 0.0f),
      PointXYZ(1.0f, -1.0f, 0.0f),
      PointXYZ(1.0f, 1.0f, 0.0f),
      PointXYZ(-1.0f, 1.0f, 0.0f),
  };
  const std::vector<PointXYZ> inner = {
      PointXYZ(-0.25f, -0.25f, 0.0f),
      PointXYZ(0.25f, -0.25f, 0.0f),
      PointXYZ(0.25f, 0.25f, 0.0f),
      PointXYZ(-0.25f, 0.25f, 0.0f),
  };
  const std::vector<std::vector<PointXYZ>> polygons = {outer, inner};

  const auto expected = eppd::segmentPolygonalPrismReference(
      cloud, indices, polygons, -0.05f, 0.10f);
  const auto actual = eppd::segmentPolygonalPrismRvvCandidate(
      cloud, indices, polygons, -0.05f, 0.10f);

  EXPECT_EQ(actual, expected);
}
#endif
