/*
 * 本文件做什么：
 * 这个 benchmark（性能测试）只测 integral image normal 的 map-preparation
 * diagnostic helper，不包含 production distance transform、normal solver 或输出写回。
 *
 * 证据边界：
 * QEMU 运行只作为 log-shape smoke（日志形状小型验证），不能作为真实性能结论。
 */

#include "integral_image_normal.h"

#include <pcl/features/integral_image2D.h>
#include <pcl/features/integral_image_normal.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Geometry>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace iin = pcl::features::rvv_test::integral_image_normal;

namespace
{
using PclIntegralImage3 = pcl::IntegralImage2D<float, 3>;

struct XYZPadPoint
{
  float x;
  float y;
  float z;
  float pad;
};

struct ProfileStats
{
  double diff_ms = 0.0;
  double integral_ms = 0.0;
  double query_ms = 0.0;
  double total_ms = 0.0;
  double diff_checksum = 0.0;
  double integral_checksum = 0.0;
  double query_checksum = 0.0;
  double total_checksum = 0.0;
};

struct Integral3Profile
{
  std::size_t width = 0;
  std::size_t height = 0;
  std::vector<double> sx;
  std::vector<double> sy;
  std::vector<double> sz;
  std::vector<unsigned> count;

  void reset(std::size_t input_width, std::size_t input_height)
  {
    width = input_width;
    height = input_height;
    const std::size_t cells = (width + 1) * (height + 1);
    sx.assign(cells, 0.0);
    sy.assign(cells, 0.0);
    sz.assign(cells, 0.0);
    count.assign(cells, 0);
  }

  void build(const float* data)
  {
    if (sx.empty())
      reset(width, height);

    for (std::size_t row = 0; row < height; ++row)
    {
      double row_x = 0.0;
      double row_y = 0.0;
      double row_z = 0.0;
      unsigned row_count = 0;
      for (std::size_t col = 0; col < width; ++col)
      {
        const std::size_t data_index = (row * width + col) * 4;
        const float vx = data[data_index + 0];
        const float vy = data[data_index + 1];
        const float vz = data[data_index + 2];
        if (std::isfinite(vx) && std::isfinite(vy) && std::isfinite(vz))
        {
          row_x += vx;
          row_y += vy;
          row_z += vz;
          ++row_count;
        }

        const std::size_t out = (row + 1) * (width + 1) + (col + 1);
        const std::size_t above = row * (width + 1) + (col + 1);
        sx[out] = sx[above] + row_x;
        sy[out] = sy[above] + row_y;
        sz[out] = sz[above] + row_z;
        count[out] = count[above] + row_count;
      }
    }
  }

  void sumRect(std::size_t x,
               std::size_t y,
               std::size_t rect_width,
               std::size_t rect_height,
               double& out_x,
               double& out_y,
               double& out_z,
               unsigned& out_count) const
  {
    const std::size_t x1 = std::min(width, x + rect_width);
    const std::size_t y1 = std::min(height, y + rect_height);
    const std::size_t a = y * (width + 1) + x;
    const std::size_t b = y * (width + 1) + x1;
    const std::size_t c = y1 * (width + 1) + x;
    const std::size_t d = y1 * (width + 1) + x1;
    out_x = sx[d] - sx[b] - sx[c] + sx[a];
    out_y = sy[d] - sy[b] - sy[c] + sy[a];
    out_z = sz[d] - sz[b] - sz[c] + sz[a];
    out_count = count[d] - count[b] - count[c] + count[a];
  }

  double sampledChecksum() const
  {
    double checksum = 0.0;
    const std::size_t stride = std::max<std::size_t>(1, sx.size() / 257);
    for (std::size_t i = 0; i < sx.size(); i += stride)
      checksum += (sx[i] + 0.25 * sy[i] + 0.125 * sz[i] + count[i]) * static_cast<double>((i % 7) + 1);
    return checksum;
  }
};

std::vector<float>
makeDepths(std::size_t width, std::size_t height)
{
  std::vector<float> z(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      float value = 1.0f + 0.0007f * static_cast<float>(x) + 0.0013f * static_cast<float>(y);
      if ((x % 97) == 0 && (y % 11) == 0)
        value += 0.75f;
      if ((x % 211) == 7 && (y % 17) == 3)
        value = std::numeric_limits<float>::quiet_NaN();
      z[y * width + x] = value;
    }
  }
  return z;
}

