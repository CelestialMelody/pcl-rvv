/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2011-2012, Willow Garage, Inc.
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
 */

#include <pcl/pcl_config.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include <pcl/point_types.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace
{
  enum class ONIGrabberXYZPathHook
  {
    None = 0,
    Scalar = 1,
    Rvv = 2,
  };

#if defined(PCL_RVV_ONI_GRABBER_TEST_HOOK)
  int g_oni_grabber_last_hook = static_cast<int> (ONIGrabberXYZPathHook::None);

  extern "C" void
  pcl_rvv_oni_grabber_reset_test_hook ()
  {
    g_oni_grabber_last_hook = static_cast<int> (ONIGrabberXYZPathHook::None);
  }

  extern "C" int
  pcl_rvv_oni_grabber_last_test_hook ()
  {
    return g_oni_grabber_last_hook;
  }
#endif

  void
  recordONIGrabberXYZPath (ONIGrabberXYZPathHook path)
  {
#if defined(PCL_RVV_ONI_GRABBER_TEST_HOOK)
    g_oni_grabber_last_hook = static_cast<int> (path);
#else
    (void)path;
#endif
  }

  bool
  oniGrabberFitsU16Compare (std::uint64_t value)
  {
    return value <= std::numeric_limits<std::uint16_t>::max ();
  }

  void
  fillXYZPointCloudStd (const std::uint16_t* depth_map,
                        unsigned width,
                        unsigned height,
                        float constant,
                        int center_x,
                        int center_y,
                        std::uint64_t no_sample_value,
                        std::uint64_t shadow_value,
                        pcl::PointXYZ* points)
  {
    (void)width;
    (void)height;
    recordONIGrabberXYZPath (ONIGrabberXYZPathHook::Scalar);
    const float bad_point = std::numeric_limits<float>::quiet_NaN ();
    unsigned depth_idx = 0;
    for (int v = -center_y; v < center_y; ++v)
    {
      for (int u = -center_x; u < center_x; ++u, ++depth_idx)
      {
        pcl::PointXYZ& pt = points[depth_idx];
        const std::uint16_t pixel = depth_map[depth_idx];
        if (pixel == 0 || pixel == no_sample_value || pixel == shadow_value)
        {
          pt.x = pt.y = pt.z = bad_point;
          continue;
        }
        pt.z = pixel * 0.001f;
        pt.x = static_cast<float> (u) * pt.z * constant;
        pt.y = static_cast<float> (v) * pt.z * constant;
      }
    }
  }

#if defined(__RVV10__)
  vbool8_t
  makeInvalidDepthMask (vuint16m2_t pixels,
                        std::uint64_t no_sample_value,
                        std::uint64_t shadow_value,
                        std::size_t vl)
  {
    vbool8_t invalid = __riscv_vmseq_vx_u16m2_b8 (pixels, 0, vl);
    if (oniGrabberFitsU16Compare (no_sample_value))
      invalid = __riscv_vmor_mm_b8 (
          invalid,
          __riscv_vmseq_vx_u16m2_b8 (pixels, static_cast<std::uint16_t> (no_sample_value), vl),
          vl);
    if (oniGrabberFitsU16Compare (shadow_value))
      invalid = __riscv_vmor_mm_b8 (
          invalid,
          __riscv_vmseq_vx_u16m2_b8 (pixels, static_cast<std::uint16_t> (shadow_value), vl),
          vl);
    return invalid;
  }

  void
  fillXYZPointCloudRVV (const std::uint16_t* depth_map,
                        unsigned width,
                        unsigned height,
                        float constant,
                        int center_x,
                        int center_y,
                        std::uint64_t no_sample_value,
                        std::uint64_t shadow_value,
                        pcl::PointXYZ* points)
  {
    (void)height;
    recordONIGrabberXYZPath (ONIGrabberXYZPathHook::Rvv);
    const float bad_point = std::numeric_limits<float>::quiet_NaN ();
    auto* point_bytes = reinterpret_cast<unsigned char*> (points);
    const auto point_stride = static_cast<std::ptrdiff_t> (sizeof (pcl::PointXYZ));

    for (int v = -center_y; v < center_y; ++v)
    {
      const unsigned row = static_cast<unsigned> (v + center_y);
      const std::size_t row_offset = static_cast<std::size_t> (row) * width;
      for (unsigned u = 0; u < width;)
      {
        const std::size_t vl = __riscv_vsetvl_e16m2 (width - u);
        const vuint16m2_t pixels = __riscv_vle16_v_u16m2 (depth_map + row_offset + u, vl);
        const vbool8_t invalid = makeInvalidDepthMask (pixels, no_sample_value, shadow_value, vl);
        const vbool8_t valid = __riscv_vmnot_m_b8 (invalid, vl);
        const vuint32m4_t widened = __riscv_vwaddu_vx_u32m4 (pixels, 0, vl);
        const vfloat32m4_t z =
            __riscv_vfmul_vf_f32m4 (__riscv_vfcvt_f_xu_v_f32m4 (widened, vl), 0.001f, vl);

        vuint32m4_t lane_u = __riscv_vid_v_u32m4 (vl);
        lane_u = __riscv_vadd_vx_u32m4 (lane_u, u, vl);
        const vfloat32m4_t uf = __riscv_vfcvt_f_xu_v_f32m4 (lane_u, vl);
        const vfloat32m4_t x = __riscv_vfmul_vf_f32m4 (
            __riscv_vfmul_vv_f32m4 (
                __riscv_vfsub_vf_f32m4 (uf, static_cast<float> (center_x), vl), z, vl),
            constant,
            vl);
        const vfloat32m4_t y = __riscv_vfmul_vf_f32m4 (
            __riscv_vfmul_vv_f32m4 (__riscv_vfmv_v_f_f32m4 (static_cast<float> (v), vl), z, vl),
            constant,
            vl);
        const vfloat32m4_t bad = __riscv_vfmv_v_f_f32m4 (bad_point, vl);

        const std::size_t point_index = row_offset + u;
        auto* x_ptr = reinterpret_cast<float*> (
            point_bytes + point_index * sizeof (pcl::PointXYZ) + offsetof (pcl::PointXYZ, x));
        auto* y_ptr = reinterpret_cast<float*> (
            point_bytes + point_index * sizeof (pcl::PointXYZ) + offsetof (pcl::PointXYZ, y));
        auto* z_ptr = reinterpret_cast<float*> (
            point_bytes + point_index * sizeof (pcl::PointXYZ) + offsetof (pcl::PointXYZ, z));

        __riscv_vsse32_v_f32m4 (x_ptr, point_stride, bad, vl);
        __riscv_vsse32_v_f32m4 (y_ptr, point_stride, bad, vl);
        __riscv_vsse32_v_f32m4 (z_ptr, point_stride, bad, vl);
        __riscv_vsse32_v_f32m4_m (valid, x_ptr, point_stride, x, vl);
        __riscv_vsse32_v_f32m4_m (valid, y_ptr, point_stride, y, vl);
        __riscv_vsse32_v_f32m4_m (valid, z_ptr, point_stride, z, vl);
        u += static_cast<unsigned> (vl);
      }
    }
  }
#endif

  void
  fillXYZPointCloudCandidate (const std::uint16_t* depth_map,
                              unsigned width,
                              unsigned height,
                              float constant,
                              int center_x,
                              int center_y,
                              std::uint64_t no_sample_value,
                              std::uint64_t shadow_value,
                              pcl::PointXYZ* points)
  {
#if defined(__RVV10__)
    if (width == static_cast<unsigned> (center_x * 2) &&
        height == static_cast<unsigned> (center_y * 2))
    {
      fillXYZPointCloudRVV (depth_map,
                            width,
                            height,
                            constant,
                            center_x,
                            center_y,
                            no_sample_value,
                            shadow_value,
                            points);
      return;
    }
#endif
    fillXYZPointCloudStd (depth_map,
                          width,
                          height,
                          constant,
                          center_x,
                          center_y,
                          no_sample_value,
                          shadow_value,
                          points);
  }

#if defined(PCL_RVV_ONI_GRABBER_TEST_HOOK)
  extern "C" void
  pcl_rvv_oni_grabber_fill_xyz_std_test_hook (const std::uint16_t* depth,
                                              unsigned width,
                                              unsigned height,
                                              float constant,
                                              int center_x,
                                              int center_y,
                                              std::uint64_t no_sample_value,
                                              std::uint64_t shadow_value,
                                              pcl::PointXYZ* cloud)
  {
    fillXYZPointCloudStd (depth,
                          width,
                          height,
                          constant,
                          center_x,
                          center_y,
                          no_sample_value,
                          shadow_value,
                          cloud);
  }

  extern "C" void
  pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook (const std::uint16_t* depth,
                                                    unsigned width,
                                                    unsigned height,
                                                    float constant,
                                                    int center_x,
                                                    int center_y,
                                                    std::uint64_t no_sample_value,
                                                    std::uint64_t shadow_value,
                                                    pcl::PointXYZ* cloud)
  {
    fillXYZPointCloudCandidate (depth,
                                width,
                                height,
                                constant,
                                center_x,
                                center_y,
                                no_sample_value,
                                shadow_value,
                                cloud);
  }
#endif
}

