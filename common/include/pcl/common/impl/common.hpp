/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2010, Willow Garage, Inc.
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
 * $Id$
 *
 */

#ifndef PCL_COMMON_IMPL_H_
#define PCL_COMMON_IMPL_H_

#include <pcl/point_types.h>
#include <pcl/common/common.h>
#include <cmath>
#include <cstdint>
#include <limits>

//////////////////////////////////////////////////////////////////////////////////////////////
inline double
pcl::getAngle3D (const Eigen::Vector4f &v1, const Eigen::Vector4f &v2, const bool in_degree)
{
  // Compute the actual angle
  double rad = v1.normalized ().dot (v2.normalized ());
  if (rad < -1.0)
    rad = -1.0;
  else if (rad >  1.0)
    rad = 1.0;
  return (in_degree ? std::acos (rad) * 180.0 / M_PI : std::acos (rad));
}

inline double
pcl::getAngle3D (const Eigen::Vector3f &v1, const Eigen::Vector3f &v2, const bool in_degree)
{
  // Compute the actual angle
  double rad = v1.normalized ().dot (v2.normalized ());
  if (rad < -1.0)
    rad = -1.0;
  else if (rad >  1.0)
    rad = 1.0;
  return (in_degree ? std::acos (rad) * 180.0 / M_PI : std::acos (rad));
}

#ifdef __SSE__
inline __m128
pcl::acos_SSE (const __m128 &x)
{
  /*
  This python code generates the coefficients:
  import math, numpy, scipy.optimize
  def get_error(S):
      err_sum=0.0
      for x in numpy.arange(0.0, 1.0, 0.0025):
          if (S[3]+S[4]*x)<0.0:
              err_sum+=10.0
          else:
              err_sum+=((S[0]+x*(S[1]+x*S[2]))*numpy.sqrt(S[3]+S[4]*x)+S[5]+x*(S[6]+x*S[7])-math.acos(x))**2.0
      return err_sum/400.0

  print(scipy.optimize.minimize(fun=get_error, x0=[1.57, 0.0, 0.0, 1.0, -1.0, 0.0, 0.0, 0.0], method='Nelder-Mead', options={'maxiter':42000, 'maxfev':42000, 'disp':True, 'xatol':1e-6, 'fatol':1e-6}))
  */
  const __m128 mul_term = _mm_add_ps (_mm_set1_ps (1.59121552f), _mm_mul_ps (x, _mm_add_ps (_mm_set1_ps (-0.15461442f), _mm_mul_ps (x, _mm_set1_ps (0.05354897f)))));
  const __m128 add_term = _mm_add_ps (_mm_set1_ps (0.06681017f), _mm_mul_ps (x, _mm_add_ps (_mm_set1_ps (-0.09402311f), _mm_mul_ps (x, _mm_set1_ps (0.02708663f)))));
  return _mm_add_ps (_mm_mul_ps (mul_term, _mm_sqrt_ps (_mm_add_ps (_mm_set1_ps (0.89286965f), _mm_mul_ps (_mm_set1_ps (-0.89282669f), x)))), add_term);
}

inline __m128
pcl::getAcuteAngle3DSSE (const __m128 &x1, const __m128 &y1, const __m128 &z1, const __m128 &x2, const __m128 &y2, const __m128 &z2)
{
  const __m128 dot_product = _mm_add_ps (_mm_add_ps (_mm_mul_ps (x1, x2), _mm_mul_ps (y1, y2)), _mm_mul_ps (z1, z2));
  // The andnot-function realizes an abs-operation: the sign bit is removed
  // -0.0f (negative zero) means that all bits are 0, only the sign bit is 1
  return acos_SSE (_mm_min_ps (_mm_set1_ps (1.0f), _mm_andnot_ps (_mm_set1_ps (-0.0f), dot_product)));
}
#endif // ifdef __SSE__

#ifdef __AVX__
inline __m256
pcl::acos_AVX (const __m256 &x)
{
  const __m256 mul_term = _mm256_add_ps (_mm256_set1_ps (1.59121552f), _mm256_mul_ps (x, _mm256_add_ps (_mm256_set1_ps (-0.15461442f), _mm256_mul_ps (x, _mm256_set1_ps (0.05354897f)))));
  const __m256 add_term = _mm256_add_ps (_mm256_set1_ps (0.06681017f), _mm256_mul_ps (x, _mm256_add_ps (_mm256_set1_ps (-0.09402311f), _mm256_mul_ps (x, _mm256_set1_ps (0.02708663f)))));
  return _mm256_add_ps (_mm256_mul_ps (mul_term, _mm256_sqrt_ps (_mm256_add_ps (_mm256_set1_ps (0.89286965f), _mm256_mul_ps (_mm256_set1_ps (-0.89282669f), x)))), add_term);
}

inline __m256
pcl::getAcuteAngle3DAVX (const __m256 &x1, const __m256 &y1, const __m256 &z1, const __m256 &x2, const __m256 &y2, const __m256 &z2)
{
  const __m256 dot_product = _mm256_add_ps (_mm256_add_ps (_mm256_mul_ps (x1, x2), _mm256_mul_ps (y1, y2)), _mm256_mul_ps (z1, z2));
  // The andnot-function realizes an abs-operation: the sign bit is removed
  // -0.0f (negative zero) means that all bits are 0, only the sign bit is 1
  return acos_AVX (_mm256_min_ps (_mm256_set1_ps (1.0f), _mm256_andnot_ps (_mm256_set1_ps (-0.0f), dot_product)));
}
#endif // ifdef __AVX__

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <pcl/rvv_point_load.h>
#endif

