/*
 * shot.hpp RVV topic benchmark.
 *
 * 输出格式兼容 test-rvv/script/analyze_bench_compare.py：Dataset、Iterations
 * 和 "<case>: <time> ms / iter"。性能结论只能来自板卡 repeated run（重复板卡测试）；
 * QEMU 运行仅可作为 build / log-shape smoke（日志形状小型验证）。
 */

#include <pcl/features/shot.h>
#include <pcl/search/kdtree.h>

#include "shot.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
  int side = 21;
  int iterations = 30;
  int warmup = 3;
  std::string case_filter = "all";
};

bool
hasArgValue(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], key) == 0) {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], key) == 0) {
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
  hasArgValue(argc, argv, "--side", options.side);
  hasArgValue(argc, argv, "--iterations", options.iterations);
  hasArgValue(argc, argv, "--warmup", options.warmup);
  hasArgValue(argc, argv, "--case-filter", options.case_filter);
  options.side = std::max(options.side, 9);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  return options;
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
  return std::chrono::duration<double, std::milli>(t1 - t0).count() /
         static_cast<double>(iterations);
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  return options.case_filter == "all" || options.case_filter == name;
}

void
printCase(const std::string& name, const double ms_per_iter, const double checksum)
{
  std::cout << name << ": " << ms_per_iter << " ms / iter\n";
  std::cout << name << " checksum: " << checksum << "\n";
}

std::vector<float>
makeDescriptorBatch(const int descriptor_length, const int batch_size)
{
  std::vector<float> values(static_cast<std::size_t>(descriptor_length * batch_size));
  for (int b = 0; b < batch_size; ++b) {
    for (int i = 0; i < descriptor_length; ++i) {
      const float wave = std::sin(static_cast<float>(i + b * 13) * 0.071f);
      values[static_cast<std::size_t>(b * descriptor_length + i)] =
          0.125f + 0.5f * wave + static_cast<float>((i % 11) + 1) * 0.01f;
    }
  }
  return values;
}

double
descriptorBatchChecksum(const std::vector<float>& values)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < values.size(); ++i)
    checksum += static_cast<double>(values[i]) * static_cast<double>((i % 17) + 1);
  return checksum;
}

void
makeShapeBinBatch(std::vector<float>& nx, std::vector<float>& ny, std::vector<float>& nz)
{
  const std::size_t count = nx.size();
  for (std::size_t i = 0; i < count; ++i) {
    nx[i] = std::sin(static_cast<float>(i) * 0.17f) * 0.35f;
    ny[i] = std::cos(static_cast<float>(i) * 0.11f) * 0.25f;
    nz[i] = 0.72f + static_cast<float>(i % 9) * 0.04f;
  }
}

pcl::PointCloud<pcl::Normal>
makeShapeBinAosBatch(const std::size_t count)
{
  std::vector<float> nx(count), ny(count), nz(count);
  makeShapeBinBatch(nx, ny, nz);

  pcl::PointCloud<pcl::Normal> normals;
  normals.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    normals[i].normal_x = nx[i];
    normals[i].normal_y = ny[i];
    normals[i].normal_z = nz[i];
  }
  return normals;
}

double
shapeBinChecksum(const std::vector<double>& values)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (std::isfinite(values[i]))
      checksum += values[i] * static_cast<double>((i % 17) + 1);
  }
  return checksum;
}

double
runNormalizeBatch(std::vector<float>& values, const int descriptor_length)
{
  const int batch_size = static_cast<int>(values.size() / static_cast<std::size_t>(descriptor_length));
  for (int b = 0; b < batch_size; ++b)
    pcl_rvv_shot::normalizeDescriptorRVV(values.data() + b * descriptor_length,
                                         static_cast<std::size_t>(descriptor_length));
  return descriptorBatchChecksum(values);
}

double
runShapeBinBatch(const std::vector<float>& nx,
                 const std::vector<float>& ny,
                 const std::vector<float>& nz,
                 std::vector<double>& out)
{
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};
  pcl_rvv_shot::computeShapeBinDistanceRVV(
      nx.data(), ny.data(), nz.data(), nx.size(), frame_z, 10, out.data());
  return shapeBinChecksum(out);
}

double
runShapeBinAosBatch(const pcl::PointCloud<pcl::Normal>& normals, std::vector<double>& out)
{
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};
  pcl_rvv_shot::computeShapeBinDistanceAoSRVV(normals, 0, normals.size(), frame_z, 10, out.data());
  return shapeBinChecksum(out);
}

