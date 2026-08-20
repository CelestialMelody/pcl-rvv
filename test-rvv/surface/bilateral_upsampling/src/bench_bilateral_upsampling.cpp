#include "bilateral_upsampling.h"

#include <pcl/point_types.h>
#include <pcl/surface/bilateral_upsampling.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>
#include <unistd.h>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace diag = pcl_rvv_surface_bilateral_upsampling;

namespace {


// 仅用于 phase 030 消融：临时静默 process() 里的既有矩阵打印，避免 stdout I/O
// 掩盖 steady-state public entry 的真实耗时。
class ScopedStdoutSilence {
public:
  ScopedStdoutSilence()
  {
    std::fflush(stdout);
    saved_stdout_ = dup(fileno(stdout));
    null_stdout_ = std::fopen("/dev/null", "w");
    if (saved_stdout_ >= 0 && null_stdout_ != nullptr) {
      dup2(fileno(null_stdout_), fileno(stdout));
      active_ = true;
    }
  }

  ~ScopedStdoutSilence()
  {
    std::fflush(stdout);
    if (active_)
      dup2(saved_stdout_, fileno(stdout));
    if (saved_stdout_ >= 0)
      close(saved_stdout_);
    if (null_stdout_ != nullptr)
      std::fclose(null_stdout_);
  }

private:
  int saved_stdout_{-1};
  FILE* null_stdout_{nullptr};
  bool active_{false};
};

Eigen::Matrix3f
makeProductionProjection()
{
  Eigen::Matrix3f projection;
  projection << 500.0f, 0.0f, 320.0f,
                0.0f, 500.0f, 240.0f,
                0.0f, 0.0f, 1.0f;
  return projection;
}

Eigen::Matrix3f
makeProductionUnprojection()
{
  return makeProductionProjection().inverse();
}

Eigen::MatrixXf
makeProductionDepthMatrix(int window)
{
  Eigen::MatrixXf depth(2 * window + 1, 2 * window + 1);
  for (int dx = -window; dx <= window; ++dx) {
    for (int dy = -window; dy <= window; ++dy) {
      const float value = std::exp(-static_cast<float>(dx * dx + dy * dy) /
                                   (2.0f * 0.5f * 0.5f));
      depth(dx + window, dy + window) = value;
    }
  }
  return depth;
}

Eigen::VectorXf
makeProductionRgbVector()
{
  Eigen::VectorXf rgb(3 * 255 + 1);
  for (int d_color = 0; d_color <= 3 * 255; ++d_color) {
    const float value =
        std::exp(-static_cast<float>(d_color * d_color) / (2.0f * 15.0f * 15.0f));
    rgb(d_color) = value;
  }
  return rgb;
}

template <typename PointT>
pcl::PointCloud<PointT>
makePclCloud(const std::vector<diag::RgbPoint>& src, int width, int height)
{
  pcl::PointCloud<PointT> cloud;
  cloud.width = static_cast<std::uint32_t>(width);
  cloud.height = static_cast<std::uint32_t>(height);
  cloud.is_dense = false;
  cloud.resize(src.size());
  for (std::size_t i = 0; i < src.size(); ++i) {
    cloud[i].x = src[i].x;
    cloud[i].y = src[i].y;
    cloud[i].z = src[i].z;
    cloud[i].r = src[i].r;
    cloud[i].g = src[i].g;
    cloud[i].b = src[i].b;
    if constexpr (std::is_same_v<PointT, pcl::PointXYZRGBA>)
      cloud[i].a = src[i].a;
  }
  return cloud;
}

template <typename PointT>
std::vector<diag::RgbPoint>
fromPclCloud(const pcl::PointCloud<PointT>& cloud)
{
  std::vector<diag::RgbPoint> out(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    out[i].x = cloud[i].x;
    out[i].y = cloud[i].y;
    out[i].z = cloud[i].z;
    out[i].r = cloud[i].r;
    out[i].g = cloud[i].g;
    out[i].b = cloud[i].b;
  }
  return out;
}

#if defined(__RVV10__)
inline float
reduceSumF32M2(vfloat32m2_t values, const std::size_t vl)
{
  const vfloat32m1_t zero = __riscv_vfmv_v_f_f32m1(0.0f, 1);
  const vfloat32m1_t sum = __riscv_vfredusum_vs_f32m2_f32m1(values, zero, vl);
  return __riscv_vfmv_f_s_f32m1_f32(sum);
}

// Phase 050 的测试专用变体：保持生产点型和 helper-only 计时边界，但只用
// z == z 过滤 NaN。它不覆盖 infinity 语义，不能直接作为 production patch。
template <typename PointT>
bool
performProcessingNanMaskK64RVV(const pcl::PointCloud<PointT>& input,
                               pcl::PointCloud<PointT>& output,
                               const int window_size,
                               const Eigen::MatrixXf& val_exp_depth_matrix,
                               const Eigen::VectorXf& val_exp_rgb_vector,
                               const Eigen::Matrix3f& unprojection_matrix)
{
  if (window_size <= 0 || window_size > 32 || input.empty() || input.width == 0 ||
      input.height == 0)
    return false;

  output.resize(input.size());
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const int width = static_cast<int>(input.width);
  const int height = static_cast<int>(input.height);
  const auto* base = input.points.data();
  const ptrdiff_t point_stride_bytes =
      static_cast<ptrdiff_t>(input.width) * static_cast<ptrdiff_t>(sizeof(PointT));

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      const int center = y * width + x;
      const int start_window_x = std::max(x - window_size, 0);
      const int start_window_y = std::max(y - window_size, 0);
      const int end_window_x = std::min(x + window_size, width);
      const int end_window_y = std::min(y + window_size, height);

      float sum = 0.0f;
      float norm_sum = 0.0f;

      for (int x_w = start_window_x; x_w < end_window_x; ++x_w) {
        int y_w = start_window_y;
        while (y_w < end_window_y) {
          constexpr std::size_t kMaxChunkLanes = 64;
          const std::size_t remaining =
              std::min<std::size_t>(static_cast<std::size_t>(end_window_y - y_w),
                                    kMaxChunkLanes);
          const std::size_t vl = __riscv_vsetvl_e32m2(remaining);
          alignas(64) float weights[kMaxChunkLanes];
          for (std::size_t lane = 0; lane < vl; ++lane) {
            const int yy = y_w + static_cast<int>(lane);
            const int id = yy * width + x_w;
            const float val_exp_depth = val_exp_depth_matrix(
                static_cast<Eigen::MatrixXf::Index>(x - x_w + window_size),
                static_cast<Eigen::MatrixXf::Index>(y - yy + window_size));
            const auto d_color = static_cast<Eigen::VectorXf::Index>(
                std::abs(input[id].r - input[center].r) +
                std::abs(input[id].g - input[center].g) +
                std::abs(input[id].b - input[center].b));
            weights[lane] = val_exp_depth * val_exp_rgb_vector(d_color);
          }

          const auto* z_ptr = &base[y_w * width + x_w].z;
          const vfloat32m2_t z = __riscv_vlse32_v_f32m2(z_ptr, point_stride_bytes, vl);
          vfloat32m2_t w = __riscv_vle32_v_f32m2(weights, vl);
          const vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(z, z, vl);
          const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
          w = __riscv_vmerge_vvm_f32m2(zero, w, finite, vl);
          const vfloat32m2_t z_safe = __riscv_vmerge_vvm_f32m2(zero, z, finite, vl);
          sum += reduceSumF32M2(__riscv_vfmul_vv_f32m2(w, z_safe, vl), vl);
          norm_sum += reduceSumF32M2(w, vl);
          y_w += static_cast<int>(vl);
        }
      }

      output[center].r = input[center].r;
      output[center].g = input[center].g;
      output[center].b = input[center].b;

      if (norm_sum != 0.0f) {
        const float depth = sum / norm_sum;
        const Eigen::Vector3f pc(static_cast<float>(x) * depth,
                                 static_cast<float>(y) * depth,
                                 depth);
        const Eigen::Vector3f pw(unprojection_matrix * pc);
        output[center].x = pw[0];
        output[center].y = pw[1];
        output[center].z = pw[2];
      }
      else {
        output[center].x = nan;
        output[center].y = nan;
        output[center].z = nan;
      }
    }
  }

  output.header = input.header;
  output.width = input.width;
  output.height = input.height;
  return true;
}

