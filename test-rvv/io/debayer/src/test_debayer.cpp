/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 debayer topic 的测试专用
 * bilinear interior（双线性内区）reference（参考链路）和 RVV candidate
 *（候选链路）是否复刻 DeBayer::debayerBilinear() 的 GRBG Bayer stencil
 *（邻域模板）语义。测试把内区结果同时和 production DeBayer 输出对拍。
 *
 * 证据边界：
 * 这里还没有修改 production（生产源码）里的 DeBayer 公开入口，也不证明
 * production dispatch（生产分流）已经接入 RVV。RVV build 必须命中
 * candidate path（候选路径）；否则这个测试会失败，避免纯标量 fallback
 * 被误写成 RVV 证据。
 */

#include "debayer.h"

#include <pcl/io/debayer.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace debayer = pcl::io::rvv_debayer_support;

namespace {

std::vector<std::uint8_t>
makeBayerPattern(const unsigned width, const unsigned height, const unsigned line_step)
{
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(line_step) * height, 0);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      pixels[static_cast<std::size_t>(y) * line_step + x] =
          static_cast<std::uint8_t>((17 * x + 31 * y + 7 * ((x ^ y) & 3) + 19) & 0xff);
    }
  }
  return pixels;
}

void
expectInteriorEquals(const std::vector<std::uint8_t>& reference,
                     const std::vector<std::uint8_t>& candidate,
                     const unsigned width,
                     const unsigned height,
                     const unsigned rgb_line_step)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (unsigned y = 2; y < height - 2; ++y) {
    for (unsigned x = 2; x < width - 2; ++x) {
      const auto index = static_cast<std::size_t>(y) * rgb_line_step + x * 3;
      EXPECT_EQ(reference[index + 0], candidate[index + 0]) << "x=" << x << " y=" << y << " channel=R";
      EXPECT_EQ(reference[index + 1], candidate[index + 1]) << "x=" << x << " y=" << y << " channel=G";
      EXPECT_EQ(reference[index + 2], candidate[index + 2]) << "x=" << x << " y=" << y << " channel=B";
    }
  }
}

} // namespace

TEST(DebayerBilinearDiagnostic, InteriorReferenceMatchesProductionWithPadding)
{
  const unsigned width = 34;
  const unsigned height = 18;
  const unsigned bayer_line_step = width;
  const unsigned rgb_line_step = width * 3 + 11;
  const auto bayer = makeBayerPattern(width, height, bayer_line_step);
  std::vector<std::uint8_t> production(static_cast<std::size_t>(rgb_line_step) * height, 0x5a);
  std::vector<std::uint8_t> reference(production.size(), 0x5a);

  pcl::io::DeBayer d;
  d.debayerBilinear(
      bayer.data(), production.data(), width, height, bayer_line_step, bayer_line_step * 2, rgb_line_step);
  debayer::debayerBilinearInteriorScalar(
      bayer.data(), reference.data(), width, height, bayer_line_step, bayer_line_step * 2, rgb_line_step);

  expectInteriorEquals(production, reference, width, height, rgb_line_step);
}

TEST(DebayerBilinearDiagnostic, RvvBuildHitsCandidatePathAndMatchesScalarInterior)
{
  const unsigned width = 66;
  const unsigned height = 34;
  const unsigned bayer_line_step = width;
  const unsigned rgb_line_step = width * 3 + 5;
  const auto bayer = makeBayerPattern(width, height, bayer_line_step);
  std::vector<std::uint8_t> reference(static_cast<std::size_t>(rgb_line_step) * height, 0xa5);
  std::vector<std::uint8_t> candidate(reference.size(), 0xa5);

  debayer::debayerBilinearInteriorScalar(
      bayer.data(), reference.data(), width, height, bayer_line_step, bayer_line_step * 2, rgb_line_step);
  const auto path = debayer::debayerBilinearInteriorCandidate(
      bayer.data(), candidate.data(), width, height, bayer_line_step, bayer_line_step * 2, rgb_line_step);

#if defined(__RVV10__)
  EXPECT_EQ(path, debayer::ExecutionPath::RvvBilinearInterior);
#else
  EXPECT_EQ(path, debayer::ExecutionPath::ScalarFallback);
#endif
  expectInteriorEquals(reference, candidate, width, height, rgb_line_step);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
