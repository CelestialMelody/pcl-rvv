/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2013-, Open Perception, Inc.
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

#ifndef PCL_TRAJKOVIC_KEYPOINT_3D_IMPL_H_
#define PCL_TRAJKOVIC_KEYPOINT_3D_IMPL_H_

#include <pcl/features/integral_image_normal.h>
#include <pcl/rvv_point_traits.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif


namespace pcl
{

namespace detail
{
template <typename NormalT, bool HasNormal = pcl::traits::has_normal<NormalT>::value>
struct trajkovic3DNormalAoSFloatLayout : std::false_type {};

template <typename NormalT>
struct trajkovic3DNormalAoSFloatLayout<NormalT, true>
{
  using Pod = typename pcl::traits::POD<NormalT>::type;

  static constexpr std::size_t kNormalX = pcl::traits::offset<NormalT, pcl::fields::normal_x>::value;
  static constexpr std::size_t kNormalY = pcl::traits::offset<NormalT, pcl::fields::normal_y>::value;
  static constexpr std::size_t kNormalZ = pcl::traits::offset<NormalT, pcl::fields::normal_z>::value;

  static constexpr bool value =
      pcl::rvv::RVVNormalFloatLayout<NormalT>::value &&
      std::is_standard_layout_v<NormalT> && std::is_standard_layout_v<Pod> &&
      sizeof(NormalT) == sizeof(Pod) && sizeof(NormalT) % alignof(float) == 0 &&
      kNormalX % alignof(float) == 0 && kNormalY % alignof(float) == 0 &&
      kNormalZ % alignof(float) == 0;
};

template <typename PointInT, typename NormalT>
inline constexpr bool trajkovic3DResponseRVVSupported =
    pcl::rvv::RVVXYZAoSFloatLayout<PointInT>::value &&
    trajkovic3DNormalAoSFloatLayout<NormalT>::value;

template <typename PointInT, typename NormalT>
inline constexpr bool trajkovic3DFourCornersRVVSupported = trajkovic3DResponseRVVSupported<PointInT, NormalT>;

template <typename PointInT, typename NormalT>
inline constexpr bool trajkovic3DEightCornersRVVSupported = trajkovic3DResponseRVVSupported<PointInT, NormalT>;

#if defined(__RVV10__)
inline vbool32_t
trajkovic3DFiniteF32(vfloat32m1_t value, std::size_t vl)
{
  vbool32_t finite = __riscv_vmfeq_vv_f32m1_b32(value, value, vl);
  finite = __riscv_vmand_mm_b32(
      finite,
      __riscv_vmflt_vf_f32m1_b32(__riscv_vfabs_v_f32m1(value, vl), std::numeric_limits<float>::infinity(), vl),
      vl);
  return finite;
}

inline vbool32_t
trajkovic3DPointFiniteMask(vfloat32m1_t x, vfloat32m1_t y, vfloat32m1_t z, std::size_t vl)
{
  vbool32_t finite = trajkovic3DFiniteF32(x, vl);
  finite = __riscv_vmand_mm_b32(finite, trajkovic3DFiniteF32(y, vl), vl);
  finite = __riscv_vmand_mm_b32(finite, trajkovic3DFiniteF32(z, vl), vl);
  return finite;
}

inline vbool32_t
trajkovic3DNormalFiniteMask(vfloat32m1_t nx, vfloat32m1_t ny, vfloat32m1_t nz, std::size_t vl)
{
  return trajkovic3DPointFiniteMask(nx, ny, nz, vl);
}

inline void
trajkovic3DApplyNullNormalForInvalidMask(vbool32_t finite,
                                         std::size_t vl,
                                         vfloat32m1_t& nx,
                                         vfloat32m1_t& ny,
                                         vfloat32m1_t& nz)
{
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, vl);
  nx = __riscv_vmerge_vvm_f32m1(zero, nx, finite, vl);
  ny = __riscv_vmerge_vvm_f32m1(zero, ny, finite, vl);
  nz = __riscv_vmerge_vvm_f32m1(zero, nz, finite, vl);
}

inline vfloat32m1_t
trajkovic3DNormalDiff(vfloat32m1_t ax,
                      vfloat32m1_t ay,
                      vfloat32m1_t az,
                      vfloat32m1_t bx,
                      vfloat32m1_t by,
                      vfloat32m1_t bz,
                      std::size_t vl)
{
  vfloat32m1_t dot = __riscv_vfmul_vv_f32m1(ax, bx, vl);
  dot = __riscv_vfmacc_vv_f32m1(dot, ay, by, vl);
  dot = __riscv_vfmacc_vv_f32m1(dot, az, bz, vl);
  return __riscv_vfrsub_vf_f32m1(dot, 1.0f, vl);
}

template <typename PointInT>
inline void
trajkovic3DLoadPointFields(const PointInT* base,
                           std::size_t offset,
                           std::size_t vl,
                           vfloat32m1_t& x,
                           vfloat32m1_t& y,
                           vfloat32m1_t& z)
{
  constexpr std::size_t kXOff = pcl::traits::offset<PointInT, pcl::fields::x>::value;
  constexpr std::size_t kYOff = pcl::traits::offset<PointInT, pcl::fields::y>::value;
  constexpr std::size_t kZOff = pcl::traits::offset<PointInT, pcl::fields::z>::value;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(base + offset);
  x = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + kXOff),
                             static_cast<std::ptrdiff_t>(sizeof(PointInT)),
                             vl);
  y = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + kYOff),
                             static_cast<std::ptrdiff_t>(sizeof(PointInT)),
                             vl);
  z = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + kZOff),
                             static_cast<std::ptrdiff_t>(sizeof(PointInT)),
                             vl);
}

