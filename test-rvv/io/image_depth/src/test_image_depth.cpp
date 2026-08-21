/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 image_depth 的测试专用候选
 * helper 与标量参考链路一致。测试覆盖 invalid pixel（无效像素）、
 * millimeter-to-meter（毫米到米）转换、disparity（视差）除法、最近邻
 * downsample（下采样）和 line_step padding（行尾填充）。
 *
 * 证据边界：
 * 这里还没有修改 production（生产源码）里的 DepthImage 公开入口，也不证明
 * production dispatch（生产分流）已经接入 RVV。它只证明本 topic 的
 * production-shaped diagnostic（生产形态诊断）候选可作为后续板卡 bench 的对拍对象。
 */

#include "image_depth.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <pcl/io/image_depth.h>
#include <pcl/io/image_metadata_wrapper.h>
#include <pcl/io/io_exception.h>

namespace depth = pcl::io::rvv_image_depth_support;

namespace {

class VectorFrameWrapper : public pcl::io::FrameWrapper {
public:
  VectorFrameWrapper(const unsigned width,
                     const unsigned height,
                     std::vector<std::uint16_t> pixels)
  : width_(width), height_(height), pixels_(std::move(pixels))
  {}

  const void*
  getData() const override
  {
    return pixels_.data();
  }

  unsigned
  getDataSize() const override
  {
    return static_cast<unsigned>(pixels_.size() * sizeof(std::uint16_t));
  }

  unsigned
  getWidth() const override
  {
    return width_;
  }

  unsigned
  getHeight() const override
  {
    return height_;
  }

  unsigned
  getFrameID() const override
  {
    return 7;
  }

  std::uint64_t
  getTimestamp() const override
  {
    return 123456;
  }

private:
  unsigned width_;
  unsigned height_;
  std::vector<std::uint16_t> pixels_;
};

enum class ProductionPathHook {
  None = 0,
  DepthScalar = 1,
  DepthContiguousRvv = 2,
  DepthDownsampleRvv = 3,
  DisparityScalar = 4,
  DisparityContiguousRvv = 5,
  DisparityDownsampleRvv = 6,
};

#if defined(__RVV10__)
extern "C" __attribute__((weak)) void
pcl_rvv_image_depth_reset_test_hook()
{}
extern "C" __attribute__((weak)) int
pcl_rvv_image_depth_last_test_hook()
{
  return static_cast<int>(ProductionPathHook::None);
}
#endif

std::vector<std::uint16_t>
makeDepthPixels(const unsigned width, const unsigned height)
{
  std::vector<std::uint16_t> pixels(static_cast<std::size_t>(width) * height);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y) * width + x;
      pixels[index] = static_cast<std::uint16_t>(450 + ((x * 37 + y * 53) % 4200));
      if ((x + 2 * y) % 17 == 0)
        pixels[index] = 0;
      if ((x + y) % 29 == 0)
        pixels[index] = 2047;
      if ((3 * x + y) % 31 == 0)
        pixels[index] = 65535;
    }
  }
  return pixels;
}

void
expectSameFloatImage(const std::vector<float>& reference, const std::vector<float>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    if (std::isnan(reference[i])) {
      EXPECT_TRUE(std::isnan(candidate[i])) << "index=" << i;
    }
    else {
      EXPECT_FLOAT_EQ(reference[i], candidate[i]) << "index=" << i;
    }
  }
}

pcl::io::DepthImage
makeDepthImage(const unsigned width,
               const unsigned height,
               std::vector<std::uint16_t> pixels,
               const float baseline = 0.075f,
               const float focal_length = 525.0f)
{
  pcl::io::FrameWrapper::Ptr wrapper =
      std::make_shared<VectorFrameWrapper>(width, height, std::move(pixels));
  return pcl::io::DepthImage(wrapper, baseline, focal_length, 65535, 2047);
}

} // namespace

