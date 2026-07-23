/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2019-, Open Perception, Inc.
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

#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/cloud_iterator.h>
#include <pcl/field_traits.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace pcl {

namespace registration {

#if defined(__RVV10__)
namespace detail {

struct SymmetricPointNormalEquation {
  Eigen::Matrix<float, 6, 6> ata = Eigen::Matrix<float, 6, 6>::Zero();
  Eigen::Matrix<float, 6, 1> atb = Eigen::Matrix<float, 6, 1>::Zero();
};

inline vbool16_t
finiteMaskF32M2(vfloat32m2_t value, const std::size_t vl)
{
  const vfloat32m2_t abs_value = __riscv_vfabs_v_f32m2(value, vl);
  return __riscv_vmfle_vf_f32m2_b16(abs_value, std::numeric_limits<float>::max(), vl);
}

inline void
selectSymmetricNormalRVV(vfloat32m2_t n1x,
                         vfloat32m2_t n1y,
                         vfloat32m2_t n1z,
                         vfloat32m2_t n2x,
                         vfloat32m2_t n2y,
                         vfloat32m2_t n2z,
                         const bool enforce_same_direction_normals,
                         const std::size_t vl,
                         vfloat32m2_t& nx,
                         vfloat32m2_t& ny,
                         vfloat32m2_t& nz)
{
  const vfloat32m2_t add_x = __riscv_vfadd_vv_f32m2(n1x, n2x, vl);
  const vfloat32m2_t add_y = __riscv_vfadd_vv_f32m2(n1y, n2y, vl);
  const vfloat32m2_t add_z = __riscv_vfadd_vv_f32m2(n1z, n2z, vl);
  if (!enforce_same_direction_normals) {
    nx = add_x;
    ny = add_y;
    nz = add_z;
    return;
  }

  vfloat32m2_t dot = __riscv_vfmul_vv_f32m2(n1x, n2x, vl);
  dot = __riscv_vfadd_vv_f32m2(dot, __riscv_vfmul_vv_f32m2(n1y, n2y, vl), vl);
  dot = __riscv_vfadd_vv_f32m2(dot, __riscv_vfmul_vv_f32m2(n1z, n2z, vl), vl);
  const vbool16_t same_direction = __riscv_vmfge_vf_f32m2_b16(dot, 0.0f, vl);

  const vfloat32m2_t sub_x = __riscv_vfsub_vv_f32m2(n1x, n2x, vl);
  const vfloat32m2_t sub_y = __riscv_vfsub_vv_f32m2(n1y, n2y, vl);
  const vfloat32m2_t sub_z = __riscv_vfsub_vv_f32m2(n1z, n2z, vl);
  nx = __riscv_vmerge_vvm_f32m2(sub_x, add_x, same_direction, vl);
  ny = __riscv_vmerge_vvm_f32m2(sub_y, add_y, same_direction, vl);
  nz = __riscv_vmerge_vvm_f32m2(sub_z, add_z, same_direction, vl);
}

inline void
stageSymmetricFormulaRVV(vfloat32m2_t sx,
                         vfloat32m2_t sy,
                         vfloat32m2_t sz,
                         vfloat32m2_t tx,
                         vfloat32m2_t ty,
                         vfloat32m2_t tz,
                         vfloat32m2_t nx,
                         vfloat32m2_t ny,
                         vfloat32m2_t nz,
                         const std::size_t vl,
                         vfloat32m2_t& a,
                         vfloat32m2_t& b,
                         vfloat32m2_t& c,
                         vfloat32m2_t& d)
{
  const vfloat32m2_t px = __riscv_vfadd_vv_f32m2(sx, tx, vl);
  const vfloat32m2_t py = __riscv_vfadd_vv_f32m2(sy, ty, vl);
  const vfloat32m2_t pz = __riscv_vfadd_vv_f32m2(sz, tz, vl);
  const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(tx, sx, vl);
  const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(ty, sy, vl);
  const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(tz, sz, vl);

  a = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(py, nz, vl),
                             __riscv_vfmul_vv_f32m2(pz, ny, vl),
                             vl);
  b = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(pz, nx, vl),
                             __riscv_vfmul_vv_f32m2(px, nz, vl),
                             vl);
  c = __riscv_vfsub_vv_f32m2(__riscv_vfmul_vv_f32m2(px, ny, vl),
                             __riscv_vfmul_vv_f32m2(py, nx, vl),
                             vl);
  d = __riscv_vfmul_vv_f32m2(dx, nx, vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(dy, ny, vl), vl);
  d = __riscv_vfadd_vv_f32m2(d, __riscv_vfmul_vv_f32m2(dz, nz, vl), vl);
}

