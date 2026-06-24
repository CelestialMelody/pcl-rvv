#pragma once

#include <pcl/common/rvv_point_load.h>
#include <pcl/field_traits.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/correspondence_estimation_organized_projection.h>

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace pcl::registration::correspondence_estimation_organized_projection_diag {

using Matrix4f = Eigen::Matrix4f;

enum class DiagMode { Std, Candidate, Projected, ProjectedIdentity, Accepted };

struct ProjectionParams {
  Matrix4f transform{Matrix4f::Identity()};
  float fx{525.0f};
  float fy{525.0f};
  float cx{319.5f};
  float cy{239.5f};
  float depth_threshold{std::numeric_limits<float>::max()};
  double max_distance{1.0};
};

struct ProjectionCandidate {
  std::uint32_t source_index;
  float x;
  float y;
  float z;
};

struct ProjectedCandidate {
  std::uint32_t source_index;
  std::uint32_t target_index;
  float x;
  float y;
  float z;
};

struct AcceptedCandidate {
  std::uint32_t source_index;
  std::uint32_t target_index;
  float distance;
};

template <typename PointT, bool HasXYZ = pcl::traits::has_xyz<PointT>::value>
struct XYZFloatLayout : std::false_type {};

template <typename PointT>
struct XYZFloatLayout<PointT, true>
: std::bool_constant<
      std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::x>::decomposed::type, float> &&
      std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::y>::decomposed::type, float> &&
      std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::z>::decomposed::type, float> &&
      pcl::traits::datatype<PointT, pcl::fields::x>::decomposed::value == 1 &&
      pcl::traits::datatype<PointT, pcl::fields::y>::decomposed::value == 1 &&
      pcl::traits::datatype<PointT, pcl::fields::z>::decomposed::value == 1> {};

template <typename PointT>
inline constexpr bool kXYZFloatLayoutV = XYZFloatLayout<PointT>::value;

template <typename PointT>
inline bool
isFiniteXYZ(const PointT& point)
{
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline bool
transformIsExactIdentity(const Matrix4f& transform)
{
  return transform(0, 0) == 1.0f && transform(0, 1) == 0.0f &&
         transform(0, 2) == 0.0f && transform(0, 3) == 0.0f &&
         transform(1, 0) == 0.0f && transform(1, 1) == 1.0f &&
         transform(1, 2) == 0.0f && transform(1, 3) == 0.0f &&
         transform(2, 0) == 0.0f && transform(2, 1) == 0.0f &&
         transform(2, 2) == 1.0f && transform(2, 3) == 0.0f &&
         transform(3, 0) == 0.0f && transform(3, 1) == 0.0f &&
         transform(3, 2) == 0.0f && transform(3, 3) == 1.0f;
}

template <typename PointSource, typename PointTarget>
inline void
determineCorrespondencesStd(const pcl::PointCloud<PointSource>& input,
                            const pcl::PointCloud<PointTarget>& target,
                            const pcl::Indices& indices,
                            const ProjectionParams& params,
                            pcl::Correspondences& correspondences)
{
  correspondences.resize(indices.size());
  std::size_t c_index = 0;

  for (const auto& src_idx : indices) {
    if (isFiniteXYZ(input[src_idx])) {
      const Eigen::Vector4f p_src(params.transform * input[src_idx].getVector4fMap());
      const Eigen::Vector3f p_src3(p_src[0], p_src[1], p_src[2]);
      const float uv0 = params.fx * p_src3[0] + params.cx * p_src3[2];
      const float uv1 = params.fy * p_src3[1] + params.cy * p_src3[2];
      const float uv2 = p_src3[2];

      if (uv2 <= 0.0f)
        continue;

      const int u = static_cast<int>(uv0 / uv2);
      const int v = static_cast<int>(uv1 / uv2);

      if (u >= 0 && u < static_cast<int>(target.width) && v >= 0 &&
          v < static_cast<int>(target.height)) {
        const PointTarget& pt_tgt = target.at(u, v);
        if (!isFiniteXYZ(pt_tgt))
          continue;
        if (std::abs(uv2 - pt_tgt.z) > params.depth_threshold)
          continue;

        const float dx = p_src3[0] - pt_tgt.x;
        const float dy = p_src3[1] - pt_tgt.y;
        const float dz = p_src3[2] - pt_tgt.z;
        const double dist = std::sqrt(static_cast<double>(dx) * dx +
                                      static_cast<double>(dy) * dy +
                                      static_cast<double>(dz) * dz);
        if (dist < params.max_distance)
          correspondences[c_index++] = pcl::Correspondence(
              src_idx, v * target.width + u, static_cast<float>(dist));
      }
    }
  }

  correspondences.resize(c_index);
}

#if defined(__RVV10__)

inline vbool16_t
finiteMask(vfloat32m2_t values, std::size_t vl)
{
  const vbool16_t eq_self = __riscv_vmfeq_vv_f32m2_b16(values, values, vl);
  const vfloat32m2_t abs_v = __riscv_vfabs_v_f32m2(values, vl);
  const vfloat32m2_t inf_v =
      __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::infinity(), vl);
  const vbool16_t not_inf = __riscv_vmflt_vv_f32m2_b16(abs_v, inf_v, vl);
  return __riscv_vmand_mm_b16(eq_self, not_inf, vl);
}

// Low-level source-candidate RVV stage. It mirrors the production source
// staging boundary: indexed source xyz gather, source finite mask, Eigen-aligned
// 4x4 transform, positive transformed depth, and ordered candidate compression.
template <typename PointSource, typename PointTarget>
inline bool
projectCandidatesRVV(const pcl::PointCloud<PointSource>& input,
                     const pcl::PointCloud<PointTarget>& target,
                     const pcl::Indices& indices,
                     const ProjectionParams& params,
                     std::vector<ProjectionCandidate>& candidates)
{
  if constexpr (!kXYZFloatLayoutV<PointSource> || !kXYZFloatLayoutV<PointTarget>) {
    return false;
  } else {
    constexpr std::size_t kXOff = pcl::traits::offset<PointSource, pcl::fields::x>::value;
    constexpr std::size_t kYOff = pcl::traits::offset<PointSource, pcl::fields::y>::value;
    constexpr std::size_t kZOff = pcl::traits::offset<PointSource, pcl::fields::z>::value;

    const std::size_t n = indices.size();
    if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        input.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        target.width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        target.height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
      return false;

    const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
    if (vlmax > 64)
      return false;

    candidates.resize(n);
    auto* out = candidates.data();
    std::size_t kept = 0;
    std::size_t i = 0;
    const auto* base = reinterpret_cast<const std::uint8_t*>(input.points.data());

    alignas(16) std::uint32_t source_buf[64];
    alignas(16) float x_buf[64];
    alignas(16) float y_buf[64];
    alignas(16) float z_buf[64];

    while (i < n) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
      const vuint32m2_t v_indices_u = __riscv_vreinterpret_v_i32m2_u32m2(v_indices);
      const vuint32m2_t offsets =
          __riscv_vmul_vx_u32m2(v_indices_u, static_cast<std::uint32_t>(sizeof(PointSource)), vl);

      vfloat32m2_t x;
      vfloat32m2_t y;
      vfloat32m2_t z;
      pcl::rvv_load::indexed_load3_fields_f32m2<
          typename pcl::traits::POD<PointSource>::type, kXOff, kYOff, kZOff>(
          base, offsets, vl, x, y, z);

      // Match Eigen's row-dot lowering seen in the scalar RVV build:
      // (m0*x + m1*y) + (m2*z + m3*w), with each pair formed by one FMA.
      vfloat32m2_t tx_lo = __riscv_vfmul_vf_f32m2(y, params.transform(0, 1), vl);
      tx_lo = __riscv_vfmacc_vf_f32m2(tx_lo, params.transform(0, 0), x, vl);
      vfloat32m2_t tx_hi = __riscv_vfmv_v_f_f32m2(params.transform(0, 3), vl);
      tx_hi = __riscv_vfmacc_vf_f32m2(tx_hi, params.transform(0, 2), z, vl);
      vfloat32m2_t tx = __riscv_vfadd_vv_f32m2(tx_hi, tx_lo, vl);

      vfloat32m2_t ty_lo = __riscv_vfmul_vf_f32m2(y, params.transform(1, 1), vl);
      ty_lo = __riscv_vfmacc_vf_f32m2(ty_lo, params.transform(1, 0), x, vl);
      vfloat32m2_t ty_hi = __riscv_vfmv_v_f_f32m2(params.transform(1, 3), vl);
      ty_hi = __riscv_vfmacc_vf_f32m2(ty_hi, params.transform(1, 2), z, vl);
      vfloat32m2_t ty = __riscv_vfadd_vv_f32m2(ty_hi, ty_lo, vl);

      vfloat32m2_t tz_lo = __riscv_vfmul_vf_f32m2(y, params.transform(2, 1), vl);
      tz_lo = __riscv_vfmacc_vf_f32m2(tz_lo, params.transform(2, 0), x, vl);
      vfloat32m2_t tz_hi = __riscv_vfmv_v_f_f32m2(params.transform(2, 3), vl);
      tz_hi = __riscv_vfmacc_vf_f32m2(tz_hi, params.transform(2, 2), z, vl);
      vfloat32m2_t tz = __riscv_vfadd_vv_f32m2(tz_hi, tz_lo, vl);

      // This staging mask only removes source lanes the scalar path would skip
      // before projection. u/v truncation and target access stay scalar, avoiding
      // boundary differences from changing the projection arithmetic order.
      vbool16_t keep = __riscv_vmand_mm_b16(
          __riscv_vmand_mm_b16(finiteMask(x, vl), finiteMask(y, vl), vl),
          finiteMask(z, vl),
          vl);
      keep = __riscv_vmand_mm_b16(keep, __riscv_vmfgt_vf_f32m2_b16(tz, 0.0f, vl), vl);

      const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(v_indices_u, keep, vl);
      const vfloat32m2_t x_kept = __riscv_vcompress_vm_f32m2(tx, keep, vl);
      const vfloat32m2_t y_kept = __riscv_vcompress_vm_f32m2(ty, keep, vl);
      const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(tz, keep, vl);
      const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

      __riscv_vse32_v_u32m2(source_buf, source_kept, keep_count);
      __riscv_vse32_v_f32m2(x_buf, x_kept, keep_count);
      __riscv_vse32_v_f32m2(y_buf, y_kept, keep_count);
      __riscv_vse32_v_f32m2(z_buf, z_kept, keep_count);

      for (std::size_t lane = 0; lane < keep_count; ++lane) {
        out[kept + lane] =
            ProjectionCandidate{source_buf[lane], x_buf[lane], y_buf[lane], z_buf[lane]};
      }
      kept += keep_count;
      i += vl;
    }

    candidates.resize(kept);
    return true;
  }
}

