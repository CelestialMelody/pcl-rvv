/*
 * 本文件做什么：
 * 这里放 DON（Difference of Normals，法线差分）topic 的 test-only
 * helper。它复刻 production `computeFeature()` 的逐点热点：small normal
 * 与 large normal 相减后乘 0.5，非有限输出 normal 置零，再写 curvature
 * 为三分量欧氏范数。
 *
 * 证据边界：
 * 这些 helper 不参与 PCL production（生产源码）构建；它们只为当前 topic
 * 提供 same-chain（同构链路）correctness、bench 和 asm attribution
 * （反汇编归属）证据。真实生产接入仍需要后续 PI 阶段。
 */

#pragma once

#include <pcl/point_types.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::don {

inline void
computeDoNScalar(const pcl::Normal* small,
                 const pcl::Normal* large,
                 pcl::Normal* output,
                 const std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i) {
    float nx = (small[i].normal_x - large[i].normal_x) * 0.5f;
    float ny = (small[i].normal_y - large[i].normal_y) * 0.5f;
    float nz = (small[i].normal_z - large[i].normal_z) * 0.5f;
    if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
      nx = 0.0f;
      ny = 0.0f;
      nz = 0.0f;
    }
    output[i].normal_x = nx;
    output[i].normal_y = ny;
    output[i].normal_z = nz;
    output[i].curvature = std::sqrt(nx * nx + ny * ny + nz * nz);
  }
}

inline void
computeDoNFiniteOnlyNoMaskScalar(const pcl::Normal* small,
                                 const pcl::Normal* large,
                                 pcl::Normal* output,
                                 const std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i) {
    const float nx = (small[i].normal_x - large[i].normal_x) * 0.5f;
    const float ny = (small[i].normal_y - large[i].normal_y) * 0.5f;
    const float nz = (small[i].normal_z - large[i].normal_z) * 0.5f;
    output[i].normal_x = nx;
    output[i].normal_y = ny;
    output[i].normal_z = nz;
    output[i].curvature = std::sqrt(nx * nx + ny * ny + nz * nz);
  }
}

inline void
computeDoNNoSqrtStoreZeroCurvatureScalar(const pcl::Normal* small,
                                         const pcl::Normal* large,
                                         pcl::Normal* output,
                                         const std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i) {
    float nx = (small[i].normal_x - large[i].normal_x) * 0.5f;
    float ny = (small[i].normal_y - large[i].normal_y) * 0.5f;
    float nz = (small[i].normal_z - large[i].normal_z) * 0.5f;
    if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
      nx = 0.0f;
      ny = 0.0f;
      nz = 0.0f;
    }
    output[i].normal_x = nx;
    output[i].normal_y = ny;
    output[i].normal_z = nz;
    output[i].curvature = 0.0f;
  }
}

inline void
computeDoNNormalOnlyScalar(const pcl::Normal* small,
                           const pcl::Normal* large,
                           pcl::Normal* output,
                           const std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i) {
    float nx = (small[i].normal_x - large[i].normal_x) * 0.5f;
    float ny = (small[i].normal_y - large[i].normal_y) * 0.5f;
    float nz = (small[i].normal_z - large[i].normal_z) * 0.5f;
    if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
      nx = 0.0f;
      ny = 0.0f;
      nz = 0.0f;
    }
    output[i].normal_x = nx;
    output[i].normal_y = ny;
    output[i].normal_z = nz;
  }
}

#if defined(__RVV10__)
inline void
computeDoNRvvKernel(const pcl::Normal* small,
                    const pcl::Normal* large,
                    pcl::Normal* output,
                    const std::size_t count)
{
  constexpr ptrdiff_t kStride = static_cast<ptrdiff_t>(sizeof(pcl::Normal));
  const float kMaxFinite = std::numeric_limits<float>::max();
  const std::uint8_t* small_bytes = reinterpret_cast<const std::uint8_t*>(small);
  const std::uint8_t* large_bytes = reinterpret_cast<const std::uint8_t*>(large);
  std::uint8_t* output_bytes = reinterpret_cast<std::uint8_t*>(output);

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const std::size_t byte_offset = i * sizeof(pcl::Normal);

    const float* sx = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* sy = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* sz = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    const float* lx = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* ly = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* lz = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_z));

    vfloat32m2_t vx = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sx, kStride, vl),
                               __riscv_vlse32_v_f32m2(lx, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vy = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sy, kStride, vl),
                               __riscv_vlse32_v_f32m2(ly, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vz = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sz, kStride, vl),
                               __riscv_vlse32_v_f32m2(lz, kStride, vl),
                               vl),
        0.5f,
        vl);

    vbool16_t finite = __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl), kMaxFinite, vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vy, vl), kMaxFinite, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vz, vl), kMaxFinite, vl), vl);

    const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
    vx = __riscv_vmerge_vvm_f32m2(zero, vx, finite, vl);
    vy = __riscv_vmerge_vvm_f32m2(zero, vy, finite, vl);
    vz = __riscv_vmerge_vvm_f32m2(zero, vz, finite, vl);

    vfloat32m2_t curvature = __riscv_vfmul_vv_f32m2(vx, vx, vl);
    curvature = __riscv_vfmacc_vv_f32m2(curvature, vy, vy, vl);
    curvature = __riscv_vfmacc_vv_f32m2(curvature, vz, vz, vl);
    curvature = __riscv_vfsqrt_v_f32m2(curvature, vl);

    float* ox = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    float* oy = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    float* oz = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    float* oc = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, curvature));
    __riscv_vsse32_v_f32m2(ox, kStride, vx, vl);
    __riscv_vsse32_v_f32m2(oy, kStride, vy, vl);
    __riscv_vsse32_v_f32m2(oz, kStride, vz, vl);
    __riscv_vsse32_v_f32m2(oc, kStride, curvature, vl);

    i += vl;
  }
}