template <typename NormalT>
inline void
trajkovic3DLoadNormalFields(const NormalT* base,
                            std::size_t offset,
                            std::size_t vl,
                            vfloat32m1_t& nx,
                            vfloat32m1_t& ny,
                            vfloat32m1_t& nz)
{
  constexpr std::size_t kNXOff = pcl::traits::offset<NormalT, pcl::fields::normal_x>::value;
  constexpr std::size_t kNYOff = pcl::traits::offset<NormalT, pcl::fields::normal_y>::value;
  constexpr std::size_t kNZOff = pcl::traits::offset<NormalT, pcl::fields::normal_z>::value;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(base + offset);
  nx = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + kNXOff),
                              static_cast<std::ptrdiff_t>(sizeof(NormalT)),
                              vl);
  ny = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + kNYOff),
                              static_cast<std::ptrdiff_t>(sizeof(NormalT)),
                              vl);
  nz = __riscv_vlse32_v_f32m1(reinterpret_cast<const float*>(bytes + kNZOff),
                              static_cast<std::ptrdiff_t>(sizeof(NormalT)),
                              vl);
}

template <typename PointInT, typename NormalT>
inline void
trajkovic3DFourCornersResponseRVV(const PointInT* points,
                                  const NormalT* normals,
                                  const int width,
                                  const int height,
                                  const int half_window_size,
                                  const float first_threshold,
                                  pcl::PointCloud<float>& response)
{
  std::fill(response.points.begin(), response.points.end(), 0.0f);
  const std::size_t step = static_cast<std::size_t>(half_window_size);
  for (std::size_t row = step; row < static_cast<std::size_t>(height) - step; ++row)
  {
    std::size_t col = step;
    for (; col < static_cast<std::size_t>(width) - step;)
    {
      const std::size_t available = static_cast<std::size_t>(width) - step - col;
      const std::size_t vl = __riscv_vsetvl_e32m1(available);
      const std::size_t center = row * static_cast<std::size_t>(width) + col;

      vfloat32m1_t px;
      vfloat32m1_t py;
      vfloat32m1_t pz;
      vfloat32m1_t cx;
      vfloat32m1_t cy;
      vfloat32m1_t cz;
      vfloat32m1_t ux;
      vfloat32m1_t uy;
      vfloat32m1_t uz;
      vfloat32m1_t dx;
      vfloat32m1_t dy;
      vfloat32m1_t dz;
      vfloat32m1_t lx;
      vfloat32m1_t ly;
      vfloat32m1_t lz;
      vfloat32m1_t rx;
      vfloat32m1_t ry;
      vfloat32m1_t rz;
      trajkovic3DLoadPointFields(points, center, vl, px, py, pz);
      trajkovic3DLoadNormalFields(normals, center, vl, cx, cy, cz);
      trajkovic3DLoadNormalFields(normals, (row - step) * static_cast<std::size_t>(width) + col, vl, ux, uy, uz);
      trajkovic3DLoadNormalFields(normals, (row + step) * static_cast<std::size_t>(width) + col, vl, dx, dy, dz);
      trajkovic3DLoadNormalFields(normals, row * static_cast<std::size_t>(width) + (col - step), vl, lx, ly, lz);
      trajkovic3DLoadNormalFields(normals, row * static_cast<std::size_t>(width) + (col + step), vl, rx, ry, rz);

      const vbool32_t up_finite = trajkovic3DNormalFiniteMask(ux, uy, uz, vl);
      const vbool32_t down_finite = trajkovic3DNormalFiniteMask(dx, dy, dz, vl);
      const vbool32_t left_finite = trajkovic3DNormalFiniteMask(lx, ly, lz, vl);
      const vbool32_t right_finite = trajkovic3DNormalFiniteMask(rx, ry, rz, vl);
      trajkovic3DApplyNullNormalForInvalidMask(up_finite, vl, ux, uy, uz);
      trajkovic3DApplyNullNormalForInvalidMask(down_finite, vl, dx, dy, dz);
      trajkovic3DApplyNullNormalForInvalidMask(left_finite, vl, lx, ly, lz);
      trajkovic3DApplyNullNormalForInvalidMask(right_finite, vl, rx, ry, rz);

      vbool32_t active =
          __riscv_vmand_mm_b32(trajkovic3DPointFiniteMask(px, py, pz, vl), trajkovic3DNormalFiniteMask(cx, cy, cz, vl), vl);
      vbool32_t neighbor_any = up_finite;
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, down_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, left_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, right_finite, vl);
      active = __riscv_vmand_mm_b32(active, neighbor_any, vl);

      const vfloat32m1_t up_diff = trajkovic3DNormalDiff(ux, uy, uz, cx, cy, cz, vl);
      const vfloat32m1_t down_diff = trajkovic3DNormalDiff(dx, dy, dz, cx, cy, cz, vl);
      const vfloat32m1_t right_diff = trajkovic3DNormalDiff(rx, ry, rz, cx, cy, cz, vl);
      const vfloat32m1_t left_diff = trajkovic3DNormalDiff(lx, ly, lz, cx, cy, cz, vl);
      const vfloat32m1_t sn1 = __riscv_vfmul_vv_f32m1(up_diff, up_diff, vl);
      const vfloat32m1_t sn2 = __riscv_vfmul_vv_f32m1(down_diff, down_diff, vl);
      const vfloat32m1_t r1 = __riscv_vfadd_vv_f32m1(sn1, sn2, vl);
      const vfloat32m1_t right_sq = __riscv_vfmul_vv_f32m1(right_diff, right_diff, vl);
      const vfloat32m1_t left_sq = __riscv_vfmul_vv_f32m1(left_diff, left_diff, vl);
      const vfloat32m1_t r2 = __riscv_vfadd_vv_f32m1(right_sq, left_sq, vl);
      const vfloat32m1_t d = __riscv_vfmin_vv_f32m1(r1, r2, vl);

      active = __riscv_vmand_mm_b32(active, __riscv_vmfge_vf_f32m1_b32(d, first_threshold, vl), vl);

      const vfloat32m1_t sn1_sqrt = __riscv_vfsqrt_v_f32m1(sn1, vl);
      const vfloat32m1_t sn2_sqrt = __riscv_vfsqrt_v_f32m1(sn2, vl);
      vfloat32m1_t b1 = __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(rx, ry, rz, ux, uy, uz, vl), sn1_sqrt, vl);
      b1 = __riscv_vfmacc_vv_f32m1(b1, trajkovic3DNormalDiff(lx, ly, lz, dx, dy, dz, vl), sn2_sqrt, vl);
      vfloat32m1_t b2 = __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(rx, ry, rz, dx, dy, dz, vl), sn2_sqrt, vl);
      b2 = __riscv_vfmacc_vv_f32m1(b2, trajkovic3DNormalDiff(lx, ly, lz, ux, uy, uz, vl), sn1_sqrt, vl);
      const vfloat32m1_t b = __riscv_vfmin_vv_f32m1(b1, b2, vl);
      const vfloat32m1_t a = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r2, r1, vl),
                                                    __riscv_vfmul_vf_f32m1(b, 2.0f, vl),
                                                    vl);
      const vbool32_t use_quad = __riscv_vmand_mm_b32(
          __riscv_vmflt_vf_f32m1_b32(b, 0.0f, vl),
          __riscv_vmfgt_vf_f32m1_b32(__riscv_vfadd_vv_f32m1(b, a, vl), 0.0f, vl),
          vl);
      const vfloat32m1_t bb = __riscv_vfmul_vv_f32m1(b, b, vl);
      const vfloat32m1_t quad = __riscv_vfsub_vv_f32m1(r1, __riscv_vfdiv_vv_f32m1(bb, a, vl), vl);
      const vfloat32m1_t selected = __riscv_vmerge_vvm_f32m1(d, quad, use_quad, vl);
      __riscv_vse32_v_f32m1_m(active, response.points.data() + center, selected, vl);
      col += vl;
    }
  }
}

