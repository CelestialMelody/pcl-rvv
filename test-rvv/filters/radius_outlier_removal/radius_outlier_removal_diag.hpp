#pragma once

#include "radius_outlier_removal_replay.hpp"

#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace pcl_rvv_filters_radius_outlier_removal {

inline void
mix(std::uint64_t& h, std::uint64_t v);

inline std::uint64_t
checksumIndices(const pcl::Indices& values);

struct ApplyFilterIndicesDiagnosticResult {
  ApplyFilterIndicesTailResult tail;
  std::uint64_t checksum{0};
  std::uint64_t search_checksum{0};
};

struct EntryDiagnosticResult {
  pcl::Indices kept;
  pcl::Indices removed;
  std::uint64_t checksum{0};
};

// Test-only subclass of the upstream target class. This preserves the public
// setup shape for entry smoke tests, but it does not replace production code.
template <typename PointT>
class RadiusOutlierRemovalEntryDiagnostic
    : public pcl::RadiusOutlierRemoval<PointT> {
public:
  using Base = pcl::RadiusOutlierRemoval<PointT>;
  using PointCloud = pcl::PointCloud<PointT>;
  using PointCloudConstPtr = typename PointCloud::ConstPtr;

  explicit RadiusOutlierRemovalEntryDiagnostic(bool extract_removed_indices = false)
  : Base(extract_removed_indices)
  {}

  // Same-shape diagnostic entry for the upstream target:
  //   RadiusOutlierRemoval<PointT>::applyFilterIndices(Indices&)
  void
  applyFilterIndices(pcl::Indices& indices)
  {
    if (!this->initCompute())
      return;
    Base::applyFilterIndices(indices);
    this->deinitCompute();
  }

  EntryDiagnosticResult
  runApplyFilterIndicesEntry()
  {
    EntryDiagnosticResult result;
    applyFilterIndices(result.kept);
    if (this->getRemovedIndices())
      result.removed = *this->getRemovedIndices();
    result.checksum = checksumEntryDiagnosticResult(result);
    return result;
  }

  static std::uint64_t
  checksumEntryDiagnosticResult(const EntryDiagnosticResult& result)
  {
    std::uint64_t h = checksumIndices(result.kept);
    mix(h, checksumIndices(result.removed));
    mix(h, result.kept.size());
    mix(h, result.removed.size());
    return h;
  }
};

inline void
mix(std::uint64_t& h, std::uint64_t v)
{
  h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
}

inline std::uint64_t
checksumIndices(const pcl::Indices& values)
{
  std::uint64_t h = 1469598103934665603ull;
  for (const auto value : values)
    mix(h, static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)));
  return h;
}

inline std::uint64_t
checksumApplyFilterIndicesTailResult(const ApplyFilterIndicesTailResult& result)
{
  std::uint64_t h = checksumIndices(result.kept);
  mix(h, checksumIndices(result.removed));
  mix(h, result.kept.size());
  mix(h, result.removed.size());
  return h;
}

inline pcl::Indices
makeIndices(std::size_t n, bool shuffled)
{
  pcl::Indices indices(n);
  std::iota(indices.begin(), indices.end(), 0);
  if (shuffled) {
    for (std::size_t i = 0; i < n; ++i) {
      const std::size_t j = (i * 1103515245u + 12345u) % n;
      std::swap(indices[i], indices[j]);
    }
  }
  return indices;
}

inline std::vector<std::uint8_t>
makeKeepMask(std::size_t n, int pattern)
{
  std::vector<std::uint8_t> to_keep(n, 1);
  for (std::size_t i = 0; i < n; ++i) {
    bool keep = true;
    switch (pattern) {
      case 0:
        keep = ((i % 17u) != 0u);
        break;
      case 1:
        keep = ((i * 37u + 11u) % 100u) < 50u;
        break;
      case 2:
        keep = ((i * 13u + 7u) % 100u) < 12u;
        break;
      default:
        keep = ((i & 3u) != 0u);
        break;
    }
    to_keep[i] = keep ? 1u : 0u;
  }
  return to_keep;
}

inline pcl::PointCloud<pcl::PointXYZ>
makePointCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.resize(n);
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  for (std::size_t i = 0; i < n; ++i) {
    const float x = static_cast<float>((i * 17u) % 257u) * 0.03125f;
    const float y = static_cast<float>((i * 29u + 3u) % 263u) * 0.02734375f;
    const float z = static_cast<float>((i * 41u + 5u) % 269u) * 0.0234375f;
    cloud[i].x = x;
    cloud[i].y = y;
    cloud[i].z = z;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::PointXYZ>
makeEntryDiagnosticPointCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.resize(n);
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t cluster = i / 8u;
    const std::size_t lane = i % 8u;
    cloud[i].x = static_cast<float>(cluster) * 0.25f + static_cast<float>(lane) * 0.006f;
    cloud[i].y = static_cast<float>((lane * 3u) % 7u) * 0.004f;
    cloud[i].z = static_cast<float>((lane * 5u) % 11u) * 0.003f;
  }
  return cloud;
}

// Mirrors the work done before the tail loop in radius_outlier_removal.hpp:87-155.
inline std::uint64_t
simulateApplyFilterIndicesSearchPhase(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                      const pcl::Indices& indices,
                                      int rounds)
{
  double acc = 0.0;
  for (int r = 0; r < rounds; ++r) {
    for (const auto idx : indices) {
      const auto& p = cloud[static_cast<std::size_t>(idx)];
      const double dx = static_cast<double>(p.x) + 0.001 * static_cast<double>(r + 1);
      const double dy = static_cast<double>(p.y) - 0.002 * static_cast<double>((r % 5) + 1);
      const double dz = static_cast<double>(p.z) + 0.003 * static_cast<double>((r % 7) + 1);
      acc += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
  }
  return static_cast<std::uint64_t>(acc * 1000000.0);
}

// Mirrors full applyFilterIndices flow: search phase -> tail loop.
inline ApplyFilterIndicesDiagnosticResult
runApplyFilterIndicesDiagnostic(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                const pcl::Indices& source_indices,
                                const std::vector<std::uint8_t>& to_keep,
                                bool extract_removed_indices,
                                int simulated_search_rounds,
                                bool use_rvv)
{
  ApplyFilterIndicesDiagnosticResult result;
  result.search_checksum = simulateApplyFilterIndicesSearchPhase(cloud, source_indices, simulated_search_rounds);
  ApplyFilterIndicesTailReplayContext tail_ctx{&source_indices, &to_keep, extract_removed_indices};
  result.tail = applyFilterIndicesTailAutoReplay(tail_ctx, use_rvv);
  result.checksum = checksumApplyFilterIndicesTailResult(result.tail);
  mix(result.checksum, result.search_checksum);
  return result;
}

} // namespace pcl_rvv_filters_radius_outlier_removal