inline void
computeDoNFiniteOnlyNoMaskRvvKernel(const pcl::Normal* small,
                                    const pcl::Normal* large,
                                    pcl::Normal* output,
                                    const std::size_t count)
{
  constexpr ptrdiff_t kStride = static_cast<ptrdiff_t>(sizeof(pcl::Normal));
  const std::uint8_t* small_bytes = reinterpret_cast<const std::uint8_t*>(small);
  const std::uint8_t* large_bytes = reinterpret_cast<const std::uint8_t*>(large);
  std::uint8_t* output_bytes = reinterpret_cast<std::uint8_t*>(output);

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const std::size_t byte_offset = i * sizeof(pcl::Normal);

    const float* sx = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* sy = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* sz = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    const float* lx = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* ly = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* lz = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_z));

    vfloat32m2_t vx = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sx, kStride, vl),
                               __riscv_vlse32_v_f32m2(lx, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vy = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sy, kStride, vl),
                               __riscv_vlse32_v_f32m2(ly, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vz = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sz, kStride, vl),
                               __riscv_vlse32_v_f32m2(lz, kStride, vl),
                               vl),
        0.5f,
        vl);

    vfloat32m2_t curvature = __riscv_vfmul_vv_f32m2(vx, vx, vl);
    curvature = __riscv_vfmacc_vv_f32m2(curvature, vy, vy, vl);
    curvature = __riscv_vfmacc_vv_f32m2(curvature, vz, vz, vl);
    curvature = __riscv_vfsqrt_v_f32m2(curvature, vl);

    float* ox = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    float* oy = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    float* oz = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    float* oc = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, curvature));
    __riscv_vsse32_v_f32m2(ox, kStride, vx, vl);
    __riscv_vsse32_v_f32m2(oy, kStride, vy, vl);
    __riscv_vsse32_v_f32m2(oz, kStride, vz, vl);
    __riscv_vsse32_v_f32m2(oc, kStride, curvature, vl);

    i += vl;
  }
}

inline void
computeDoNNoSqrtStoreZeroCurvatureRvvKernel(const pcl::Normal* small,
                                            const pcl::Normal* large,
                                            pcl::Normal* output,
                                            const std::size_t count)
{
  constexpr ptrdiff_t kStride = static_cast<ptrdiff_t>(sizeof(pcl::Normal));
  const float kMaxFinite = std::numeric_limits<float>::max();
  const std::uint8_t* small_bytes = reinterpret_cast<const std::uint8_t*>(small);
  const std::uint8_t* large_bytes = reinterpret_cast<const std::uint8_t*>(large);
  std::uint8_t* output_bytes = reinterpret_cast<std::uint8_t*>(output);

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const std::size_t byte_offset = i * sizeof(pcl::Normal);

    const float* sx = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* sy = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* sz = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    const float* lx = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* ly = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* lz = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_z));

    vfloat32m2_t vx = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sx, kStride, vl),
                               __riscv_vlse32_v_f32m2(lx, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vy = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sy, kStride, vl),
                               __riscv_vlse32_v_f32m2(ly, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vz = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sz, kStride, vl),
                               __riscv_vlse32_v_f32m2(lz, kStride, vl),
                               vl),
        0.5f,
        vl);

    vbool16_t finite = __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl), kMaxFinite, vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vy, vl), kMaxFinite, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vz, vl), kMaxFinite, vl), vl);

    const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
    vx = __riscv_vmerge_vvm_f32m2(zero, vx, finite, vl);
    vy = __riscv_vmerge_vvm_f32m2(zero, vy, finite, vl);
    vz = __riscv_vmerge_vvm_f32m2(zero, vz, finite, vl);

    float* ox = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    float* oy = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    float* oz = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    float* oc = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, curvature));
    __riscv_vsse32_v_f32m2(ox, kStride, vx, vl);
    __riscv_vsse32_v_f32m2(oy, kStride, vy, vl);
    __riscv_vsse32_v_f32m2(oz, kStride, vz, vl);
    __riscv_vsse32_v_f32m2(oc, kStride, zero, vl);

    i += vl;
  }
}

