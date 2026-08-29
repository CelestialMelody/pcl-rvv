/*
 * 本文件做什么：
 * 这些 gtest 验证 Harris 2D production-shaped diagnostic（生产形态诊断）
 * 的 scalar reference（标量参考链路）和 RVV candidate（RVV 候选链路）
 * 是否按同一 organized response map（有组织响应图）语义对拍。
 * RVV build 必须命中 RVV path；否则测试失败，防止把纯标量 fallback
 * 误登记为 RVV 证据。
 */

#include "harris_2d.h"

#include <gtest/gtest.h>

#include <cmath>

namespace h2d = pcl::keypoints::rvv_test::harris_2d;

namespace
{

void
expectImagesNear(const h2d::ResponseImage& actual,
                 const h2d::ResponseImage& expected,
                 const float tolerance = 1e-4f)
{
  ASSERT_EQ(actual.width, expected.width);
  ASSERT_EQ(actual.height, expected.height);
  ASSERT_EQ(actual.intensity.size(), expected.intensity.size());
  for (std::size_t i = 0; i < actual.intensity.size(); ++i)
    EXPECT_NEAR(actual.intensity[i], expected.intensity[i], tolerance) << "index=" << i;
}

} // namespace

TEST(Harris2DDiagnostic, ScalarReferenceCoversAllResponseMethods)
{
  const auto input = h2d::makeSyntheticImage(17, 13);
  for (const auto method :
       {h2d::ResponseMethod::Harris, h2d::ResponseMethod::Noble, h2d::ResponseMethod::Lowe, h2d::ResponseMethod::Tomasi})
  {
    h2d::ResponseImage output;
    h2d::computeResponsesScalar(input, method, 3, 3, output);
    EXPECT_EQ(output.width, input.width);
    EXPECT_EQ(output.height, input.height);
    EXPECT_NE(h2d::checksumResponses(output), 0u);
  }
}

TEST(Harris2DDiagnostic, NonFiniteInputPointKeepsZeroResponse)
{
  auto input = h2d::makeSyntheticImage(11, 9);
  h2d::injectNonFinitePoint(input, 17);

  h2d::ResponseImage output;
  h2d::computeResponsesScalar(input, h2d::ResponseMethod::Tomasi, 3, 3, output);

  EXPECT_EQ(output.intensity[17], 0.0f);
}

TEST(Harris2DDiagnostic, ResponseChecksumUsesToleranceBucket)
{
  h2d::ResponseImage baseline;
  baseline.width = 3;
  baseline.height = 1;
  baseline.intensity = {1.0f, -2.0f, 3.0f};

  auto tiny_drift = baseline;
  tiny_drift.intensity[1] += 2e-5f;

  auto visible_drift = baseline;
  visible_drift.intensity[1] += 2e-3f;

  EXPECT_EQ(h2d::checksumResponses(baseline), h2d::checksumResponses(tiny_drift));
  EXPECT_NE(h2d::checksumResponses(baseline), h2d::checksumResponses(visible_drift));
}

TEST(Harris2DDiagnostic, CandidateMatchesScalarReference)
{
  auto input = h2d::makeSyntheticImage(41, 29);
  h2d::injectNonFinitePoint(input, 97);

  for (const auto method :
       {h2d::ResponseMethod::Harris, h2d::ResponseMethod::Noble, h2d::ResponseMethod::Lowe, h2d::ResponseMethod::Tomasi})
  {
    h2d::ResponseImage expected;
    h2d::ResponseImage actual;
    h2d::computeResponsesScalar(input, method, 3, 3, expected);
    std::size_t vector_chunks = 0;
    const auto path =
        h2d::computeResponsesCandidate(input, method, 3, 3, actual, &vector_chunks);

    expectImagesNear(actual, expected);
#if defined(__RVV10__)
    EXPECT_EQ(path, h2d::ExecutionPath::RvvDerivativeResponse);
    EXPECT_GT(vector_chunks, 0u);
#else
    EXPECT_EQ(path, h2d::ExecutionPath::ScalarFallback);
    EXPECT_EQ(vector_chunks, 0u);
#endif
  }
}

TEST(Harris2DDiagnostic, CandidateHandlesTailWidth)
{
  const auto input = h2d::makeSyntheticImage(65, 31);
  h2d::ResponseImage expected;
  h2d::ResponseImage actual;
  h2d::computeResponsesScalar(input, h2d::ResponseMethod::Noble, 5, 5, expected);
  std::size_t vector_chunks = 0;
  const auto path =
      h2d::computeResponsesCandidate(input, h2d::ResponseMethod::Noble, 5, 5, actual, &vector_chunks);

  expectImagesNear(actual, expected);
#if defined(__RVV10__)
  EXPECT_EQ(path, h2d::ExecutionPath::RvvDerivativeResponse);
  EXPECT_GT(vector_chunks, 0u);
#else
  EXPECT_EQ(path, h2d::ExecutionPath::ScalarFallback);
#endif
}

TEST(Harris2DProductionDirect, PublicComputeMatchesScalarReference)
{
  const auto input = h2d::makeSyntheticImage(39, 27);
  for (const auto method :
       {h2d::ResponseMethod::Harris, h2d::ResponseMethod::Noble, h2d::ResponseMethod::Lowe, h2d::ResponseMethod::Tomasi})
  {
    h2d::ResponseImage expected;
    h2d::ResponseImage actual;
    h2d::computeResponsesScalar(input, method, 3, 3, expected);
    const auto path = h2d::computeResponsesPublic(input, method, 3, 3, actual);

    expectImagesNear(actual, expected, 5e-4f);
#if defined(__RVV10__)
    EXPECT_EQ(path, h2d::ExecutionPath::RvvDerivativeResponse);
#else
    EXPECT_EQ(path, h2d::ExecutionPath::ScalarFallback);
#endif
  }
}

TEST(Harris2DProductionDirect, PublicComputeDoesNotReusePreviousImageSize)
{
  h2d::ResponseImage warmup_actual;
  (void)h2d::computeResponsesPublic(
      h2d::makeSyntheticImage(23, 19), h2d::ResponseMethod::Noble, 3, 3, warmup_actual);

  const auto input = h2d::makeSyntheticImage(65, 31);
  h2d::ResponseImage expected;
  h2d::ResponseImage actual;
  h2d::computeResponsesScalar(input, h2d::ResponseMethod::Noble, 5, 5, expected);
  (void)h2d::computeResponsesPublic(input, h2d::ResponseMethod::Noble, 5, 5, actual);

  expectImagesNear(actual, expected, 5e-4f);
}
