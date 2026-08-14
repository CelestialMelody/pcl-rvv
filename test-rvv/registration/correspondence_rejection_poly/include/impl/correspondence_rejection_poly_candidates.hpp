/*
 * 本文件做什么：
 * 这里保存 correspondence_rejection_poly topic 的 test-only reference（测试参考链路）
 * 和 RVV candidate（RVV 候选链路）。reference 复刻
 * registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp 中
 * getRemainingCorrespondences、thresholdPolygon、computeHistogram 和 findThresholdOtsu
 * 的关键语义。candidate 只覆盖连续数组上的局部热点：edge similarity（边相似度）
 * 批量计算、accept rate（接受率）计算和最终筛选 mask。
 *
 * 证据边界：
 * 本文件没有 production dispatch（生产分流）。完整随机采样仍由标量 reference 或
 * production class 负责；RVV 分支只证明局部数据并行候选是否保持 same-chain（同构链路）
 * 语义，并为后续 QEMU / asm / board evidence（板卡证据）留下入口。
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <vector>

#include <pcl/conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/correspondence_rejection_poly.h>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl::registration::rvv_correspondence_rejection_poly_support {

struct CandidateStats {
  bool used_rvv{false};
  std::size_t input_size{0};
  std::size_t output_size{0};
};

struct AcceptanceRateResult {
  std::vector<float> rates;
  CandidateStats stats;
};

struct FilterResult {
  std::vector<int> kept_indices;
  CandidateStats stats;
};

struct EdgeBatchResult {
  std::vector<std::uint8_t> accepted;
  CandidateStats stats;
};

struct EdgePair {
  int first_correspondence{0};
  int second_correspondence{0};
};

inline pcl::PointCloud<pcl::PointXYZ>::Ptr
make_source_cloud(const std::size_t size)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->resize(size);
  for (std::size_t i = 0; i < size; ++i) {
    const float t = static_cast<float>(i);
    (*cloud)[i].x = 0.125f * t + static_cast<float>((i % 7) * 0.01);
    (*cloud)[i].y = 0.03125f * static_cast<float>((i * i) % 97);
    (*cloud)[i].z = 0.0625f * static_cast<float>((i * 13) % 53);
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZ>::Ptr
make_target_cloud_from_source(const pcl::PointCloud<pcl::PointXYZ>& source,
                              const std::size_t scrambled_prefix)
{
  auto target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  *target = source;
  if (target->empty())
    return target;

  const std::size_t shift = std::max<std::size_t>(1, target->size() / 8);
  const std::size_t last = std::min(scrambled_prefix, target->size());
  for (std::size_t i = 0; i < last; ++i) {
    (*target)[i] = source[(i + shift) % source.size()];
    (*target)[i].x += 0.37f;
    (*target)[i].y -= 0.19f;
  }
  return target;
}

inline pcl::Correspondences
make_identity_correspondences(const std::size_t size)
{
  pcl::Correspondences correspondences;
  correspondences.reserve(size);
  for (std::size_t i = 0; i < size; ++i)
    correspondences.emplace_back(static_cast<int>(i), static_cast<int>(i), 0.0f);
  return correspondences;
}

inline std::vector<EdgePair>
make_edge_pairs(const std::size_t edge_count, const std::size_t correspondence_count)
{
  std::vector<EdgePair> edge_pairs;
  if (correspondence_count < 2)
    return edge_pairs;

  edge_pairs.reserve(edge_count);
  for (std::size_t i = 0; i < edge_count; ++i) {
    const std::size_t first = (i * 17u + 3u) % correspondence_count;
    const std::size_t offset = 1u + ((i * 13u + 7u) % (correspondence_count - 1u));
    const std::size_t second = (first + offset) % correspondence_count;
    edge_pairs.push_back(
        {static_cast<int>(first), static_cast<int>(second)});
  }
  return edge_pairs;
}

inline float
compute_squared_distance(const pcl::PointXYZ& p1, const pcl::PointXYZ& p2)
{
  const float dx = p2.x - p1.x;
  const float dy = p2.y - p1.y;
  const float dz = p2.z - p1.z;
  return dx * dx + dy * dy + dz * dz;
}

inline bool
threshold_edge_length_reference(const pcl::PointCloud<pcl::PointXYZ>& source,
                                const pcl::PointCloud<pcl::PointXYZ>& target,
                                int source_index_1,
                                int source_index_2,
                                int target_index_1,
                                int target_index_2,
                                const float similarity_threshold_squared)
{
  const float dist_src =
      compute_squared_distance(source[source_index_1], source[source_index_2]);
  const float dist_tgt =
      compute_squared_distance(target[target_index_1], target[target_index_2]);
  const float edge_sim =
      dist_src < dist_tgt ? dist_src / dist_tgt : dist_tgt / dist_src;
  return edge_sim >= similarity_threshold_squared;
}

inline bool
threshold_polygon_reference(const pcl::PointCloud<pcl::PointXYZ>& source,
                            const pcl::PointCloud<pcl::PointXYZ>& target,
                            const pcl::Correspondences& correspondences,
                            const std::vector<int>& sampled_indices,
                            const int cardinality,
                            const float similarity_threshold_squared)
{
  if (cardinality == 2) {
    return threshold_edge_length_reference(
        source,
        target,
        correspondences[sampled_indices[0]].index_query,
        correspondences[sampled_indices[1]].index_query,
        correspondences[sampled_indices[0]].index_match,
        correspondences[sampled_indices[1]].index_match,
        similarity_threshold_squared);
  }

  for (int i = 0; i < cardinality; ++i) {
    const int next = (i + 1) % cardinality;
    if (!threshold_edge_length_reference(
            source,
            target,
            correspondences[sampled_indices[i]].index_query,
            correspondences[sampled_indices[next]].index_query,
            correspondences[sampled_indices[i]].index_match,
            correspondences[sampled_indices[next]].index_match,
            similarity_threshold_squared)) {
      return false;
    }
  }
  return true;
}

inline std::vector<int>
unique_random_indices_reference(const int n, const int k)
{
  std::vector<bool> sampled(static_cast<std::size_t>(n), false);
  std::vector<int> result;
  result.reserve(static_cast<std::size_t>(k));
  int samples = 0;
  do {
    const int idx = std::rand() % n;
    if (!sampled[static_cast<std::size_t>(idx)]) {
      sampled[static_cast<std::size_t>(idx)] = true;
      ++samples;
      result.push_back(idx);
    }
  } while (samples < k);
  return result;
}

inline std::vector<float>
compute_acceptance_rates_reference(const std::vector<int>& num_samples,
                                   const std::vector<int>& num_accepted)
{
  std::vector<float> accept_rate(num_samples.size(), 0.0f);
  for (std::size_t i = 0; i < num_samples.size(); ++i) {
    if (num_samples[i] != 0)
      accept_rate[i] =
          static_cast<float>(num_accepted[i]) / static_cast<float>(num_samples[i]);
  }
  return accept_rate;
}

inline AcceptanceRateResult
compute_acceptance_rates_candidate(const std::vector<int>& num_samples,
                                   const std::vector<int>& num_accepted)
{
  AcceptanceRateResult result;
  result.rates.assign(num_samples.size(), 0.0f);
  result.stats.input_size = num_samples.size();

#ifdef __RVV10__
  if (num_samples.size() == num_accepted.size() && !num_samples.empty()) {
    std::size_t i = 0;
    while (i < num_samples.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(num_samples.size() - i);
      const vint32m2_t samples = __riscv_vle32_v_i32m2(num_samples.data() + i, vl);
      const vint32m2_t accepted = __riscv_vle32_v_i32m2(num_accepted.data() + i, vl);
      const vbool16_t has_samples = __riscv_vmsne_vx_i32m2_b16(samples, 0, vl);
      const vfloat32m2_t sample_f = __riscv_vfcvt_f_x_v_f32m2(samples, vl);
      const vfloat32m2_t accepted_f = __riscv_vfcvt_f_x_v_f32m2(accepted, vl);
      const vfloat32m2_t ratio = __riscv_vfdiv_vv_f32m2(accepted_f, sample_f, vl);
      const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
      const vfloat32m2_t selected =
          __riscv_vmerge_vvm_f32m2(zero, ratio, has_samples, vl);
      __riscv_vse32_v_f32m2(result.rates.data() + i, selected, vl);
      i += vl;
    }
    result.stats.used_rvv = true;
    result.stats.output_size = result.rates.size();
    return result;
  }
#endif

  result.rates = compute_acceptance_rates_reference(num_samples, num_accepted);
  result.stats.output_size = result.rates.size();
  return result;
}

inline std::vector<int>
compute_histogram_reference(const std::vector<float>& data,
                            const float lower,
                            const float upper,
                            const int bins)
{
  std::vector<int> result(static_cast<std::size_t>(bins), 0);
  const int last_idx = bins - 1;
  const float idx_per_val = static_cast<float>(bins) / (upper - lower);
  for (const float value : data) {
    const int idx = std::min(last_idx, static_cast<int>(value * idx_per_val));
    ++result[static_cast<std::size_t>(idx)];
  }
  return result;
}

inline int
find_threshold_otsu_reference(const std::vector<int>& histogram)
{
  constexpr double eps = std::numeric_limits<double>::epsilon();
  const int nbins = static_cast<int>(histogram.size());
  double mean = 0.0;
  double sum_inv = 0.0;
  for (int i = 0; i < nbins; ++i) {
    mean += static_cast<double>(i * histogram[static_cast<std::size_t>(i)]);
    sum_inv += static_cast<double>(histogram[static_cast<std::size_t>(i)]);
  }
  sum_inv = 1.0 / sum_inv;
  mean *= sum_inv;

  double class_mean1 = 0.0;
  double class_prob1 = 0.0;
  double class_prob2 = 1.0;
  double between_class_variance_max = 0.0;
  int result = 0;

  for (int i = 0; i < nbins; ++i) {
    class_mean1 *= class_prob1;
    const double prob_i =
        static_cast<double>(histogram[static_cast<std::size_t>(i)]) * sum_inv;
    class_prob1 += prob_i;
    class_prob2 -= prob_i;
    if (std::min(class_prob1, class_prob2) < eps ||
        std::max(class_prob1, class_prob2) > 1.0 - eps)
      continue;

    class_mean1 = (class_mean1 + static_cast<double>(i) * prob_i) / class_prob1;
    const double class_mean2 = (mean - class_prob1 * class_mean1) / class_prob2;
    const double between_class_variance = class_prob1 * class_prob2 *
                                          (class_mean1 - class_mean2) *
                                          (class_mean1 - class_mean2);
    if (between_class_variance > between_class_variance_max) {
      between_class_variance_max = between_class_variance;
      result = i;
    }
  }
  return result;
}

inline FilterResult
filter_by_acceptance_rate_reference(const std::vector<float>& accept_rate,
                                    const float cut)
{
  FilterResult result;
  result.stats.input_size = accept_rate.size();
  for (std::size_t i = 0; i < accept_rate.size(); ++i) {
    if (accept_rate[i] > cut)
      result.kept_indices.push_back(static_cast<int>(i));
  }
  result.stats.output_size = result.kept_indices.size();
  return result;
}

inline FilterResult
filter_by_acceptance_rate_candidate(const std::vector<float>& accept_rate,
                                    const float cut)
{
  FilterResult result;
  result.stats.input_size = accept_rate.size();
  result.kept_indices.reserve(accept_rate.size());

#ifdef __RVV10__
  if (!accept_rate.empty()) {
    std::vector<float> chunk(__riscv_vsetvlmax_e32m2());
    std::size_t i = 0;
    while (i < accept_rate.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(accept_rate.size() - i);
      const vfloat32m2_t values = __riscv_vle32_v_f32m2(accept_rate.data() + i, vl);
      const vbool16_t keep = __riscv_vmfgt_vf_f32m2_b16(values, cut, vl);
      const vfloat32m2_t keep_as_one =
          __riscv_vmerge_vvm_f32m2(__riscv_vfmv_v_f_f32m2(0.0f, vl),
                                   __riscv_vfmv_v_f_f32m2(1.0f, vl),
                                   keep,
                                   vl);
      __riscv_vse32_v_f32m2(chunk.data(), keep_as_one, vl);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        if (chunk[lane] != 0.0f)
          result.kept_indices.push_back(static_cast<int>(i + lane));
      }
      i += vl;
    }
    result.stats.used_rvv = true;
    result.stats.output_size = result.kept_indices.size();
    return result;
  }
#endif

  result = filter_by_acceptance_rate_reference(accept_rate, cut);
  return result;
}

inline EdgeBatchResult
edge_similarity_batch_reference(const std::vector<float>& source_distances,
                                const std::vector<float>& target_distances,
                                const float similarity_threshold_squared)
{
  EdgeBatchResult result;
  result.accepted.assign(source_distances.size(), 0);
  result.stats.input_size = source_distances.size();
  for (std::size_t i = 0; i < source_distances.size(); ++i) {
    const float dist_src = source_distances[i];
    const float dist_tgt = target_distances[i];
    const float edge_sim =
        dist_src < dist_tgt ? dist_src / dist_tgt : dist_tgt / dist_src;
    result.accepted[i] = edge_sim >= similarity_threshold_squared ? 1u : 0u;
  }
  result.stats.output_size =
      static_cast<std::size_t>(std::accumulate(result.accepted.begin(),
                                              result.accepted.end(),
                                              0u));
  return result;
}

inline EdgeBatchResult
edge_similarity_batch_candidate(const std::vector<float>& source_distances,
                                const std::vector<float>& target_distances,
                                const float similarity_threshold_squared)
{
  EdgeBatchResult result;
  result.accepted.assign(source_distances.size(), 0);
  result.stats.input_size = source_distances.size();

#ifdef __RVV10__
  if (source_distances.size() == target_distances.size() && !source_distances.empty()) {
    std::vector<float> chunk(__riscv_vsetvlmax_e32m2());
    std::size_t i = 0;
    while (i < source_distances.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(source_distances.size() - i);
      const vfloat32m2_t src = __riscv_vle32_v_f32m2(source_distances.data() + i, vl);
      const vfloat32m2_t tgt = __riscv_vle32_v_f32m2(target_distances.data() + i, vl);
      const vfloat32m2_t min_v = __riscv_vfmin_vv_f32m2(src, tgt, vl);
      const vfloat32m2_t max_v = __riscv_vfmax_vv_f32m2(src, tgt, vl);
      const vfloat32m2_t ratio = __riscv_vfdiv_vv_f32m2(min_v, max_v, vl);
      const vbool16_t keep =
          __riscv_vmfge_vf_f32m2_b16(ratio, similarity_threshold_squared, vl);
      const vfloat32m2_t keep_as_one =
          __riscv_vmerge_vvm_f32m2(__riscv_vfmv_v_f_f32m2(0.0f, vl),
                                   __riscv_vfmv_v_f_f32m2(1.0f, vl),
                                   keep,
                                   vl);
      __riscv_vse32_v_f32m2(chunk.data(), keep_as_one, vl);
      for (std::size_t lane = 0; lane < vl; ++lane)
        result.accepted[i + lane] = chunk[lane] != 0.0f ? 1u : 0u;
      i += vl;
    }
    result.stats.used_rvv = true;
    result.stats.output_size =
        static_cast<std::size_t>(std::accumulate(result.accepted.begin(),
                                                result.accepted.end(),
                                                0u));
    return result;
  }
#endif

  result = edge_similarity_batch_reference(
      source_distances, target_distances, similarity_threshold_squared);
  return result;
}

inline EdgeBatchResult
edge_similarity_gather_reference(const pcl::PointCloud<pcl::PointXYZ>& source,
                                 const pcl::PointCloud<pcl::PointXYZ>& target,
                                 const pcl::Correspondences& correspondences,
                                 const std::vector<EdgePair>& edge_pairs,
                                 const float similarity_threshold_squared)
{
  EdgeBatchResult result;
  result.accepted.assign(edge_pairs.size(), 0);
  result.stats.input_size = edge_pairs.size();

  for (std::size_t i = 0; i < edge_pairs.size(); ++i) {
    const EdgePair& edge = edge_pairs[i];
    const pcl::Correspondence& first =
        correspondences[static_cast<std::size_t>(edge.first_correspondence)];
    const pcl::Correspondence& second =
        correspondences[static_cast<std::size_t>(edge.second_correspondence)];
    result.accepted[i] =
        threshold_edge_length_reference(source,
                                        target,
                                        first.index_query,
                                        second.index_query,
                                        first.index_match,
                                        second.index_match,
                                        similarity_threshold_squared)
            ? 1u
            : 0u;
  }

  result.stats.output_size =
      static_cast<std::size_t>(std::accumulate(result.accepted.begin(),
                                              result.accepted.end(),
                                              0u));
  return result;
}

inline EdgeBatchResult
edge_similarity_gather_candidate(const pcl::PointCloud<pcl::PointXYZ>& source,
                                 const pcl::PointCloud<pcl::PointXYZ>& target,
                                 const pcl::Correspondences& correspondences,
                                 const std::vector<EdgePair>& edge_pairs,
                                 const float similarity_threshold_squared)
{
#ifdef __RVV10__
  if (!edge_pairs.empty()) {
    std::vector<float> source_distances(edge_pairs.size());
    std::vector<float> target_distances(edge_pairs.size());
    for (std::size_t i = 0; i < edge_pairs.size(); ++i) {
      const EdgePair& edge = edge_pairs[i];
      const pcl::Correspondence& first =
          correspondences[static_cast<std::size_t>(edge.first_correspondence)];
      const pcl::Correspondence& second =
          correspondences[static_cast<std::size_t>(edge.second_correspondence)];
      source_distances[i] =
          compute_squared_distance(source[first.index_query], source[second.index_query]);
      target_distances[i] =
          compute_squared_distance(target[first.index_match], target[second.index_match]);
    }
    return edge_similarity_batch_candidate(
        source_distances, target_distances, similarity_threshold_squared);
  }
#endif

  return edge_similarity_gather_reference(
      source, target, correspondences, edge_pairs, similarity_threshold_squared);
}

inline pcl::Correspondences
remaining_correspondences_reference(const pcl::PointCloud<pcl::PointXYZ>& source,
                                    const pcl::PointCloud<pcl::PointXYZ>& target,
                                    const pcl::Correspondences& original_correspondences,
                                    const int iterations,
                                    const int cardinality,
                                    const float similarity_threshold)
{
  pcl::Correspondences remaining = original_correspondences;
  if (cardinality < 2)
    return remaining;
  const int nr_correspondences = static_cast<int>(original_correspondences.size());
  if (cardinality >= nr_correspondences)
    return remaining;
  if (similarity_threshold < 0.0f || similarity_threshold > 1.0f)
    return remaining;

  const float similarity_threshold_squared =
      similarity_threshold * similarity_threshold;
  remaining.clear();
  remaining.reserve(static_cast<std::size_t>(nr_correspondences));
  std::vector<int> num_samples(static_cast<std::size_t>(nr_correspondences), 0);
  std::vector<int> num_accepted(static_cast<std::size_t>(nr_correspondences), 0);

  for (int i = 0; i < iterations; ++i) {
    const std::vector<int> idx =
        unique_random_indices_reference(nr_correspondences, cardinality);
    const bool accepted =
        threshold_polygon_reference(source,
                                    target,
                                    original_correspondences,
                                    idx,
                                    cardinality,
                                    similarity_threshold_squared);
    for (int j = 0; j < cardinality; ++j) {
      ++num_samples[static_cast<std::size_t>(idx[static_cast<std::size_t>(j)])];
      if (accepted)
        ++num_accepted[static_cast<std::size_t>(idx[static_cast<std::size_t>(j)])];
    }
  }

  const std::vector<float> accept_rate =
      compute_acceptance_rates_reference(num_samples, num_accepted);
  const int hist_size = nr_correspondences / 2;
  const std::vector<int> histogram =
      compute_histogram_reference(accept_rate, 0.0f, 1.0f, hist_size);
  const int cut_idx = find_threshold_otsu_reference(histogram);
  const float cut = static_cast<float>(cut_idx) / static_cast<float>(hist_size);
  for (int i = 0; i < nr_correspondences; ++i) {
    if (accept_rate[static_cast<std::size_t>(i)] > cut)
      remaining.push_back(original_correspondences[static_cast<std::size_t>(i)]);
  }
  return remaining;
}

inline std::uint64_t
checksum_u8(const std::vector<std::uint8_t>& values)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto value : values)
    checksum = (checksum ^ static_cast<std::uint64_t>(value)) * 1099511628211ull;
  return checksum;
}

inline std::uint64_t
checksum_indices(const std::vector<int>& values)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto value : values)
    checksum = (checksum ^ static_cast<std::uint32_t>(value)) * 1099511628211ull;
  return checksum;
}

inline std::uint64_t
checksum_floats(const std::vector<float>& values)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const float value : values) {
    const auto scaled = static_cast<std::int64_t>(value * 1000000.0f);
    checksum = (checksum ^ static_cast<std::uint64_t>(scaled)) * 1099511628211ull;
  }
  return checksum;
}

} // namespace pcl::registration::rvv_correspondence_rejection_poly_support
