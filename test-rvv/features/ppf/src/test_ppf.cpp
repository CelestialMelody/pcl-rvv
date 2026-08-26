/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）先为 PPF（Point Pair Feature，点对特征）
 * 建立 test-only scalar reference（测试专用标量参考链路）。后续 RVV candidate
 * （候选实现）必须先和这个 reference 做 same-chain（同构链路）对拍，再进入
 * component ablation（组件消融）或 production integration（生产接入）判断。
 *
 * 证据边界：
 * 本文件不修改 production 头文件。Std/RVV 两个构建都运行同一套测试，用来验证
 * PPF all-pairs output（所有点对输出）、identity pair（同一点对）NaN 和
 * helper choice（辅助函数选择）语义。
 */

#define PCL_RVV_PPF_ENABLE_TEST_TRACE

#include "ppf.h"

#include <pcl/features/ppf.h>
#include <pcl/point_cloud.h>
#include <pcl/test/gtest.h>

#include <cmath>
#include <cstddef>

namespace ppf_test = pcl::features::rvv_test::ppf;

#if defined(__RVV10__) && defined(PCL_RVV_PPF_ENABLE_TEST_TRACE)
extern "C" {
std::size_t pcl_rvv_ppf_alpha_m_trace_hits = 0;
}
#endif

namespace
{
void
expectSignatureNear(const pcl::PPFSignature& actual,
                    const pcl::PPFSignature& expected,
                    const float tolerance)
{
  EXPECT_NEAR(actual.f1, expected.f1, tolerance);
  EXPECT_NEAR(actual.f2, expected.f2, tolerance);
  EXPECT_NEAR(actual.f3, expected.f3, tolerance);
  EXPECT_NEAR(actual.f4, expected.f4, tolerance);
  EXPECT_NEAR(actual.alpha_m, expected.alpha_m, tolerance);
}
} // namespace

TEST(PPFReference, ComputesProductionLikeAllPairsOutput)
{
  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(6);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(7);

  pcl::PPFEstimation<pcl::PointXYZ, pcl::Normal, pcl::PPFSignature> estimator;
  estimator.setInputCloud(cloud);
  estimator.setInputNormals(normals);
  estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

  pcl::PointCloud<pcl::PPFSignature> production;
  estimator.compute(production);

  pcl::PointCloud<pcl::PPFSignature> reference;
  ppf_test::computePPFReference(*cloud, *normals, indices, reference);

  ASSERT_EQ(production.size(), reference.size());
  ASSERT_EQ(production.width, reference.width);
  ASSERT_EQ(production.height, reference.height);
  ASSERT_EQ(production.is_dense, reference.is_dense);

  for (std::size_t i = 0; i < production.size(); ++i)
  {
    if (std::isnan(reference[i].f1))
    {
      EXPECT_TRUE(std::isnan(production[i].f1)) << "row=" << i;
      EXPECT_TRUE(std::isnan(production[i].alpha_m)) << "row=" << i;
      continue;
    }
    expectSignatureNear(production[i], reference[i], 1e-5f);
  }
}

TEST(PPFReference, MarksIdentityPairsAsNaNAndNotDense)
{
  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(5);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(cloud->size());

  pcl::PointCloud<pcl::PPFSignature> reference;
  ppf_test::computePPFReference(*cloud, *normals, indices, reference);

  ASSERT_EQ(reference.size(), indices.size() * cloud->size());
  EXPECT_FALSE(reference.is_dense);
  for (std::size_t index_i = 0; index_i < indices.size(); ++index_i)
  {
    const auto row = index_i * cloud->size() + static_cast<std::size_t>(indices[index_i]);
    EXPECT_TRUE(std::isnan(reference[row].f1)) << "row=" << row;
    EXPECT_TRUE(std::isnan(reference[row].f2)) << "row=" << row;
    EXPECT_TRUE(std::isnan(reference[row].f3)) << "row=" << row;
    EXPECT_TRUE(std::isnan(reference[row].f4)) << "row=" << row;
    EXPECT_TRUE(std::isnan(reference[row].alpha_m)) << "row=" << row;
  }
}

