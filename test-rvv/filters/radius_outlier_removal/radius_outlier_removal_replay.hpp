#pragma once

#include <pcl/types.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl_rvv_filters_radius_outlier_removal {

struct ApplyFilterIndicesTailResult {
  pcl::Indices kept;
  pcl::Indices removed;
};

// Replay source:
//   filters/include/pcl/filters/impl/radius_outlier_removal.hpp
//   pcl::RadiusOutlierRemoval<PointT>::applyFilterIndices(Indices&)
//
// Fragment boundary:
//   original lines 157-172, after nearestKSearch/radiusSearch has filled
//   `to_keep` and before output vectors are resized.
//
// Member-to-context mapping:
//   (*indices_)                 -> source_indices
//   extract_removed_indices_    -> extract_removed_indices
//   indices                     -> returned `kept`
//   (*removed_indices_)         -> returned `removed`
//   to_keep                     -> to_keep
struct ApplyFilterIndicesTailReplayContext {
  const pcl::Indices* source_indices{nullptr};
  const std::vector<std::uint8_t>* to_keep{nullptr};
  bool extract_removed_indices{false};
};

inline bool
isValidApplyFilterIndicesTailReplayContext(const ApplyFilterIndicesTailReplayContext& ctx)
{
  return ctx.source_indices != nullptr && ctx.to_keep != nullptr &&
         ctx.source_indices->size() == ctx.to_keep->size();
}

// Scalar replay of radius_outlier_removal.hpp:157-172.
inline ApplyFilterIndicesTailResult
applyFilterIndicesTailStdReplay(const ApplyFilterIndicesTailReplayContext& ctx)
{
  ApplyFilterIndicesTailResult result;
  if (!isValidApplyFilterIndicesTailReplayContext(ctx))
    return result;

  const auto& source_indices = *ctx.source_indices;
  const auto& to_keep = *ctx.to_keep;
  result.kept.resize(to_keep.size());
  result.removed.resize(ctx.extract_removed_indices ? to_keep.size() : 0);

  int oii = 0;
  int rii = 0;

  for (pcl::index_t i = 0; i < static_cast<pcl::index_t>(to_keep.size()); i++)
  {
    if (to_keep[static_cast<std::size_t>(i)] == 0)
    {
      if (ctx.extract_removed_indices)
        result.removed[rii++] = source_indices[static_cast<std::size_t>(i)];
      continue;
    }

    result.kept[oii++] = source_indices[static_cast<std::size_t>(i)];
  }

  result.kept.resize(static_cast<std::size_t>(oii));
  result.removed.resize(static_cast<std::size_t>(rii));
  return result;
}

// RVV replacement of the same replayed tail loop.
inline bool
applyFilterIndicesTailRVVReplay(const ApplyFilterIndicesTailReplayContext& ctx,
                                ApplyFilterIndicesTailResult& result)
{
  if (!isValidApplyFilterIndicesTailReplayContext(ctx) || ctx.to_keep->size() < 64)
    return false;

#if defined(__RVV10__) && defined(PCL_RADIUS_OUTLIER_REMOVAL_RVV_DIAGNOSTIC)
  const auto& source_indices = *ctx.source_indices;
  const auto& to_keep = *ctx.to_keep;
  result.kept.resize(to_keep.size());
  result.removed.resize(ctx.extract_removed_indices ? to_keep.size() : 0);

  std::size_t kept_count = 0;
  std::size_t removed_count = 0;
  const auto* keep_data = to_keep.data();
  const auto* source_data = source_indices.data();

  for (std::size_t i = 0; i < to_keep.size();) {
    const std::size_t vl = __riscv_vsetvl_e8m1(to_keep.size() - i);
    const vuint8m1_t v_keep = __riscv_vle8_v_u8m1(keep_data + i, vl);
    const vbool8_t m_keep = __riscv_vmsne_vx_u8m1_b8(v_keep, 0, vl);
    const vuint32m4_t v_src = __riscv_vle32_v_u32m4(
        reinterpret_cast<const std::uint32_t*>(source_data + i), vl);
    const vuint32m4_t v_kept = __riscv_vcompress_vm_u32m4(v_src, m_keep, vl);
    const std::size_t n_kept = __riscv_vcpop_m_b8(m_keep, vl);
    __riscv_vse32_v_u32m4(
        reinterpret_cast<std::uint32_t*>(result.kept.data() + kept_count), v_kept, n_kept);
    kept_count += n_kept;

    if (ctx.extract_removed_indices) {
      const vbool8_t m_removed = __riscv_vmnot_m_b8(m_keep, vl);
      const vuint32m4_t v_removed = __riscv_vcompress_vm_u32m4(v_src, m_removed, vl);
      const std::size_t n_removed = vl - n_kept;
      __riscv_vse32_v_u32m4(
          reinterpret_cast<std::uint32_t*>(result.removed.data() + removed_count),
          v_removed,
          n_removed);
      removed_count += n_removed;
    }

    i += vl;
  }

  result.kept.resize(kept_count);
  result.removed.resize(removed_count);
  return true;
#else
  (void)result;
  return false;
#endif
}

inline ApplyFilterIndicesTailResult
applyFilterIndicesTailAutoReplay(const ApplyFilterIndicesTailReplayContext& ctx, bool use_rvv)
{
  ApplyFilterIndicesTailResult result;
  if (use_rvv && applyFilterIndicesTailRVVReplay(ctx, result))
    return result;
  return applyFilterIndicesTailStdReplay(ctx);
}

} // namespace pcl_rvv_filters_radius_outlier_removal
