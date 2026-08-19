/*
 * 本文件负责 gicp topic 的 RVV candidate（候选实现）。候选只接管
 * residual / Mahalanobis dense-row 累加和 covariance post-KNN 这两个
 * test-only 组件；其余入口、KdTree、SVD 和 Newton solver 保持在证据边界外。
 *
 * 证据边界：
 * RVV 路径在 `__RVV10__` 下用 intrinsic（内建函数）执行向量分块和规约。
 * 该路径会改变浮点规约顺序，因此 correctness（正确性）测试使用小误差预算，
 * 不能要求逐位相同。非 RVV 构建直接走标量 reference，证明 fallback 形状。
 */

#pragma once

#include "gicp_references.hpp"

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_gicp_support {

#ifdef __RVV10__
inline double
reduce_sum(vfloat64m1_t values, const std::size_t vl)
{
  vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vl);
  vfloat64m1_t reduced = __riscv_vfredusum_vs_f64m1_f64m1(values, zero, vl);
  return __riscv_vfmv_f_s_f64m1_f64(reduced);
}

inline double
reduce_sum_m4(vfloat64m4_t values, const std::size_t vl)
{
  vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1(0.0, vl);
  vfloat64m1_t reduced = __riscv_vfredusum_vs_f64m4_f64m1(values, zero, vl);
  return __riscv_vfmv_f_s_f64m1_f64(reduced);
}

inline vfloat64m1_t
apply3x4_v(const std::array<double, 12>& t,
           const int row,
           vfloat64m1_t x,
           vfloat64m1_t y,
           vfloat64m1_t z,
           const std::size_t vl)
{
  const std::size_t r = static_cast<std::size_t>(row) * 4u;
  vfloat64m1_t out = __riscv_vfmv_v_f_f64m1(t[r + 3u], vl);
  out = __riscv_vfmacc_vf_f64m1(out, t[r], x, vl);
  out = __riscv_vfmacc_vf_f64m1(out, t[r + 1u], y, vl);
  out = __riscv_vfmacc_vf_f64m1(out, t[r + 2u], z, vl);
  return out;
}

inline vfloat64m4_t
apply3x4_v_m4(const std::array<double, 12>& t,
              const int row,
              vfloat64m4_t x,
              vfloat64m4_t y,
              vfloat64m4_t z,
              const std::size_t vl)
{
  const std::size_t r = static_cast<std::size_t>(row) * 4u;
  vfloat64m4_t out = __riscv_vfmv_v_f_f64m4(t[r + 3u], vl);
  out = __riscv_vfmacc_vf_f64m4(out, t[r], x, vl);
  out = __riscv_vfmacc_vf_f64m4(out, t[r + 1u], y, vl);
  out = __riscv_vfmacc_vf_f64m4(out, t[r + 2u], z, vl);
  return out;
}
#endif