double
runMapPrepCase(const std::string& label, std::size_t width, std::size_t height, int iterations)
{
  constexpr float max_depth_change_factor = 20.0f * 0.001f;
  std::vector<float> z = makeDepths(width, height);
  std::vector<unsigned char> map(z.size());
  std::vector<float> distance(z.size());
  const float far_distance = static_cast<float>(width + height);

  iin::buildDepthChangeMapRVV(z.data(), width, height, max_depth_change_factor, map.data());
  iin::initializeDistanceMapRVV(map.data(), map.size(), far_distance, distance.data());

  const auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
  {
    iin::buildDepthChangeMapRVV(z.data(), width, height, max_depth_change_factor, map.data());
    iin::initializeDistanceMapRVV(map.data(), map.size(), far_distance, distance.data());
  }
  const auto stop = std::chrono::steady_clock::now();

  double checksum = 0.0;
  for (std::size_t i = 0; i < distance.size(); i += 17)
    checksum += distance[i] * static_cast<double>((i % 13) + 1);

  const double ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  std::cout << label << ": " << ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  return checksum;
}

std::vector<XYZPadPoint>
makeXYZPadCloud(std::size_t width, std::size_t height)
{
  std::vector<XYZPadPoint> points(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);
      float z = 1.0f + 0.0011f * fx * fx + 0.0037f * fy;
      if ((x % 173) == 5 && (y % 19) == 11)
        z += 0.5f;
      points[y * width + x] = {
          0.03125f * fx + 0.0625f * fy,
          0.25f + 0.015625f * fx - 0.0078125f * fy,
          z,
          1234.0f};
    }
  }
  return points;
}

double
runAverage3DGradientDiffCase(const std::string& label, std::size_t width, std::size_t height, int iterations)
{
  std::vector<XYZPadPoint> points = makeXYZPadCloud(width, height);
  std::vector<float> diff_x(width * height * 4);
  std::vector<float> diff_y(width * height * 4);

  iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());

  const auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
    iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());
  const auto stop = std::chrono::steady_clock::now();

  double checksum = 0.0;
  for (std::size_t i = 0; i < diff_x.size(); i += 19)
    checksum += (diff_x[i] + 0.5 * diff_y[i]) * static_cast<double>((i % 11) + 1);

  const double ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  std::cout << label << ": " << ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  return checksum;
}

double
sampleDiffChecksum(const std::vector<float>& diff_x, const std::vector<float>& diff_y)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < diff_x.size(); i += 19)
    checksum += (diff_x[i] + 0.5 * diff_y[i]) * static_cast<double>((i % 11) + 1);
  return checksum;
}

double
queryAverage3DGradientNormals(const Integral3Profile& dx,
                              const Integral3Profile& dy,
                              std::size_t rect_width,
                              std::size_t rect_height)
{
  const std::size_t half_w = rect_width / 2;
  const std::size_t half_h = rect_height / 2;
  double checksum = 0.0;
  if (dx.width <= rect_width || dx.height <= rect_height)
    return checksum;

  for (std::size_t row = half_h; row + half_h < dx.height; ++row)
  {
    for (std::size_t col = half_w; col + half_w < dx.width; ++col)
    {
      double gx0 = 0.0;
      double gx1 = 0.0;
      double gx2 = 0.0;
      double gy0 = 0.0;
      double gy1 = 0.0;
      double gy2 = 0.0;
      unsigned count_x = 0;
      unsigned count_y = 0;
      dx.sumRect(col - half_w, row - half_h, rect_width, rect_height, gx0, gx1, gx2, count_x);
      dy.sumRect(col - half_w, row - half_h, rect_width, rect_height, gy0, gy1, gy2, count_y);
      if (count_x == 0 || count_y == 0)
        continue;

      const double nx = gy1 * gx2 - gy2 * gx1;
      const double ny = gy2 * gx0 - gy0 * gx2;
      const double nz = gy0 * gx1 - gy1 * gx0;
      const double normal_length = nx * nx + ny * ny + nz * nz;
      if (normal_length == 0.0)
        continue;
      const double scale = 1.0 / std::sqrt(normal_length);
      checksum += (nx * scale + 2.0 * ny * scale + 3.0 * nz * scale) *
                  static_cast<double>(((row * dx.width + col) % 17) + 1);
    }
  }
  return checksum;
}

