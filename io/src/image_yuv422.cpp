/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011 2011 Willow Garage, Inc.
 *    Suat Gedikli <gedikli@willowgarage.com>
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <pcl/pcl_config.h>
#include <pcl/io/image_yuv422.h>

#include <pcl/io/io_exception.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

#define CLIP_CHAR(c) static_cast<unsigned char> ((c)>255?255:(c)<0?0:(c))

using pcl::io::FrameWrapper;
using pcl::io::IOException;

namespace
{
void
fillRGBStd (const std::uint8_t* yuv_buffer, unsigned source_width, unsigned source_height,
            unsigned width, unsigned height, unsigned char* rgb_buffer, unsigned rgb_line_step)
{
  unsigned rgb_line_skip = 0;
  if (rgb_line_step != 0)
    rgb_line_skip = rgb_line_step - width * 3;

  if (source_width == width && source_height == height)
  {
    for (unsigned yIdx = 0; yIdx < height; ++yIdx, rgb_buffer += rgb_line_skip)
    {
      for (unsigned xIdx = 0; xIdx < width; xIdx += 2, rgb_buffer += 6, yuv_buffer += 4)
      {
        int v = yuv_buffer[2] - 128;
        int u = yuv_buffer[0] - 128;

        rgb_buffer[0] =  CLIP_CHAR (yuv_buffer[1] + ((v * 18678 + 8192 ) >> 14));
        rgb_buffer[1] =  CLIP_CHAR (yuv_buffer[1] + ((v * -9519 - u * 6472 + 8192 ) >> 14));
        rgb_buffer[2] =  CLIP_CHAR (yuv_buffer[1] + ((u * 33292 + 8192 ) >> 14));

        rgb_buffer[3] =  CLIP_CHAR (yuv_buffer[3] + ((v * 18678 + 8192 ) >> 14));
        rgb_buffer[4] =  CLIP_CHAR (yuv_buffer[3] + ((v * -9519 - u * 6472 + 8192 ) >> 14));
        rgb_buffer[5] =  CLIP_CHAR (yuv_buffer[3] + ((u * 33292 + 8192 ) >> 14));
      }
    }
  }
  else
  {
    unsigned yuv_step = source_width / width;
    unsigned yuv_x_step = yuv_step << 1;
    unsigned yuv_skip = (source_height / height - 1) * ( source_width << 1 );

    for (unsigned yIdx = 0; yIdx < source_height; yIdx += yuv_step, yuv_buffer += yuv_skip, rgb_buffer += rgb_line_skip)
    {
      for (unsigned xIdx = 0; xIdx < source_width; xIdx += yuv_step, rgb_buffer += 3, yuv_buffer += yuv_x_step)
      {
        int v = yuv_buffer[2] - 128;
        int u = yuv_buffer[0] - 128;

        rgb_buffer[0] =  CLIP_CHAR (yuv_buffer[1] + ((v * 18678 + 8192 ) >> 14));
        rgb_buffer[1] =  CLIP_CHAR (yuv_buffer[1] + ((v * -9519 - u * 6472 + 8192 ) >> 14));
        rgb_buffer[2] =  CLIP_CHAR (yuv_buffer[1] + ((u * 33292 + 8192 ) >> 14));
      }
    }
  }
}

#if defined(__RVV10__)
vint32m2_t
u8ToI32 (const vuint8mf2_t value, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1 (value, vl);
  return __riscv_vwadd_vx_i32m2 (__riscv_vreinterpret_v_u16m1_i16m1 (widened), 0, vl);
}

vint32m2_t
u8OffsetToI32 (const vuint8mf2_t value, const int offset, const std::size_t vl)
{
  const vuint16m1_t widened = __riscv_vzext_vf2_u16m1 (value, vl);
  const vuint16m1_t shifted = __riscv_vsub_vx_u16m1 (widened, offset, vl);
  return __riscv_vwadd_vx_i32m2 (__riscv_vreinterpret_v_u16m1_i16m1 (shifted), 0, vl);
}

vuint8mf2_t
clipI32ToU8 (vint32m2_t value, const std::size_t vl)
{
  value = __riscv_vmax_vx_i32m2 (value, 0, vl);
  value = __riscv_vmin_vx_i32m2 (value, 255, vl);
  const vuint32m2_t value_u32 = __riscv_vreinterpret_v_i32m2_u32m2 (value);
  const vuint16m1_t value_u16 = __riscv_vncvt_x_x_w_u16m1 (value_u32, vl);
  return __riscv_vncvt_x_x_w_u8mf2 (value_u16, vl);
}

vuint8mf2_t
rgbChannel (const vint32m2_t y, vint32m2_t delta, const std::size_t vl)
{
  delta = __riscv_vadd_vx_i32m2 (delta, 8192, vl);
  delta = __riscv_vsra_vx_i32m2 (delta, 14, vl);
  return clipI32ToU8 (__riscv_vadd_vv_i32m2 (y, delta, vl), vl);
}

void
storeRGBTriplet (std::uint8_t* base, const vuint8mf2_t r, const vuint8mf2_t g,
                 const vuint8mf2_t b, const std::size_t vl)
{
  vuint8mf2x3_t triplet = __riscv_vcreate_v_u8mf2x3 (r, g, b);
  __riscv_vssseg3e8_v_u8mf2x3 (base, 6, triplet, vl);
}

void
storeRGBTripletContiguous (std::uint8_t* base, const vuint8mf2_t r, const vuint8mf2_t g,
                           const vuint8mf2_t b, const std::size_t vl)
{
  vuint8mf2x3_t triplet = __riscv_vcreate_v_u8mf2x3 (r, g, b);
  __riscv_vsseg3e8_v_u8mf2x3 (base, triplet, vl);
}

void
fillRGBFullSizeRVV (const std::uint8_t* yuv_buffer, unsigned width, unsigned height,
                    unsigned char* rgb_buffer, unsigned rgb_line_step)
{
  const unsigned rgb_step = rgb_line_step == 0 ? width * 3 : rgb_line_step;
  const unsigned pairs = width / 2;

  // YUYV packs two pixels as U/Y1/V/Y2; each vector lane converts one pair.
  for (unsigned row = 0; row < height; ++row)
  {
    const auto* src = yuv_buffer + static_cast<std::size_t> (row) * width * 2;
    auto* dst = rgb_buffer + static_cast<std::size_t> (row) * rgb_step;
    for (unsigned pair = 0; pair < pairs;)
    {
      const std::size_t vl = __riscv_vsetvl_e8mf2 (pairs - pair);
      const auto* base = src + static_cast<std::size_t> (pair) * 4;
      auto* out = dst + static_cast<std::size_t> (pair) * 6;

      const vuint8mf2_t u8 = __riscv_vlse8_v_u8mf2 (base + 0, 4, vl);
      const vuint8mf2_t y1_8 = __riscv_vlse8_v_u8mf2 (base + 1, 4, vl);
      const vuint8mf2_t v8 = __riscv_vlse8_v_u8mf2 (base + 2, 4, vl);
      const vuint8mf2_t y2_8 = __riscv_vlse8_v_u8mf2 (base + 3, 4, vl);

      const vint32m2_t u = u8OffsetToI32 (u8, 128, vl);
      const vint32m2_t v = u8OffsetToI32 (v8, 128, vl);
      const vint32m2_t y1 = u8ToI32 (y1_8, vl);
      const vint32m2_t y2 = u8ToI32 (y2_8, vl);

      const vint32m2_t r_delta = __riscv_vmul_vx_i32m2 (v, 18678, vl);
      vint32m2_t g_delta = __riscv_vmul_vx_i32m2 (v, -9519, vl);
      g_delta = __riscv_vsub_vv_i32m2 (g_delta, __riscv_vmul_vx_i32m2 (u, 6472, vl), vl);
      const vint32m2_t b_delta = __riscv_vmul_vx_i32m2 (u, 33292, vl);

      storeRGBTriplet (out + 0,
                       rgbChannel (y1, r_delta, vl),
                       rgbChannel (y1, g_delta, vl),
                       rgbChannel (y1, b_delta, vl),
                       vl);
      storeRGBTriplet (out + 3,
                       rgbChannel (y2, r_delta, vl),
                       rgbChannel (y2, g_delta, vl),
                       rgbChannel (y2, b_delta, vl),
                       vl);

      pair += static_cast<unsigned> (vl);
    }
  }
}

bool
canUseRGBDownsampleRVV (unsigned source_width, unsigned source_height, unsigned width, unsigned height)
{
  if (source_width == width || source_height == height || width == 0 || height == 0)
    return false;
  if (source_width % width != 0 || source_height % height != 0)
    return false;
  return ((source_width / width) % 2) == 0 && ((source_height / height) % 2) == 0;
}

void
fillRGBDownsampleRVV (const std::uint8_t* yuv_buffer, unsigned source_width, unsigned source_height,
                      unsigned width, unsigned height, unsigned char* rgb_buffer, unsigned rgb_line_step)
{
  const unsigned rgb_step = rgb_line_step == 0 ? width * 3 : rgb_line_step;
  const unsigned yuv_step = source_width / width;
  const unsigned yuv_x_step = yuv_step << 1;

  // The scalar downsample path samples the first pixel of each selected YUYV pair.
  for (unsigned row = 0; row < height; ++row)
  {
    const auto* src = yuv_buffer + static_cast<std::size_t> (row) * yuv_step * source_width * 2;
    auto* dst = rgb_buffer + static_cast<std::size_t> (row) * rgb_step;
    for (unsigned x = 0; x < width;)
    {
      const std::size_t vl = __riscv_vsetvl_e8mf2 (width - x);
      const auto* base = src + static_cast<std::size_t> (x) * yuv_x_step;
      auto* out = dst + static_cast<std::size_t> (x) * 3;

      const vuint8mf2_t u8 = __riscv_vlse8_v_u8mf2 (base + 0, yuv_x_step, vl);
      const vuint8mf2_t y8 = __riscv_vlse8_v_u8mf2 (base + 1, yuv_x_step, vl);
      const vuint8mf2_t v8 = __riscv_vlse8_v_u8mf2 (base + 2, yuv_x_step, vl);

      const vint32m2_t u = u8OffsetToI32 (u8, 128, vl);
      const vint32m2_t v = u8OffsetToI32 (v8, 128, vl);
      const vint32m2_t yy = u8ToI32 (y8, vl);

      const vint32m2_t r_delta = __riscv_vmul_vx_i32m2 (v, 18678, vl);
      vint32m2_t g_delta = __riscv_vmul_vx_i32m2 (v, -9519, vl);
      g_delta = __riscv_vsub_vv_i32m2 (g_delta, __riscv_vmul_vx_i32m2 (u, 6472, vl), vl);
      const vint32m2_t b_delta = __riscv_vmul_vx_i32m2 (u, 33292, vl);

      storeRGBTripletContiguous (out,
                                 rgbChannel (yy, r_delta, vl),
                                 rgbChannel (yy, g_delta, vl),
                                 rgbChannel (yy, b_delta, vl),
                                 vl);

      x += static_cast<unsigned> (vl);
    }
  }
}
#endif

} // namespace

