/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010, Willow Garage, Inc.
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
 *  FOR a PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
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

#include <pcl/common/norms.h>
#include <pcl/console/print.h>
#include <pcl/pcl_macros.h>

#include <cmath>
#include <type_traits>
#include <vector>

#if defined(__RVV10__)
#include <cstddef>
#include <riscv_vector.h>
#include <pcl/common/impl/rvv_math.hpp> // for pcl::logf_RVV_f32m2
#endif


namespace pcl
{
namespace detail
{
template <typename T>
struct is_std_vector_float : std::false_type
{};
template <typename Alloc>
struct is_std_vector_float<std::vector<float, Alloc>> : std::true_type
{};

/** \brief True for float* / const float* or std::vector<float,...> (contiguous float). */
template <typename T>
inline constexpr bool kNormRvvContiguousFloatV =
    (std::is_pointer_v<std::decay_t<T>> &&
     std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>, float>) ||
    is_std_vector_float<std::decay_t<T>>::value;

template <typename T>
inline const float*
norm_contiguous_float_data (const T& a) noexcept
{
  if constexpr (std::is_pointer_v<std::decay_t<T>>)
    return a;
  else
    return a.data ();
}

} // namespace detail

#if defined(__RVV10__)
/** \brief Below this length, RVV paths delegate to \c L*_Norm_Std (strip overhead). */
inline constexpr int kNormRvvMinDim = 16;
#endif

///////////////////////////////////////////////////////////////////////////////////////////////

/** \brief Scalar L1 (non-RVV TU or non-contiguous \c FloatVectorT). */
template <typename FloatVectorT> inline float
L1_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.0f;
  for (int i = 0; i < dim; ++i)
    norm += std::abs (a[i] - b[i]);
  return norm;
}

#if defined(__RVV10__)
/** \brief RVV L1 for contiguous \c float buffers (\c __RVV10__ only). */
inline float
L1_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return L1_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vabs = __riscv_vfabs_v_f32m2 (vd, vl);
    v_acc = __riscv_vfadd_vv_f32m2_tu (v_acc, v_acc, vabs, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
L1_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return L1_Norm_RVV (detail::norm_contiguous_float_data (a),
                        detail::norm_contiguous_float_data (b), dim);
  else
    return L1_Norm_Std (a, b, dim);
#else
  return L1_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////

/** \brief Scalar squared L2. */
template <typename FloatVectorT> inline float
L2_Norm_SQR_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.0f;
  for (int i = 0; i < dim; ++i)
  {
    const float diff = a[i] - b[i];
    norm += diff * diff;
  }
  return norm;
}

#if defined(__RVV10__)
/** \brief RVV squared L2 for contiguous \c float buffers (\c __RVV10__ only). */
inline float
L2_Norm_SQR_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return L2_Norm_SQR_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    v_acc = __riscv_vfmacc_vv_f32m2_tu (v_acc, vd, vd, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
L2_Norm_SQR (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return L2_Norm_SQR_RVV (detail::norm_contiguous_float_data (a),
                            detail::norm_contiguous_float_data (b), dim);
  else
    return L2_Norm_SQR_Std (a, b, dim);
#else
  return L2_Norm_SQR_Std (a, b, dim);
#endif
}

template <typename FloatVectorT> inline float
L2_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
  return std::sqrt (L2_Norm_SQR (a, b, dim));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

/** \brief Scalar L∞. */
template <typename FloatVectorT> inline float
Linf_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.0f;
  for (int i = 0; i < dim; ++i)
    norm = (std::max) (std::abs (a[i] - b[i]), norm);
  return norm;
}

