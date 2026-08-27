#pragma once

/*
 * 本文件做什么：
 * 构造一个稳定、全有限的 RangeImage（距离图）夹具，并用真实
 * RangeImageBorderExtractor 生成 left/right/top/bottom 四张 border score
 * image（边界分数图）。它服务 Phase 020 的 production-shaped diagnostic：
 * 计时边界可以包含真实 score generation（分数生成），但仍不修改 production。
 */

#include "impl/range_image_border_extractor_score_update.hpp"

#include <pcl/features/range_image_border_extractor.h>
#include <pcl/range_image/range_image.h>

#include <cmath>
#include <cstddef>
#include <vector>

namespace pcl::features::rvv_test::range_image_border_extractor
{

inline pcl::RangeImage
makeRangeImageFixture(const std::uint32_t width, const std::uint32_t height)
{
  pcl::RangeImage range_image;
  range_image.width = width;
  range_image.height = height;
  range_image.is_dense = true;
  range_image.points.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

  const float center_x = 0.5f * static_cast<float>(width - 1);
  const float center_y = 0.5f * static_cast<float>(height - 1);
  for (std::uint32_t y = 0; y < height; ++y)
  {
    for (std::uint32_t x = 0; x < width; ++x)
    {
      const float world_x = (static_cast<float>(x) - center_x) * 0.015f;
      const float world_y = (static_cast<float>(y) - center_y) * 0.015f;
      const float step = x > width / 2 ? 0.42f : 0.0f;
      const float ripple =
          0.025f * std::sin(static_cast<float>(x) * 0.17f) +
          0.020f * std::cos(static_cast<float>(y) * 0.13f);
      pcl::PointWithRange point;
      point.x = world_x;
      point.y = world_y;
      point.z = 2.0f + 0.10f * world_x - 0.06f * world_y + step + ripple;
      point.range = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
      range_image.points[static_cast<std::size_t>(y) * width + x] = point;
    }
  }
  return range_image;
}

inline void
configureExtractor(pcl::RangeImageBorderExtractor& extractor, const float minimum_border_probability)
{
  extractor.getParameters().max_no_of_threads = 1;
  extractor.getParameters().pixel_radius_borders = 3;
  extractor.getParameters().pixel_radius_plane_extraction = 2;
  extractor.getParameters().minimum_border_probability = minimum_border_probability;
}

inline ScoreImageSet
extractProductionScoreImages(const pcl::RangeImage& range_image, const float minimum_border_probability)
{
  pcl::RangeImageBorderExtractor extractor(&range_image);
  configureExtractor(extractor, minimum_border_probability);

  const std::size_t count =
      static_cast<std::size_t>(range_image.width) * static_cast<std::size_t>(range_image.height);
  ScoreImageSet scores;
  const float* left = extractor.getBorderScoresLeft();
  const float* right = extractor.getBorderScoresRight();
  const float* top = extractor.getBorderScoresTop();
  const float* bottom = extractor.getBorderScoresBottom();
  scores.left.assign(left, left + count);
  scores.right.assign(right, right + count);
  scores.top.assign(top, top + count);
  scores.bottom.assign(bottom, bottom + count);
  return scores;
}

inline double
checksumSurfaceStructure(pcl::RangeImageBorderExtractor::LocalSurface* const* surfaces, const std::size_t count)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < count; ++i)
  {
    const auto* surface = surfaces[i];
    if (surface == nullptr)
      continue;
    checksum += static_cast<double>(surface->max_neighbor_distance_squared) *
                static_cast<double>((i % 17) + 1);
    checksum += static_cast<double>(surface->normal_no_jumps[2]) * 0.125;
  }
  return checksum;
}

inline double
checksumScoreImageSet(const ScoreImageSet& scores)
{
  double checksum = 0.0;
  const auto add_image = [&checksum](const std::vector<float>& values, const int salt) {
    for (std::size_t i = 0; i < values.size(); ++i)
      checksum += static_cast<double>(values[i]) * static_cast<double>((i % 31) + 1 + salt);
  };
  add_image(scores.left, 0);
  add_image(scores.right, 3);
  add_image(scores.top, 7);
  add_image(scores.bottom, 11);
  return checksum;
}

inline std::size_t
countMeaningfulScores(const ScoreImageSet& scores)
{
  std::size_t count = 0;
  const auto add_image = [&count](const std::vector<float>& values) {
    for (const float value : values)
      if (std::abs(value) > 1.0e-6f)
        ++count;
  };
  add_image(scores.left);
  add_image(scores.right);
  add_image(scores.top);
  add_image(scores.bottom);
  return count;
}

} // namespace pcl::features::rvv_test::range_image_border_extractor