double
samplePclIntegralChecksum(const PclIntegralImage3& dx,
                          const PclIntegralImage3& dy,
                          std::size_t width,
                          std::size_t height)
{
  double checksum = 0.0;
  const std::size_t row_step = std::max<std::size_t>(1, height / 17);
  const std::size_t col_step = std::max<std::size_t>(1, width / 19);
  for (std::size_t row = 0; row < height; row += row_step)
  {
    for (std::size_t col = 0; col < width; col += col_step)
    {
      const unsigned rect_w = static_cast<unsigned>(std::min<std::size_t>(5, width - col));
      const unsigned rect_h = static_cast<unsigned>(std::min<std::size_t>(5, height - row));
      const auto sum_x = dx.getFirstOrderSum(static_cast<unsigned>(col), static_cast<unsigned>(row), rect_w, rect_h);
      const auto sum_y = dy.getFirstOrderSum(static_cast<unsigned>(col), static_cast<unsigned>(row), rect_w, rect_h);
      const unsigned count_x =
          dx.getFiniteElementsCount(static_cast<unsigned>(col), static_cast<unsigned>(row), rect_w, rect_h);
      const unsigned count_y =
          dy.getFiniteElementsCount(static_cast<unsigned>(col), static_cast<unsigned>(row), rect_w, rect_h);
      const double weight = static_cast<double>(((row * width + col) % 23) + 1);
      checksum += (sum_x[0] + 0.25 * sum_x[1] + 0.125 * sum_x[2] + 0.5 * sum_y[0] +
                   0.0625 * sum_y[1] + static_cast<double>(count_x + count_y)) *
                  weight;
    }
  }
  return checksum;
}

double
queryPclAverage3DGradientNormals(const PclIntegralImage3& dx,
                                 const PclIntegralImage3& dy,
                                 std::size_t width,
                                 std::size_t height,
                                 std::size_t rect_width,
                                 std::size_t rect_height)
{
  const std::size_t half_w = rect_width / 2;
  const std::size_t half_h = rect_height / 2;
  double checksum = 0.0;
  if (width <= rect_width || height <= rect_height)
    return checksum;

  for (std::size_t row = half_h; row + half_h < height; ++row)
  {
    for (std::size_t col = half_w; col + half_w < width; ++col)
    {
      const unsigned x = static_cast<unsigned>(col - half_w);
      const unsigned y = static_cast<unsigned>(row - half_h);
      const unsigned rw = static_cast<unsigned>(rect_width);
      const unsigned rh = static_cast<unsigned>(rect_height);
      const unsigned count_x = dx.getFiniteElementsCount(x, y, rw, rh);
      const unsigned count_y = dy.getFiniteElementsCount(x, y, rw, rh);
      if (count_x == 0 || count_y == 0)
        continue;

      const Eigen::Vector3d gradient_x = dx.getFirstOrderSum(x, y, rw, rh);
      const Eigen::Vector3d gradient_y = dy.getFirstOrderSum(x, y, rw, rh);
      Eigen::Vector3d normal_vector = gradient_y.cross(gradient_x);
      const double normal_length = normal_vector.squaredNorm();
      if (normal_length == 0.0)
        continue;

      normal_vector /= std::sqrt(normal_length);
      checksum += (normal_vector[0] + 2.0 * normal_vector[1] + 3.0 * normal_vector[2]) *
                  static_cast<double>(((row * width + col) % 17) + 1);
    }
  }
  return checksum;
}

