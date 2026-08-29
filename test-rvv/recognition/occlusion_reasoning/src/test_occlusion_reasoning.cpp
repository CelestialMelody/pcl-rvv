/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 ZBuffering::filter() 的
 * pre-production diagnostic（接入生产前诊断）是否保持 projection、depth
 * compare 和 keep indices 的语义。RVV build 必须命中 RVV candidate；
 * 否则测试失败，防止标量 fallback 被误写成 RVV 证据。
 */

#include "occlusion_reasoning.h"

#include <gtest/gtest.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/recognition/hv/occlusion_reasoning.h>

#include <cmath>
#include <limits>
#include <vector>

namespace occ = pcl::test::occlusion_reasoning_rvv;

namespace
{

std::vector<float>
makeDepthBuffer(const int width, const int height)
{
  std::vector<float> depth(static_cast<std::size_t>(width * height), 4.0f);
  depth[static_cast<std::size_t>(3 * height + 3)] = 2.0f;
  depth[static_cast<std::size_t>(4 * height + 3)] = 1.0f;
  depth[static_cast<std::size_t>(2 * height + 2)] = std::numeric_limits<float>::quiet_NaN();
  return depth;
}

std::vector<occ::ProjectionPoint>
makeFilterFixturePoints()
{
  return {
      {0.0f, 0.0f, 2.0f},   // keep: same depth as buffer at (3,3)
      {1.0f, 0.0f, 2.0f},   // reject: occluded by depth 1.0 at (4,3)
      {0.0f, 0.0f, 2.02f},  // keep: threshold allows this small delta
      {-1.0f, -1.0f, 2.0f}, // reject: invalid depth at (2,2)
      {9.0f, 0.0f, 2.0f},   // reject: out of bounds
      {0.0f, 0.0f, 5.0f},   // reject: clearly behind depth
      {0.0f, 0.0f, 1.5f},   // keep: in front of depth
      {0.0f, 0.0f, 0.0f},   // reject: unsafe projection input
  };
}

pcl::PointXYZ
makePointForProjectionPixel(const int u,
                            const int v,
                            const float z,
                            const int width,
                            const int height,
                            const float focal)
{
  const float cx = static_cast<float>(width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(height) / 2.0f - 0.5f;
  return pcl::PointXYZ{(static_cast<float>(u) - cx) * z / focal,
                       (static_cast<float>(v) - cy) * z / focal,
                       z};
}

} // namespace

TEST(OcclusionReasoningDiagnostic, FilterReferenceKeepsExpectedIndicesAndOrder)
{
  const int width = 7;
  const int height = 7;
  const float focal = 2.0f;
  const float threshold = 0.05f;
  const auto points = makeFilterFixturePoints();
  const auto depth = makeDepthBuffer(width, height);
  std::vector<int> indices;

  occ::filterIndicesScalarReference(
      points.data(), points.size(), depth.data(), width, height, focal, threshold, indices);

  const std::vector<int> expected = {0, 2, 6};
  EXPECT_EQ(indices, expected);
  EXPECT_NE(occ::checksumIndices(indices), 0u);
}

TEST(OcclusionReasoningDiagnostic, RvvBuildHitsCandidatePathAndMatchesScalarReference)
{
  const int width = 7;
  const int height = 7;
  const float focal = 2.0f;
  const float threshold = 0.05f;
  const auto points = makeFilterFixturePoints();
  const auto depth = makeDepthBuffer(width, height);
  std::vector<int> expected;
  std::vector<int> actual;

  occ::filterIndicesScalarReference(
      points.data(), points.size(), depth.data(), width, height, focal, threshold, expected);
  const auto path = occ::filterIndicesCandidate(
      points.data(), points.size(), depth.data(), width, height, focal, threshold, actual);

  EXPECT_EQ(actual, expected);
#if defined(__RVV10__)
  EXPECT_EQ(path, occ::ExecutionPath::RvvProjectionFilter);
#else
  EXPECT_EQ(path, occ::ExecutionPath::ScalarFallback);
#endif
}

TEST(OcclusionReasoningDiagnostic, ProductionDepthMapRectangularResolutionRegression)
{
  auto scene = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  scene->push_back(pcl::PointXYZ{1.0f, 0.0f, 1.0f});
  scene->width = 1;
  scene->height = 1;
  scene->is_dense = true;

  auto model = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  model->push_back(pcl::PointXYZ{1.0f, 0.0f, 1.0f});
  model->width = 1;
  model->height = 1;
  model->is_dense = true;

  pcl::occlusion_reasoning::ZBuffering<pcl::PointXYZ, pcl::PointXYZ> zbuffer(7, 5, 1.0f);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr scene_const(scene);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr model_const(model);
  zbuffer.computeDepthMap(scene_const, false, false);
  pcl::Indices indices;
  zbuffer.filter(model_const, indices, 0.01f);

  // 非正方形分辨率下，这个期望保护“同一个 scene point 建出的 depth cell 能被
  // filter 读回”。若失败，说明当前 production depth map 构建和 filter 读取索引不一致。
  EXPECT_EQ(indices, pcl::Indices({0}));
}

#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
namespace pcl::detail
{
extern "C" void pcl_rvv_occlusion_reasoning_reset_test_hook();
extern "C" int pcl_rvv_occlusion_reasoning_last_test_hook();
} // namespace pcl::detail
#endif

TEST(OcclusionReasoningDiagnostic, ProductionZBufferingFilterHitsRvvPathAndKeepsExpectedIndices)
{
  const int width = 7;
  const int height = 7;
  const float focal = 2.0f;

  auto scene = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  scene->is_dense = true;
  scene->push_back(makePointForProjectionPixel(3, 3, 2.0f, width, height, focal));
  scene->push_back(makePointForProjectionPixel(4, 3, 1.0f, width, height, focal));
  scene->width = static_cast<std::uint32_t>(scene->size());
  scene->height = 1;

  auto model = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  model->is_dense = true;
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{1.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 2.02f});
  model->push_back(pcl::PointXYZ{-1.0f, -1.0f, 2.0f});
  model->push_back(pcl::PointXYZ{9.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 5.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 1.5f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 0.0f});
  model->width = static_cast<std::uint32_t>(model->size());
  model->height = 1;