#endif // __RVV10__

// Scalar source-candidate stage used by std builds and fallback checks. It keeps
// the same staging contract as projectCandidatesRVV() so later stages can be
// tested independently of RVV availability.
template <typename PointSource, typename PointTarget>
inline bool
projectCandidatesStd(const pcl::PointCloud<PointSource>& input,
                     const pcl::PointCloud<PointTarget>&,
                     const pcl::Indices& indices,
                     const ProjectionParams& params,
                     std::vector<ProjectionCandidate>& candidates)
{
  if constexpr (!kXYZFloatLayoutV<PointSource> || !kXYZFloatLayoutV<PointTarget>)
    return false;

  if (indices.size() < 64)
    return false;

  candidates.resize(indices.size());
  std::size_t kept = 0;
  for (const auto& src_idx : indices) {
    const auto& point = input[src_idx];
    if (!isFiniteXYZ(point))
      continue;

    const Eigen::Vector4f p_src(params.transform * point.getVector4fMap());
    const float x = p_src[0];
    const float y = p_src[1];
    const float z = p_src[2];
    if (z <= 0.0f)
      continue;

    candidates[kept++] = ProjectionCandidate{static_cast<std::uint32_t>(src_idx), x, y, z};
  }
  candidates.resize(kept);
  return true;
}

