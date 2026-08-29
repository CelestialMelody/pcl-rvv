#pragma once

/*
 * 本文件做什么：
 * 这里放 geometric_consistency topic 的 diagnostic helper。我们先把
 * `clusterCorrespondences()` 里最清楚的局部谓词拆出来：给定一个固定 consensus set，
 * 判断候选 correspondence 是否仍然满足 pairwise distance consistency。
 *
 * 证据边界：
 * 这些 helper 是 diagnostic / component ablation，不是 production direct。
 * 它们不覆盖 `std::sort`、RANSAC rejector、输出 transformation 顺序，也不覆盖
 * production `clusterCorrespondences()` 的完整 growth 语义。
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::test::geometric_consistency_rvv
{

enum class ExecutionPath
{
  ScalarFallback,
  RvvPairwisePredicate
};

struct PackedCorrespondenceCloud
{
  std::vector<float> model_x;
  std::vector<float> model_y;
  std::vector<float> model_z;
  std::vector<float> scene_x;
  std::vector<float> scene_y;
  std::vector<float> scene_z;

  [[nodiscard]] std::size_t
  size() const
  {
    return model_x.size();
  }
};

inline PackedCorrespondenceCloud
makeSyntheticPackedCloud(const std::size_t count,
                         const float model_shift_x = 0.0f,
                         const float model_shift_y = 0.0f,
                         const float model_shift_z = 0.0f,
                         const float scene_shift_x = 0.25f,
                         const float scene_shift_y = -0.15f,
                         const float scene_shift_z = 0.40f)
{
  PackedCorrespondenceCloud cloud;
  cloud.model_x.reserve(count);
  cloud.model_y.reserve(count);
  cloud.model_z.reserve(count);
  cloud.scene_x.reserve(count);
  cloud.scene_y.reserve(count);
  cloud.scene_z.reserve(count);

  for (std::size_t i = 0; i < count; ++i)
  {
    const float base = static_cast<float>(i) * 0.013f;
    const float mx = model_shift_x + base;
    const float my = model_shift_y + base * 1.7f;
    const float mz = 1.0f + model_shift_z + base * 0.3f;

    cloud.model_x.push_back(mx);
    cloud.model_y.push_back(my);
    cloud.model_z.push_back(mz);
    cloud.scene_x.push_back(mx + scene_shift_x);
    cloud.scene_y.push_back(my + scene_shift_y);
    cloud.scene_z.push_back(mz + scene_shift_z);
  }

  return cloud;
}

inline void
injectSceneNoise(PackedCorrespondenceCloud& cloud,
                 const std::size_t index,
                 const float dx,
                 const float dy,
                 const float dz)
{
  if (index >= cloud.size())
    return;
  cloud.scene_x[index] += dx;
  cloud.scene_y[index] += dy;
  cloud.scene_z[index] += dz;
}

inline bool
pairwiseConsistencyScalarReference(const PackedCorrespondenceCloud& cloud,
                                   const std::vector<int>& consensus_indices,
                                   const std::size_t candidate_index,
                                   const float gc_size)
{
  if (candidate_index >= cloud.size())
    return false;

  for (const int consensus_index : consensus_indices)
  {
    if (consensus_index < 0 || static_cast<std::size_t>(consensus_index) >= cloud.size())
      return false;

    const std::size_t k = static_cast<std::size_t>(consensus_index);
    const float scene_dx = cloud.scene_x[k] - cloud.scene_x[candidate_index];
    const float scene_dy = cloud.scene_y[k] - cloud.scene_y[candidate_index];
    const float scene_dz = cloud.scene_z[k] - cloud.scene_z[candidate_index];
    const float model_dx = cloud.model_x[k] - cloud.model_x[candidate_index];
    const float model_dy = cloud.model_y[k] - cloud.model_y[candidate_index];
    const float model_dz = cloud.model_z[k] - cloud.model_z[candidate_index];

    const float scene_norm =
        std::sqrt(scene_dx * scene_dx + scene_dy * scene_dy + scene_dz * scene_dz);
    const float model_norm =
        std::sqrt(model_dx * model_dx + model_dy * model_dy + model_dz * model_dz);
    if (std::fabs(scene_norm - model_norm) > gc_size)
      return false;
  }

  return true;
}

inline ExecutionPath
pairwiseConsistencyCandidate(const PackedCorrespondenceCloud& cloud,
                             const std::vector<int>& consensus_indices,
                             const std::size_t candidate_index,
                             const float gc_size,
                             bool& is_consistent,
                             std::size_t* vector_chunks = nullptr)
{
  is_consistent = false;

  if (candidate_index >= cloud.size())
    return ExecutionPath::ScalarFallback;

#if defined(__RVV10__)
  if (consensus_indices.empty())
  {
    is_consistent = true;
    return ExecutionPath::RvvPairwisePredicate;
  }

  const float cand_model_x = cloud.model_x[candidate_index];
  const float cand_model_y = cloud.model_y[candidate_index];
  const float cand_model_z = cloud.model_z[candidate_index];
  const float cand_scene_x = cloud.scene_x[candidate_index];
  const float cand_scene_y = cloud.scene_y[candidate_index];
  const float cand_scene_z = cloud.scene_z[candidate_index];
  const float* model_x_base = cloud.model_x.data();
  const float* model_y_base = cloud.model_y.data();
  const float* model_z_base = cloud.model_z.data();
  const float* scene_x_base = cloud.scene_x.data();
  const float* scene_y_base = cloud.scene_y.data();
  const float* scene_z_base = cloud.scene_z.data();

  if (vector_chunks != nullptr)
    *vector_chunks = 0;

  for (std::size_t offset = 0; offset < consensus_indices.size();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(consensus_indices.size() - offset);
    const auto k_indices = __riscv_vle32_v_u32m2(
        reinterpret_cast<const std::uint32_t*>(consensus_indices.data() + offset), vl);
    const auto k_byte_offsets = __riscv_vmul_vx_u32m2(k_indices, sizeof(float), vl);
    const vfloat32m2_t model_x_k = __riscv_vluxei32_v_f32m2(model_x_base, k_byte_offsets, vl);
    const vfloat32m2_t model_y_k = __riscv_vluxei32_v_f32m2(model_y_base, k_byte_offsets, vl);
    const vfloat32m2_t model_z_k = __riscv_vluxei32_v_f32m2(model_z_base, k_byte_offsets, vl);
    const vfloat32m2_t scene_x_k = __riscv_vluxei32_v_f32m2(scene_x_base, k_byte_offsets, vl);
    const vfloat32m2_t scene_y_k = __riscv_vluxei32_v_f32m2(scene_y_base, k_byte_offsets, vl);
    const vfloat32m2_t scene_z_k = __riscv_vluxei32_v_f32m2(scene_z_base, k_byte_offsets, vl);

    const vfloat32m2_t cand_model_x_v = __riscv_vfmv_v_f_f32m2(cand_model_x, vl);
    const vfloat32m2_t cand_model_y_v = __riscv_vfmv_v_f_f32m2(cand_model_y, vl);
    const vfloat32m2_t cand_model_z_v = __riscv_vfmv_v_f_f32m2(cand_model_z, vl);
    const vfloat32m2_t cand_scene_x_v = __riscv_vfmv_v_f_f32m2(cand_scene_x, vl);
    const vfloat32m2_t cand_scene_y_v = __riscv_vfmv_v_f_f32m2(cand_scene_y, vl);
    const vfloat32m2_t cand_scene_z_v = __riscv_vfmv_v_f_f32m2(cand_scene_z, vl);

    const vfloat32m2_t model_dx = __riscv_vfsub_vv_f32m2(model_x_k, cand_model_x_v, vl);
    const vfloat32m2_t model_dy = __riscv_vfsub_vv_f32m2(model_y_k, cand_model_y_v, vl);
    const vfloat32m2_t model_dz = __riscv_vfsub_vv_f32m2(model_z_k, cand_model_z_v, vl);
    const vfloat32m2_t scene_dx = __riscv_vfsub_vv_f32m2(scene_x_k, cand_scene_x_v, vl);
    const vfloat32m2_t scene_dy = __riscv_vfsub_vv_f32m2(scene_y_k, cand_scene_y_v, vl);
    const vfloat32m2_t scene_dz = __riscv_vfsub_vv_f32m2(scene_z_k, cand_scene_z_v, vl);

    const vfloat32m2_t model_norm = __riscv_vfsqrt_v_f32m2(
        __riscv_vfadd_vv_f32m2(
            __riscv_vfadd_vv_f32m2(
                __riscv_vfmul_vv_f32m2(model_dx, model_dx, vl),
                __riscv_vfmul_vv_f32m2(model_dy, model_dy, vl),
                vl),
            __riscv_vfmul_vv_f32m2(model_dz, model_dz, vl),
            vl),
        vl);
    const vfloat32m2_t scene_norm = __riscv_vfsqrt_v_f32m2(
        __riscv_vfadd_vv_f32m2(
            __riscv_vfadd_vv_f32m2(
                __riscv_vfmul_vv_f32m2(scene_dx, scene_dx, vl),
                __riscv_vfmul_vv_f32m2(scene_dy, scene_dy, vl),
                vl),
            __riscv_vfmul_vv_f32m2(scene_dz, scene_dz, vl),
            vl),
        vl);

    const vfloat32m2_t diff =
        __riscv_vfabs_v_f32m2(__riscv_vfsub_vv_f32m2(scene_norm, model_norm, vl), vl);
    const vbool16_t keep = __riscv_vmfle_vf_f32m2_b16(diff, gc_size, vl);
    if (vector_chunks != nullptr)
      ++(*vector_chunks);
    if (__riscv_vcpop_m_b16(keep, vl) != vl)
      return ExecutionPath::RvvPairwisePredicate;

    offset += vl;
  }

  is_consistent = true;
  return ExecutionPath::RvvPairwisePredicate;
#else
  (void)vector_chunks;
  is_consistent = pairwiseConsistencyScalarReference(cloud, consensus_indices, candidate_index, gc_size);
  return ExecutionPath::ScalarFallback;
#endif
}

inline std::size_t
countConsistentCandidatesScalarReference(const PackedCorrespondenceCloud& cloud,
                                         const std::vector<int>& consensus_indices,
                                         const float gc_size)
{
  std::size_t count = 0;
  for (std::size_t candidate = 0; candidate < cloud.size(); ++candidate)
  {
    if (std::find(consensus_indices.begin(), consensus_indices.end(), static_cast<int>(candidate)) !=
        consensus_indices.end())
      continue;
    if (pairwiseConsistencyScalarReference(cloud, consensus_indices, candidate, gc_size))
      ++count;
  }
  return count;
}

inline ExecutionPath
countConsistentCandidatesCandidate(const PackedCorrespondenceCloud& cloud,
                                   const std::vector<int>& consensus_indices,
                                   const float gc_size,
                                   std::size_t& count,
                                   std::size_t* vector_chunks = nullptr)
{
  count = 0;
  std::size_t local_chunks = 0;
  for (std::size_t candidate = 0; candidate < cloud.size(); ++candidate)
  {
    if (std::find(consensus_indices.begin(), consensus_indices.end(), static_cast<int>(candidate)) !=
        consensus_indices.end())
      continue;
    bool is_consistent = false;
    std::size_t candidate_chunks = 0;
    const auto path = pairwiseConsistencyCandidate(
        cloud, consensus_indices, candidate, gc_size, is_consistent, &candidate_chunks);
    local_chunks += candidate_chunks;
    if (is_consistent)
      ++count;
    (void)path;
  }
  if (vector_chunks != nullptr)
    *vector_chunks = local_chunks;
#if defined(__RVV10__)
  return ExecutionPath::RvvPairwisePredicate;
#else
  return ExecutionPath::ScalarFallback;
#endif
}

struct ClusterGrowthResult
{
  std::vector<std::vector<int>> clusters;
  std::size_t accepted_correspondences{0};
  std::size_t vector_chunks{0};
};

inline void
clusterGrowthScalarReference(const PackedCorrespondenceCloud& cloud,
                             const float gc_size,
                             const int gc_threshold,
                             ClusterGrowthResult& result)
{
  result = {};
  std::vector<bool> taken_corresps(cloud.size(), false);

  for (std::size_t i = 0; i < cloud.size(); ++i)
  {
    if (taken_corresps[i])
      continue;

    std::vector<int> consensus_set;
    consensus_set.push_back(static_cast<int>(i));

    for (std::size_t j = 0; j < cloud.size(); ++j)
    {
      if (j == i || taken_corresps[j])
        continue;

      if (pairwiseConsistencyScalarReference(cloud, consensus_set, j, gc_size))
        consensus_set.push_back(static_cast<int>(j));
    }

    if (static_cast<int>(consensus_set.size()) > gc_threshold)
    {
      for (const int index : consensus_set)
        taken_corresps[static_cast<std::size_t>(index)] = true;
      result.accepted_correspondences += consensus_set.size();
      result.clusters.push_back(std::move(consensus_set));
    }
  }
}

inline ExecutionPath
clusterGrowthCandidate(const PackedCorrespondenceCloud& cloud,
                       const float gc_size,
                       const int gc_threshold,
                       ClusterGrowthResult& result)
{
  result = {};
  std::vector<bool> taken_corresps(cloud.size(), false);
  ExecutionPath observed_path = ExecutionPath::ScalarFallback;

  for (std::size_t i = 0; i < cloud.size(); ++i)
  {
    if (taken_corresps[i])
      continue;

    std::vector<int> consensus_set;
    consensus_set.push_back(static_cast<int>(i));

    for (std::size_t j = 0; j < cloud.size(); ++j)
    {
      if (j == i || taken_corresps[j])
        continue;

      bool is_consistent = false;
      std::size_t predicate_chunks = 0;
      const auto path = pairwiseConsistencyCandidate(
          cloud, consensus_set, j, gc_size, is_consistent, &predicate_chunks);
      result.vector_chunks += predicate_chunks;
      if (path == ExecutionPath::RvvPairwisePredicate)
        observed_path = path;
      if (is_consistent)
        consensus_set.push_back(static_cast<int>(j));
    }

    if (static_cast<int>(consensus_set.size()) > gc_threshold)
    {
      for (const int index : consensus_set)
        taken_corresps[static_cast<std::size_t>(index)] = true;
      result.accepted_correspondences += consensus_set.size();
      result.clusters.push_back(std::move(consensus_set));
    }
  }

  return observed_path;
}

inline std::uint64_t
checksumClusters(const ClusterGrowthResult& result)
{
  std::uint64_t checksum = 1469598103934665603ull;
  checksum ^= static_cast<std::uint64_t>(result.clusters.size());
  checksum *= 1099511628211ull;
  checksum ^= static_cast<std::uint64_t>(result.accepted_correspondences);
  checksum *= 1099511628211ull;
  for (const auto& cluster : result.clusters)
  {
    checksum ^= static_cast<std::uint64_t>(cluster.size());
    checksum *= 1099511628211ull;
    for (const int index : cluster)
    {
      checksum ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(index));
      checksum *= 1099511628211ull;
    }
  }
  return checksum;
}

inline std::uint64_t
checksumCount(const std::size_t count)
{
  std::uint64_t result = 1469598103934665603ull;
  result ^= static_cast<std::uint64_t>(count);
  result *= 1099511628211ull;
  return result;
}

} // namespace pcl::test::geometric_consistency_rvv