///////////////////////////////////////////////////////////////////////////
/** \brief Scalar kernel: compute sum and sum-of-squares of data[0..n-1]. Used by getMeanStd. */
inline void
getMeanStdKernelStandard (const float* data, std::size_t n, double& sum, double& sq_sum)
{
  sum = 0;
  sq_sum = 0;
  for (std::size_t i = 0; i < n; ++i)
  {
    const float v = data[i];
    sum += v;
    sq_sum += static_cast<double>(v) * v;
  }
}

#if defined(__RVV10__)
/** \brief RVV kernel: compute sum and sum-of-squares of data[0..n-1].
 *
 * \note Precision: accumulation uses \c float (vfloat32m2) and only the final
 *       sums are cast to \c double. The scalar path (\ref getMeanStdKernelStandard)
 *       accumulates in \c double from each \c float sample, so results may differ.
 */
inline void
getMeanStdKernelRVV (const float* data, std::size_t n, double& sum, double& sq_sum)
{
  sum = 0;
  sq_sum = 0;
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc_sum = __riscv_vfmv_v_f_f32m2 (0.0f, max_vl);
  vfloat32m2_t v_acc_sq  = __riscv_vfmv_v_f_f32m2 (0.0f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t v = __riscv_vle32_v_f32m2 (data + i, vl);
    v_acc_sum = __riscv_vfadd_vv_f32m2_tu (v_acc_sum, v_acc_sum, v, vl);
    v_acc_sq  = __riscv_vfmacc_vv_f32m2_tu (v_acc_sq, v, v, vl);
    i += vl;
  }
  vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.0f, 1);
  vfloat32m1_t v_sum  = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc_sum, v_zero, max_vl);
  vfloat32m1_t v_sq   = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc_sq,  v_zero, max_vl);
  sum    = static_cast<double>(__riscv_vfmv_f_s_f32m1_f32 (v_sum));
  sq_sum = static_cast<double>(__riscv_vfmv_f_s_f32m1_f32 (v_sq));
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
inline void
pcl::getMeanStd (const std::vector<float> &values, double &mean, double &stddev)
{
  // throw an exception when the input array is empty
  if (values.empty ())
  {
    PCL_THROW_EXCEPTION (BadArgumentException, "Input array must have at least 1 element.");
  }

  // when the array has only one element, mean is the number itself and standard dev is 0
  if (values.size () == 1)
  {
    mean = values.at (0);
    stddev = 0;
    return;
  }

  double sum = 0, sq_sum = 0;
#if defined(__RVV10__)
  getMeanStdKernelRVV (values.data (), values.size (), sum, sq_sum);
#else
  getMeanStdKernelStandard (values.data (), values.size (), sum, sq_sum);
#endif
  mean = sum / static_cast<double>(values.size ());
  double variance = (sq_sum - sum * sum / static_cast<double>(values.size ())) / (static_cast<double>(values.size ()) - 1);
  stddev = sqrt (variance);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// getPointsInBox: __RVV10__ 时 getPointsInBoxRVV（dense 且 n≥16 为向量条带，否则标量），否则 getPointsInBoxStandard。
/** \brief Scalar path for getPointsInBox. Not for direct use; see pcl::getPointsInBox. */
template <typename PointT>
inline int
getPointsInBoxStandard (const pcl::PointCloud<PointT> &cloud,
                        Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt,
                        pcl::Indices &indices)
{
  if (cloud.is_dense)
  {
    int l = 0;
    for (std::size_t i = 0; i < cloud.size (); ++i)
    {
      if (cloud[i].x < min_pt[0] || cloud[i].y < min_pt[1] || cloud[i].z < min_pt[2])
        continue;
      if (cloud[i].x > max_pt[0] || cloud[i].y > max_pt[1] || cloud[i].z > max_pt[2])
        continue;
      indices[l++] = static_cast<pcl::index_t>(i);
    }
    return l;
  }
  int l = 0;
  for (std::size_t i = 0; i < cloud.size (); ++i)
  {
    if (!std::isfinite (cloud[i].x) ||
        !std::isfinite (cloud[i].y) ||
        !std::isfinite (cloud[i].z))
      continue;
    if (cloud[i].x < min_pt[0] || cloud[i].y < min_pt[1] || cloud[i].z < min_pt[2])
      continue;
    if (cloud[i].x > max_pt[0] || cloud[i].y > max_pt[1] || cloud[i].z > max_pt[2])
      continue;
    indices[l++] = static_cast<pcl::index_t>(i);
  }
  return l;
}

#if defined(__RVV10__)
/** \brief RVV path: dense and n≥16 uses strip-mine + vcompress; else getPointsInBoxStandard. */
template <typename PointT>
inline int
getPointsInBoxRVV (const pcl::PointCloud<PointT> &cloud,
                   Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt,
                   pcl::Indices &indices)
{
  const std::size_t n = cloud.size ();
  if (!cloud.is_dense || n < 16)
    return getPointsInBoxStandard (cloud, min_pt, max_pt, indices);

  const uint8_t* base = reinterpret_cast<const uint8_t*>(cloud.data ());
  const float min_x = min_pt[0], min_y = min_pt[1], min_z = min_pt[2];
  const float max_x = max_pt[0], max_y = max_pt[1], max_z = max_pt[2];
  int l = 0;
  std::size_t i = 0;
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const uint8_t* chunk = base + i * sizeof (PointT);
    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof (PointT),
                                              offsetof (PointT, x),
                                              offsetof (PointT, y),
                                              offsetof (PointT, z)> (
        chunk, vl, vx, vy, vz);

    vbool16_t in_x = __riscv_vmfge_vf_f32m2_b16 (vx, min_x, vl);
    in_x = __riscv_vmand_mm_b16 (in_x, __riscv_vmfle_vf_f32m2_b16 (vx, max_x, vl), vl);
    vbool16_t in_y = __riscv_vmfge_vf_f32m2_b16 (vy, min_y, vl);
    in_y = __riscv_vmand_mm_b16 (in_y, __riscv_vmfle_vf_f32m2_b16 (vy, max_y, vl), vl);
    vbool16_t in_z = __riscv_vmfge_vf_f32m2_b16 (vz, min_z, vl);
    in_z = __riscv_vmand_mm_b16 (in_z, __riscv_vmfle_vf_f32m2_b16 (vz, max_z, vl), vl);
    vbool16_t mask = __riscv_vmand_mm_b16 (__riscv_vmand_mm_b16 (in_x, in_y, vl), in_z, vl);

    const vuint32m2_t vid = __riscv_vadd_vx_u32m2 (__riscv_vid_v_u32m2 (vl), static_cast<uint32_t> (i), vl);
    const vuint32m2_t compressed = __riscv_vcompress_vm_u32m2 (vid, mask, vl);
    const std::size_t cnt = __riscv_vcpop_m_b16 (mask, vl);
    if (cnt > 0)
    {
      // cnt <= vl <= VLMAX for this strip; vcompress packs the first cnt lanes.
      const std::size_t vl_store = __riscv_vsetvl_e32m2 (cnt);
      std::uint32_t* const out_u32 =
          reinterpret_cast<std::uint32_t*> (indices.data () + l);
      __riscv_vse32_v_u32m2 (out_u32, compressed, vl_store);
      l += static_cast<int>(cnt);
    }
    i += vl;
  }
  return l;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> inline void
pcl::getPointsInBox (const pcl::PointCloud<PointT> &cloud,
                     Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt,
                     Indices &indices)
{
  indices.resize (cloud.size ());
  int l;
#if defined(__RVV10__)
  l = getPointsInBoxRVV (cloud, min_pt, max_pt, indices);
#else
  l = getPointsInBoxStandard (cloud, min_pt, max_pt, indices);
#endif
  indices.resize (l);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// getMaxDistance: Standard = 原本实现 (getVector3fMap + .norm()). 分发: __RVV10__ 时 RVV，否则 Standard。
template<typename PointT>
inline void setMaxPt (const pcl::PointCloud<PointT> &cloud, int max_idx, Eigen::Vector4f &max_pt)
{
  if (max_idx != -1)
    max_pt = cloud[max_idx].getVector4fMap ();
  else
    max_pt = Eigen::Vector4f (std::numeric_limits<float>::quiet_NaN (), std::numeric_limits<float>::quiet_NaN (),
                              std::numeric_limits<float>::quiet_NaN (), std::numeric_limits<float>::quiet_NaN ());
}

template<typename PointT>
inline void setMaxPtFromIndices (const pcl::PointCloud<PointT> &cloud, const pcl::Indices &indices, int max_idx, Eigen::Vector4f &max_pt)
{
  if (max_idx != -1)
    max_pt = cloud[indices[static_cast<std::size_t>(max_idx)]].getVector4fMap ();
  else
    max_pt = Eigen::Vector4f (std::numeric_limits<float>::quiet_NaN (), std::numeric_limits<float>::quiet_NaN (),
                              std::numeric_limits<float>::quiet_NaN (), std::numeric_limits<float>::quiet_NaN ());
}

// Standard: L2 via getVector3fMap() and .norm()
template<typename PointT>
inline void getMaxDistanceStandard (const pcl::PointCloud<PointT> &cloud, const Eigen::Vector4f &pivot_pt, Eigen::Vector4f &max_pt)
{
  float max_dist = std::numeric_limits<float>::lowest();
  int max_idx = -1;
  float dist;
  const Eigen::Vector3f pivot_pt3 = pivot_pt.head<3>();

  if (cloud.is_dense)
  {
    for (std::size_t i = 0; i < cloud.size(); ++i)
    {
      pcl::Vector3fMapConst pt = cloud[i].getVector3fMap();
      dist = (pivot_pt3 - pt).norm();
      if (dist > max_dist)
      {
        max_idx = static_cast<int>(i);
        max_dist = dist;
      }
    }
  }
  else
  {
    for (std::size_t i = 0; i < cloud.size(); ++i)
    {
      if (!std::isfinite(cloud[i].x) || !std::isfinite(cloud[i].y) ||
          !std::isfinite(cloud[i].z))
        continue;
      pcl::Vector3fMapConst pt = cloud[i].getVector3fMap();
      dist = (pivot_pt3 - pt).norm();
      if (dist > max_dist)
      {
        max_idx = static_cast<int>(i);
        max_dist = dist;
      }
    }
  }
  setMaxPt(cloud, max_idx, max_pt);
}

// RVV: when __RVV10__ and dense and n>=16 use RVV, else getMaxDistanceStandard. L2²; argmax(L2)=argmax(L2²).
#if defined(__RVV10__)
template<typename PointT>
inline void getMaxDistanceRVV (const pcl::PointCloud<PointT> &cloud, const Eigen::Vector4f &pivot_pt, Eigen::Vector4f &max_pt)
{
  const float px = pivot_pt[0], py = pivot_pt[1], pz = pivot_pt[2];
  const std::size_t n = cloud.size ();

  if (!cloud.is_dense || n < 16)
  {
    getMaxDistanceStandard (cloud, pivot_pt, max_pt);
    return;
  }

  const uint8_t* base = reinterpret_cast<const uint8_t*>(cloud.data());
  float max_chunk = -1.0f;
  int idx_chunk = -1;
  std::size_t i = 0;
  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const uint8_t* chunk = base + i * sizeof (PointT);
    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof (PointT),
                                              offsetof (PointT, x),
                                              offsetof (PointT, y),
                                              offsetof (PointT, z)> (
        chunk, vl, vx, vy, vz);
    const vfloat32m2_t v_dx = __riscv_vfrsub_vf_f32m2(vx, px, vl);
    const vfloat32m2_t v_dy = __riscv_vfrsub_vf_f32m2(vy, py, vl);
    const vfloat32m2_t v_dz = __riscv_vfrsub_vf_f32m2(vz, pz, vl);
    const vfloat32m2_t v_d2 = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(v_dx, v_dx, vl), v_dy, v_dy, vl),
        v_dz,
        v_dz,
        vl);
    const vfloat32m1_t v_max =
        __riscv_vfredmax_vs_f32m2_f32m1(v_d2, __riscv_vfmv_s_f_f32m1(-1.0f, 1), vl);
    const float chunk_max = __riscv_vfmv_f_s_f32m1_f32(v_max);
    if (chunk_max > max_chunk) {
      const vfloat32m2_t v_broadcast = __riscv_vfmv_v_f_f32m2(chunk_max, vl);
      const vbool16_t mask = __riscv_vmfeq_vv_f32m2_b16(v_d2, v_broadcast, vl);
      const vuint32m2_t vid =
          __riscv_vadd_vx_u32m2(__riscv_vid_v_u32m2(vl), static_cast<uint32_t>(i), vl);
      const vuint32m2_t comp = __riscv_vcompress_vm_u32m2(vid, mask, vl);
      idx_chunk = static_cast<int>(__riscv_vmv_x_s_u32m2_u32(comp));
      max_chunk = chunk_max;
    }
    i += vl;
  }
  int max_idx = -1;
  if (idx_chunk >= 0)
    max_idx = idx_chunk;
  setMaxPt (cloud, max_idx, max_pt);
}
#endif

