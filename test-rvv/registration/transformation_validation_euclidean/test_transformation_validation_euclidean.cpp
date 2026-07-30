#include "transformation_validation_euclidean_diag.hpp"

// 本文件做什么：
// 这些专项测试把 test-only RVV staging 与标量参考链路对拍，并额外覆盖
// prebuilt KdTree（预建搜索树）和 force_no_recompute（强制不重建 tree）的
// 生产形态语义。它们证明 correctness（正确性）和入口形态，不证明真实性能。

#include <pcl/registration/transformation_validation_euclidean.h>
#include <pcl/test/gtest.h>

#include <cmath>
#include <limits>

namespace diag = pcl::registration::transformation_validation_euclidean_diag;

namespace {

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeCloud(std::size_t n)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = true;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 1021) - 510) * 0.01f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 7) % 1031) - 515) * 0.008f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 13) % 1033) - 516) * 0.006f;
  }
  return cloud;
}

diag::Matrix4f
makeTransform()
{
  diag::Matrix4f t = diag::Matrix4f::Identity();
  t(0, 0) = 0.9659258f;
  t(0, 1) = -0.2588190f;
  t(1, 0) = 0.2588190f;
  t(1, 1) = 0.9659258f;
  t(0, 3) = 0.12f;
  t(1, 3) = -0.04f;
  t(2, 3) = 0.08f;
  return t;
}

} // namespace

TEST(TransformationValidationEuclideanDiag, TransformCandidateMatchesScalar)
{
  const auto cloud = makeCloud(4096);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  pcl::PointCloud<pcl::PointXYZ> candidate;
  const auto transform = makeTransform();

  diag::transformPointXYZStd(*cloud, scalar, transform);
  diag::transformPointXYZCandidate(*cloud, candidate, transform);

  ASSERT_EQ(scalar.size(), candidate.size());
  for (std::size_t i = 0; i < scalar.size(); ++i) {
    EXPECT_NEAR(scalar[i].x, candidate[i].x, 1e-6f);
    EXPECT_NEAR(scalar[i].y, candidate[i].y, 1e-6f);
    EXPECT_NEAR(scalar[i].z, candidate[i].z, 1e-6f);
  }
}

// 小规模输入必须走 fallback（回退路径）。失败说明规模 gate 可能误把小输入送进 RVV。
TEST(TransformationValidationEuclideanDiag, SmallInputFallbackMatchesScalar)
{
  const auto cloud = makeCloud(17);
  pcl::PointCloud<pcl::PointXYZ> scalar;
  pcl::PointCloud<pcl::PointXYZ> candidate;
  const auto transform = makeTransform();

  diag::transformPointXYZStd(*cloud, scalar, transform);
  diag::transformPointXYZCandidate(*cloud, candidate, transform);

  ASSERT_EQ(scalar.size(), candidate.size());
  for (std::size_t i = 0; i < scalar.size(); ++i) {
    EXPECT_FLOAT_EQ(scalar[i].x, candidate[i].x);
    EXPECT_FLOAT_EQ(scalar[i].y, candidate[i].y);
    EXPECT_FLOAT_EQ(scalar[i].z, candidate[i].z);
  }
}

// fresh-tree full validation 是最接近 production 默认入口的 test-only 诊断：
// RVV 候选只替换 transform staging，KdTree setup/search 和 score tail 保持标量。
TEST(TransformationValidationEuclideanDiag, FullValidationScoreMatchesScalar)
{
  const auto source = makeCloud(2048);
  const auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const auto transform = makeTransform();
  diag::transformPointXYZStd(*source, *target, transform);

  const double scalar = diag::validateTransformationStd(source, target, transform, 1.0);
  const double candidate = diag::validateTransformationCandidate(source, target, transform, 1.0);
  EXPECT_NEAR(scalar, candidate, 1e-12);
}