// Phase 060 的历史 helper-only 变体：当年用于和 public shell 做 A/B。
// 现在它保留为兼容入口，实际直接走当前 color-gather helper，避免历史
// bench 标签失效；它仍然只是 bench-local helper，不代表 production API。
template <typename PointT>
bool
performProcessingColorGatherRVV(const pcl::PointCloud<PointT>& input,
                                pcl::PointCloud<PointT>& output,
                                const int window_size,
                                const Eigen::MatrixXf& val_exp_depth_matrix,
                                const Eigen::VectorXf& val_exp_rgb_vector,
                                const Eigen::Matrix3f& unprojection_matrix)
{
  if (window_size <= 0 || window_size > 32 || input.empty() || input.width == 0 ||
      input.height == 0)
    return false;

  output.resize(input.size());
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const int width = static_cast<int>(input.width);
  const int height = static_cast<int>(input.height);
  const auto* base = input.points.data();
  const ptrdiff_t point_stride_bytes =
      static_cast<ptrdiff_t>(input.width) * static_cast<ptrdiff_t>(sizeof(PointT));
  const ptrdiff_t depth_col_stride_bytes =
      -static_cast<ptrdiff_t>(val_exp_depth_matrix.outerStride()) *
      static_cast<ptrdiff_t>(sizeof(float));
  const float* rgb_base = val_exp_rgb_vector.data();

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      const int center = y * width + x;
      const auto& center_point = input[center];
      const std::uint8_t center_r = center_point.r;
      const std::uint8_t center_g = center_point.g;
      const std::uint8_t center_b = center_point.b;
      const int start_window_x = std::max(x - window_size, 0);
      const int start_window_y = std::max(y - window_size, 0);
      const int end_window_x = std::min(x + window_size, width);
      const int end_window_y = std::min(y + window_size, height);

      float sum = 0.0f;
      float norm_sum = 0.0f;

      for (int x_w = start_window_x; x_w < end_window_x; ++x_w) {
        int y_w = start_window_y;
        while (y_w < end_window_y) {
          constexpr std::size_t kMaxChunkLanes = 64;
          const std::size_t remaining =
              std::min<std::size_t>(static_cast<std::size_t>(end_window_y - y_w),
                                    kMaxChunkLanes);
          const std::size_t vl = __riscv_vsetvl_e32m2(remaining);

          const auto* depth_ptr = &val_exp_depth_matrix(
              static_cast<Eigen::MatrixXf::Index>(x - x_w + window_size),
              static_cast<Eigen::MatrixXf::Index>(y - y_w + window_size));
          const vfloat32m2_t depth_w =
              __riscv_vlse32_v_f32m2(depth_ptr, depth_col_stride_bytes, vl);

          const auto* r_ptr = &base[y_w * width + x_w].r;
          const auto* g_ptr = &base[y_w * width + x_w].g;
          const auto* b_ptr = &base[y_w * width + x_w].b;
          const vuint8mf2_t r8 = __riscv_vlse8_v_u8mf2(r_ptr, point_stride_bytes, vl);
          const vuint8mf2_t g8 = __riscv_vlse8_v_u8mf2(g_ptr, point_stride_bytes, vl);
          const vuint8mf2_t b8 = __riscv_vlse8_v_u8mf2(b_ptr, point_stride_bytes, vl);
          const vuint16m1_t r16 = __riscv_vzext_vf2_u16m1(r8, vl);
          const vuint16m1_t g16 = __riscv_vzext_vf2_u16m1(g8, vl);
          const vuint16m1_t b16 = __riscv_vzext_vf2_u16m1(b8, vl);
          const vuint16m1_t center_r_vec = __riscv_vmv_v_x_u16m1(center_r, vl);
          const vuint16m1_t center_g_vec = __riscv_vmv_v_x_u16m1(center_g, vl);
          const vuint16m1_t center_b_vec = __riscv_vmv_v_x_u16m1(center_b, vl);
          const vuint16m1_t dr =
              __riscv_vsub_vv_u16m1(__riscv_vmaxu_vv_u16m1(r16, center_r_vec, vl),
                                    __riscv_vminu_vv_u16m1(r16, center_r_vec, vl),
                                    vl);
          const vuint16m1_t dg =
              __riscv_vsub_vv_u16m1(__riscv_vmaxu_vv_u16m1(g16, center_g_vec, vl),
                                    __riscv_vminu_vv_u16m1(g16, center_g_vec, vl),
                                    vl);
          const vuint16m1_t db =
              __riscv_vsub_vv_u16m1(__riscv_vmaxu_vv_u16m1(b16, center_b_vec, vl),
                                    __riscv_vminu_vv_u16m1(b16, center_b_vec, vl),
                                    vl);
          const vuint16m1_t d_color = __riscv_vadd_vv_u16m1(__riscv_vadd_vv_u16m1(dr, dg, vl), db, vl);
          const vuint16m1_t color_offsets = __riscv_vsll_vx_u16m1(d_color, 2, vl);
          const vfloat32m2_t rgb_w = __riscv_vluxei16_v_f32m2(rgb_base, color_offsets, vl);
          const vfloat32m2_t w = __riscv_vfmul_vv_f32m2(depth_w, rgb_w, vl);

          const auto* z_ptr = &base[y_w * width + x_w].z;
          const vfloat32m2_t z = __riscv_vlse32_v_f32m2(z_ptr, point_stride_bytes, vl);
          vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(z, z, vl);
          finite = __riscv_vmand_mm_b16(
              finite,
              __riscv_vmflt_vf_f32m2_b16(
                  __riscv_vfabs_v_f32m2(z, vl), std::numeric_limits<float>::infinity(), vl),
              vl);
          const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vl);
          const vfloat32m2_t w_safe = __riscv_vmerge_vvm_f32m2(zero, w, finite, vl);
          const vfloat32m2_t z_safe = __riscv_vmerge_vvm_f32m2(zero, z, finite, vl);
          sum += reduceSumF32M2(__riscv_vfmul_vv_f32m2(w_safe, z_safe, vl), vl);
          norm_sum += reduceSumF32M2(w_safe, vl);
          y_w += static_cast<int>(vl);
        }
      }

      output[center].r = center_r;
      output[center].g = center_g;
      output[center].b = center_b;

      if (norm_sum != 0.0f) {
        const float depth = sum / norm_sum;
        const Eigen::Vector3f pc(static_cast<float>(x) * depth,
                                 static_cast<float>(y) * depth,
                                 depth);
        const Eigen::Vector3f pw(unprojection_matrix * pc);
        output[center].x = pw[0];
        output[center].y = pw[1];
        output[center].z = pw[2];
      }
      else {
        output[center].x = nan;
        output[center].y = nan;
        output[center].z = nan;
      }
    }
  }

  output.header = input.header;
  output.width = input.width;
  output.height = input.height;
  return true;
}
#endif

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

