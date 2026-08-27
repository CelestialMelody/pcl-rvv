/*
 * 本文件做什么：
 * 这里保存 GASD topic 的 first candidate（首个候选）：fixed-grid histogram copy
 * 的 RVV（RISC-V Vector，可变长度向量）直拷贝实现。shape 和 color 两条链路都
 * 需要把 cell 内部 bins 线性写入 descriptor 输出，因此这段 helper 先做连续 load/store
 * 对拍，后续再决定是否值得把 sample projection 和 interpolation 也拆出来。
 *
 * 证据边界：
 * 这是 test-only candidate（测试专用候选），不是 production dispatch（生产分流）。
 * non-RVV 构建会自然回退到 scalar reference。bench 计时要把拷贝和任何预处理分开，
 * 这样 board 结论才不会把非目标成本混进去。
 */

#pragma once

#include "gasd_reference.hpp"

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::gasd
{
inline void
copyShapeHistogramsStdToBuffer(const HistogramGrid& hists,
                               const std::size_t half_grid_size,
                               const std::size_t hists_size,
                               std::vector<float>& output)
{
  output = copyShapeHistogramsStd(hists, half_grid_size, hists_size);
}

inline void
copyColorHistogramsStdToBuffer(HistogramGrid hists,
                               const std::size_t half_grid_size,
                               const std::size_t hists_size,
                               std::vector<float>& output)
{
  output = copyColorHistogramsStd(std::move(hists), half_grid_size, hists_size);
}

inline void
projectShapeSamplesStdToBuffersFallback(const ShapeCloudT& cloud,
                                        const float max_coord,
                                        const float distance_normalization_factor,
                                        const std::size_t half_grid_size,
                                        const std::size_t hists_size,
                                        ShapeProjectionBuffers& buffers)
{
  projectShapeSamplesStdToBuffers(cloud, max_coord, distance_normalization_factor, half_grid_size, hists_size, buffers);
}

inline void
projectColorHueStdToBuffersFallback(const ColorCloudT& cloud,
                                    const std::size_t color_hists_size,
                                    ColorHueBuffers& buffers)
{
  projectColorHueStdToBuffers(cloud, color_hists_size, buffers);
}

inline void
computeTrilinearInterpolationStdToBuffersFallback(const ShapeProjectionBuffers& projection,
                                                  const std::size_t half_grid_size,
                                                  TrilinearInterpolationBuffers& buffers)
{
  computeTrilinearInterpolationStdToBuffers(projection, half_grid_size, buffers);
}

inline void
accumulateTrilinearHistogramStdFallback(const ShapeProjectionBuffers& projection,
                                        const std::size_t half_grid_size,
                                        const std::size_t hists_size,
                                        const float hist_incr,
                                        std::vector<float>& hists)
{
  accumulateTrilinearHistogramStd(projection, half_grid_size, hists_size, hist_incr, hists);
}

inline void
accumulateTrilinearHistogramEigenStdFallback(const ShapeProjectionBuffers& projection,
                                             const std::size_t half_grid_size,
                                             const std::size_t hists_size,
                                             const float hist_incr,
                                             HistogramGrid& hists)
{
  accumulateTrilinearHistogramEigenStd(projection, half_grid_size, hists_size, hist_incr, hists);
}

inline void
computeShapeDescriptorTrilinearStdFallback(const ShapeCloudT& cloud,
                                           const float max_coord,
                                           const float distance_normalization_factor,
                                           const std::size_t half_grid_size,
                                           const std::size_t hists_size,
                                           std::vector<float>& output)
{
  computeShapeDescriptorTrilinearStd(cloud, max_coord, distance_normalization_factor, half_grid_size, hists_size, output);
}

#if defined(__RVV10__)
inline vint32m2_t
floorF32ToI32NoFrm(vfloat32m2_t values, const std::size_t vl)
{
  const vint32m2_t trunc = __riscv_vfcvt_rtz_x_f_v_i32m2(values, vl);
  const vfloat32m2_t trunc_f = __riscv_vfcvt_f_x_v_f32m2(trunc, vl);
  const vbool16_t negative_fraction = __riscv_vmflt_vv_f32m2_b16(values, trunc_f, vl);
  const vint32m2_t zero = __riscv_vmv_v_x_i32m2(0, vl);
  const vint32m2_t adjust = __riscv_vmerge_vxm_i32m2(zero, 1, negative_fraction, vl);
  return __riscv_vsub_vv_i32m2(trunc, adjust, vl);
}

inline void
copyShapeHistogramsRVVToBuffer(const HistogramGrid& hists,
                               const std::size_t half_grid_size,
                               const std::size_t hists_size,
                               std::vector<float>& output)
{
  const std::size_t grid_size = half_grid_size * 2;
  output.assign(grid_size * grid_size * grid_size * hists_size, 0.0f);
  std::size_t pos = 0;
  for (std::size_t i = 0; i < grid_size; ++i)
  {
    for (std::size_t j = 0; j < grid_size; ++j)
    {
      for (std::size_t k = 0; k < grid_size; ++k)
      {
        const std::size_t idx = ((i + 1) * (grid_size + 2) + (j + 1)) * (grid_size + 2) + (k + 1);
        const float* src = hists[idx].data() + 1;
        for (std::size_t bin = 0; bin < hists_size;)
        {
          const std::size_t vl = __riscv_vsetvl_e32m2(hists_size - bin);
          const vfloat32m2_t v = __riscv_vle32_v_f32m2(src + bin, vl);
          __riscv_vse32_v_f32m2(output.data() + pos + bin, v, vl);
          bin += vl;
        }
        pos += hists_size;
      }
    }
  }
}

inline void
copyColorHistogramsRVVToBuffer(HistogramGrid hists,
                               const std::size_t half_grid_size,
                               const std::size_t hists_size,
                               std::vector<float>& output)
{
  const std::size_t grid_size = half_grid_size * 2;
  output.assign(grid_size * grid_size * grid_size * hists_size, 0.0f);
  std::size_t pos = 0;
  for (std::size_t i = 0; i < grid_size; ++i)
  {
    for (std::size_t j = 0; j < grid_size; ++j)
    {
      for (std::size_t k = 0; k < grid_size; ++k)
      {
        const std::size_t idx = ((i + 1) * (grid_size + 2) + (j + 1)) * (grid_size + 2) + (k + 1);
        hists[idx][1] += hists[idx][hists_size + 1];
        hists[idx][hists_size] += hists[idx][0];
        const float* src = hists[idx].data() + 1;
        for (std::size_t bin = 0; bin < hists_size;)
        {
          const std::size_t vl = __riscv_vsetvl_e32m2(hists_size - bin);
          const vfloat32m2_t v = __riscv_vle32_v_f32m2(src + bin, vl);
          __riscv_vse32_v_f32m2(output.data() + pos + bin, v, vl);
          bin += vl;
        }
        pos += hists_size;
      }
    }
  }
}

inline void
projectShapeSamplesRVVToBuffers(const ShapeCloudT& cloud,
                                const float max_coord,
                                const float distance_normalization_factor,
                                const std::size_t half_grid_size,
                                const std::size_t hists_size,
                                ShapeProjectionBuffers& buffers)
{
  resizeShapeProjectionBuffers(buffers, cloud.size());
  const float half_grid = static_cast<float>(half_grid_size);
  const float shape_grid_step = distance_normalization_factor / half_grid;
  const float coordinate_scale = half_grid / max_coord;
  const float hists_scale = static_cast<float>(hists_size);
  const auto stride = static_cast<ptrdiff_t>(sizeof(pcl::PointXYZ));
  const float* base_x = cloud.empty() ? nullptr : &cloud[0].x;
  const float* base_y = cloud.empty() ? nullptr : &cloud[0].y;
  const float* base_z = cloud.empty() ? nullptr : &cloud[0].z;

  for (std::size_t i = 0; i < cloud.size();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    const vfloat32m2_t x = __riscv_vlse32_v_f32m2(base_x + i * (stride / sizeof(float)), stride, vl);
    const vfloat32m2_t y = __riscv_vlse32_v_f32m2(base_y + i * (stride / sizeof(float)), stride, vl);
    const vfloat32m2_t z = __riscv_vlse32_v_f32m2(base_z + i * (stride / sizeof(float)), stride, vl);

    __riscv_vse32_v_f32m2(buffers.grid_x.data() + i, __riscv_vfadd_vf_f32m2(__riscv_vfmul_vf_f32m2(x, coordinate_scale, vl), half_grid, vl), vl);
    __riscv_vse32_v_f32m2(buffers.grid_y.data() + i, __riscv_vfadd_vf_f32m2(__riscv_vfmul_vf_f32m2(y, coordinate_scale, vl), half_grid, vl), vl);
    __riscv_vse32_v_f32m2(buffers.grid_z.data() + i, __riscv_vfadd_vf_f32m2(__riscv_vfmul_vf_f32m2(z, coordinate_scale, vl), half_grid, vl), vl);

    vfloat32m2_t squared = __riscv_vfmul_vv_f32m2(x, x, vl);
    squared = __riscv_vfmacc_vv_f32m2(squared, y, y, vl);
    squared = __riscv_vfmacc_vv_f32m2(squared, z, z, vl);
    const vfloat32m2_t ratio = __riscv_vfdiv_vf_f32m2(__riscv_vfsqrt_v_f32m2(squared, vl), shape_grid_step, vl);
    const vuint32m2_t integral = __riscv_vfcvt_rtz_xu_f_v_u32m2(ratio, vl);
    const vfloat32m2_t integral_float = __riscv_vfcvt_f_xu_v_f32m2(integral, vl);
    const vfloat32m2_t fractional = __riscv_vfsub_vv_f32m2(ratio, integral_float, vl);
    __riscv_vse32_v_f32m2(buffers.dbin.data() + i, __riscv_vfmul_vf_f32m2(fractional, hists_scale, vl), vl);

    i += vl;
  }
}

inline void
projectColorHueRVVToBuffers(const ColorCloudT& cloud,
                            const std::size_t color_hists_size,
                            ColorHueBuffers& buffers)
{
  resizeColorHueBuffers(buffers, cloud.size());
  const float hists_scale = static_cast<float>(color_hists_size) / 360.0f;
  const auto stride = static_cast<ptrdiff_t>(sizeof(pcl::PointXYZRGBA));
  const std::uint8_t* base_r = cloud.empty() ? nullptr : &cloud[0].r;
  const std::uint8_t* base_g = cloud.empty() ? nullptr : &cloud[0].g;
  const std::uint8_t* base_b = cloud.empty() ? nullptr : &cloud[0].b;

  for (std::size_t i = 0; i < cloud.size();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    const vuint8mf2_t r8 = __riscv_vlse8_v_u8mf2(base_r + i * stride, stride, vl);
    const vuint8mf2_t g8 = __riscv_vlse8_v_u8mf2(base_g + i * stride, stride, vl);
    const vuint8mf2_t b8 = __riscv_vlse8_v_u8mf2(base_b + i * stride, stride, vl);
    const vuint16m1_t r16 = __riscv_vzext_vf2_u16m1(r8, vl);
    const vuint16m1_t g16 = __riscv_vzext_vf2_u16m1(g8, vl);
    const vuint16m1_t b16 = __riscv_vzext_vf2_u16m1(b8, vl);
    const vuint32m2_t r = __riscv_vzext_vf2_u32m2(r16, vl);
    const vuint32m2_t g = __riscv_vzext_vf2_u32m2(g16, vl);
    const vuint32m2_t b = __riscv_vzext_vf2_u32m2(b16, vl);

    const vuint32m2_t max_rg = __riscv_vmaxu_vv_u32m2(r, g, vl);
    const vuint32m2_t max_rgb = __riscv_vmaxu_vv_u32m2(max_rg, b, vl);
    const vuint32m2_t min_rg = __riscv_vminu_vv_u32m2(r, g, vl);
    const vuint32m2_t min_rgb = __riscv_vminu_vv_u32m2(min_rg, b, vl);
    const vuint32m2_t diff_i = __riscv_vsub_vv_u32m2(max_rgb, min_rgb, vl);

    const vfloat32m2_t r_f = __riscv_vfcvt_f_xu_v_f32m2(r, vl);
    const vfloat32m2_t g_f = __riscv_vfcvt_f_xu_v_f32m2(g, vl);
    const vfloat32m2_t b_f = __riscv_vfcvt_f_xu_v_f32m2(b, vl);
    const vfloat32m2_t diff_f = __riscv_vfcvt_f_xu_v_f32m2(diff_i, vl);
    const vbool16_t diff_is_zero = __riscv_vmseq_vx_u32m2_b16(diff_i, 0, vl);
    const vfloat32m2_t diff_safe =
        __riscv_vmerge_vvm_f32m2(diff_f, __riscv_vfmv_v_f_f32m2(1.0f, vl), diff_is_zero, vl);
    const vfloat32m2_t diff_inv = __riscv_vfrdiv_vf_f32m2(diff_safe, 1.0f, vl);

    vfloat32m2_t hue_r = __riscv_vfmul_vf_f32m2(
        __riscv_vfmul_vv_f32m2(__riscv_vfsub_vv_f32m2(g_f, b_f, vl), diff_inv, vl), 60.0f, vl);
    const vbool16_t hue_r_negative = __riscv_vmflt_vf_f32m2_b16(hue_r, 0.0f, vl);
    hue_r = __riscv_vfadd_vv_f32m2(
        hue_r,
        __riscv_vmerge_vvm_f32m2(__riscv_vfmv_v_f_f32m2(0.0f, vl),
                                 __riscv_vfmv_v_f_f32m2(360.0f, vl),
                                 hue_r_negative,
                                 vl),
        vl);
    const vfloat32m2_t hue_g = __riscv_vfmul_vf_f32m2(
        __riscv_vfadd_vf_f32m2(
            __riscv_vfmul_vv_f32m2(__riscv_vfsub_vv_f32m2(b_f, r_f, vl), diff_inv, vl), 2.0f, vl),
        60.0f,
        vl);
    const vfloat32m2_t hue_b = __riscv_vfmul_vf_f32m2(
        __riscv_vfadd_vf_f32m2(
            __riscv_vfmul_vv_f32m2(__riscv_vfsub_vv_f32m2(r_f, g_f, vl), diff_inv, vl), 4.0f, vl),
        60.0f,
        vl);

    const vbool16_t max_is_r = __riscv_vmseq_vv_u32m2_b16(max_rgb, r, vl);
    const vbool16_t max_is_g = __riscv_vmand_mm_b16(
        __riscv_vmnot_m_b16(max_is_r, vl), __riscv_vmseq_vv_u32m2_b16(max_rgb, g, vl), vl);
    vfloat32m2_t hue = __riscv_vmerge_vvm_f32m2(hue_b, hue_g, max_is_g, vl);
    hue = __riscv_vmerge_vvm_f32m2(hue, hue_r, max_is_r, vl);
    hue = __riscv_vmerge_vvm_f32m2(hue, __riscv_vfmv_v_f_f32m2(0.0f, vl), diff_is_zero, vl);

    __riscv_vse32_v_f32m2(buffers.hue.data() + i, hue, vl);
    __riscv_vse32_v_f32m2(buffers.hbin.data() + i, __riscv_vfmul_vf_f32m2(hue, hists_scale, vl), vl);
    i += vl;
  }
}

inline void
computeTrilinearInterpolationRVVToBuffers(const ShapeProjectionBuffers& projection,
                                          const std::size_t half_grid_size,
                                          TrilinearInterpolationBuffers& buffers)
{
  resizeTrilinearInterpolationBuffers(buffers, projection.grid_x.size());
  const auto grid_stride = static_cast<std::int32_t>(half_grid_size * 2 + 2);

  for (std::size_t i = 0; i < projection.grid_x.size();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(projection.grid_x.size() - i);
    const vfloat32m2_t half = __riscv_vfmv_v_f_f32m2(0.5f, vl);
    const vfloat32m2_t coord_x = __riscv_vfsub_vv_f32m2(__riscv_vle32_v_f32m2(projection.grid_x.data() + i, vl), half, vl);
    const vfloat32m2_t coord_y = __riscv_vfsub_vv_f32m2(__riscv_vle32_v_f32m2(projection.grid_y.data() + i, vl), half, vl);
    const vfloat32m2_t coord_z = __riscv_vfsub_vv_f32m2(__riscv_vle32_v_f32m2(projection.grid_z.data() + i, vl), half, vl);
    const vfloat32m2_t coord_h = __riscv_vfsub_vv_f32m2(__riscv_vle32_v_f32m2(projection.dbin.data() + i, vl), half, vl);

    const vint32m2_t bin_x = floorF32ToI32NoFrm(coord_x, vl);
    const vint32m2_t bin_y = floorF32ToI32NoFrm(coord_y, vl);
    const vint32m2_t bin_z = floorF32ToI32NoFrm(coord_z, vl);
    const vint32m2_t bin_h = floorF32ToI32NoFrm(coord_h, vl);

    vint32m2_t grid_idx = __riscv_vadd_vx_i32m2(bin_x, 1, vl);
    grid_idx = __riscv_vmul_vx_i32m2(grid_idx, grid_stride, vl);
    grid_idx = __riscv_vadd_vv_i32m2(grid_idx, bin_y, vl);
    grid_idx = __riscv_vadd_vx_i32m2(grid_idx, 1, vl);
    grid_idx = __riscv_vmul_vx_i32m2(grid_idx, grid_stride, vl);
    grid_idx = __riscv_vadd_vv_i32m2(grid_idx, bin_z, vl);
    grid_idx = __riscv_vadd_vx_i32m2(grid_idx, 1, vl);
    const vint32m2_t h_idx = __riscv_vadd_vx_i32m2(bin_h, 1, vl);
    __riscv_vse32_v_u32m2(buffers.grid_idx.data() + i, __riscv_vreinterpret_v_i32m2_u32m2(grid_idx), vl);
    __riscv_vse32_v_u32m2(buffers.h_idx.data() + i, __riscv_vreinterpret_v_i32m2_u32m2(h_idx), vl);

    const vfloat32m2_t x_frac = __riscv_vfsub_vv_f32m2(coord_x, __riscv_vfcvt_f_x_v_f32m2(bin_x, vl), vl);
    const vfloat32m2_t y_frac = __riscv_vfsub_vv_f32m2(coord_y, __riscv_vfcvt_f_x_v_f32m2(bin_y, vl), vl);
    const vfloat32m2_t z_frac = __riscv_vfsub_vv_f32m2(coord_z, __riscv_vfcvt_f_x_v_f32m2(bin_z, vl), vl);
    const vfloat32m2_t one_v = __riscv_vfmv_v_f_f32m2(1.0f, vl);

    const vfloat32m2_t v_x1 = x_frac;
    const vfloat32m2_t v_x0 = __riscv_vfsub_vv_f32m2(one_v, v_x1, vl);
    const vfloat32m2_t v_xy11 = __riscv_vfmul_vv_f32m2(v_x1, y_frac, vl);
    const vfloat32m2_t v_xy10 = __riscv_vfsub_vv_f32m2(v_x1, v_xy11, vl);
    const vfloat32m2_t v_xy01 = __riscv_vfmul_vv_f32m2(v_x0, y_frac, vl);
    const vfloat32m2_t v_xy00 = __riscv_vfsub_vv_f32m2(v_x0, v_xy01, vl);

    const vfloat32m2_t w111 = __riscv_vfmul_vv_f32m2(v_xy11, z_frac, vl);
    const vfloat32m2_t w110 = __riscv_vfsub_vv_f32m2(v_xy11, w111, vl);
    const vfloat32m2_t w101 = __riscv_vfmul_vv_f32m2(v_xy10, z_frac, vl);
    const vfloat32m2_t w100 = __riscv_vfsub_vv_f32m2(v_xy10, w101, vl);
    const vfloat32m2_t w011 = __riscv_vfmul_vv_f32m2(v_xy01, z_frac, vl);
    const vfloat32m2_t w010 = __riscv_vfsub_vv_f32m2(v_xy01, w011, vl);
    const vfloat32m2_t w001 = __riscv_vfmul_vv_f32m2(v_xy00, z_frac, vl);
    const vfloat32m2_t w000 = __riscv_vfsub_vv_f32m2(v_xy00, w001, vl);

    __riscv_vse32_v_f32m2(buffers.w000.data() + i, w000, vl);
    __riscv_vse32_v_f32m2(buffers.w001.data() + i, w001, vl);
    __riscv_vse32_v_f32m2(buffers.w010.data() + i, w010, vl);
    __riscv_vse32_v_f32m2(buffers.w011.data() + i, w011, vl);
    __riscv_vse32_v_f32m2(buffers.w100.data() + i, w100, vl);
    __riscv_vse32_v_f32m2(buffers.w101.data() + i, w101, vl);
    __riscv_vse32_v_f32m2(buffers.w110.data() + i, w110, vl);
    __riscv_vse32_v_f32m2(buffers.w111.data() + i, w111, vl);
    i += vl;
  }
}

inline void
accumulateTrilinearHistogramRVVStaged(const ShapeProjectionBuffers& projection,
                                      const std::size_t half_grid_size,
                                      const std::size_t hists_size,
                                      const float hist_incr,
                                      std::vector<float>& hists)
{
  TrilinearInterpolationBuffers buffers;
  computeTrilinearInterpolationRVVToBuffers(projection, half_grid_size, buffers);
  accumulateTrilinearInterpolationBuffersToFlatHistogram(buffers, half_grid_size, hists_size, hist_incr, hists);
}

inline void
accumulateTrilinearHistogramEigenRVVStaged(const ShapeProjectionBuffers& projection,
                                           const std::size_t half_grid_size,
                                           const std::size_t hists_size,
                                           const float hist_incr,
                                           HistogramGrid& hists)
{
  TrilinearInterpolationBuffers buffers;
  computeTrilinearInterpolationRVVToBuffers(projection, half_grid_size, buffers);
  accumulateTrilinearInterpolationBuffersToEigenHistogram(buffers, half_grid_size, hists_size, hist_incr, hists);
}

inline void
computeShapeDescriptorTrilinearRVVStaged(const ShapeCloudT& cloud,
                                         const float max_coord,
                                         const float distance_normalization_factor,
                                         const std::size_t half_grid_size,
                                         const std::size_t hists_size,
                                         std::vector<float>& output)
{
  ShapeProjectionBuffers projection;
  projectShapeSamplesRVVToBuffers(cloud, max_coord, distance_normalization_factor, half_grid_size, hists_size, projection);
  HistogramGrid hists;
  const float hist_incr = 100.0f / static_cast<float>(cloud.size() - 1);
  accumulateTrilinearHistogramEigenRVVStaged(projection, half_grid_size, hists_size, hist_incr, hists);
  copyShapeHistogramsRVVToBuffer(hists, half_grid_size, hists_size, output);
}
#else
inline void
copyShapeHistogramsRVVToBuffer(const HistogramGrid& hists,
                               const std::size_t half_grid_size,
                               const std::size_t hists_size,
                               std::vector<float>& output)
{
  copyShapeHistogramsStdToBuffer(hists, half_grid_size, hists_size, output);
}

inline void
copyColorHistogramsRVVToBuffer(HistogramGrid hists,
                               const std::size_t half_grid_size,
                               const std::size_t hists_size,
                               std::vector<float>& output)
{
  copyColorHistogramsStdToBuffer(std::move(hists), half_grid_size, hists_size, output);
}

inline void
projectShapeSamplesRVVToBuffers(const ShapeCloudT& cloud,
                                const float max_coord,
                                const float distance_normalization_factor,
                                const std::size_t half_grid_size,
                                const std::size_t hists_size,
                                ShapeProjectionBuffers& buffers)
{
  projectShapeSamplesStdToBuffersFallback(cloud, max_coord, distance_normalization_factor, half_grid_size, hists_size, buffers);
}

inline void
projectColorHueRVVToBuffers(const ColorCloudT& cloud,
                            const std::size_t color_hists_size,
                            ColorHueBuffers& buffers)
{
  projectColorHueStdToBuffersFallback(cloud, color_hists_size, buffers);
}

inline void
computeTrilinearInterpolationRVVToBuffers(const ShapeProjectionBuffers& projection,
                                          const std::size_t half_grid_size,
                                          TrilinearInterpolationBuffers& buffers)
{
  computeTrilinearInterpolationStdToBuffersFallback(projection, half_grid_size, buffers);
}

inline void
accumulateTrilinearHistogramRVVStaged(const ShapeProjectionBuffers& projection,
                                      const std::size_t half_grid_size,
                                      const std::size_t hists_size,
                                      const float hist_incr,
                                      std::vector<float>& hists)
{
  accumulateTrilinearHistogramStdFallback(projection, half_grid_size, hists_size, hist_incr, hists);
}

inline void
accumulateTrilinearHistogramEigenRVVStaged(const ShapeProjectionBuffers& projection,
                                           const std::size_t half_grid_size,
                                           const std::size_t hists_size,
                                           const float hist_incr,
                                           HistogramGrid& hists)
{
  accumulateTrilinearHistogramEigenStdFallback(projection, half_grid_size, hists_size, hist_incr, hists);
}

inline void
computeShapeDescriptorTrilinearRVVStaged(const ShapeCloudT& cloud,
                                         const float max_coord,
                                         const float distance_normalization_factor,
                                         const std::size_t half_grid_size,
                                         const std::size_t hists_size,
                                         std::vector<float>& output)
{
  computeShapeDescriptorTrilinearStdFallback(cloud, max_coord, distance_normalization_factor, half_grid_size, hists_size, output);
}
#endif
} // namespace pcl::features::rvv_test::gasd