// Standard (indices version)
template<typename PointT>
inline void getMaxDistanceStandard (const pcl::PointCloud<PointT> &cloud, const pcl::Indices &indices,
                                        const Eigen::Vector4f &pivot_pt, Eigen::Vector4f &max_pt)
{
  float max_dist = std::numeric_limits<float>::lowest();
  int max_idx = -1;
  float dist;
  const Eigen::Vector3f pivot_pt3 = pivot_pt.head<3>();

  if (cloud.is_dense)
  {
    for (std::size_t i = 0; i < indices.size(); ++i)
    {
      pcl::Vector3fMapConst pt = cloud[indices[i]].getVector3fMap();
      dist = (pivot_pt3 - pt).norm();
      if (dist > max_dist)
      {
        max_idx = static_cast<int>(i);
        max_dist = dist;
      }
    }
  }
  else {
    for (std::size_t i = 0; i < indices.size(); ++i)
    {
      if (!std::isfinite(cloud[indices[i]].x) || !std::isfinite(cloud[indices[i]].y) ||
          !std::isfinite(cloud[indices[i]].z))
        continue;
      pcl::Vector3fMapConst pt = cloud[indices[i]].getVector3fMap();
      dist = (pivot_pt3 - pt).norm();
      if (dist > max_dist)
      {
        max_idx = static_cast<int>(i);
        max_dist = dist;
      }
    }
  }
  setMaxPtFromIndices(cloud, indices, max_idx, max_pt);
}