inline void
computeDoNNormalOnlyRvvKernel(const pcl::Normal* small,
                              const pcl::Normal* large,
                              pcl::Normal* output,
                              const std::size_t count)
{
  constexpr ptrdiff_t kStride = static_cast<ptrdiff_t>(sizeof(pcl::Normal));
  const float kMaxFinite = std::numeric_limits<float>::max();
  const std::uint8_t* small_bytes = reinterpret_cast<const std::uint8_t*>(small);
  const std::uint8_t* large_bytes = reinterpret_cast<const std::uint8_t*>(large);
  std::uint8_t* output_bytes = reinterpret_cast<std::uint8_t*>(output);

  for (std::size_t i = 0; i < count;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(count - i);
    const std::size_t byte_offset = i * sizeof(pcl::Normal);

    const float* sx = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* sy = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* sz = reinterpret_cast<const float*>(small_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    const float* lx = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    const float* ly = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    const float* lz = reinterpret_cast<const float*>(large_bytes + byte_offset + offsetof(pcl::Normal, normal_z));

    vfloat32m2_t vx = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sx, kStride, vl),
                               __riscv_vlse32_v_f32m2(lx, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vy = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sy, kStride, vl),
                               __riscv_vlse32_v_f32m2(ly, kStride, vl),
                               vl),
        0.5f,
        vl);
    vfloat32m2_t vz = __riscv_vfmul_vf_f32m2(
        __riscv_vfsub_vv_f32m2(__riscv_vlse32_v_f32m2(sz, kStride, vl),
                               __riscv_vlse32_v_f32m2(lz, kStride, vl),
                               vl),
        0.5f,
        vl);

    vbool16_t finite = __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vx, vl), kMaxFinite, vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vy, vl), kMaxFinite, vl), vl);
    finite = __riscv_vmand_mm_b16(
        finite, __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(vz, vl), kMaxFinite, vl), vl);

    const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
    vx = __riscv_vmerge_vvm_f32m2(zero, vx, finite, vl);
    vy = __riscv_vmerge_vvm_f32m2(zero, vy, finite, vl);
    vz = __riscv_vmerge_vvm_f32m2(zero, vz, finite, vl);

    float* ox = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_x));
    float* oy = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_y));
    float* oz = reinterpret_cast<float*>(output_bytes + byte_offset + offsetof(pcl::Normal, normal_z));
    __riscv_vsse32_v_f32m2(ox, kStride, vx, vl);
    __riscv_vsse32_v_f32m2(oy, kStride, vy, vl);
    __riscv_vsse32_v_f32m2(oz, kStride, vz, vl);

    i += vl;
  }
}
#endif

inline void
computeDoNRVV(const pcl::Normal* small,
              const pcl::Normal* large,
              pcl::Normal* output,
              const std::size_t count)
{
#if defined(__RVV10__)
  computeDoNRvvKernel(small, large, output, count);
#else
  computeDoNScalar(small, large, output, count);
#endif
}

inline void
computeDoNFiniteOnlyNoMask(const pcl::Normal* small,
                           const pcl::Normal* large,
                           pcl::Normal* output,
                           const std::size_t count)
{
#if defined(__RVV10__)
  computeDoNFiniteOnlyNoMaskRvvKernel(small, large, output, count);
#else
  computeDoNFiniteOnlyNoMaskScalar(small, large, output, count);
#endif
}

inline void
computeDoNNoSqrtStoreZeroCurvature(const pcl::Normal* small,
                                   const pcl::Normal* large,
                                   pcl::Normal* output,
                                   const std::size_t count)
{
#if defined(__RVV10__)
  computeDoNNoSqrtStoreZeroCurvatureRvvKernel(small, large, output, count);
#else
  computeDoNNoSqrtStoreZeroCurvatureScalar(small, large, output, count);
#endif
}

inline void
computeDoNNormalOnly(const pcl::Normal* small,
                     const pcl::Normal* large,
                     pcl::Normal* output,
                     const std::size_t count)
{
#if defined(__RVV10__)
  computeDoNNormalOnlyRvvKernel(small, large, output, count);
#else
  computeDoNNormalOnlyScalar(small, large, output, count);
#endif
}

} // namespace pcl::features::rvv_test::don
