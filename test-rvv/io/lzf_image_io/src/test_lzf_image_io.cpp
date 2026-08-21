/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 lzf_image_io topic 的测试专用
 * helper 与当前 `LZF*ImageReader` 解压后转换语义一致。测试直接喂
 * uncompressed buffer（解压后的缓冲区），因此它不覆盖 LZF 解压、文件读取
 * 或 ImageGrabber 调度。
 *
 * 证据边界：
 * 这里是 production-shaped diagnostic（生产形态诊断），不是 production direct
 *（真实生产路径证据）。通过这些测试只能说明 post-decompress conversion
 * candidate（解压后转换候选）和标量 reference（参考链路）一致。
 */

#include "lzf_image_io.h"

#include <gtest/gtest.h>

#include <pcl/io/lzf_image_io.h>
#include <pcl/point_types.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace lzf = pcl::io::rvv_lzf_image_io_support;

namespace {

struct WideRgbPoint {
  std::uint16_t b = 0;
  std::uint16_t g = 0;
  std::uint16_t r = 0;
};

std::vector<std::uint16_t>
makeDepthValues(const unsigned width, const unsigned height)
{
  std::vector<std::uint16_t> values(static_cast<std::size_t>(width) * height);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y) * width + x;
      values[index] = static_cast<std::uint16_t>(400 + ((x * 23 + y * 41) % 6000));
      if ((x + y * 3) % 19 == 0)
        values[index] = 0;
      if ((x * 5 + y) % 97 == 0)
        values[index] = 65535;
    }
  }
  return values;
}

std::vector<std::uint8_t>
makePlanarYuv422(const unsigned width, const unsigned height)
{
  const auto pixels = static_cast<std::size_t>(width) * height;
  const auto pairs = pixels / 2;
  std::vector<std::uint8_t> data(pairs + pixels + pairs);
  auto* u_plane = data.data();
  auto* y_plane = data.data() + pairs;
  auto* v_plane = data.data() + pairs + pixels;
  for (std::size_t i = 0; i < pairs; ++i) {
    u_plane[i] = static_cast<std::uint8_t>((17 + i * 13) & 0xffu);
    v_plane[i] = static_cast<std::uint8_t>((223 + i * 7) & 0xffu);
  }
  for (std::size_t i = 0; i < pixels; ++i)
    y_plane[i] = static_cast<std::uint8_t>((31 + i * 5) & 0xffu);
  return data;
}

std::vector<std::uint8_t>
makeRgbBuffer(const unsigned width, const unsigned height)
{
  std::vector<std::uint8_t> data(static_cast<std::size_t>(width) * height * 3);
  for (std::size_t i = 0; i < data.size(); ++i)
    data[i] = static_cast<std::uint8_t>((11 + i * 29) & 0xffu);
  return data;
}

void
expectSameCloud(const std::vector<lzf::PointXYZRGB>& reference,
                const std::vector<lzf::PointXYZRGB>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    if (std::isnan(reference[i].x)) {
      EXPECT_TRUE(std::isnan(candidate[i].x)) << "x index=" << i;
      EXPECT_TRUE(std::isnan(candidate[i].y)) << "y index=" << i;
      EXPECT_TRUE(std::isnan(candidate[i].z)) << "z index=" << i;
    }
    else {
      EXPECT_FLOAT_EQ(reference[i].x, candidate[i].x) << "x index=" << i;
      EXPECT_FLOAT_EQ(reference[i].y, candidate[i].y) << "y index=" << i;
      EXPECT_FLOAT_EQ(reference[i].z, candidate[i].z) << "z index=" << i;
    }
    EXPECT_EQ(reference[i].r, candidate[i].r) << "r index=" << i;
    EXPECT_EQ(reference[i].g, candidate[i].g) << "g index=" << i;
    EXPECT_EQ(reference[i].b, candidate[i].b) << "b index=" << i;
  }
}

template <typename PointT>
void
expectSameRgb(const std::vector<PointT>& reference, const std::vector<PointT>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(reference[i].r, candidate[i].r) << "r index=" << i;
    EXPECT_EQ(reference[i].g, candidate[i].g) << "g index=" << i;
    EXPECT_EQ(reference[i].b, candidate[i].b) << "b index=" << i;
  }
}

} // namespace

