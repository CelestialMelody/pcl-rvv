#pragma once

/*
 * 本文件做什么：
 * 这里保存 Phase 020 的 alpha_m closed-form audit（闭式公式审计）helper。它不写
 * RVV intrinsic，也不修改 production；作用是把 `impl/ppf.hpp` 中每个 pair 都构造
 * Eigen `AngleAxisf` / `Affine3f` 的标量链路，改写成等价的 Rodrigues rotation formula
 * （罗德里格旋转公式）y/z 分量计算，先验证公式边界是否值得继续 RVV 化。
 *
 * 证据边界：
 * 这是 test-only candidate（测试专用候选），只证明公式可对拍。即使它通过 correctness，
 * 也不能直接证明 production dispatch（生产分流）或板卡性能。
 */

#include "impl/ppf_reference.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>

#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::ppf
{

inline float
computeAlphaMClosedForm(const pcl::PointXYZ& reference_point,
                        const pcl::Normal& reference_normal,
                        const pcl::PointXYZ& model_point)
{
  const Eigen::Vector3f delta =
      model_point.getVector3fMap() - reference_point.getVector3fMap();
  const Eigen::Vector3f normal = reference_normal.getNormalVector3fMap();

  const float nx = normal.x();
  const float ny = normal.y();
  const float nz = normal.z();
  const float dx = delta.x();
  const float dy = delta.y();
  const float dz = delta.z();

  float transformed_y = dy;
  float transformed_z = dz;

  if (ny == 0.0f && nz == 0.0f)
  {
    const float sin_angle = std::sqrt(std::max(0.0f, 1.0f - nx * nx));
    transformed_y = dy;
    transformed_z = -sin_angle * dx + nx * dz;
  }
  else
  {
    const float yz_norm2 = ny * ny + nz * nz;
    const float one_minus_cos_over_axis_norm2 = (1.0f - nx) / yz_norm2;
    const float axis_dot_delta = nz * dy - ny * dz;

    transformed_y = nx * dy - ny * dx +
                    nz * axis_dot_delta * one_minus_cos_over_axis_norm2;
    transformed_z = nx * dz - nz * dx -
                    ny * axis_dot_delta * one_minus_cos_over_axis_norm2;
  }

  float angle = std::atan2(-transformed_z, transformed_y);
  if (std::sin(angle) * transformed_z < 0.0f)
    angle *= -1.0f;
  return -angle;
}

struct AlphaMStaging
{
  std::vector<float> dx;
  std::vector<float> dy;
  std::vector<float> dz;
  std::vector<float> nx;
  std::vector<float> ny;
  std::vector<float> nz;
  std::vector<std::uint32_t> output_rows;
};

inline void
reserveAlphaMStaging(AlphaMStaging& staging, const std::size_t pair_count)
{
  staging.dx.reserve(pair_count);
  staging.dy.reserve(pair_count);
  staging.dz.reserve(pair_count);
  staging.nx.reserve(pair_count);
  staging.ny.reserve(pair_count);
  staging.nz.reserve(pair_count);
  staging.output_rows.reserve(pair_count);
}

inline void
appendAlphaMStaging(AlphaMStaging& staging,
                    const pcl::PointXYZ& reference_point,
                    const pcl::Normal& reference_normal,
                    const pcl::PointXYZ& model_point,
                    const std::uint32_t output_row)
{
  staging.dx.push_back(model_point.x - reference_point.x);
  staging.dy.push_back(model_point.y - reference_point.y);
  staging.dz.push_back(model_point.z - reference_point.z);
  staging.nx.push_back(reference_normal.normal_x);
  staging.ny.push_back(reference_normal.normal_y);
  staging.nz.push_back(reference_normal.normal_z);
  staging.output_rows.push_back(output_row);
}

#if defined(__RVV10__)
inline void
computeAlphaMClosedFormRVV(const AlphaMStaging& staging, std::vector<float>& alpha_m)
{
  const std::size_t pair_count = staging.dx.size();
  alpha_m.resize(pair_count);

  for (std::size_t offset = 0; offset < pair_count;)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(pair_count - offset);
    const vfloat32m2_t dx = __riscv_vle32_v_f32m2(staging.dx.data() + offset, vl);
    const vfloat32m2_t dy = __riscv_vle32_v_f32m2(staging.dy.data() + offset, vl);
    const vfloat32m2_t dz = __riscv_vle32_v_f32m2(staging.dz.data() + offset, vl);
    const vfloat32m2_t nx = __riscv_vle32_v_f32m2(staging.nx.data() + offset, vl);
    const vfloat32m2_t ny = __riscv_vle32_v_f32m2(staging.ny.data() + offset, vl);
    const vfloat32m2_t nz = __riscv_vle32_v_f32m2(staging.nz.data() + offset, vl);

    const vfloat32m2_t ny2 = __riscv_vfmul_vv_f32m2(ny, ny, vl);
    const vfloat32m2_t yz_norm2 = __riscv_vfmacc_vv_f32m2(ny2, nz, nz, vl);
    const vbool16_t parallel_to_x = __riscv_vmfeq_vf_f32m2_b16(yz_norm2, 0.0f, vl);

    const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2(1.0f, vl);
    const vfloat32m2_t yz_norm2_safe =
        __riscv_vmerge_vvm_f32m2(yz_norm2, one, parallel_to_x, vl);
    const vfloat32m2_t one_minus_cos =
        __riscv_vfsub_vv_f32m2(one, nx, vl);
    const vfloat32m2_t scale =
        __riscv_vfdiv_vv_f32m2(one_minus_cos, yz_norm2_safe, vl);
    const vfloat32m2_t axis_dot_delta =
        __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(nz, dy, vl),
                               __riscv_vfmul_vv_f32m2(ny, dz, vl),
                               vl);
    const vfloat32m2_t scaled_axis_dot =
        __riscv_vfmul_vv_f32m2(axis_dot_delta, scale, vl);

    vfloat32m2_t general_y =
        __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(nx, dy, vl),
                               __riscv_vfmul_vv_f32m2(ny, dx, vl),
                               vl);
    general_y = __riscv_vfmacc_vv_f32m2(general_y, nz, scaled_axis_dot, vl);
    vfloat32m2_t general_z =
        __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(nx, dz, vl),
                               __riscv_vfmul_vv_f32m2(nz, dx, vl),
                               vl);
    general_z = __riscv_vfnmsac_vv_f32m2(general_z, ny, scaled_axis_dot, vl);

    const vfloat32m2_t nx2 = __riscv_vfmul_vv_f32m2(nx, nx, vl);
    const vfloat32m2_t sin_angle =
        __riscv_vfsqrt_v_f32m2(__riscv_vfmax_vf_f32m2(__riscv_vfsub_vv_f32m2(one, nx2, vl), 0.0f, vl), vl);
    const vfloat32m2_t parallel_y = dy;
    const vfloat32m2_t parallel_z =
        __riscv_vfadd_vv_f32m2(__riscv_vfneg_v_f32m2(__riscv_vfmul_vv_f32m2(sin_angle, dx, vl), vl),
                               __riscv_vfmul_vv_f32m2(nx, dz, vl),
                               vl);

    const vfloat32m2_t transformed_y =
        __riscv_vmerge_vvm_f32m2(general_y, parallel_y, parallel_to_x, vl);
    const vfloat32m2_t transformed_z =
        __riscv_vmerge_vvm_f32m2(general_z, parallel_z, parallel_to_x, vl);
    const vfloat32m2_t alpha =
        pcl::atan2_RVV_f32m2(__riscv_vfneg_v_f32m2(transformed_z, vl), transformed_y, vl);
    __riscv_vse32_v_f32m2(alpha_m.data() + offset, alpha, vl);

    offset += vl;
  }
}
#endif

