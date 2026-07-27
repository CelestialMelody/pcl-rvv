/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2012-, Open Perception, Inc.
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
 *   * Neither the name of the copyright holder(s) nor the names of its
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

#pragma once

#include <pcl/2d/convolution.h>
#include <pcl/2d/edge.h>
#include <pcl/common/angles.h> // for rad2deg
#if defined(__RVV10__)
#include <pcl/common/common.h>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>
#include <riscv_vector.h>
#include <cstddef>
#include <cstdint>
#endif

namespace pcl {

#if defined(__RVV10__)
template <typename PointOutT>
inline constexpr bool kEdgeMagnitudeDirectionFieldsCompatible =
    pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZI, pcl::fields::intensity>::value &&
    pcl::rvv::RVVFloatFieldLayout<PointOutT, pcl::fields::magnitude_x>::value &&
    pcl::rvv::RVVFloatFieldLayout<PointOutT, pcl::fields::magnitude_y>::value &&
    pcl::rvv::RVVFloatFieldLayout<PointOutT, pcl::fields::magnitude>::value &&
    pcl::rvv::RVVFloatFieldLayout<PointOutT, pcl::fields::direction>::value;

template <typename PointOutT>
inline constexpr bool kEdgeDirectionFieldCompatible =
    pcl::rvv::RVVFloatFieldLayout<PointOutT, pcl::fields::direction>::value;

template <typename PointOutT>
static void
computeMagnitudeDirectionRVV(const pcl::PointCloud<pcl::PointXYZI>& magnitude_x,
                             const pcl::PointCloud<pcl::PointXYZI>& magnitude_y,
                             pcl::PointCloud<PointOutT>& output,
                             std::size_t n)
{
  static_assert(kEdgeMagnitudeDirectionFieldsCompatible<PointOutT>,
                "Edge RVV path requires registered single-float intensity and edge output fields.");
  const std::uint8_t* base_mx =
      reinterpret_cast<const std::uint8_t*>(magnitude_x.points.data());
  const std::uint8_t* base_my =
      reinterpret_cast<const std::uint8_t*>(magnitude_y.points.data());
  std::uint8_t* base_out = reinterpret_cast<std::uint8_t*>(output.points.data());

  std::size_t j0 = 0;
  while (j0 < n) {
    std::size_t vl = __riscv_vsetvl_e32m2(n - j0);
    const std::uint8_t* chunk_mx = base_mx + j0 * sizeof(pcl::PointXYZI);
    const std::uint8_t* chunk_my = base_my + j0 * sizeof(pcl::PointXYZI);
    std::uint8_t* chunk_out = base_out + j0 * sizeof(PointOutT);

    vfloat32m2_t v_mx =
        pcl::rvv_load::strided_load_field_f32m2<pcl::PointXYZI, pcl::fields::intensity>(
            chunk_mx, vl);
    vfloat32m2_t v_my =
        pcl::rvv_load::strided_load_field_f32m2<pcl::PointXYZI, pcl::fields::intensity>(
            chunk_my, vl);

    vfloat32m2_t v_mag =
        __riscv_vfsqrt_v_f32m2(__riscv_vfadd_vv_f32m2(
            __riscv_vfmul_vv_f32m2(v_mx, v_mx, vl),
            __riscv_vfmul_vv_f32m2(v_my, v_my, vl), vl), vl);
    vfloat32m2_t v_dir = pcl::atan2_RVV_f32m2(v_my, v_mx, vl);

    pcl::rvv_store::strided_store_field_f32m2<PointOutT, pcl::fields::magnitude_x>(
        chunk_out, v_mx, vl);
    pcl::rvv_store::strided_store_field_f32m2<PointOutT, pcl::fields::magnitude_y>(
        chunk_out, v_my, vl);
    pcl::rvv_store::strided_store_field_f32m2<PointOutT, pcl::fields::magnitude>(
        chunk_out, v_mag, vl);
    pcl::rvv_store::strided_store_field_f32m2<PointOutT, pcl::fields::direction>(
        chunk_out, v_dir, vl);

    j0 += vl;
  }
}
#endif

template <typename PointOutT>
static void
computeMagnitudeDirectionStd(const pcl::PointCloud<pcl::PointXYZI>& magnitude_x,
                             const pcl::PointCloud<pcl::PointXYZI>& magnitude_y,
                             pcl::PointCloud<PointOutT>& output,
                             std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    const float mx = magnitude_x[i].intensity;
    const float my = magnitude_y[i].intensity;
    output[i].magnitude_x = mx;
    output[i].magnitude_y = my;
    output[i].magnitude = std::sqrt(mx * mx + my * my);
    output[i].direction = std::atan2(my, mx);
  }
}

