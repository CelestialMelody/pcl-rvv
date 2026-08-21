/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 image_yuv422 的测试专用
 * reference（参考链路）和 RVV candidate（候选链路）是否复刻
 * ImageYUV422::fillRGB() / fillGrayscale() 的 YUYV 语义。测试覆盖
 * U/V 两像素共享、CLIP_CHAR 饱和、downsample（下采样）stride 和
 * line_step padding（行尾填充）。
 *
 * 证据边界：
 * 这里还没有修改 production（生产源码）里的 ImageYUV422 公开入口，也不证明
 * production dispatch（生产分流）已经接入 RVV。它只证明本 topic 的
 * production-shaped diagnostic（生产形态诊断）候选可作为后续板卡 bench 的对拍对象。
 */

#include "image_yuv422.h"
#include "impl/image_yuv422_production_fixture.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace yuv = pcl::io::rvv_image_yuv422_support;

namespace {

std::vector<std::uint8_t>
makeYuyvPixels(const unsigned width, const unsigned height)
{
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 2);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; x += 2) {
      const auto pair = (static_cast<std::size_t>(y) * width + x) * 2;
      pixels[pair + 0] = static_cast<std::uint8_t>((37 * x + 19 * y + 11) & 0xff);
      pixels[pair + 1] = static_cast<std::uint8_t>((53 * x + 7 * y + 3) & 0xff);
      pixels[pair + 2] = static_cast<std::uint8_t>((13 * x + 97 * y + 251) & 0xff);
      pixels[pair + 3] = static_cast<std::uint8_t>((29 * x + 31 * y + 127) & 0xff);
    }
  }
  return pixels;
}

void
expectSameBytes(const std::vector<std::uint8_t>& reference,
                const std::vector<std::uint8_t>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i)
    EXPECT_EQ(reference[i], candidate[i]) << "index=" << i;
}

} // namespace

TEST(ImageYUV422Diagnostic, RGBFullSizeMatchesScalarWithSaturationAndPadding)
{
  const unsigned width = 34;
  const unsigned height = 7;
  const unsigned line_step = width * 3 + 5;
  const auto pixels = makeYuyvPixels(width, height);
  std::vector<std::uint8_t> reference(static_cast<std::size_t>(line_step) * height, 0xa5);
  std::vector<std::uint8_t> candidate(reference.size(), 0xa5);

  yuv::fillRgbScalar(pixels.data(), width, height, width, height, reference.data(), line_step);
  yuv::fillRgbCandidate(pixels.data(), width, height, width, height, candidate.data(), line_step);

  expectSameBytes(reference, candidate);
  for (unsigned y = 0; y < height; ++y) {
    const auto row_tail = static_cast<std::size_t>(y) * line_step + width * 3;
    for (unsigned pad = 0; pad < 5; ++pad)
      EXPECT_EQ(candidate[row_tail + pad], 0xa5) << "row=" << y << " pad=" << pad;
  }
}

TEST(ImageYUV422Diagnostic, RGBDownsampleKeepsScalarStrideSemantics)
{
  const unsigned src_width = 40;
  const unsigned src_height = 16;
  const unsigned dst_width = 10;
  const unsigned dst_height = 4;
  const unsigned line_step = dst_width * 3 + 4;
  const auto pixels = makeYuyvPixels(src_width, src_height);
  std::vector<std::uint8_t> reference(static_cast<std::size_t>(line_step) * dst_height, 0x6c);
  std::vector<std::uint8_t> candidate(reference.size(), 0x6c);

  yuv::fillRgbScalar(
      pixels.data(), src_width, src_height, dst_width, dst_height, reference.data(), line_step);
  yuv::fillRgbCandidate(
      pixels.data(), src_width, src_height, dst_width, dst_height, candidate.data(), line_step);

  expectSameBytes(reference, candidate);
  for (unsigned y = 0; y < dst_height; ++y) {
    const auto row_tail = static_cast<std::size_t>(y) * line_step + dst_width * 3;
    for (unsigned pad = 0; pad < 4; ++pad)
      EXPECT_EQ(candidate[row_tail + pad], 0x6c) << "row=" << y << " pad=" << pad;
  }
}

