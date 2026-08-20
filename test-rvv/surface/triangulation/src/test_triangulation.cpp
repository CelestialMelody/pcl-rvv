/*
 * 本文件做什么：
 * 这些 TEST 验证 triangulation 的测试专用 RVV candidate（候选实现）是否
 * 与标量 reference path（参考链路）保持同样的参数网格、triangle 输出顺序
 * 和未裁剪 surface Evaluate 结果。
 *
 * 证据边界：
 * 这里是 correctness（正确性）证据，不是性能证据，也不证明 production
 * dispatch（生产分流）。production 源码保持未修改。
 */

#include "triangulation.h"

#include <pcl/test/gtest.h>

namespace tri = pcl::surface::rvv_triangulation_support;

TEST(TriangulationRVV, ParamGridCandidateMatchesScalarReference)
{
  for (const unsigned resolution : {1u, 2u, 7u, 31u, 96u}) {
    const auto reference = tri::runParamGridReference(resolution);
    const auto candidate = tri::runParamGridCandidate(resolution);
    EXPECT_EQ(reference.points, candidate.points) << resolution;
    EXPECT_EQ(reference.polygons, candidate.polygons) << resolution;
    EXPECT_EQ(reference.checksum, candidate.checksum) << resolution;
  }
}

TEST(TriangulationRVV, SurfaceCandidateMatchesScalarReference)
{
  for (const unsigned resolution : {1u, 3u, 16u, 64u}) {
    const auto reference = tri::runSurfaceReference(resolution);
    const auto candidate = tri::runSurfaceCandidate(resolution);
    EXPECT_EQ(reference.points, candidate.points) << resolution;
    EXPECT_EQ(reference.polygons, candidate.polygons) << resolution;
    EXPECT_EQ(reference.checksum, candidate.checksum) << resolution;
  }
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