#if defined(__RVV10__)
template <typename PointOutT>
static void
discretizeAnglesRVV(pcl::PointCloud<PointOutT>& thet, int height, int width)
{
  static_assert(kEdgeDirectionFieldCompatible<PointOutT>,
                "Edge angle discretization RVV path requires a registered single-float direction field.");
  const int n = height * width;
  const float rad2deg = 180.0f / 3.14159265358979323846f;
  std::uint8_t* base = reinterpret_cast<std::uint8_t*>(thet.points.data());

  std::size_t j0 = 0;
  while (j0 < static_cast<std::size_t>(n)) {
    std::size_t vl = __riscv_vsetvl_e32m2(static_cast<std::size_t>(n) - j0);
    std::uint8_t* chunk = base + j0 * sizeof(PointOutT);
    vfloat32m2_t v_rad =
        pcl::rvv_load::strided_load_field_f32m2<PointOutT, pcl::fields::direction>(
            chunk, vl);
    vfloat32m2_t v_deg =
        __riscv_vfmul_vf_f32m2(v_rad, rad2deg, vl);

    // Fold negative degrees into [0, 180]; each bin is one interval (see discretizeAnglesStd).
    vbool16_t m_neg = __riscv_vmflt_vf_f32m2_b16(v_deg, 0.0f, vl);
    vfloat32m2_t v_deg_fold =
        __riscv_vfadd_vf_f32m2(v_deg, 180.0f, vl);
    v_deg = __riscv_vmerge_vvm_f32m2(v_deg, v_deg_fold, m_neg, vl);

    const vfloat32m2_t v_0 = __riscv_vfmv_v_f_f32m2(0.0f, vl);
    const vfloat32m2_t v_45 = __riscv_vfmv_v_f_f32m2(45.0f, vl);
    const vfloat32m2_t v_90 = __riscv_vfmv_v_f_f32m2(90.0f, vl);
    const vfloat32m2_t v_135 = __riscv_vfmv_v_f_f32m2(135.0f, vl);

    vbool16_t m45 = __riscv_vmfgt_vf_f32m2_b16(v_deg, 22.5f, vl);
    m45 = __riscv_vmand_mm_b16(m45, __riscv_vmflt_vf_f32m2_b16(v_deg, 67.5f, vl), vl);

    vbool16_t m90 = __riscv_vmfge_vf_f32m2_b16(v_deg, 67.5f, vl);
    m90 = __riscv_vmand_mm_b16(m90, __riscv_vmfle_vf_f32m2_b16(v_deg, 112.5f, vl), vl);

    vbool16_t m135 = __riscv_vmfgt_vf_f32m2_b16(v_deg, 112.5f, vl);
    m135 = __riscv_vmand_mm_b16(m135, __riscv_vmflt_vf_f32m2_b16(v_deg, 157.5f, vl), vl);

    // vmerge(op1, op2, mask) => mask ? op2 : op1; remainder is 0°
    vfloat32m2_t result = __riscv_vmerge_vvm_f32m2(v_0, v_45, m45, vl);
    result = __riscv_vmerge_vvm_f32m2(result, v_90, m90, vl);
    result = __riscv_vmerge_vvm_f32m2(result, v_135, m135, vl);
    pcl::rvv_store::strided_store_field_f32m2<PointOutT, pcl::fields::direction>(
        chunk, result, vl);
    j0 += vl;
  }
}
#endif