inline void
accumulateSymmetricRowsRVV(vfloat32m2_t a,
                           vfloat32m2_t b,
                           vfloat32m2_t c,
                           vfloat32m2_t d,
                           vfloat32m2_t nx,
                           vfloat32m2_t ny,
                           vfloat32m2_t nz,
                           vbool16_t keep,
                           const std::size_t vl,
                           SymmetricPointNormalEquation& eq)
{
  alignas(16) float a_buf[64];
  alignas(16) float b_buf[64];
  alignas(16) float c_buf[64];
  alignas(16) float d_buf[64];
  alignas(16) float nx_buf[64];
  alignas(16) float ny_buf[64];
  alignas(16) float nz_buf[64];
  const std::size_t kept = __riscv_vcpop_m_b16(keep, vl);
  if (kept == 0)
    return;

  __riscv_vse32_v_f32m2(a_buf, __riscv_vcompress_vm_f32m2(a, keep, vl), kept);
  __riscv_vse32_v_f32m2(b_buf, __riscv_vcompress_vm_f32m2(b, keep, vl), kept);
  __riscv_vse32_v_f32m2(c_buf, __riscv_vcompress_vm_f32m2(c, keep, vl), kept);
  __riscv_vse32_v_f32m2(d_buf, __riscv_vcompress_vm_f32m2(d, keep, vl), kept);
  __riscv_vse32_v_f32m2(nx_buf, __riscv_vcompress_vm_f32m2(nx, keep, vl), kept);
  __riscv_vse32_v_f32m2(ny_buf, __riscv_vcompress_vm_f32m2(ny, keep, vl), kept);
  __riscv_vse32_v_f32m2(nz_buf, __riscv_vcompress_vm_f32m2(nz, keep, vl), kept);

  for (std::size_t lane = 0; lane < kept; ++lane) {
    const float a_f = a_buf[lane];
    const float b_f = b_buf[lane];
    const float c_f = c_buf[lane];
    const float d_f = d_buf[lane];
    const float nx_f = nx_buf[lane];
    const float ny_f = ny_buf[lane];
    const float nz_f = nz_buf[lane];

    eq.ata.coeffRef(0) += a_f * a_f;
    eq.ata.coeffRef(1) += a_f * b_f;
    eq.ata.coeffRef(2) += a_f * c_f;
    eq.ata.coeffRef(3) += a_f * nx_f;
    eq.ata.coeffRef(4) += a_f * ny_f;
    eq.ata.coeffRef(5) += a_f * nz_f;
    eq.ata.coeffRef(7) += b_f * b_f;
    eq.ata.coeffRef(8) += b_f * c_f;
    eq.ata.coeffRef(9) += b_f * nx_f;
    eq.ata.coeffRef(10) += b_f * ny_f;
    eq.ata.coeffRef(11) += b_f * nz_f;
    eq.ata.coeffRef(14) += c_f * c_f;
    eq.ata.coeffRef(15) += c_f * nx_f;
    eq.ata.coeffRef(16) += c_f * ny_f;
    eq.ata.coeffRef(17) += c_f * nz_f;
    eq.ata.coeffRef(21) += nx_f * nx_f;
    eq.ata.coeffRef(22) += nx_f * ny_f;
    eq.ata.coeffRef(23) += nx_f * nz_f;
    eq.ata.coeffRef(28) += ny_f * ny_f;
    eq.ata.coeffRef(29) += ny_f * nz_f;
    eq.ata.coeffRef(35) += nz_f * nz_f;

    eq.atb.coeffRef(0) += a_f * d_f;
    eq.atb.coeffRef(1) += b_f * d_f;
    eq.atb.coeffRef(2) += c_f * d_f;
    eq.atb.coeffRef(3) += nx_f * d_f;
    eq.atb.coeffRef(4) += ny_f * d_f;
    eq.atb.coeffRef(5) += nz_f * d_f;
  }
}