TEST(ImageDepthDiagnostic, DepthFloatMatchesScalarWithInvalidPixelsAndPadding)
{
  const unsigned src_width = 19;
  const unsigned src_height = 7;
  const unsigned dst_width = 19;
  const unsigned dst_height = 7;
  const unsigned line_step = (dst_width + 3) * static_cast<unsigned>(sizeof(float));
  const auto pixels = makeDepthPixels(src_width, src_height);
  std::vector<float> reference(static_cast<std::size_t>(line_step / sizeof(float)) * dst_height,
                               -123.0f);
  std::vector<float> candidate(reference.size(), -123.0f);

  depth::fillDepthMetersScalar(pixels.data(),
                               src_width,
                               src_height,
                               2047,
                               65535,
                               dst_width,
                               dst_height,
                               reference.data(),
                               line_step);
  depth::fillDepthMetersCandidate(pixels.data(),
                                  src_width,
                                  src_height,
                                  2047,
                                  65535,
                                  dst_width,
                                  dst_height,
                                  candidate.data(),
                                  line_step);

  expectSameFloatImage(reference, candidate);
  for (unsigned y = 0; y < dst_height; ++y) {
    const auto row = static_cast<std::size_t>(y) * (line_step / sizeof(float));
    EXPECT_FLOAT_EQ(candidate[row + dst_width], -123.0f);
    EXPECT_FLOAT_EQ(candidate[row + dst_width + 1], -123.0f);
    EXPECT_FLOAT_EQ(candidate[row + dst_width + 2], -123.0f);
  }
}

TEST(ImageDepthDiagnostic, DisparityMatchesScalarWithDownsampleStride)
{
  const unsigned src_width = 24;
  const unsigned src_height = 12;
  const unsigned dst_width = 8;
  const unsigned dst_height = 6;
  const unsigned line_step = dst_width * static_cast<unsigned>(sizeof(float));
  const auto pixels = makeDepthPixels(src_width, src_height);
  std::vector<float> reference(static_cast<std::size_t>(dst_width) * dst_height, 0.0f);
  std::vector<float> candidate(reference.size(), 0.0f);

  depth::fillDisparityScalar(pixels.data(),
                             src_width,
                             src_height,
                             2047,
                             65535,
                             0.075f,
                             525.0f,
                             dst_width,
                             dst_height,
                             reference.data(),
                             line_step);
  depth::fillDisparityCandidate(pixels.data(),
                                src_width,
                                src_height,
                                2047,
                                65535,
                                0.075f,
                                525.0f,
                                dst_width,
                                dst_height,
                                candidate.data(),
                                line_step);

  expectSameFloatImage(reference, candidate);
}

TEST(ImageDepthDiagnostic, ZeroLineStepUsesTightOutputRows)
{
  const unsigned src_width = 32;
  const unsigned src_height = 5;
  const unsigned dst_width = 32;
  const unsigned dst_height = 5;
  const auto pixels = makeDepthPixels(src_width, src_height);
  std::vector<float> reference(static_cast<std::size_t>(dst_width) * dst_height, 0.0f);
  std::vector<float> candidate(reference.size(), 0.0f);

  depth::fillDepthMetersScalar(
      pixels.data(), src_width, src_height, 2047, 65535, dst_width, dst_height, reference.data(), 0);
  depth::fillDepthMetersCandidate(
      pixels.data(), src_width, src_height, 2047, 65535, dst_width, dst_height, candidate.data(), 0);

  expectSameFloatImage(reference, candidate);
}

TEST(ImageDepthDiagnostic, RvvBuildSelectsDownsamplePathForIntegerStride)
{
#if defined(__RVV10__)
  EXPECT_EQ(depth::CandidatePath::DownsampleRvv,
            depth::selectDepthMetersCandidatePath(24, 12, 8, 6));
  EXPECT_EQ(depth::CandidatePath::DownsampleRvv,
            depth::selectDisparityCandidatePath(24, 12, 8, 6));
#else
  EXPECT_EQ(depth::CandidatePath::Scalar,
            depth::selectDepthMetersCandidatePath(24, 12, 8, 6));
  EXPECT_EQ(depth::CandidatePath::Scalar,
            depth::selectDisparityCandidatePath(24, 12, 8, 6));
#endif
}

TEST(ImageDepthProductionPublic, DepthDownsampleFallsBackAfterProductionEvidenceDidNotSupportRvv)
{
  const unsigned src_width = 24;
  const unsigned src_height = 12;
  const unsigned dst_width = 8;
  const unsigned dst_height = 6;
  const unsigned line_step = (dst_width + 2) * static_cast<unsigned>(sizeof(float));
  const auto pixels = makeDepthPixels(src_width, src_height);
  std::vector<float> reference(static_cast<std::size_t>(line_step / sizeof(float)) * dst_height,
                               -123.0f);
  std::vector<float> production(reference.size(), -123.0f);
  auto image = makeDepthImage(src_width, src_height, pixels);

  depth::fillDepthMetersScalar(pixels.data(),
                               src_width,
                               src_height,
                               2047,
                               65535,
                               dst_width,
                               dst_height,
                               reference.data(),
                               line_step);
#if defined(__RVV10__)
  pcl_rvv_image_depth_reset_test_hook();
#endif
  image.fillDepthImage(dst_width, dst_height, production.data(), line_step);

  expectSameFloatImage(reference, production);
  for (unsigned y = 0; y < dst_height; ++y) {
    const auto row = static_cast<std::size_t>(y) * (line_step / sizeof(float));
    EXPECT_FLOAT_EQ(production[row + dst_width], -123.0f);
    EXPECT_FLOAT_EQ(production[row + dst_width + 1], -123.0f);
  }
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::DepthScalar),
            pcl_rvv_image_depth_last_test_hook());