// prebuilt-tree 形态模拟用户已经设置 target tree、计分时不重建 tree 的场景。
// 这条测试保证 tree reuse 不改变 RVV staging 与标量参考的分数语义。
TEST(TransformationValidationEuclideanDiag, PrebuiltTreeScoreMatchesFreshTree)
{
  const auto source = makeCloud(2048);
  const auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const auto transform = makeTransform();
  diag::transformPointXYZStd(*source, *target, transform);

  diag::KdTree tree;
  diag::setupTargetTree(tree, target);
  const auto scalar = diag::validateTransformationStdWithTree(source, tree, transform, 1.0);

  diag::KdTree candidate_tree;
  diag::setupTargetTree(candidate_tree, target);
  const auto candidate =
      diag::validateTransformationCandidateWithTree(source, candidate_tree, transform, 1.0);

  EXPECT_NEAR(scalar.score, candidate.score, 1e-12);
  EXPECT_EQ(scalar.accepted_points, candidate.accepted_points);
}

// search-only 负向对照把 transform 从计时和语义路径中拿掉，只检查同一棵 tree
// 上的 nearestKSearch + max_range + score tail 是否和 full diagnostic 的尾段一致。
TEST(TransformationValidationEuclideanDiag, SearchOnlyScoreMatchesFullValidationTail)
{
  const auto source = makeCloud(1024);
  const auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const auto transform = makeTransform();
  pcl::PointCloud<pcl::PointXYZ> transformed;
  diag::transformPointXYZStd(*source, transformed, transform);
  diag::transformPointXYZStd(*source, *target, transform);

  diag::KdTree tree;
  diag::setupTargetTree(tree, target);
  const auto search_only = diag::scoreTransformedWithTree(transformed, tree, 1.0);
  const double full_score = diag::validateTransformationStd(source, target, transform, 1.0);

  EXPECT_NEAR(search_only.score, full_score, 1e-12);
  EXPECT_EQ(search_only.accepted_points, static_cast<int>(source->size()));
}

// 真实 public class（公开类）通过 setSearchMethodTarget(..., true) 进入
// force_no_recompute。它仍是标量 production 路径；这里只用来证明本轮
// prebuilt-tree 诊断形态没有偏离公开入口语义。
TEST(TransformationValidationEuclideanDiag, ProductionForceNoRecomputeMatchesFreshTree)
{
  const auto source = makeCloud(1024);
  const auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  const auto transform = makeTransform();
  diag::transformPointXYZStd(*source, *target, transform);

  pcl::registration::TransformationValidationEuclidean<pcl::PointXYZ, pcl::PointXYZ>
      fresh_validator;
  fresh_validator.setMaxRange(1.0);
  const double fresh = fresh_validator.validateTransformation(source, target, transform);

  auto prebuilt_tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZ>>();
  prebuilt_tree->setInputCloud(target);
  pcl::registration::TransformationValidationEuclidean<pcl::PointXYZ, pcl::PointXYZ>
      reused_validator;
  reused_validator.setMaxRange(1.0);
  reused_validator.setSearchMethodTarget(prebuilt_tree, true);
  const double reused = reused_validator.validateTransformation(source, target, transform);

  EXPECT_NEAR(fresh, reused, 1e-12);
}

// max_range 全拒绝时 production 返回 double::max。这个边界保护 score tail 的
// accepted_points 语义，避免无有效匹配被误写成 0 分。
TEST(TransformationValidationEuclideanDiag, MaxRangeRejectsAllMatchesScalar)
{
  const auto source = makeCloud(1024);
  const auto target = makeCloud(1024);
  diag::Matrix4f transform = diag::Matrix4f::Identity();
  transform(0, 3) = 100.0f;

  const double scalar = diag::validateTransformationStd(source, target, transform, 1e-6);
  const double candidate = diag::validateTransformationCandidate(source, target, transform, 1e-6);
  EXPECT_EQ(scalar, std::numeric_limits<double>::max());
  EXPECT_EQ(candidate, scalar);
}
