/*
 * 本文件做什么：
 * 这是 ndt topic 的 correctness（正确性）入口。测试把 test-only RVV
 * diagnostic candidate（测试专用 RVV 诊断候选）和标量 reference（参考链路）
 * 放在同一 derivative accumulation（导数累加）样本集上对拍。
 *
 * 证据边界：
 * 这些 TEST 不修改 production，也不证明 NDT public entry（公开入口）已经
 * 接入 RVV。它们只回答 updateDerivatives 同构数学核在 SoA staged input
 *（数组结构暂存输入）上是否保持数值一致。
 */

#include "ndt.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>

namespace {

using pcl::registration::rvv_ndt_support::DerivativeResult;

#ifdef __RVV10__
constexpr bool kExpectRvvForNonEmptyInput = true;
#else
constexpr bool kExpectRvvForNonEmptyInput = false;
#endif

void
expect_result_near(const DerivativeResult& actual,
                   const DerivativeResult& expected,
                   const double gradient_tolerance,
                   const double hessian_tolerance)
{
  EXPECT_NEAR(actual.score, expected.score, gradient_tolerance);
  for (std::size_t i = 0; i < actual.gradient.size(); ++i)
    EXPECT_NEAR(actual.gradient[i], expected.gradient[i], gradient_tolerance)
        << "gradient index " << i;
  for (std::size_t i = 0; i < actual.hessian.size(); ++i)
    EXPECT_NEAR(actual.hessian[i], expected.hessian[i], hessian_tolerance)
        << "hessian index " << i;
}

} // namespace

// 大样本覆盖 NDT computeDerivatives 中最像热点的 point-neighbor sample 累加。
// 失败通常说明 RVV 分块规约或 SoA 字段映射和标量 reference 不一致。
TEST(NDTDerivativeDiagnostic, DenseHessianMatchesScalarReference)
{
  using namespace pcl::registration::rvv_ndt_support;

  const DerivativeBatch batch = make_derivative_batch(8192, true);
  const DerivativeResult expected = derivative_accumulate_std(batch);
  const DerivativeResult actual = derivative_accumulate_candidate(batch);

  expect_result_near(actual, expected, 1e-8, 1e-7);
  EXPECT_EQ(actual.samples, batch.size());
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// 非 2 的幂规模覆盖 RVV tail（尾段）。这条路径对 production 候选很重要，
// 因为真实邻域命中数量不会自然对齐到固定向量长度。
TEST(NDTDerivativeDiagnostic, TailHessianMatchesScalarReference)
{
  using namespace pcl::registration::rvv_ndt_support;

  const DerivativeBatch batch = make_derivative_batch(1031, true);
  const DerivativeResult expected = derivative_accumulate_std(batch);
  const DerivativeResult actual = derivative_accumulate_candidate(batch);

  expect_result_near(actual, expected, 1e-9, 1e-8);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// line search（线搜索）里有 compute_hessian=false 的路径；该测试确认候选
// 不会在只需要 gradient 时错误写入 hessian。
TEST(NDTDerivativeDiagnostic, GradientOnlyPathKeepsHessianZero)
{
  using namespace pcl::registration::rvv_ndt_support;

  const DerivativeBatch batch = make_derivative_batch(2049, false);
  const DerivativeResult expected = derivative_accumulate_std(batch);
  const DerivativeResult actual = derivative_accumulate_candidate(batch);

  expect_result_near(actual, expected, 1e-9, 0.0);
  for (const double value : actual.hessian)
    EXPECT_EQ(value, 0.0);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// 空输入是 defensive boundary（防御性边界）。真实 NDT 在没有 source 点时
// 不应声称 RVV 命中；这里保证 diagnostic helper 的 fallback 形状可审查。
TEST(NDTDerivativeDiagnostic, EmptyInputKeepsFallbackShape)
{
  using namespace pcl::registration::rvv_ndt_support;

  const DerivativeBatch batch = make_derivative_batch(0, true);
  const DerivativeResult expected = derivative_accumulate_std(batch);
  const DerivativeResult actual = derivative_accumulate_candidate(batch);

  expect_result_near(actual, expected, 0.0, 0.0);
  EXPECT_FALSE(actual.used_rvv);
}