// Dispatch wrapper for tests that compare "candidate stage available" with the
// full scalar reference. RVV builds try the intrinsic stage; std builds use the
// scalar staging analogue.
template <typename PointSource, typename PointTarget>
inline bool
projectCandidatesCandidate(const pcl::PointCloud<PointSource>& input,
                           const pcl::PointCloud<PointTarget>& target,
                           const pcl::Indices& indices,
                           const ProjectionParams& params,
                           std::vector<ProjectionCandidate>& candidates)
{
#if defined(__RVV10__)
  return projectCandidatesRVV(input, target, indices, params, candidates);
#else
  return projectCandidatesStd(input, target, indices, params, candidates);
#endif
}

// Scalar projection-pixel stage. It consumes source candidates, computes u/v and
// target_index, and intentionally stops before target access so projection
// boundary behavior can be isolated.
template <typename PointTarget>
inline bool
projectPixelsStd(const pcl::PointCloud<PointTarget>& target,
                 const ProjectionParams& params,
                 const std::vector<ProjectionCandidate>& candidates,
                 std::vector<ProjectedCandidate>& projected)
{
  if (candidates.size() < 64)
    return false;

  projected.resize(candidates.size());
  std::size_t kept = 0;
  for (const auto& candidate : candidates) {
    const float uv0 = params.fx * candidate.x + params.cx * candidate.z;
    const float uv1 = params.fy * candidate.y + params.cy * candidate.z;
    if (candidate.z <= 0.0f)
      continue;
    const int u = static_cast<int>(uv0 / candidate.z);
    const int v = static_cast<int>(uv1 / candidate.z);
    if (u < 0 || u >= static_cast<int>(target.width) || v < 0 ||
        v >= static_cast<int>(target.height))
      continue;

    projected[kept++] = ProjectedCandidate{
        candidate.source_index,
        static_cast<std::uint32_t>(v * target.width + u),
        candidate.x,
        candidate.y,
        candidate.z};
  }

  projected.resize(kept);
  return true;
}

#if defined(__RVV10__)