inline ResidualResult
residual_accumulate_candidate(const ResidualInput& input)
{
#ifndef __RVV10__
  return residual_accumulate_std(input);
#else
  ResidualResult result;
  const std::size_t n = input.rows;
  if (n == 0)
    return result;

  double f = 0.0;
  double gx = 0.0;
  double gy = 0.0;
  double gz = 0.0;
  double dcost[9] = {};

  for (std::size_t i = 0; i < n;) {
    const std::size_t vl = __riscv_vsetvl_e64m4(n - i);
    const double* sxp = input.src_x.data() + i;
    const double* syp = input.src_y.data() + i;
    const double* szp = input.src_z.data() + i;
    vfloat64m4_t sx = __riscv_vle64_v_f64m4(sxp, vl);
    vfloat64m4_t sy = __riscv_vle64_v_f64m4(syp, vl);
    vfloat64m4_t sz = __riscv_vle64_v_f64m4(szp, vl);

    vfloat64m4_t tx = apply3x4_v_m4(input.transform, 0, sx, sy, sz, vl);
    vfloat64m4_t ty = apply3x4_v_m4(input.transform, 1, sx, sy, sz, vl);
    vfloat64m4_t tz = apply3x4_v_m4(input.transform, 2, sx, sy, sz, vl);
    vfloat64m4_t dx =
        __riscv_vfsub_vv_f64m4(tx, __riscv_vle64_v_f64m4(input.tgt_x.data() + i, vl), vl);
    vfloat64m4_t dy =
        __riscv_vfsub_vv_f64m4(ty, __riscv_vle64_v_f64m4(input.tgt_y.data() + i, vl), vl);
    vfloat64m4_t dz =
        __riscv_vfsub_vv_f64m4(tz, __riscv_vle64_v_f64m4(input.tgt_z.data() + i, vl), vl);

    vfloat64m4_t mdx =
        __riscv_vfmul_vv_f64m4(__riscv_vle64_v_f64m4(input.m00.data() + i, vl), dx, vl);
    mdx = __riscv_vfmacc_vv_f64m4(
        mdx, __riscv_vle64_v_f64m4(input.m01.data() + i, vl), dy, vl);
    mdx = __riscv_vfmacc_vv_f64m4(
        mdx, __riscv_vle64_v_f64m4(input.m02.data() + i, vl), dz, vl);

    vfloat64m4_t mdy =
        __riscv_vfmul_vv_f64m4(__riscv_vle64_v_f64m4(input.m10.data() + i, vl), dx, vl);
    mdy = __riscv_vfmacc_vv_f64m4(
        mdy, __riscv_vle64_v_f64m4(input.m11.data() + i, vl), dy, vl);
    mdy = __riscv_vfmacc_vv_f64m4(
        mdy, __riscv_vle64_v_f64m4(input.m12.data() + i, vl), dz, vl);

    vfloat64m4_t mdz =
        __riscv_vfmul_vv_f64m4(__riscv_vle64_v_f64m4(input.m20.data() + i, vl), dx, vl);
    mdz = __riscv_vfmacc_vv_f64m4(
        mdz, __riscv_vle64_v_f64m4(input.m21.data() + i, vl), dy, vl);
    mdz = __riscv_vfmacc_vv_f64m4(
        mdz, __riscv_vle64_v_f64m4(input.m22.data() + i, vl), dz, vl);

    vfloat64m4_t term = __riscv_vfmul_vv_f64m4(dx, mdx, vl);
    term = __riscv_vfmacc_vv_f64m4(term, dy, mdy, vl);
    term = __riscv_vfmacc_vv_f64m4(term, dz, mdz, vl);
    f += reduce_sum_m4(term, vl);
    gx += reduce_sum_m4(mdx, vl);
    gy += reduce_sum_m4(mdy, vl);
    gz += reduce_sum_m4(mdz, vl);

    vfloat64m4_t bx = apply3x4_v_m4(input.base_transform, 0, sx, sy, sz, vl);
    vfloat64m4_t by = apply3x4_v_m4(input.base_transform, 1, sx, sy, sz, vl);
    vfloat64m4_t bz = apply3x4_v_m4(input.base_transform, 2, sx, sy, sz, vl);
    dcost[0] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bx, mdx, vl), vl);
    dcost[1] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bx, mdy, vl), vl);
    dcost[2] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bx, mdz, vl), vl);
    dcost[3] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(by, mdx, vl), vl);
    dcost[4] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(by, mdy, vl), vl);
    dcost[5] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(by, mdz, vl), vl);
    dcost[6] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bz, mdx, vl), vl);
    dcost[7] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bz, mdy, vl), vl);
    dcost[8] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bz, mdz, vl), vl);
    i += vl;
  }

  const double inv_n = 1.0 / static_cast<double>(n);
  result.f = f * inv_n;
  result.translation_gradient = {2.0 * gx * inv_n, 2.0 * gy * inv_n, 2.0 * gz * inv_n};
  for (std::size_t i = 0; i < 9u; ++i)
    result.dcost_drt[i] = 2.0 * dcost[i] * inv_n;
  result.used_rvv = true;
  return result;
#endif
}

