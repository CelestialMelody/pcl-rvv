/*
 * 本文件做什么：
 * 这里是 bfgs topic 的 correctness（正确性）入口。测试把 test-only RVV
 * diagnostic candidate（测试专用 RVV 诊断候选）和标量 reference（参考链路）
 * 放在同一数学链路中对拍，并额外保留一个真实 BFGS public API smoke
 * （公开 API 小型验证）。
 *
 * 证据边界：
 * 这些 TEST 不修改 production，也不证明 GICP 热点已经接入 RVV。它们只回答：
 * 局部 direction update（方向更新）和 move-to+slope（移动与方向导数）候选是否
 * 在 Std / RVV build 中保持同构结果。
 */

#include "bfgs.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {

using pcl::registration::rvv_bfgs_support::DirectionResult;
using pcl::registration::rvv_bfgs_support::MoveSlopeResult;

#ifdef __RVV10__
constexpr bool kExpectRvvForNonEmptyInput = true;
#else
constexpr bool kExpectRvvForNonEmptyInput = false;
#endif

void
expect_near_vector(const std::vector<double>& actual,
                   const std::vector<double>& expected,
                   const double tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "vector index " << i;
}

void
expect_direction_near(const DirectionResult& actual,
                      const DirectionResult& expected,
                      const double tolerance)
{
  expect_near_vector(actual.dx0, expected.dx0, tolerance);
  expect_near_vector(actual.dg0, expected.dg0, tolerance);
  expect_near_vector(actual.p, expected.p, tolerance);
  EXPECT_NEAR(actual.dxg, expected.dxg, tolerance);
  EXPECT_NEAR(actual.dgg, expected.dgg, tolerance);
  EXPECT_NEAR(actual.dxdg, expected.dxdg, tolerance);
  EXPECT_NEAR(actual.dgnorm, expected.dgnorm, tolerance);
  EXPECT_NEAR(actual.A, expected.A, tolerance);
  EXPECT_NEAR(actual.B, expected.B, tolerance);
  EXPECT_NEAR(actual.g0norm, expected.g0norm, tolerance);
  EXPECT_NEAR(actual.pnorm, expected.pnorm, tolerance);
  EXPECT_NEAR(actual.fp0, expected.fp0, tolerance);
}

void
expect_move_slope_near(const MoveSlopeResult& actual,
                       const MoveSlopeResult& expected,
                       const double tolerance)
{
  expect_near_vector(actual.x_alpha, expected.x_alpha, tolerance);
  EXPECT_NEAR(actual.slope, expected.slope, tolerance);
}

} // namespace

// 这个测试锁住 GICP caller 当前使用的 6 维状态形状。失败说明 test-only
// candidate 已经不能复刻 bfgs.h 中 direction update 的局部数学语义。
TEST(BFGSDiagnostic, DirectionUpdateVector6MatchesScalarReference)
{
  using namespace pcl::registration::rvv_bfgs_support;

  const DirectionInput input = make_direction_input(6);
  const DirectionResult expected = direction_update_std(input);
  const DirectionResult actual = direction_update_candidate(input);

  expect_direction_near(actual, expected, 1e-10);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// 这个测试用更长的连续 double 向量覆盖 VL chunk（可变向量长度分块）循环。
// 它不代表 production 输入规模，只证明候选不会只在 6 维时偶然通过。
TEST(BFGSDiagnostic, DirectionUpdateLongVectorMatchesScalarReference)
{
  using namespace pcl::registration::rvv_bfgs_support;

  const DirectionInput input = make_direction_input(128);
  const DirectionResult expected = direction_update_std(input);
  const DirectionResult actual = direction_update_candidate(input);

  expect_direction_near(actual, expected, 1e-10);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// dxdg 为 0 时，BFGS 更新会跳过 A/B 系数公式。这个边界保护除零路径，
// 避免 RVV candidate 在特殊输入上改变标量 fallback 语义。
TEST(BFGSDiagnostic, DirectionUpdateZeroDxdgBoundaryMatchesScalarReference)
{
  using namespace pcl::registration::rvv_bfgs_support;

  const DirectionInput input = make_zero_dxdg_input();
  const DirectionResult expected = direction_update_std(input);
  const DirectionResult actual = direction_update_candidate(input);

  EXPECT_EQ(expected.dxdg, 0.0);
  EXPECT_EQ(actual.dxdg, 0.0);
  expect_direction_near(actual, expected, 1e-10);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// 空输入是 defensive boundary（防御性边界）：它不是 production BFGS 的典型状态，
// 但能证明 helper 在无 lane 可处理时不会错误声称命中 RVV。
TEST(BFGSDiagnostic, DirectionUpdateEmptyInputKeepsScalarFallbackShape)
{
  using namespace pcl::registration::rvv_bfgs_support;

  const DirectionInput input = make_direction_input(0);
  const DirectionResult expected = direction_update_std(input);
  const DirectionResult actual = direction_update_candidate(input);

  expect_direction_near(actual, expected, 0.0);
  EXPECT_FALSE(actual.used_rvv);
}

// moveTo(alpha) 后的 slope() 是 line search（线搜索）中反复出现的局部链路。
// 这里对拍的是 x_alpha 和 gradient.dot(p)，不声称覆盖完整 lineSearch 状态机。
TEST(BFGSDiagnostic, MoveToAndSlopeMatchesScalarReference)
{
  using namespace pcl::registration::rvv_bfgs_support;

  const DirectionInput input = make_direction_input(64);
  const double alpha = 0.375;
  const MoveSlopeResult expected = move_to_and_slope_std(input, alpha);
  const MoveSlopeResult actual = move_to_and_slope_candidate(input, alpha);

  expect_move_slope_near(actual, expected, 1e-10);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// 这个 smoke 真实实例化 BFGS<Functor>，保护 public API 与测试依赖的链接边界。
// functor 是简单二次函数，不模拟 GICP cost，因此这里只验证 solver 可运行和状态有限。
TEST(BFGSPublicApiSmoke, QuadraticFunctorMinimizeOneStepRuns)
{
  using pcl::registration::rvv_bfgs_support::make_vector6_center;
  using pcl::registration::rvv_bfgs_support::make_vector6_initial;
  using pcl::registration::rvv_bfgs_support::QuadraticFunctor;

  QuadraticFunctor functor(make_vector6_center());
  BFGS<QuadraticFunctor> optimizer(functor);
  auto x = make_vector6_initial();
  const auto before = x;

  EXPECT_EQ(optimizer.minimizeInit(x), BFGSSpace::NotStarted);
  EXPECT_EQ(optimizer.testGradient(), BFGSSpace::Running);

  const double initial_f = optimizer.f;
  const BFGSSpace::Status status = optimizer.minimizeOneStep(x);

  EXPECT_TRUE(status == BFGSSpace::Success || status == BFGSSpace::NoProgress)
      << "unexpected BFGS status " << static_cast<int>(status);
  EXPECT_TRUE(std::isfinite(optimizer.f));
  EXPECT_TRUE(std::isfinite(x.norm()));
  EXPECT_GT(functor.fdf_calls, 0);
  EXPECT_LE(optimizer.f, initial_f + 1e-10);
  EXPECT_LT((x - functor.center).norm(), (before - functor.center).norm() + 1e-10);
}