class Benchmarker {
public:
  explicit Benchmarker(std::string name) : name_(std::move(name)) {}

  void run(const std::function<void()>& fn, int iterations, int warmup) const
  {
    for (int i = 0; i < warmup; ++i)
      fn();
    const auto begin = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
      fn();
    const auto end = std::chrono::high_resolution_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(end - begin).count();
    std::cout << std::left << std::setw(64) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void setChecksum(std::uint64_t value) const { checksum_ = value; }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

void
printStats(const diag::ErrorStats& stats)
{
  std::cout << "  Error: max_abs_xyz=" << std::scientific << std::setprecision(6)
            << stats.max_abs_xyz << ", max_abs_z=" << stats.max_abs_z
            << ", rmse_xyz=" << stats.rmse_xyz << ", finite_points=" << std::dec
            << stats.finite_points << ", nan_points=" << stats.nan_points
            << ", rgb_equal=" << (stats.rgb_equal ? "yes" : "no") << std::fixed
            << std::setprecision(4) << '\n';
}

void
runCase(const std::string& name,
        int width,
        int height,
        int window,
        bool holes,
        int iterations,
        int warmup)
{
  const auto cloud = diag::makeCloud(width, height, holes);
  const auto tables = diag::computeTables(window, 0.5f, 15.0f);
  const auto unprojection = diag::makeSimpleUnprojection();
  std::vector<diag::RgbPoint> output;
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(cloud, width, height, tables, unprojection, expected);

  Benchmarker bench(name);
  bench.run([&]() {
#if defined(__RVV10__)
    diag::processCandidate(cloud, width, height, tables, unprojection, output);
#else
    diag::processScalar(cloud, width, height, tables, unprojection, output);
#endif
    bench.setChecksum(diag::checksumCloud(output));
    doNotOptimize(output);
  }, iterations, warmup);
  printStats(diag::compareClouds(expected, output));
}

void
runDirectDepthCase(const std::string& name,
                   int width,
                   int height,
                   int window,
                   bool holes,
                   int iterations,
                   int warmup)
{
  const auto cloud = diag::makeCloud(width, height, holes);
  const auto tables = diag::computeTables(window, 0.5f, 15.0f);
  const auto unprojection = diag::makeSimpleUnprojection();
  std::vector<diag::RgbPoint> output;
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(cloud, width, height, tables, unprojection, expected);

  Benchmarker bench(name);
  bench.run([&]() {
#if defined(__RVV10__)
    diag::processDirectDepthCandidate(cloud, width, height, tables, unprojection, output);
#else
    diag::processScalar(cloud, width, height, tables, unprojection, output);
#endif
    bench.setChecksum(diag::checksumCloud(output));
    doNotOptimize(output);
  }, iterations, warmup);
  printStats(diag::compareClouds(expected, output));
}


template <typename PointInT, typename PointOutT = PointInT>
void
runProductionCase(const std::string& name,
                  int width,
                  int height,
                  int window,
                  bool holes,
                  int iterations,
                  int warmup)
{
  const auto diag_cloud = diag::makeCloud(width, height, holes);
  const auto tables = diag::computeTables(window, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(diag_cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  auto input = makePclCloud<PointInT>(diag_cloud, width, height);
  auto input_ptr = input.makeShared();
  pcl::PointCloud<PointOutT> output;

  Benchmarker bench(name);
  bench.run([&]() {
    pcl::BilateralUpsampling<PointInT, PointOutT> upsampling;
    upsampling.setInputCloud(input_ptr);
    upsampling.setWindowSize(window);
    upsampling.setSigmaDepth(0.5f);
    upsampling.setSigmaColor(15.0f);
    upsampling.setProjectionMatrix(makeProductionProjection());
    upsampling.process(output);
    bench.setChecksum(diag::checksumCloud(fromPclCloud(output)));
    doNotOptimize(output);
  }, iterations, warmup);
  printStats(diag::compareClouds(expected, fromPclCloud(output)));
}

template <typename PointInT, typename PointOutT = PointInT>
void
runProductionSteadyStateCase(const std::string& name,
                             int width,
                             int height,
                             int window,
                             bool holes,
                             int iterations,
                             int warmup)
{
  const auto diag_cloud = diag::makeCloud(width, height, holes);
  const auto tables = diag::computeTables(window, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(diag_cloud, width, height, tables, diag::makeSimpleUnprojection(), expected);
  auto input = makePclCloud<PointInT>(diag_cloud, width, height);
  auto input_ptr = input.makeShared();
  pcl::BilateralUpsampling<PointInT, PointOutT> upsampling;
  upsampling.setInputCloud(input_ptr);
  upsampling.setWindowSize(window);
  upsampling.setSigmaDepth(0.5f);
  upsampling.setSigmaColor(15.0f);
  upsampling.setProjectionMatrix(makeProductionProjection());
  pcl::PointCloud<PointOutT> output;

  Benchmarker bench(name);
  bench.run([&]() {
    ScopedStdoutSilence silence;
    upsampling.process(output);
    bench.setChecksum(diag::checksumCloud(fromPclCloud(output)));
    doNotOptimize(output);
  }, iterations, warmup);
  printStats(diag::compareClouds(expected, fromPclCloud(output)));
}

template <typename PointT>
void
runProductionDetailHelperCase(const std::string& name,
                              int width,
                              int height,
                              int window,
                              bool holes,
                              int iterations,
                              int warmup)
{
  const auto diag_cloud = diag::makeCloud(width, height, holes);
  const auto expected_tables = diag::computeTables(window, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(diag_cloud, width, height, expected_tables, diag::makeSimpleUnprojection(), expected);

  auto input = makePclCloud<PointT>(diag_cloud, width, height);
  auto input_ptr = input.makeShared();
  const Eigen::MatrixXf depth = makeProductionDepthMatrix(window);
  const Eigen::VectorXf rgb = makeProductionRgbVector();
  const Eigen::Matrix3f unprojection = makeProductionUnprojection();
  pcl::PointCloud<PointT> output;

  Benchmarker bench(name);
  bench.run([&]() {
#if defined(__RVV10__)
    const bool rvv_hit = pcl::bilateralUpsamplingPerformProcessingRVV<PointT, PointT>(
        *input_ptr, output, window, depth, rgb, unprojection);
    if (!rvv_hit)
      pcl::bilateralUpsamplingPerformProcessingStd<PointT, PointT>(
          *input_ptr, output, window, depth, rgb, unprojection);
#else
    pcl::bilateralUpsamplingPerformProcessingStd<PointT, PointT>(
        *input_ptr, output, window, depth, rgb, unprojection);
#endif
    bench.setChecksum(diag::checksumCloud(fromPclCloud(output)));
    doNotOptimize(output);
  }, iterations, warmup);
  printStats(diag::compareClouds(expected, fromPclCloud(output)));
}

template <typename PointT>
void
runProductionDetailNanMaskK64Case(const std::string& name,
                                  int width,
                                  int height,
                                  int window,
                                  bool holes,
                                  int iterations,
                                  int warmup)
{
  const auto diag_cloud = diag::makeCloud(width, height, holes);
  const auto expected_tables = diag::computeTables(window, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(diag_cloud, width, height, expected_tables, diag::makeSimpleUnprojection(), expected);

  auto input = makePclCloud<PointT>(diag_cloud, width, height);
  const Eigen::MatrixXf depth = makeProductionDepthMatrix(window);
  const Eigen::VectorXf rgb = makeProductionRgbVector();
  const Eigen::Matrix3f unprojection = makeProductionUnprojection();
  pcl::PointCloud<PointT> output;

  Benchmarker bench(name);
  bench.run([&]() {
#if defined(__RVV10__)
    const bool rvv_hit =
        performProcessingNanMaskK64RVV<PointT>(input, output, window, depth, rgb, unprojection);
    if (!rvv_hit)
      pcl::bilateralUpsamplingPerformProcessingStd<PointT, PointT>(
          input, output, window, depth, rgb, unprojection);
#else
    pcl::bilateralUpsamplingPerformProcessingStd<PointT, PointT>(
        input, output, window, depth, rgb, unprojection);
#endif
    bench.setChecksum(diag::checksumCloud(fromPclCloud(output)));
    doNotOptimize(output);
  }, iterations, warmup);
  printStats(diag::compareClouds(expected, fromPclCloud(output)));
}

template <typename PointT>
void
runProductionDetailColorGatherCase(const std::string& name,
                                   int width,
                                   int height,
                                   int window,
                                   bool holes,
                                   int iterations,
                                   int warmup)
{
  const auto diag_cloud = diag::makeCloud(width, height, holes);
  const auto expected_tables = diag::computeTables(window, 0.5f, 15.0f);
  std::vector<diag::RgbPoint> expected;
  diag::processScalar(diag_cloud, width, height, expected_tables, diag::makeSimpleUnprojection(), expected);

  auto input = makePclCloud<PointT>(diag_cloud, width, height);
  const Eigen::MatrixXf depth = makeProductionDepthMatrix(window);
  const Eigen::VectorXf rgb = makeProductionRgbVector();
  const Eigen::Matrix3f unprojection = makeProductionUnprojection();
  pcl::PointCloud<PointT> output;

  Benchmarker bench(name);
  bench.run([&]() {
#if defined(__RVV10__)
    const bool rvv_hit =
        performProcessingColorGatherRVV<PointT>(input, output, window, depth, rgb, unprojection);
    if (!rvv_hit)
      pcl::bilateralUpsamplingPerformProcessingStd<PointT, PointT>(
          input, output, window, depth, rgb, unprojection);
#else
    pcl::bilateralUpsamplingPerformProcessingStd<PointT, PointT>(
        input, output, window, depth, rgb, unprojection);
#endif
    bench.setChecksum(diag::checksumCloud(fromPclCloud(output)));
    doNotOptimize(output);
  }, iterations, warmup);
  printStats(diag::compareClouds(expected, fromPclCloud(output)));
}

} // namespace

int
main(int argc, char** argv)
{
  int iterations = 5;
  int warmup = 2;
  if (argc > 1)
    iterations = std::atoi(argv[1]);
  if (argc > 2)
    warmup = std::atoi(argv[2]);

  std::cout << std::string(96, '=') << '\n';
  std::cout << "PCL surface/bilateral_upsampling RVV diagnostic\n";
  std::cout << "Dataset: synthetic organized RGBD grids; table lookup weights; output unprojection included\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup << '\n';
#if defined(__RVV10__)
  std::cout << "Build: RVV production-detail candidate family (__RVV10__ enabled)\n";
#else
  std::cout << "Build: scalar reference (__RVV10__ disabled)\n";
#endif
  std::cout << std::string(96, '=') << '\n';

  runCase("bilateral upsampling table window 80x60 w3 dense", 80, 60, 3, false, iterations, warmup);
  runCase("bilateral upsampling table window 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runCase("bilateral upsampling table window 180x120 w5 dense", 180, 120, 5, false, iterations, warmup);
  runDirectDepthCase("bilateral upsampling direct-depth 80x60 w3 dense", 80, 60, 3, false, iterations, warmup);
  runDirectDepthCase("bilateral upsampling direct-depth 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runDirectDepthCase("bilateral upsampling direct-depth 180x120 w5 dense", 180, 120, 5, false, iterations, warmup);
  runProductionCase<pcl::PointXYZRGB>("bilateral upsampling production public PointXYZRGB 80x60 w3 dense", 80, 60, 3, false, iterations, warmup);
  runProductionCase<pcl::PointXYZRGB>("bilateral upsampling production public PointXYZRGB 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runProductionCase<pcl::PointXYZRGBA>("bilateral upsampling production public PointXYZRGBA 180x120 w5 dense", 180, 120, 5, false, iterations, warmup);
  runProductionCase<pcl::PointXYZRGB, pcl::PointXYZRGBA>("bilateral upsampling production public PointXYZRGB to PointXYZRGBA 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runProductionCase<pcl::PointXYZRGBA, pcl::PointXYZRGB>("bilateral upsampling production public PointXYZRGBA to PointXYZRGB 120x90 w4 dense", 120, 90, 4, false, iterations, warmup);
  runProductionSteadyStateCase<pcl::PointXYZRGB>("bilateral upsampling production steady public PointXYZRGB 80x60 w3 dense", 80, 60, 3, false, iterations, warmup);
  runProductionSteadyStateCase<pcl::PointXYZRGB>("bilateral upsampling production steady public PointXYZRGB 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runProductionSteadyStateCase<pcl::PointXYZRGBA>("bilateral upsampling production steady public PointXYZRGBA 180x120 w5 dense", 180, 120, 5, false, iterations, warmup);
  runProductionSteadyStateCase<pcl::PointXYZRGB, pcl::PointXYZRGBA>("bilateral upsampling production steady public PointXYZRGB to PointXYZRGBA 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runProductionSteadyStateCase<pcl::PointXYZRGBA, pcl::PointXYZRGB>("bilateral upsampling production steady public PointXYZRGBA to PointXYZRGB 120x90 w4 dense", 120, 90, 4, false, iterations, warmup);
  runProductionDetailHelperCase<pcl::PointXYZRGB>("bilateral upsampling production detail helper PointXYZRGB 80x60 w3 dense", 80, 60, 3, false, iterations, warmup);
  runProductionDetailHelperCase<pcl::PointXYZRGB>("bilateral upsampling production detail helper PointXYZRGB 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runProductionDetailHelperCase<pcl::PointXYZRGBA>("bilateral upsampling production detail helper PointXYZRGBA 180x120 w5 dense", 180, 120, 5, false, iterations, warmup);
  runProductionDetailNanMaskK64Case<pcl::PointXYZRGB>("bilateral upsampling production detail local nan-mask k64 PointXYZRGB 80x60 w3 dense", 80, 60, 3, false, iterations, warmup);
  runProductionDetailNanMaskK64Case<pcl::PointXYZRGB>("bilateral upsampling production detail local nan-mask k64 PointXYZRGB 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runProductionDetailNanMaskK64Case<pcl::PointXYZRGBA>("bilateral upsampling production detail local nan-mask k64 PointXYZRGBA 180x120 w5 dense", 180, 120, 5, false, iterations, warmup);
  runProductionDetailColorGatherCase<pcl::PointXYZRGB>("bilateral upsampling production detail color-gather PointXYZRGB 80x60 w3 dense", 80, 60, 3, false, iterations, warmup);
  runProductionDetailColorGatherCase<pcl::PointXYZRGB>("bilateral upsampling production detail color-gather PointXYZRGB 120x90 w4 holes", 120, 90, 4, true, iterations, warmup);
  runProductionDetailColorGatherCase<pcl::PointXYZRGBA>("bilateral upsampling production detail color-gather PointXYZRGBA 180x120 w5 dense", 180, 120, 5, false, iterations, warmup);

  std::cout << std::string(96, '=') << '\n';
  return 0;
}