inline ResidualResult
indexed_residual_accumulate_candidate(const IndexedResidualInput& input)
{
#ifndef __RVV10__
  return indexed_residual_accumulate_std(input);
#else
  ResidualResult result;
  const std::size_t n = input.rows;
  if (n == 0)
    return result;

  double f = 0.0;
  double gx = 0.0;
  double gy = 0.0;
  double gz = 0.0;
  double dcost[9] = {};

  for (std::size_t i = 0; i < n;) {
    const std::size_t vl = __riscv_vsetvl_e64m4(n - i);
    vuint64m4_t src_index = __riscv_vle64_v_u64m4(input.src_idx.data() + i, vl);
    vuint64m4_t tgt_index = __riscv_vle64_v_u64m4(input.tgt_idx.data() + i, vl);
    vuint64m4_t src_offset = __riscv_vmul_vx_u64m4(src_index, sizeof(double), vl);
    vuint64m4_t tgt_offset = __riscv_vmul_vx_u64m4(tgt_index, sizeof(double), vl);

    vfloat64m4_t sx = __riscv_vluxei64_v_f64m4(input.src_x.data(), src_offset, vl);
    vfloat64m4_t sy = __riscv_vluxei64_v_f64m4(input.src_y.data(), src_offset, vl);
    vfloat64m4_t sz = __riscv_vluxei64_v_f64m4(input.src_z.data(), src_offset, vl);

    vfloat64m4_t tx = apply3x4_v_m4(input.transform, 0, sx, sy, sz, vl);
    vfloat64m4_t ty = apply3x4_v_m4(input.transform, 1, sx, sy, sz, vl);
    vfloat64m4_t tz = apply3x4_v_m4(input.transform, 2, sx, sy, sz, vl);
    vfloat64m4_t dx =
        __riscv_vfsub_vv_f64m4(tx, __riscv_vluxei64_v_f64m4(input.tgt_x.data(), tgt_offset, vl), vl);
    vfloat64m4_t dy =
        __riscv_vfsub_vv_f64m4(ty, __riscv_vluxei64_v_f64m4(input.tgt_y.data(), tgt_offset, vl), vl);
    vfloat64m4_t dz =
        __riscv_vfsub_vv_f64m4(tz, __riscv_vluxei64_v_f64m4(input.tgt_z.data(), tgt_offset, vl), vl);

    vfloat64m4_t mdx =
        __riscv_vfmul_vv_f64m4(__riscv_vluxei64_v_f64m4(input.m00.data(), src_offset, vl), dx, vl);
    mdx = __riscv_vfmacc_vv_f64m4(
        mdx, __riscv_vluxei64_v_f64m4(input.m01.data(), src_offset, vl), dy, vl);
    mdx = __riscv_vfmacc_vv_f64m4(
        mdx, __riscv_vluxei64_v_f64m4(input.m02.data(), src_offset, vl), dz, vl);

    vfloat64m4_t mdy =
        __riscv_vfmul_vv_f64m4(__riscv_vluxei64_v_f64m4(input.m10.data(), src_offset, vl), dx, vl);
    mdy = __riscv_vfmacc_vv_f64m4(
        mdy, __riscv_vluxei64_v_f64m4(input.m11.data(), src_offset, vl), dy, vl);
    mdy = __riscv_vfmacc_vv_f64m4(
        mdy, __riscv_vluxei64_v_f64m4(input.m12.data(), src_offset, vl), dz, vl);

    vfloat64m4_t mdz =
        __riscv_vfmul_vv_f64m4(__riscv_vluxei64_v_f64m4(input.m20.data(), src_offset, vl), dx, vl);
    mdz = __riscv_vfmacc_vv_f64m4(
        mdz, __riscv_vluxei64_v_f64m4(input.m21.data(), src_offset, vl), dy, vl);
    mdz = __riscv_vfmacc_vv_f64m4(
        mdz, __riscv_vluxei64_v_f64m4(input.m22.data(), src_offset, vl), dz, vl);

    vfloat64m4_t term = __riscv_vfmul_vv_f64m4(dx, mdx, vl);
    term = __riscv_vfmacc_vv_f64m4(term, dy, mdy, vl);
    term = __riscv_vfmacc_vv_f64m4(term, dz, mdz, vl);
    f += reduce_sum_m4(term, vl);
    gx += reduce_sum_m4(mdx, vl);
    gy += reduce_sum_m4(mdy, vl);
    gz += reduce_sum_m4(mdz, vl);

    vfloat64m4_t bx = apply3x4_v_m4(input.base_transform, 0, sx, sy, sz, vl);
    vfloat64m4_t by = apply3x4_v_m4(input.base_transform, 1, sx, sy, sz, vl);
    vfloat64m4_t bz = apply3x4_v_m4(input.base_transform, 2, sx, sy, sz, vl);
    dcost[0] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bx, mdx, vl), vl);
    dcost[1] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bx, mdy, vl), vl);
    dcost[2] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bx, mdz, vl), vl);
    dcost[3] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(by, mdx, vl), vl);
    dcost[4] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(by, mdy, vl), vl);
    dcost[5] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(by, mdz, vl), vl);
    dcost[6] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bz, mdx, vl), vl);
    dcost[7] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bz, mdy, vl), vl);
    dcost[8] += reduce_sum_m4(__riscv_vfmul_vv_f64m4(bz, mdz, vl), vl);
    i += vl;
  }

  const double inv_n = 1.0 / static_cast<double>(n);
  result.f = f * inv_n;
  result.translation_gradient = {2.0 * gx * inv_n, 2.0 * gy * inv_n, 2.0 * gz * inv_n};
  for (std::size_t i = 0; i < 9u; ++i)
    result.dcost_drt[i] = 2.0 * dcost[i] * inv_n;
  result.used_rvv = true;
  return result;
