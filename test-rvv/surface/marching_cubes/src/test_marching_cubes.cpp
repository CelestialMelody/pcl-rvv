/*
 * 本文件做什么：
 * 这些测试验证 marching_cubes 的 test-only production-shaped diagnostic
 * （测试专用生产形态诊断）是否和标量 reference path（参考链路）保持一致。
 * 它们覆盖 dense sphere、wave surface、带 NaN hole 的 sparse sphere、generic
 * xyz AoS 点型和 fallback 回退；失败表示 RVV active-cell prepass 已经破坏
 * createSurface 风格输出语义或 production dispatch 边界。
 */

#include "marching_cubes.h"

#include <pcl/rvv_point_traits.h>
#include <pcl/test/gtest.h>
#include <pcl/surface/marching_cubes.h>

namespace mc = pcl::surface::rvv_marching_cubes_support;

struct EIGEN_ALIGN16 LocalXYZFallback
{
  PCL_ADD_POINT4D
  virtual ~LocalXYZFallback() = default;
  LocalXYZFallback() : LocalXYZFallback(0.f, 0.f, 0.f) {}
  LocalXYZFallback(float _x, float _y, float _z)
  {
    x = _x;
    y = _y;
    z = _z;
    data[3] = 1.0f;
  }
  PCL_MAKE_ALIGNED_OPERATOR_NEW
};

POINT_CLOUD_REGISTER_POINT_STRUCT(LocalXYZFallback, (float, x, x)(float, y, y)(float, z, z))

namespace {

template <typename PointT>
class SyntheticMarchingCubes final : public pcl::MarchingCubes<PointT> {
public:
  SyntheticMarchingCubes(const mc::GridSpec& spec, std::vector<float> grid)
  : pcl::MarchingCubes<PointT>(0.0f, spec.iso_level), grid_values_(std::move(grid))
  {
    this->setGridResolution(spec.res_x, spec.res_y, spec.res_z);
    auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
    cloud->resize(2);
    (*cloud)[0].x = spec.lower[0];
    (*cloud)[0].y = spec.lower[1];
    (*cloud)[0].z = spec.lower[2];
    const Eigen::Array3f upper = spec.lower +
                                 spec.size_voxel *
                                     Eigen::Array3f(static_cast<float>(spec.res_x),
                                                    static_cast<float>(spec.res_y),
                                                    static_cast<float>(spec.res_z));
    (*cloud)[1].x = upper[0];
    (*cloud)[1].y = upper[1];
    (*cloud)[1].z = upper[2];
    this->setInputCloud(cloud);
  }

private:
  void
  voxelizeData() override
  {
    this->grid_ = grid_values_;
  }

  std::vector<float> grid_values_;
};

template <typename PointT>
std::uint64_t
updatePointChecksum(const PointT& point, std::uint64_t checksum)
{
  checksum = mc::mixChecksum(checksum, mc::floatBits(point.x));
  checksum = mc::mixChecksum(checksum, mc::floatBits(point.y));
  checksum = mc::mixChecksum(checksum, mc::floatBits(point.z));
  return checksum;
}

template <typename PointT>
std::uint64_t
productionChecksum(const pcl::PointCloud<PointT>& points,
                   const std::vector<pcl::Vertices>& polygons,
                   const std::size_t active_cells)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto& point : points)
    checksum = updatePointChecksum(point, checksum);
  checksum = mc::mixChecksum(checksum, points.size());
  checksum = mc::mixChecksum(checksum, polygons.size());
  checksum = mc::mixChecksum(checksum, active_cells);
  return checksum;
}

void
expectCandidateMatchesReference(const int resolution, const mc::GridKind kind)
{
  const auto spec = mc::makeGridSpec(resolution);
  const auto grid = mc::makeGrid(spec, kind);
  const auto reference = mc::runReference(spec, grid);
  const auto candidate = mc::runCandidate(spec, grid);
  const auto prepass = mc::runPrepassCandidate(spec, grid);

  EXPECT_TRUE(mc::sameStats(reference, candidate))
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution
      << " reference checksum=" << reference.checksum
      << " candidate checksum=" << candidate.checksum;
  EXPECT_TRUE(mc::sameStats(reference, prepass))
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution
      << " reference checksum=" << reference.checksum
      << " prepass checksum=" << prepass.checksum;
  EXPECT_GT(reference.visited_cells, 0u);
  EXPECT_GT(reference.active_cells, 0u);
  EXPECT_EQ(reference.points, reference.triangles * 3u);
}