inline void
completeSymmetricPointNormalEquation(SymmetricPointNormalEquation& eq)
{
  eq.ata.coeffRef(6) = eq.ata.coeff(1);
  eq.ata.coeffRef(12) = eq.ata.coeff(2);
  eq.ata.coeffRef(13) = eq.ata.coeff(8);
  eq.ata.coeffRef(18) = eq.ata.coeff(3);
  eq.ata.coeffRef(19) = eq.ata.coeff(9);
  eq.ata.coeffRef(20) = eq.ata.coeff(15);
  eq.ata.coeffRef(24) = eq.ata.coeff(4);
  eq.ata.coeffRef(25) = eq.ata.coeff(10);
  eq.ata.coeffRef(26) = eq.ata.coeff(16);
  eq.ata.coeffRef(27) = eq.ata.coeff(22);
  eq.ata.coeffRef(30) = eq.ata.coeff(5);
  eq.ata.coeffRef(31) = eq.ata.coeff(11);
  eq.ata.coeffRef(32) = eq.ata.coeff(17);
  eq.ata.coeffRef(33) = eq.ata.coeff(23);
  eq.ata.coeffRef(34) = eq.ata.coeff(29);
}

inline Eigen::Matrix4f
constructSymmetricPointNormalTransform(const Eigen::Matrix<float, 6, 1>& parameters)
{
  const Eigen::AngleAxisf rotation_z(parameters(2), Eigen::Vector3f::UnitZ());
  const Eigen::AngleAxisf rotation_y(parameters(1), Eigen::Vector3f::UnitY());
  const Eigen::AngleAxisf rotation_x(parameters(0), Eigen::Vector3f::UnitX());
  const Eigen::Translation<float, 3> translation(
      parameters(3), parameters(4), parameters(5));
  const Eigen::Transform<float, 3, Eigen::Affine> transform =
      rotation_z * rotation_y * rotation_x * translation * rotation_z * rotation_y *
      rotation_x;
  return transform.matrix();
}