pcl::Indices
makeShapeBinIndexedOrder(const std::size_t count)
{
  pcl::Indices indices(count);
  for (std::size_t i = 0; i < count; ++i)
    indices[i] = static_cast<int>((i * 37 + (i / 11) * 13) % count);
  return indices;
}

double
runShapeBinIndexedBatch(const pcl::PointCloud<pcl::Normal>& normals,
                        const pcl::Indices& indices,
                        std::vector<double>& out)
{
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};
  const unsigned nan_count =
      pcl_rvv_shot::computeShapeBinDistanceIndexedRVV(normals, indices, frame_z, 10, out.data());
  return shapeBinChecksum(out) + static_cast<double>(nan_count) * 0.125;
}

template <typename PointInT, typename NormalT, typename OutputT>
class ShotShapeBinDirectProbe
: public pcl::SHOTEstimation<PointInT, NormalT, OutputT> {
 public:
  using Base = pcl::SHOTEstimation<PointInT, NormalT, OutputT>;
  using Base::createBinDistanceShape;
};

double
runProductionShapeBinDirectBatch(ShotShapeBinDirectProbe<pcl::PointXYZ, pcl::Normal, pcl::SHOT352>& shot,
                                 const pcl::Indices& indices,
                                 std::vector<double>& out)
{
  shot.createBinDistanceShape(0, indices, out);
  return shapeBinChecksum(out);
}

pcl::PointCloud<pcl::PointXYZ>
makeInterpolationSurfaceBatch(const std::size_t count)
{
  pcl::PointCloud<pcl::PointXYZ> surface;
  surface.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    surface[i].x = std::sin(static_cast<float>(i) * 0.013f) * 0.08f + static_cast<float>(i % 17) * 0.001f;
    surface[i].y = std::cos(static_cast<float>(i) * 0.017f) * 0.07f - static_cast<float>(i % 13) * 0.001f;
    surface[i].z = 0.02f + std::sin(static_cast<float>(i) * 0.019f) * 0.03f;
  }
  return surface;
}

pcl::Indices
makeInterpolationIndexedOrder(const std::size_t count)
{
  pcl::Indices indices(count);
  for (std::size_t i = 0; i < count; ++i)
    indices[i] = static_cast<int>((i * 41 + (i / 7) * 19) % count);
  return indices;
}

double
interpolationGeometryChecksum(const std::vector<double>& x,
                              const std::vector<double>& y,
                              const std::vector<double>& z,
                              const std::vector<double>& distance,
                              const std::vector<std::uint8_t>& valid)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < x.size(); ++i) {
    checksum += x[i] * static_cast<double>((i % 11) + 1);
    checksum += y[i] * static_cast<double>((i % 13) + 1);
    checksum += z[i] * static_cast<double>((i % 17) + 1);
    checksum += distance[i] * static_cast<double>((i % 19) + 1);
    checksum += static_cast<double>(valid[i]) * 0.03125;
  }
  return checksum;
}

void
makeInterpolationBinSelectionBatch(std::vector<double>& x,
                                   std::vector<double>& y,
                                   std::vector<double>& z,
                                   std::vector<double>& distance,
                                   std::vector<double>& bin_distance)
{
  const std::size_t count = x.size();
  for (std::size_t i = 0; i < count; ++i) {
    x[i] = std::sin(static_cast<double>(i) * 0.013) * 0.8 + static_cast<double>(i % 5) * 0.03;
    y[i] = std::cos(static_cast<double>(i) * 0.017) * 0.7 - static_cast<double>(i % 7) * 0.02;
    z[i] = std::sin(static_cast<double>(i) * 0.019) * 0.6;
    distance[i] = 0.08 + static_cast<double>(i % 37) * 0.011;
    bin_distance[i] = static_cast<double>(i % 10) + 0.15 * static_cast<double>(static_cast<int>(i % 7) - 3);
  }
  for (std::size_t i = 31; i < count; i += 257)
    bin_distance[i] = std::numeric_limits<double>::quiet_NaN();
  for (std::size_t i = 47; i < count; i += 263)
    distance[i] = 0.0;
}