ProfileStats
runAverage3DGradientProfileOnce(std::vector<XYZPadPoint>& points,
                                std::vector<float>& diff_x,
                                std::vector<float>& diff_y,
                                Integral3Profile& integral_dx,
                                Integral3Profile& integral_dy,
                                std::size_t width,
                                std::size_t height,
                                int iterations)
{
  ProfileStats stats;
  const std::size_t output_count = width * height * 4;
  diff_x.resize(output_count);
  diff_y.resize(output_count);
  integral_dx.reset(width, height);
  integral_dy.reset(width, height);

  iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());
  integral_dx.build(diff_x.data());
  integral_dy.build(diff_y.data());
  (void)queryAverage3DGradientNormals(integral_dx, integral_dy, 5, 5);

  auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
    iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());
  auto stop = std::chrono::steady_clock::now();
  stats.diff_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  stats.diff_checksum = sampleDiffChecksum(diff_x, diff_y);

  start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
  {
    integral_dx.build(diff_x.data());
    integral_dy.build(diff_y.data());
  }
  stop = std::chrono::steady_clock::now();
  stats.integral_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  stats.integral_checksum = integral_dx.sampledChecksum() + 0.5 * integral_dy.sampledChecksum();

  start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
    stats.query_checksum += queryAverage3DGradientNormals(integral_dx, integral_dy, 5, 5);
  stop = std::chrono::steady_clock::now();
  stats.query_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);

  start = std::chrono::steady_clock::now();
  stats.total_checksum = 0.0;
  for (int iter = 0; iter < iterations; ++iter)
  {
    iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());
    integral_dx.build(diff_x.data());
    integral_dy.build(diff_y.data());
    stats.total_checksum += queryAverage3DGradientNormals(integral_dx, integral_dy, 5, 5);
  }
  stop = std::chrono::steady_clock::now();
  stats.total_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  return stats;
}

ProfileStats
runAverage3DGradientPclProfileOnce(std::vector<XYZPadPoint>& points,
                                   std::vector<float>& diff_x,
                                   std::vector<float>& diff_y,
                                   PclIntegralImage3& integral_dx,
                                   PclIntegralImage3& integral_dy,
                                   std::size_t width,
                                   std::size_t height,
                                   int iterations)
{
  ProfileStats stats;
  const std::size_t output_count = width * height * 4;
  diff_x.resize(output_count);
  diff_y.resize(output_count);

  iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());
  integral_dx.setInput(diff_x.data(), static_cast<unsigned>(width), static_cast<unsigned>(height), 4, static_cast<unsigned>(width << 2));
  integral_dy.setInput(diff_y.data(), static_cast<unsigned>(width), static_cast<unsigned>(height), 4, static_cast<unsigned>(width << 2));
  (void)queryPclAverage3DGradientNormals(integral_dx, integral_dy, width, height, 5, 5);

  auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
    iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());
  auto stop = std::chrono::steady_clock::now();
  stats.diff_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  stats.diff_checksum = sampleDiffChecksum(diff_x, diff_y);

  start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
  {
    integral_dx.setInput(diff_x.data(), static_cast<unsigned>(width), static_cast<unsigned>(height), 4, static_cast<unsigned>(width << 2));
    integral_dy.setInput(diff_y.data(), static_cast<unsigned>(width), static_cast<unsigned>(height), 4, static_cast<unsigned>(width << 2));
  }
  stop = std::chrono::steady_clock::now();
  stats.integral_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  stats.integral_checksum = samplePclIntegralChecksum(integral_dx, integral_dy, width, height);

  start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
    stats.query_checksum += queryPclAverage3DGradientNormals(integral_dx, integral_dy, width, height, 5, 5);
  stop = std::chrono::steady_clock::now();
  stats.query_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);

  start = std::chrono::steady_clock::now();
  stats.total_checksum = 0.0;
  for (int iter = 0; iter < iterations; ++iter)
  {
    iin::buildAverage3DGradientDiffBuffersRVV(points.data(), width, height, diff_x.data(), diff_y.data());
    integral_dx.setInput(diff_x.data(), static_cast<unsigned>(width), static_cast<unsigned>(height), 4, static_cast<unsigned>(width << 2));
    integral_dy.setInput(diff_y.data(), static_cast<unsigned>(width), static_cast<unsigned>(height), 4, static_cast<unsigned>(width << 2));
    stats.total_checksum += queryPclAverage3DGradientNormals(integral_dx, integral_dy, width, height, 5, 5);
  }
  stop = std::chrono::steady_clock::now();
  stats.total_ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  return stats;
}