  pcl::occlusion_reasoning::ZBuffering<pcl::PointXYZ, pcl::PointXYZ> zbuffer(width, height, focal);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr scene_const(scene);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr model_const(model);
  zbuffer.computeDepthMap(scene_const, false, false);

#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  pcl::detail::pcl_rvv_occlusion_reasoning_reset_test_hook();
#endif
  pcl::Indices indices;
  zbuffer.filter(model_const, indices, 0.05f);

  EXPECT_EQ(indices, pcl::Indices({0, 2, 6}));
#if defined(__RVV10__) && defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  EXPECT_EQ(pcl::detail::pcl_rvv_occlusion_reasoning_last_test_hook(), 2);
#elif defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  EXPECT_EQ(pcl::detail::pcl_rvv_occlusion_reasoning_last_test_hook(), 1);
#endif
}

TEST(OcclusionReasoningDiagnostic, ProductionInlineFilterHitsRvvPathAndKeepsExpectedPoints)
{
  const int width = 7;
  const int height = 7;
  const float focal = 2.0f;

  auto scene = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  scene->resize(static_cast<std::size_t>(width * height));
  scene->width = static_cast<std::uint32_t>(width);
  scene->height = static_cast<std::uint32_t>(height);
  scene->is_dense = true;
  for (auto& point : *scene)
    point = pcl::PointXYZ{0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()};

  scene->at(3, 3) = pcl::PointXYZ{0.0f, 0.0f, 2.0f};
  scene->at(4, 3) = pcl::PointXYZ{0.0f, 0.0f, 1.0f};
  scene->at(2, 2) = pcl::PointXYZ{std::numeric_limits<float>::quiet_NaN(), 0.0f, 2.0f};

  auto model = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  model->is_dense = true;
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{1.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 2.02f});
  model->push_back(pcl::PointXYZ{-1.0f, -1.0f, 2.0f});
  model->push_back(pcl::PointXYZ{9.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 5.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 1.5f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 0.0f});
  model->width = static_cast<std::uint32_t>(model->size());
  model->height = 1;

  pcl::PointCloud<pcl::PointXYZ>::ConstPtr scene_const(scene);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr model_const(model);
#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  pcl::detail::pcl_rvv_occlusion_reasoning_reset_test_hook();
#endif

  const auto filtered =
      pcl::occlusion_reasoning::filter<pcl::PointXYZ, pcl::PointXYZ>(
          scene_const, model_const, focal, 0.05f);

  ASSERT_EQ(filtered->size(), 3u);
  EXPECT_FLOAT_EQ((*filtered)[0].z, 2.0f);
  EXPECT_FLOAT_EQ((*filtered)[1].z, 2.02f);
  EXPECT_FLOAT_EQ((*filtered)[2].z, 1.5f);
#if defined(__RVV10__) && defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  EXPECT_EQ(pcl::detail::pcl_rvv_occlusion_reasoning_last_test_hook(), 2);
#elif defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  EXPECT_EQ(pcl::detail::pcl_rvv_occlusion_reasoning_last_test_hook(), 1);
#endif
}

TEST(OcclusionReasoningDiagnostic, ProductionInlineGetOccludedCloudHitsRvvPathAndKeepsExpectedPoints)
{
  const int width = 7;
  const int height = 7;
  const float focal = 2.0f;

  auto scene = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  scene->resize(static_cast<std::size_t>(width * height));
  scene->width = static_cast<std::uint32_t>(width);
  scene->height = static_cast<std::uint32_t>(height);
  scene->is_dense = true;
  for (auto& point : *scene)
    point = pcl::PointXYZ{0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()};

  scene->at(3, 3) = pcl::PointXYZ{0.0f, 0.0f, 2.0f};
  scene->at(4, 3) = pcl::PointXYZ{0.0f, 0.0f, 1.0f};
  scene->at(2, 2) = pcl::PointXYZ{std::numeric_limits<float>::quiet_NaN(), 0.0f, 2.0f};

  auto model = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  model->is_dense = true;
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{1.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 2.02f});
  model->push_back(pcl::PointXYZ{-1.0f, -1.0f, 2.0f});
  model->push_back(pcl::PointXYZ{9.0f, 0.0f, 2.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 5.0f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 1.5f});
  model->push_back(pcl::PointXYZ{0.0f, 0.0f, 0.0f});
  model->width = static_cast<std::uint32_t>(model->size());
  model->height = 1;

#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  pcl::detail::pcl_rvv_occlusion_reasoning_reset_test_hook();
#endif

  const auto filtered =
      pcl::occlusion_reasoning::getOccludedCloud<pcl::PointXYZ, pcl::PointXYZ>(
          scene, model, focal, 0.05f);

  ASSERT_EQ(filtered->size(), 2u);
  EXPECT_FLOAT_EQ((*filtered)[0].z, 2.0f);
  EXPECT_FLOAT_EQ((*filtered)[1].z, 5.0f);
#if defined(__RVV10__) && defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  EXPECT_EQ(pcl::detail::pcl_rvv_occlusion_reasoning_last_test_hook(), 2);
#elif defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
  EXPECT_EQ(pcl::detail::pcl_rvv_occlusion_reasoning_last_test_hook(), 1);
#endif
}