// RVV with gather for indices (when __RVV10__ and dense and n>=16)
#if defined(__RVV10__)
template<typename PointT>
inline void getMaxDistanceRVV (const pcl::PointCloud<PointT> &cloud, const pcl::Indices &indices,
                                         const Eigen::Vector4f &pivot_pt, Eigen::Vector4f &max_pt)
{
  const float px = pivot_pt[0], py = pivot_pt[1], pz = pivot_pt[2];
  const std::size_t n = indices.size ();

  // 与「is_dense → (n>=16 ? RVV : std) : std」等价；早退减少嵌套。
  if (!cloud.is_dense || n < 16)
  {
    getMaxDistanceStandard (cloud, indices, pivot_pt, max_pt);
    return;
  }

  const uint8_t* points_base = reinterpret_cast<const uint8_t*>(cloud.data ());
  const uint32_t* indices_ptr = reinterpret_cast<const uint32_t*>(indices.data ());
  float max_chunk = -1.0f;
  int idx_chunk = -1;
  std::size_t i = 0;
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vuint32m2_t v_idx = __riscv_vle32_v_u32m2 (indices_ptr + i, vl);
    const vuint32m2_t v_off = __riscv_vmul_vx_u32m2 (v_idx, sizeof (PointT), vl);
    const vfloat32m2_t vx = __riscv_vluxei32_v_f32m2 (
        reinterpret_cast<const float*>(points_base + offsetof (PointT, x)), v_off, vl);
    const vfloat32m2_t vy = __riscv_vluxei32_v_f32m2 (
        reinterpret_cast<const float*>(points_base + offsetof (PointT, y)), v_off, vl);
    const vfloat32m2_t vz = __riscv_vluxei32_v_f32m2 (
        reinterpret_cast<const float*>(points_base + offsetof (PointT, z)), v_off, vl);
    const vfloat32m2_t v_dx = __riscv_vfrsub_vf_f32m2 (vx, px, vl);
    const vfloat32m2_t v_dy = __riscv_vfrsub_vf_f32m2 (vy, py, vl);
    const vfloat32m2_t v_dz = __riscv_vfrsub_vf_f32m2 (vz, pz, vl);
    const vfloat32m2_t v_d2 = __riscv_vfmacc_vv_f32m2 (
        __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (v_dx, v_dx, vl), v_dy, v_dy, vl),
        v_dz,
        v_dz,
        vl);
    const vfloat32m1_t v_max =
        __riscv_vfredmax_vs_f32m2_f32m1 (v_d2, __riscv_vfmv_s_f_f32m1 (-1.0f, 1), vl);
    const float chunk_max = __riscv_vfmv_f_s_f32m1_f32 (v_max);
    if (chunk_max > max_chunk)
    {
      const vfloat32m2_t v_broadcast = __riscv_vfmv_v_f_f32m2 (chunk_max, vl);
      const vbool16_t mask = __riscv_vmfeq_vv_f32m2_b16 (v_d2, v_broadcast, vl);
      const vuint32m2_t vid =
          __riscv_vadd_vx_u32m2 (__riscv_vid_v_u32m2 (vl), static_cast<uint32_t> (i), vl);
      const vuint32m2_t comp = __riscv_vcompress_vm_u32m2 (vid, mask, vl);
      idx_chunk = static_cast<int>(__riscv_vmv_x_s_u32m2_u32 (comp));
      max_chunk = chunk_max;
    }
    i += vl;
  }
  int max_idx = -1;
  if (idx_chunk >= 0)
    max_idx = idx_chunk;
  setMaxPtFromIndices (cloud, indices, max_idx, max_pt);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> inline void