double
runAverage3DGradientProfileCase(const std::string& label, std::size_t width, std::size_t height, int iterations)
{
  std::vector<XYZPadPoint> points = makeXYZPadCloud(width, height);
  std::vector<float> diff_x;
  std::vector<float> diff_y;
  Integral3Profile integral_dx;
  Integral3Profile integral_dy;
  const ProfileStats stats =
      runAverage3DGradientProfileOnce(points, diff_x, diff_y, integral_dx, integral_dy, width, height, iterations);

  const std::string suffix = label.substr(std::string("avg3d_profile_").size());
  std::cout << "profile_component_diff_" << suffix << ": " << stats.diff_ms << " ms / iter\n";
  std::cout << "checksum_profile_component_diff_" << suffix << "=" << stats.diff_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  std::cout << "profile_component_integral_" << suffix << ": " << stats.integral_ms << " ms / iter\n";
  std::cout << "checksum_profile_component_integral_" << suffix << "=" << stats.integral_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  std::cout << "profile_component_query_" << suffix << ": " << stats.query_ms << " ms / iter\n";
  std::cout << "checksum_profile_component_query_" << suffix << "=" << stats.query_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  std::cout << label << ": " << stats.total_ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << stats.total_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  return stats.total_checksum + stats.diff_checksum + stats.integral_checksum + stats.query_checksum;
}

double
runAverage3DGradientPclProfileCase(const std::string& label, std::size_t width, std::size_t height, int iterations)
{
  std::vector<XYZPadPoint> points = makeXYZPadCloud(width, height);
  std::vector<float> diff_x;
  std::vector<float> diff_y;
  PclIntegralImage3 integral_dx(false);
  PclIntegralImage3 integral_dy(false);
  const ProfileStats stats =
      runAverage3DGradientPclProfileOnce(points, diff_x, diff_y, integral_dx, integral_dy, width, height, iterations);

  const std::string suffix = label.substr(std::string("pcl_avg3d_profile_").size());
  std::cout << "pcl_iin_diff_" << suffix << ": " << stats.diff_ms << " ms / iter\n";
  std::cout << "checksum_pcl_iin_diff_" << suffix << "=" << stats.diff_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  std::cout << "pcl_iin_setinput_dxdy_" << suffix << ": " << stats.integral_ms << " ms / iter\n";
  std::cout << "checksum_pcl_iin_setinput_dxdy_" << suffix << "=" << stats.integral_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  std::cout << "pcl_iin_query_" << suffix << ": " << stats.query_ms << " ms / iter\n";
  std::cout << "checksum_pcl_iin_query_" << suffix << "=" << stats.query_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  std::cout << label << ": " << stats.total_ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << stats.total_checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  return stats.total_checksum + stats.diff_checksum + stats.integral_checksum + stats.query_checksum;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeProductionCloud(std::size_t width, std::size_t height)
{
  auto cloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>);
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = false;
  cloud->points.resize(width * height);
  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      float z = 1.0f + 0.0007f * static_cast<float>(x) + 0.0013f * static_cast<float>(y);
      if ((x % 97) == 0 && (y % 11) == 0)
        z += 0.75f;
      if ((x % 211) == 7 && (y % 17) == 3)
        z = std::numeric_limits<float>::quiet_NaN();
      cloud->points[y * width + x] = {
          static_cast<float>(x) * 0.01f,
          static_cast<float>(y) * 0.02f,
          z};
    }
  }
  return cloud;
}