template <typename PointInT, typename NormalT>
inline void
trajkovic3DEightCornersResponseRVV(const PointInT* points,
                                   const NormalT* normals,
                                   const int width,
                                   const int height,
                                   const int half_window_size,
                                   const float first_threshold,
                                   pcl::PointCloud<float>& response)
{
  std::fill(response.points.begin(), response.points.end(), 0.0f);
  const std::size_t step = static_cast<std::size_t>(half_window_size);
  for (std::size_t row = step; row < static_cast<std::size_t>(height) - step; ++row)
  {
    std::size_t col = step;
    for (; col < static_cast<std::size_t>(width) - step;)
    {
      const std::size_t available = static_cast<std::size_t>(width) - step - col;
      const std::size_t vl = __riscv_vsetvl_e32m1(available);
      const std::size_t center = row * static_cast<std::size_t>(width) + col;

      vfloat32m1_t px;
      vfloat32m1_t py;
      vfloat32m1_t pz;
      vfloat32m1_t cx;
      vfloat32m1_t cy;
      vfloat32m1_t cz;
      vfloat32m1_t ux;
      vfloat32m1_t uy;
      vfloat32m1_t uz;
      vfloat32m1_t dx;
      vfloat32m1_t dy;
      vfloat32m1_t dz;
      vfloat32m1_t lx;
      vfloat32m1_t ly;
      vfloat32m1_t lz;
      vfloat32m1_t rx;
      vfloat32m1_t ry;
      vfloat32m1_t rz;
      vfloat32m1_t ulx;
      vfloat32m1_t uly;
      vfloat32m1_t ulz;
      vfloat32m1_t urx;
      vfloat32m1_t ury;
      vfloat32m1_t urz;
      vfloat32m1_t dlx;
      vfloat32m1_t dly;
      vfloat32m1_t dlz;
      vfloat32m1_t drx;
      vfloat32m1_t dry;
      vfloat32m1_t drz;
      trajkovic3DLoadPointFields(points, center, vl, px, py, pz);
      trajkovic3DLoadNormalFields(normals, center, vl, cx, cy, cz);
      trajkovic3DLoadNormalFields(normals, (row - step) * static_cast<std::size_t>(width) + col, vl, ux, uy, uz);
      trajkovic3DLoadNormalFields(normals, (row + step) * static_cast<std::size_t>(width) + col, vl, dx, dy, dz);
      trajkovic3DLoadNormalFields(normals, row * static_cast<std::size_t>(width) + (col - step), vl, lx, ly, lz);
      trajkovic3DLoadNormalFields(normals, row * static_cast<std::size_t>(width) + (col + step), vl, rx, ry, rz);
      trajkovic3DLoadNormalFields(
          normals, (row - step) * static_cast<std::size_t>(width) + (col - step), vl, ulx, uly, ulz);
      trajkovic3DLoadNormalFields(
          normals, (row - step) * static_cast<std::size_t>(width) + (col + step), vl, urx, ury, urz);
      trajkovic3DLoadNormalFields(
          normals, (row + step) * static_cast<std::size_t>(width) + (col - step), vl, dlx, dly, dlz);
      trajkovic3DLoadNormalFields(
          normals, (row + step) * static_cast<std::size_t>(width) + (col + step), vl, drx, dry, drz);

      const vbool32_t up_finite = trajkovic3DNormalFiniteMask(ux, uy, uz, vl);
      const vbool32_t down_finite = trajkovic3DNormalFiniteMask(dx, dy, dz, vl);
      const vbool32_t left_finite = trajkovic3DNormalFiniteMask(lx, ly, lz, vl);
      const vbool32_t right_finite = trajkovic3DNormalFiniteMask(rx, ry, rz, vl);
      const vbool32_t upleft_finite = trajkovic3DNormalFiniteMask(ulx, uly, ulz, vl);
      const vbool32_t upright_finite = trajkovic3DNormalFiniteMask(urx, ury, urz, vl);
      const vbool32_t downleft_finite = trajkovic3DNormalFiniteMask(dlx, dly, dlz, vl);
      const vbool32_t downright_finite = trajkovic3DNormalFiniteMask(drx, dry, drz, vl);
      trajkovic3DApplyNullNormalForInvalidMask(up_finite, vl, ux, uy, uz);
      trajkovic3DApplyNullNormalForInvalidMask(down_finite, vl, dx, dy, dz);
      trajkovic3DApplyNullNormalForInvalidMask(left_finite, vl, lx, ly, lz);
      trajkovic3DApplyNullNormalForInvalidMask(right_finite, vl, rx, ry, rz);
      trajkovic3DApplyNullNormalForInvalidMask(upleft_finite, vl, ulx, uly, ulz);
      trajkovic3DApplyNullNormalForInvalidMask(upright_finite, vl, urx, ury, urz);
      trajkovic3DApplyNullNormalForInvalidMask(downleft_finite, vl, dlx, dly, dlz);
      trajkovic3DApplyNullNormalForInvalidMask(downright_finite, vl, drx, dry, drz);

      vbool32_t active = __riscv_vmand_mm_b32(trajkovic3DPointFiniteMask(px, py, pz, vl),
                                              trajkovic3DNormalFiniteMask(cx, cy, cz, vl),
                                              vl);
      vbool32_t neighbor_any = up_finite;
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, down_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, left_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, right_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, upleft_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, upright_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, downleft_finite, vl);
      neighbor_any = __riscv_vmor_mm_b32(neighbor_any, downright_finite, vl);
      active = __riscv_vmand_mm_b32(active, neighbor_any, vl);

      const vfloat32m1_t up_center = trajkovic3DNormalDiff(ux, uy, uz, cx, cy, cz, vl);
      const vfloat32m1_t down_center = trajkovic3DNormalDiff(dx, dy, dz, cx, cy, cz, vl);
      const vfloat32m1_t left_center = trajkovic3DNormalDiff(lx, ly, lz, cx, cy, cz, vl);
      const vfloat32m1_t right_center = trajkovic3DNormalDiff(rx, ry, rz, cx, cy, cz, vl);
      const vfloat32m1_t upright_center = trajkovic3DNormalDiff(urx, ury, urz, cx, cy, cz, vl);
      const vfloat32m1_t downleft_center = trajkovic3DNormalDiff(dlx, dly, dlz, cx, cy, cz, vl);
      const vfloat32m1_t downright_center = trajkovic3DNormalDiff(drx, dry, drz, cx, cy, cz, vl);
      const vfloat32m1_t upleft_center = trajkovic3DNormalDiff(ulx, uly, ulz, cx, cy, cz, vl);

      const vfloat32m1_t r0 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(up_center, up_center, vl),
          __riscv_vfmul_vv_f32m1(down_center, down_center, vl),
          vl);
      const vfloat32m1_t r1 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(upright_center, upright_center, vl),
          __riscv_vfmul_vv_f32m1(downleft_center, downleft_center, vl),
          vl);
      const vfloat32m1_t r2 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(right_center, right_center, vl),
          __riscv_vfmul_vv_f32m1(left_center, left_center, vl),
          vl);
      const vfloat32m1_t r3 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(downright_center, downright_center, vl),
          __riscv_vfmul_vv_f32m1(upleft_center, upleft_center, vl),
          vl);
      const vfloat32m1_t d = __riscv_vfmin_vv_f32m1(
          __riscv_vfmin_vv_f32m1(r0, r1, vl),
          __riscv_vfmin_vv_f32m1(r2, r3, vl),
          vl);

      active = __riscv_vmand_mm_b32(active, __riscv_vmfge_vf_f32m1_b32(d, first_threshold, vl), vl);

      const vfloat32m1_t b0 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(urx, ury, urz, ux, uy, uz, vl), up_center, vl),
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(dlx, dly, dlz, dx, dy, dz, vl), down_center, vl),
          vl);
      const vfloat32m1_t b1 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(rx, ry, rz, urx, ury, urz, vl), upright_center, vl),
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(lx, ly, lz, dlx, dly, dlz, vl), downleft_center, vl),
          vl);
      const vfloat32m1_t b2 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(drx, dry, drz, rx, ry, rz, vl), downright_center, vl),
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(ulx, uly, ulz, lx, ly, lz, vl), upleft_center, vl),
          vl);
      const vfloat32m1_t b3 = __riscv_vfadd_vv_f32m1(
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(dx, dy, dz, drx, dry, drz, vl), downright_center, vl),
          __riscv_vfmul_vv_f32m1(trajkovic3DNormalDiff(ux, uy, uz, ulx, uly, ulz, vl), upleft_center, vl),
          vl);
      const vfloat32m1_t a0 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r1, r0, vl),
                                                     __riscv_vfadd_vv_f32m1(b0, b0, vl),
                                                     vl);
      const vfloat32m1_t a1 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r2, r1, vl),
                                                     __riscv_vfadd_vv_f32m1(b1, b1, vl),
                                                     vl);
      const vfloat32m1_t a2 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r3, r2, vl),
                                                     __riscv_vfadd_vv_f32m1(b2, b2, vl),
                                                     vl);
      const vfloat32m1_t a3 = __riscv_vfsub_vv_f32m1(__riscv_vfsub_vv_f32m1(r0, r3, vl),
                                                     __riscv_vfadd_vv_f32m1(b3, b3, vl),
                                                     vl);
      const vbool32_t use_d = __riscv_vmand_mm_b32(
          __riscv_vmflt_vf_f32m1_b32(__riscv_vfmax_vv_f32m1(__riscv_vfmax_vv_f32m1(b0, b1, vl),
                                                            __riscv_vfmax_vv_f32m1(b2, b3, vl),
                                                            vl),
                                     0.0f,
                                     vl),
          __riscv_vmfgt_vf_f32m1_b32(__riscv_vfmin_vv_f32m1(
                                         __riscv_vfmin_vv_f32m1(__riscv_vfadd_vv_f32m1(a0, b0, vl),
                                                                __riscv_vfadd_vv_f32m1(a1, b1, vl),
                                                                vl),
                                         __riscv_vfmin_vv_f32m1(__riscv_vfadd_vv_f32m1(a2, b2, vl),
                                                                __riscv_vfadd_vv_f32m1(a3, b3, vl),
                                                                vl),
                                         vl),
                                       0.0f,
                                       vl),
          vl);
      const vfloat32m1_t d0 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b0, b0, vl), a0, vl);
      const vfloat32m1_t d1 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b1, b1, vl), a1, vl);
      const vfloat32m1_t d2 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b2, b2, vl), a2, vl);
      const vfloat32m1_t d3 = __riscv_vfdiv_vv_f32m1(__riscv_vfmul_vv_f32m1(b3, b3, vl), a3, vl);
      const vfloat32m1_t selected = __riscv_vmerge_vvm_f32m1(
          d,
          __riscv_vfmin_vv_f32m1(__riscv_vfmin_vv_f32m1(d0, d1, vl),
                                 __riscv_vfmin_vv_f32m1(d2, d3, vl),
                                 vl),
          use_d,
          vl);
      __riscv_vse32_v_f32m1_m(active, response.points.data() + center, selected, vl);
      col += vl;
    }
  }
}
#endif
} // namespace detail