TEST(PPFCandidate, PairFeatureBatchRVVComputesProductionLikeOutput)
{
  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(7);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(11);

  pcl::PointCloud<pcl::PPFSignature> expected;
  pcl::PointCloud<pcl::PPFSignature> candidate;
  ppf_test::computePPFReference(*cloud, *normals, indices, expected);
  ppf_test::computePPFPairFeatureBatchRVV(*cloud, *normals, indices, candidate);

  ASSERT_EQ(candidate.size(), expected.size());
  ASSERT_EQ(candidate.width, expected.width);
  ASSERT_EQ(candidate.height, expected.height);
  ASSERT_EQ(candidate.is_dense, expected.is_dense);

  for (std::size_t i = 0; i < candidate.size(); ++i)
  {
    if (std::isnan(expected[i].f1))
    {
      EXPECT_TRUE(std::isnan(candidate[i].f1)) << "row=" << i;
      EXPECT_TRUE(std::isnan(candidate[i].alpha_m)) << "row=" << i;
      continue;
    }
    expectSignatureNear(candidate[i], expected[i], 2e-3f);
  }
}

TEST(PPFAlphaM, ClosedFormMatchesEigenReference)
{
  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(7);

  for (std::size_t i = 0; i < cloud->size(); i += 5)
  {
    for (std::size_t j = 0; j < cloud->size(); j += 7)
    {
      if (i == j)
        continue;
      const float expected = ppf_test::computeAlphaMReference((*cloud)[i], (*normals)[i], (*cloud)[j]);
      const float actual = ppf_test::computeAlphaMClosedForm((*cloud)[i], (*normals)[i], (*cloud)[j]);
      EXPECT_NEAR(actual, expected, 1e-5f) << "i=" << i << " j=" << j;
    }
  }

  pcl::PointXYZ reference_point;
  reference_point.x = 1.0f;
  reference_point.y = -2.0f;
  reference_point.z = 0.5f;
  pcl::PointXYZ model_point;
  model_point.x = 1.75f;
  model_point.y = 0.25f;
  model_point.z = -0.125f;
  pcl::Normal parallel_normal;
  parallel_normal.normal_x = 1.0f;
  parallel_normal.normal_y = 0.0f;
  parallel_normal.normal_z = 0.0f;

  const float expected = ppf_test::computeAlphaMReference(reference_point, parallel_normal, model_point);
  const float actual = ppf_test::computeAlphaMClosedForm(reference_point, parallel_normal, model_point);
  EXPECT_TRUE(std::isfinite(actual));
  EXPECT_NEAR(actual, expected, 1e-5f);
}

TEST(PPFCandidate, AlphaMBatchRVVComputesProductionLikeOutput)
{
  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(7);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(11);

  pcl::PointCloud<pcl::PPFSignature> expected;
  pcl::PointCloud<pcl::PPFSignature> candidate;
  ppf_test::computePPFReference(*cloud, *normals, indices, expected);
  ppf_test::computePPFAlphaMBatchRVV(*cloud, *normals, indices, candidate);

  ASSERT_EQ(candidate.size(), expected.size());
  ASSERT_EQ(candidate.width, expected.width);
  ASSERT_EQ(candidate.height, expected.height);
  ASSERT_EQ(candidate.is_dense, expected.is_dense);

  for (std::size_t i = 0; i < candidate.size(); ++i)
  {
    if (std::isnan(expected[i].f1))
    {
      EXPECT_TRUE(std::isnan(candidate[i].f1)) << "row=" << i;
      EXPECT_TRUE(std::isnan(candidate[i].alpha_m)) << "row=" << i;
      continue;
    }
    expectSignatureNear(candidate[i], expected[i], 2e-3f);
  }
}

#if defined(__RVV10__) && defined(PCL_RVV_PPF_ENABLE_TEST_TRACE)
TEST(PPFProductionDirect, RVVAlphaMPathHitsPublicComputeForExactTypes)
{
  pcl_rvv_ppf_alpha_m_trace_hits = 0;

  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(7);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(11);

  pcl::PPFEstimation<pcl::PointXYZ, pcl::Normal, pcl::PPFSignature> estimator;
  estimator.setInputCloud(cloud);
  estimator.setInputNormals(normals);
  estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

  pcl::PointCloud<pcl::PPFSignature> production;
  estimator.compute(production);

  pcl::PointCloud<pcl::PPFSignature> reference;
  ppf_test::computePPFReference(*cloud, *normals, indices, reference);

  ASSERT_EQ(production.size(), reference.size());
  ASSERT_EQ(production.width, reference.width);
  ASSERT_EQ(production.height, reference.height);
  ASSERT_EQ(production.is_dense, reference.is_dense);
  EXPECT_GT(pcl_rvv_ppf_alpha_m_trace_hits, 0u);

  for (std::size_t i = 0; i < production.size(); ++i)
  {
    if (std::isnan(reference[i].f1))
    {
      EXPECT_TRUE(std::isnan(production[i].f1)) << "row=" << i;
      EXPECT_TRUE(std::isnan(production[i].alpha_m)) << "row=" << i;
      continue;
    }
    expectSignatureNear(production[i], reference[i], 2e-3f);
  }
}