double
checksumProductionResult(const pcl::PointCloud<pcl::Normal>& output, const float* distance_map)
{
  auto finiteOrSentinel = [](float value, double sentinel) {
    return std::isfinite(value) ? static_cast<double>(value) : sentinel;
  };
  double checksum = 0.0;
  const std::size_t normal_stride = std::max<std::size_t>(1, output.size() / 257);
  for (std::size_t i = 0; i < output.size(); i += normal_stride)
  {
    const pcl::Normal& normal = output.points[i];
    checksum += (finiteOrSentinel(normal.normal_x, -0.25) +
                 2.0 * finiteOrSentinel(normal.normal_y, -0.5) +
                 3.0 * finiteOrSentinel(normal.normal_z, -0.75) +
                 finiteOrSentinel(normal.curvature, -1.0)) *
                static_cast<double>((i % 13) + 1);
  }
  if (distance_map != nullptr)
  {
    const std::size_t distance_stride = std::max<std::size_t>(1, output.size() / 263);
    for (std::size_t i = 0; i < output.size(); i += distance_stride)
      checksum += static_cast<double>(distance_map[i]) * static_cast<double>((i % 17) + 1);
  }
  return checksum;
}

double
runProductionComputeCase(const std::string& label, std::size_t width, std::size_t height, int iterations)
{
  auto cloud = makeProductionCloud(width, height);
  pcl::IntegralImageNormalEstimation<pcl::PointXYZ, pcl::Normal> estimator;
  estimator.setNormalEstimationMethod(estimator.AVERAGE_DEPTH_CHANGE);
  estimator.setBorderPolicy(estimator.BORDER_POLICY_IGNORE);
  estimator.setNormalSmoothingSize(10.0f);
  estimator.setInputCloud(cloud);

  pcl::PointCloud<pcl::Normal> output;
  estimator.compute(output);

  const auto start = std::chrono::steady_clock::now();
  for (int iter = 0; iter < iterations; ++iter)
    estimator.compute(output);
  const auto stop = std::chrono::steady_clock::now();

  const double checksum = checksumProductionResult(output, estimator.getDistanceMap());
  const double ms =
      std::chrono::duration<double, std::milli>(stop - start).count() / static_cast<double>(iterations);
  std::cout << label << ": " << ms << " ms / iter\n";
  std::cout << "checksum_" << label << "=" << checksum
            << " width=" << width
            << " height=" << height
            << "\n";
  return checksum;
}
} // namespace

int
main(int argc, char** argv)
{
  int iterations = 200;
  if (argc > 1)
    iterations = std::max(1, std::stoi(argv[1]));

#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "bench_role=diagnostic boundary=test_helper row_source=ordered-organized-image timer_boundary=mixed-by-case\n";
  std::cout << "Dataset: integral_image_normal_synthetic_diagnostics\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: 1\n";

  double checksum = 0.0;
  checksum += runMapPrepCase("map_prep_320x240", 320, 240, iterations);
  checksum += runMapPrepCase("map_prep_641x481_tail", 641, 481, iterations / 4 + 1);
  checksum += runAverage3DGradientDiffCase("avg3d_diff_320x240", 320, 240, iterations);
  checksum += runAverage3DGradientDiffCase("avg3d_diff_641x481_tail", 641, 481, iterations / 4 + 1);
  checksum += runAverage3DGradientProfileCase("avg3d_profile_320x240", 320, 240, iterations / 4 + 1);
  checksum += runAverage3DGradientProfileCase("avg3d_profile_641x481_tail", 641, 481, iterations / 20 + 1);
  checksum += runAverage3DGradientPclProfileCase("pcl_avg3d_profile_320x240", 320, 240, iterations / 4 + 1);
  checksum += runAverage3DGradientPclProfileCase("pcl_avg3d_profile_641x481_tail", 641, 481, iterations / 20 + 1);
  checksum += runProductionComputeCase("prod_compute_avg_depth_320x240", 320, 240, iterations / 20 + 1);
  checksum += runProductionComputeCase("prod_compute_avg_depth_641x481_tail", 641, 481, iterations / 80 + 1);
  std::cout << "total_checksum=" << checksum << "\n";
  return 0;
}