pcl::getMaxDistance (const pcl::PointCloud<PointT> &cloud, const Eigen::Vector4f &pivot_pt, Eigen::Vector4f &max_pt)
{
#if defined(__RVV10__)
  getMaxDistanceRVV (cloud, pivot_pt, max_pt);
#else
  getMaxDistanceStandard (cloud, pivot_pt, max_pt);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointT> inline void
pcl::getMaxDistance (const pcl::PointCloud<PointT> &cloud, const Indices &indices,
                     const Eigen::Vector4f &pivot_pt, Eigen::Vector4f &max_pt)
{
#if defined(__RVV10__)
  getMaxDistanceRVV (cloud, indices, pivot_pt, max_pt);
#else
  getMaxDistanceStandard (cloud, indices, pivot_pt, max_pt);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////
// getMinMax3D: Standard = 原本实现 (getVector4fMap + cwiseMin/cwiseMax). 分发: __RVV10__ 时 RVV，否则 Standard。
// Standard: 上游 PCL 原实现; RVV: __RVV10__ 且 dense 且 n>=16 时 vfredmin/vfredmax。
// Standard (original implementation from upstream PCL)
template <typename PointT>
inline void getMinMax3DStandard (const pcl::PointCloud<PointT> &cloud, Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt)
{
  min_pt.setConstant (std::numeric_limits<float>::max ());
  max_pt.setConstant (std::numeric_limits<float>::lowest ());
  if (cloud.is_dense)
  {
    for (const auto& point : cloud.points)
    {
      const pcl::Vector4fMapConst pt = point.getVector4fMap ();
      min_pt = min_pt.cwiseMin (pt);
      max_pt = max_pt.cwiseMax (pt);
    }
  }
  else
  {
    for (const auto& point : cloud.points)
    {
      if (!std::isfinite (point.x) || !std::isfinite (point.y) || !std::isfinite (point.z))
        continue;
      const pcl::Vector4fMapConst pt = point.getVector4fMap ();
      min_pt = min_pt.cwiseMin (pt);
      max_pt = max_pt.cwiseMax (pt);
    }
  }
}

template <typename PointT>
inline void getMinMax3DStandard (const pcl::PointCloud<PointT> &cloud, const pcl::Indices &indices,
                                Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt)
{
  min_pt.setConstant (std::numeric_limits<float>::max ());
  max_pt.setConstant (std::numeric_limits<float>::lowest ());
  if (cloud.is_dense)
  {
    for (const auto &index : indices)
    {
      const pcl::Vector4fMapConst pt = cloud[index].getVector4fMap ();
      min_pt = min_pt.cwiseMin (pt);
      max_pt = max_pt.cwiseMax (pt);
    }
  }
  else
  {
    for (const auto &index : indices)
    {
      if (!std::isfinite (cloud[index].x) || !std::isfinite (cloud[index].y) || !std::isfinite (cloud[index].z))
        continue;
      const pcl::Vector4fMapConst pt = cloud[index].getVector4fMap ();
      min_pt = min_pt.cwiseMin (pt);
      max_pt = max_pt.cwiseMax (pt);
    }
  }
}

#if defined(__RVV10__)
template <typename PointT>
inline void getMinMax3DRVV (const pcl::PointCloud<PointT> &cloud, Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt)
{
  const std::size_t n = cloud.size ();

  if (!cloud.is_dense || n < 16)
  {
    getMinMax3DStandard (cloud, min_pt, max_pt);
    return;
  }

  const std::size_t vlmax = __riscv_vsetvl_e32m2 (static_cast<std::size_t> (-1));
  const float init_min = std::numeric_limits<float>::max ();
  const float init_max = std::numeric_limits<float>::lowest ();
  const uint8_t* base = reinterpret_cast<const uint8_t*>(cloud.data ());

  // 循环内按 lane 做 vfmin/vfmax 归并；_tu 避免末段 vl<vlmax 时 TA 破坏高 lane 上已有极值。循环外各维一次 vfred。
  vfloat32m2_t v_acc_min_x = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
  vfloat32m2_t v_acc_min_y = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
  vfloat32m2_t v_acc_min_z = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
  vfloat32m2_t v_acc_max_x = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);
  vfloat32m2_t v_acc_max_y = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);
  vfloat32m2_t v_acc_max_z = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);

  std::size_t i = 0;
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const uint8_t* chunk = base + i * sizeof (PointT);
    vfloat32m2_t vx;
    vfloat32m2_t vy;
    vfloat32m2_t vz;
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof (PointT),
                                              offsetof (PointT, x),
                                              offsetof (PointT, y),
                                              offsetof (PointT, z)> (
        chunk, vl, vx, vy, vz);
    v_acc_min_x = __riscv_vfmin_vv_f32m2_tu (v_acc_min_x, v_acc_min_x, vx, vl);
    v_acc_min_y = __riscv_vfmin_vv_f32m2_tu (v_acc_min_y, v_acc_min_y, vy, vl);
    v_acc_min_z = __riscv_vfmin_vv_f32m2_tu (v_acc_min_z, v_acc_min_z, vz, vl);
    v_acc_max_x = __riscv_vfmax_vv_f32m2_tu (v_acc_max_x, v_acc_max_x, vx, vl);
    v_acc_max_y = __riscv_vfmax_vv_f32m2_tu (v_acc_max_y, v_acc_max_y, vy, vl);
    v_acc_max_z = __riscv_vfmax_vv_f32m2_tu (v_acc_max_z, v_acc_max_z, vz, vl);
    i += vl;
  }

  const vfloat32m1_t red_min_seed = __riscv_vfmv_s_f_f32m1 (init_min, 1);
  const vfloat32m1_t red_max_seed = __riscv_vfmv_s_f_f32m1 (init_max, 1);
  min_pt[0] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1 (v_acc_min_x, red_min_seed, vlmax));
  min_pt[1] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1 (v_acc_min_y, red_min_seed, vlmax));
  min_pt[2] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1 (v_acc_min_z, red_min_seed, vlmax));
  min_pt[3] = 0.0f;
  max_pt[0] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1 (v_acc_max_x, red_max_seed, vlmax));
  max_pt[1] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1 (v_acc_max_y, red_max_seed, vlmax));
  max_pt[2] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1 (v_acc_max_z, red_max_seed, vlmax));
  max_pt[3] = 0.0f;
}