TEST(PPFProductionDirect, RVVAlphaMPathHitsPublicComputeForPointXYZILikeSource)
{
  pcl_rvv_ppf_alpha_m_trace_hits = 0;

  const auto [cloud, normals] = ppf_test::makeXYZIAndNormalClouds(7);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(11);

  pcl::PPFEstimation<pcl::PointXYZI, pcl::Normal, pcl::PPFSignature> estimator;
  estimator.setInputCloud(cloud);
  estimator.setInputNormals(normals);
  estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

  pcl::PointCloud<pcl::PPFSignature> production;
  estimator.compute(production);

  pcl::PointCloud<pcl::PPFSignature> reference;
  pcl::detail::computePPFFeatureStd(*cloud, *normals, indices, reference, "PPFEstimation");

  ASSERT_EQ(production.size(), reference.size());
  ASSERT_EQ(production.width, reference.width);
  ASSERT_EQ(production.height, reference.height);
  ASSERT_EQ(production.is_dense, reference.is_dense);
  EXPECT_GT(pcl_rvv_ppf_alpha_m_trace_hits, 0u);

  for (std::size_t i = 0; i < production.size(); ++i)
  {
    if (std::isnan(reference[i].f1))
    {
      EXPECT_TRUE(std::isnan(production[i].f1)) << "row=" << i;
      EXPECT_TRUE(std::isnan(production[i].alpha_m)) << "row=" << i;
      continue;
    }
    expectSignatureNear(production[i], reference[i], 2e-3f);
  }
}

TEST(PPFProductionDirect, RVVAlphaMPathHitsPublicComputeForPointNormalLikeNormals)
{
  pcl_rvv_ppf_alpha_m_trace_hits = 0;

  const auto [cloud, normals] = ppf_test::makeXYZAndPointNormalClouds(7);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(11);

  pcl::PPFEstimation<pcl::PointXYZ, pcl::PointNormal, pcl::PPFSignature> estimator;
  estimator.setInputCloud(cloud);
  estimator.setInputNormals(normals);
  estimator.setIndices(pcl::IndicesPtr(new pcl::Indices(indices)));

  pcl::PointCloud<pcl::PPFSignature> production;
  estimator.compute(production);

  pcl::PointCloud<pcl::PPFSignature> reference;
  pcl::detail::computePPFFeatureStd(*cloud, *normals, indices, reference, "PPFEstimation");

  ASSERT_EQ(production.size(), reference.size());
  ASSERT_EQ(production.width, reference.width);
  ASSERT_EQ(production.height, reference.height);
  ASSERT_EQ(production.is_dense, reference.is_dense);
  EXPECT_GT(pcl_rvv_ppf_alpha_m_trace_hits, 0u);

  for (std::size_t i = 0; i < production.size(); ++i)
  {
    if (std::isnan(reference[i].f1))
    {
      EXPECT_TRUE(std::isnan(production[i].f1)) << "row=" << i;
      EXPECT_TRUE(std::isnan(production[i].alpha_m)) << "row=" << i;
      continue;
    }
    expectSignatureNear(production[i], reference[i], 2e-3f);
  }
}

TEST(PPFProductionDirect, RVVAlphaMRejectsUnsupportedLayouts)
{
  pcl_rvv_ppf_alpha_m_trace_hits = 0;

  const auto [cloud, normals] = ppf_test::makeXYZAndNormalClouds(7);
  const pcl::Indices indices = ppf_test::makeSequentialIndices(11);

  pcl::PointCloud<pcl::PFHSignature125> wrong_output_type;
  EXPECT_FALSE(pcl::detail::computePPFFeatureAlphaMRVV(
      *cloud, *normals, indices, wrong_output_type, "PPFEstimation"));
  EXPECT_EQ(pcl_rvv_ppf_alpha_m_trace_hits, 0u);

  pcl::PointCloud<pcl::PPFSignature> output;
  EXPECT_FALSE(pcl::detail::computePPFFeatureAlphaMRVV(
      *cloud, *cloud, indices, output, "PPFEstimation"));
  EXPECT_EQ(pcl_rvv_ppf_alpha_m_trace_hits, 0u);
}
#endif
