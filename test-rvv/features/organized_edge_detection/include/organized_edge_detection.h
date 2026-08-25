#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pcl::features::rvv_test::organized_edge_detection
{
constexpr std::uint32_t kInvalidLabel = 0U;
constexpr std::uint32_t kNanBoundary = 1U;
constexpr std::uint32_t kOccluding = 2U;
constexpr std::uint32_t kOccluded = 4U;
constexpr int kNumEdgeTypes = 5;

void
computeDepthLabelsScalar(const float* z,
                         std::size_t width,
                         std::size_t height,
                         float depth_discon_threshold,
                         int max_search_neighbors,
                         std::uint32_t detecting_edge_types,
                         std::uint32_t* labels);

void
computeDepthLabelsRVV(const float* z,
                      std::size_t width,
                      std::size_t height,
                      float depth_discon_threshold,
                      int max_search_neighbors,
                      std::uint32_t detecting_edge_types,
                      std::uint32_t* labels);

std::vector<std::vector<std::size_t>>
assignLabelIndices(const std::uint32_t* labels, std::size_t count);

double
checksumLabels(const std::uint32_t* labels, std::size_t count);
} // namespace pcl::features::rvv_test::organized_edge_detection

#include "impl/organized_edge_detection_depth_labels.hpp"