template <typename PointInT, typename PointOutT, typename NormalT> bool
TrajkovicKeypoint3D<PointInT, PointOutT, NormalT>::initCompute ()
{
  if (!PCLBase<PointInT>::initCompute ())
    return (false);

  keypoints_indices_.reset (new pcl::PointIndices);
  keypoints_indices_->indices.reserve (input_->size ());

  if (indices_->size () != input_->size ())
  {
    PCL_ERROR ("[pcl::%s::initCompute] %s doesn't support setting indices!\n", name_.c_str ());
    return (false);
  }

  if ((window_size_%2) == 0)
  {
    PCL_ERROR ("[pcl::%s::initCompute] Window size must be odd!\n", name_.c_str ());
    return (false);
  }

  if (window_size_ < 3)
  {
    PCL_ERROR ("[pcl::%s::initCompute] Window size must be >= 3x3!\n", name_.c_str ());
    return (false);
  }

  half_window_size_ = window_size_ / 2;

  if (!normals_)
  {
    NormalsPtr normals (new Normals ());
    pcl::IntegralImageNormalEstimation<PointInT, NormalT> normal_estimation;
    normal_estimation.setNormalEstimationMethod (pcl::IntegralImageNormalEstimation<PointInT, NormalT>::SIMPLE_3D_GRADIENT);
    normal_estimation.setInputCloud (input_);
    normal_estimation.setNormalSmoothingSize (5.0);
    normal_estimation.compute (*normals);
    normals_ = normals;
  }

  if (normals_->size () != input_->size ())
  {
    PCL_ERROR ("[pcl::%s::initCompute] normals given, but the number of normals does not match the number of input points!\n", name_.c_str ());
    return (false);
  }

  return (true);
}