TEST(LzfImageIoDiagnostic, DepthXyzMatchesScalarWithInvalidDepth)
{
  const unsigned width = 23;
  const unsigned height = 11;
  const auto depth = makeDepthValues(width, height);
  const lzf::DepthCameraParameters params{525.0f, 530.0f, 11.5f, 5.25f, 0.001f};
  std::vector<lzf::PointXYZRGB> reference(static_cast<std::size_t>(width) * height);
  std::vector<lzf::PointXYZRGB> candidate(reference.size());
  bool reference_dense = true;
  bool candidate_dense = true;

  lzf::convertDepthToCloudScalar(
      depth.data(), width, height, params, reference.data(), &reference_dense);
  lzf::convertDepthToCloudCandidate(
      depth.data(), width, height, params, candidate.data(), &candidate_dense);

  EXPECT_EQ(reference_dense, candidate_dense);
  expectSameCloud(reference, candidate);
}

TEST(LzfImageIoDiagnostic, PlanarYuv422ToRgbMatchesScalar)
{
  const unsigned width = 34;
  const unsigned height = 9;
  const auto yuv = makePlanarYuv422(width, height);
  std::vector<lzf::PointXYZRGB> reference(static_cast<std::size_t>(width) * height);
  std::vector<lzf::PointXYZRGB> candidate(reference.size());

  lzf::convertPlanarYuv422ToRgbScalar(yuv.data(), width, height, reference.data());
  lzf::convertPlanarYuv422ToRgbCandidate(yuv.data(), width, height, candidate.data());

  expectSameCloud(reference, candidate);
}

TEST(LzfImageIoProductionProbe, PlanarYuv422ProductionHelperMatchesScalarForPointXYZRGB)
{
  const unsigned width = 34;
  const unsigned height = 9;
  const auto yuv = makePlanarYuv422(width, height);
  std::vector<pcl::PointXYZRGB> reference(static_cast<std::size_t>(width) * height);
  std::vector<pcl::PointXYZRGB> candidate(reference.size());

  pcl::io::detail::convertPlanarYuv422ToPointCloudStd(
      yuv.data(), width, height, reference.data());
#if defined(__RVV10__)
  EXPECT_TRUE(pcl::io::detail::convertPlanarYuv422ToPointCloudRVV(
      yuv.data(), width, height, candidate.data()));
#else
  pcl::io::detail::convertPlanarYuv422ToPointCloudStd(
      yuv.data(), width, height, candidate.data());
#endif

  expectSameRgb(reference, candidate);
}

TEST(LzfImageIoProductionProbe, PlanarYuv422ProductionHelperRejectsNonByteRgbFields)
{
  const unsigned width = 34;
  const unsigned height = 9;
  const auto yuv = makePlanarYuv422(width, height);
  std::vector<WideRgbPoint> reference(static_cast<std::size_t>(width) * height);
  std::vector<WideRgbPoint> candidate(reference.size());

  pcl::io::detail::convertPlanarYuv422ToPointCloudStd(
      yuv.data(), width, height, reference.data());
#if defined(__RVV10__)
  EXPECT_FALSE(pcl::io::detail::convertPlanarYuv422ToPointCloudRVV(
      yuv.data(), width, height, candidate.data()));
#endif
  pcl::io::detail::convertPlanarYuv422ToPointCloudStd(
      yuv.data(), width, height, candidate.data());

  expectSameRgb(reference, candidate);
}

TEST(LzfImageIoDiagnostic, RgbBufferToCloudMatchesScalar)
{
  const unsigned width = 31;
  const unsigned height = 7;
  const auto rgb = makeRgbBuffer(width, height);
  std::vector<lzf::PointXYZRGB> reference(static_cast<std::size_t>(width) * height);
  std::vector<lzf::PointXYZRGB> candidate(reference.size());

  lzf::copyRgbBufferToCloudScalar(rgb.data(), width, height, reference.data());
  lzf::copyRgbBufferToCloudCandidate(rgb.data(), width, height, candidate.data());

  expectSameCloud(reference, candidate);
}