template <typename PointT>
inline void getMinMax3DRVV (const pcl::PointCloud<PointT> &cloud, const pcl::Indices &indices,
                            Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt)
{
  const std::size_t n = indices.size ();

  if (!cloud.is_dense || n < 16)
  {
    getMinMax3DStandard (cloud, indices, min_pt, max_pt);
    return;
  }

  const uint8_t* points_base = reinterpret_cast<const uint8_t*>(cloud.data ());
  const uint32_t* indices_ptr = reinterpret_cast<const uint32_t*>(indices.data ());
  const std::size_t vlmax = __riscv_vsetvl_e32m2 (static_cast<std::size_t> (-1));
  const float init_min = std::numeric_limits<float>::max ();
  const float init_max = std::numeric_limits<float>::lowest ();

  // 同 dense 版；vfmin/vfmax 用 _tu 保证尾段不污染未参与本拍比较的 lane。
  vfloat32m2_t v_acc_min_x = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
  vfloat32m2_t v_acc_min_y = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
  vfloat32m2_t v_acc_min_z = __riscv_vfmv_v_f_f32m2 (init_min, vlmax);
  vfloat32m2_t v_acc_max_x = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);
  vfloat32m2_t v_acc_max_y = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);
  vfloat32m2_t v_acc_max_z = __riscv_vfmv_v_f_f32m2 (init_max, vlmax);
  std::size_t i = 0;
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vuint32m2_t v_idx = __riscv_vle32_v_u32m2 (indices_ptr + i, vl);
    const vuint32m2_t v_off = __riscv_vmul_vx_u32m2 (v_idx, sizeof (PointT), vl);
    const vfloat32m2_t vx = __riscv_vluxei32_v_f32m2 (
        reinterpret_cast<const float*>(points_base + offsetof (PointT, x)), v_off, vl);
    const vfloat32m2_t vy = __riscv_vluxei32_v_f32m2 (
        reinterpret_cast<const float*>(points_base + offsetof (PointT, y)), v_off, vl);
    const vfloat32m2_t vz = __riscv_vluxei32_v_f32m2 (
        reinterpret_cast<const float*>(points_base + offsetof (PointT, z)), v_off, vl);
    v_acc_min_x = __riscv_vfmin_vv_f32m2_tu (v_acc_min_x, v_acc_min_x, vx, vl);
    v_acc_min_y = __riscv_vfmin_vv_f32m2_tu (v_acc_min_y, v_acc_min_y, vy, vl);
    v_acc_min_z = __riscv_vfmin_vv_f32m2_tu (v_acc_min_z, v_acc_min_z, vz, vl);
    v_acc_max_x = __riscv_vfmax_vv_f32m2_tu (v_acc_max_x, v_acc_max_x, vx, vl);
    v_acc_max_y = __riscv_vfmax_vv_f32m2_tu (v_acc_max_y, v_acc_max_y, vy, vl);
    v_acc_max_z = __riscv_vfmax_vv_f32m2_tu (v_acc_max_z, v_acc_max_z, vz, vl);
    i += vl;
  }
  const vfloat32m1_t red_min_seed = __riscv_vfmv_s_f_f32m1 (init_min, 1);
  const vfloat32m1_t red_max_seed = __riscv_vfmv_s_f_f32m1 (init_max, 1);
  min_pt[0] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1 (v_acc_min_x, red_min_seed, vlmax));
  min_pt[1] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1 (v_acc_min_y, red_min_seed, vlmax));
  min_pt[2] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmin_vs_f32m2_f32m1 (v_acc_min_z, red_min_seed, vlmax));
  min_pt[3] = 0.0f;
  max_pt[0] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1 (v_acc_max_x, red_max_seed, vlmax));
  max_pt[1] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1 (v_acc_max_y, red_max_seed, vlmax));
  max_pt[2] = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredmax_vs_f32m2_f32m1 (v_acc_max_z, red_max_seed, vlmax));
  max_pt[3] = 0.0f;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> inline void