template <typename RowSourcePolicy>
inline bool
estimateSymmetricPointNormalRowsRVV(
    const typename RowSourcePolicy::Context& context,
    const bool enforce_same_direction_normals,
    Eigen::Matrix4f& transformation_matrix)
{
  using PointSource = typename RowSourcePolicy::PointSource;
  using PointTarget = typename RowSourcePolicy::PointTarget;

  if constexpr (!pcl::rvv::RVVXYZNormalFloatLayout<PointSource>::value ||
                !pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>::value) {
    return false;
  } else {
    const std::size_t nr_points = RowSourcePolicy::row_count(context);
    if (!RowSourcePolicy::can_use_rvv(context, nr_points))
      return false;

    SymmetricPointNormalEquation eq;
    const auto* src_base =
        reinterpret_cast<const std::uint8_t*>(context.source.points.data());
    const auto* tgt_base =
        reinterpret_cast<const std::uint8_t*>(context.target.points.data());
    for (std::size_t i = 0; i < nr_points;) {
      const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
      vfloat32m2_t sx, sy, sz, tx, ty, tz, n1x, n1y, n1z, n2x, n2y, n2z;
      RowSourcePolicy::load_chunk(context,
                                  src_base,
                                  tgt_base,
                                  i,
                                  vl,
                                  sx,
                                  sy,
                                  sz,
                                  tx,
                                  ty,
                                  tz,
                                  n1x,
                                  n1y,
                                  n1z,
                                  n2x,
                                  n2y,
                                  n2z);

      vfloat32m2_t nx, ny, nz;
      selectSymmetricNormalRVV(n1x,
                               n1y,
                               n1z,
                               n2x,
                               n2y,
                               n2z,
                               enforce_same_direction_normals,
                               vl,
                               nx,
                               ny,
                               nz);

      vbool16_t keep = finiteMaskF32M2(sx, vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(sy, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(sz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(tx, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(ty, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(tz, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(nx, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(ny, vl), vl);
      keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2(nz, vl), vl);

      vfloat32m2_t a, b, c, d;
      stageSymmetricFormulaRVV(sx, sy, sz, tx, ty, tz, nx, ny, nz, vl, a, b, c, d);
      accumulateSymmetricRowsRVV(a, b, c, d, nx, ny, nz, keep, vl, eq);
      i += vl;
    }

    completeSymmetricPointNormalEquation(eq);
    const Eigen::Matrix<float, 6, 1> x =
        eq.ata.template selfadjointView<Eigen::Upper>().ldlt().solve(eq.atb);
    transformation_matrix = constructSymmetricPointNormalTransform(x);
    return true;
  }
}

template <typename SourceT, typename TargetT>
struct SymmetricFullCloudRowSource {
  using PointSource = SourceT;
  using PointTarget = TargetT;

  struct Context {
    const pcl::PointCloud<PointSource>& source;
    const pcl::PointCloud<PointTarget>& target;
  };

  static std::size_t
  row_count(const Context& context)
  {
    return std::min(context.source.size(), context.target.size());
  }

  static bool
  can_use_rvv(const Context&, const std::size_t nr_points)
  {
    return nr_points >= 64 && __riscv_vsetvlmax_e32m2() <= 64 &&
           nr_points <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() &&
           nr_points <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>();
  }

  static void
  load_chunk(const Context&,
             const std::uint8_t* source_base,
             const std::uint8_t* target_base,
             const std::size_t i,
             const std::size_t vl,
             vfloat32m2_t& sx,
             vfloat32m2_t& sy,
             vfloat32m2_t& sz,
             vfloat32m2_t& tx,
             vfloat32m2_t& ty,
             vfloat32m2_t& tz,
             vfloat32m2_t& n1x,
             vfloat32m2_t& n1y,
             vfloat32m2_t& n1z,
             vfloat32m2_t& n2x,
             vfloat32m2_t& n2y,
             vfloat32m2_t& n2z)
  {
    using SrcLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointSource>;
    using TgtLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>;
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(PointSource),
                                                SrcLayout::kX,
                                                SrcLayout::kY,
                                                SrcLayout::kZ>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(PointTarget),
                                                TgtLayout::kX,
                                                TgtLayout::kY,
                                                TgtLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(PointSource),
                                                SrcLayout::kNX,
                                                SrcLayout::kNY,
                                                SrcLayout::kNZ>(
        source_base + i * sizeof(PointSource), vl, n1x, n1y, n1z);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(PointTarget),
                                                TgtLayout::kNX,
                                                TgtLayout::kNY,
                                                TgtLayout::kNZ>(
        target_base + i * sizeof(PointTarget), vl, n2x, n2y, n2z);
  }
};

template <typename SourceT, typename TargetT>
struct SymmetricSourceIndexedRowSource {
  using PointSource = SourceT;
  using PointTarget = TargetT;

  struct Context {
    const pcl::PointCloud<PointSource>& source;
    const pcl::Indices& source_indices;
    const pcl::PointCloud<PointTarget>& target;
  };

  static std::size_t
  row_count(const Context& context)
  {
    return context.source_indices.size();
  }

  static bool
  can_use_rvv(const Context& context, const std::size_t nr_points)
  {
    // The public indices overload inherits PCL's valid-index precondition.
    // RVV only checks that every valid source index can fit in a 32-bit byte offset.
    return context.target.size() == nr_points && nr_points >= 64 &&
           __riscv_vsetvlmax_e32m2() <= 64 &&
           context.source.size() <=
               pcl::rvv::rvvMaxU32ByteOffsetElements<PointSource>() &&
           context.target.size() <=
               pcl::rvv::rvvMaxU32ByteOffsetElements<PointTarget>();
  }

  static void
  load_chunk(const Context& context,
             const std::uint8_t* source_base,
             const std::uint8_t* target_base,
             const std::size_t i,
             const std::size_t vl,
             vfloat32m2_t& sx,
             vfloat32m2_t& sy,
             vfloat32m2_t& sz,
             vfloat32m2_t& tx,
             vfloat32m2_t& ty,
             vfloat32m2_t& tz,
             vfloat32m2_t& n1x,
             vfloat32m2_t& n1y,
             vfloat32m2_t& n1z,
             vfloat32m2_t& n2x,
             vfloat32m2_t& n2y,
             vfloat32m2_t& n2z)
  {
    using SrcLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointSource>;
    using TgtLayout = pcl::rvv::RVVXYZNormalFloatLayout<PointTarget>;
    using SrcPod = typename SrcLayout::Pod;
    const vint32m2_t source_indices =
        __riscv_vle32_v_i32m2(context.source_indices.data() + i, vl);
    const vuint32m2_t source_offsets =
        pcl::rvv_load::byte_offsets_u32m2<SrcPod>(
            __riscv_vreinterpret_v_i32m2_u32m2(source_indices), vl);
    pcl::rvv_load::indexed_load3_fields_f32m2<SrcPod,
                                               SrcLayout::kX,
                                               SrcLayout::kY,
                                               SrcLayout::kZ>(
        source_base, source_offsets, vl, sx, sy, sz);
    pcl::rvv_load::indexed_load3_fields_f32m2<SrcPod,
                                               SrcLayout::kNX,
                                               SrcLayout::kNY,
                                               SrcLayout::kNZ>(
        source_base, source_offsets, vl, n1x, n1y, n1z);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(PointTarget),
                                                TgtLayout::kX,
                                                TgtLayout::kY,
                                                TgtLayout::kZ>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    pcl::rvv_load::strided_load3_fields_f32m2<sizeof(PointTarget),
                                                TgtLayout::kNX,
                                                TgtLayout::kNY,
                                                TgtLayout::kNZ>(
        target_base + i * sizeof(PointTarget), vl, n2x, n2y, n2z);
  }
};

template <typename PointSource, typename PointTarget>
inline bool
estimateSymmetricPointNormalFullCloudRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const bool enforce_same_direction_normals,
    Eigen::Matrix4f& transformation_matrix)
{
  using RowSource = SymmetricFullCloudRowSource<PointSource, PointTarget>;
  return estimateSymmetricPointNormalRowsRVV<RowSource>(
      typename RowSource::Context{cloud_src, cloud_tgt},
      enforce_same_direction_normals,
      transformation_matrix);
}

template <typename PointSource, typename PointTarget>
inline bool
estimateSymmetricPointNormalSourceIndicesRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const bool enforce_same_direction_normals,
    Eigen::Matrix4f& transformation_matrix)
{
  using RowSource = SymmetricSourceIndexedRowSource<PointSource, PointTarget>;
  return estimateSymmetricPointNormalRowsRVV<RowSource>(
      typename RowSource::Context{cloud_src, indices_src, cloud_tgt},
      enforce_same_direction_normals,
      transformation_matrix);
}

} // namespace detail
#endif // defined(__RVV10__)

namespace detail {

template <typename Scalar>
inline void
constructSymmetricPointNormalTransformationMatrix(
    const Eigen::Matrix<Scalar, 6, 1>& parameters,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  const Eigen::AngleAxis<Scalar> rotation_z(parameters(2),
                                            Eigen::Matrix<Scalar, 3, 1>::UnitZ());
  const Eigen::AngleAxis<Scalar> rotation_y(parameters(1),
                                            Eigen::Matrix<Scalar, 3, 1>::UnitY());
  const Eigen::AngleAxis<Scalar> rotation_x(parameters(0),
                                            Eigen::Matrix<Scalar, 3, 1>::UnitX());
  const Eigen::Translation<Scalar, 3> translation(
      parameters(3), parameters(4), parameters(5));
  const Eigen::Transform<Scalar, 3, Eigen::Affine> transform =
      rotation_z * rotation_y * rotation_x * translation * rotation_z * rotation_y *
      rotation_x;
  transformation_matrix = transform.matrix();
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
estimateSymmetricPointNormalStd(ConstCloudIterator<PointSource>& source_it,
                                ConstCloudIterator<PointTarget>& target_it,
                                const bool enforce_same_direction_normals,
                                Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  using Matrix6 = Eigen::Matrix<Scalar, 6, 6>;
  using Vector3 = Eigen::Matrix<Scalar, 3, 1>;
  using Vector6 = Eigen::Matrix<Scalar, 6, 1>;

  Matrix6 ATA;
  Vector6 ATb;
  ATA.setZero();
  ATb.setZero();
  auto M = ATA.template selfadjointView<Eigen::Upper>();

  // Approximate as a linear least squares problem
  source_it.reset();
  target_it.reset();
  for (; source_it.isValid() && target_it.isValid(); ++source_it, ++target_it) {
    const Vector3 p(source_it->x, source_it->y, source_it->z);
    const Vector3 q(target_it->x, target_it->y, target_it->z);
    const Vector3 n1(source_it->getNormalVector3fMap().template cast<Scalar>());
    const Vector3 n2(target_it->getNormalVector3fMap().template cast<Scalar>());
    Vector3 n;
    if (enforce_same_direction_normals) {
      if (n1.dot(n2) >= 0.)
        n = n1 + n2;
      else
        n = n1 - n2;
    }
    else {
      n = n1 + n2;
    }

    if (!p.array().isFinite().all() || !q.array().isFinite().all() ||
        !n.array().isFinite().all()) {
      continue;
    }

    Vector6 v;
    v << (p + q).cross(n), n;
    M.rankUpdate(v);

    ATb += v * (q - p).dot(n);
  }

  // Solve A*x = b
  const Vector6 x = M.ldlt().solve(ATb);

  // Construct the transformation matrix from x
  constructSymmetricPointNormalTransformationMatrix<Scalar>(x, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
estimateSymmetricPointNormalFullCloudStd(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const bool enforce_same_direction_normals,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  ConstCloudIterator<PointSource> source_it(cloud_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateSymmetricPointNormalStd<PointSource, PointTarget, Scalar>(
      source_it, target_it, enforce_same_direction_normals, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
estimateSymmetricPointNormalSourceIndicesStd(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const bool enforce_same_direction_normals,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateSymmetricPointNormalStd<PointSource, PointTarget, Scalar>(
      source_it, target_it, enforce_same_direction_normals, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
estimateSymmetricPointNormalSourceTargetIndicesStd(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::Indices& indices_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Indices& indices_tgt,
    const bool enforce_same_direction_normals,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, indices_tgt);
  estimateSymmetricPointNormalStd<PointSource, PointTarget, Scalar>(
      source_it, target_it, enforce_same_direction_normals, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
estimateSymmetricPointNormalCorrespondencesStd(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    const pcl::Correspondences& correspondences,
    const bool enforce_same_direction_normals,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);
  estimateSymmetricPointNormalStd<PointSource, PointTarget, Scalar>(
      source_it, target_it, enforce_same_direction_normals, transformation_matrix);
}

} // namespace detail

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  const auto nr_points = cloud_src.size();
  if (cloud_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationSymmetricPointToPlaneLLS::"
              "estimateRigidTransformation] Number or points in source (%zu) differs "
              "from target (%zu)!\n",
              static_cast<std::size_t>(nr_points),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

#if defined(__RVV10__)
  if constexpr (std::is_same_v<Scalar, float>) {
    if (detail::estimateSymmetricPointNormalFullCloudRVV(
            cloud_src, cloud_tgt, enforce_same_direction_normals_, transformation_matrix))
      return;
  }
#endif // defined(__RVV10__)

  detail::estimateSymmetricPointNormalFullCloudStd<PointSource, PointTarget, Scalar>(
      cloud_src, cloud_tgt, enforce_same_direction_normals_, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  const auto nr_points = indices_src.size();
  if (cloud_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationSymmetricPointToPlaneLLS::"
              "estimateRigidTransformation] Number or points in source (%zu) differs "
              "than target (%zu)!\n",
              indices_src.size(),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

#if defined(__RVV10__)
  if constexpr (std::is_same_v<Scalar, float>) {
    if (detail::estimateSymmetricPointNormalSourceIndicesRVV(
            cloud_src,
            indices_src,
            cloud_tgt,
            enforce_same_direction_normals_,
            transformation_matrix))
      return;
  }
#endif // defined(__RVV10__)

  detail::estimateSymmetricPointNormalSourceIndicesStd<PointSource, PointTarget, Scalar>(
      cloud_src,
      indices_src,
      cloud_tgt,
      enforce_same_direction_normals_,
      transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Indices& indices_tgt,
                                Matrix4& transformation_matrix) const
{
  const auto nr_points = indices_src.size();
  if (indices_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimationSymmetricPointToPlaneLLS::"
              "estimateRigidTransformation] Number or points in source (%zu) differs "
              "than target (%zu)!\n",
              indices_src.size(),
              indices_tgt.size());
    return;
  }

  detail::estimateSymmetricPointNormalSourceTargetIndicesStd<
      PointSource,
      PointTarget,
      Scalar>(cloud_src,
              indices_src,
              cloud_tgt,
              indices_tgt,
              enforce_same_direction_normals_,
              transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Correspondences& correspondences,
                                Matrix4& transformation_matrix) const
{
  detail::estimateSymmetricPointNormalCorrespondencesStd<PointSource,
                                                         PointTarget,
                                                         Scalar>(
      cloud_src,
      cloud_tgt,
      correspondences,
      enforce_same_direction_normals_,
      transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    constructTransformationMatrix(const Vector6& parameters,
                                  Matrix4& transformation_matrix) const
{
  detail::constructSymmetricPointNormalTransformationMatrix<Scalar>(
      parameters, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(ConstCloudIterator<PointSource>& source_it,
                                ConstCloudIterator<PointTarget>& target_it,
                                Matrix4& transformation_matrix) const
{
  detail::estimateSymmetricPointNormalStd<PointSource, PointTarget, Scalar>(
      source_it, target_it, enforce_same_direction_normals_, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    setEnforceSameDirectionNormals(bool enforce_same_direction_normals)
{
  enforce_same_direction_normals_ = enforce_same_direction_normals;
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
TransformationEstimationSymmetricPointToPlaneLLS<PointSource, PointTarget, Scalar>::
    getEnforceSameDirectionNormals()
{
  return enforce_same_direction_normals_;
}

} // namespace registration
} // namespace pcl