#ifdef HAVE_OPENNI

#include <pcl/io/oni_grabber.h>
#include <pcl/point_cloud.h>
#include <pcl/common/time.h>
#include <pcl/console/print.h>
#include <boost/shared_array.hpp> // for boost::shared_array
#include <pcl/memory.h>  // for dynamic_pointer_cast
#include <pcl/exceptions.h>

namespace
{
  union RGBValue
  {
    struct /*anonymous*/
    {
      unsigned char Blue;
      unsigned char Green;
      unsigned char Red;
      unsigned char Alpha;
    };
    float float_value;
    long long_value;
  };
}

namespace pcl
{

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ONIGrabber::ONIGrabber (const std::string& file_name, bool repeat, bool stream)
  : rgb_frame_id_ ("/openni_rgb_optical_frame")
  , depth_frame_id_ ("/openni_depth_optical_frame")
{
  openni_wrapper::OpenNIDriver& driver = openni_wrapper::OpenNIDriver::getInstance ();
  device_ = dynamic_pointer_cast< openni_wrapper::DeviceONI> (driver.createVirtualDevice (file_name, repeat, stream));

  if (!device_->hasDepthStream ())
    PCL_THROW_EXCEPTION (pcl::IOException, "Device does not provide 3D information.");

  XnMapOutputMode depth_mode = device_->getDepthOutputMode();
  depth_width_ = depth_mode.nXRes;
  depth_height_ = depth_mode.nYRes;

  depth_image_signal_ = createSignal <sig_cb_openni_depth_image > ();
  point_cloud_signal_ = createSignal <sig_cb_openni_point_cloud > ();

  if (device_->hasIRStream ())
  {
    ir_image_signal_        = createSignal <sig_cb_openni_ir_image > ();
    point_cloud_i_signal_   = createSignal <sig_cb_openni_point_cloud_i > ();
    ir_depth_image_signal_  = createSignal <sig_cb_openni_ir_depth_image > ();
  }

  if (device_->hasImageStream ())
  {
    XnMapOutputMode depth_mode = device_->getImageOutputMode ();
    image_width_ = depth_mode.nXRes;
    image_height_ = depth_mode.nYRes;

    image_signal_             = createSignal <sig_cb_openni_image> ();
    image_depth_image_signal_ = createSignal <sig_cb_openni_image_depth_image> ();
    point_cloud_rgb_signal_   = createSignal <sig_cb_openni_point_cloud_rgb> ();
    point_cloud_rgba_signal_   = createSignal <sig_cb_openni_point_cloud_rgba> ();
    rgb_sync_.addCallback ([this] (const openni_wrapper::Image::Ptr& image,
                                   const openni_wrapper::DepthImage::Ptr& depth_image,
                                   unsigned long,
                                   unsigned long)
    {
      imageDepthImageCallback (image, depth_image);
    });
  }

  image_callback_handle = device_->registerImageCallback (&ONIGrabber::imageCallback, *this);
  depth_callback_handle = device_->registerDepthCallback (&ONIGrabber::depthCallback, *this);
  ir_callback_handle    = device_->registerIRCallback (&ONIGrabber::irCallback, *this);

  // if in trigger mode -> publish these topics
  if (!stream)
  {
    // check if we need to start/stop any stream
    if (device_->hasImageStream () && !device_->isImageStreamRunning ())
      device_->startImageStream ();

    if (device_->hasDepthStream () && !device_->isDepthStreamRunning ())
      device_->startDepthStream ();

    if (device_->hasIRStream () && !device_->isIRStreamRunning ())
      device_->startIRStream ();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ONIGrabber::~ONIGrabber() noexcept
{
  try
  {
    stop();
    // unregister callbacks
    device_->unregisterDepthCallback(depth_callback_handle);
    device_->unregisterImageCallback(image_callback_handle);
    device_->unregisterIRCallback(image_callback_handle);

    // disconnect all listeners
    disconnect_all_slots <sig_cb_openni_image> ();
    disconnect_all_slots <sig_cb_openni_depth_image> ();
    disconnect_all_slots <sig_cb_openni_ir_image> ();
    disconnect_all_slots <sig_cb_openni_image_depth_image> ();
    disconnect_all_slots <sig_cb_openni_point_cloud> ();
    disconnect_all_slots <sig_cb_openni_point_cloud_rgb> ();
    disconnect_all_slots <sig_cb_openni_point_cloud_rgba> ();
    disconnect_all_slots <sig_cb_openni_point_cloud_i > ();
  }
  catch (...)
  {
    // destructor never throws
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void 
ONIGrabber::start ()
{
  if (device_->isStreaming ())
  {
    try
    {
      // check if we need to start/stop any stream
      if (device_->hasImageStream() && !device_->isImageStreamRunning())
        device_->startImageStream();

      if (device_->hasDepthStream() && !device_->isDepthStreamRunning())
        device_->startDepthStream();

      if (device_->hasIRStream() && !device_->isIRStreamRunning())
        device_->startIRStream();

      running_ = true;
    }
    catch (openni_wrapper::OpenNIException& ex)
    {
      PCL_THROW_EXCEPTION (pcl::IOException, "Could not start streams. Reason: " << ex.what());
    }
  }
  else
  {
    if (device_->hasImageStream ())
      device_->trigger ();

    if (device_->hasDepthStream ())
      device_->trigger ();

    if (device_->hasIRStream ())
      device_->trigger ();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void 
ONIGrabber::stop ()
{
  if (device_->isStreaming ())
  {
    try
    {
      if (device_->hasDepthStream() && device_->isDepthStreamRunning())
        device_->stopDepthStream();

      if (device_->hasImageStream() && device_->isImageStreamRunning())
        device_->stopImageStream();

      if (device_->hasIRStream() && device_->isIRStreamRunning())
        device_->stopIRStream();

      running_ = false;
    }
    catch (openni_wrapper::OpenNIException& ex)
    {
      PCL_THROW_EXCEPTION (pcl::IOException, "Could not stop streams. Reason: " << ex.what());
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool 
ONIGrabber::isRunning() const
{
  return (running_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string 
ONIGrabber::getName () const
{
  return {"ONIGrabber"};
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float 
ONIGrabber::getFramesPerSecond () const
{
  if (device_->isStreaming())
    return (static_cast<float> (device_->getDepthOutputMode ().nFPS));
  return (0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void 
ONIGrabber::imageCallback(openni_wrapper::Image::Ptr image, void*)
{
  if (num_slots<sig_cb_openni_point_cloud_rgb> () > 0 ||
      num_slots<sig_cb_openni_point_cloud_rgba> () > 0 ||
      num_slots<sig_cb_openni_image_depth_image > () > 0)
    rgb_sync_.add0(image, image->getTimeStamp());

  if (image_signal_->num_slots() > 0)
    image_signal_->operator()(image);

  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void 
ONIGrabber::depthCallback(openni_wrapper::DepthImage::Ptr depth_image, void*)
{
  if (num_slots<sig_cb_openni_point_cloud_rgb> () > 0 ||
      num_slots<sig_cb_openni_point_cloud_rgba> () > 0 ||
      num_slots<sig_cb_openni_image_depth_image> () > 0)
    rgb_sync_.add1(depth_image, depth_image->getTimeStamp());

  if (num_slots<sig_cb_openni_point_cloud_i > () > 0 ||
      num_slots<sig_cb_openni_ir_depth_image > () > 0)
    ir_sync_.add1(depth_image, depth_image->getTimeStamp());

  if (depth_image_signal_->num_slots() > 0)
    depth_image_signal_->operator()(depth_image);

  if (point_cloud_signal_->num_slots() > 0)
    point_cloud_signal_->operator()(convertToXYZPointCloud(depth_image));

  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void 
ONIGrabber::irCallback(openni_wrapper::IRImage::Ptr ir_image, void*)
{
  if (num_slots<sig_cb_openni_point_cloud_i > () > 0 ||
      num_slots<sig_cb_openni_ir_depth_image > () > 0)
    ir_sync_.add0(ir_image, ir_image->getTimeStamp());

  if (ir_image_signal_->num_slots() > 0)
    ir_image_signal_->operator()(ir_image);

  return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void 
ONIGrabber::imageDepthImageCallback(const openni_wrapper::Image::Ptr &image, const openni_wrapper::DepthImage::Ptr &depth_image)
{
  // check if we have color point cloud slots
  if (point_cloud_rgb_signal_->num_slots () > 0)
  {
    PCL_WARN ("PointXYZRGB callbacks deprecated. Use PointXYZRGBA instead.\n");
    point_cloud_rgb_signal_->operator() (convertToXYZRGBPointCloud (image, depth_image));
  }

  if (point_cloud_rgba_signal_->num_slots () > 0)
    point_cloud_rgba_signal_->operator() (convertToXYZRGBAPointCloud (image, depth_image));

  if (image_depth_image_signal_->num_slots() > 0)
  {
    float constant = 1.0f / device_->getDepthFocalLength(depth_width_);
    image_depth_image_signal_->operator()(image, depth_image, constant);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void 
ONIGrabber::irDepthImageCallback(const openni_wrapper::IRImage::Ptr &ir_image, const openni_wrapper::DepthImage::Ptr &depth_image)
{
  // check if we have color point cloud slots
  if (point_cloud_i_signal_->num_slots() > 0)
    point_cloud_i_signal_->operator()(convertToXYZIPointCloud(ir_image, depth_image));

  if (ir_depth_image_signal_->num_slots() > 0)
  {
    float constant = 1.0f / device_->getDepthFocalLength(depth_width_);
    ir_depth_image_signal_->operator()(ir_image, depth_image, constant);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
pcl::PointCloud<pcl::PointXYZ>::Ptr 
ONIGrabber::convertToXYZPointCloud(const openni_wrapper::DepthImage::Ptr& depth_image) const
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud <pcl::PointXYZ>);

  // TODO cloud->header.stamp = time;
  cloud->height = depth_height_;
  cloud->width = depth_width_;
  cloud->is_dense = false;

  cloud->points.resize (cloud->height * cloud->width);

  float constant = 1.0f / device_->getDepthFocalLength (depth_width_);

  if (device_->isDepthRegistered ())
    cloud->header.frame_id = rgb_frame_id_;
  else
    cloud->header.frame_id = depth_frame_id_;

  int centerX = (cloud->width >> 1);
  int centerY = (cloud->height >> 1);

  // we have to use Data, since operator[] uses assert -> Debug-mode very slow!
  const unsigned short* depth_map = depth_image->getDepthMetaData ().Data ();
  if (depth_image->getWidth () != depth_width_ || depth_image->getHeight () != depth_height_)
  {
    static unsigned buffer_size = 0;
    static boost::shared_array<unsigned short> depth_buffer (nullptr);

    if (buffer_size < depth_width_ * depth_height_)
    {
      buffer_size = depth_width_ * depth_height_;
      depth_buffer.reset (new unsigned short [buffer_size]);
    }
    depth_image->fillDepthImageRaw (depth_width_, depth_height_, depth_buffer.get ());
    depth_map = depth_buffer.get ();
  }

  fillXYZPointCloudCandidate (depth_map,
                              cloud->width,
                              cloud->height,
                              constant,
                              centerX,
                              centerY,
                              depth_image->getNoSampleValue (),
                              depth_image->getShadowValue (),
                              cloud->points.data ());
  return (cloud);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
pcl::PointCloud<pcl::PointXYZRGB>::Ptr ONIGrabber::convertToXYZRGBPointCloud (
    const openni_wrapper::Image::Ptr &image,
    const openni_wrapper::DepthImage::Ptr &depth_image) const
{
  static unsigned rgb_array_size = 0;
  static boost::shared_array<unsigned char> rgb_array(nullptr);
  static unsigned char* rgb_buffer = nullptr;

  pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);

  cloud->header.frame_id = rgb_frame_id_;
  cloud->height = depth_height_;
  cloud->width = depth_width_;
  cloud->is_dense = false;

  cloud->points.resize(cloud->height * cloud->width);

  float constant = 1.0f / device_->getImageFocalLength(cloud->width);
  int centerX = (cloud->width >> 1);
  int centerY = (cloud->height >> 1);

  const XnDepthPixel* depth_map = depth_image->getDepthMetaData().Data();
  if (depth_image->getWidth() != depth_width_ || depth_image->getHeight() != depth_height_)
  {
    static unsigned buffer_size = 0;
    static boost::shared_array<unsigned short> depth_buffer(nullptr);

    if (buffer_size < depth_width_ * depth_height_)
    {
      buffer_size = depth_width_ * depth_height_;
      depth_buffer.reset (new unsigned short [buffer_size]);
    }

    depth_image->fillDepthImageRaw (depth_width_, depth_height_, depth_buffer.get());
    depth_map = depth_buffer.get ();
  }

  // here we need exact the size of the point cloud for a one-one correspondence!
  if (rgb_array_size < image_width_ * image_height_ * 3)
  {
    rgb_array_size = image_width_ * image_height_ * 3;
    rgb_array.reset(new unsigned char [rgb_array_size]);
    rgb_buffer = rgb_array.get();
  }
  image->fillRGB(image_width_, image_height_, rgb_buffer, image_width_ * 3);

  // depth_image already has the desired dimensions, but rgb_msg may be higher res.
  int color_idx = 0, depth_idx = 0;
  RGBValue color;
  color.Alpha = 0;

  float bad_point = std::numeric_limits<float>::quiet_NaN();

  for (int v = -centerY; v < centerY; ++v)
  {
    for (int u = -centerX; u < centerX; ++u, color_idx += 3, ++depth_idx)
    {
      pcl::PointXYZRGB& pt = (*cloud)[depth_idx];
      /// @todo Different values for these cases
      // Check for invalid measurements
      if (depth_map[depth_idx] == 0 ||
          depth_map[depth_idx] == depth_image->getNoSampleValue() ||
          depth_map[depth_idx] == depth_image->getShadowValue())
      {
        pt.x = pt.y = pt.z = bad_point;
      }
      else
      {
        pt.z = depth_map[depth_idx] * 0.001f;
        pt.x = static_cast<float> (u) * pt.z * constant;
        pt.y = static_cast<float> (v) * pt.z * constant;
      }

      // Fill in color
      color.Red = rgb_buffer[color_idx];
      color.Green = rgb_buffer[color_idx + 1];
      color.Blue = rgb_buffer[color_idx + 2];
      pt.rgb = color.float_value;
    }
  }
  return (cloud);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
pcl::PointCloud<pcl::PointXYZRGBA>::Ptr ONIGrabber::convertToXYZRGBAPointCloud (
    const openni_wrapper::Image::Ptr &image,
    const openni_wrapper::DepthImage::Ptr &depth_image) const
{
  static unsigned rgb_array_size = 0;
  static boost::shared_array<unsigned char> rgb_array(nullptr);
  static unsigned char* rgb_buffer = nullptr;

  pcl::PointCloud<pcl::PointXYZRGBA>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZRGBA>);

  cloud->header.frame_id = rgb_frame_id_;
  cloud->height = depth_height_;
  cloud->width = depth_width_;
  cloud->is_dense = false;

  cloud->points.resize(cloud->height * cloud->width);

  float constant = 1.0f / device_->getImageFocalLength(cloud->width);
  int centerX = (cloud->width >> 1);
  int centerY = (cloud->height >> 1);

  const XnDepthPixel* depth_map = depth_image->getDepthMetaData().Data();
  if (depth_image->getWidth() != depth_width_ || depth_image->getHeight() != depth_height_)
  {
    static unsigned buffer_size = 0;
    static boost::shared_array<unsigned short> depth_buffer(nullptr);

    if (buffer_size < depth_width_ * depth_height_)
    {
      buffer_size = depth_width_ * depth_height_;
      depth_buffer.reset(new unsigned short [buffer_size]);
    }

    depth_image->fillDepthImageRaw(depth_width_, depth_height_, depth_buffer.get());
    depth_map = depth_buffer.get();
  }

  // here we need exact the size of the point cloud for a one-one correspondence!
  if (rgb_array_size < image_width_ * image_height_ * 3)
  {
    rgb_array_size = image_width_ * image_height_ * 3;
    rgb_array.reset (new unsigned char [rgb_array_size]);
    rgb_buffer = rgb_array.get();
  }
  image->fillRGB(image_width_, image_height_, rgb_buffer, image_width_ * 3);

  // depth_image already has the desired dimensions, but rgb_msg may be higher res.
  int color_idx = 0, depth_idx = 0;
  RGBValue color;
  color.Alpha = 0;

  float bad_point = std::numeric_limits<float>::quiet_NaN();

  for (int v = -centerY; v < centerY; ++v)
  {
    for (int u = -centerX; u < centerX; ++u, color_idx += 3, ++depth_idx)
    {
      pcl::PointXYZRGBA& pt = (*cloud)[depth_idx];
      /// @todo Different values for these cases
      // Check for invalid measurements
      if (depth_map[depth_idx] == 0 ||
          depth_map[depth_idx] == depth_image->getNoSampleValue() ||
          depth_map[depth_idx] == depth_image->getShadowValue())
      {
        pt.x = pt.y = pt.z = bad_point;
      }
      else
      {
        pt.z = depth_map[depth_idx] * 0.001f;
        pt.x = static_cast<float> (u) * pt.z * constant;
        pt.y = static_cast<float> (v) * pt.z * constant;
      }

      // Fill in color
      color.Red = rgb_buffer[color_idx];
      color.Green = rgb_buffer[color_idx + 1];
      color.Blue = rgb_buffer[color_idx + 2];
      pt.rgba = static_cast<std::uint32_t> (color.long_value);
    }
  }
  return (cloud);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
pcl::PointCloud<pcl::PointXYZI>::Ptr ONIGrabber::convertToXYZIPointCloud(const openni_wrapper::IRImage::Ptr &ir_image,
  const openni_wrapper::DepthImage::Ptr &depth_image) const
{
  pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>);

  cloud->header.frame_id = rgb_frame_id_;
  cloud->height = depth_height_;
  cloud->width = depth_width_;
  cloud->is_dense = false;

  cloud->points.resize(cloud->height * cloud->width);

  float constant = 1.0f / device_->getImageFocalLength(cloud->width);
  int centerX = (cloud->width >> 1);
  int centerY = (cloud->height >> 1);

  const XnDepthPixel* depth_map = depth_image->getDepthMetaData().Data();
  const XnIRPixel* ir_map = ir_image->getMetaData().Data();

  if (depth_image->getWidth() != depth_width_ || depth_image->getHeight() != depth_height_)
  {
    static unsigned buffer_size = 0;
    static boost::shared_array<unsigned short> depth_buffer(nullptr);
    static boost::shared_array<unsigned short> ir_buffer(nullptr);

    if (buffer_size < depth_width_ * depth_height_)
    {
      buffer_size = depth_width_ * depth_height_;
      depth_buffer.reset(new unsigned short [buffer_size]);
      ir_buffer.reset(new unsigned short [buffer_size]);
    }

    depth_image->fillDepthImageRaw(depth_width_, depth_height_, depth_buffer.get());
    depth_map = depth_buffer.get();

    ir_image->fillRaw(depth_width_, depth_height_, ir_buffer.get());
    ir_map = ir_buffer.get ();
  }

  int depth_idx = 0;
  float bad_point = std::numeric_limits<float>::quiet_NaN();

  for (int v = -centerY; v < centerY; ++v)
  {
    for (int u = -centerX; u < centerX; ++u, ++depth_idx)
    {
      pcl::PointXYZI& pt = (*cloud)[depth_idx];
      /// @todo Different values for these cases
      // Check for invalid measurements
      if (depth_map[depth_idx] == 0 ||
          depth_map[depth_idx] == depth_image->getNoSampleValue() ||
          depth_map[depth_idx] == depth_image->getShadowValue())
      {
        pt.x = pt.y = pt.z = bad_point;
      }
      else
      {
        pt.z = depth_map[depth_idx] * 0.001f;
        pt.x = static_cast<float> (u) * pt.z * constant;
        pt.y = static_cast<float> (v) * pt.z * constant;
      }

      pt.data_c[0] = pt.data_c[1] = pt.data_c[2] = pt.data_c[3] = 0;
      pt.intensity = static_cast<float> (ir_map[depth_idx]);
    }
  }
  return (cloud);
}

}
#endif // HAVE_OPENNI