template <typename PointOutT>
static void
discretizeAnglesStd(pcl::PointCloud<PointOutT>& thet, int height, int width)
{
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      const float angle = pcl::rad2deg(thet(j, i).direction);
      if (((angle <= 22.5f) && (angle >= -22.5f)) || (angle >= 157.5f) ||
          (angle <= -157.5f))
        thet(j, i).direction = 0;
      else if (((angle > 22.5f) && (angle < 67.5f)) ||
               ((angle < -112.5f) && (angle > -157.5f)))
        thet(j, i).direction = 45;
      else if (((angle >= 67.5f) && (angle <= 112.5f)) ||
               ((angle <= -67.5f) && (angle >= -112.5f)))
        thet(j, i).direction = 90;
      else if (((angle > 112.5f) && (angle < 157.5f)) ||
               ((angle < -22.5f) && (angle > -67.5f)))
        thet(j, i).direction = 135;
    }
  }
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::detectEdgeSobel(pcl::PointCloud<PointOutT>& output)
{
  convolution_.setInputCloud(input_);
  pcl::PointCloud<PointXYZI>::Ptr kernel_x(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_x(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::SOBEL_X);
  kernel_.fetchKernel(*kernel_x);
  convolution_.setKernel(*kernel_x);
  convolution_.filter(*magnitude_x);

  pcl::PointCloud<PointXYZI>::Ptr kernel_y(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_y(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::SOBEL_Y);
  kernel_.fetchKernel(*kernel_y);
  convolution_.setKernel(*kernel_y);
  convolution_.filter(*magnitude_y);

  const int height = input_->height;
  const int width = input_->width;
  const std::size_t n = static_cast<std::size_t>(height) * static_cast<std::size_t>(width);

  output.resize(n);
  output.height = height;
  output.width = width;

#if defined(__RVV10__)
  if constexpr (kEdgeMagnitudeDirectionFieldsCompatible<PointOutT>)
    computeMagnitudeDirectionRVV(*magnitude_x, *magnitude_y, output, n);
  else
    computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#else
  computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#endif
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::sobelMagnitudeDirection(
    const pcl::PointCloud<PointInT>& input_x,
    const pcl::PointCloud<PointInT>& input_y,
    pcl::PointCloud<PointOutT>& output)
{
  convolution_.setInputCloud(input_x.makeShared());
  pcl::PointCloud<PointXYZI>::Ptr kernel_x(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_x(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::SOBEL_X);
  kernel_.fetchKernel(*kernel_x);
  convolution_.setKernel(*kernel_x);
  convolution_.filter(*magnitude_x);

  convolution_.setInputCloud(input_y.makeShared());
  pcl::PointCloud<PointXYZI>::Ptr kernel_y(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_y(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::SOBEL_Y);
  kernel_.fetchKernel(*kernel_y);
  convolution_.setKernel(*kernel_y);
  convolution_.filter(*magnitude_y);

  const int height = input_x.height;
  const int width = input_x.width;
  const std::size_t n = static_cast<std::size_t>(height) * static_cast<std::size_t>(width);

  output.resize(n);
  output.height = height;
  output.width = width;

#if defined(__RVV10__)
  if constexpr (kEdgeMagnitudeDirectionFieldsCompatible<PointOutT>)
    computeMagnitudeDirectionRVV(*magnitude_x, *magnitude_y, output, n);
  else
    computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#else
  computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#endif
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::detectEdgePrewitt(pcl::PointCloud<PointOutT>& output)
{
  convolution_.setInputCloud(input_);

  pcl::PointCloud<PointXYZI>::Ptr kernel_x(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_x(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::PREWITT_X);
  kernel_.fetchKernel(*kernel_x);
  convolution_.setKernel(*kernel_x);
  convolution_.filter(*magnitude_x);

  pcl::PointCloud<PointXYZI>::Ptr kernel_y(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_y(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::PREWITT_Y);
  kernel_.fetchKernel(*kernel_y);
  convolution_.setKernel(*kernel_y);
  convolution_.filter(*magnitude_y);

  const int height = input_->height;
  const int width = input_->width;
  const std::size_t n = static_cast<std::size_t>(height) * static_cast<std::size_t>(width);

  output.resize(n);
  output.height = height;
  output.width = width;

#if defined(__RVV10__)
  if constexpr (kEdgeMagnitudeDirectionFieldsCompatible<PointOutT>)
    computeMagnitudeDirectionRVV(*magnitude_x, *magnitude_y, output, n);
  else
    computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#else
  computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#endif
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::detectEdgeRoberts(pcl::PointCloud<PointOutT>& output)
{
  convolution_.setInputCloud(input_);

  pcl::PointCloud<PointXYZI>::Ptr kernel_x(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_x(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::ROBERTS_X);
  kernel_.fetchKernel(*kernel_x);
  convolution_.setKernel(*kernel_x);
  convolution_.filter(*magnitude_x);

  pcl::PointCloud<PointXYZI>::Ptr kernel_y(new pcl::PointCloud<PointXYZI>);
  pcl::PointCloud<PointXYZI>::Ptr magnitude_y(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::ROBERTS_Y);
  kernel_.fetchKernel(*kernel_y);
  convolution_.setKernel(*kernel_y);
  convolution_.filter(*magnitude_y);

  const int height = input_->height;
  const int width = input_->width;
  const std::size_t n = static_cast<std::size_t>(height) * static_cast<std::size_t>(width);

  output.resize(n);
  output.height = height;
  output.width = width;

#if defined(__RVV10__)
  if constexpr (kEdgeMagnitudeDirectionFieldsCompatible<PointOutT>)
    computeMagnitudeDirectionRVV(*magnitude_x, *magnitude_y, output, n);
  else
    computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#else
  computeMagnitudeDirectionStd(*magnitude_x, *magnitude_y, output, n);
#endif
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::cannyTraceEdge(
    int rowOffset, int colOffset, int row, int col, pcl::PointCloud<PointXYZI>& maxima)
{
  int newRow = row + rowOffset;
  int newCol = col + colOffset;
  PointXYZI& pt = maxima(newCol, newRow);

  if (newRow > 0 && newRow < static_cast<int>(maxima.height) && newCol > 0 &&
      newCol < static_cast<int>(maxima.width)) {
    if (pt.intensity == 0.0f || pt.intensity == std::numeric_limits<float>::max())
      return;

    pt.intensity = std::numeric_limits<float>::max();
    cannyTraceEdge(1, 0, newRow, newCol, maxima);
    cannyTraceEdge(-1, 0, newRow, newCol, maxima);
    cannyTraceEdge(1, 1, newRow, newCol, maxima);
    cannyTraceEdge(-1, -1, newRow, newCol, maxima);
    cannyTraceEdge(0, -1, newRow, newCol, maxima);
    cannyTraceEdge(0, 1, newRow, newCol, maxima);
    cannyTraceEdge(-1, 1, newRow, newCol, maxima);
    cannyTraceEdge(1, -1, newRow, newCol, maxima);
  }
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::discretizeAngles(pcl::PointCloud<PointOutT>& thet)
{
  const int height = thet.height;
  const int width = thet.width;

#if defined(__RVV10__)
  if constexpr (kEdgeDirectionFieldCompatible<PointOutT>)
    discretizeAnglesRVV(thet, height, width);
  else
    discretizeAnglesStd(thet, height, width);
#else
  discretizeAnglesStd(thet, height, width);
#endif
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::suppressNonMaxima(
    const pcl::PointCloud<PointXYZIEdge>& edges,
    pcl::PointCloud<PointXYZI>& maxima,
    float tLow)
{
  const int height = edges.height;
  const int width = edges.width;

  maxima.resize(edges.width, edges.height);

  for (auto& point : maxima)
    point.intensity = 0.0f;

  // tHigh and non-maximal suppression
  for (int i = 1; i < height - 1; i++) {
    for (int j = 1; j < width - 1; j++) {
      const PointXYZIEdge& ptedge = edges(j, i);
      PointXYZI& ptmax = maxima(j, i);

      if (ptedge.magnitude < tLow)
        continue;

      // maxima (j, i).intensity = 0;

      switch (static_cast<int>(ptedge.direction)) {
      case 0: {
        if (ptedge.magnitude >= edges(j - 1, i).magnitude &&
            ptedge.magnitude >= edges(j + 1, i).magnitude)
          ptmax.intensity = ptedge.magnitude;
        break;
      }
      case 45: {
        if (ptedge.magnitude >= edges(j - 1, i - 1).magnitude &&
            ptedge.magnitude >= edges(j + 1, i + 1).magnitude)
          ptmax.intensity = ptedge.magnitude;
        break;
      }
      case 90: {
        if (ptedge.magnitude >= edges(j, i - 1).magnitude &&
            ptedge.magnitude >= edges(j, i + 1).magnitude)
          ptmax.intensity = ptedge.magnitude;
        break;
      }
      case 135: {
        if (ptedge.magnitude >= edges(j + 1, i - 1).magnitude &&
            ptedge.magnitude >= edges(j - 1, i + 1).magnitude)
          ptmax.intensity = ptedge.magnitude;
        break;
      }
      }
    }
  }
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::detectEdgeCanny(pcl::PointCloud<PointOutT>& output)
{
  float tHigh = hysteresis_threshold_high_;
  float tLow = hysteresis_threshold_low_;
  const int height = input_->height;
  const int width = input_->width;

  output.resize(height * width);
  output.height = height;
  output.width = width;

  // Noise reduction using gaussian blurring
  pcl::PointCloud<PointXYZI>::Ptr gaussian_kernel(new pcl::PointCloud<PointXYZI>);
  PointCloudInPtr smoothed_cloud(new PointCloudIn);
  kernel_.setKernelSize(3);
  kernel_.setKernelSigma(1.0);
  kernel_.setKernelType(kernel<PointXYZI>::GAUSSIAN);
  kernel_.fetchKernel(*gaussian_kernel);
  convolution_.setKernel(*gaussian_kernel);
  convolution_.setInputCloud(input_);
  convolution_.filter(*smoothed_cloud);

  // Edge detection using Sobel
  pcl::PointCloud<PointXYZIEdge>::Ptr edges(new pcl::PointCloud<PointXYZIEdge>);
  setInputCloud(smoothed_cloud);
  detectEdgeSobel(*edges);

  // Edge discretization
  discretizeAngles(*edges);

  // tHigh and non-maximal suppression
  pcl::PointCloud<PointXYZI>::Ptr maxima(new pcl::PointCloud<PointXYZI>);
  suppressNonMaxima(*edges, *maxima, tLow);

  // Edge tracing
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      if ((*maxima)(j, i).intensity < tHigh ||
          (*maxima)(j, i).intensity == std::numeric_limits<float>::max())
        continue;

      (*maxima)(j, i).intensity = std::numeric_limits<float>::max();
      cannyTraceEdge(1, 0, i, j, *maxima);
      cannyTraceEdge(-1, 0, i, j, *maxima);
      cannyTraceEdge(1, 1, i, j, *maxima);
      cannyTraceEdge(-1, -1, i, j, *maxima);
      cannyTraceEdge(0, -1, i, j, *maxima);
      cannyTraceEdge(0, 1, i, j, *maxima);
      cannyTraceEdge(-1, 1, i, j, *maxima);
      cannyTraceEdge(1, -1, i, j, *maxima);
    }
  }

  // Final thresholding
  for (std::size_t i = 0; i < input_->size(); ++i) {
    if ((*maxima)[i].intensity == std::numeric_limits<float>::max())
      output[i].magnitude = 255;
    else
      output[i].magnitude = 0;
  }
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::canny(const pcl::PointCloud<PointInT>& input_x,
                                 const pcl::PointCloud<PointInT>& input_y,
                                 pcl::PointCloud<PointOutT>& output)
{
  float tHigh = hysteresis_threshold_high_;
  float tLow = hysteresis_threshold_low_;
  const int height = input_x.height;
  const int width = input_x.width;

  output.resize(height * width);
  output.height = height;
  output.width = width;

  // Noise reduction using gaussian blurring
  pcl::PointCloud<PointXYZI>::Ptr gaussian_kernel(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelSize(3);
  kernel_.setKernelSigma(1.0);
  kernel_.setKernelType(kernel<PointXYZI>::GAUSSIAN);
  kernel_.fetchKernel(*gaussian_kernel);
  convolution_.setKernel(*gaussian_kernel);

  PointCloudIn smoothed_cloud_x;
  convolution_.setInputCloud(input_x.makeShared());
  convolution_.filter(smoothed_cloud_x);

  PointCloudIn smoothed_cloud_y;
  convolution_.setInputCloud(input_y.makeShared());
  convolution_.filter(smoothed_cloud_y);

  // Edge detection using Sobel
  pcl::PointCloud<PointXYZIEdge>::Ptr edges(new pcl::PointCloud<PointXYZIEdge>);
  sobelMagnitudeDirection(smoothed_cloud_x, smoothed_cloud_y, *edges.get());

  // Edge discretization
  discretizeAngles(*edges);

  pcl::PointCloud<PointXYZI>::Ptr maxima(new pcl::PointCloud<PointXYZI>);
  suppressNonMaxima(*edges, *maxima, tLow);

  // Edge tracing
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      if ((*maxima)(j, i).intensity < tHigh ||
          (*maxima)(j, i).intensity == std::numeric_limits<float>::max())
        continue;

      (*maxima)(j, i).intensity = std::numeric_limits<float>::max();

      // clang-format off
      cannyTraceEdge( 1,  0, i, j, *maxima);
      cannyTraceEdge(-1,  0, i, j, *maxima);
      cannyTraceEdge( 1,  1, i, j, *maxima);
      cannyTraceEdge(-1, -1, i, j, *maxima);
      cannyTraceEdge( 0, -1, i, j, *maxima);
      cannyTraceEdge( 0,  1, i, j, *maxima);
      cannyTraceEdge(-1,  1, i, j, *maxima);
      cannyTraceEdge( 1, -1, i, j, *maxima);
      // clang-format on
    }
  }

  // Final thresholding
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      if ((*maxima)(j, i).intensity == std::numeric_limits<float>::max())
        output(j, i).magnitude = 255;
      else
        output(j, i).magnitude = 0;
    }
  }
}

template <typename PointInT, typename PointOutT>
void
Edge<PointInT, PointOutT>::detectEdgeLoG(const float kernel_sigma,
                                         const float kernel_size,
                                         pcl::PointCloud<PointOutT>& output)
{
  convolution_.setInputCloud(input_);

  pcl::PointCloud<PointXYZI>::Ptr log_kernel(new pcl::PointCloud<PointXYZI>);
  kernel_.setKernelType(kernel<PointXYZI>::LOG);
  kernel_.setKernelSigma(kernel_sigma);
  kernel_.setKernelSize(kernel_size);
  kernel_.fetchKernel(*log_kernel);
  convolution_.setKernel(*log_kernel);
  convolution_.filter(output);
}

} // namespace pcl