double
interpolationBinSelectionChecksum(const std::vector<std::int32_t>& desc_index,
                                  const std::vector<std::int32_t>& step_index,
                                  const std::vector<std::int32_t>& adjacent_index,
                                  const std::vector<float>& adjacent_delta,
                                  const std::vector<float>& center_weight,
                                  const std::vector<std::uint8_t>& valid)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < desc_index.size(); ++i) {
    checksum += static_cast<double>(desc_index[i]) * static_cast<double>((i % 7) + 1);
    checksum += static_cast<double>(step_index[i]) * static_cast<double>((i % 11) + 1);
    checksum += static_cast<double>(adjacent_index[i]) * 0.03125;
    checksum += static_cast<double>(adjacent_delta[i]) * static_cast<double>((i % 13) + 1);
    checksum += static_cast<double>(center_weight[i]) * static_cast<double>((i % 17) + 1);
    checksum += static_cast<double>(valid[i]) * 0.015625;
  }
  return checksum;
}

void
makeColorLabBatch(std::vector<float>& l, std::vector<float>& a, std::vector<float>& b)
{
  const std::size_t count = l.size();
  for (std::size_t i = 0; i < count; ++i) {
    l[i] = 0.10f + static_cast<float>(i % 23) * 0.031f;
    a[i] = -0.82f + static_cast<float>(i % 37) * 0.047f;
    b[i] = 0.78f - static_cast<float>(i % 41) * 0.039f;
  }
  for (std::size_t i = 19; i < count; i += 97) {
    l[i] = 4.0f;
    a[i] = -7.0f;
    b[i] = 7.0f;
  }
}

double
colorBinChecksum(const std::vector<double>& values)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < values.size(); ++i)
    checksum += values[i] * static_cast<double>((i % 23) + 1);
  return checksum;
}

double
runColorLabDistanceBatch(const std::vector<float>& l,
                         const std::vector<float>& a,
                         const std::vector<float>& b,
                         std::vector<double>& out)
{
  pcl_rvv_shot::computeColorBinDistanceRVV(
      l.data(), a.data(), b.data(), l.size(), 0.42f, -0.35f, 0.61f, 30, out.data());
  return colorBinChecksum(out);
}

double
runInterpolationBinSelectionBatch(const std::vector<double>& x,
                                  const std::vector<double>& y,
                                  const std::vector<double>& z,
                                  const std::vector<double>& distance,
                                  const std::vector<double>& bin_distance,
                                  std::vector<std::int32_t>& desc_index,
                                  std::vector<std::int32_t>& step_index,
                                  std::vector<std::int32_t>& adjacent_index,
                                  std::vector<float>& adjacent_delta,
                                  std::vector<float>& center_weight,
                                  std::vector<std::uint8_t>& valid)
{
  pcl_rvv_shot::computeInterpolationBinSelectionRVV(x.data(),
                                                    y.data(),
                                                    z.data(),
                                                    distance.data(),
                                                    bin_distance.data(),
                                                    x.size(),
                                                    10,
                                                    0.5,
                                                    desc_index.data(),
                                                    step_index.data(),
                                                    adjacent_index.data(),
                                                    adjacent_delta.data(),
                                                    center_weight.data(),
                                                    valid.data());
  return interpolationBinSelectionChecksum(desc_index,
                                           step_index,
                                           adjacent_index,
                                           adjacent_delta,
                                           center_weight,
                                           valid);
}

double
runColorRgbLutIndexedBatch(const pcl::PointCloud<pcl::PointXYZRGBA>& surface,
                           const pcl::Indices& indices,
                           std::vector<double>& out)
{
  const int reference_index = indices.empty() ? 0 : indices[indices.size() / 2];
  pcl_rvv_shot::computeColorBinDistanceIndexedRGBRVV(surface, indices, reference_index, 30, out.data());
  return colorBinChecksum(out);
}