pcl::getMinMax3D (const pcl::PointCloud<PointT> &cloud, PointT &min_pt, PointT &max_pt)
{
  Eigen::Vector4f min_p, max_p;
  pcl::getMinMax3D (cloud, min_p, max_p);
  min_pt.x = min_p[0]; min_pt.y = min_p[1]; min_pt.z = min_p[2];
  max_pt.x = max_p[0]; max_pt.y = max_p[1]; max_pt.z = max_p[2];
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> inline void
pcl::getMinMax3D (const pcl::PointCloud<PointT> &cloud, Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt)
{
#if defined(__RVV10__)
  getMinMax3DRVV (cloud, min_pt, max_pt);
#else
  getMinMax3DStandard (cloud, min_pt, max_pt);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> inline void
pcl::getMinMax3D (const pcl::PointCloud<PointT> &cloud, const pcl::PointIndices &indices,
                  Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt)
{
  pcl::getMinMax3D (cloud, indices.indices, min_pt, max_pt);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> inline void
pcl::getMinMax3D (const pcl::PointCloud<PointT> &cloud, const Indices &indices,
                  Eigen::Vector4f &min_pt, Eigen::Vector4f &max_pt)
{
#if defined(__RVV10__)
  getMinMax3DRVV (cloud, indices, min_pt, max_pt);
#else
  getMinMax3DStandard (cloud, indices, min_pt, max_pt);
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> inline double
pcl::getCircumcircleRadius (const PointT &pa, const PointT &pb, const PointT &pc)
{
  Eigen::Vector4f p1 (pa.x, pa.y, pa.z, 0);
  Eigen::Vector4f p2 (pb.x, pb.y, pb.z, 0);
  Eigen::Vector4f p3 (pc.x, pc.y, pc.z, 0);

  double p2p1 = (p2 - p1).norm (), p3p2 = (p3 - p2).norm (), p1p3 = (p1 - p3).norm ();
  // Calculate the area of the triangle using Heron's formula
  // (https://en.wikipedia.org/wiki/Heron's_formula)
  double semiperimeter = (p2p1 + p3p2 + p1p3) / 2.0;
  double area = sqrt (semiperimeter * (semiperimeter - p2p1) * (semiperimeter - p3p2) * (semiperimeter - p1p3));
  // Compute the radius of the circumscribed circle
  return ((p2p1 * p3p2 * p1p3) / (4.0 * area));
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Based on a search in the current PCL tree, this function does not appear to
// be used inside the core library; Could it be a legacy helper tool for
// histogram-like point types that support [] operator access?
template <typename PointT> inline void
pcl::getMinMax (const PointT &histogram, int len, float &min_p, float &max_p)
{
  min_p = std::numeric_limits<float>::max();
  max_p = std::numeric_limits<float>::lowest();

  for (int i = 0; i < len; ++i)
  {
    min_p = (histogram[i] > min_p) ? min_p : histogram[i];
    max_p = (histogram[i] < max_p) ? max_p : histogram[i];
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
// calculatePolygonArea: Standard = 原本实现 (getVector3fMap + cross); RVV 当 __RVV10__ 且 n>=16 时 stride-load + 向量叉积归约；否则 Standard.
template <typename PointT>
inline float calculatePolygonAreaStandard (const pcl::PointCloud<PointT> &polygon)
{
  float area = 0.0f;
  const int num_points = static_cast<int>(polygon.size ());
  Eigen::Vector3f va, vb, res;
  res(0) = res(1) = res(2) = 0.0f;
  for (int i = 0; i < num_points; ++i)
  {
    int j = (i + 1) % num_points;
    va = polygon[i].getVector3fMap ();
    vb = polygon[j].getVector3fMap ();
    res += va.cross (vb);
  }
  area = res.norm ();
  return (area * 0.5f);
}

#if defined(__RVV10__)
template <typename PointT>
inline float calculatePolygonAreaRVV (const pcl::PointCloud<PointT> &polygon)
{
  const int num_points = static_cast<int>(polygon.size ());
  if (num_points < 16)
    return calculatePolygonAreaStandard (polygon);

  const std::size_t n = static_cast<std::size_t>(num_points);
  const uint8_t* base = reinterpret_cast<const uint8_t*>(polygon.data ());
  const std::size_t n_pairs = n - 1;
  const std::size_t vlmax = __riscv_vsetvl_e32m2 (static_cast<std::size_t> (-1));
  vfloat32m2_t v_acc_x = __riscv_vfmv_v_f_f32m2 (0.0f, vlmax);
  vfloat32m2_t v_acc_y = __riscv_vfmv_v_f_f32m2 (0.0f, vlmax);
  vfloat32m2_t v_acc_z = __riscv_vfmv_v_f_f32m2 (0.0f, vlmax);

  // Pairs (0,1)..(n-2,n-1): per-lane 累加后 vfredusum；vfadd 用 _tu 避免末段 vl<vlmax 时尾部 lane 被 TA 污染。
  std::size_t i = 0;
  while (i < n_pairs)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n_pairs - i);
    const uint8_t* chunk_a = base + i * sizeof (PointT);
    const uint8_t* chunk_b = base + (i + 1) * sizeof (PointT);
    vfloat32m2_t ax;
    vfloat32m2_t ay;
    vfloat32m2_t az;
    vfloat32m2_t bx;
    vfloat32m2_t by;
    vfloat32m2_t bz;
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof (PointT),
                                              offsetof (PointT, x),
                                              offsetof (PointT, y),
                                              offsetof (PointT, z)> (
        chunk_a, vl, ax, ay, az);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof (PointT),
                                              offsetof (PointT, x),
                                              offsetof (PointT, y),
                                              offsetof (PointT, z)> (
        chunk_b, vl, bx, by, bz);
    // vfmsac.vv：vd = vs1*vs2 - vd（规范正文，非 vd - vs1*vs2；后者为 vfnmsac）。用 vd=az*by 再 vfmsac(_, ay, bz) 得 ay*bz - az*by = (a×b)_x。
    const vfloat32m2_t cx = __riscv_vfmsac_vv_f32m2 (__riscv_vfmul_vv_f32m2 (az, by, vl), ay, bz, vl);
    const vfloat32m2_t cy = __riscv_vfmsac_vv_f32m2 (__riscv_vfmul_vv_f32m2 (ax, bz, vl), az, bx, vl);
    const vfloat32m2_t cz = __riscv_vfmsac_vv_f32m2 (__riscv_vfmul_vv_f32m2 (ay, bx, vl), ax, by, vl);
    v_acc_x = __riscv_vfadd_vv_f32m2_tu (v_acc_x, v_acc_x, cx, vl);
    v_acc_y = __riscv_vfadd_vv_f32m2_tu (v_acc_y, v_acc_y, cy, vl);
    v_acc_z = __riscv_vfadd_vv_f32m2_tu (v_acc_z, v_acc_z, cz, vl);
    i += vl;
  }

  vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.0f, 1);
  float rx = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredusum_vs_f32m2_f32m1 (v_acc_x, v_zero, vlmax));
  float ry = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredusum_vs_f32m2_f32m1 (v_acc_y, v_zero, vlmax));
  float rz = __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredusum_vs_f32m2_f32m1 (v_acc_z, v_zero, vlmax));

  // Last pair (n-1, 0)
  const float ax = polygon[n - 1].x, ay = polygon[n - 1].y, az = polygon[n - 1].z;
  const float bx = polygon[0].x, by = polygon[0].y, bz = polygon[0].z;
  rx += ay * bz - az * by;
  ry += az * bx - ax * bz;
  rz += ax * by - ay * bx;
  const float area = std::sqrt (rx * rx + ry * ry + rz * rz) * 0.5f;
  return area;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT> inline float
pcl::calculatePolygonArea (const pcl::PointCloud<PointT> &polygon)
{
#if defined(__RVV10__)
  return calculatePolygonAreaRVV (polygon);
#else
  return calculatePolygonAreaStandard (polygon);
#endif
}

#endif  //#ifndef PCL_COMMON_IMPL_H_