void
expectProductionMatchesReference(const int resolution, const mc::GridKind kind)
{
  const auto spec = mc::makeGridSpec(resolution);
  const auto grid = mc::makeGrid(spec, kind);
  const auto reference = mc::runReference(spec, grid);
  SyntheticMarchingCubes<pcl::PointNormal> production(spec, grid);
  pcl::PointCloud<pcl::PointNormal> points;
  std::vector<pcl::Vertices> polygons;
  production.reconstruct(points, polygons);

  EXPECT_EQ(points.size(), reference.points)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
  EXPECT_EQ(polygons.size(), reference.triangles)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
  EXPECT_EQ(productionChecksum(points, polygons, reference.active_cells), reference.checksum)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
}

template <typename PointT>
void
expectGenericProductionMatchesReference(const int resolution, const mc::GridKind kind)
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value);
  const auto spec = mc::makeGridSpec(resolution);
  const auto grid = mc::makeGrid(spec, kind);
  const auto reference = mc::runReference(spec, grid);
  SyntheticMarchingCubes<PointT> production(spec, grid);
  pcl::PointCloud<PointT> points;
  std::vector<pcl::Vertices> polygons;
  production.reconstruct(points, polygons);

  EXPECT_EQ(points.size(), reference.points)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
  EXPECT_EQ(polygons.size(), reference.triangles)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
  EXPECT_EQ(productionChecksum(points, polygons, reference.active_cells), reference.checksum)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
}

void
expectNonAoSXYZFallbackMatchesReference(const int resolution, const mc::GridKind kind)
{
  static_assert(!pcl::rvv::RVVXYZAoSFloatLayout<LocalXYZFallback>::value);
  const auto spec = mc::makeGridSpec(resolution);
  const auto grid = mc::makeGrid(spec, kind);
  const auto reference = mc::runReference(spec, grid);
  SyntheticMarchingCubes<LocalXYZFallback> fallback(spec, grid);
  pcl::PointCloud<LocalXYZFallback> points;
  std::vector<pcl::Vertices> polygons;
  fallback.reconstruct(points, polygons);

  EXPECT_EQ(points.size(), reference.points)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
  EXPECT_EQ(polygons.size(), reference.triangles)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
  EXPECT_EQ(productionChecksum(points, polygons, reference.active_cells), reference.checksum)
      << "kind=" << mc::gridKindName(kind) << " resolution=" << resolution;
}

} // namespace

TEST(MarchingCubesRVV, CandidateMatchesDenseSphereReference)
{
  expectCandidateMatchesReference(24, mc::GridKind::sphere);
  expectCandidateMatchesReference(40, mc::GridKind::sphere);
}

TEST(MarchingCubesRVV, CandidateMatchesWaveReference)
{
  expectCandidateMatchesReference(32, mc::GridKind::wave);
}

TEST(MarchingCubesRVV, CandidatePreservesNaNCellSkip)
{
  expectCandidateMatchesReference(36, mc::GridKind::sparse_sphere);
}

TEST(MarchingCubesRVV, ProductionDirectMatchesReference)
{
  expectProductionMatchesReference(24, mc::GridKind::sphere);
  expectProductionMatchesReference(32, mc::GridKind::wave);
  expectProductionMatchesReference(36, mc::GridKind::sparse_sphere);
}

TEST(MarchingCubesRVV, GenericXYZPointTypesMatchReference)
{
  expectGenericProductionMatchesReference<pcl::PointXYZ>(24, mc::GridKind::sphere);
  expectGenericProductionMatchesReference<pcl::PointXYZI>(24, mc::GridKind::wave);
  expectGenericProductionMatchesReference<pcl::PointXYZRGB>(24, mc::GridKind::sphere);
  expectGenericProductionMatchesReference<pcl::PointXYZRGBA>(24, mc::GridKind::sparse_sphere);
}

TEST(MarchingCubesRVV, NonAoSXYZFallsBackToScalarReference)
{
  expectNonAoSXYZFallbackMatchesReference(24, mc::GridKind::sphere);
  expectNonAoSXYZFallbackMatchesReference(32, mc::GridKind::wave);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