// RVV projection-pixel stage. This is the diagnostic counterpart to production
// projectOrganizedProjectionPixelsRVV(): it validates the vfmacc/vfdiv/RTZ
// conversion and in-bounds mask before target predicates are considered.
template <typename PointTarget>
inline bool
projectPixelsRVV(const pcl::PointCloud<PointTarget>& target,
                 const ProjectionParams& params,
                 const std::vector<ProjectionCandidate>& candidates,
                 std::vector<ProjectedCandidate>& projected)
{
  const std::size_t n = candidates.size();
  if (n < 64 || n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
      target.width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
      target.height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    return false;

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  if (vlmax > 64)
    return false;

  projected.resize(n);
  std::size_t kept = 0;
  std::size_t i = 0;

  alignas(16) std::uint32_t source_buf[64];
  alignas(16) std::uint32_t target_buf[64];
  alignas(16) float x_buf[64];
  alignas(16) float y_buf[64];
  alignas(16) float z_buf[64];

  while (i < n) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    const auto* base = candidates.data() + i;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);

    const vuint32m2_t offsets = __riscv_vmul_vx_u32m2(
        __riscv_vid_v_u32m2(vl), static_cast<std::uint32_t>(sizeof(ProjectionCandidate)), vl);
    const vuint32m2_t source =
        __riscv_vluxei32_v_u32m2(reinterpret_cast<const std::uint32_t*>(bytes),
                                 offsets,
                                 vl);
    const vfloat32m2_t x =
        __riscv_vluxei32_v_f32m2(reinterpret_cast<const float*>(bytes + offsetof(ProjectionCandidate, x)),
                                 offsets,
                                 vl);
    const vfloat32m2_t y =
        __riscv_vluxei32_v_f32m2(reinterpret_cast<const float*>(bytes + offsetof(ProjectionCandidate, y)),
                                 offsets,
                                 vl);
    const vfloat32m2_t z =
        __riscv_vluxei32_v_f32m2(reinterpret_cast<const float*>(bytes + offsetof(ProjectionCandidate, z)),
                                 offsets,
                                 vl);

    // This diagnostic step moves only the C++ static_cast<int> projection and
    // image bounds check into RVV. Target gather, depth/distance, and append
    // remain scalar so pixel staging can be verified in isolation.
    // Match the scalar compiler's usual contraction of a*b + c*d more closely:
    // one product is rounded into the accumulator, and the other is fused.
    vfloat32m2_t uv0 = __riscv_vfmul_vf_f32m2(z, params.cx, vl);
    uv0 = __riscv_vfmacc_vf_f32m2(uv0, params.fx, x, vl);
    vfloat32m2_t uv1 = __riscv_vfmul_vf_f32m2(z, params.cy, vl);
    uv1 = __riscv_vfmacc_vf_f32m2(uv1, params.fy, y, vl);

    const vint32m2_t u =
        __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv0, z, vl), vl);
    const vint32m2_t v =
        __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv1, z, vl), vl);

    vbool16_t keep = __riscv_vmsge_vx_i32m2_b16(u, 0, vl);
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmslt_vx_i32m2_b16(u, static_cast<int>(target.width), vl), vl);
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmsge_vx_i32m2_b16(v, 0, vl), vl);
    keep = __riscv_vmand_mm_b16(keep, __riscv_vmslt_vx_i32m2_b16(v, static_cast<int>(target.height), vl), vl);

    const vint32m2_t target_index_i = __riscv_vadd_vv_i32m2(
        __riscv_vmul_vx_i32m2(v, static_cast<int>(target.width), vl), u, vl);
    const vuint32m2_t target_index =
        __riscv_vreinterpret_v_i32m2_u32m2(target_index_i);

    const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(source, keep, vl);
    const vuint32m2_t target_kept = __riscv_vcompress_vm_u32m2(target_index, keep, vl);
    const vfloat32m2_t x_kept = __riscv_vcompress_vm_f32m2(x, keep, vl);
    const vfloat32m2_t y_kept = __riscv_vcompress_vm_f32m2(y, keep, vl);
    const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(z, keep, vl);
    const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

    __riscv_vse32_v_u32m2(source_buf, source_kept, keep_count);
    __riscv_vse32_v_u32m2(target_buf, target_kept, keep_count);
    __riscv_vse32_v_f32m2(x_buf, x_kept, keep_count);
    __riscv_vse32_v_f32m2(y_buf, y_kept, keep_count);
    __riscv_vse32_v_f32m2(z_buf, z_kept, keep_count);

    for (std::size_t lane = 0; lane < keep_count; ++lane) {
      projected[kept + lane] = ProjectedCandidate{
          source_buf[lane], target_buf[lane], x_buf[lane], y_buf[lane], z_buf[lane]};
    }
    kept += keep_count;
    i += vl;
  }

  projected.resize(kept);
  return true;
}