template <typename PointInT, typename PointOutT, typename NormalT> void
TrajkovicKeypoint3D<PointInT, PointOutT, NormalT>::detectKeypoints (PointCloudOut &output)
{
  response_.reset (new pcl::PointCloud<float> (input_->width, input_->height));
  const Normals &normals = *normals_;
  const PointCloudIn &input = *input_;
  pcl::PointCloud<float>& response = *response_;
  const int w = static_cast<int> (input_->width) - half_window_size_;
  const int h = static_cast<int> (input_->height) - half_window_size_;

  bool response_done = false;
#if defined(__RVV10__)
  if constexpr (detail::trajkovic3DResponseRVVSupported<PointInT, NormalT>)
  {
    if (method_ == FOUR_CORNERS && window_size_ == 3 && input.is_dense && normals.is_dense)
    {
      detail::trajkovic3DFourCornersResponseRVV<PointInT, NormalT> (
          input.points.data (), normals.points.data (),
          static_cast<int> (input_->width), static_cast<int> (input_->height),
          half_window_size_, first_threshold_, response);
      response_done = true;
    }
    else if (method_ == EIGHT_CORNERS && window_size_ == 3 && input.is_dense && normals.is_dense)
    {
      detail::trajkovic3DEightCornersResponseRVV<PointInT, NormalT> (
          input.points.data (), normals.points.data (),
          static_cast<int> (input_->width), static_cast<int> (input_->height),
          half_window_size_, first_threshold_, response);
      response_done = true;
    }
  }
#endif

  if (!response_done)
  {
  if (method_ == FOUR_CORNERS)
  {
#if OPENMP_LEGACY_CONST_DATA_SHARING_RULE
#pragma omp parallel for \
  default(none) \
  shared(input, normals, response) \
  num_threads(threads_)
#else
#pragma omp parallel for \
  default(none) \
  shared(h, input, normals, response, w) \
  num_threads(threads_)
#endif
    for(int j = half_window_size_; j < h; ++j)
    {
      for(int i = half_window_size_; i < w; ++i)
      {
        if (!isFinite (input (i,j))) continue;
        const NormalT &center = normals (i,j);
        if (!isFinite (center)) continue;

        int count = 0;
        const NormalT &up = getNormalOrNull (i, j-half_window_size_, count);
        const NormalT &down = getNormalOrNull (i, j+half_window_size_, count);
        const NormalT &left = getNormalOrNull (i-half_window_size_, j, count);
        const NormalT &right = getNormalOrNull (i+half_window_size_, j, count);
        // Get rid of isolated points
        if (!count) continue;

        float sn1 = squaredNormalsDiff (up, center);
        float sn2 = squaredNormalsDiff (down, center);
        float r1 = sn1 + sn2;
        float r2 = squaredNormalsDiff (right, center) + squaredNormalsDiff (left, center);

        float d = std::min (r1, r2);
        if (d < first_threshold_) continue;

        sn1 = std::sqrt (sn1);
        sn2 = std::sqrt (sn2);
        float b1 = normalsDiff (right, up) * sn1;
        b1+= normalsDiff (left, down) * sn2;
        float b2 = normalsDiff (right, down) * sn2;
        b2+= normalsDiff (left, up) * sn1;
        float B = std::min (b1, b2);
        float A = r2 - r1 - 2*B;

        response (i,j) = ((B < 0) && ((B + A) > 0)) ? r1 - ((B*B)/A) : d;
      }
    }
  }
  else if (method_ == EIGHT_CORNERS)
  {
#if OPENMP_LEGACY_CONST_DATA_SHARING_RULE
#pragma omp parallel for \
  default(none) \
  shared(input, normals, response) \
  num_threads(threads_)
#else
#pragma omp parallel for \
  default(none) \
  shared(h, input, normals, response, w) \
  num_threads(threads_)
#endif
    for(int j = half_window_size_; j < h; ++j)
    {
      for(int i = half_window_size_; i < w; ++i)
      {
        if (!isFinite (input (i,j))) continue;
        const NormalT &center = normals (i,j);
        if (!isFinite (center)) continue;

        int count = 0;
        const NormalT &up = getNormalOrNull (i, j-half_window_size_, count);
        const NormalT &down = getNormalOrNull (i, j+half_window_size_, count);
        const NormalT &left = getNormalOrNull (i-half_window_size_, j, count);
        const NormalT &right = getNormalOrNull (i+half_window_size_, j, count);
        const NormalT &upleft = getNormalOrNull (i-half_window_size_, j-half_window_size_, count);
        const NormalT &upright = getNormalOrNull (i+half_window_size_, j-half_window_size_, count);
        const NormalT &downleft = getNormalOrNull (i-half_window_size_, j+half_window_size_, count);
        const NormalT &downright = getNormalOrNull (i+half_window_size_, j+half_window_size_, count);
        if (!count) continue;

        std::vector<float> r (4,0);

        r[0] = squaredNormalsDiff (up, center);
        r[0]+= squaredNormalsDiff (down, center);

        r[1] = squaredNormalsDiff (upright, center);
        r[1]+= squaredNormalsDiff (downleft, center);

        r[2] = squaredNormalsDiff (right, center);
        r[2]+= squaredNormalsDiff (left, center);

        r[3] = squaredNormalsDiff (downright, center);
        r[3]+= squaredNormalsDiff (upleft, center);

        float d = *(std::min_element (r.begin (), r.end ()));

        if (d < first_threshold_) continue;

        std::vector<float> B (4,0);
        std::vector<float> A (4,0);
        std::vector<float> sumAB (4,0);
        B[0] = normalsDiff (upright, up) * normalsDiff (up, center);
        B[0]+= normalsDiff (downleft, down) * normalsDiff (down, center);
        B[1] = normalsDiff (right, upright) * normalsDiff (upright, center);
        B[1]+= normalsDiff (left, downleft) * normalsDiff (downleft, center);
        B[2] = normalsDiff (downright, right) * normalsDiff (downright, center);
        B[2]+= normalsDiff (upleft, left) * normalsDiff (upleft, center);
        B[3] = normalsDiff (down, downright) * normalsDiff (downright, center);
        B[3]+= normalsDiff (up, upleft) * normalsDiff (upleft, center);
        A[0] = r[1] - r[0] - B[0] - B[0];
        A[1] = r[2] - r[1] - B[1] - B[1];
        A[2] = r[3] - r[2] - B[2] - B[2];
        A[3] = r[0] - r[3] - B[3] - B[3];
        sumAB[0] = A[0] + B[0];
        sumAB[1] = A[1] + B[1];
        sumAB[2] = A[2] + B[2];
        sumAB[3] = A[3] + B[3];
        if ((*std::max_element (B.begin (), B.end ()) < 0) &&
            (*std::min_element (sumAB.begin (), sumAB.end ()) > 0))
        {
          std::vector<float> D (4,0);
          D[0] = B[0] * B[0] / A[0];
          D[1] = B[1] * B[1] / A[1];
          D[2] = B[2] * B[2] / A[2];
          D[3] = B[3] * B[3] / A[3];
          response (i,j) = *(std::min (D.begin (), D.end ()));
        }
        else
          response (i,j) = d;
      }
    }
  }
  else
  {
#if OPENMP_LEGACY_CONST_DATA_SHARING_RULE
#pragma omp parallel for \
  default(none) \
  shared(input, normals, response) \
  num_threads(threads_)
#else
#pragma omp parallel for \
  default(none) \
  shared(h, input, normals, response, w) \
  num_threads(threads_)
#endif
    for(int j = half_window_size_; j < h; ++j)
    {
      for(int i = half_window_size_; i < w; ++i)
      {
        if (!isFinite (input (i,j))) continue;
        const NormalT &center = normals (i,j);
        if (!isFinite (center)) continue;

        int count = 0;
        const NormalT &up = getNormalOrNull (i, j-half_window_size_, count);
        const NormalT &down = getNormalOrNull (i, j+half_window_size_, count);
        const NormalT &left = getNormalOrNull (i-half_window_size_, j, count);
        const NormalT &right = getNormalOrNull (i+half_window_size_, j, count);
        const NormalT &upleft = getNormalOrNull (i-half_window_size_, j-half_window_size_, count);
        const NormalT &upright = getNormalOrNull (i+half_window_size_, j-half_window_size_, count);
        const NormalT &downleft = getNormalOrNull (i-half_window_size_, j+half_window_size_, count);
        const NormalT &downright = getNormalOrNull (i+half_window_size_, j+half_window_size_, count);
        // Get rid of isolated points
        if (!count) continue;

        std::vector<float> r (4,0);

        r[0] = squaredNormalsDiff (up, center);
        r[0]+= squaredNormalsDiff (down, center);

        r[1] = squaredNormalsDiff (upright, center);
        r[1]+= squaredNormalsDiff (downleft, center);

        r[2] = squaredNormalsDiff (right, center);
        r[2]+= squaredNormalsDiff (left, center);

        r[3] = squaredNormalsDiff (downright, center);
        r[3]+= squaredNormalsDiff (upleft, center);

        float d = *(std::min_element (r.begin (), r.end ()));

        if (d < first_threshold_) continue;

        std::vector<float> B (4,0);
        std::vector<float> A (4,0);
        std::vector<float> sumAB (4,0);
        B[0] = normalsDiff (upright, up) * normalsDiff (up, center);
        B[0]+= normalsDiff (downleft, down) * normalsDiff (down, center);
        B[1] = normalsDiff (right, upright) * normalsDiff (upright, center);
        B[1]+= normalsDiff (left, downleft) * normalsDiff (downleft, center);
        B[2] = normalsDiff (downright, right) * normalsDiff (downright, center);
        B[2]+= normalsDiff (upleft, left) * normalsDiff (upleft, center);
        B[3] = normalsDiff (down, downright) * normalsDiff (downright, center);
        B[3]+= normalsDiff (up, upleft) * normalsDiff (upleft, center);
        A[0] = r[1] - r[0] - B[0] - B[0];
        A[1] = r[2] - r[1] - B[1] - B[1];
        A[2] = r[3] - r[2] - B[2] - B[2];
        A[3] = r[0] - r[3] - B[3] - B[3];
        sumAB[0] = A[0] + B[0];
        sumAB[1] = A[1] + B[1];
        sumAB[2] = A[2] + B[2];
        sumAB[3] = A[3] + B[3];
        if ((*std::max_element (B.begin (), B.end ()) < 0) &&
            (*std::min_element (sumAB.begin (), sumAB.end ()) > 0))
        {
          std::vector<float> D (4,0);
          D[0] = B[0] * B[0] / A[0];
          D[1] = B[1] * B[1] / A[1];
          D[2] = B[2] * B[2] / A[2];
          D[3] = B[3] * B[3] / A[3];
          response (i,j) = *(std::min (D.begin (), D.end ()));
        }
        else
          response (i,j) = d;
      }
    }
  }
  }
  // Non maximas suppression
  pcl::Indices indices = *indices_;
  std::sort (indices.begin (), indices.end (), [this] (int p1, int p2) { return greaterCornernessAtIndices (p1, p2); });

  output.clear ();
  output.reserve (input_->size ());

  std::vector<bool> occupency_map (indices.size (), false);
  const int width (input_->width);
  const int height (input_->height);

#if OPENMP_LEGACY_CONST_DATA_SHARING_RULE
#pragma omp parallel for \
  default(none) \
  shared(indices, occupency_map, output) \
  num_threads(threads_)
#else
#pragma omp parallel for \
  default(none) \
  shared(height, indices, occupency_map, output, width) \
  num_threads(threads_)
#endif
  for (int i = 0; i < static_cast<int>(indices.size ()); ++i)
  {
    int idx = indices[static_cast<std::size_t>(i)];
    if (((*response_)[idx] < second_threshold_) || occupency_map[idx])
      continue;

    PointOutT p;
    p.getVector3fMap () = (*input_)[idx].getVector3fMap ();
    p.intensity = response_->points [idx];

#pragma omp critical
    {
      output.push_back (p);
      keypoints_indices_->indices.push_back (idx);
    }

    const int x = idx % width;
    const int y = idx / width;
    const int u_end = std::min (width, x + half_window_size_);
    const int v_end = std::min (height, y + half_window_size_);
    for(int v = std::max (0, y - half_window_size_); v < v_end; ++v)
      for(int u = std::max (0, x - half_window_size_); u < u_end; ++u)
        occupency_map[v*width + u] = true;
  }

  output.height = 1;
  output.width = output.size();
  // we don not change the denseness
  output.is_dense = true;
}

} // namespace pcl

#define PCL_INSTANTIATE_TrajkovicKeypoint3D(T,U,N) template class PCL_EXPORTS pcl::TrajkovicKeypoint3D<T,U,N>;

#endif
