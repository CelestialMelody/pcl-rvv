/*
 * GASD topic benchmark（性能测试）入口。
 *
 * 输出格式兼容 `test-rvv/script/analyze_bench_compare.py`。QEMU（仿真器）运行只
 * 允许作为 build / log-shape smoke（构建 / 日志形状小型验证）；性能结论只能来自
 * board（板卡）或目标硬件 repeated benchmark（重复性能测试）。
 */

#include "gasd.h"

#include <pcl/features/gasd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace gasd = pcl::features::rvv_test::gasd;

namespace
{
using Clock = std::chrono::steady_clock;

struct Options
{
  std::size_t points = 4096;
  std::size_t shape_half_grid = 4;
  std::size_t color_half_grid = 3;
  std::size_t hists_size = 12;
  int repeat = 8;
  int iterations = 8;
  int warmup = 2;
  std::string case_filter = "all";
};

bool
hasArgValue(const int argc, char** argv, const char* key, std::size_t& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = static_cast<std::size_t>(std::strtoull(argv[i + 1], nullptr, 10));
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = argv[i + 1];
      return true;
    }
  }
  return false;
}

Options
parseOptions(const int argc, char** argv)
{
  Options options;
  hasArgValue(argc, argv, "--points", options.points);
  hasArgValue(argc, argv, "--shape-half-grid", options.shape_half_grid);
  hasArgValue(argc, argv, "--color-half-grid", options.color_half_grid);
  hasArgValue(argc, argv, "--hists-size", options.hists_size);
  hasArgValue(argc, argv, "--repeat", options.repeat);
  hasArgValue(argc, argv, "--iterations", options.iterations);
  hasArgValue(argc, argv, "--warmup", options.warmup);
  hasArgValue(argc, argv, "--case-filter", options.case_filter);
  options.points = std::max(options.points, static_cast<std::size_t>(8));
  options.shape_half_grid = std::max(options.shape_half_grid, static_cast<std::size_t>(1));
  options.color_half_grid = std::max(options.color_half_grid, static_cast<std::size_t>(1));
  options.hists_size = std::max(options.hists_size, static_cast<std::size_t>(1));
  options.repeat = std::max(options.repeat, 1);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  return options.case_filter == "all" || options.case_filter == name;
}

template <typename Fn>
double
timeCase(const int warmup, const int iterations, Fn&& fn, double& checksum)
{
  for (int i = 0; i < warmup; ++i)
    checksum += fn();

  const auto t0 = Clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / static_cast<double>(iterations);
}

void
printCase(const std::string& name, const double ms_per_iter, const double checksum)
{
  std::cout << name << ": " << ms_per_iter << " ms / iter\n";
  std::cout << name << " checksum: " << checksum << "\n";
}

gasd::HistogramGrid
makeShapeGrid(const Options& options)
{
  return gasd::makeHistogramGrid(options.shape_half_grid, options.hists_size);
}