#if defined(__RVV10__)
/** \brief RVV L∞ for contiguous \c float buffers (\c __RVV10__ only). */
inline float
Linf_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return Linf_Norm_Std (a, b, dim);
  float m = 0.f;
  std::size_t i = 0;
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vabs = __riscv_vfabs_v_f32m2 (vd, vl);
    const vfloat32m1_t vinit = __riscv_vfmv_s_f_f32m1 (m, 1);
    const vfloat32m1_t vmaxm1 = __riscv_vfredmax_vs_f32m2_f32m1 (vabs, vinit, vl);
    m = __riscv_vfmv_f_s_f32m1_f32 (vmaxm1);
    i += vl;
  }
  return m;
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
Linf_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return Linf_Norm_RVV (detail::norm_contiguous_float_data (a),
                          detail::norm_contiguous_float_data (b), dim);
  else
    return Linf_Norm_Std (a, b, dim);
#else
  return Linf_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
JM_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float d = std::sqrt (a[i]) - std::sqrt (b[i]);
    norm += d * d;
  }
  return std::sqrt (norm);
}

#if defined(__RVV10__)
inline float
JM_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return JM_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t sa = __riscv_vfsqrt_v_f32m2 (va, vl);
    const vfloat32m2_t sb = __riscv_vfsqrt_v_f32m2 (vb, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (sa, sb, vl);
    v_acc = __riscv_vfmacc_vv_f32m2_tu (v_acc, vd, vd, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return std::sqrt (__riscv_vfmv_f_s_f32m1_f32 (v_sum));
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
JM_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return JM_Norm_RVV (detail::norm_contiguous_float_data (a),
                        detail::norm_contiguous_float_data (b), dim);
  else
    return JM_Norm_Std (a, b, dim);
#else
  return JM_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
B_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
    norm += std::sqrt (a[i] * b[i]);
  if (norm > 0.f)
    return -std::log (norm);
  return 0.f;
}

#if defined(__RVV10__)
inline float
B_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return B_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vprod = __riscv_vfmul_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vt = __riscv_vfsqrt_v_f32m2 (vprod, vl);
    v_acc = __riscv_vfadd_vv_f32m2_tu (v_acc, v_acc, vt, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  const float norm = __riscv_vfmv_f_s_f32m1_f32 (v_sum);
  if (norm > 0.f)
    return -std::log (norm);
  return 0.f;
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
B_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return B_Norm_RVV (detail::norm_contiguous_float_data (a),
                       detail::norm_contiguous_float_data (b), dim);
  else
    return B_Norm_Std (a, b, dim);
#else
  return B_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
Sublinear_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
    norm += std::sqrt (std::abs (a[i] - b[i]));
  return norm;
}

#if defined(__RVV10__)
inline float
Sublinear_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return Sublinear_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vabs = __riscv_vfabs_v_f32m2 (vd, vl);
    const vfloat32m2_t vt = __riscv_vfsqrt_v_f32m2 (vabs, vl);
    v_acc = __riscv_vfadd_vv_f32m2_tu (v_acc, v_acc, vt, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
Sublinear_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return Sublinear_Norm_RVV (detail::norm_contiguous_float_data (a),
                              detail::norm_contiguous_float_data (b), dim);
  else
    return Sublinear_Norm_Std (a, b, dim);
#else
  return Sublinear_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
CS_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
    if ((a[i] + b[i]) != 0.f)
      norm += (a[i] - b[i]) * (a[i] - b[i]) / (a[i] + b[i]);
  return norm;
}

#if defined(__RVV10__)
inline float
CS_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return CS_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  const vfloat32m2_t v_zero = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vs = __riscv_vfadd_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vnum = __riscv_vfmul_vv_f32m2 (vd, vd, vl);
    const vbool16_t m_ne0 = __riscv_vmfne_vf_f32m2_b16 (vs, 0.f, vl);
    const vfloat32m2_t vterm = __riscv_vfdiv_vv_f32m2_mu (m_ne0, v_zero, vnum, vs, vl);
    v_acc = __riscv_vfadd_vv_f32m2_tu (v_acc, v_acc, vterm, vl);
    i += vl;
  }
  const vfloat32m1_t vz = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, vz, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
CS_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return CS_Norm_RVV (detail::norm_contiguous_float_data (a),
                         detail::norm_contiguous_float_data (b), dim);
  else
    return CS_Norm_Std (a, b, dim);
#else
  return CS_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
