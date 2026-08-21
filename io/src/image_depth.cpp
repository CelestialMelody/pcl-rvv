/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011 Willow Garage, Inc.
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
#include <pcl/io/image_depth.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include <pcl/io/io_exception.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

using pcl::io::FrameWrapper;
using pcl::io::IOException;

namespace {

enum class ImageDepthPathHook {
  None = 0,
  DepthScalar = 1,
  DepthContiguousRvv = 2,
  DepthDownsampleRvv = 3,
  DisparityScalar = 4,
  DisparityContiguousRvv = 5,
  DisparityDownsampleRvv = 6,
};

#if defined(PCL_RVV_IMAGE_DEPTH_TEST_HOOK)
int g_image_depth_last_hook = static_cast<int> (ImageDepthPathHook::None);

extern "C" void
pcl_rvv_image_depth_reset_test_hook ()
{
  g_image_depth_last_hook = static_cast<int> (ImageDepthPathHook::None);
}

extern "C" int
pcl_rvv_image_depth_last_test_hook ()
{
  return g_image_depth_last_hook;
}
#endif

void
recordImageDepthPath (ImageDepthPathHook path)
{
#if defined(PCL_RVV_IMAGE_DEPTH_TEST_HOOK)
  g_image_depth_last_hook = static_cast<int> (path);
#else
  (void)path;
#endif
}

bool
fitsU16Compare (std::uint64_t value)
{
  return value <= std::numeric_limits<unsigned short>::max ();
}

void
fillDepthImageStd (const unsigned short* inputBuffer,
                   unsigned src_width,
                   unsigned src_height,
                   std::uint64_t no_sample_value,
                   std::uint64_t shadow_value,
                   unsigned width,
                   unsigned height,
                   float* depth_buffer,
                   unsigned line_step)
{
  recordImageDepthPath (ImageDepthPathHook::DepthScalar);
  unsigned bufferSkip = line_step - width * static_cast<unsigned> (sizeof (float));
  unsigned xStep = src_width / width;
  unsigned ySkip = (src_height / height - 1) * src_width;
  float bad_point = std::numeric_limits<float>::quiet_NaN ();
  unsigned depthIdx = 0;

  for (unsigned yIdx = 0; yIdx < height; ++yIdx, depthIdx += ySkip)
  {
    for (unsigned xIdx = 0; xIdx < width; ++xIdx, depthIdx += xStep, ++depth_buffer)
    {
      unsigned short pixel = inputBuffer[depthIdx];
      if (pixel == 0 || pixel == no_sample_value || pixel == shadow_value)
        *depth_buffer = bad_point;
      else
      {
        *depth_buffer = static_cast<unsigned short> (pixel) * 0.001f;  // millimeters to meters
      }
    }
    if (bufferSkip > 0)
    {
      char* cBuffer = reinterpret_cast<char*> (depth_buffer);
      depth_buffer = reinterpret_cast<float*> (cBuffer + bufferSkip);
    }
  }
}

void
fillDisparityImageStd (const unsigned short* inputBuffer,
                       unsigned src_width,
                       unsigned src_height,
                       std::uint64_t no_sample_value,
                       std::uint64_t shadow_value,
                       float baseline,
                       float focal_length,
                       unsigned width,
                       unsigned height,
                       float* disparity_buffer,
                       unsigned line_step)
{
  recordImageDepthPath (ImageDepthPathHook::DisparityScalar);
  unsigned xStep = src_width / width;
  unsigned ySkip = (src_height / height - 1) * src_width;
  unsigned bufferSkip = line_step - width * static_cast<unsigned> (sizeof (float));
  float constant = focal_length * baseline * 1000.0f / static_cast<float> (xStep);

  for (unsigned yIdx = 0, depthIdx = 0; yIdx < height; ++yIdx, depthIdx += ySkip)
  {
    for (unsigned xIdx = 0; xIdx < width; ++xIdx, depthIdx += xStep, ++disparity_buffer)
    {
      unsigned short pixel = inputBuffer[depthIdx];
      if (pixel == 0 || pixel == no_sample_value || pixel == shadow_value)
        *disparity_buffer = 0.0;
      else
        *disparity_buffer = constant / static_cast<float> (pixel);
    }
    if (bufferSkip > 0)
    {
      char* cBuffer = reinterpret_cast<char*> (disparity_buffer);
      disparity_buffer = reinterpret_cast<float*> (cBuffer + bufferSkip);
    }
  }
}

#if defined(__RVV10__)
vbool8_t
invalidDepthMask (vuint16m2_t pixels,
                  std::uint64_t no_sample_value,
                  std::uint64_t shadow_value,
                  std::size_t vl)
{
  vbool8_t invalid = __riscv_vmseq_vx_u16m2_b8 (pixels, 0, vl);
  if (fitsU16Compare (no_sample_value))
  {
    invalid = __riscv_vmor_mm_b8 (
        invalid,
        __riscv_vmseq_vx_u16m2_b8 (pixels, static_cast<unsigned short> (no_sample_value), vl),
        vl);
  }
  if (fitsU16Compare (shadow_value))
  {
    invalid = __riscv_vmor_mm_b8 (
        invalid,
        __riscv_vmseq_vx_u16m2_b8 (pixels, static_cast<unsigned short> (shadow_value), vl),
        vl);
  }
  return invalid;
}

vfloat32m4_t
depthPixelsToFloat (vuint16m2_t pixels, std::size_t vl)
{
  const vuint32m4_t widened = __riscv_vwaddu_vx_u32m4 (pixels, 0, vl);
  return __riscv_vfcvt_f_xu_v_f32m4 (widened, vl);
}

void
fillDepthImageContiguousRVV (const unsigned short* inputBuffer,
                             std::uint64_t no_sample_value,
                             std::uint64_t shadow_value,
                             unsigned width,
                             unsigned height,
                             float* depth_buffer,
                             unsigned line_step)
{
  recordImageDepthPath (ImageDepthPathHook::DepthContiguousRvv);
  const unsigned row_stride = line_step / static_cast<unsigned> (sizeof (float));
  const float bad_point = std::numeric_limits<float>::quiet_NaN ();

  for (unsigned y = 0; y < height; ++y)
  {
    const auto* src = inputBuffer + static_cast<std::size_t> (y) * width;
    auto* dst = depth_buffer + static_cast<std::size_t> (y) * row_stride;
    for (unsigned x = 0; x < width;)
    {
      const std::size_t vl = __riscv_vsetvl_e16m2 (width - x);
      const vuint16m2_t pixels = __riscv_vle16_v_u16m2 (src + x, vl);
      const vbool8_t invalid = invalidDepthMask (pixels, no_sample_value, shadow_value, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8 (invalid, vl);
      const vfloat32m4_t values =
          __riscv_vfmul_vf_f32m4 (depthPixelsToFloat (pixels, vl), 0.001f, vl);
      const vfloat32m4_t bad = __riscv_vfmv_v_f_f32m4 (bad_point, vl);

      __riscv_vse32_v_f32m4 (dst + x, bad, vl);
      __riscv_vse32_v_f32m4_m (valid, dst + x, values, vl);
      x += static_cast<unsigned> (vl);
    }
  }
}

void
fillDisparityImageContiguousRVV (const unsigned short* inputBuffer,
                                 std::uint64_t no_sample_value,
                                 std::uint64_t shadow_value,
                                 float constant,
                                 unsigned width,
                                 unsigned height,
                                 float* disparity_buffer,
                                 unsigned line_step)
{
  recordImageDepthPath (ImageDepthPathHook::DisparityContiguousRvv);
  const unsigned row_stride = line_step / static_cast<unsigned> (sizeof (float));

  for (unsigned y = 0; y < height; ++y)
  {
    const auto* src = inputBuffer + static_cast<std::size_t> (y) * width;
    auto* dst = disparity_buffer + static_cast<std::size_t> (y) * row_stride;
    for (unsigned x = 0; x < width;)
    {
      const std::size_t vl = __riscv_vsetvl_e16m2 (width - x);
      const vuint16m2_t pixels = __riscv_vle16_v_u16m2 (src + x, vl);
      const vbool8_t invalid = invalidDepthMask (pixels, no_sample_value, shadow_value, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8 (invalid, vl);
      const vfloat32m4_t values =
          __riscv_vfrdiv_vf_f32m4 (depthPixelsToFloat (pixels, vl), constant, vl);
      const vfloat32m4_t zero = __riscv_vfmv_v_f_f32m4 (0.0f, vl);

      __riscv_vse32_v_f32m4 (dst + x, zero, vl);
      __riscv_vse32_v_f32m4_m (valid, dst + x, values, vl);
      x += static_cast<unsigned> (vl);
    }
  }
}

void
fillDisparityImageDownsampleRVV (const unsigned short* inputBuffer,
                                 unsigned src_width,
                                 unsigned src_height,
                                 std::uint64_t no_sample_value,
                                 std::uint64_t shadow_value,
                                 float constant,
                                 unsigned width,
                                 unsigned height,
                                 float* disparity_buffer,
                                 unsigned line_step)
{
  recordImageDepthPath (ImageDepthPathHook::DisparityDownsampleRvv);
  const unsigned xStep = src_width / width;
  const unsigned yStep = src_height / height;
  const unsigned row_stride = line_step / static_cast<unsigned> (sizeof (float));
  const std::ptrdiff_t stride_bytes =
      static_cast<std::ptrdiff_t> (xStep * sizeof (unsigned short));

  for (unsigned y = 0; y < height; ++y)
  {
    const auto* src = inputBuffer + static_cast<std::size_t> (y) * yStep * src_width;
    auto* dst = disparity_buffer + static_cast<std::size_t> (y) * row_stride;
    for (unsigned x = 0; x < width;)
    {
      const std::size_t vl = __riscv_vsetvl_e16m2 (width - x);
      const vuint16m2_t pixels =
          __riscv_vlse16_v_u16m2 (src + static_cast<std::size_t> (x) * xStep,
                                  stride_bytes,
                                  vl);
      const vbool8_t invalid = invalidDepthMask (pixels, no_sample_value, shadow_value, vl);
      const vbool8_t valid = __riscv_vmnot_m_b8 (invalid, vl);
      const vfloat32m4_t values =
          __riscv_vfrdiv_vf_f32m4 (depthPixelsToFloat (pixels, vl), constant, vl);
      const vfloat32m4_t zero = __riscv_vfmv_v_f_f32m4 (0.0f, vl);

      __riscv_vse32_v_f32m4 (dst + x, zero, vl);
      __riscv_vse32_v_f32m4_m (valid, dst + x, values, vl);
      x += static_cast<unsigned> (vl);
    }
  }
}

bool
canUseFloatImageRVV (unsigned src_width,
                     unsigned src_height,
                     unsigned width,
                     unsigned height,
                     unsigned line_step)
{
  return line_step % static_cast<unsigned> (sizeof (float)) == 0 &&
         ((src_width == width && src_height == height) ||
          (src_width % width == 0 && src_height % height == 0 &&
           (src_width / width) > 1));
}
#endif

} // namespace

pcl::io::DepthImage::DepthImage (FrameWrapper::Ptr depth_metadata, float baseline, float focal_length, std::uint64_t shadow_value, std::uint64_t no_sample_value)
: wrapper_ (std::move(depth_metadata))
, baseline_ (baseline)
, focal_length_ (focal_length)
, shadow_value_ (shadow_value)
, no_sample_value_ (no_sample_value)
, timestamp_ (Clock::now ())
{}


pcl::io::DepthImage::DepthImage (FrameWrapper::Ptr depth_metadata, float baseline, float focal_length, std::uint64_t shadow_value, std::uint64_t no_sample_value, Timestamp timestamp)
: wrapper_(std::move(depth_metadata))
, baseline_ (baseline)
, focal_length_ (focal_length)
, shadow_value_ (shadow_value)
, no_sample_value_ (no_sample_value)
, timestamp_(timestamp)
{}


pcl::io::DepthImage::~DepthImage () = default;

const unsigned short*
pcl::io::DepthImage::getData ()
{
  return static_cast<const unsigned short*> (wrapper_->getData ());
}


int
pcl::io::DepthImage::getDataSize () const
{
  return (wrapper_->getDataSize ());
}


const FrameWrapper::Ptr
pcl::io::DepthImage::getMetaData () const
{
  return (wrapper_);
}


float
pcl::io::DepthImage::getBaseline () const
{
  return (baseline_);
}


float
pcl::io::DepthImage::getFocalLength () const
{
  return (focal_length_);
}


std::uint64_t
pcl::io::DepthImage::getShadowValue () const
{
  return (shadow_value_);
}


std::uint64_t
pcl::io::DepthImage::getNoSampleValue () const
{
  return (no_sample_value_);
}


unsigned
pcl::io::DepthImage::getWidth () const
{
  return (wrapper_->getWidth ());
}


unsigned
pcl::io::DepthImage::getHeight () const
{
  return (wrapper_->getHeight ());
}


unsigned
pcl::io::DepthImage::getFrameID () const
{
  return (wrapper_->getFrameID ());
}


std::uint64_t
pcl::io::DepthImage::getTimestamp () const
{
  return (wrapper_->getTimestamp ());
}


pcl::io::DepthImage::Timestamp
pcl::io::DepthImage::getSystemTimestamp () const
{
  return (timestamp_);
}

// Fill external buffers ////////////////////////////////////////////////////

void
pcl::io::DepthImage::fillDepthImageRaw (unsigned width, unsigned height, unsigned short* depth_buffer, unsigned line_step) const
{
  if (width > wrapper_->getWidth () || height > wrapper_->getHeight ())
    THROW_IO_EXCEPTION ("upsampling not supported: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  if (wrapper_->getWidth () % width != 0 || wrapper_->getHeight () % height != 0)
    THROW_IO_EXCEPTION ("downsampling only supported for integer scale: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  if (line_step == 0)
    line_step = width * static_cast<unsigned> (sizeof (unsigned short));

  // special case no sclaing, no padding => memcopy!
  if (width == wrapper_->getWidth () && height == wrapper_->getHeight () && (line_step == width * sizeof (unsigned short)))
  {
    memcpy (depth_buffer, wrapper_->getData (), wrapper_->getDataSize ());
    return;
  }

  // padding skip for destination image
  unsigned bufferSkip = line_step - width * static_cast<unsigned> (sizeof (unsigned short));

  // step and padding skip for source image
  unsigned xStep = wrapper_->getWidth () / width;
  unsigned ySkip = (wrapper_->getHeight () / height - 1) * wrapper_->getWidth ();

  // Fill in the depth image data, converting mm to m
  short bad_point = std::numeric_limits<short>::quiet_NaN ();
  unsigned depthIdx = 0;

  const auto* inputBuffer = static_cast<const unsigned short*> (wrapper_->getData ());

  for (unsigned yIdx = 0; yIdx < height; ++yIdx, depthIdx += ySkip)
  {
    for (unsigned xIdx = 0; xIdx < width; ++xIdx, depthIdx += xStep, ++depth_buffer)
    {
      /// @todo Different values for these cases
      unsigned short pixel = inputBuffer[depthIdx];
      if (pixel == 0 || pixel == no_sample_value_ || pixel == shadow_value_)
        *depth_buffer = bad_point;
      else
      {
        *depth_buffer = static_cast<unsigned short>( pixel );
      }
    }
    // if we have padding
    if (bufferSkip > 0)
    {
      char* cBuffer = reinterpret_cast<char*> (depth_buffer);
      depth_buffer = reinterpret_cast<unsigned short*> (cBuffer + bufferSkip);
    }
  }
}

void
pcl::io::DepthImage::fillDepthImage (unsigned width, unsigned height, float* depth_buffer, unsigned line_step) const
{
  if (width > wrapper_->getWidth () || height > wrapper_->getHeight ())
    THROW_IO_EXCEPTION ("upsampling not supported: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  if (wrapper_->getWidth () % width != 0 || wrapper_->getHeight () % height != 0)
    THROW_IO_EXCEPTION ("downsampling only supported for integer scale: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  if (line_step == 0)
    line_step = width * static_cast<unsigned> (sizeof (float));

  const auto* inputBuffer = static_cast<const unsigned short*> (wrapper_->getData ());
#if defined(__RVV10__)
  if (canUseFloatImageRVV (wrapper_->getWidth (), wrapper_->getHeight (), width, height, line_step))
  {
    if (wrapper_->getWidth () == width && wrapper_->getHeight () == height)
    {
      fillDepthImageContiguousRVV (
          inputBuffer, no_sample_value_, shadow_value_, width, height, depth_buffer, line_step);
      return;
    }
  }
#endif
  fillDepthImageStd (inputBuffer,
                     wrapper_->getWidth (),
                     wrapper_->getHeight (),
                     no_sample_value_,
                     shadow_value_,
                     width,
                     height,
                     depth_buffer,
                     line_step);
}

void
pcl::io::DepthImage::fillDisparityImage (unsigned width, unsigned height, float* disparity_buffer, unsigned line_step) const
{
  if (width > wrapper_->getWidth () || height > wrapper_->getHeight ())
    THROW_IO_EXCEPTION ("upsampling not supported: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  if (wrapper_->getWidth () % width != 0 || wrapper_->getHeight () % height != 0)
    THROW_IO_EXCEPTION ("downsampling only supported for integer scale: %d x %d -> %d x %d", wrapper_->getWidth (), wrapper_->getHeight (), width, height);

  if (line_step == 0)
    line_step = width * static_cast<unsigned> (sizeof (float));

  const auto* inputBuffer = static_cast<const unsigned short*> (wrapper_->getData ());
#if defined(__RVV10__)
  if (canUseFloatImageRVV (wrapper_->getWidth (), wrapper_->getHeight (), width, height, line_step))
  {
    if (wrapper_->getWidth () == width && wrapper_->getHeight () == height)
    {
      const float constant = focal_length_ * baseline_ * 1000.0f;
      fillDisparityImageContiguousRVV (inputBuffer,
                                       no_sample_value_,
                                       shadow_value_,
                                       constant,
                                       width,
                                       height,
                                       disparity_buffer,
                                       line_step);
      return;
    }
    if (wrapper_->getWidth () / width > 1)
    {
      const unsigned xStep = wrapper_->getWidth () / width;
      const float constant = focal_length_ * baseline_ * 1000.0f / static_cast<float> (xStep);
      fillDisparityImageDownsampleRVV (inputBuffer,
                                       wrapper_->getWidth (),
                                       wrapper_->getHeight (),
                                       no_sample_value_,
                                       shadow_value_,
                                       constant,
                                       width,
                                       height,
                                       disparity_buffer,
                                       line_step);
      return;
    }
  }
#endif
  fillDisparityImageStd (inputBuffer,
                         wrapper_->getWidth (),
                         wrapper_->getHeight (),
                         no_sample_value_,
                         shadow_value_,
                         baseline_,
                         focal_length_,
                         width,
                         height,
                         disparity_buffer,
                         line_step);
}
