#pragma once

#include <Eigen/Core>
#include <pcl/point_types.h>

#include <cstddef>
#include <cstdint>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::keypoints::rvv_test::iss_3d
{
#if defined(__GNUC__)
#define PCL_ISS3D_RVV_TEST_NOINLINE __attribute__((noinline))
#else
#define PCL_ISS3D_RVV_TEST_NOINLINE
#endif

PCL_ISS3D_RVV_TEST_NOINLINE inline void
computeScatterMatrixStd(const pcl::PointXYZ* points,
                        int current_index,
                        const int* neighbor_indices,
                        std::size_t neighbor_count,
                        Eigen::Matrix3d& scatter)
{
  scatter.setZero();
  if (points == nullptr || neighbor_indices == nullptr || neighbor_count == 0)
    return;

  const pcl::PointXYZ& center = points[current_index];
  const double cx = center.x;
  const double cy = center.y;
  const double cz = center.z;
  double c00 = 0.0, c01 = 0.0, c02 = 0.0;
  double c11 = 0.0, c12 = 0.0, c22 = 0.0;

  for (std::size_t i = 0; i < neighbor_count; ++i)
  {
    const pcl::PointXYZ& point = points[neighbor_indices[i]];
    const double dx = point.x - cx;
    const double dy = point.y - cy;
    const double dz = point.z - cz;
    c00 += dx * dx;
    c01 += dx * dy;
    c02 += dx * dz;
    c11 += dy * dy;
    c12 += dy * dz;
    c22 += dz * dz;
  }

  scatter << c00, c01, c02,
             c01, c11, c12,
             c02, c12, c22;
}

#if defined(__RVV10__)
inline double
reduceF64(vfloat64m2_t value, const std::size_t vlmax)
{
  const vfloat64m1_t zero = __riscv_vfmv_s_f_f64m1(0.0, 1);
  return __riscv_vfmv_f_s_f64m1_f64(__riscv_vfredosum_vs_f64m2_f64m1(value, zero, vlmax));
}

PCL_ISS3D_RVV_TEST_NOINLINE inline void
computeScatterMatrixRVV(const pcl::PointXYZ* points,
                        int current_index,
                        const int* neighbor_indices,
                        std::size_t neighbor_count,
                        Eigen::Matrix3d& scatter)
{
  scatter.setZero();
  if (points == nullptr || neighbor_indices == nullptr || neighbor_count == 0)
    return;

  const pcl::PointXYZ& center = points[current_index];
  const std::size_t vlmax = __riscv_vsetvlmax_e64m2();
  const vfloat64m2_t zero = __riscv_vfmv_v_f_f64m2(0.0, vlmax);
  vfloat64m2_t c00 = zero, c01 = zero, c02 = zero;
  vfloat64m2_t c11 = zero, c12 = zero, c22 = zero;
  const auto* base = reinterpret_cast<const std::uint8_t*>(points);
  const auto* idx_i32 = reinterpret_cast<const std::int32_t*>(neighbor_indices);

  std::size_t i = 0;
  while (i < neighbor_count)
  {
    const std::size_t vl = __riscv_vsetvl_e64m2(neighbor_count - i);
    const vint32m1_t v_idx_i32 = __riscv_vle32_v_i32m1(idx_i32 + i, vl);
    const vuint32m1_t v_idx = __riscv_vreinterpret_v_i32m1_u32m1(v_idx_i32);
    const vuint32m1_t v_off = __riscv_vmul_vx_u32m1(v_idx, sizeof(pcl::PointXYZ), vl);
    const auto* x_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, x));
    const auto* y_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, y));
    const auto* z_base = reinterpret_cast<const float*>(base + offsetof(pcl::PointXYZ, z));
    const vfloat32m1_t x = __riscv_vluxei32_v_f32m1(x_base, v_off, vl);
    const vfloat32m1_t y = __riscv_vluxei32_v_f32m1(y_base, v_off, vl);
    const vfloat32m1_t z = __riscv_vluxei32_v_f32m1(z_base, v_off, vl);

    const vfloat64m2_t dx = __riscv_vfsub_vf_f64m2(__riscv_vfwcvt_f_f_v_f64m2(x, vl), center.x, vl);
    const vfloat64m2_t dy = __riscv_vfsub_vf_f64m2(__riscv_vfwcvt_f_f_v_f64m2(y, vl), center.y, vl);
    const vfloat64m2_t dz = __riscv_vfsub_vf_f64m2(__riscv_vfwcvt_f_f_v_f64m2(z, vl), center.z, vl);
    c00 = __riscv_vfmacc_vv_f64m2_tu(c00, dx, dx, vl);
    c01 = __riscv_vfmacc_vv_f64m2_tu(c01, dx, dy, vl);
    c02 = __riscv_vfmacc_vv_f64m2_tu(c02, dx, dz, vl);
    c11 = __riscv_vfmacc_vv_f64m2_tu(c11, dy, dy, vl);
    c12 = __riscv_vfmacc_vv_f64m2_tu(c12, dy, dz, vl);
    c22 = __riscv_vfmacc_vv_f64m2_tu(c22, dz, dz, vl);
    i += vl;
  }

  const double s00 = reduceF64(c00, vlmax);
  const double s01 = reduceF64(c01, vlmax);
  const double s02 = reduceF64(c02, vlmax);
  const double s11 = reduceF64(c11, vlmax);
  const double s12 = reduceF64(c12, vlmax);
  const double s22 = reduceF64(c22, vlmax);
  scatter << s00, s01, s02,
             s01, s11, s12,
             s02, s12, s22;
}
#endif

PCL_ISS3D_RVV_TEST_NOINLINE inline void
computeScatterMatrixCandidate(const pcl::PointXYZ* points,
                              int current_index,
                              const int* neighbor_indices,
                              std::size_t neighbor_count,
                              Eigen::Matrix3d& scatter)
{
#if defined(__RVV10__)
  if (neighbor_count >= 16)
  {
    computeScatterMatrixRVV(points, current_index, neighbor_indices, neighbor_count, scatter);
    return;
  }
#endif
  computeScatterMatrixStd(points, current_index, neighbor_indices, neighbor_count, scatter);
}

#undef PCL_ISS3D_RVV_TEST_NOINLINE
} // namespace pcl::keypoints::rvv_test::iss_3d