pcl::io::ImageYUV422::ImageYUV422 (FrameWrapper::Ptr image_metadata)
  : Image (std::move(image_metadata))
{}


pcl::io::ImageYUV422::ImageYUV422 (FrameWrapper::Ptr image_metadata, Timestamp timestamp)
  : Image (std::move(image_metadata), timestamp)
{}


pcl::io::ImageYUV422::~ImageYUV422 () noexcept = default;

bool
pcl::io::ImageYUV422::isResizingSupported (unsigned input_width, unsigned input_height, unsigned output_width, unsigned output_height) const
{
  return (output_width <= input_width && output_height <= input_height && input_width % output_width == 0 && input_height % output_height == 0 );
}


void
pcl::io::ImageYUV422::fillRGB (unsigned width, unsigned height, unsigned char* rgb_buffer, unsigned rgb_line_step) const
{
  // 0  1   2  3
  // u  y1  v  y2

  if (wrapper_->getWidth () != width && wrapper_->getHeight () != height)
  {
    if (width > wrapper_->getWidth () || height > wrapper_->getHeight () )
      THROW_IO_EXCEPTION ("Upsampling not supported. Request was: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

    if ( wrapper_->getWidth () % width != 0 || wrapper_->getHeight () % height != 0
      || (wrapper_->getWidth () / width) & 0x01 || (wrapper_->getHeight () / height & 0x01) )
      THROW_IO_EXCEPTION ("Downsampling only possible for power of two scale in both dimensions. Request was %d x %d -> %d x %d.", wrapper_->getWidth (), wrapper_->getHeight (), width, height);
  }

  const auto* yuv_buffer = reinterpret_cast<const std::uint8_t*>(wrapper_->getData ());

#if defined(__RVV10__)
  if (wrapper_->getWidth () == width && wrapper_->getHeight () == height && (width % 2) == 0)
  {
    fillRGBFullSizeRVV (yuv_buffer, width, height, rgb_buffer, rgb_line_step);
    return;
  }

  if (canUseRGBDownsampleRVV (wrapper_->getWidth (), wrapper_->getHeight (), width, height))
  {
    fillRGBDownsampleRVV (yuv_buffer, wrapper_->getWidth (), wrapper_->getHeight (), width, height, rgb_buffer, rgb_line_step);
    return;
  }
#endif

  fillRGBStd (yuv_buffer, wrapper_->getWidth (), wrapper_->getHeight (), width, height, rgb_buffer, rgb_line_step);
}


void
pcl::io::ImageYUV422::fillGrayscale (unsigned width, unsigned height, unsigned char* gray_buffer, unsigned gray_line_step) const
{
  // u y1 v y2
  if (width > wrapper_->getWidth () || height > wrapper_->getHeight ())
    THROW_IO_EXCEPTION ("Upsampling not supported. Request was: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  if (wrapper_->getWidth () % width != 0 || wrapper_->getHeight () % height != 0)
    THROW_IO_EXCEPTION ("Downsampling only possible for integer scales in both dimensions. Request was %d x %d -> %d x %d.", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  unsigned gray_line_skip = 0;
  if (gray_line_step != 0)
    gray_line_skip = gray_line_step - width;

  unsigned yuv_step = wrapper_->getWidth () / width;
  unsigned yuv_x_step = yuv_step << 1;
  unsigned yuv_skip = (wrapper_->getHeight () / height - 1) * ( wrapper_->getWidth () << 1 );
  const std::uint8_t* yuv_buffer = ( reinterpret_cast<const std::uint8_t*>(wrapper_->getData ()) + 1);

  for (unsigned yIdx = 0; yIdx < wrapper_->getHeight (); yIdx += yuv_step, yuv_buffer += yuv_skip, gray_buffer += gray_line_skip)
  {
    for (unsigned xIdx = 0; xIdx < wrapper_->getWidth (); xIdx += yuv_step, ++gray_buffer, yuv_buffer += yuv_x_step)
    {
      *gray_buffer = *yuv_buffer;
    }
  }
}
