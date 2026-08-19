/*
 * 本文件做什么：
 * 这里的测试验证 organized_fast_mesh 的 test-only RVV candidate（测试专用 RVV 候选）
 * 是否保持和标量参考链路、PCL 公开入口一致的 polygon 输出顺序，并检查生产
 * 分流的主要 fallback（回退路径）不会误吞未覆盖形态。
 *
 * 证据边界：
 * 这些 TEST 是 correctness（正确性）证据。public-path case（公开入口用例）会通过
 * OrganizedFastMesh 公开入口运行；它能证明输出语义和 fallback 边界，但性能仍要看板卡 bench。
 */

#include "organized_fast_mesh.h"

#include <pcl/test/gtest.h>

namespace support = pcl::surface::rvv_ofm_support;

namespace {

pcl::PointCloud<pcl::PointXYZRGB>
makeRgbCloud(const pcl::PointCloud<pcl::PointXYZ>& source)
{
  pcl::PointCloud<pcl::PointXYZRGB> result;
  result.width = source.width;
  result.height = source.height;
  result.is_dense = source.is_dense;
  result.resize(source.size());
  for (std::size_t i = 0; i < source.size(); ++i) {
    result[i].x = source[i].x;
    result[i].y = source[i].y;
    result[i].z = source[i].z;
    result[i].r = 7;
    result[i].g = 11;
    result[i].b = 13;
  }
  return result;
}

void
expectCandidateMatchesReference(const int width,
                                const int height,
                                const support::MeshKind kind,
                                const bool inject_invalid)
{
  const auto cloud = support::makeOrganizedCloud(width, height, inject_invalid);
  const auto options = support::makeOptions(width, height, kind);
  const auto reference = support::generateMeshReference(cloud, options);
  const auto candidate = support::generateMeshCandidate(cloud, options);

  EXPECT_TRUE(support::samePolygons(reference.polygons, candidate.polygons))
      << support::meshKindName(kind) << " candidate diverged from scalar reference";
  EXPECT_EQ(reference.checksum, candidate.checksum);
}

void
expectReferenceMatchesPclPublicPath(const int width,
                                    const int height,
                                    const support::MeshKind kind,
                                    const bool inject_invalid)
{
  const auto cloud = support::makeOrganizedCloud(width, height, inject_invalid);
  const auto options = support::makeOptions(width, height, kind);
  const auto reference = support::generateMeshReference(cloud, options);
  const auto public_result = support::generateMeshPublicPath(cloud, options, true);

  EXPECT_TRUE(support::samePolygons(reference.polygons, public_result.polygons))
      << support::meshKindName(kind) << " reference does not match PCL public path";
  EXPECT_EQ(reference.checksum, public_result.checksum);
}

}  // namespace

TEST(OrganizedFastMeshRVV, CandidateMatchesScalarForFiniteAndInvalidPoints)
{
  for (const auto kind : {support::MeshKind::quad,
                         support::MeshKind::right_cut,
                         support::MeshKind::left_cut,
                         support::MeshKind::adaptive_cut}) {
    expectCandidateMatchesReference(19, 23, kind, true);
    expectCandidateMatchesReference(64, 32, kind, false);
  }
}

TEST(OrganizedFastMeshRVV, ReferenceMatchesPublicPathWhenShadowChecksAreDisabled)
{
  for (const auto kind : {support::MeshKind::quad,
                         support::MeshKind::right_cut,
                         support::MeshKind::left_cut,
                         support::MeshKind::adaptive_cut}) {
    expectReferenceMatchesPclPublicPath(11, 9, kind, true);
  }
}

TEST(OrganizedFastMeshRVV, CandidatePreservesAdaptiveCutOrder)
{
  expectCandidateMatchesReference(127, 61, support::MeshKind::adaptive_cut, true);
}

TEST(OrganizedFastMeshRVV, PublicPathMatchesReferenceInsideProductionGate)
{
  for (const auto kind : {support::MeshKind::quad,
                         support::MeshKind::right_cut,
                         support::MeshKind::left_cut,
                         support::MeshKind::adaptive_cut}) {
    expectReferenceMatchesPclPublicPath(71, 37, kind, true);
  }
}

TEST(OrganizedFastMeshRVV, PublicPathFallbacksKeepScalarSemantics)
{
  {
    const auto cloud = support::makeOrganizedCloud(7, 7, true);
    const auto options = support::makeOptions(7, 7, support::MeshKind::adaptive_cut);
    const auto reference = support::generateMeshReference(cloud, options);
    const auto public_result = support::generateMeshPublicPath(cloud, options, true);
    EXPECT_TRUE(support::samePolygons(reference.polygons, public_result.polygons));
    EXPECT_EQ(reference.checksum, public_result.checksum);
  }

  {
    auto options = support::makeOptions(17, 13, support::MeshKind::right_cut);
    options.row_step = 2;
    options.column_step = 1;
    const auto cloud = support::makeOrganizedCloud(options.width, options.height, true);
    const auto reference = support::generateMeshReference(cloud, options);
    const auto public_result = support::generateMeshPublicPath(cloud, options, true);
    EXPECT_TRUE(support::samePolygons(reference.polygons, public_result.polygons));
    EXPECT_EQ(reference.checksum, public_result.checksum);
  }

  {
    const auto xyz_cloud = support::makeOrganizedCloud(23, 19, true);
    const auto rgb_cloud = makeRgbCloud(xyz_cloud);
    const auto options = support::makeOptions(23, 19, support::MeshKind::left_cut);
    const auto reference = support::generateMeshReference(rgb_cloud, options);
    const auto public_result = support::generateMeshPublicPath(rgb_cloud, options, true);
    EXPECT_TRUE(support::samePolygons(reference.polygons, public_result.polygons));
    EXPECT_EQ(reference.checksum, public_result.checksum);
  }
}

TEST(OrganizedFastMeshRVV, PublicPathShadowChecksFallbackSmoke)
{
  const auto cloud = support::makeOrganizedCloud(29, 17, false);
  const auto options = support::makeOptions(29, 17, support::MeshKind::quad);
  const auto public_result = support::generateMeshPublicPath(cloud, options, false);
  EXPECT_EQ(public_result.checksum, support::polygonChecksum(public_result.polygons));
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