gasd::HistogramGrid
makeColorGrid(const Options& options)
{
  return gasd::makeHistogramGrid(options.color_half_grid, options.hists_size);
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  const gasd::HistogramGrid shape_grid = makeShapeGrid(options);
  const gasd::HistogramGrid color_grid = makeColorGrid(options);
  const auto shape_cloud = gasd::makeShapeCloud(options.points);
  const auto color_cloud = gasd::makeColorCloud(*shape_cloud);
  const auto shape_projection_normalization = gasd::computeShapeProjectionNormalization(*shape_cloud);

  std::cout << "Dataset: synthetic gasd points=" << options.points
            << " shape_half_grid=" << options.shape_half_grid
            << " color_half_grid=" << options.color_half_grid
            << " hists_size=" << options.hists_size << " repeat=" << options.repeat << "\n";
  std::cout << "Shape projection normalization: max_coord=" << shape_projection_normalization.max_coord
            << " distance_normalization_factor=" << shape_projection_normalization.distance_normalization_factor << "\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  if (caseEnabled(options, "component_shape_copy_std"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      std::vector<float> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::copyShapeHistogramsStdToBuffer(shape_grid, options.shape_half_grid, options.hists_size, output);
        local_checksum += static_cast<double>(gasd::checksumFlat(output));
      }
      return local_checksum;
    }, checksum);
    printCase("component_shape_copy_std", ms, checksum);
  }

  if (caseEnabled(options, "candidate_shape_copy_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      std::vector<float> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::copyShapeHistogramsRVVToBuffer(shape_grid, options.shape_half_grid, options.hists_size, output);
        local_checksum += static_cast<double>(gasd::checksumFlat(output));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_shape_copy_rvv", ms, checksum);
  }

  if (caseEnabled(options, "component_color_copy_std"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      std::vector<float> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::copyColorHistogramsStdToBuffer(color_grid, options.color_half_grid, options.hists_size, output);
        local_checksum += static_cast<double>(gasd::checksumFlat(output));
      }
      return local_checksum;
    }, checksum);
    printCase("component_color_copy_std", ms, checksum);
  }

  if (caseEnabled(options, "candidate_color_copy_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      std::vector<float> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::copyColorHistogramsRVVToBuffer(color_grid, options.color_half_grid, options.hists_size, output);
        local_checksum += static_cast<double>(gasd::checksumFlat(output));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_color_copy_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_shape_projection_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      gasd::ShapeProjectionBuffers buffers;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::projectShapeSamplesRVVToBuffers(*shape_cloud,
                                              shape_projection_normalization.max_coord,
                                              shape_projection_normalization.distance_normalization_factor,
                                              options.shape_half_grid,
                                              options.hists_size,
                                              buffers);
        local_checksum += static_cast<double>(gasd::checksumShapeProjection(buffers));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_shape_projection_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_trilinear_interpolation_rvv"))
  {
    gasd::ShapeProjectionBuffers projection;
    gasd::projectShapeSamplesStdToBuffers(*shape_cloud,
                                          shape_projection_normalization.max_coord,
                                          shape_projection_normalization.distance_normalization_factor,
                                          options.shape_half_grid,
                                          options.hists_size,
                                          projection);

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      gasd::TrilinearInterpolationBuffers buffers;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::computeTrilinearInterpolationRVVToBuffers(projection, options.shape_half_grid, buffers);
        local_checksum += static_cast<double>(gasd::checksumTrilinearInterpolation(buffers));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_trilinear_interpolation_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_trilinear_histogram_write_rvv"))
  {
    gasd::ShapeProjectionBuffers projection;
    gasd::projectShapeSamplesStdToBuffers(*shape_cloud,
                                          shape_projection_normalization.max_coord,
                                          shape_projection_normalization.distance_normalization_factor,
                                          options.shape_half_grid,
                                          options.hists_size,
                                          projection);
    const float hist_incr = 100.0f / static_cast<float>(shape_cloud->size() - 1);

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      std::vector<float> hists;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::accumulateTrilinearHistogramRVVStaged(projection, options.shape_half_grid, options.hists_size, hist_incr, hists);
        local_checksum += static_cast<double>(gasd::checksumFlat(hists));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_trilinear_histogram_write_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_trilinear_eigen_histogram_write_rvv"))
  {
    gasd::ShapeProjectionBuffers projection;
    gasd::projectShapeSamplesStdToBuffers(*shape_cloud,
                                          shape_projection_normalization.max_coord,
                                          shape_projection_normalization.distance_normalization_factor,
                                          options.shape_half_grid,
                                          options.hists_size,
                                          projection);
    const float hist_incr = 100.0f / static_cast<float>(shape_cloud->size() - 1);

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      gasd::HistogramGrid hists;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::accumulateTrilinearHistogramEigenRVVStaged(projection, options.shape_half_grid, options.hists_size, hist_incr, hists);
        local_checksum += static_cast<double>(gasd::checksumHistogramGrid(hists));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_trilinear_eigen_histogram_write_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_shape_combined_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      std::vector<float> output;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::computeShapeDescriptorTrilinearRVVStaged(*shape_cloud,
                                                       shape_projection_normalization.max_coord,
                                                       shape_projection_normalization.distance_normalization_factor,
                                                       options.shape_half_grid,
                                                       options.hists_size,
                                                       output);
        local_checksum += static_cast<double>(gasd::checksumFlatScaled(output, 1000.0f));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_shape_combined_rvv", ms, checksum);
  }

  if (caseEnabled(options, "candidate_color_hue_rvv"))
  {
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      gasd::ColorHueBuffers buffers;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        gasd::projectColorHueRVVToBuffers(*color_cloud, options.hists_size, buffers);
        local_checksum += static_cast<double>(gasd::checksumColorHue(buffers));
      }
      return local_checksum;
    }, checksum);
    printCase("candidate_color_hue_rvv", ms, checksum);
  }

  if (caseEnabled(options, "public_gasd_shape_compute"))
  {
    pcl::GASDEstimation<pcl::PointXYZ, pcl::GASDSignature512> estimator;
    estimator.setInputCloud(shape_cloud);

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::GASDSignature512> descriptor;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        estimator.compute(descriptor);
        local_checksum += static_cast<double>(descriptor[0].descriptorSize());
      }
      return local_checksum;
    }, checksum);
    printCase("public_gasd_shape_compute", ms, checksum);
  }

  if (caseEnabled(options, "public_gasd_color_compute"))
  {
    pcl::GASDColorEstimation<pcl::PointXYZRGBA, pcl::GASDSignature984> estimator;
    estimator.setInputCloud(color_cloud);

    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      pcl::PointCloud<pcl::GASDSignature984> descriptor;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        estimator.compute(descriptor);
        local_checksum += static_cast<double>(descriptor[0].descriptorSize());
      }
      return local_checksum;
    }, checksum);
    printCase("public_gasd_color_compute", ms, checksum);
  }

  return 0;
}