double
runInterpolationGeometryBatch(const pcl::PointCloud<pcl::PointXYZ>& surface,
                              const pcl::Indices& indices,
                              const std::vector<float>& sqr_dists,
                              const std::vector<double>& bin_distance,
                              std::vector<double>& out_x,
                              std::vector<double>& out_y,
                              std::vector<double>& out_z,
                              std::vector<double>& out_distance,
                              std::vector<std::uint8_t>& out_valid)
{
  const float central[3] = {surface[indices[indices.size() / 2]].x,
                            surface[indices[indices.size() / 2]].y,
                            surface[indices[indices.size() / 2]].z};
  const float frame_x[3] = {0.96f, 0.08f, 0.02f};
  const float frame_y[3] = {-0.05f, 0.98f, 0.04f};
  const float frame_z[3] = {0.03f, -0.02f, 1.0f};
  pcl_rvv_shot::computeInterpolationGeometryIndexedRVV(surface,
                                                       indices,
                                                       sqr_dists.data(),
                                                       bin_distance.data(),
                                                       central,
                                                       frame_x,
                                                       frame_y,
                                                       frame_z,
                                                       out_x.data(),
                                                       out_y.data(),
                                                       out_z.data(),
                                                       out_distance.data(),
                                                       out_valid.data());
  return interpolationGeometryChecksum(out_x, out_y, out_z, out_distance, out_valid);
}

} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);

  const auto shape_cloud = pcl_rvv_shot::makeShotShapeCloud(options.side);
  const auto color_cloud = pcl_rvv_shot::makeShotColorCloud(options.side);
  const auto shape_normals = pcl_rvv_shot::makeShotNormals(shape_cloud->size());
  const auto color_normals = pcl_rvv_shot::makeShotNormals(color_cloud->size());
  const auto indices = pcl_rvv_shot::makeCenterIndices(options.side);
  const auto frames = pcl_rvv_shot::makeIdentityFrames(indices->size());

  std::cout << "Dataset: synthetic shot grid side=" << options.side
            << " points=" << shape_cloud->size()
            << " keypoints=" << indices->size()
            << " fixed_lrf=true\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  if (caseEnabled(options, "public_shot352_fixed_lrf")) {
    pcl::SHOTEstimation<pcl::PointXYZ, pcl::Normal, pcl::SHOT352> shot;
    shot.setInputCloud(shape_cloud);
    shot.setSearchSurface(shape_cloud);
    shot.setInputNormals(shape_normals);
    shot.setIndices(indices);
    shot.setInputReferenceFrames(frames);
    shot.setSearchMethod(pcl::search::KdTree<pcl::PointXYZ>::Ptr(new pcl::search::KdTree<pcl::PointXYZ>));
    shot.setRadiusSearch(0.055);

    pcl::PointCloud<pcl::SHOT352> output;
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      shot.compute(output);
      return pcl_rvv_shot::descriptorChecksum(output);
    }, checksum);
    printCase("public_shot352_fixed_lrf", ms, checksum);
  }

  if (caseEnabled(options, "public_shot1344_fixed_lrf")) {
    pcl::SHOTColorEstimation<pcl::PointXYZRGBA, pcl::Normal, pcl::SHOT1344> shot(true, true);
    shot.setInputCloud(color_cloud);
    shot.setSearchSurface(color_cloud);
    shot.setInputNormals(color_normals);
    shot.setIndices(indices);
    shot.setInputReferenceFrames(frames);
    shot.setSearchMethod(pcl::search::KdTree<pcl::PointXYZRGBA>::Ptr(new pcl::search::KdTree<pcl::PointXYZRGBA>));
    shot.setRadiusSearch(0.055);

    pcl::PointCloud<pcl::SHOT1344> output;
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      shot.compute(output);
      return pcl_rvv_shot::descriptorChecksum(output);
    }, checksum);
    printCase("public_shot1344_fixed_lrf", ms, checksum);
  }

  if (caseEnabled(options, "normalize_352_component")) {
    auto descriptors = makeDescriptorBatch(352, options.side * options.side);
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runNormalizeBatch(descriptors, 352);
    }, checksum);
    printCase("normalize_352_component", ms, checksum);
  }

  if (caseEnabled(options, "normalize_1344_component")) {
    auto descriptors = makeDescriptorBatch(1344, options.side * options.side);
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runNormalizeBatch(descriptors, 1344);
    }, checksum);
    printCase("normalize_1344_component", ms, checksum);
  }

  if (caseEnabled(options, "shape_bin_component")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    std::vector<float> nx(count), ny(count), nz(count);
    std::vector<double> out(count);
    makeShapeBinBatch(nx, ny, nz);
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runShapeBinBatch(nx, ny, nz, out);
    }, checksum);
    printCase("shape_bin_component", ms, checksum);
  }

  if (caseEnabled(options, "shape_bin_aos_component")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    const auto normals = makeShapeBinAosBatch(count);
    std::vector<double> out(count);
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runShapeBinAosBatch(normals, out);
    }, checksum);
    printCase("shape_bin_aos_component", ms, checksum);
  }

  if (caseEnabled(options, "shape_bin_indexed_component")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    const auto normals = makeShapeBinAosBatch(count);
    const auto indices = makeShapeBinIndexedOrder(count);
    std::vector<double> out(indices.size());
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runShapeBinIndexedBatch(normals, indices, out);
    }, checksum);
    printCase("shape_bin_indexed_component", ms, checksum);
  }

  if (caseEnabled(options, "production_shape_bin_direct")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    const auto normals =
        pcl::make_shared<pcl::PointCloud<pcl::Normal>>(makeShapeBinAosBatch(count));
    const auto shape_indices = makeShapeBinIndexedOrder(count);
    const auto shape_frames = pcl::make_shared<pcl::PointCloud<pcl::ReferenceFrame>>();
    shape_frames->resize(1);
    (*shape_frames)[0].z_axis[0] = 0.11f;
    (*shape_frames)[0].z_axis[1] = -0.07f;
    (*shape_frames)[0].z_axis[2] = 1.0f;

    ShotShapeBinDirectProbe<pcl::PointXYZ, pcl::Normal, pcl::SHOT352> shot;
    shot.setInputNormals(normals);
    shot.setInputReferenceFrames(shape_frames);

    std::vector<double> out(shape_indices.size());
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runProductionShapeBinDirectBatch(shot, shape_indices, out);
    }, checksum);
    printCase("production_shape_bin_direct", ms, checksum);
  }

  if (caseEnabled(options, "interpolation_geometry_component")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    const auto surface = makeInterpolationSurfaceBatch(count);
    const auto interpolation_indices = makeInterpolationIndexedOrder(count);
    std::vector<float> sqr_dists(count);
    std::vector<double> bin_distance(count);
    for (std::size_t i = 0; i < count; ++i) {
      sqr_dists[i] = 0.0004f + static_cast<float>(i % 23) * 0.000071f;
      bin_distance[i] = 2.0 + static_cast<double>(i % 9) * 0.125;
    }
    std::vector<double> out_x(count), out_y(count), out_z(count), out_distance(count);
    std::vector<std::uint8_t> out_valid(count);
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runInterpolationGeometryBatch(surface,
                                           interpolation_indices,
                                           sqr_dists,
                                           bin_distance,
                                           out_x,
                                           out_y,
                                           out_z,
                                           out_distance,
                                           out_valid);
    }, checksum);
    printCase("interpolation_geometry_component", ms, checksum);
  }

  if (caseEnabled(options, "interpolation_bin_selection_component")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    std::vector<double> x(count), y(count), z(count), distance(count), bin_distance(count);
    makeInterpolationBinSelectionBatch(x, y, z, distance, bin_distance);
    std::vector<std::int32_t> desc_index(count), step_index(count), adjacent_index(count);
    std::vector<float> adjacent_delta(count), center_weight(count);
    std::vector<std::uint8_t> valid(count);
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runInterpolationBinSelectionBatch(x,
                                               y,
                                               z,
                                               distance,
                                               bin_distance,
                                               desc_index,
                                               step_index,
                                               adjacent_index,
                                               adjacent_delta,
                                               center_weight,
                                               valid);
    }, checksum);
    printCase("interpolation_bin_selection_component", ms, checksum);
  }

  if (caseEnabled(options, "color_lab_distance_component")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    std::vector<float> l(count), a(count), b(count);
    std::vector<double> out(count);
    makeColorLabBatch(l, a, b);
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runColorLabDistanceBatch(l, a, b, out);
    }, checksum);
    printCase("color_lab_distance_component", ms, checksum);
  }

  if (caseEnabled(options, "color_rgb_lut_indexed_component")) {
    const std::size_t count = static_cast<std::size_t>(options.side * options.side * 64);
    const auto color_surface = pcl_rvv_shot::makeShotColorCloud(static_cast<int>(std::sqrt(count)));
    pcl::Indices color_indices(count);
    for (std::size_t i = 0; i < count; ++i)
      color_indices[i] = static_cast<int>((i * 43 + (i / 5) * 17) % color_surface->size());
    std::vector<double> out(color_indices.size());
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      return runColorRgbLutIndexedBatch(*color_surface, color_indices, out);
    }, checksum);
    printCase("color_rgb_lut_indexed_component", ms, checksum);
  }

  return 0;
}