inline void
computePPFAlphaMBatchRVV(const XYZCloudT& cloud,
                         const NormalCloudT& normals,
                         const pcl::Indices& indices,
                         pcl::PointCloud<pcl::PPFSignature>& output)
{
  output.resize(indices.size() * cloud.size());
  output.height = 1;
  output.width = static_cast<std::uint32_t>(output.size());
  output.is_dense = true;

  for (auto& signature : output)
    setNaN(signature);
  if (!indices.empty())
    output.is_dense = false;

#if defined(__RVV10__)
  AlphaMStaging staging;
  reserveAlphaMStaging(staging, indices.size() * cloud.size());
  for (std::size_t index_i = 0; index_i < indices.size(); ++index_i)
  {
    const auto i = static_cast<std::size_t>(indices[index_i]);
    for (std::size_t j = 0; j < cloud.size(); ++j)
    {
      if (i == j)
        continue;

      auto& signature = output[index_i * cloud.size() + j];
      if (pcl::computePairFeatures(cloud[i].getVector4fMap(),
                                   normals[i].getNormalVector4fMap(),
                                   cloud[j].getVector4fMap(),
                                   normals[j].getNormalVector4fMap(),
                                   signature.f1,
                                   signature.f2,
                                   signature.f3,
                                   signature.f4))
      {
        appendAlphaMStaging(staging,
                            cloud[i],
                            normals[i],
                            cloud[j],
                            static_cast<std::uint32_t>(index_i * cloud.size() + j));
      }
      else
      {
        setNaN(signature);
        output.is_dense = false;
      }
    }
  }

  std::vector<float> alpha_m;
  computeAlphaMClosedFormRVV(staging, alpha_m);
  for (std::size_t k = 0; k < alpha_m.size(); ++k)
    output[staging.output_rows[k]].alpha_m = alpha_m[k];
#else
  computePPFReference(cloud, normals, indices, output);
#endif
}

} // namespace pcl::features::rvv_test::ppf
