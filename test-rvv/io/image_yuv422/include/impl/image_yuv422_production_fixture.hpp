#pragma once

/*
 * 本文件做什么：
 * 这里放 production direct（真实生产路径）测试和 bench 共用的最小
 * FrameWrapper。它只把内存中的 YUYV 字节交给真实 ImageYUV422 public
 * entry，不改变 production 源码，也不参与 test-only RVV candidate。
 */

#include <pcl/io/image_yuv422.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace pcl::io::rvv_image_yuv422_support {

class MemoryYuv422FrameWrapper final : public pcl::io::FrameWrapper {
public:
  MemoryYuv422FrameWrapper(const unsigned width,
                           const unsigned height,
                           std::vector<std::uint8_t> pixels)
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
    return static_cast<unsigned>(pixels_.size());
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
    return 1;
  }

  std::uint64_t
  getTimestamp() const override
  {
    return 0;
  }

private:
  unsigned width_;
  unsigned height_;
  std::vector<std::uint8_t> pixels_;
};

inline pcl::io::ImageYUV422
makeProductionImage(const unsigned width,
                    const unsigned height,
                    const std::vector<std::uint8_t>& pixels)
{
  pcl::io::FrameWrapper::Ptr frame(
      new MemoryYuv422FrameWrapper(width, height, std::vector<std::uint8_t>(pixels)));
  return pcl::io::ImageYUV422(frame);
}

} // namespace pcl::io::rvv_image_yuv422_support