template <typename PointTarget>
inline bool
acceptProjectedCandidatesRVV(const pcl::PointCloud<PointTarget>& target,
                             const ProjectionParams& params,
                             const std::vector<ProjectedCandidate>& projected,
                             std::vector<AcceptedCandidate>& accepted)
{
  if constexpr (!kXYZFloatLayoutV<PointTarget>) {
    return false;
  } else {
    constexpr std::size_t kXOff = pcl::traits::offset<PointTarget, pcl::fields::x>::value;
    constexpr std::size_t kYOff = pcl::traits::offset<PointTarget, pcl::fields::y>::value;
    constexpr std::size_t kZOff = pcl::traits::offset<PointTarget, pcl::fields::z>::value;

    const std::size_t n = projected.size();
    if (n < 64 ||
        n > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
        target.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
      return false;

    const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
    if (vlmax > 64)
      return false;

    accepted.resize(n);
    std::size_t kept = 0;
    std::size_t i = 0;
    const auto* target_base = reinterpret_cast<const std::uint8_t*>(target.points.data());

    alignas(16) std::uint32_t source_buf[64];
    alignas(16) std::uint32_t target_buf[64];
    alignas(16) float dist_buf[64];
    alignas(16) float sx_buf[64];
    alignas(16) float sy_buf[64];
    alignas(16) float sz_buf[64];
    alignas(16) float tx_buf[64];
    alignas(16) float ty_buf[64];
    alignas(16) float tz_buf[64];

    while (i < n) {
      const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
      const auto* base = projected.data() + i;
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(base);
      const vuint32m2_t projected_offsets = __riscv_vmul_vx_u32m2(
          __riscv_vid_v_u32m2(vl), static_cast<std::uint32_t>(sizeof(ProjectedCandidate)), vl);

      const vuint32m2_t source =
          __riscv_vluxei32_v_u32m2(reinterpret_cast<const std::uint32_t*>(bytes),
                                   projected_offsets,
                                   vl);
      const vuint32m2_t target_index = __riscv_vluxei32_v_u32m2(
          reinterpret_cast<const std::uint32_t*>(bytes + offsetof(ProjectedCandidate, target_index)),
          projected_offsets,
          vl);
      const vfloat32m2_t sx =
          __riscv_vluxei32_v_f32m2(reinterpret_cast<const float*>(bytes + offsetof(ProjectedCandidate, x)),
                                   projected_offsets,
                                   vl);
      const vfloat32m2_t sy =
          __riscv_vluxei32_v_f32m2(reinterpret_cast<const float*>(bytes + offsetof(ProjectedCandidate, y)),
                                   projected_offsets,
                                   vl);
      const vfloat32m2_t sz =
          __riscv_vluxei32_v_f32m2(reinterpret_cast<const float*>(bytes + offsetof(ProjectedCandidate, z)),
                                   projected_offsets,
                                   vl);

      const vuint32m2_t target_offsets = __riscv_vmul_vx_u32m2(
          target_index, static_cast<std::uint32_t>(sizeof(PointTarget)), vl);

      vfloat32m2_t tx;
      vfloat32m2_t ty;
      vfloat32m2_t tz;
      pcl::rvv_load::indexed_load3_fields_f32m2<
          typename pcl::traits::POD<PointTarget>::type, kXOff, kYOff, kZOff>(
          target_base, target_offsets, vl, tx, ty, tz);

      // This diagnostic stage begins after projected pixel staging. It gathers
      // target xyz, applies target finite/depth/distance predicates, and keeps
      // append order through vcompress; correspondence append itself stays scalar.
      vbool16_t keep = __riscv_vmand_mm_b16(
          __riscv_vmand_mm_b16(finiteMask(tx, vl), finiteMask(ty, vl), vl),
          finiteMask(tz, vl),
          vl);
      const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(sz, tz, vl);
      keep = __riscv_vmand_mm_b16(
          keep,
          __riscv_vmfle_vf_f32m2_b16(__riscv_vfabs_v_f32m2(dz, vl),
                                      params.depth_threshold,
                                      vl),
          vl);

      const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(sx, tx, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(sy, ty, vl);
      vfloat32m2_t dist2 = __riscv_vfmul_vv_f32m2(dx, dx, vl);
      dist2 = __riscv_vfmacc_vv_f32m2(dist2, dy, dy, vl);
      dist2 = __riscv_vfmacc_vv_f32m2(dist2, dz, dz, vl);
      const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
      keep = __riscv_vmand_mm_b16(
          keep, __riscv_vmflt_vf_f32m2_b16(dist, static_cast<float>(params.max_distance), vl), vl);

      const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(source, keep, vl);
      const vuint32m2_t target_kept = __riscv_vcompress_vm_u32m2(target_index, keep, vl);
      const vfloat32m2_t sx_kept = __riscv_vcompress_vm_f32m2(sx, keep, vl);
      const vfloat32m2_t sy_kept = __riscv_vcompress_vm_f32m2(sy, keep, vl);
      const vfloat32m2_t sz_kept = __riscv_vcompress_vm_f32m2(sz, keep, vl);
      const vfloat32m2_t tx_kept = __riscv_vcompress_vm_f32m2(tx, keep, vl);
      const vfloat32m2_t ty_kept = __riscv_vcompress_vm_f32m2(ty, keep, vl);
      const vfloat32m2_t tz_kept = __riscv_vcompress_vm_f32m2(tz, keep, vl);
      const std::size_t keep_count = __riscv_vcpop_m_b16(keep, vl);

      __riscv_vse32_v_u32m2(source_buf, source_kept, keep_count);
      __riscv_vse32_v_u32m2(target_buf, target_kept, keep_count);
      __riscv_vse32_v_f32m2(sx_buf, sx_kept, keep_count);
      __riscv_vse32_v_f32m2(sy_buf, sy_kept, keep_count);
      __riscv_vse32_v_f32m2(sz_buf, sz_kept, keep_count);
      __riscv_vse32_v_f32m2(tx_buf, tx_kept, keep_count);
      __riscv_vse32_v_f32m2(ty_buf, ty_kept, keep_count);
      __riscv_vse32_v_f32m2(tz_buf, tz_kept, keep_count);

      for (std::size_t lane = 0; lane < keep_count; ++lane) {
        const float sdx = sx_buf[lane] - tx_buf[lane];
        const float sdy = sy_buf[lane] - ty_buf[lane];
        const float sdz = sz_buf[lane] - tz_buf[lane];
        const double scalar_dist =
            std::sqrt(static_cast<double>(sdx) * sdx +
                      static_cast<double>(sdy) * sdy +
                      static_cast<double>(sdz) * sdz);
        dist_buf[lane] = static_cast<float>(scalar_dist);
        accepted[kept + lane] =
            AcceptedCandidate{source_buf[lane], target_buf[lane], dist_buf[lane]};
      }

      kept += keep_count;
      i += vl;
    }

    accepted.resize(kept);
    return true;
  }
}

#endif // __RVV10__

// Dispatch wrapper for the projection-pixel stage. Tests use this to exercise
// the same stage boundary in RVV and non-RVV builds.
template <typename PointTarget>
inline bool
projectPixelsCandidate(const pcl::PointCloud<PointTarget>& target,
                       const ProjectionParams& params,
                       const std::vector<ProjectionCandidate>& candidates,
                       std::vector<ProjectedCandidate>& projected)
{
#if defined(__RVV10__)
  return projectPixelsRVV(target, params, candidates, projected);
#else
  return projectPixelsStd(target, params, candidates, projected);
#endif
}

// Scalar accepted-candidate stage. It starts after target_index is known and
// keeps target finite, depth threshold, distance predicate, and accepted order
// aligned with the RVV target-predicate stage.
template <typename PointTarget>
inline bool
acceptProjectedCandidatesStd(const pcl::PointCloud<PointTarget>& target,
                             const ProjectionParams& params,
                             const std::vector<ProjectedCandidate>& projected,
                             std::vector<AcceptedCandidate>& accepted)
{
  if constexpr (!kXYZFloatLayoutV<PointTarget>)
    return false;

  if (projected.size() < 64)
    return false;

  accepted.resize(projected.size());
  std::size_t kept = 0;
  for (const auto& candidate : projected) {
    const PointTarget& pt_tgt = target.points[candidate.target_index];
    if (!isFiniteXYZ(pt_tgt))
      continue;
    if (std::abs(candidate.z - pt_tgt.z) > params.depth_threshold)
      continue;

    const float dx = candidate.x - pt_tgt.x;
    const float dy = candidate.y - pt_tgt.y;
    const float dz = candidate.z - pt_tgt.z;
    const double dist = std::sqrt(static_cast<double>(dx) * dx +
                                  static_cast<double>(dy) * dy +
                                  static_cast<double>(dz) * dz);
    if (dist < static_cast<float>(params.max_distance))
      accepted[kept++] =
          AcceptedCandidate{candidate.source_index,
                            candidate.target_index,
                            static_cast<float>(dist)};
  }

  accepted.resize(kept);
  return true;
}

// Dispatch wrapper for the target-predicate stage.
template <typename PointTarget>
inline bool
acceptProjectedCandidatesCandidate(const pcl::PointCloud<PointTarget>& target,
                                   const ProjectionParams& params,
                                   const std::vector<ProjectedCandidate>& projected,
                                   std::vector<AcceptedCandidate>& accepted)
{
#if defined(__RVV10__)
  return acceptProjectedCandidatesRVV(target, params, projected, accepted);
#else
  return acceptProjectedCandidatesStd(target, params, projected, accepted);
#endif
}

// Fallback tail after only source-candidate staging succeeded. It must preserve
// scalar projection, target access, predicates, and append order from the
// original CEOP loop.
template <typename PointTarget>
inline void
finishCorrespondencesFromCandidates(const pcl::PointCloud<PointTarget>& target,
                                    const ProjectionParams& params,
                                    const std::vector<ProjectionCandidate>& candidates,
                                    pcl::Correspondences& correspondences)
{
  correspondences.resize(candidates.size());
  std::size_t c_index = 0;

  for (const auto& candidate : candidates) {
    const float uv0 = params.fx * candidate.x + params.cx * candidate.z;
    const float uv1 = params.fy * candidate.y + params.cy * candidate.z;
    if (candidate.z <= 0.0f)
      continue;
    const int u = static_cast<int>(uv0 / candidate.z);
    const int v = static_cast<int>(uv1 / candidate.z);
    if (u < 0 || u >= static_cast<int>(target.width) || v < 0 ||
        v >= static_cast<int>(target.height))
      continue;

    const PointTarget& pt_tgt = target.at(u, v);
    if (!isFiniteXYZ(pt_tgt))
      continue;
    if (std::abs(candidate.z - pt_tgt.z) > params.depth_threshold)
      continue;

    const float dx = candidate.x - pt_tgt.x;
    const float dy = candidate.y - pt_tgt.y;
    const float dz = candidate.z - pt_tgt.z;
    const double dist = std::sqrt(static_cast<double>(dx) * dx +
                                  static_cast<double>(dy) * dy +
                                  static_cast<double>(dz) * dz);
    if (dist < params.max_distance)
      correspondences[c_index++] = pcl::Correspondence(
          static_cast<int>(candidate.source_index), v * target.width + u, static_cast<float>(dist));
  }

  correspondences.resize(c_index);
}

// Fallback tail after projection-pixel staging succeeded. u/v and bounds are
// already resolved to target_index; this tail only performs target predicates and
// scalar append.
template <typename PointTarget>
inline void
finishCorrespondencesFromProjected(const pcl::PointCloud<PointTarget>& target,
                                   const ProjectionParams& params,
                                   const std::vector<ProjectedCandidate>& projected,
                                   pcl::Correspondences& correspondences)
{
  correspondences.resize(projected.size());
  std::size_t c_index = 0;

  for (const auto& candidate : projected) {
    const PointTarget& pt_tgt = target.points[candidate.target_index];
    if (!isFiniteXYZ(pt_tgt))
      continue;
    if (std::abs(candidate.z - pt_tgt.z) > params.depth_threshold)
      continue;

    const float dx = candidate.x - pt_tgt.x;
    const float dy = candidate.y - pt_tgt.y;
    const float dz = candidate.z - pt_tgt.z;
    const double dist = std::sqrt(static_cast<double>(dx) * dx +
                                  static_cast<double>(dy) * dy +
                                  static_cast<double>(dz) * dz);
    if (dist < params.max_distance)
      correspondences[c_index++] = pcl::Correspondence(
          static_cast<int>(candidate.source_index),
          static_cast<int>(candidate.target_index),
          static_cast<float>(dist));
  }

  correspondences.resize(c_index);
}

// Final production-shaped append tail after accepted staging. Predicate work is
// already complete; this function only materializes pcl::Correspondence in
// accepted order.
inline void
finishCorrespondencesFromAccepted(const std::vector<AcceptedCandidate>& accepted,
                                  pcl::Correspondences& correspondences)
{
  correspondences.resize(accepted.size());
  for (std::size_t i = 0; i < accepted.size(); ++i) {
    correspondences[i] = pcl::Correspondence(static_cast<int>(accepted[i].source_index),
                                             static_cast<int>(accepted[i].target_index),
                                             accepted[i].distance);
  }
}

template <typename PointSource,
          typename PointTarget,
          typename Scalar = float,
          DiagMode Mode = DiagMode::Candidate>
class CorrespondenceEstimationOrganizedProjectionDiagnostic
: public pcl::registration::
      CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar> {
public:
  using Base = pcl::registration::
      CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar>;
  using PointCloudSource = pcl::PointCloud<PointSource>;
  using PointCloudTarget = pcl::PointCloud<PointTarget>;

  void determineCorrespondences(pcl::Correspondences& correspondences,
                                const double max_distance) override
  {
    // This is the production-shaped diagnostic entry. It deliberately enters
    // through the upstream CEOP initCompute(), so target organization checks,
    // CorrespondenceEstimationBase state, and PCLBase fake indices are the same
    // front door used by the production function.
    if (!Base::initCompute())
      return;

    const ProjectionParams call_params = paramsWithMaxDistance(max_distance);
    if constexpr (Mode == DiagMode::Candidate) {
      std::vector<ProjectionCandidate> candidates;
      if (projectCandidatesCandidate(*this->input_,
                                     *this->target_,
                                     *this->indices_,
                                     call_params,
                                     candidates)) {
        finishCorrespondencesFromCandidates(
            *this->target_, call_params, candidates, correspondences);
        return;
      }
    } else if constexpr (Mode == DiagMode::ProjectedIdentity) {
      if (transformIsExactIdentity(call_params.transform)) {
        std::vector<ProjectionCandidate> candidates;
        std::vector<ProjectedCandidate> projected;
        if (projectCandidatesCandidate(*this->input_,
                                       *this->target_,
                                       *this->indices_,
                                       call_params,
                                       candidates) &&
            projectPixelsCandidate(*this->target_, call_params, candidates, projected)) {
          finishCorrespondencesFromProjected(
              *this->target_, call_params, projected, correspondences);
          return;
        }
      }
    } else if constexpr (Mode == DiagMode::Projected) {
      std::vector<ProjectionCandidate> candidates;
      std::vector<ProjectedCandidate> projected;
      if (projectCandidatesCandidate(*this->input_,
                                     *this->target_,
                                     *this->indices_,
                                     call_params,
                                     candidates) &&
          projectPixelsCandidate(*this->target_, call_params, candidates, projected)) {
        finishCorrespondencesFromProjected(
            *this->target_, call_params, projected, correspondences);
        return;
      }
    } else if constexpr (Mode == DiagMode::Accepted) {
      std::vector<ProjectionCandidate> candidates;
      std::vector<ProjectedCandidate> projected;
      std::vector<AcceptedCandidate> accepted;
      if (projectCandidatesCandidate(*this->input_,
                                     *this->target_,
                                     *this->indices_,
                                     call_params,
                                     candidates) &&
          projectPixelsCandidate(*this->target_, call_params, candidates, projected) &&
          acceptProjectedCandidatesCandidate(
              *this->target_, call_params, projected, accepted)) {
        finishCorrespondencesFromAccepted(accepted, correspondences);
        return;
      }
    }

    correspondence_estimation_organized_projection_diag::determineCorrespondencesStd(
        *this->input_, *this->target_, *this->indices_, call_params, correspondences);
  }

  void determineReciprocalCorrespondences(pcl::Correspondences& correspondences,
                                          const double max_distance) override
  {
    determineCorrespondences(correspondences, max_distance);
  }

private:
  ProjectionParams paramsWithMaxDistance(double max_distance) const
  {
    ProjectionParams params;
    params.transform = this->src_to_tgt_transformation_;
    params.fx = this->fx_;
    params.fy = this->fy_;
    params.cx = this->cx_;
    params.cy = this->cy_;
    params.depth_threshold = this->depth_threshold_;
    params.max_distance = max_distance;
    return params;
  }
};

template <typename PointSource, typename PointTarget>
inline void
determineCorrespondencesCandidate(const pcl::PointCloud<PointSource>& input,
                                  const pcl::PointCloud<PointTarget>& target,
                                  const pcl::Indices& indices,
                                  const ProjectionParams& params,
                                  pcl::Correspondences& correspondences)
{
  std::vector<ProjectionCandidate> candidates;
  if (projectCandidatesCandidate(input, target, indices, params, candidates)) {
    finishCorrespondencesFromCandidates(target, params, candidates, correspondences);
    return;
  }

  determineCorrespondencesStd(input, target, indices, params, correspondences);
}

template <typename PointSource, typename PointTarget>
inline void
determineCorrespondencesProjectedCandidate(const pcl::PointCloud<PointSource>& input,
                                           const pcl::PointCloud<PointTarget>& target,
                                           const pcl::Indices& indices,
                                           const ProjectionParams& params,
                                           pcl::Correspondences& correspondences)
{
  std::vector<ProjectionCandidate> candidates;
  std::vector<ProjectedCandidate> projected;
  if (projectCandidatesCandidate(input, target, indices, params, candidates) &&
      projectPixelsCandidate(target, params, candidates, projected)) {
    finishCorrespondencesFromProjected(target, params, projected, correspondences);
    return;
  }

  determineCorrespondencesStd(input, target, indices, params, correspondences);
}

template <typename PointSource, typename PointTarget>
inline void
determineCorrespondencesProjectedIdentityCandidate(
    const pcl::PointCloud<PointSource>& input,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& indices,
    const ProjectionParams& params,
    pcl::Correspondences& correspondences)
{
  if (transformIsExactIdentity(params.transform)) {
    std::vector<ProjectionCandidate> candidates;
    std::vector<ProjectedCandidate> projected;
    if (projectCandidatesCandidate(input, target, indices, params, candidates) &&
        projectPixelsCandidate(target, params, candidates, projected)) {
      finishCorrespondencesFromProjected(target, params, projected, correspondences);
      return;
    }
  }

  determineCorrespondencesStd(input, target, indices, params, correspondences);
}

template <typename PointSource, typename PointTarget>
inline void
determineCorrespondencesAcceptedCandidate(const pcl::PointCloud<PointSource>& input,
                                          const pcl::PointCloud<PointTarget>& target,
                                          const pcl::Indices& indices,
                                          const ProjectionParams& params,
                                          pcl::Correspondences& correspondences)
{
  std::vector<ProjectionCandidate> candidates;
  std::vector<ProjectedCandidate> projected;
  std::vector<AcceptedCandidate> accepted;
  if (projectCandidatesCandidate(input, target, indices, params, candidates) &&
      projectPixelsCandidate(target, params, candidates, projected) &&
      acceptProjectedCandidatesCandidate(target, params, projected, accepted)) {
    finishCorrespondencesFromAccepted(accepted, correspondences);
    return;
  }

  determineCorrespondencesStd(input, target, indices, params, correspondences);
}

inline void
determineCorrespondencesPclClass(const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& input,
                                 const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                                 const ProjectionParams& params,
                                 pcl::Correspondences& correspondences)
{
  pcl::registration::CorrespondenceEstimationOrganizedProjection<pcl::PointXYZ, pcl::PointXYZ> ce;
  ce.setInputSource(input);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  ce.determineCorrespondences(correspondences, params.max_distance);
}

} // namespace pcl::registration::correspondence_estimation_organized_projection_diag
