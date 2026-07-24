/*
 * Compile-only coverage for RVV PCL field-tag load/store helpers.
 *
 * This file intentionally does not benchmark or run. It verifies that the
 * single-field helpers can be instantiated for registered single-float fields
 * without implying any production algorithm dispatch support for those point
 * types.
 */
#include <pcl/point_types.h>
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>

#include <cstddef>
#include <cstdint>

static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXY, pcl::fields::x>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXY, pcl::fields::y>::value);
static_assert(!pcl::rvv::RVVFloatFieldLayout<pcl::PointXY, pcl::fields::z>::value);

static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZI, pcl::fields::x>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZI, pcl::fields::intensity>::value);
static_assert(!pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZI, pcl::fields::normal_x>::value);

static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZINormal, pcl::fields::x>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZINormal, pcl::fields::intensity>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZINormal, pcl::fields::normal_x>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZINormal, pcl::fields::normal_y>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZINormal, pcl::fields::normal_z>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZINormal, pcl::fields::curvature>::value);
static_assert(pcl::rvv::RVVNormalFloatLayout<pcl::PointXYZINormal>::value);

static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::Normal, pcl::fields::normal_x>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::Normal, pcl::fields::normal_y>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::Normal, pcl::fields::normal_z>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::Normal, pcl::fields::curvature>::value);
static_assert(!pcl::rvv::RVVFloatFieldLayout<pcl::Normal, pcl::fields::x>::value);
static_assert(pcl::rvv::RVVNormalFloatLayout<pcl::Normal>::value);
static_assert(!pcl::rvv::RVVXYZFloatLayout<pcl::Normal>::value);

static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointNormal, pcl::fields::x>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointNormal, pcl::fields::normal_x>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointNormal, pcl::fields::normal_y>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointNormal, pcl::fields::normal_z>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<pcl::PointNormal, pcl::fields::curvature>::value);
static_assert(pcl::rvv::RVVNormalFloatLayout<pcl::PointNormal>::value);
static_assert(pcl::rvv::RVVXYZFloatLayout<pcl::PointNormal>::value);

static_assert(!pcl::rvv::RVVNormalFloatLayout<pcl::PointXYZI>::value);
static_assert(!pcl::rvv::RVVFloatFieldLayout<pcl::PointXYZI, pcl::fields::curvature>::value);

#if defined(__RVV10__)
namespace {

void
instantiate_pointxy_fields(const std::uint8_t* src_u8,
                           std::uint8_t* dst_u8,
                           const std::uint32_t* idx,
                           const std::size_t vl)
{
  const vuint32m2_t v_idx = __riscv_vle32_v_u32m2(idx, vl);
  const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXY>(v_idx, vl);
  const vfloat32m2_t vx =
      pcl::rvv_load::strided_load_field_f32m2<pcl::PointXY, pcl::fields::x>(src_u8, vl);
  const vfloat32m2_t vy =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::PointXY, pcl::fields::y>(src_u8, v_off, vl);
  pcl::rvv_store::strided_store_field_f32m2<pcl::PointXY, pcl::fields::x>(dst_u8, vx, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::PointXY, pcl::fields::y>(dst_u8, v_off, vy, vl);
}

void
instantiate_pointxyzi_fields(const std::uint8_t* src_u8,
                             std::uint8_t* dst_u8,
                             const std::uint32_t* idx,
                             const std::size_t vl)
{
  const vuint32m2_t v_idx = __riscv_vle32_v_u32m2(idx, vl);
  const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZI>(v_idx, vl);
  const vfloat32m2_t vi =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::PointXYZI, pcl::fields::intensity>(
          src_u8, v_off, vl);
  pcl::rvv_store::strided_store_field_f32m2<pcl::PointXYZI, pcl::fields::intensity>(
      dst_u8, vi, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::PointXYZI, pcl::fields::intensity>(
      dst_u8, v_off, vi, vl);
}

void
instantiate_pointxyzinormal_fields(const std::uint8_t* src_u8,
                                   std::uint8_t* dst_u8,
                                   const std::uint32_t* idx,
                                   const std::size_t vl)
{
  const vuint32m2_t v_idx = __riscv_vle32_v_u32m2(idx, vl);
  const vuint32m2_t v_off =
      pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZINormal>(v_idx, vl);
  const vfloat32m2_t vx =
      pcl::rvv_load::strided_load_field_f32m2<pcl::PointXYZINormal, pcl::fields::x>(
          src_u8, vl);
  const vfloat32m2_t vnx =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::PointXYZINormal, pcl::fields::normal_x>(
          src_u8, v_off, vl);
  const vfloat32m2_t vi =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::PointXYZINormal, pcl::fields::intensity>(
          src_u8, v_off, vl);
  pcl::rvv_store::strided_store_field_f32m2<pcl::PointXYZINormal, pcl::fields::x>(
      dst_u8, vx, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::PointXYZINormal, pcl::fields::normal_x>(
      dst_u8, v_off, vnx, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::PointXYZINormal, pcl::fields::intensity>(
      dst_u8, v_off, vi, vl);
}

void
instantiate_normal_fields(const std::uint8_t* src_u8,
                          std::uint8_t* dst_u8,
                          const std::uint32_t* idx,
                          const std::size_t vl)
{
  const vuint32m2_t v_idx = __riscv_vle32_v_u32m2(idx, vl);
  const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<pcl::Normal>(v_idx, vl);
  const vfloat32m2_t vnx =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::Normal, pcl::fields::normal_x>(
          src_u8, v_off, vl);
  const vfloat32m2_t vny =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::Normal, pcl::fields::normal_y>(
          src_u8, v_off, vl);
  const vfloat32m2_t vnz =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::Normal, pcl::fields::normal_z>(
          src_u8, v_off, vl);
  const vfloat32m2_t vc =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::Normal, pcl::fields::curvature>(
          src_u8, v_off, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::Normal, pcl::fields::normal_x>(
      dst_u8, v_off, vnx, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::Normal, pcl::fields::normal_y>(
      dst_u8, v_off, vny, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::Normal, pcl::fields::normal_z>(
      dst_u8, v_off, vnz, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::Normal, pcl::fields::curvature>(
      dst_u8, v_off, vc, vl);
}

void
instantiate_pointnormal_fields(const std::uint8_t* src_u8,
                               std::uint8_t* dst_u8,
                               const std::uint32_t* idx,
                               const std::size_t vl)
{
  const vuint32m2_t v_idx = __riscv_vle32_v_u32m2(idx, vl);
  const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<pcl::PointNormal>(v_idx, vl);
  const vfloat32m2_t vnx =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::PointNormal, pcl::fields::normal_x>(
          src_u8, v_off, vl);
  const vfloat32m2_t vc =
      pcl::rvv_load::indexed_load_field_f32m2<pcl::PointNormal, pcl::fields::curvature>(
          src_u8, v_off, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::PointNormal, pcl::fields::normal_x>(
      dst_u8, v_off, vnx, vl);
  pcl::rvv_store::scatter_store_field_f32m2<pcl::PointNormal, pcl::fields::curvature>(
      dst_u8, v_off, vc, vl);
}

} // namespace
#endif
