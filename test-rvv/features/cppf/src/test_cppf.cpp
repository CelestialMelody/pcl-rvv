/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）为 CPPF（Colored Point Pair Feature，
 * 彩色点对特征）建立 test-only scalar reference（测试专用标量参考链路），
 * 并验收两个 RVV candidate（候选实现）：pair/HSV 批处理和 `alpha_m` 批处理。
 *
 * 证据边界：
 * Std/RVV 两个构建都运行同一套测试。它们证明 component ablation（组件消融）
 * 的 same-chain correctness（同构链路正确性），不证明 production dispatch（生产分流）
 * 或目标硬件性能。
 */

#include "cppf.h"

#include <pcl/features/cppf.h>
#include <pcl/test/gtest.h>

#include <cmath>

namespace cppf_test = pcl::features::rvv_test::cppf;

namespace
{
void
expectSignatureNear(const pcl::CPPFSignature& actual,
                    const pcl::CPPFSignature& expected,
                    const float tolerance)
{
  EXPECT_NEAR(actual.f1, expected.f1, tolerance);
  EXPECT_NEAR(actual.f2, expected.f2, tolerance);
  EXPECT_NEAR(actual.f3, expected.f3, tolerance);
  EXPECT_NEAR(actual.f4, expected.f4, tolerance);
  EXPECT_NEAR(actual.f5, expected.f5, tolerance);
  EXPECT_NEAR(actual.f6, expected.f6, tolerance);
  EXPECT_NEAR(actual.f7, expected.f7, tolerance);
  EXPECT_NEAR(actual.f8, expected.f8, tolerance);
  EXPECT_NEAR(actual.f9, expected.f9, tolerance);
  EXPECT_NEAR(actual.f10, expected.f10, tolerance);
  EXPECT_NEAR(actual.alpha_m, expected.alpha_m, tolerance);
}

void
expectCloudNear(const pcl::PointCloud<pcl::CPPFSignature>& actual,
                const pcl::PointCloud<pcl::CPPFSignature>& expected,
                const float tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  ASSERT_EQ(actual.width, expected.width);
  ASSERT_EQ(actual.height, expected.height);
  ASSERT_EQ(actual.is_dense, expected.is_dense);

  for (std::size_t i = 0; i < actual.size(); ++i)
  {
    if (std::isnan(expected[i].f1))
    {
      EXPECT_TRUE(std::isnan(actual[i].f1)) << "row=" << i;
      EXPECT_TRUE(std::isnan(actual[i].f10)) << "row=" << i;
      EXPECT_TRUE(std::isnan(actual[i].alpha_m)) << "row=" << i;
      continue;
    }
    expectSignatureNear(actual[i], expected[i], tolerance);
  }
}
} // namespace

TEST(CPPFReference, ComputesProductionLikeAllPairsOutput)
{
  const auto cloud = cppf_test::makeCPPFCloud(7);
  const pcl::Indices indices = cppf_test::makePrefixIndices(cloud->size(), 13);

  pcl::CPPFEstimation<cppf_test::PointT, cppf_test::PointT, pcl::CPPFSignature> estimator;
  estimator.setInputCloud(cloud);
  estimator.setInputNormals(cloud);
  estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

  pcl::PointCloud<pcl::CPPFSignature> production;
  estimator.compute(production);

  pcl::PointCloud<pcl::CPPFSignature> reference;
  cppf_test::computeCPPFReference(*cloud, indices, reference);

  expectCloudNear(production, reference, 1e-5f);
}

TEST(CPPFReference, MarksIdentityPairsAsNaNAndNotDense)
{
  const auto cloud = cppf_test::makeCPPFCloud(6);
  const pcl::Indices indices = cppf_test::makeSequentialIndices(cloud->size());

  pcl::PointCloud<pcl::CPPFSignature> reference;
  cppf_test::computeCPPFReference(*cloud, indices, reference);

  ASSERT_EQ(reference.size(), indices.size() * cloud->size());
  EXPECT_FALSE(reference.is_dense);
  for (std::size_t index_i = 0; index_i < indices.size(); ++index_i)
  {
    const auto row = index_i * cloud->size() + static_cast<std::size_t>(indices[index_i]);
    EXPECT_TRUE(std::isnan(reference[row].f1)) << "row=" << row;
    EXPECT_TRUE(std::isnan(reference[row].f5)) << "row=" << row;
    EXPECT_TRUE(std::isnan(reference[row].f10)) << "row=" << row;
    EXPECT_TRUE(std::isnan(reference[row].alpha_m)) << "row=" << row;
  }
}

TEST(CPPFCandidate, PairHSVBatchRVVComputesProductionLikeOutput)
{
  const auto cloud = cppf_test::makeCPPFCloud(7);
  const pcl::Indices indices = cppf_test::makePrefixIndices(cloud->size(), 13);

  pcl::PointCloud<pcl::CPPFSignature> expected;
  pcl::PointCloud<pcl::CPPFSignature> candidate;
  cppf_test::computeCPPFReference(*cloud, indices, expected);
  cppf_test::computeCPPFPairHSVBatchRVV(*cloud, indices, candidate);

  expectCloudNear(candidate, expected, 3e-3f);
}

TEST(CPPFAlphaM, ClosedFormMatchesEigenReference)
{
  const auto cloud = cppf_test::makeCPPFCloud(8);

  for (std::size_t i = 0; i < cloud->size(); i += 7)
  {
    for (std::size_t j = 0; j < cloud->size(); j += 9)
    {
      if (i == j)
        continue;
      const float expected = cppf_test::computeAlphaMReference((*cloud)[i], (*cloud)[i], (*cloud)[j]);
      const float actual = cppf_test::computeAlphaMClosedForm((*cloud)[i], (*cloud)[i], (*cloud)[j]);
      EXPECT_NEAR(actual, expected, 1e-5f) << "i=" << i << " j=" << j;
    }
  }
}

TEST(CPPFCandidate, AlphaMBatchRVVComputesProductionLikeOutput)
{
  const auto cloud = cppf_test::makeCPPFCloud(7);
  const pcl::Indices indices = cppf_test::makePrefixIndices(cloud->size(), 13);

  pcl::PointCloud<pcl::CPPFSignature> expected;
  pcl::PointCloud<pcl::CPPFSignature> candidate;
  cppf_test::computeCPPFReference(*cloud, indices, expected);
  cppf_test::computeCPPFAlphaMBatchRVV(*cloud, indices, candidate);

  expectCloudNear(candidate, expected, 4e-3f);
}
