#ifndef TEST_RVV_SEGMENTATION_IMPL_EPPD_CANDIDATES_HPP_
#define TEST_RVV_SEGMENTATION_IMPL_EPPD_CANDIDATES_HPP_

#include <impl/eppd_reference.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace eppd {

inline bool
hasDenseOrderedIndices(const std::vector<int>& indices)
{
  for (std::size_t i = 0; i < indices.size(); ++i) {
    if (indices[i] != static_cast<int>(i)) {
      return false;
    }
  }
  return true;
}

inline bool
canUseUint32PointByteOffsets(const std::vector<int>& indices, std::size_t point_count)
{
  // indexed gather（索引离散加载）使用 32-bit byte offset；这个 gate 只证明
  // 当前 PointXYZ 测试资产的地址范围安全，production 泛型点类型必须另做 traits 审计。
  if (point_count >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() / sizeof(pcl::PointXYZ))) {
    return false;
  }

  for (const int index : indices) {
    if (index < 0 || static_cast<std::size_t>(index) >= point_count) {
      return false;
    }
  }
  return true;
}

#ifdef __RVV10__
template <typename PointContainer>
std::vector<int>
segmentPolygonalPrismRvvFullScanCandidate(
    const PointContainer& points,
    const std::vector<int>& indices,
    const std::vector<std::vector<pcl::PointXYZ>>& polygons,
    const std::vector<pcl::PointXYZ>& projected_points,
    const Eigen::Vector4f& model_coefficients,
    float height_min,
    float height_max,
    int k1,
    int k2,
    CandidatePath* path = nullptr)
{
  const bool dense_ordered = hasDenseOrderedIndices(indices);
  if (polygons.size() != 1 || projected_points.size() != indices.size() ||
      (!dense_ordered && !canUseUint32PointByteOffsets(indices, points.size()))) {
    if (path != nullptr) {
      *path = CandidatePath::ReferenceFallback;
    }
    return segmentPolygonalPrismFullScanReference(points,
                                                  indices,
                                                  polygons,
                                                  projected_points,
                                                  model_coefficients,
                                                  height_min,
                                                  height_max,
                                                  k1,
                                                  k2);
  }

  std::vector<int> output;
  output.reserve(indices.size());
  if (path != nullptr) {
    *path = dense_ordered ? CandidatePath::RvvDense : CandidatePath::RvvIndexed;
  }

  const auto* point_base = points.data();
  const auto* projected_base = projected_points.data();
  constexpr ptrdiff_t point_stride = static_cast<ptrdiff_t>(sizeof(pcl::PointXYZ));
  constexpr ptrdiff_t field_stride = static_cast<ptrdiff_t>(sizeof(float));
  const ptrdiff_t projected_axis1_offset = static_cast<ptrdiff_t>(k1) * field_stride;
  const ptrdiff_t projected_axis2_offset = static_cast<ptrdiff_t>(k2) * field_stride;

  for (std::size_t i = 0; i < indices.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const auto* projected_bytes =
        reinterpret_cast<const std::uint8_t*>(projected_base + i);

    vuint32m2_t source =
        __riscv_vadd_vx_u32m2(__riscv_vid_v_u32m2(vl), static_cast<std::uint32_t>(i), vl);
    vfloat32m2_t px;
    vfloat32m2_t py;
    vfloat32m2_t pz;
    if (dense_ordered) {
      const auto* point = point_base + i;
      px = __riscv_vlse32_v_f32m2(&point->x, point_stride, vl);
      py = __riscv_vlse32_v_f32m2(&point->y, point_stride, vl);
      pz = __riscv_vlse32_v_f32m2(&point->z, point_stride, vl);
    }
    else {
      // production full-scan 语义中，原始点由 indices 指向；projected_points 则已经
      // 按扫描顺序展开，所以这里只对原始 cloud 字段使用 gather。
      source = __riscv_vle32_v_u32m2(
          reinterpret_cast<const std::uint32_t*>(indices.data() + i), vl);
      const vuint32m2_t point_offsets =
          __riscv_vmul_vx_u32m2(source, static_cast<std::uint32_t>(sizeof(pcl::PointXYZ)), vl);
      px = __riscv_vluxei32_v_f32m2(&point_base->x, point_offsets, vl);
      py = __riscv_vluxei32_v_f32m2(&point_base->y, point_offsets, vl);
      pz = __riscv_vluxei32_v_f32m2(&point_base->z, point_offsets, vl);
    }

    vfloat32m2_t distance =
        __riscv_vfmul_vf_f32m2(px, model_coefficients[0], vl);
    distance = __riscv_vfmacc_vf_f32m2(distance, model_coefficients[1], py, vl);
    distance = __riscv_vfmacc_vf_f32m2(distance, model_coefficients[2], pz, vl);
    distance = __riscv_vfadd_vf_f32m2(distance, model_coefficients[3], vl);

    vbool16_t height = __riscv_vmfge_vf_f32m2_b16(distance, height_min, vl);
    height = __riscv_vmand_mm_b16(height, __riscv_vmfle_vf_f32m2_b16(distance, height_max, vl), vl);

    const auto* axis1_ptr = reinterpret_cast<const float*>(projected_bytes + projected_axis1_offset);
    const auto* axis2_ptr = reinterpret_cast<const float*>(projected_bytes + projected_axis2_offset);
    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(axis1_ptr, point_stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(axis2_ptr, point_stride, vl);

    vbool16_t in_any_polygon = __riscv_vmclr_m_b16(vl);
    for (const auto& polygon : polygons) {
      if (polygon.empty()) {
        continue;
      }

      vbool16_t in_poly = __riscv_vmclr_m_b16(vl);
      double xold = polygon.back().x;
      double yold = polygon.back().y;
      for (const auto& vertex : polygon) {
        const double xnew = vertex.x;
        const double ynew = vertex.y;
        const bool new_greater = xnew > xold;
        const float x1 = static_cast<float>(new_greater ? xold : xnew);
        const float x2 = static_cast<float>(new_greater ? xnew : xold);
        const float y1 = static_cast<float>(new_greater ? yold : ynew);
        const float y2 = static_cast<float>(new_greater ? ynew : yold);

        const vbool16_t left = __riscv_vmfgt_vf_f32m2_b16(vx, static_cast<float>(xnew), vl);
        const vbool16_t right = __riscv_vmfle_vf_f32m2_b16(vx, static_cast<float>(xold), vl);
        const vbool16_t same_side = __riscv_vmnot_m_b16(__riscv_vmxor_mm_b16(left, right, vl), vl);

        vfloat32m2_t lhs = __riscv_vfsub_vf_f32m2(vy, y1, vl);
        lhs = __riscv_vfmul_vf_f32m2(lhs, x2 - x1, vl);
        vfloat32m2_t rhs = __riscv_vfsub_vf_f32m2(vx, x1, vl);
        rhs = __riscv_vfmul_vf_f32m2(rhs, y2 - y1, vl);
        const vbool16_t below = __riscv_vmflt_vv_f32m2_b16(lhs, rhs, vl);
        const vbool16_t crosses = __riscv_vmand_mm_b16(same_side, below, vl);

        in_poly = __riscv_vmxor_mm_b16(in_poly, crosses, vl);
        xold = xnew;
        yold = ynew;
      }

      in_any_polygon = __riscv_vmxor_mm_b16(in_any_polygon, in_poly, vl);
    }

    const vbool16_t keep = __riscv_vmand_mm_b16(height, in_any_polygon, vl);
    const std::size_t kept = __riscv_vcpop_m_b16(keep, vl);
    if (kept != 0) {
      const vuint32m2_t compact = __riscv_vcompress_vm_u32m2(source, keep, vl);
      std::array<std::uint32_t, 64> packed{};
      const std::size_t vl_store = __riscv_vsetvl_e32m2(kept);
      __riscv_vse32_v_u32m2(packed.data(), compact, vl_store);
      for (std::size_t lane = 0; lane < kept; ++lane) {
        output.push_back(static_cast<int>(packed[lane]));
      }
    }

    i += vl;
  }

  return output;
}

template <typename PointContainer>
std::vector<int>
segmentPolygonalPrismRvvCandidate(const PointContainer& points,
                                  const std::vector<int>& indices,
                                  const std::vector<std::vector<pcl::PointXYZ>>& polygons,
                                  float height_min,
                                  float height_max)
{
  if (!hasDenseOrderedIndices(indices) || polygons.size() != 1) {
    return segmentPolygonalPrismReference(points, indices, polygons, height_min, height_max);
  }

  std::vector<int> output;
  output.reserve(indices.size());

  const auto* base = points.data();
  constexpr ptrdiff_t point_stride = static_cast<ptrdiff_t>(sizeof(pcl::PointXYZ));

  for (std::size_t i = 0; i < indices.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - i);
    const auto* point = base + i;

    const vfloat32m2_t vx = __riscv_vlse32_v_f32m2(&point->x, point_stride, vl);
    const vfloat32m2_t vy = __riscv_vlse32_v_f32m2(&point->y, point_stride, vl);
    const vfloat32m2_t vz = __riscv_vlse32_v_f32m2(&point->z, point_stride, vl);

    vbool16_t height = __riscv_vmfge_vf_f32m2_b16(vz, height_min, vl);
    height = __riscv_vmand_mm_b16(height, __riscv_vmfle_vf_f32m2_b16(vz, height_max, vl), vl);

    vbool16_t in_any_polygon = __riscv_vmclr_m_b16(vl);
    for (const auto& polygon : polygons) {
      if (polygon.empty()) {
        continue;
      }

      vbool16_t in_poly = __riscv_vmclr_m_b16(vl);
      double xold = polygon.back().x;
      double yold = polygon.back().y;
      for (const auto& vertex : polygon) {
        const double xnew = vertex.x;
        const double ynew = vertex.y;
        const bool new_greater = xnew > xold;
        const float x1 = static_cast<float>(new_greater ? xold : xnew);
        const float x2 = static_cast<float>(new_greater ? xnew : xold);
        const float y1 = static_cast<float>(new_greater ? yold : ynew);
        const float y2 = static_cast<float>(new_greater ? ynew : yold);

        const vbool16_t left = __riscv_vmfgt_vf_f32m2_b16(vx, static_cast<float>(xnew), vl);
        const vbool16_t right = __riscv_vmfle_vf_f32m2_b16(vx, static_cast<float>(xold), vl);
        const vbool16_t same_side = __riscv_vmnot_m_b16(__riscv_vmxor_mm_b16(left, right, vl), vl);

        vfloat32m2_t lhs = __riscv_vfsub_vf_f32m2(vy, y1, vl);
        lhs = __riscv_vfmul_vf_f32m2(lhs, x2 - x1, vl);
        vfloat32m2_t rhs = __riscv_vfsub_vf_f32m2(vx, x1, vl);
        rhs = __riscv_vfmul_vf_f32m2(rhs, y2 - y1, vl);
        const vbool16_t below = __riscv_vmflt_vv_f32m2_b16(lhs, rhs, vl);
        const vbool16_t crosses = __riscv_vmand_mm_b16(same_side, below, vl);

        in_poly = __riscv_vmxor_mm_b16(in_poly, crosses, vl);
        xold = xnew;
        yold = ynew;
      }

      in_any_polygon = __riscv_vmxor_mm_b16(in_any_polygon, in_poly, vl);
    }

    const vbool16_t keep = __riscv_vmand_mm_b16(height, in_any_polygon, vl);
    const std::size_t kept = __riscv_vcpop_m_b16(keep, vl);
    if (kept != 0) {
      const vuint32m2_t source =
          __riscv_vadd_vx_u32m2(__riscv_vid_v_u32m2(vl), static_cast<std::uint32_t>(i), vl);
      const vuint32m2_t compact = __riscv_vcompress_vm_u32m2(source, keep, vl);
      std::array<std::uint32_t, 64> packed{};
      const std::size_t vl_store = __riscv_vsetvl_e32m2(kept);
      __riscv_vse32_v_u32m2(packed.data(), compact, vl_store);
      for (std::size_t lane = 0; lane < kept; ++lane) {
        output.push_back(static_cast<int>(packed[lane]));
      }
    }

    i += vl;
  }

  return output;
}
#endif

} // namespace eppd

#endif // TEST_RVV_SEGMENTATION_IMPL_EPPD_CANDIDATES_HPP_
