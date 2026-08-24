#pragma once

#include <cstddef>

namespace pcl::features::rvv_test::integral_image_normal
{
void
buildDepthChangeMapRVV(const float* z,
                       std::size_t width,
                       std::size_t height,
                       float max_depth_change_factor,
                       unsigned char* depth_change_map);

void
initializeDistanceMapRVV(const unsigned char* depth_change_map,
                         std::size_t count,
                         float far_distance,
                         float* distance_map);

template <typename PointT>
void
buildAverage3DGradientDiffBuffersRVV(const PointT* points,
                                     std::size_t width,
                                     std::size_t height,
                                     float* diff_x,
                                     float* diff_y);
} // namespace pcl::features::rvv_test::integral_image_normal

#include "impl/integral_image_normal_map_prep.hpp"