#endif
}

inline HessianLoopResult
hessian_loop_accumulate_candidate(const ResidualInput& input)
{
#ifndef __RVV10__
  return hessian_loop_accumulate_std(input);
#else
  HessianLoopResult result;
  const std::size_t n = input.rows;
  if (n == 0)
    return result;

  double gx = 0.0, gy = 0.0, gz = 0.0;
  double htt[9] = {};
  double dcost[9] = {};
  double dcost_b[27] = {};
  double hrot[54] = {};

  auto add_product = [](double& dst, vfloat64m4_t a, vfloat64m4_t b, std::size_t vl) {
    dst += reduce_sum_m4(__riscv_vfmul_vv_f64m4(a, b, vl), vl);
  };

  for (std::size_t i = 0; i < n;) {
    const std::size_t vl = __riscv_vsetvl_e64m4(n - i);
    vfloat64m4_t sx = __riscv_vle64_v_f64m4(input.src_x.data() + i, vl);
    vfloat64m4_t sy = __riscv_vle64_v_f64m4(input.src_y.data() + i, vl);
    vfloat64m4_t sz = __riscv_vle64_v_f64m4(input.src_z.data() + i, vl);

    vfloat64m4_t tx = apply3x4_v_m4(input.transform, 0, sx, sy, sz, vl);
    vfloat64m4_t ty = apply3x4_v_m4(input.transform, 1, sx, sy, sz, vl);
    vfloat64m4_t tz = apply3x4_v_m4(input.transform, 2, sx, sy, sz, vl);
    vfloat64m4_t dx =
        __riscv_vfsub_vv_f64m4(tx, __riscv_vle64_v_f64m4(input.tgt_x.data() + i, vl), vl);
    vfloat64m4_t dy =
        __riscv_vfsub_vv_f64m4(ty, __riscv_vle64_v_f64m4(input.tgt_y.data() + i, vl), vl);
    vfloat64m4_t dz =
        __riscv_vfsub_vv_f64m4(tz, __riscv_vle64_v_f64m4(input.tgt_z.data() + i, vl), vl);

    vfloat64m4_t m00 = __riscv_vle64_v_f64m4(input.m00.data() + i, vl);
    vfloat64m4_t m01 = __riscv_vle64_v_f64m4(input.m01.data() + i, vl);
    vfloat64m4_t m02 = __riscv_vle64_v_f64m4(input.m02.data() + i, vl);
    vfloat64m4_t m10 = __riscv_vle64_v_f64m4(input.m10.data() + i, vl);
    vfloat64m4_t m11 = __riscv_vle64_v_f64m4(input.m11.data() + i, vl);
    vfloat64m4_t m12 = __riscv_vle64_v_f64m4(input.m12.data() + i, vl);
    vfloat64m4_t m20 = __riscv_vle64_v_f64m4(input.m20.data() + i, vl);
    vfloat64m4_t m21 = __riscv_vle64_v_f64m4(input.m21.data() + i, vl);
    vfloat64m4_t m22 = __riscv_vle64_v_f64m4(input.m22.data() + i, vl);

    vfloat64m4_t mdx = __riscv_vfmul_vv_f64m4(m00, dx, vl);
    mdx = __riscv_vfmacc_vv_f64m4(mdx, m01, dy, vl);
    mdx = __riscv_vfmacc_vv_f64m4(mdx, m02, dz, vl);
    vfloat64m4_t mdy = __riscv_vfmul_vv_f64m4(m10, dx, vl);
    mdy = __riscv_vfmacc_vv_f64m4(mdy, m11, dy, vl);
    mdy = __riscv_vfmacc_vv_f64m4(mdy, m12, dz, vl);
    vfloat64m4_t mdz = __riscv_vfmul_vv_f64m4(m20, dx, vl);
    mdz = __riscv_vfmacc_vv_f64m4(mdz, m21, dy, vl);
    mdz = __riscv_vfmacc_vv_f64m4(mdz, m22, dz, vl);

    gx += reduce_sum_m4(mdx, vl);
    gy += reduce_sum_m4(mdy, vl);
    gz += reduce_sum_m4(mdz, vl);
    htt[0] += reduce_sum_m4(m00, vl);
    htt[1] += reduce_sum_m4(m01, vl);
    htt[2] += reduce_sum_m4(m02, vl);
    htt[3] += reduce_sum_m4(m10, vl);
    htt[4] += reduce_sum_m4(m11, vl);
    htt[5] += reduce_sum_m4(m12, vl);
    htt[6] += reduce_sum_m4(m20, vl);
    htt[7] += reduce_sum_m4(m21, vl);
    htt[8] += reduce_sum_m4(m22, vl);

    vfloat64m4_t bx = apply3x4_v_m4(input.base_transform, 0, sx, sy, sz, vl);
    vfloat64m4_t by = apply3x4_v_m4(input.base_transform, 1, sx, sy, sz, vl);
    vfloat64m4_t bz = apply3x4_v_m4(input.base_transform, 2, sx, sy, sz, vl);
    add_product(dcost[0], bx, mdx, vl);
    add_product(dcost[1], bx, mdy, vl);
    add_product(dcost[2], bx, mdz, vl);
    add_product(dcost[3], by, mdx, vl);
    add_product(dcost[4], by, mdy, vl);
    add_product(dcost[5], by, mdz, vl);
    add_product(dcost[6], bz, mdx, vl);
    add_product(dcost[7], bz, mdy, vl);
    add_product(dcost[8], bz, mdz, vl);

#define PCL_RVV_GICP_ADD_DFDR_B(OFFSET, BASE) \
    do {                                      \
      add_product(dcost_b[(OFFSET) + 0], (BASE), m00, vl); \
      add_product(dcost_b[(OFFSET) + 1], (BASE), m01, vl); \
      add_product(dcost_b[(OFFSET) + 2], (BASE), m02, vl); \
      add_product(dcost_b[(OFFSET) + 3], (BASE), m10, vl); \
      add_product(dcost_b[(OFFSET) + 4], (BASE), m11, vl); \
      add_product(dcost_b[(OFFSET) + 5], (BASE), m12, vl); \
      add_product(dcost_b[(OFFSET) + 6], (BASE), m20, vl); \
      add_product(dcost_b[(OFFSET) + 7], (BASE), m21, vl); \
      add_product(dcost_b[(OFFSET) + 8], (BASE), m22, vl); \
    } while (false)
    PCL_RVV_GICP_ADD_DFDR_B(0, bx);
    PCL_RVV_GICP_ADD_DFDR_B(9, by);
    PCL_RVV_GICP_ADD_DFDR_B(18, bz);
#undef PCL_RVV_GICP_ADD_DFDR_B

    vfloat64m4_t bxx = __riscv_vfmul_vv_f64m4(bx, bx, vl);
    vfloat64m4_t bxy = __riscv_vfmul_vv_f64m4(bx, by, vl);
    vfloat64m4_t bxz = __riscv_vfmul_vv_f64m4(bx, bz, vl);
    vfloat64m4_t byy = __riscv_vfmul_vv_f64m4(by, by, vl);
    vfloat64m4_t byz = __riscv_vfmul_vv_f64m4(by, bz, vl);
    vfloat64m4_t bzz = __riscv_vfmul_vv_f64m4(bz, bz, vl);
#define PCL_RVV_GICP_ADD_HROT(OFFSET, MVALUE) \
    do {                                      \
      add_product(hrot[(OFFSET) + 0], (MVALUE), bxx, vl); \
      add_product(hrot[(OFFSET) + 1], (MVALUE), bxy, vl); \
      add_product(hrot[(OFFSET) + 2], (MVALUE), bxz, vl); \
      add_product(hrot[(OFFSET) + 3], (MVALUE), byy, vl); \
      add_product(hrot[(OFFSET) + 4], (MVALUE), byz, vl); \
      add_product(hrot[(OFFSET) + 5], (MVALUE), bzz, vl); \
    } while (false)
    PCL_RVV_GICP_ADD_HROT(0, m00);
    PCL_RVV_GICP_ADD_HROT(6, m10);
    PCL_RVV_GICP_ADD_HROT(12, m20);
    PCL_RVV_GICP_ADD_HROT(18, m01);
    PCL_RVV_GICP_ADD_HROT(24, m11);
    PCL_RVV_GICP_ADD_HROT(30, m21);
    PCL_RVV_GICP_ADD_HROT(36, m02);
    PCL_RVV_GICP_ADD_HROT(42, m12);
    PCL_RVV_GICP_ADD_HROT(48, m22);
#undef PCL_RVV_GICP_ADD_HROT
    i += vl;
  }

  const double scale = 2.0 / static_cast<double>(n);
  result.translation_gradient = {gx * scale, gy * scale, gz * scale};
  for (std::size_t i = 0; i < 9u; ++i) {
    result.translation_hessian[i] = htt[i] * scale;
    result.dcost_drt[i] = dcost[i] * scale;
  }
  for (std::size_t i = 0; i < 27u; ++i)
    result.dcost_drt_b[i] = dcost_b[i] * scale;
  for (std::size_t i = 0; i < 54u; ++i)
    result.hessian_rot_tmp[i] = hrot[i] * scale;
  result.used_rvv = true;
  return result;
#endif
}

