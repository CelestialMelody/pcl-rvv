/*
 * SHOT normalization component diagnostic helper.
 *
 * 本文件只服务 test-rvv/features/shot 的 Phase 010。它把 production
 * `normalizeHistogram` 中的连续 descriptor（描述子）归一化拆成 test-only
 * same-chain（同构链路）参考和 RVV 候选，用来判断这段循环是否值得进入后续
 * production probe（生产探针）。这里不参与 PCL production 构建。
 */

#pragma once

#include <cmath>
#include <cstddef>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl_rvv_shot {

inline float
descriptorL2Norm(const float* values, const std::size_t length)
{
  double acc = 0.0;
  for (std::size_t i = 0; i < length; ++i)
    acc += static_cast<double>(values[i]) * static_cast<double>(values[i]);
  return static_cast<float>(std::sqrt(acc));
}

// 标量参考链路复刻 production `normalizeHistogram` 的两个循环：double 累加
// 平方和，sqrt 后转成 float norm，再逐元素除以 norm。
inline void
normalizeDescriptorScalar(float* values, const std::size_t length)
{
  double acc = 0.0;
  for (std::size_t i = 0; i < length; ++i)
    acc += static_cast<double>(values[i]) * static_cast<double>(values[i]);

  const float norm = static_cast<float>(std::sqrt(acc));
  for (std::size_t i = 0; i < length; ++i)
    values[i] /= norm;
}

#if defined(__RVV10__)
inline void
normalizeDescriptorRvvKernel(float* values, const std::size_t length)
{
  if (length == 0)
    return;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m1_t zero_m1 = __riscv_vfmv_v_f_f32m1(0.0f, 1);
  vfloat32m2_t acc = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);

  for (std::size_t i = 0; i < length;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(length - i);
    const vfloat32m2_t v = __riscv_vle32_v_f32m2(values + i, vl);
    acc = __riscv_vfmacc_vv_f32m2_tu(acc, v, v, vl);
    i += vl;
  }

  const float sum =
      __riscv_vfmv_f_s_f32m1_f32(__riscv_vfredosum_vs_f32m2_f32m1(acc, zero_m1, vlmax));
  const float inv_norm = 1.0f / std::sqrt(sum);

  for (std::size_t i = 0; i < length;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(length - i);
    const vfloat32m2_t v = __riscv_vle32_v_f32m2(values + i, vl);
    __riscv_vse32_v_f32m2(values + i, __riscv_vfmul_vf_f32m2(v, inv_norm, vl), vl);
    i += vl;
  }
}
#endif

inline void
normalizeDescriptorRVV(float* values, const std::size_t length)
{
#if defined(__RVV10__)
  normalizeDescriptorRvvKernel(values, length);
#else
  normalizeDescriptorScalar(values, length);
#endif
}

} // namespace pcl_rvv_shot