TEST(ImageYUV422Diagnostic, GrayscaleMatchesScalarForFullSizeAndDownsample)
{
  const unsigned src_width = 36;
  const unsigned src_height = 12;
  const auto pixels = makeYuyvPixels(src_width, src_height);

  {
    const unsigned line_step = src_width + 3;
    std::vector<std::uint8_t> reference(static_cast<std::size_t>(line_step) * src_height, 0x5a);
    std::vector<std::uint8_t> candidate(reference.size(), 0x5a);
    yuv::fillGrayscaleScalar(
        pixels.data(), src_width, src_height, src_width, src_height, reference.data(), line_step);
    yuv::fillGrayscaleCandidate(
        pixels.data(), src_width, src_height, src_width, src_height, candidate.data(), line_step);
    expectSameBytes(reference, candidate);
  }

  {
    const unsigned dst_width = 12;
    const unsigned dst_height = 6;
    std::vector<std::uint8_t> reference(static_cast<std::size_t>(dst_width) * dst_height, 0);
    std::vector<std::uint8_t> candidate(reference.size(), 0);
    yuv::fillGrayscaleScalar(
        pixels.data(), src_width, src_height, dst_width, dst_height, reference.data(), 0);
    yuv::fillGrayscaleCandidate(
        pixels.data(), src_width, src_height, dst_width, dst_height, candidate.data(), 0);
    expectSameBytes(reference, candidate);
  }
}

TEST(ImageYUV422ProductionDirect, RGBFullSizeMatchesDiagnosticReferenceWithPadding)
{
  const unsigned width = 34;
  const unsigned height = 7;
  const unsigned line_step = width * 3 + 5;
  const auto pixels = makeYuyvPixels(width, height);
  const auto image = yuv::makeProductionImage(width, height, pixels);
  std::vector<std::uint8_t> reference(static_cast<std::size_t>(line_step) * height, 0xa5);
  std::vector<std::uint8_t> production(reference.size(), 0xa5);

  yuv::fillRgbScalar(pixels.data(), width, height, width, height, reference.data(), line_step);
  image.fillRGB(width, height, production.data(), line_step);

  expectSameBytes(reference, production);
}

TEST(ImageYUV422ProductionDirect, RGBDownsampleMatchesScalarReferenceWithPadding)
{
  const unsigned src_width = 40;
  const unsigned src_height = 16;
  const unsigned dst_width = 10;
  const unsigned dst_height = 4;
  const unsigned line_step = dst_width * 3 + 4;
  const auto pixels = makeYuyvPixels(src_width, src_height);
  const auto image = yuv::makeProductionImage(src_width, src_height, pixels);
  std::vector<std::uint8_t> reference(static_cast<std::size_t>(line_step) * dst_height, 0x6c);
  std::vector<std::uint8_t> production(reference.size(), 0x6c);

  yuv::fillRgbScalar(
      pixels.data(), src_width, src_height, dst_width, dst_height, reference.data(), line_step);
  image.fillRGB(dst_width, dst_height, production.data(), line_step);

  expectSameBytes(reference, production);
  for (unsigned y = 0; y < dst_height; ++y) {
    const auto row_tail = static_cast<std::size_t>(y) * line_step + dst_width * 3;
    for (unsigned pad = 0; pad < 4; ++pad)
      EXPECT_EQ(production[row_tail + pad], 0x6c) << "row=" << y << " pad=" << pad;
  }
}

TEST(ImageYUV422ProductionDirect, GrayscaleStillMatchesScalarReference)
{
  const unsigned src_width = 36;
  const unsigned src_height = 12;
  const auto pixels = makeYuyvPixels(src_width, src_height);
  const auto image = yuv::makeProductionImage(src_width, src_height, pixels);

  {
    const unsigned line_step = src_width + 3;
    std::vector<std::uint8_t> reference(static_cast<std::size_t>(line_step) * src_height, 0x5a);
    std::vector<std::uint8_t> production(reference.size(), 0x5a);
    yuv::fillGrayscaleScalar(
        pixels.data(), src_width, src_height, src_width, src_height, reference.data(), line_step);
    image.fillGrayscale(src_width, src_height, production.data(), line_step);
    expectSameBytes(reference, production);
  }

  {
    const unsigned dst_width = 12;
    const unsigned dst_height = 6;
    std::vector<std::uint8_t> reference(static_cast<std::size_t>(dst_width) * dst_height, 0);
    std::vector<std::uint8_t> production(reference.size(), 0);
    yuv::fillGrayscaleScalar(
        pixels.data(), src_width, src_height, dst_width, dst_height, reference.data(), 0);
    image.fillGrayscale(dst_width, dst_height, production.data(), 0);
    expectSameBytes(reference, production);
  }
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