inline CovarianceResult
covariance_post_knn_candidate(const CovarianceInput& input)
{
#ifndef __RVV10__
  return covariance_post_knn_std(input);
#else
  CovarianceResult result;
  result.cov00.resize(input.points);
  result.cov10.resize(input.points);
  result.cov11.resize(input.points);
  result.cov20.resize(input.points);
  result.cov21.resize(input.points);
  result.cov22.resize(input.points);
  if (input.k == 0)
    return result;

  const double inv_k = 1.0 / static_cast<double>(input.k);
  for (std::size_t p = 0; p < input.points; ++p) {
    const std::size_t offset = p * input.k;
    double sx = 0.0, sy = 0.0, sz = 0.0;
    double sxx = 0.0, syx = 0.0, syy = 0.0, szx = 0.0, szy = 0.0, szz = 0.0;
    for (std::size_t j = 0; j < input.k;) {
      const std::size_t vl = __riscv_vsetvl_e64m1(input.k - j);
      vfloat64m1_t dx = __riscv_vle64_v_f64m1(input.dx.data() + offset + j, vl);
      vfloat64m1_t dy = __riscv_vle64_v_f64m1(input.dy.data() + offset + j, vl);
      vfloat64m1_t dz = __riscv_vle64_v_f64m1(input.dz.data() + offset + j, vl);
      sx += reduce_sum(dx, vl);
      sy += reduce_sum(dy, vl);
      sz += reduce_sum(dz, vl);
      sxx += reduce_sum(__riscv_vfmul_vv_f64m1(dx, dx, vl), vl);
      syx += reduce_sum(__riscv_vfmul_vv_f64m1(dy, dx, vl), vl);
      syy += reduce_sum(__riscv_vfmul_vv_f64m1(dy, dy, vl), vl);
      szx += reduce_sum(__riscv_vfmul_vv_f64m1(dz, dx, vl), vl);
      szy += reduce_sum(__riscv_vfmul_vv_f64m1(dz, dy, vl), vl);
      szz += reduce_sum(__riscv_vfmul_vv_f64m1(dz, dz, vl), vl);
      j += vl;
    }
    const double mx = sx * inv_k;
    const double my = sy * inv_k;
    const double mz = sz * inv_k;
    result.cov00[p] = sxx * inv_k - mx * mx;
    result.cov10[p] = syx * inv_k - my * mx;
    result.cov11[p] = syy * inv_k - my * my;
    result.cov20[p] = szx * inv_k - mz * mx;
    result.cov21[p] = szy * inv_k - mz * my;
    result.cov22[p] = szz * inv_k - mz * mz;
  }
  result.used_rvv = true;
  return result;
#endif
}

} // namespace pcl::registration::rvv_gicp_support