Div_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
    if ((a[i] / b[i]) > 0.f)
      norm += (a[i] - b[i]) * std::log (a[i] / b[i]);
  return norm;
}

#if defined(__RVV10__)
inline float
Div_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return Div_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vratio = __riscv_vfdiv_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vlog = pcl::logf_RVV_f32m2 (vratio, vl);
    const vfloat32m2_t vdiff = __riscv_vfsub_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vterm = __riscv_vfmul_vv_f32m2 (vdiff, vlog, vl);
    const vbool16_t mpos = __riscv_vmfgt_vf_f32m2_b16 (vratio, 0.0f, vl);
    v_acc = __riscv_vfadd_vv_f32m2_mu (mpos, v_acc, v_acc, vterm, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
Div_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return Div_Norm_RVV (detail::norm_contiguous_float_data (a),
                         detail::norm_contiguous_float_data (b), dim);
  else
    return Div_Norm_Std (a, b, dim);
#else
  return Div_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
PF_Norm_Std (FloatVectorT a, FloatVectorT b, int dim, float P1, float P2)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float d = P1 * a[i] - P2 * b[i];
    norm += d * d;
  }
  return std::sqrt (norm);
}

#if defined(__RVV10__)
inline float
PF_Norm_RVV (const float* a, const float* b, int dim, float P1, float P2)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return PF_Norm_Std (a, b, dim, P1, P2);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t t1 = __riscv_vfmul_vf_f32m2 (va, P1, vl);
    const vfloat32m2_t t2 = __riscv_vfmul_vf_f32m2 (vb, P2, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (t1, t2, vl);
    v_acc = __riscv_vfmacc_vv_f32m2_tu (v_acc, vd, vd, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return std::sqrt (__riscv_vfmv_f_s_f32m1_f32 (v_sum));
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
PF_Norm (FloatVectorT a, FloatVectorT b, int dim, float P1, float P2)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return PF_Norm_RVV (detail::norm_contiguous_float_data (a),
                        detail::norm_contiguous_float_data (b), dim, P1, P2);
  else
    return PF_Norm_Std (a, b, dim, P1, P2);
#else
  return PF_Norm_Std (a, b, dim, P1, P2);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
K_Norm_Std (FloatVectorT a, FloatVectorT b, int dim, float P1, float P2)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
    norm += std::abs (P1 * a[i] - P2 * b[i]);
  return norm;
}

#if defined(__RVV10__)
inline float
K_Norm_RVV (const float* a, const float* b, int dim, float P1, float P2)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return K_Norm_Std (a, b, dim, P1, P2);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t t1 = __riscv_vfmul_vf_f32m2 (va, P1, vl);
    const vfloat32m2_t t2 = __riscv_vfmul_vf_f32m2 (vb, P2, vl);
    const vfloat32m2_t vd = __riscv_vfsub_vv_f32m2 (t1, t2, vl);
    const vfloat32m2_t vabs = __riscv_vfabs_v_f32m2 (vd, vl);
    v_acc = __riscv_vfadd_vv_f32m2_tu (v_acc, v_acc, vabs, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
K_Norm (FloatVectorT a, FloatVectorT b, int dim, float P1, float P2)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return K_Norm_RVV (detail::norm_contiguous_float_data (a),
                       detail::norm_contiguous_float_data (b), dim, P1, P2);
  else
    return K_Norm_Std (a, b, dim, P1, P2);
#else
  return K_Norm_Std (a, b, dim, P1, P2);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
KL_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
    if ((b[i] != 0.f) && ((a[i] / b[i]) > 0.f))
      norm += a[i] * std::log (a[i] / b[i]);
  return norm;
}

#if defined(__RVV10__)
inline float
KL_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return KL_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vratio = __riscv_vfdiv_vv_f32m2 (va, vb, vl);
    const vfloat32m2_t vlog = pcl::logf_RVV_f32m2 (vratio, vl);
    const vfloat32m2_t vterm = __riscv_vfmul_vv_f32m2 (va, vlog, vl);
    const vbool16_t bnz = __riscv_vmfne_vf_f32m2_b16 (vb, 0.0f, vl);
    const vbool16_t rpos = __riscv_vmfgt_vf_f32m2_b16 (vratio, 0.0f, vl);
    const vbool16_t m = __riscv_vmand_mm_b16 (bnz, rpos, vl);
    v_acc = __riscv_vfadd_vv_f32m2_mu (m, v_acc, v_acc, vterm, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
KL_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return KL_Norm_RVV (detail::norm_contiguous_float_data (a),
                        detail::norm_contiguous_float_data (b), dim);
  else
    return KL_Norm_Std (a, b, dim);
#else
  return KL_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
HIK_Norm_Std (FloatVectorT a, FloatVectorT b, int dim)
{
  float norm = 0.f;
  for (int i = 0; i < dim; ++i)
    norm += (std::min) (a[i], b[i]);
  return norm;
}

#if defined(__RVV10__)
inline float
HIK_Norm_RVV (const float* a, const float* b, int dim)
{
  if (dim <= 0)
    return 0.f;
  const std::size_t n = static_cast<std::size_t> (dim);
  if (n < static_cast<std::size_t> (kNormRvvMinDim))
    return HIK_Norm_Std (a, b, dim);
  std::size_t i = 0;
  const std::size_t max_vl = __riscv_vsetvl_e32m2 (n);
  vfloat32m2_t v_acc = __riscv_vfmv_v_f_f32m2 (0.f, max_vl);
  while (i < n)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (n - i);
    const vfloat32m2_t va = __riscv_vle32_v_f32m2 (a + i, vl);
    const vfloat32m2_t vb = __riscv_vle32_v_f32m2 (b + i, vl);
    const vfloat32m2_t vm = __riscv_vfmin_vv_f32m2 (va, vb, vl);
    v_acc = __riscv_vfadd_vv_f32m2_tu (v_acc, v_acc, vm, vl);
    i += vl;
  }
  const vfloat32m1_t v_zero = __riscv_vfmv_s_f_f32m1 (0.f, 1);
  const vfloat32m1_t v_sum = __riscv_vfredosum_vs_f32m2_f32m1 (v_acc, v_zero, max_vl);
  return __riscv_vfmv_f_s_f32m1_f32 (v_sum);
}
#endif // __RVV10__

template <typename FloatVectorT> inline float
HIK_Norm (FloatVectorT a, FloatVectorT b, int dim)
{
#if defined(__RVV10__)
  if constexpr (detail::kNormRvvContiguousFloatV<FloatVectorT>)
    return HIK_Norm_RVV (detail::norm_contiguous_float_data (a),
                        detail::norm_contiguous_float_data (b), dim);
  else
    return HIK_Norm_Std (a, b, dim);
#else
  return HIK_Norm_Std (a, b, dim);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename FloatVectorT> inline float
selectNorm (FloatVectorT a, FloatVectorT b, int dim, NormType norm_type)
{
  // {L1, L2_SQR, L2, LINF, JM, B, SUBLINEAR, CS, DIV, PF, K, KL, HIK};
  switch (norm_type)
  {
    case (L1):
      return L1_Norm (a, b, dim);
    case (L2_SQR):
      return L2_Norm_SQR (a, b, dim);
    case (L2):
      return L2_Norm (a, b, dim);
    case (LINF):
      return Linf_Norm (a, b, dim);
    case (JM):
      return JM_Norm (a, b, dim);
    case (B):
      return B_Norm (a, b, dim);
    case (SUBLINEAR):
      return Sublinear_Norm (a, b, dim);
    case (CS):
      return CS_Norm (a, b, dim);
    case (DIV):
      return Div_Norm (a, b, dim);
    case (KL):
      return KL_Norm (a, b, dim);
    case (HIK):
      return HIK_Norm (a, b, dim);

    case (PF):
    case (K):
    default:
      PCL_ERROR ("[pcl::selectNorm] For PF and K norms you have to explicitly call the method, as they need additional parameters\n");
      return -1;
  }
}

} // namespace pcl
