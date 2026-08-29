/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证公共
 * `QuantizedMap::spreadQuantizedMap()` helper 在 Std/RVV 两个构建里保持
 * 同一输出语义。RVV build 必须命中 RVV path（RVV 执行链路），否则测试失败；
 * 这样可以防止把“两个构建都走标量 fallback（回退路径）”误写成 RVV 证据。
 */

#include "qm.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace qm = pcl::recognition::rvv_test::quantizable_modality;

namespace {

extern "C" __attribute__((weak)) void
pcl_rvv_qm_reset_test_hook ()
{}

extern "C" __attribute__((weak)) int
pcl_rvv_qm_last_test_hook ()
{
  return static_cast<int> (qm::SpreadPath::None);
}

extern "C" __attribute__((weak)) void
pcl_rvv_qm_set_force_scalar_test_hook (const int)
{}

void
resetSpreadHook ()
{
  pcl_rvv_qm_reset_test_hook ();
}

void
setForceScalarSpreadHook (const bool enabled)
{
  pcl_rvv_qm_set_force_scalar_test_hook (enabled ? 1 : 0);
}

int
lastSpreadHook ()
{
  return pcl_rvv_qm_last_test_hook ();
}

void
expectSameMap (const pcl::QuantizedMap& reference, const pcl::QuantizedMap& candidate)
{
  ASSERT_EQ (reference.getWidth (), candidate.getWidth ());
  ASSERT_EQ (reference.getHeight (), candidate.getHeight ());
  for (std::size_t y = 0; y < reference.getHeight (); ++y)
  {
    for (std::size_t x = 0; x < reference.getWidth (); ++x)
    {
      EXPECT_EQ (reference (x, y), candidate (x, y)) << "x=" << x << " y=" << y;
    }
  }
}

void
runForcedScalarAndCandidateCompare (const std::size_t width,
                                    const std::size_t height,
                                    const std::size_t spread)
{
  const auto input = qm::makePatternMap (width, height);
  pcl::QuantizedMap scalar_output;
  pcl::QuantizedMap candidate_output;

  resetSpreadHook ();
  setForceScalarSpreadHook (true);
  pcl::QuantizedMap::spreadQuantizedMap (input, scalar_output, spread);
  const int scalar_hook = lastSpreadHook ();

  resetSpreadHook ();
  setForceScalarSpreadHook (false);
  pcl::QuantizedMap::spreadQuantizedMap (input, candidate_output, spread);
  const int candidate_hook = lastSpreadHook ();

  expectSameMap (scalar_output, candidate_output);

#if defined(__RVV10__)
  EXPECT_EQ (scalar_hook, static_cast<int> (qm::SpreadPath::Scalar));
  EXPECT_EQ (candidate_hook, static_cast<int> (qm::SpreadPath::Rvv));
#else
  EXPECT_EQ (scalar_hook, static_cast<int> (qm::SpreadPath::None));
  EXPECT_EQ (candidate_hook, static_cast<int> (qm::SpreadPath::None));
#endif
}

} // namespace

TEST (QuantizableModalitySpread, RvvBuildHitsSharedSpreadPathAndMatchesScalar)
{
  runForcedScalarAndCandidateCompare (65, 37, 8);
}

TEST (QuantizableModalitySpread, RvvTailWidthMatchesScalar)
{
  runForcedScalarAndCandidateCompare (641, 113, 8);
}

TEST (QuantizableModalitySpread, UnsupportedSpreadFallsBackToScalar)
{
  const auto input = qm::makePatternMap (47, 31);
  pcl::QuantizedMap output;

  resetSpreadHook ();
  setForceScalarSpreadHook (false);
  pcl::QuantizedMap::spreadQuantizedMap (input, output, 5);

#if defined(__RVV10__)
  EXPECT_EQ (lastSpreadHook (), static_cast<int> (qm::SpreadPath::Scalar));
#else
  EXPECT_EQ (lastSpreadHook (), static_cast<int> (qm::SpreadPath::None));
#endif
  EXPECT_EQ (output.getWidth (), 47u);
  EXPECT_EQ (output.getHeight (), 31u);
}