#endif
}

TEST(ImageDepthProductionPublic, DepthFloatHitsContiguousRvvPathWithZeroLineStep)
{
  const unsigned src_width = 32;
  const unsigned src_height = 5;
  const auto pixels = makeDepthPixels(src_width, src_height);
  std::vector<float> reference(static_cast<std::size_t>(src_width) * src_height, -1.0f);
  std::vector<float> production(reference.size(), -1.0f);
  auto image = makeDepthImage(src_width, src_height, pixels);

  depth::fillDepthMetersScalar(pixels.data(),
                               src_width,
                               src_height,
                               2047,
                               65535,
                               src_width,
                               src_height,
                               reference.data(),
                               0);
#if defined(__RVV10__)
  pcl_rvv_image_depth_reset_test_hook();
#endif
  image.fillDepthImage(src_width, src_height, production.data(), 0);

  expectSameFloatImage(reference, production);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::DepthContiguousRvv),
            pcl_rvv_image_depth_last_test_hook());
#endif
}

TEST(ImageDepthProductionPublic, DisparityMatchesScalarAndHitsContiguousRvvPath)
{
  const unsigned src_width = 32;
  const unsigned src_height = 5;
  const auto pixels = makeDepthPixels(src_width, src_height);
  std::vector<float> reference(static_cast<std::size_t>(src_width) * src_height, -1.0f);
  std::vector<float> production(reference.size(), -1.0f);
  auto image = makeDepthImage(src_width, src_height, pixels);

  depth::fillDisparityScalar(pixels.data(),
                             src_width,
                             src_height,
                             2047,
                             65535,
                             0.075f,
                             525.0f,
                             src_width,
                             src_height,
                             reference.data(),
                             0);
#if defined(__RVV10__)
  pcl_rvv_image_depth_reset_test_hook();
#endif
  image.fillDisparityImage(src_width, src_height, production.data(), 0);

  expectSameFloatImage(reference, production);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::DisparityContiguousRvv),
            pcl_rvv_image_depth_last_test_hook());
#endif
}

TEST(ImageDepthProductionPublic, DisparityHitsDownsampleRvvPath)
{
  const unsigned src_width = 24;
  const unsigned src_height = 12;
  const unsigned dst_width = 8;
  const unsigned dst_height = 6;
  const auto pixels = makeDepthPixels(src_width, src_height);
  std::vector<float> reference(static_cast<std::size_t>(dst_width) * dst_height, -1.0f);
  std::vector<float> production(reference.size(), -1.0f);
  auto image = makeDepthImage(src_width, src_height, pixels);

  depth::fillDisparityScalar(pixels.data(),
                             src_width,
                             src_height,
                             2047,
                             65535,
                             0.075f,
                             525.0f,
                             dst_width,
                             dst_height,
                             reference.data(),
                             0);
#if defined(__RVV10__)
  pcl_rvv_image_depth_reset_test_hook();
#endif
  image.fillDisparityImage(dst_width, dst_height, production.data(), 0);

  expectSameFloatImage(reference, production);
#if defined(__RVV10__)
  EXPECT_EQ(static_cast<int>(ProductionPathHook::DisparityDownsampleRvv),
            pcl_rvv_image_depth_last_test_hook());
#endif
}

TEST(ImageDepthProductionPublic, KeepsExceptionBehaviorForNonIntegerDownsample)
{
  auto image = makeDepthImage(23, 11, makeDepthPixels(23, 11));
  std::vector<float> output(6 * 5, 0.0f);

  EXPECT_THROW(image.fillDepthImage(6, 5, output.data(), 0), pcl::io::IOException);
  EXPECT_THROW(image.fillDisparityImage(6, 5, output.data(), 0), pcl::io::IOException);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
