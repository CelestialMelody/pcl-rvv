/*
 * 本文件做什么：
 * 这里保存 GASD（Globally Aligned Spatial Distribution，全球对齐空间分布）
 * topic 的 test-only fixture（测试专用夹具）和 scalar reference（标量参考链路）
 * 辅助函数。当前 phase 000 主要把固定网格直拷贝需要的 synthetic histogram
 * （合成直方图）形状固定下来，供后续 RVV candidate（候选实现）和 bench 对拍。
 *
 * 证据边界：
 * 这些 helper 只证明当前 topic 的局部形状和连续写回语义。它们不证明 production
 * dispatch（生产分流）、不覆盖完整 `GASDEstimation::computeFeature` 热点，也不把
 * QEMU（仿真器）时间写成性能结论。
 */

#pragma once

#include <Eigen/Core>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

namespace pcl::features::rvv_test::gasd
{
using ShapeCloudT = pcl::PointCloud<pcl::PointXYZ>;
using ColorCloudT = pcl::PointCloud<pcl::PointXYZRGBA>;
using HistogramGrid = std::vector<Eigen::VectorXf>;

struct ShapeProjectionBuffers
{
  std::vector<float> grid_x;
  std::vector<float> grid_y;
  std::vector<float> grid_z;
  std::vector<float> dbin;
};

struct ColorHueBuffers
{
  std::vector<float> hue;
  std::vector<float> hbin;
};

struct TrilinearInterpolationBuffers
{
  std::vector<std::uint32_t> grid_idx;
  std::vector<std::uint32_t> h_idx;
  std::vector<float> w000;
  std::vector<float> w001;
  std::vector<float> w010;
  std::vector<float> w011;
  std::vector<float> w100;
  std::vector<float> w101;
  std::vector<float> w110;
  std::vector<float> w111;
};

struct ShapeProjectionNormalization
{
  float max_coord = 1.0f;
  float distance_normalization_factor = 1.0f;
};

inline ShapeCloudT::Ptr
makeShapeCloud(const std::size_t count)
{
  auto cloud = ShapeCloudT::Ptr(new ShapeCloudT);
  cloud->points.resize(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    const float f = static_cast<float>(i);
    cloud->points[i].x = 0.0375f * f - 2.5f;
    cloud->points[i].y = 0.0125f * static_cast<float>((i * 5) % 23) - 1.5f;
    cloud->points[i].z = 0.021f * f * f - 0.45f * static_cast<float>(i % 7);
  }
  cloud->width = static_cast<std::uint32_t>(count);
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

inline ColorCloudT::Ptr
makeColorCloud(const ShapeCloudT& shape_cloud)
{
  auto cloud = ColorCloudT::Ptr(new ColorCloudT);
  cloud->points.resize(shape_cloud.size());
  for (std::size_t i = 0; i < shape_cloud.size(); ++i)
  {
    cloud->points[i].x = shape_cloud[i].x;
    cloud->points[i].y = shape_cloud[i].y;
    cloud->points[i].z = shape_cloud[i].z;
    cloud->points[i].r = static_cast<std::uint8_t>((i * 37) % 255);
    cloud->points[i].g = static_cast<std::uint8_t>((255 - i * 11) % 255);
    cloud->points[i].b = static_cast<std::uint8_t>((i * 19) % 255);
    cloud->points[i].a = 255;
  }
  cloud->width = static_cast<std::uint32_t>(shape_cloud.size());
  cloud->height = 1;
  cloud->is_dense = true;
  return cloud;
}

inline std::size_t
gridCellCount(const std::size_t half_grid_size)
{
  const std::size_t grid_size = half_grid_size * 2;
  return (grid_size + 2) * (grid_size + 2) * (grid_size + 2);
}

inline HistogramGrid
makeHistogramGrid(const std::size_t half_grid_size, const std::size_t hists_size)
{
  const std::size_t cells = gridCellCount(half_grid_size);
  HistogramGrid hists(cells, Eigen::VectorXf::Zero(static_cast<Eigen::Index>(hists_size + 2)));
  for (std::size_t cell = 0; cell < cells; ++cell)
  {
    for (std::size_t bin = 0; bin < hists[cell].size(); ++bin)
      hists[cell][static_cast<Eigen::Index>(bin)] = 0.01f * static_cast<float>(cell + 1) +
                                                    0.001f * static_cast<float>(bin + 1);
  }
  return hists;
}

inline std::vector<float>
copyShapeHistogramsStd(const HistogramGrid& hists,
                       const std::size_t half_grid_size,
                       const std::size_t hists_size)
{
  const std::size_t grid_size = half_grid_size * 2;
  std::vector<float> output(grid_size * grid_size * grid_size * hists_size, 0.0f);
  std::size_t pos = 0;
  for (std::size_t i = 0; i < grid_size; ++i)
  {
    for (std::size_t j = 0; j < grid_size; ++j)
    {
      for (std::size_t k = 0; k < grid_size; ++k)
      {
        const std::size_t idx = ((i + 1) * (grid_size + 2) + (j + 1)) * (grid_size + 2) + (k + 1);
        std::copy(hists[idx].data() + 1, hists[idx].data() + 1 + hists_size, output.data() + pos);
        pos += hists_size;
      }
    }
  }
  return output;
}

inline std::vector<float>
copyColorHistogramsStd(HistogramGrid hists,
                       const std::size_t half_grid_size,
                       const std::size_t hists_size)
{
  const std::size_t grid_size = half_grid_size * 2;
  std::vector<float> output(grid_size * grid_size * grid_size * hists_size, 0.0f);
  std::size_t pos = 0;
  for (std::size_t i = 0; i < grid_size; ++i)
  {
    for (std::size_t j = 0; j < grid_size; ++j)
    {
      for (std::size_t k = 0; k < grid_size; ++k)
      {
        const std::size_t idx = ((i + 1) * (grid_size + 2) + (j + 1)) * (grid_size + 2) + (k + 1);
        hists[idx][1] += hists[idx][hists_size + 1];
        hists[idx][hists_size] += hists[idx][0];
        std::copy(hists[idx].data() + 1, hists[idx].data() + 1 + hists_size, output.data() + pos);
        pos += hists_size;
      }
    }
  }
  return output;
}

inline void
resizeShapeProjectionBuffers(ShapeProjectionBuffers& buffers, const std::size_t count)
{
  buffers.grid_x.assign(count, 0.0f);
  buffers.grid_y.assign(count, 0.0f);
  buffers.grid_z.assign(count, 0.0f);
  buffers.dbin.assign(count, 0.0f);
}

inline void
resizeColorHueBuffers(ColorHueBuffers& buffers, const std::size_t count)
{
  buffers.hue.assign(count, 0.0f);
  buffers.hbin.assign(count, 0.0f);
}

inline void
resizeTrilinearInterpolationBuffers(TrilinearInterpolationBuffers& buffers, const std::size_t count)
{
  buffers.grid_idx.assign(count, 0);
  buffers.h_idx.assign(count, 0);
  buffers.w000.assign(count, 0.0f);
  buffers.w001.assign(count, 0.0f);
  buffers.w010.assign(count, 0.0f);
  buffers.w011.assign(count, 0.0f);
  buffers.w100.assign(count, 0.0f);
  buffers.w101.assign(count, 0.0f);
  buffers.w110.assign(count, 0.0f);
  buffers.w111.assign(count, 0.0f);
}

inline void
resizeFlatHistogram(std::vector<float>& hists,
                    const std::size_t half_grid_size,
                    const std::size_t hists_size)
{
  hists.assign(gridCellCount(half_grid_size) * (hists_size + 2), 0.0f);
}

inline void
resizeEigenHistogram(HistogramGrid& hists,
                     const std::size_t half_grid_size,
                     const std::size_t hists_size)
{
  hists.assign(gridCellCount(half_grid_size), Eigen::VectorXf::Zero(static_cast<Eigen::Index>(hists_size + 2)));
}

inline ShapeProjectionNormalization
computeShapeProjectionNormalization(const ShapeCloudT& cloud)
{
  ShapeProjectionNormalization normalization;
  float max_coord = 0.0f;
  float max_distance = 0.0f;

  for (const auto& sample : cloud)
  {
    max_coord = std::max(max_coord, std::abs(sample.x));
    max_coord = std::max(max_coord, std::abs(sample.y));
    max_coord = std::max(max_coord, std::abs(sample.z));
    max_distance = std::max(max_distance, std::sqrt(sample.x * sample.x + sample.y * sample.y + sample.z * sample.z));
  }

  normalization.max_coord = std::max(max_coord, 1.0f);
  normalization.distance_normalization_factor = std::max(max_distance, 1.0f);
  return normalization;
}

inline void
projectShapeSamplesStdToBuffers(const ShapeCloudT& cloud,
                                const float max_coord,
                                const float distance_normalization_factor,
                                const std::size_t half_grid_size,
                                const std::size_t hists_size,
                                ShapeProjectionBuffers& buffers)
{
  resizeShapeProjectionBuffers(buffers, cloud.size());
  const float half_grid = static_cast<float>(half_grid_size);
  const float shape_grid_step = distance_normalization_factor / half_grid;
  const float coordinate_scale = half_grid / max_coord;
  const float hists_scale = static_cast<float>(hists_size);

  for (std::size_t i = 0; i < cloud.size(); ++i)
  {
    const auto& sample = cloud[i];
    buffers.grid_x[i] = sample.x * coordinate_scale + half_grid;
    buffers.grid_y[i] = sample.y * coordinate_scale + half_grid;
    buffers.grid_z[i] = sample.z * coordinate_scale + half_grid;

    const float distance = std::sqrt(sample.x * sample.x + sample.y * sample.y + sample.z * sample.z);
    const float ratio = distance / shape_grid_step;
    float integral = 0.0f;
    const float fractional = std::modf(ratio, &integral);
    buffers.dbin[i] = fractional * hists_scale;
  }
}

inline void
projectColorHueStdToBuffers(const ColorCloudT& cloud,
                            const std::size_t color_hists_size,
                            ColorHueBuffers& buffers)
{
  resizeColorHueBuffers(buffers, cloud.size());
  const float hists_scale = static_cast<float>(color_hists_size);

  for (std::size_t i = 0; i < cloud.size(); ++i)
  {
    const auto& sample = cloud[i];
    float hue = 0.0f;

    const unsigned char max = std::max(sample.r, std::max(sample.g, sample.b));
    const unsigned char min = std::min(sample.r, std::min(sample.g, sample.b));
    const float diff_inv = 1.0f / static_cast<float>(max - min);

    if (std::isfinite(diff_inv))
    {
      if (max == sample.r)
        hue = 60.0f * (static_cast<float>(sample.g - sample.b) * diff_inv);
      else if (max == sample.g)
        hue = 60.0f * (2.0f + static_cast<float>(sample.b - sample.r) * diff_inv);
      else
        hue = 60.0f * (4.0f + static_cast<float>(sample.r - sample.g) * diff_inv);

      if (hue < 0.0f)
        hue += 360.0f;
    }

    buffers.hue[i] = hue;
    buffers.hbin[i] = (hue / 360.0f) * hists_scale;
  }
}

inline void
computeTrilinearInterpolationStdToBuffers(const ShapeProjectionBuffers& projection,
                                          const std::size_t half_grid_size,
                                          TrilinearInterpolationBuffers& buffers)
{
  resizeTrilinearInterpolationBuffers(buffers, projection.grid_x.size());
  const std::size_t grid_size = half_grid_size * 2;

  for (std::size_t i = 0; i < projection.grid_x.size(); ++i)
  {
    float coord_x = projection.grid_x[i] - 0.5f;
    float coord_y = projection.grid_y[i] - 0.5f;
    float coord_z = projection.grid_z[i] - 0.5f;
    const float coord_h = projection.dbin[i] - 0.5f;

    const float bin_x = std::floor(coord_x);
    const float bin_y = std::floor(coord_y);
    const float bin_z = std::floor(coord_z);
    const float bin_h = std::floor(coord_h);

    buffers.grid_idx[i] = static_cast<std::uint32_t>(
        ((static_cast<std::int32_t>(bin_x) + 1) * static_cast<std::int32_t>(grid_size + 2) +
         static_cast<std::int32_t>(bin_y) + 1) *
            static_cast<std::int32_t>(grid_size + 2) +
        static_cast<std::int32_t>(bin_z) + 1);
    buffers.h_idx[i] = static_cast<std::uint32_t>(static_cast<std::int32_t>(bin_h) + 1);

    coord_x -= bin_x;
    coord_y -= bin_y;
    coord_z -= bin_z;

    const float v_x1 = coord_x;
    const float v_x0 = 1.0f - v_x1;
    const float v_xy11 = v_x1 * coord_y;
    const float v_xy10 = v_x1 - v_xy11;
    const float v_xy01 = v_x0 * coord_y;
    const float v_xy00 = v_x0 - v_xy01;

    buffers.w111[i] = v_xy11 * coord_z;
    buffers.w110[i] = v_xy11 - buffers.w111[i];
    buffers.w101[i] = v_xy10 * coord_z;
    buffers.w100[i] = v_xy10 - buffers.w101[i];
    buffers.w011[i] = v_xy01 * coord_z;
    buffers.w010[i] = v_xy01 - buffers.w011[i];
    buffers.w001[i] = v_xy00 * coord_z;
    buffers.w000[i] = v_xy00 - buffers.w001[i];
  }
}

inline void
accumulateTrilinearInterpolationBuffersToFlatHistogram(const TrilinearInterpolationBuffers& buffers,
                                                       const std::size_t half_grid_size,
                                                       const std::size_t hists_size,
                                                       const float hist_incr,
                                                       std::vector<float>& hists)
{
  const std::size_t grid_stride = half_grid_size * 2 + 2;
  const std::size_t hist_stride = hists_size + 2;
  resizeFlatHistogram(hists, half_grid_size, hists_size);

  const auto add = [&](const std::size_t sample, const std::size_t cell_offset, const float weight) {
    const std::size_t cell = static_cast<std::size_t>(buffers.grid_idx[sample]) + cell_offset;
    const std::size_t h_idx = static_cast<std::size_t>(buffers.h_idx[sample]);
    hists[cell * hist_stride + h_idx] += hist_incr * weight;
  };

  for (std::size_t i = 0; i < buffers.grid_idx.size(); ++i)
  {
    add(i, 0, buffers.w000[i]);
    add(i, 1, buffers.w001[i]);
    add(i, grid_stride, buffers.w010[i]);
    add(i, grid_stride + 1, buffers.w011[i]);
    add(i, grid_stride * grid_stride, buffers.w100[i]);
    add(i, grid_stride * grid_stride + 1, buffers.w101[i]);
    add(i, grid_stride * (grid_stride + 1), buffers.w110[i]);
    add(i, grid_stride * (grid_stride + 1) + 1, buffers.w111[i]);
  }
}

inline void
accumulateTrilinearInterpolationBuffersToEigenHistogram(const TrilinearInterpolationBuffers& buffers,
                                                        const std::size_t half_grid_size,
                                                        const std::size_t hists_size,
                                                        const float hist_incr,
                                                        HistogramGrid& hists)
{
  const std::size_t grid_stride = half_grid_size * 2 + 2;
  resizeEigenHistogram(hists, half_grid_size, hists_size);

  const auto add = [&](const std::size_t sample, const std::size_t cell_offset, const float weight) {
    const std::size_t cell = static_cast<std::size_t>(buffers.grid_idx[sample]) + cell_offset;
    const Eigen::Index h_idx = static_cast<Eigen::Index>(buffers.h_idx[sample]);
    hists[cell][h_idx] += hist_incr * weight;
  };

  for (std::size_t i = 0; i < buffers.grid_idx.size(); ++i)
  {
    add(i, 0, buffers.w000[i]);
    add(i, 1, buffers.w001[i]);
    add(i, grid_stride, buffers.w010[i]);
    add(i, grid_stride + 1, buffers.w011[i]);
    add(i, grid_stride * grid_stride, buffers.w100[i]);
    add(i, grid_stride * grid_stride + 1, buffers.w101[i]);
    add(i, grid_stride * (grid_stride + 1), buffers.w110[i]);
    add(i, grid_stride * (grid_stride + 1) + 1, buffers.w111[i]);
  }
}

inline void
accumulateTrilinearHistogramStd(const ShapeProjectionBuffers& projection,
                                const std::size_t half_grid_size,
                                const std::size_t hists_size,
                                const float hist_incr,
                                std::vector<float>& hists)
{
  const std::size_t grid_size = half_grid_size * 2;
  const std::size_t grid_stride = grid_size + 2;
  const std::size_t hist_stride = hists_size + 2;
  resizeFlatHistogram(hists, half_grid_size, hists_size);

  const auto add = [&](const std::size_t grid_idx,
                       const std::size_t h_idx,
                       const std::size_t cell_offset,
                       const float value) {
    hists[(grid_idx + cell_offset) * hist_stride + h_idx] += value;
  };

  for (std::size_t i = 0; i < projection.grid_x.size(); ++i)
  {
    float coord_x = projection.grid_x[i] - 0.5f;
    float coord_y = projection.grid_y[i] - 0.5f;
    float coord_z = projection.grid_z[i] - 0.5f;
    const float coord_h = projection.dbin[i] - 0.5f;

    const float bin_x = std::floor(coord_x);
    const float bin_y = std::floor(coord_y);
    const float bin_z = std::floor(coord_z);
    const float bin_h = std::floor(coord_h);

    const std::size_t grid_idx = static_cast<std::size_t>(
        ((static_cast<std::int32_t>(bin_x) + 1) * static_cast<std::int32_t>(grid_stride) +
         static_cast<std::int32_t>(bin_y) + 1) *
            static_cast<std::int32_t>(grid_stride) +
        static_cast<std::int32_t>(bin_z) + 1);
    const std::size_t h_idx = static_cast<std::size_t>(static_cast<std::int32_t>(bin_h) + 1);

    coord_x -= bin_x;
    coord_y -= bin_y;
    coord_z -= bin_z;

    const float v_x1 = hist_incr * coord_x;
    const float v_x0 = hist_incr - v_x1;
    const float v_xy11 = v_x1 * coord_y;
    const float v_xy10 = v_x1 - v_xy11;
    const float v_xy01 = v_x0 * coord_y;
    const float v_xy00 = v_x0 - v_xy01;

    const float w111 = v_xy11 * coord_z;
    const float w110 = v_xy11 - w111;
    const float w101 = v_xy10 * coord_z;
    const float w100 = v_xy10 - w101;
    const float w011 = v_xy01 * coord_z;
    const float w010 = v_xy01 - w011;
    const float w001 = v_xy00 * coord_z;
    const float w000 = v_xy00 - w001;

    add(grid_idx, h_idx, 0, w000);
    add(grid_idx, h_idx, 1, w001);
    add(grid_idx, h_idx, grid_stride, w010);
    add(grid_idx, h_idx, grid_stride + 1, w011);
    add(grid_idx, h_idx, grid_stride * grid_stride, w100);
    add(grid_idx, h_idx, grid_stride * grid_stride + 1, w101);
    add(grid_idx, h_idx, grid_stride * (grid_stride + 1), w110);
    add(grid_idx, h_idx, grid_stride * (grid_stride + 1) + 1, w111);
  }
}

inline void
accumulateTrilinearHistogramEigenStd(const ShapeProjectionBuffers& projection,
                                     const std::size_t half_grid_size,
                                     const std::size_t hists_size,
                                     const float hist_incr,
                                     HistogramGrid& hists)
{
  const std::size_t grid_size = half_grid_size * 2;
  const std::size_t grid_stride = grid_size + 2;
  resizeEigenHistogram(hists, half_grid_size, hists_size);

  const auto add = [&](const std::size_t grid_idx,
                       const Eigen::Index h_idx,
                       const std::size_t cell_offset,
                       const float value) {
    hists[grid_idx + cell_offset][h_idx] += value;
  };

  for (std::size_t i = 0; i < projection.grid_x.size(); ++i)
  {
    float coord_x = projection.grid_x[i] - 0.5f;
    float coord_y = projection.grid_y[i] - 0.5f;
    float coord_z = projection.grid_z[i] - 0.5f;
    const float coord_h = projection.dbin[i] - 0.5f;

    const float bin_x = std::floor(coord_x);
    const float bin_y = std::floor(coord_y);
    const float bin_z = std::floor(coord_z);
    const float bin_h = std::floor(coord_h);

    const std::size_t grid_idx = static_cast<std::size_t>(
        ((static_cast<std::int32_t>(bin_x) + 1) * static_cast<std::int32_t>(grid_stride) +
         static_cast<std::int32_t>(bin_y) + 1) *
            static_cast<std::int32_t>(grid_stride) +
        static_cast<std::int32_t>(bin_z) + 1);
    const Eigen::Index h_idx = static_cast<Eigen::Index>(static_cast<std::int32_t>(bin_h) + 1);

    coord_x -= bin_x;
    coord_y -= bin_y;
    coord_z -= bin_z;

    const float v_x1 = hist_incr * coord_x;
    const float v_x0 = hist_incr - v_x1;
    const float v_xy11 = v_x1 * coord_y;
    const float v_xy10 = v_x1 - v_xy11;
    const float v_xy01 = v_x0 * coord_y;
    const float v_xy00 = v_x0 - v_xy01;

    const float w111 = v_xy11 * coord_z;
    const float w110 = v_xy11 - w111;
    const float w101 = v_xy10 * coord_z;
    const float w100 = v_xy10 - w101;
    const float w011 = v_xy01 * coord_z;
    const float w010 = v_xy01 - w011;
    const float w001 = v_xy00 * coord_z;
    const float w000 = v_xy00 - w001;

    add(grid_idx, h_idx, 0, w000);
    add(grid_idx, h_idx, 1, w001);
    add(grid_idx, h_idx, grid_stride, w010);
    add(grid_idx, h_idx, grid_stride + 1, w011);
    add(grid_idx, h_idx, grid_stride * grid_stride, w100);
    add(grid_idx, h_idx, grid_stride * grid_stride + 1, w101);
    add(grid_idx, h_idx, grid_stride * (grid_stride + 1), w110);
    add(grid_idx, h_idx, grid_stride * (grid_stride + 1) + 1, w111);
  }
}

inline void
computeShapeDescriptorTrilinearStd(const ShapeCloudT& cloud,
                                   const float max_coord,
                                   const float distance_normalization_factor,
                                   const std::size_t half_grid_size,
                                   const std::size_t hists_size,
                                   std::vector<float>& output)
{
  const std::size_t grid_size = half_grid_size * 2;
  const std::size_t grid_stride = grid_size + 2;
  const float half_grid = static_cast<float>(half_grid_size);
  const float shape_grid_step = distance_normalization_factor / half_grid;
  const float coordinate_scale = half_grid / max_coord;
  const float hists_scale = static_cast<float>(hists_size);
  const float hist_incr = 100.0f / static_cast<float>(cloud.size() - 1);
  HistogramGrid hists;
  resizeEigenHistogram(hists, half_grid_size, hists_size);

  const auto add = [&](const std::size_t grid_idx,
                       const Eigen::Index h_idx,
                       const std::size_t cell_offset,
                       const float value) {
    hists[grid_idx + cell_offset][h_idx] += value;
  };

  for (const auto& sample : cloud)
  {
    float coord_x = sample.x * coordinate_scale + half_grid - 0.5f;
    float coord_y = sample.y * coordinate_scale + half_grid - 0.5f;
    float coord_z = sample.z * coordinate_scale + half_grid - 0.5f;
    const float distance = std::sqrt(sample.x * sample.x + sample.y * sample.y + sample.z * sample.z);
    float integral = 0.0f;
    const float dist_hist_val = std::modf(distance / shape_grid_step, &integral);
    const float coord_h = dist_hist_val * hists_scale - 0.5f;

    const float bin_x = std::floor(coord_x);
    const float bin_y = std::floor(coord_y);
    const float bin_z = std::floor(coord_z);
    const float bin_h = std::floor(coord_h);

    const std::size_t grid_idx = static_cast<std::size_t>(
        ((static_cast<std::int32_t>(bin_x) + 1) * static_cast<std::int32_t>(grid_stride) +
         static_cast<std::int32_t>(bin_y) + 1) *
            static_cast<std::int32_t>(grid_stride) +
        static_cast<std::int32_t>(bin_z) + 1);
    const Eigen::Index h_idx = static_cast<Eigen::Index>(static_cast<std::int32_t>(bin_h) + 1);

    coord_x -= bin_x;
    coord_y -= bin_y;
    coord_z -= bin_z;

    const float v_x1 = hist_incr * coord_x;
    const float v_x0 = hist_incr - v_x1;
    const float v_xy11 = v_x1 * coord_y;
    const float v_xy10 = v_x1 - v_xy11;
    const float v_xy01 = v_x0 * coord_y;
    const float v_xy00 = v_x0 - v_xy01;

    const float w111 = v_xy11 * coord_z;
    const float w110 = v_xy11 - w111;
    const float w101 = v_xy10 * coord_z;
    const float w100 = v_xy10 - w101;
    const float w011 = v_xy01 * coord_z;
    const float w010 = v_xy01 - w011;
    const float w001 = v_xy00 * coord_z;
    const float w000 = v_xy00 - w001;

    add(grid_idx, h_idx, 0, w000);
    add(grid_idx, h_idx, 1, w001);
    add(grid_idx, h_idx, grid_stride, w010);
    add(grid_idx, h_idx, grid_stride + 1, w011);
    add(grid_idx, h_idx, grid_stride * grid_stride, w100);
    add(grid_idx, h_idx, grid_stride * grid_stride + 1, w101);
    add(grid_idx, h_idx, grid_stride * (grid_stride + 1), w110);
    add(grid_idx, h_idx, grid_stride * (grid_stride + 1) + 1, w111);
  }

  output = copyShapeHistogramsStd(hists, half_grid_size, hists_size);
}

inline std::uint64_t
checksumShapeProjection(const ShapeProjectionBuffers& buffers)
{
  std::uint64_t seed = 1469598103934665603ull;
  const auto mix = [&](const std::vector<float>& values) {
    for (const float value : values)
    {
      const auto q = static_cast<std::int64_t>(value * 100.0f);
      seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }
  };
  mix(buffers.grid_x);
  mix(buffers.grid_y);
  mix(buffers.grid_z);
  mix(buffers.dbin);
  return seed;
}

inline std::uint64_t
checksumColorHue(const ColorHueBuffers& buffers)
{
  std::uint64_t seed = 1469598103934665603ull;
  const auto mix = [&](const std::vector<float>& values) {
    for (const float value : values)
    {
      const auto q = static_cast<std::int64_t>(value * 1000.0f);
      seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }
  };
  mix(buffers.hue);
  mix(buffers.hbin);
  return seed;
}

inline std::uint64_t
checksumTrilinearInterpolation(const TrilinearInterpolationBuffers& buffers)
{
  std::uint64_t seed = 1469598103934665603ull;
  const auto mix_u32 = [&](const std::vector<std::uint32_t>& values) {
    for (const auto value : values)
      seed ^= static_cast<std::uint64_t>(value) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  };
  const auto mix_float = [&](const std::vector<float>& values) {
    for (const float value : values)
    {
      const auto q = static_cast<std::int64_t>(value * 100000.0f);
      seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }
  };
  mix_u32(buffers.grid_idx);
  mix_u32(buffers.h_idx);
  mix_float(buffers.w000);
  mix_float(buffers.w001);
  mix_float(buffers.w010);
  mix_float(buffers.w011);
  mix_float(buffers.w100);
  mix_float(buffers.w101);
  mix_float(buffers.w110);
  mix_float(buffers.w111);
  return seed;
}

inline std::uint64_t
checksumFlatScaled(const std::vector<float>& values, const float scale)
{
  std::uint64_t seed = 1469598103934665603ull;
  for (const float value : values)
  {
    const auto q = static_cast<std::int64_t>(value * scale);
    seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  }
  return seed;
}

inline std::uint64_t
checksumFlat(const std::vector<float>& values)
{
  return checksumFlatScaled(values, 100000.0f);
}

inline std::uint64_t
checksumHistogramGrid(const HistogramGrid& hists)
{
  std::uint64_t seed = 1469598103934665603ull;
  for (const auto& hist : hists)
  {
    for (Eigen::Index i = 0; i < hist.size(); ++i)
    {
      const auto q = static_cast<std::int64_t>(hist[i] * 100000.0f);
      seed ^= static_cast<std::uint64_t>(q) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    }
  }
  return seed;
}
} // namespace pcl::features::rvv_test::gasd
