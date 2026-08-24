/*
 * 本文件做什么：
 * 这是 transformation_estimation_svd_scale 的 QEMU bench smoke（仿真器性能测试小型验证）
 * 和 board bench（板卡性能测试）入口。它比较 production public scale path（生产公开
 * scale 路径）与 test-only fused scale candidate（测试专用融合 scale 候选）的日志形状、
 * checksum（校验和）和板卡耗时。
 *
 * 证据边界：
 * QEMU timing（QEMU 计时）只证明构建、路径和输出格式；真实性能结论必须来自 board /
 * target hardware repeated benchmark（板卡或目标硬件重复性能测试）。
 */

#include "tesvd_scale.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace support = pcl::registration::rvv_tesvd_scale_support;

namespace {

constexpr int kDefaultIterations = 20;
constexpr int kDefaultWarmupIterations = 3;
constexpr std::size_t kBannerWidth = 96;

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

void
printBanner(const char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

int
parseIntArg(const int argc, char** argv, const std::string& flag, const int fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == flag)
      return std::atoi(argv[i + 1]);
  }
  return fallback;
}

bool
caseEnabled(const int argc, char** argv, const std::string& requested)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--case-filter")
      return std::string(argv[i + 1]) == requested || std::string(argv[i + 1]) == "all";
  }
  return true;
}

void
runCase(const std::string& name,
        const int iterations,
        const int warmup_iterations,
        const std::function<std::uint64_t()>& fn,
        const std::string& detail = {})
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (int i = 0; i < warmup_iterations; ++i) {
    const std::uint64_t value = fn();
    checksum = (checksum ^ (value + static_cast<std::uint64_t>(i + 1))) *
               1099511628211ull;
  }

  const auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    const std::uint64_t value = fn();
    checksum = (checksum ^ (value + static_cast<std::uint64_t>(warmup_iterations + i + 1))) *
               1099511628211ull;
  }
  const auto end = std::chrono::high_resolution_clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

  std::cout << std::left << std::setw(60) << name << ": " << std::fixed
            << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
  std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
            << " ms, checksum: " << checksum << detail << '\n';
  doNotOptimize(checksum);
}

float
maxMatrixAbsDiff(const Eigen::Matrix4f& lhs, const Eigen::Matrix4f& rhs)
{
  float max_error = 0.0f;
  for (int r = 0; r < lhs.rows(); ++r) {
    for (int c = 0; c < lhs.cols(); ++c) {
      max_error = std::max(max_error, std::abs(lhs(r, c) - rhs(r, c)));
    }
  }
  return max_error;
}

double
maxMatrixAbsDiff(const Eigen::Matrix4d& lhs, const Eigen::Matrix4d& rhs)
{
  double max_error = 0.0;
  for (int r = 0; r < lhs.rows(); ++r) {
    for (int c = 0; c < lhs.cols(); ++c) {
      max_error = std::max(max_error, std::abs(lhs(r, c) - rhs(r, c)));
    }
  }
  return max_error;
}

void
runPublicScaleCase(const std::string& label,
                   const pcl::PointCloud<pcl::PointXYZ>& source,
                   const pcl::PointCloud<pcl::PointXYZ>& target,
                   const int iterations,
                   const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::estimateScalePublic(source, target));
  });
}

void
runScalarDoubleDiagnosticScoutCase(const std::string& label,
                                   const pcl::PointCloud<pcl::PointXYZ>& source,
                                   const pcl::PointCloud<pcl::PointXYZ>& target,
                                   const int iterations,
                                   const int warmup_iterations)
{
  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
      public_estimator;
  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  public_estimator.estimateRigidTransformation(source, target, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4d candidate_matrix =
      support::estimateScaleRVVDouble(source, target, &stats);
  std::ostringstream detail;
  detail << ", max_public_error: " << std::scientific << std::setprecision(6)
         << maxMatrixAbsDiff(candidate_matrix, public_matrix)
         << ", path: " << (stats.used_rvv ? "rvv-f64-widened" : "scalar-double-fallback");
  runCase(label, iterations, warmup_iterations, [&]() {
    support::CandidateStats iteration_stats;
    return support::matrixChecksum(
        support::estimateScaleRVVDouble(source, target, &iteration_stats));
  }, detail.str());
}

void
runScalarDoubleProductionProbeCase(const std::string& label,
                                   const pcl::PointCloud<pcl::PointXYZ>& source,
                                   const pcl::PointCloud<pcl::PointXYZ>& target,
                                   const int iterations,
                                   const int warmup_iterations)
{
  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
      public_estimator;
  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  public_estimator.estimateRigidTransformation(source, target, public_matrix);
  const Eigen::Matrix4d scalar_double_reference =
      support::estimateScaleStdDouble(source, target);

  std::ostringstream detail;
  detail << ", max_public_error: " << std::scientific << std::setprecision(6)
         << maxMatrixAbsDiff(public_matrix, scalar_double_reference)
#ifdef __RVV10__
         << ", path: public-double-rvv-f64-widened-probe";
#else
         << ", path: public-double-scalar-fallback";
#endif
  runCase(label, iterations, warmup_iterations, [&]() {
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    public_estimator.estimateRigidTransformation(source, target, matrix);
    return support::matrixChecksum(matrix);
  }, detail.str());
}

template <typename PointSource, typename PointTarget>
void
runGenericScalarDoubleOrderedProductionProbeCase(
    const std::string& label,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const int iterations,
    const int warmup_iterations)
{
  pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, double>
      public_estimator;
  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  public_estimator.estimateRigidTransformation(source, target, public_matrix);
  const Eigen::Matrix4d scalar_double_reference =
      support::estimateScaleStdDouble(source, target);

  std::ostringstream detail;
  detail << ", max_public_error: " << std::scientific << std::setprecision(6)
         << maxMatrixAbsDiff(public_matrix, scalar_double_reference)
#ifdef __RVV10__
         << ", path: public-generic-double-rvv-f64-widened-probe";
#else
         << ", path: public-generic-double-scalar-fallback";
#endif
  runCase(label, iterations, warmup_iterations, [&]() {
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    public_estimator.estimateRigidTransformation(source, target, matrix);
    return support::matrixChecksum(matrix);
  }, detail.str());
}

void
runScalarDoubleRowSourceProductionProbeSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
      public_estimator;
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const auto selected_source_target =
      support::transformCloudXYZ(selected_source, support::makeSimilarityTransform());

  const auto run_public = [&](const std::string& label,
                              const Eigen::Matrix4d& reference_matrix,
                              const std::function<void(Eigen::Matrix4d&)>& estimate) {
    Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
    estimate(public_matrix);
    std::ostringstream detail;
    detail << ", max_public_error: " << std::scientific << std::setprecision(6)
           << maxMatrixAbsDiff(public_matrix, reference_matrix)
#ifdef __RVV10__
           << ", path: public-double-row-source-rvv-f64-widened-probe";
#else
           << ", path: public-double-row-source-scalar-fallback";
#endif
    runCase(label, iterations, warmup_iterations, [&]() {
      Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
      estimate(matrix);
      return support::matrixChecksum(matrix);
    }, detail.str());
  };

  run_public("scalar double row source production probe source-indexed " + size_label,
             support::estimateScaleStdDouble(selected_source, selected_source_target),
             [&](Eigen::Matrix4d& matrix) {
               public_estimator.estimateRigidTransformation(
                   source, source_indices, selected_source_target, matrix);
             });
  run_public("scalar double row source production probe dual-indexed " + size_label,
             support::estimateScaleStdDouble(selected_source, selected_target),
             [&](Eigen::Matrix4d& matrix) {
               public_estimator.estimateRigidTransformation(
                   source, source_indices, target, target_indices, matrix);
             });
  run_public("scalar double row source production probe correspondence " + size_label,
             support::estimateScaleStdDouble(selected_source, selected_target),
             [&](Eigen::Matrix4d& matrix) {
               const pcl::Correspondences correspondences =
                   support::makeCorrespondences(source_indices, target_indices);
               public_estimator.estimateRigidTransformation(
                   source, target, correspondences, matrix);
             });
}

template <typename PointSource, typename PointTarget>
void
runGenericScalarDoubleRowSourceProductionProbeSet(
    const std::string& point_type_label,
    const std::string& size_label,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, double>
      public_estimator;
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const auto selected_source_target =
      support::transformCloudXYZTo<PointSource, PointTarget>(
          selected_source, support::makeSimilarityTransform());

  const auto run_public = [&](const std::string& label,
                              const Eigen::Matrix4d& reference_matrix,
                              const std::function<void(Eigen::Matrix4d&)>& estimate) {
    Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
    estimate(public_matrix);
    std::ostringstream detail;
    detail << ", max_public_error: " << std::scientific << std::setprecision(6)
           << maxMatrixAbsDiff(public_matrix, reference_matrix)
#ifdef __RVV10__
           << ", path: public-generic-double-row-source-rvv-f64-widened-probe";
#else
           << ", path: public-generic-double-row-source-scalar-fallback";
#endif
    runCase(label, iterations, warmup_iterations, [&]() {
      Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
      estimate(matrix);
      return support::matrixChecksum(matrix);
    }, detail.str());
  };

  run_public("row source generic scalar double production probe source-indexed " +
                 point_type_label + " " + size_label,
             support::estimateScaleStdDouble(selected_source, selected_source_target),
             [&](Eigen::Matrix4d& matrix) {
               public_estimator.estimateRigidTransformation(
                   source, source_indices, selected_source_target, matrix);
             });
  run_public("row source generic scalar double production probe dual-indexed " +
                 point_type_label + " " + size_label,
             support::estimateScaleStdDouble(selected_source, selected_target),
             [&](Eigen::Matrix4d& matrix) {
               public_estimator.estimateRigidTransformation(
                   source, source_indices, target, target_indices, matrix);
             });
  run_public("row source generic scalar double production probe correspondence " +
                 point_type_label + " " + size_label,
             support::estimateScaleStdDouble(selected_source, selected_target),
             [&](Eigen::Matrix4d& matrix) {
               const pcl::Correspondences correspondences =
                   support::makeCorrespondences(source_indices, target_indices);
               public_estimator.estimateRigidTransformation(
                   source, target, correspondences, matrix);
             });
}

template <typename PointSource, typename PointTarget>
void
runGenericPublicScaleCase(const std::string& label,
                          const pcl::PointCloud<PointSource>& source,
                          const pcl::PointCloud<PointTarget>& target,
                          const int iterations,
                          const int warmup_iterations)
{
  runCase(label, iterations, warmup_iterations, [&]() {
    return support::matrixChecksum(support::estimateScalePublic(source, target));
  });
}

template <typename PointSource, typename PointTarget>
void
runRowSourceScaleCase(const std::string& label,
                      const pcl::PointCloud<PointSource>& source,
                      const pcl::PointCloud<PointTarget>& target,
                      const pcl::Indices& source_indices,
                      const pcl::Indices& target_indices,
                      const bool use_correspondences,
                      const int iterations,
                      const int warmup_iterations)
{
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(selected_source, selected_target);
  pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, float>
      estimator;
  std::ostringstream detail;
  detail << ", max_reference_error: " << std::scientific << std::setprecision(6)
         << maxMatrixAbsDiff(reference_matrix,
                             use_correspondences
                                 ? [&]() {
                                     const pcl::Correspondences correspondences =
                                         support::makeCorrespondences(source_indices,
                                                                      target_indices);
                                     Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
                                     estimator.estimateRigidTransformation(source,
                                                                           target,
                                                                           correspondences,
                                                                           matrix);
                                     return matrix;
                                   }()
                                 : source_indices == target_indices
                                 ? [&]() {
                                     Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
                                     estimator.estimateRigidTransformation(source,
                                                                           source_indices,
                                                                           selected_target,
                                                                           matrix);
                                     return matrix;
                                   }()
                                 : [&]() {
                                     Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
                                     estimator.estimateRigidTransformation(source,
                                                                           source_indices,
                                                                           target,
                                                                           target_indices,
                                                                           matrix);
                                     return matrix;
                                   }());
  runCase(label, iterations, warmup_iterations, [&]() {
    Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
    if (use_correspondences) {
      const pcl::Correspondences correspondences =
          support::makeCorrespondences(source_indices, target_indices);
      estimator.estimateRigidTransformation(source, target, correspondences, matrix);
    }
    else if (source_indices == target_indices) {
      estimator.estimateRigidTransformation(source, source_indices, selected_target, matrix);
    }
    else {
      estimator.estimateRigidTransformation(source, source_indices, target, target_indices, matrix);
    }
    return support::matrixChecksum(matrix);
  }, detail.str());
}

template <typename PointSource, typename PointTarget>
void
runRowSourceAllPolicyScaleCases(const std::string& point_type_label,
                                const pcl::PointCloud<PointSource>& source_small,
                                const pcl::PointCloud<PointTarget>& target_small,
                                const pcl::PointCloud<PointSource>& source_medium,
                                const pcl::PointCloud<PointTarget>& target_medium,
                                const pcl::PointCloud<PointSource>& source_large,
                                const pcl::PointCloud<PointTarget>& target_large,
                                const pcl::Indices& source_small_indices,
                                const pcl::Indices& target_small_indices,
                                const pcl::Indices& source_medium_indices,
                                const pcl::Indices& target_medium_indices,
                                const pcl::Indices& source_large_indices,
                                const pcl::Indices& target_large_indices,
                                const int iterations,
                                const int warmup_iterations)
{
  const auto run_size = [&](const std::string& size_label,
                            const pcl::PointCloud<PointSource>& source,
                            const pcl::PointCloud<PointTarget>& target,
                            const pcl::Indices& source_indices,
                            const pcl::Indices& target_indices) {
    runRowSourceScaleCase("row source scale source-indexed " + point_type_label +
                              " " + size_label,
                          source,
                          target,
                          source_indices,
                          source_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale dual-indexed " + point_type_label + " " +
                              size_label,
                          source,
                          target,
                          source_indices,
                          target_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale correspondence " + point_type_label + " " +
                              size_label,
                          source,
                          target,
                          source_indices,
                          target_indices,
                          true,
                          iterations,
                          warmup_iterations);
  };

  run_size("4K", source_small, target_small, source_small_indices, target_small_indices);
  run_size(
      "64K", source_medium, target_medium, source_medium_indices, target_medium_indices);
  run_size("256K", source_large, target_large, source_large_indices, target_large_indices);
}

template <typename PointSource, typename PointTarget>
void
runRowSourceAllMoreGenericMatrixCombo(const std::string& point_type_label,
                                      const Eigen::Matrix4f& transform,
                                      const pcl::Indices& source_small_indices,
                                      const pcl::Indices& target_small_indices,
                                      const pcl::Indices& source_medium_indices,
                                      const pcl::Indices& target_medium_indices,
                                      const pcl::Indices& source_large_indices,
                                      const pcl::Indices& target_large_indices,
                                      const int iterations,
                                      const int warmup_iterations)
{
  const auto source_small = support::makePointCloudXYZ<PointSource>(8192);
  const auto source_medium = support::makePointCloudXYZ<PointSource>(131072);
  const auto source_large = support::makePointCloudXYZ<PointSource>(524288);
  const auto target_small =
      support::transformCloudXYZTo<PointSource, PointTarget>(source_small, transform);
  const auto target_medium =
      support::transformCloudXYZTo<PointSource, PointTarget>(source_medium, transform);
  const auto target_large =
      support::transformCloudXYZTo<PointSource, PointTarget>(source_large, transform);
  runRowSourceAllPolicyScaleCases(point_type_label,
                                  source_small,
                                  target_small,
                                  source_medium,
                                  target_medium,
                                  source_large,
                                  target_large,
                                  source_small_indices,
                                  target_small_indices,
                                  source_medium_indices,
                                  target_medium_indices,
                                  source_large_indices,
                                  target_large_indices,
                                  iterations,
                                  warmup_iterations);
}

void
runFusedScaleCase(const std::string& label,
                  const pcl::PointCloud<pcl::PointXYZ>& source,
                  const pcl::PointCloud<pcl::PointXYZ>& target,
                  const int iterations,
                  const int warmup_iterations)
{
  support::CandidateStats stats;
  const Eigen::Matrix4f reference_matrix = support::estimateScaleStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateScaleCandidate(source, target, &stats);
  std::ostringstream detail;
  detail << ", max_reference_error: " << std::scientific << std::setprecision(6)
         << maxMatrixAbsDiff(candidate_matrix, reference_matrix);
  runCase(label, iterations, warmup_iterations, [&]() {
    const Eigen::Matrix4f matrix = support::estimateScaleCandidate(source, target, &stats);
    return support::matrixChecksum(matrix);
  }, detail.str());
}

void
runMatrixLocalScaleCase(const std::string& label,
                        const pcl::PointCloud<pcl::PointXYZ>& source,
                        const pcl::PointCloud<pcl::PointXYZ>& target,
                        const int iterations,
                        const int warmup_iterations)
{
  const Eigen::Matrix4f legacy_matrix =
      support::estimateScaleMatrixLocalLegacy(source, target);
  const Eigen::Matrix4f trace_matrix =
      support::estimateScaleMatrixLocalTrace(source, target);
  std::ostringstream detail;
  detail << ", max_reference_error: " << std::scientific << std::setprecision(6)
         << maxMatrixAbsDiff(trace_matrix, legacy_matrix);
  runCase(label, iterations, warmup_iterations, [&]() {
#if defined(__RVV10__)
    return support::matrixChecksum(support::estimateScaleMatrixLocalTrace(source, target));
#else
    return support::matrixChecksum(support::estimateScaleMatrixLocalLegacy(source, target));
#endif
  }, detail.str());
}

void
runRowSourceOrderProfileSet(const std::string& size_label,
                            const pcl::PointCloud<pcl::PointXYZ>& source,
                            const pcl::PointCloud<pcl::PointXYZ>& target,
                            const pcl::Indices& source_indices,
                            const pcl::Indices& target_indices,
                            const std::string& order_label,
                            const int iterations,
                            const int warmup_iterations)
{
  runRowSourceScaleCase("row source scale source-indexed locality-" + order_label + " " +
                            size_label,
                        source,
                        target,
                        source_indices,
                        source_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("row source scale dual-indexed locality-" + order_label + " " +
                            size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("row source scale correspondence locality-" + order_label + " " +
                            size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        true,
                        iterations,
                        warmup_iterations);
}

template <typename PointSource, typename PointTarget>
void
runCustomRowSourceLargeVarianceProfileSet(
    const std::string& size_label,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const std::string& order_label,
    const int iterations,
    const int warmup_iterations)
{
  runRowSourceScaleCase("custom row source scale dual-indexed locality-" + order_label +
                            " LocalPaddedXYZSource->LocalWideXYZTarget " + size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("custom row source scale correspondence locality-" + order_label +
                            " LocalPaddedXYZSource->LocalWideXYZTarget " + size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        true,
                        iterations,
                        warmup_iterations);
}

template <typename PointSource, typename PointTarget>
void
runCustomLayoutPaddingSensitivitySet(
    const std::string& point_type_label,
    const std::string& size_label,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  runRowSourceScaleCase("custom padding row source scale source-indexed " +
                            point_type_label + " " + size_label,
                        source,
                        target,
                        source_indices,
                        source_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("custom padding row source scale dual-indexed " +
                            point_type_label + " " + size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("custom padding row source scale correspondence " +
                            point_type_label + " " + size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        true,
                        iterations,
                        warmup_iterations);
}

template <typename PointSource, typename PointTarget>
void
runCustomLayoutAlignmentSensitivitySet(
    const std::string& point_type_label,
    const std::string& size_label,
    const pcl::PointCloud<PointSource>& source,
    const pcl::PointCloud<PointTarget>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  runRowSourceScaleCase("custom alignment row source scale source-indexed " +
                            point_type_label + " " + size_label,
                        source,
                        target,
                        source_indices,
                        source_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("custom alignment row source scale dual-indexed " +
                            point_type_label + " " + size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("custom alignment row source scale correspondence " +
                            point_type_label + " " + size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        true,
                        iterations,
                        warmup_iterations);
}

void
runRowSourceShuffleSortedCopyDetailABSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations,
    const bool include_dual_indexed = true,
    const bool include_correspondence = true)
{
  const auto run_current = [&](const std::string& row_source_label,
                               const bool use_correspondences) {
    runRowSourceScaleCase("row source shuffle current " + row_source_label + " " +
                              size_label,
                          source,
                          target,
                          source_indices,
                          target_indices,
                          use_correspondences,
                          iterations,
                          warmup_iterations);
  };
  const auto run_sorted_copy = [&](const std::string& row_source_label,
                                   const bool use_correspondences) {
    const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
    const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
    const Eigen::Matrix4f reference_matrix =
        support::estimateScaleStd(selected_source, selected_target);
    pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ,
                                                        pcl::PointXYZ,
                                                        float>
        estimator;
    std::ostringstream detail;
    detail << ", max_reference_error: " << std::scientific << std::setprecision(6)
           << maxMatrixAbsDiff(reference_matrix,
                               [&]() {
                                 Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
                                 if (use_correspondences) {
                                   const pcl::Correspondences sorted_correspondences =
                                       support::makeSortedCorrespondencesByQueryIndex(
                                           support::makeCorrespondences(source_indices,
                                                                        target_indices));
                                   estimator.estimateRigidTransformation(source,
                                                                         target,
                                                                         sorted_correspondences,
                                                                         matrix);
                                 }
                                 else {
                                   const auto sorted_pairs =
                                       support::makeSortedIndexPairsBySource(source_indices,
                                                                             target_indices);
                                   estimator.estimateRigidTransformation(source,
                                                                         sorted_pairs.first,
                                                                         target,
                                                                         sorted_pairs.second,
                                                                         matrix);
                                 }
                                 return matrix;
                               }());
    runCase("row source shuffle sorted-copy " + row_source_label + " " + size_label,
            iterations,
            warmup_iterations,
            [&]() {
              Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
              if (use_correspondences) {
                const pcl::Correspondences sorted_correspondences =
                    support::makeSortedCorrespondencesByQueryIndex(
                        support::makeCorrespondences(source_indices, target_indices));
                estimator.estimateRigidTransformation(source,
                                                      target,
                                                      sorted_correspondences,
                                                      matrix);
              }
              else {
                const auto sorted_pairs =
                    support::makeSortedIndexPairsBySource(source_indices, target_indices);
                estimator.estimateRigidTransformation(source,
                                                      sorted_pairs.first,
                                                      target,
                                                      sorted_pairs.second,
                                                      matrix);
              }
              return support::matrixChecksum(matrix);
            },
            detail.str());
  };

  if (include_dual_indexed) {
    run_current("dual-indexed", false);
    run_sorted_copy("dual-indexed", false);
  }
  if (include_correspondence) {
    run_current("correspondence", true);
    run_sorted_copy("correspondence", true);
  }
}

void
runRowSourceAffineIndexFastPathDetailABSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  const std::size_t source_offset = static_cast<std::size_t>(source_indices.front());
  const std::size_t target_offset = static_cast<std::size_t>(target_indices.front());
  const std::size_t count = source_indices.size();

  const auto run_current = [&](const std::string& row_source_label,
                               const pcl::Indices& current_target_indices,
                               const bool use_correspondences) {
    runRowSourceScaleCase("row source affine current " + row_source_label + " " +
                              size_label,
                          source,
                          target,
                          source_indices,
                          current_target_indices,
                          use_correspondences,
                          iterations,
                          warmup_iterations);
  };
  const auto run_fast_path = [&](const std::string& row_source_label,
                                 const pcl::PointCloud<pcl::PointXYZ>& candidate_target,
                                 const std::size_t candidate_target_offset,
                                 const pcl::PointCloud<pcl::PointXYZ>& reference_target,
                                 const pcl::Indices& reference_target_indices) {
    const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
    const auto selected_target =
        support::selectPointCloudByIndices(reference_target, reference_target_indices);
    const Eigen::Matrix4f reference_matrix =
        support::estimateScaleStd(selected_source, selected_target);
    std::ostringstream detail;
    detail << ", max_reference_error: " << std::scientific << std::setprecision(6)
           << maxMatrixAbsDiff(reference_matrix,
                               support::estimateScaleContiguousOffsetCandidate(
                                   source,
                                   source_offset,
                                   candidate_target,
                                   candidate_target_offset,
                                   count));
    runCase("row source affine fast-path " + row_source_label + " " + size_label,
            iterations,
            warmup_iterations,
            [&]() {
              return support::matrixChecksum(
                  support::estimateScaleContiguousOffsetCandidate(source,
                                                                  source_offset,
                                                                  candidate_target,
                                                                  candidate_target_offset,
                                                                  count));
            },
            detail.str());
  };

  const auto selected_target_same =
      support::selectPointCloudByIndices(target, source_indices);
  run_current("source-indexed", source_indices, false);
  run_fast_path("source-indexed", selected_target_same, 0, target, source_indices);

  run_current("dual-indexed", target_indices, false);
  run_fast_path("dual-indexed", target, target_offset, target, target_indices);

  run_current("correspondence", target_indices, true);
  run_fast_path("correspondence", target, target_offset, target, target_indices);
}

void
runRowSourceAffineIndexFastPathProductionProbeSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  runRowSourceScaleCase("row source affine production probe source-indexed " +
                            size_label,
                        source,
                        target,
                        source_indices,
                        source_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("row source affine production probe dual-indexed " +
                            size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        false,
                        iterations,
                        warmup_iterations);
  runRowSourceScaleCase("row source affine production probe correspondence " +
                            size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        true,
                        iterations,
                        warmup_iterations);
}

void
runRowSourceShuffleTargetSortedDetailABSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  const auto run_current = [&](const std::string& row_source_label) {
    runRowSourceScaleCase("row source shuffle current " + row_source_label + " " +
                              size_label,
                          source,
                          target,
                          source_indices,
                          target_indices,
                          false,
                          iterations,
                          warmup_iterations);
  };
  const auto run_target_sorted = [&](const std::string& row_source_label) {
    const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
    const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
    const Eigen::Matrix4f reference_matrix =
        support::estimateScaleStd(selected_source, selected_target);
    pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ,
                                                        pcl::PointXYZ,
                                                        float>
        estimator;
    std::ostringstream detail;
    detail << ", max_reference_error: " << std::scientific << std::setprecision(6)
           << maxMatrixAbsDiff(reference_matrix,
                               [&]() {
                                 Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
                                 const auto sorted_pairs =
                                     support::makeSortedIndexPairsByTarget(source_indices,
                                                                           target_indices);
                                 estimator.estimateRigidTransformation(source,
                                                                       sorted_pairs.first,
                                                                       target,
                                                                       sorted_pairs.second,
                                                                       matrix);
                                 return matrix;
                               }());
    runCase("row source shuffle target-sorted " + row_source_label + " " + size_label,
            iterations,
            warmup_iterations,
            [&]() {
              Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
              const auto sorted_pairs =
                  support::makeSortedIndexPairsByTarget(source_indices, target_indices);
              estimator.estimateRigidTransformation(source,
                                                    sorted_pairs.first,
                                                    target,
                                                    sorted_pairs.second,
                                                    matrix);
              return support::matrixChecksum(matrix);
            },
            detail.str());
  };

  run_current("dual-indexed");
  run_target_sorted("dual-indexed");
}

void
runRowSourceShuffleStagedSelectedCloudDetailABSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  const auto run_current = [&](const std::string& row_source_label,
                               const bool use_correspondences) {
    runRowSourceScaleCase("row source shuffle current " + row_source_label + " " +
                              size_label,
                          source,
                          target,
                          source_indices,
                          target_indices,
                          use_correspondences,
                          iterations,
                          warmup_iterations);
  };
  const auto run_staged = [&](const std::string& row_source_label) {
    const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
    const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
    const Eigen::Matrix4f reference_matrix =
        support::estimateScaleStd(selected_source, selected_target);
    std::ostringstream detail;
    detail << ", max_reference_error: " << std::scientific << std::setprecision(6)
           << maxMatrixAbsDiff(reference_matrix,
                               support::estimateScalePublic(selected_source,
                                                            selected_target));
    runCase("row source shuffle staged-selected-cloud " + row_source_label + " " +
                size_label,
            iterations,
            warmup_iterations,
            [&]() {
              const auto staged_source =
                  support::selectPointCloudByIndices(source, source_indices);
              const auto staged_target =
                  support::selectPointCloudByIndices(target, target_indices);
              return support::matrixChecksum(
                  support::estimateScalePublic(staged_source, staged_target));
            },
            detail.str());
  };

  run_current("dual-indexed", false);
  run_staged("dual-indexed");
  run_current("correspondence", true);
  run_staged("correspondence");
}

void
runCorrespondenceSortedCopyProductionProbeSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  runRowSourceScaleCase("row source correspondence sorted-copy production probe " +
                            size_label,
                        source,
                        target,
                        source_indices,
                        target_indices,
                        true,
                        iterations,
                        warmup_iterations);
}

void
runCorrespondenceSortedCopyScalarDoubleProductionProbeSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const Eigen::Matrix4d reference_matrix =
      support::estimateScaleStdDouble(selected_source, selected_target);
  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
      estimator;
  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);

  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, public_matrix);
  std::ostringstream detail;
  detail << ", max_public_error: " << std::scientific << std::setprecision(6)
         << maxMatrixAbsDiff(public_matrix, reference_matrix)
#ifdef __RVV10__
         << ", path: public-double-correspondence-sorted-copy-rvv-f64-widened-probe";
#else
         << ", path: public-double-correspondence-sorted-copy-scalar-fallback";
#endif

  runCase("row source correspondence sorted-copy scalar double production probe " +
              size_label,
          iterations,
          warmup_iterations,
          [&]() {
            Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
            estimator.estimateRigidTransformation(source, target, correspondences, matrix);
            return support::matrixChecksum(matrix);
          },
          detail.str());
}

void
runCorrespondenceSortedCopyScalarDoubleDetailABSet(
    const std::string& size_label,
    const pcl::PointCloud<pcl::PointXYZ>& source,
    const pcl::PointCloud<pcl::PointXYZ>& target,
    const pcl::Indices& source_indices,
    const pcl::Indices& target_indices,
    const int iterations,
    const int warmup_iterations)
{
#ifdef __RVV10__
  using Layout = pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZ>;

  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const Eigen::Matrix4d reference_matrix =
      support::estimateScaleStdDouble(selected_source, selected_target);

  const auto run_baseline = [&]() {
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    pcl::registration::detail::solveTransformationEstimationSVDScaleD64(
        pcl::registration::detail::
            accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV<
                pcl::PointXYZ,
                pcl::PointXYZ,
                Layout,
                Layout>(source, target, correspondences),
        matrix);
    return matrix;
  };
  const auto run_candidate = [&]() {
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    const bool handled = pcl::registration::detail::
        estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV<
            pcl::PointXYZ,
            pcl::PointXYZ,
            Layout,
            Layout>(source, target, correspondences, matrix);
    if (!handled) {
      std::cerr << "sorted-copy scalar-double detail A/B gate did not handle "
                << size_label << '\n';
      std::exit(2);
    }
    return matrix;
  };

  std::ostringstream baseline_detail;
  baseline_detail << ", max_reference_error: " << std::scientific
                  << std::setprecision(6)
                  << maxMatrixAbsDiff(run_baseline(), reference_matrix);
  runCase("row source correspondence scalar double d64 gather detail " + size_label,
          iterations,
          warmup_iterations,
          [&]() { return support::matrixChecksum(run_baseline()); },
          baseline_detail.str());

  std::ostringstream candidate_detail;
  candidate_detail << ", max_reference_error: " << std::scientific
                   << std::setprecision(6)
                   << maxMatrixAbsDiff(run_candidate(), reference_matrix);
  runCase("row source correspondence sorted-copy scalar double detail " + size_label,
          iterations,
          warmup_iterations,
          [&]() { return support::matrixChecksum(run_candidate()); },
          candidate_detail.str());
#else
  (void)size_label;
  (void)source;
  (void)target;
  (void)source_indices;
  (void)target_indices;
  (void)iterations;
  (void)warmup_iterations;
#endif
}

} // namespace

int
main(int argc, char** argv)
{
  const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);
  const int warmup_iterations =
      parseIntArg(argc, argv, "--warmup-iterations", kDefaultWarmupIterations);
  const auto transform = support::makeSimilarityTransform();
  const auto small = support::makePointXYZCloud(4096);
  const auto medium = support::makePointXYZCloud(65536);
  const auto large = support::makePointXYZCloud(262144);
  const auto small_target = support::transformCloudXYZ(small, transform);
  const auto medium_target = support::transformCloudXYZ(medium, transform);
  const auto large_target = support::transformCloudXYZ(large, transform);
  const auto generic_xyzi = support::makePointCloudXYZ<pcl::PointXYZI>(4096);
  const auto generic_xyzi_large = support::makePointCloudXYZ<pcl::PointXYZI>(65536);
  const auto generic_xyzi_huge = support::makePointCloudXYZ<pcl::PointXYZI>(262144);
  const auto generic_xyzirgb =
      support::makePointCloudXYZ<pcl::PointXYZRGB>(4096);
  const auto generic_xyzirgb_large =
      support::makePointCloudXYZ<pcl::PointXYZRGB>(65536);
  const auto generic_xyzirgb_huge =
      support::makePointCloudXYZ<pcl::PointXYZRGB>(262144);
  const auto generic_xyzi_target = support::transformCloudXYZTo<pcl::PointXYZI,
                                                                 pcl::PointXYZI>(
      generic_xyzi, transform);
  const auto generic_xyzi_large_target =
      support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZI>(
          generic_xyzi_large, transform);
  const auto generic_xyzi_huge_target =
      support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZI>(
          generic_xyzi_huge, transform);
  const auto generic_xyzirgb_target = support::transformCloudXYZTo<pcl::PointXYZRGB,
                                                                    pcl::PointXYZRGB>(
      generic_xyzirgb, transform);
  const auto generic_xyzirgb_large_target =
      support::transformCloudXYZTo<pcl::PointXYZRGB, pcl::PointXYZRGB>(
          generic_xyzirgb_large, transform);
  const auto generic_xyzirgb_huge_target =
      support::transformCloudXYZTo<pcl::PointXYZRGB, pcl::PointXYZRGB>(
          generic_xyzirgb_huge, transform);
  const auto generic_xyzi_to_xyzirgb_target =
      support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZRGB>(
          generic_xyzi_large, transform);
  const auto generic_xyzirgb_to_xyz_target =
      support::transformCloudXYZTo<pcl::PointXYZRGB, pcl::PointXYZ>(
          generic_xyzirgb_large, transform);
  const auto row_source_small = support::makePointXYZCloud(8192);
  const auto row_source_medium = support::makePointXYZCloud(131072);
  const auto row_source_large = support::makePointXYZCloud(524288);
  const auto row_source_small_target = support::transformCloudXYZ(row_source_small, transform);
  const auto row_source_medium_target =
      support::transformCloudXYZ(row_source_medium, transform);
  const auto row_source_large_target =
      support::transformCloudXYZ(row_source_large, transform);
  const auto row_source_small_indices = support::makeStrideIndices(4096, 2, 1);
  const auto row_source_medium_indices = support::makeStrideIndices(65536, 2, 1);
  const auto row_source_large_indices = support::makeStrideIndices(262144, 2, 1);
  const auto row_source_small_alt_indices = support::makeStrideIndices(4096, 2, 0);
  const auto row_source_medium_alt_indices = support::makeStrideIndices(65536, 2, 0);
  const auto row_source_large_alt_indices = support::makeStrideIndices(262144, 2, 0);
  const auto row_source_small_contiguous_indices = support::makeStrideIndices(4096, 1, 0);
  const auto row_source_medium_contiguous_indices = support::makeStrideIndices(65536, 1, 0);
  const auto row_source_large_contiguous_indices = support::makeStrideIndices(262144, 1, 0);
  const auto row_source_small_contiguous_alt_indices = support::makeStrideIndices(4096, 1, 1);
  const auto row_source_medium_contiguous_alt_indices = support::makeStrideIndices(65536, 1, 1);
  const auto row_source_large_contiguous_alt_indices = support::makeStrideIndices(262144, 1, 1);
  const auto row_source_small_reverse_indices =
      support::makeReversedIndices(row_source_small_indices);
  const auto row_source_medium_reverse_indices =
      support::makeReversedIndices(row_source_medium_indices);
  const auto row_source_large_reverse_indices =
      support::makeReversedIndices(row_source_large_indices);
  const auto row_source_small_reverse_alt_indices =
      support::makeReversedIndices(row_source_small_alt_indices);
  const auto row_source_medium_reverse_alt_indices =
      support::makeReversedIndices(row_source_medium_alt_indices);
  const auto row_source_large_reverse_alt_indices =
      support::makeReversedIndices(row_source_large_alt_indices);
  const auto row_source_small_shuffle_indices =
      support::makeDeterministicShuffledIndices(row_source_small_indices);
  const auto row_source_medium_shuffle_indices =
      support::makeDeterministicShuffledIndices(row_source_medium_indices);
  const auto row_source_large_shuffle_indices =
      support::makeDeterministicShuffledIndices(row_source_large_indices);
  const auto row_source_small_shuffle_alt_indices =
      support::makeDeterministicShuffledIndices(row_source_small_alt_indices);
  const auto row_source_medium_shuffle_alt_indices =
      support::makeDeterministicShuffledIndices(row_source_medium_alt_indices);
  const auto row_source_large_shuffle_alt_indices =
      support::makeDeterministicShuffledIndices(row_source_large_alt_indices);
  printBanner('=');
  std::cout << "PCL registration/transformation_estimation_svd_scale RVV diagnostic\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic dense PointXYZ ordered-cloud-pair pairs; SVD scale similarity transform\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  printBanner('-');

  if (caseEnabled(argc, argv, "public-scale")) {
    runPublicScaleCase("public scale ordered-cloud-pair 4K",
                       small,
                       small_target,
                       iterations,
                       warmup_iterations);
    runPublicScaleCase("public scale ordered-cloud-pair 64K",
                       medium,
                       medium_target,
                       iterations,
                       warmup_iterations);
    runPublicScaleCase("public scale ordered-cloud-pair 256K",
                       large,
                       large_target,
                       iterations,
                       warmup_iterations);
  }

  if (caseEnabled(argc, argv, "ordered-cloud-pair")) {
    runFusedScaleCase("fused scale candidate ordered-cloud-pair 4K",
                      small,
                      small_target,
                      iterations,
                      warmup_iterations);
    runFusedScaleCase("fused scale candidate ordered-cloud-pair 64K",
                      medium,
                      medium_target,
                      iterations,
                      warmup_iterations);
    runFusedScaleCase("fused scale candidate ordered-cloud-pair 256K",
                      large,
                      large_target,
                      iterations,
                      warmup_iterations);
  }

  if (caseEnabled(argc, argv, "scalar-double-diagnostic-scout")) {
    runScalarDoubleDiagnosticScoutCase(
        "scalar double diagnostic scout ordered-cloud-pair 64K",
        medium,
        medium_target,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "scalar-double-production-probe")) {
    runScalarDoubleProductionProbeCase(
        "scalar double production probe ordered-cloud-pair 64K",
        medium,
        medium_target,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "generic-scalar-double-ordered-production-probe")) {
    runGenericScalarDoubleOrderedProductionProbeCase(
        "generic scalar double ordered production probe PointXYZI->PointXYZI 64K",
        generic_xyzi_large,
        generic_xyzi_large_target,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleOrderedProductionProbeCase(
        "generic scalar double ordered production probe PointXYZRGB->PointXYZRGB 64K",
        generic_xyzirgb_large,
        generic_xyzirgb_large_target,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleOrderedProductionProbeCase(
        "generic scalar double ordered production probe PointXYZI->PointXYZRGB 64K",
        generic_xyzi_large,
        generic_xyzi_to_xyzirgb_target,
        iterations,
        warmup_iterations);

    const auto xyzrgba = support::makePointCloudXYZ<pcl::PointXYZRGBA>(65536);
    const auto point_normal = support::makePointCloudXYZ<pcl::PointNormal>(65536);
    const auto xyzrgba_target =
        support::transformCloudXYZTo<pcl::PointXYZRGBA, pcl::PointXYZRGBA>(
            xyzrgba, transform);
    const auto point_normal_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointNormal, pcl::PointXYZRGB>(
            point_normal, transform);
    runGenericScalarDoubleOrderedProductionProbeCase(
        "generic scalar double ordered production probe PointXYZRGBA->PointXYZRGBA 64K",
        xyzrgba,
        xyzrgba_target,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleOrderedProductionProbeCase(
        "generic scalar double ordered production probe PointNormal->PointXYZRGB 64K",
        point_normal,
        point_normal_to_rgb_target,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "scalar-double-row-source-production-probe")) {
    runScalarDoubleRowSourceProductionProbeSet(
        "64K",
        row_source_medium,
        row_source_medium_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-generic-scalar-double-production-probe")) {
    runGenericScalarDoubleRowSourceProductionProbeSet(
        "PointXYZI->PointXYZI",
        "64K",
        generic_xyzi_huge,
        generic_xyzi_huge_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleRowSourceProductionProbeSet(
        "PointXYZRGB->PointXYZRGB",
        "64K",
        generic_xyzirgb_huge,
        generic_xyzirgb_huge_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleRowSourceProductionProbeSet(
        "PointXYZI->PointXYZRGB",
        "64K",
        generic_xyzi_huge,
        support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZRGB>(
            generic_xyzi_huge, transform),
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "custom-layout-scalar-double-diagnostic-scout")) {
    const auto custom_double_ordered_source =
        support::makePointCloudXYZ<LocalSVDScalePaddedXYZSource>(65536);
    const auto custom_double_ordered_target =
        support::transformCloudXYZTo<LocalSVDScalePaddedXYZSource,
                                     LocalSVDScaleWideXYZTarget>(
            custom_double_ordered_source, transform);
    const auto custom_double_source =
        support::makePointCloudXYZ<LocalSVDScalePaddedXYZSource>(131072);
    const auto custom_double_target =
        support::transformCloudXYZTo<LocalSVDScalePaddedXYZSource,
                                     LocalSVDScaleWideXYZTarget>(
            custom_double_source, transform);
    runGenericScalarDoubleOrderedProductionProbeCase(
        "custom layout scalar double scout ordered "
        "LocalPaddedXYZSource->LocalWideXYZTarget 64K",
        custom_double_ordered_source,
        custom_double_ordered_target,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleRowSourceProductionProbeSet(
        "LocalPaddedXYZSource->LocalWideXYZTarget",
        "64K",
        custom_double_source,
        custom_double_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "more-custom-layout-scalar-double-sampling")) {
    const auto compact_double_source =
        support::makePointCloudXYZ<LocalSVDScaleCompactXYZSource>(131072);
    const auto compact_double_target =
        support::transformCloudXYZTo<LocalSVDScaleCompactXYZSource,
                                     LocalSVDScaleCompactXYZTarget>(
            compact_double_source, transform);
    const auto huge_double_source =
        support::makePointCloudXYZ<LocalSVDScaleHugePaddingXYZSource>(131072);
    const auto huge_double_target =
        support::transformCloudXYZTo<LocalSVDScaleHugePaddingXYZSource,
                                     LocalSVDScaleHugePaddingXYZTarget>(
            huge_double_source, transform);
    const auto aligned_double_source =
        support::makePointCloudXYZ<LocalSVDScaleAligned64XYZSource>(131072);
    const auto aligned_double_target =
        support::transformCloudXYZTo<LocalSVDScaleAligned64XYZSource,
                                     LocalSVDScaleAligned32XYZTarget>(
            aligned_double_source, transform);

    runGenericScalarDoubleOrderedProductionProbeCase(
        "more custom layout scalar double sampling ordered "
        "LocalCompactXYZSource->LocalCompactXYZTarget 64K",
        compact_double_source,
        compact_double_target,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleRowSourceProductionProbeSet(
        "LocalCompactXYZSource->LocalCompactXYZTarget",
        "64K",
        compact_double_source,
        compact_double_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleOrderedProductionProbeCase(
        "more custom layout scalar double sampling ordered "
        "LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget 64K",
        huge_double_source,
        huge_double_target,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleRowSourceProductionProbeSet(
        "LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget",
        "64K",
        huge_double_source,
        huge_double_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleOrderedProductionProbeCase(
        "more custom layout scalar double sampling ordered "
        "LocalAligned64XYZSource->LocalAligned32XYZTarget 64K",
        aligned_double_source,
        aligned_double_target,
        iterations,
        warmup_iterations);
    runGenericScalarDoubleRowSourceProductionProbeSet(
        "LocalAligned64XYZSource->LocalAligned32XYZTarget",
        "64K",
        aligned_double_source,
        aligned_double_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv,
                  "correspondence-sorted-copy-scalar-double-production-probe")) {
    runCorrespondenceSortedCopyScalarDoubleProductionProbeSet(
        "64K",
        row_source_medium,
        row_source_medium_target,
        row_source_medium_shuffle_indices,
        row_source_medium_shuffle_alt_indices,
        iterations,
        warmup_iterations);
    runCorrespondenceSortedCopyScalarDoubleProductionProbeSet(
        "256K",
        row_source_large,
        row_source_large_target,
        row_source_large_shuffle_indices,
        row_source_large_shuffle_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "matrix-local-scale")) {
    runMatrixLocalScaleCase("matrix local scale simplification ordered-cloud-pair 4K",
                            small,
                            small_target,
                            iterations,
                            warmup_iterations);
    runMatrixLocalScaleCase("matrix local scale simplification ordered-cloud-pair 64K",
                            medium,
                            medium_target,
                            iterations,
                            warmup_iterations);
    runMatrixLocalScaleCase("matrix local scale simplification ordered-cloud-pair 256K",
                            large,
                            large_target,
                            iterations,
                            warmup_iterations);
  }

  if (caseEnabled(argc, argv, "generic-xyz-point-types-public")) {
    runGenericPublicScaleCase("generic public scale PointXYZI->PointXYZI 4K",
                              generic_xyzi,
                              generic_xyzi_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase("generic public scale PointXYZI->PointXYZI 64K",
                              generic_xyzi_large,
                              generic_xyzi_large_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase("generic public scale PointXYZI->PointXYZI 256K",
                              generic_xyzi_huge,
                              generic_xyzi_huge_target,
                              iterations,
                              warmup_iterations);

    runGenericPublicScaleCase("generic public scale PointXYZRGB->PointXYZRGB 4K",
                              generic_xyzirgb,
                              generic_xyzirgb_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase("generic public scale PointXYZRGB->PointXYZRGB 64K",
                              generic_xyzirgb_large,
                              generic_xyzirgb_large_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase("generic public scale PointXYZRGB->PointXYZRGB 256K",
                              generic_xyzirgb_huge,
                              generic_xyzirgb_huge_target,
                              iterations,
                              warmup_iterations);

    runGenericPublicScaleCase("generic public scale PointXYZI->PointXYZRGB 64K",
                              generic_xyzi_large,
                              generic_xyzi_to_xyzirgb_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase("generic public scale PointXYZRGB->PointXYZ 64K",
                              generic_xyzirgb_large,
                              generic_xyzirgb_to_xyz_target,
                              iterations,
                              warmup_iterations);
  }

  if (caseEnabled(argc, argv, "more-generic-xyz-aos-point-types-public")) {
    const auto xyzrgba = support::makePointCloudXYZ<pcl::PointXYZRGBA>(65536);
    const auto xyzl = support::makePointCloudXYZ<pcl::PointXYZL>(65536);
    const auto point_normal = support::makePointCloudXYZ<pcl::PointNormal>(65536);
    const auto point_with_range =
        support::makePointCloudXYZ<pcl::PointWithRange>(65536);
    const auto point_with_viewpoint =
        support::makePointCloudXYZ<pcl::PointWithViewpoint>(65536);
    const auto xyzrgba_target =
        support::transformCloudXYZTo<pcl::PointXYZRGBA, pcl::PointXYZRGBA>(
            xyzrgba, transform);
    const auto xyzl_to_xyz_target =
        support::transformCloudXYZTo<pcl::PointXYZL, pcl::PointXYZ>(xyzl,
                                                                     transform);
    const auto point_normal_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointNormal, pcl::PointXYZRGB>(
            point_normal, transform);
    const auto point_with_range_target =
        support::transformCloudXYZTo<pcl::PointWithRange, pcl::PointWithRange>(
            point_with_range, transform);
    const auto point_with_viewpoint_to_xyz_target =
        support::transformCloudXYZTo<pcl::PointWithViewpoint, pcl::PointXYZ>(
            point_with_viewpoint, transform);

    runGenericPublicScaleCase("more generic public scale PointXYZRGBA->PointXYZRGBA 64K",
                              xyzrgba,
                              xyzrgba_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase("more generic public scale PointXYZL->PointXYZ 64K",
                              xyzl,
                              xyzl_to_xyz_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase("more generic public scale PointNormal->PointXYZRGB 64K",
                              point_normal,
                              point_normal_to_rgb_target,
                              iterations,
                              warmup_iterations);
    runGenericPublicScaleCase(
        "more generic public scale PointWithRange->PointWithRange 64K",
        point_with_range,
        point_with_range_target,
        iterations,
        warmup_iterations);
    runGenericPublicScaleCase(
        "more generic public scale PointWithViewpoint->PointXYZ 64K",
        point_with_viewpoint,
        point_with_viewpoint_to_xyz_target,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-scale")) {
    runRowSourceScaleCase("row source scale source-indexed 4K",
                          row_source_small,
                          row_source_small_target,
                          row_source_small_indices,
                          row_source_small_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale source-indexed 64K",
                          row_source_medium,
                          row_source_medium_target,
                          row_source_medium_indices,
                          row_source_medium_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale source-indexed 256K",
                          row_source_large,
                          row_source_large_target,
                          row_source_large_indices,
                          row_source_large_indices,
                          false,
                          iterations,
                          warmup_iterations);

    runRowSourceScaleCase("row source scale dual-indexed 4K",
                          row_source_small,
                          row_source_small_target,
                          row_source_small_indices,
                          row_source_small_alt_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale dual-indexed 64K",
                          row_source_medium,
                          row_source_medium_target,
                          row_source_medium_indices,
                          row_source_medium_alt_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale dual-indexed 256K",
                          row_source_large,
                          row_source_large_target,
                          row_source_large_indices,
                          row_source_large_alt_indices,
                          false,
                          iterations,
                          warmup_iterations);

    runRowSourceScaleCase("row source scale correspondence 4K",
                          row_source_small,
                          row_source_small_target,
                          row_source_small_indices,
                          row_source_small_alt_indices,
                          true,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale correspondence 64K",
                          row_source_medium,
                          row_source_medium_target,
                          row_source_medium_indices,
                          row_source_medium_alt_indices,
                          true,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale correspondence 256K",
                          row_source_large,
                          row_source_large_target,
                          row_source_large_indices,
                          row_source_large_alt_indices,
                          true,
                          iterations,
                          warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-generic-xyz-point-types")) {
    const auto row_source_xyzi_small = support::makePointCloudXYZ<pcl::PointXYZI>(8192);
    const auto row_source_xyzi_medium = support::makePointCloudXYZ<pcl::PointXYZI>(131072);
    const auto row_source_xyzi_large = support::makePointCloudXYZ<pcl::PointXYZI>(524288);
    const auto row_source_xyzi_small_target =
        support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZI>(
            row_source_xyzi_small, transform);
    const auto row_source_xyzi_medium_target =
        support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZI>(
            row_source_xyzi_medium, transform);
    const auto row_source_xyzi_large_target =
        support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZI>(
            row_source_xyzi_large, transform);
    const auto row_source_rgb_small = support::makePointCloudXYZ<pcl::PointXYZRGB>(8192);
    const auto row_source_rgb_medium = support::makePointCloudXYZ<pcl::PointXYZRGB>(131072);
    const auto row_source_rgb_large = support::makePointCloudXYZ<pcl::PointXYZRGB>(524288);
    const auto row_source_rgb_small_target =
        support::transformCloudXYZTo<pcl::PointXYZRGB, pcl::PointXYZRGB>(
            row_source_rgb_small, transform);
    const auto row_source_rgb_medium_target =
        support::transformCloudXYZTo<pcl::PointXYZRGB, pcl::PointXYZRGB>(
            row_source_rgb_medium, transform);
    const auto row_source_rgb_large_target =
        support::transformCloudXYZTo<pcl::PointXYZRGB, pcl::PointXYZRGB>(
            row_source_rgb_large, transform);
    const auto row_source_xyzi_small_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZRGB>(
            row_source_xyzi_small, transform);
    const auto row_source_xyzi_medium_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZRGB>(
            row_source_xyzi_medium, transform);
    const auto row_source_xyzi_large_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointXYZI, pcl::PointXYZRGB>(
            row_source_xyzi_large, transform);

    runRowSourceScaleCase("row source scale source-indexed PointXYZI->PointXYZI 4K",
                          row_source_xyzi_small,
                          row_source_xyzi_small_target,
                          row_source_small_indices,
                          row_source_small_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale source-indexed PointXYZI->PointXYZI 64K",
                          row_source_xyzi_medium,
                          row_source_xyzi_medium_target,
                          row_source_medium_indices,
                          row_source_medium_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale source-indexed PointXYZI->PointXYZI 256K",
                          row_source_xyzi_large,
                          row_source_xyzi_large_target,
                          row_source_large_indices,
                          row_source_large_indices,
                          false,
                          iterations,
                          warmup_iterations);

    runRowSourceScaleCase("row source scale dual-indexed PointXYZRGB->PointXYZRGB 4K",
                          row_source_rgb_small,
                          row_source_rgb_small_target,
                          row_source_small_indices,
                          row_source_small_alt_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale dual-indexed PointXYZRGB->PointXYZRGB 64K",
                          row_source_rgb_medium,
                          row_source_rgb_medium_target,
                          row_source_medium_indices,
                          row_source_medium_alt_indices,
                          false,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale dual-indexed PointXYZRGB->PointXYZRGB 256K",
                          row_source_rgb_large,
                          row_source_rgb_large_target,
                          row_source_large_indices,
                          row_source_large_alt_indices,
                          false,
                          iterations,
                          warmup_iterations);

    runRowSourceScaleCase("row source scale correspondence PointXYZI->PointXYZRGB 4K",
                          row_source_xyzi_small,
                          row_source_xyzi_small_to_rgb_target,
                          row_source_small_indices,
                          row_source_small_alt_indices,
                          true,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale correspondence PointXYZI->PointXYZRGB 64K",
                          row_source_xyzi_medium,
                          row_source_xyzi_medium_to_rgb_target,
                          row_source_medium_indices,
                          row_source_medium_alt_indices,
                          true,
                          iterations,
                          warmup_iterations);
    runRowSourceScaleCase("row source scale correspondence PointXYZI->PointXYZRGB 256K",
                          row_source_xyzi_large,
                          row_source_xyzi_large_to_rgb_target,
                          row_source_large_indices,
                          row_source_large_alt_indices,
                          true,
                          iterations,
                          warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-more-generic-xyz-aos-point-types")) {
    const auto row_source_xyzrgba_small =
        support::makePointCloudXYZ<pcl::PointXYZRGBA>(8192);
    const auto row_source_xyzrgba_medium =
        support::makePointCloudXYZ<pcl::PointXYZRGBA>(131072);
    const auto row_source_xyzrgba_large =
        support::makePointCloudXYZ<pcl::PointXYZRGBA>(524288);
    const auto row_source_xyzrgba_small_target =
        support::transformCloudXYZTo<pcl::PointXYZRGBA, pcl::PointXYZRGBA>(
            row_source_xyzrgba_small, transform);
    const auto row_source_xyzrgba_medium_target =
        support::transformCloudXYZTo<pcl::PointXYZRGBA, pcl::PointXYZRGBA>(
            row_source_xyzrgba_medium, transform);
    const auto row_source_xyzrgba_large_target =
        support::transformCloudXYZTo<pcl::PointXYZRGBA, pcl::PointXYZRGBA>(
            row_source_xyzrgba_large, transform);

    const auto row_source_point_normal_small =
        support::makePointCloudXYZ<pcl::PointNormal>(8192);
    const auto row_source_point_normal_medium =
        support::makePointCloudXYZ<pcl::PointNormal>(131072);
    const auto row_source_point_normal_large =
        support::makePointCloudXYZ<pcl::PointNormal>(524288);
    const auto row_source_point_normal_small_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointNormal, pcl::PointXYZRGB>(
            row_source_point_normal_small, transform);
    const auto row_source_point_normal_medium_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointNormal, pcl::PointXYZRGB>(
            row_source_point_normal_medium, transform);
    const auto row_source_point_normal_large_to_rgb_target =
        support::transformCloudXYZTo<pcl::PointNormal, pcl::PointXYZRGB>(
            row_source_point_normal_large, transform);

    const auto row_source_viewpoint_small =
        support::makePointCloudXYZ<pcl::PointWithViewpoint>(8192);
    const auto row_source_viewpoint_medium =
        support::makePointCloudXYZ<pcl::PointWithViewpoint>(131072);
    const auto row_source_viewpoint_large =
        support::makePointCloudXYZ<pcl::PointWithViewpoint>(524288);
    const auto row_source_viewpoint_small_to_xyz_target =
        support::transformCloudXYZTo<pcl::PointWithViewpoint, pcl::PointXYZ>(
            row_source_viewpoint_small, transform);
    const auto row_source_viewpoint_medium_to_xyz_target =
        support::transformCloudXYZTo<pcl::PointWithViewpoint, pcl::PointXYZ>(
            row_source_viewpoint_medium, transform);
    const auto row_source_viewpoint_large_to_xyz_target =
        support::transformCloudXYZTo<pcl::PointWithViewpoint, pcl::PointXYZ>(
            row_source_viewpoint_large, transform);

    runRowSourceScaleCase(
        "row source scale source-indexed PointXYZRGBA->PointXYZRGBA 4K",
        row_source_xyzrgba_small,
        row_source_xyzrgba_small_target,
        row_source_small_indices,
        row_source_small_indices,
        false,
        iterations,
        warmup_iterations);
    runRowSourceScaleCase(
        "row source scale source-indexed PointXYZRGBA->PointXYZRGBA 64K",
        row_source_xyzrgba_medium,
        row_source_xyzrgba_medium_target,
        row_source_medium_indices,
        row_source_medium_indices,
        false,
        iterations,
        warmup_iterations);
    runRowSourceScaleCase(
        "row source scale source-indexed PointXYZRGBA->PointXYZRGBA 256K",
        row_source_xyzrgba_large,
        row_source_xyzrgba_large_target,
        row_source_large_indices,
        row_source_large_indices,
        false,
        iterations,
        warmup_iterations);

    runRowSourceScaleCase(
        "row source scale dual-indexed PointNormal->PointXYZRGB 4K",
        row_source_point_normal_small,
        row_source_point_normal_small_to_rgb_target,
        row_source_small_indices,
        row_source_small_alt_indices,
        false,
        iterations,
        warmup_iterations);
    runRowSourceScaleCase(
        "row source scale dual-indexed PointNormal->PointXYZRGB 64K",
        row_source_point_normal_medium,
        row_source_point_normal_medium_to_rgb_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        false,
        iterations,
        warmup_iterations);
    runRowSourceScaleCase(
        "row source scale dual-indexed PointNormal->PointXYZRGB 256K",
        row_source_point_normal_large,
        row_source_point_normal_large_to_rgb_target,
        row_source_large_indices,
        row_source_large_alt_indices,
        false,
        iterations,
        warmup_iterations);

    runRowSourceScaleCase(
        "row source scale correspondence PointWithViewpoint->PointXYZ 4K",
        row_source_viewpoint_small,
        row_source_viewpoint_small_to_xyz_target,
        row_source_small_indices,
        row_source_small_alt_indices,
        true,
        iterations,
        warmup_iterations);
    runRowSourceScaleCase(
        "row source scale correspondence PointWithViewpoint->PointXYZ 64K",
        row_source_viewpoint_medium,
        row_source_viewpoint_medium_to_xyz_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        true,
        iterations,
        warmup_iterations);
    runRowSourceScaleCase(
        "row source scale correspondence PointWithViewpoint->PointXYZ 256K",
        row_source_viewpoint_large,
        row_source_viewpoint_large_to_xyz_target,
        row_source_large_indices,
        row_source_large_alt_indices,
        true,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-all-more-generic-xyz-aos-matrix")) {
    runRowSourceAllMoreGenericMatrixCombo<pcl::PointXYZRGBA, pcl::PointXYZRGBA>(
        "PointXYZRGBA->PointXYZRGBA",
        transform,
        row_source_small_indices,
        row_source_small_alt_indices,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceAllMoreGenericMatrixCombo<pcl::PointXYZL, pcl::PointXYZ>(
        "PointXYZL->PointXYZ",
        transform,
        row_source_small_indices,
        row_source_small_alt_indices,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceAllMoreGenericMatrixCombo<pcl::PointNormal, pcl::PointXYZRGB>(
        "PointNormal->PointXYZRGB",
        transform,
        row_source_small_indices,
        row_source_small_alt_indices,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceAllMoreGenericMatrixCombo<pcl::PointWithRange, pcl::PointWithRange>(
        "PointWithRange->PointWithRange",
        transform,
        row_source_small_indices,
        row_source_small_alt_indices,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceAllMoreGenericMatrixCombo<pcl::PointWithViewpoint, pcl::PointXYZ>(
        "PointWithViewpoint->PointXYZ",
        transform,
        row_source_small_indices,
        row_source_small_alt_indices,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "custom-xyz-aos-layout-sampling")) {
    const auto custom_small = support::makePointCloudXYZ<LocalSVDScalePaddedXYZSource>(4096);
    const auto custom_medium =
        support::makePointCloudXYZ<LocalSVDScalePaddedXYZSource>(65536);
    const auto custom_large =
        support::makePointCloudXYZ<LocalSVDScalePaddedXYZSource>(262144);
    const auto custom_small_target =
        support::transformCloudXYZTo<LocalSVDScalePaddedXYZSource,
                                     LocalSVDScaleWideXYZTarget>(custom_small,
                                                                 transform);
    const auto custom_medium_target =
        support::transformCloudXYZTo<LocalSVDScalePaddedXYZSource,
                                     LocalSVDScaleWideXYZTarget>(custom_medium,
                                                                 transform);
    const auto custom_large_target =
        support::transformCloudXYZTo<LocalSVDScalePaddedXYZSource,
                                     LocalSVDScaleWideXYZTarget>(custom_large,
                                                                 transform);

    runGenericPublicScaleCase(
        "custom layout public scale LocalPaddedXYZSource->LocalWideXYZTarget 4K",
        custom_small,
        custom_small_target,
        iterations,
        warmup_iterations);
    runGenericPublicScaleCase(
        "custom layout public scale LocalPaddedXYZSource->LocalWideXYZTarget 64K",
        custom_medium,
        custom_medium_target,
        iterations,
        warmup_iterations);
    runGenericPublicScaleCase(
        "custom layout public scale LocalPaddedXYZSource->LocalWideXYZTarget 256K",
        custom_large,
        custom_large_target,
        iterations,
        warmup_iterations);

    runRowSourceAllMoreGenericMatrixCombo<LocalSVDScalePaddedXYZSource,
                                          LocalSVDScaleWideXYZTarget>(
        "LocalPaddedXYZSource->LocalWideXYZTarget",
        transform,
        row_source_small_indices,
        row_source_small_alt_indices,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "custom-row-source-large-variance-profile")) {
    const auto custom_large =
        support::makePointCloudXYZ<LocalSVDScalePaddedXYZSource>(524288);
    const auto custom_large_target =
        support::transformCloudXYZTo<LocalSVDScalePaddedXYZSource,
                                     LocalSVDScaleWideXYZTarget>(custom_large,
                                                                 transform);

    runCustomRowSourceLargeVarianceProfileSet(
        "256K",
        custom_large,
        custom_large_target,
        row_source_large_contiguous_indices,
        row_source_large_contiguous_alt_indices,
        "contiguous",
        iterations,
        warmup_iterations);
    runCustomRowSourceLargeVarianceProfileSet("256K",
                                              custom_large,
                                              custom_large_target,
                                              row_source_large_indices,
                                              row_source_large_alt_indices,
                                              "stride",
                                              iterations,
                                              warmup_iterations);
    runCustomRowSourceLargeVarianceProfileSet("256K",
                                              custom_large,
                                              custom_large_target,
                                              row_source_large_reverse_indices,
                                              row_source_large_reverse_alt_indices,
                                              "reverse",
                                              iterations,
                                              warmup_iterations);
    runCustomRowSourceLargeVarianceProfileSet("256K",
                                              custom_large,
                                              custom_large_target,
                                              row_source_large_shuffle_indices,
                                              row_source_large_shuffle_alt_indices,
                                              "shuffle",
                                              iterations,
                                              warmup_iterations);
  }

  if (caseEnabled(argc, argv, "custom-layout-padding-sensitivity")) {
    const auto compact_medium =
        support::makePointCloudXYZ<LocalSVDScaleCompactXYZSource>(131072);
    const auto compact_large =
        support::makePointCloudXYZ<LocalSVDScaleCompactXYZSource>(524288);
    const auto huge_medium =
        support::makePointCloudXYZ<LocalSVDScaleHugePaddingXYZSource>(131072);
    const auto huge_large =
        support::makePointCloudXYZ<LocalSVDScaleHugePaddingXYZSource>(524288);
    const auto compact_medium_target =
        support::transformCloudXYZTo<LocalSVDScaleCompactXYZSource,
                                     LocalSVDScaleCompactXYZTarget>(
            compact_medium, transform);
    const auto compact_large_target =
        support::transformCloudXYZTo<LocalSVDScaleCompactXYZSource,
                                     LocalSVDScaleCompactXYZTarget>(
            compact_large, transform);
    const auto huge_medium_target =
        support::transformCloudXYZTo<LocalSVDScaleHugePaddingXYZSource,
                                     LocalSVDScaleHugePaddingXYZTarget>(
            huge_medium, transform);
    const auto huge_large_target =
        support::transformCloudXYZTo<LocalSVDScaleHugePaddingXYZSource,
                                     LocalSVDScaleHugePaddingXYZTarget>(
            huge_large, transform);

    runCustomLayoutPaddingSensitivitySet(
        "LocalCompactXYZSource->LocalCompactXYZTarget",
        "64K",
        compact_medium,
        compact_medium_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
    runCustomLayoutPaddingSensitivitySet(
        "LocalCompactXYZSource->LocalCompactXYZTarget",
        "256K",
        compact_large,
        compact_large_target,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
    runCustomLayoutPaddingSensitivitySet(
        "LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget",
        "64K",
        huge_medium,
        huge_medium_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
    runCustomLayoutPaddingSensitivitySet(
        "LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget",
        "256K",
        huge_large,
        huge_large_target,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "custom-layout-alignment-sensitivity")) {
    const auto aligned_medium =
        support::makePointCloudXYZ<LocalSVDScaleAligned64XYZSource>(131072);
    const auto aligned_large =
        support::makePointCloudXYZ<LocalSVDScaleAligned64XYZSource>(524288);
    const auto aligned_medium_target =
        support::transformCloudXYZTo<LocalSVDScaleAligned64XYZSource,
                                     LocalSVDScaleAligned32XYZTarget>(
            aligned_medium, transform);
    const auto aligned_large_target =
        support::transformCloudXYZTo<LocalSVDScaleAligned64XYZSource,
                                     LocalSVDScaleAligned32XYZTarget>(
            aligned_large, transform);

    runCustomLayoutAlignmentSensitivitySet(
        "LocalAligned64XYZSource->LocalAligned32XYZTarget",
        "64K",
        aligned_medium,
        aligned_medium_target,
        row_source_medium_indices,
        row_source_medium_alt_indices,
        iterations,
        warmup_iterations);
    runCustomLayoutAlignmentSensitivitySet(
        "LocalAligned64XYZSource->LocalAligned32XYZTarget",
        "256K",
        aligned_large,
        aligned_large_target,
        row_source_large_indices,
        row_source_large_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-affine-index-fast-path-detail-ab")) {
    runRowSourceAffineIndexFastPathDetailABSet("64K",
                                               row_source_medium,
                                               row_source_medium_target,
                                               row_source_medium_contiguous_indices,
                                               row_source_medium_contiguous_alt_indices,
                                               iterations,
                                               warmup_iterations);
    runRowSourceAffineIndexFastPathDetailABSet("256K",
                                               row_source_large,
                                               row_source_large_target,
                                               row_source_large_contiguous_indices,
                                               row_source_large_contiguous_alt_indices,
                                               iterations,
                                               warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-affine-index-fast-path-production-probe")) {
    runRowSourceAffineIndexFastPathProductionProbeSet(
        "64K",
        row_source_medium,
        row_source_medium_target,
        row_source_medium_contiguous_indices,
        row_source_medium_contiguous_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceAffineIndexFastPathProductionProbeSet(
        "256K",
        row_source_large,
        row_source_large_target,
        row_source_large_contiguous_indices,
        row_source_large_contiguous_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-locality-order-profile")) {
    runRowSourceOrderProfileSet("4K",
                                row_source_small,
                                row_source_small_target,
                                row_source_small_contiguous_indices,
                                row_source_small_contiguous_alt_indices,
                                "contiguous",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("64K",
                                row_source_medium,
                                row_source_medium_target,
                                row_source_medium_contiguous_indices,
                                row_source_medium_contiguous_alt_indices,
                                "contiguous",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("256K",
                                row_source_large,
                                row_source_large_target,
                                row_source_large_contiguous_indices,
                                row_source_large_contiguous_alt_indices,
                                "contiguous",
                                iterations,
                                warmup_iterations);

    runRowSourceOrderProfileSet("4K",
                                row_source_small,
                                row_source_small_target,
                                row_source_small_indices,
                                row_source_small_alt_indices,
                                "stride",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("64K",
                                row_source_medium,
                                row_source_medium_target,
                                row_source_medium_indices,
                                row_source_medium_alt_indices,
                                "stride",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("256K",
                                row_source_large,
                                row_source_large_target,
                                row_source_large_indices,
                                row_source_large_alt_indices,
                                "stride",
                                iterations,
                                warmup_iterations);

    runRowSourceOrderProfileSet("4K",
                                row_source_small,
                                row_source_small_target,
                                row_source_small_reverse_indices,
                                row_source_small_reverse_alt_indices,
                                "reverse",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("64K",
                                row_source_medium,
                                row_source_medium_target,
                                row_source_medium_reverse_indices,
                                row_source_medium_reverse_alt_indices,
                                "reverse",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("256K",
                                row_source_large,
                                row_source_large_target,
                                row_source_large_reverse_indices,
                                row_source_large_reverse_alt_indices,
                                "reverse",
                                iterations,
                                warmup_iterations);

    runRowSourceOrderProfileSet("4K",
                                row_source_small,
                                row_source_small_target,
                                row_source_small_shuffle_indices,
                                row_source_small_shuffle_alt_indices,
                                "shuffle",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("64K",
                                row_source_medium,
                                row_source_medium_target,
                                row_source_medium_shuffle_indices,
                                row_source_medium_shuffle_alt_indices,
                                "shuffle",
                                iterations,
                                warmup_iterations);
    runRowSourceOrderProfileSet("256K",
                                row_source_large,
                                row_source_large_target,
                                row_source_large_shuffle_indices,
                                row_source_large_shuffle_alt_indices,
                                "shuffle",
                                iterations,
                                warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-shuffle-sorted-copy-detail-ab")) {
    runRowSourceShuffleSortedCopyDetailABSet("4K",
                                            row_source_small,
                                            row_source_small_target,
                                            row_source_small_shuffle_indices,
                                            row_source_small_shuffle_alt_indices,
                                            iterations,
                                            warmup_iterations);
    runRowSourceShuffleSortedCopyDetailABSet("64K",
                                            row_source_medium,
                                            row_source_medium_target,
                                            row_source_medium_shuffle_indices,
                                            row_source_medium_shuffle_alt_indices,
                                            iterations,
                                            warmup_iterations);
    runRowSourceShuffleSortedCopyDetailABSet("256K",
                                            row_source_large,
                                            row_source_large_target,
                                            row_source_large_shuffle_indices,
                                            row_source_large_shuffle_alt_indices,
                                            iterations,
                                            warmup_iterations);
  }

  if (caseEnabled(argc, argv,
                  "row-source-shuffle-dual-indexed-256k-sorted-copy-stability")) {
    runRowSourceShuffleSortedCopyDetailABSet("256K",
                                            row_source_large,
                                            row_source_large_target,
                                            row_source_large_shuffle_indices,
                                            row_source_large_shuffle_alt_indices,
                                            iterations,
                                            warmup_iterations,
                                            true,
                                            false);
  }

  if (caseEnabled(argc, argv, "row-source-shuffle-dual-indexed-target-sorted-detail-ab")) {
    runRowSourceShuffleTargetSortedDetailABSet(
        "64K",
        row_source_medium,
        row_source_medium_target,
        row_source_medium_shuffle_indices,
        row_source_medium_shuffle_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceShuffleTargetSortedDetailABSet(
        "256K",
        row_source_large,
        row_source_large_target,
        row_source_large_shuffle_indices,
        row_source_large_shuffle_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "row-source-shuffle-staged-selected-cloud-detail-ab")) {
    runRowSourceShuffleStagedSelectedCloudDetailABSet(
        "4K",
        row_source_small,
        row_source_small_target,
        row_source_small_shuffle_indices,
        row_source_small_shuffle_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceShuffleStagedSelectedCloudDetailABSet(
        "64K",
        row_source_medium,
        row_source_medium_target,
        row_source_medium_shuffle_indices,
        row_source_medium_shuffle_alt_indices,
        iterations,
        warmup_iterations);
    runRowSourceShuffleStagedSelectedCloudDetailABSet(
        "256K",
        row_source_large,
        row_source_large_target,
        row_source_large_shuffle_indices,
        row_source_large_shuffle_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv,
                  "correspondence-sorted-copy-scalar-double-detail-ab")) {
    runCorrespondenceSortedCopyScalarDoubleDetailABSet(
        "64K",
        row_source_medium,
        row_source_medium_target,
        row_source_medium_shuffle_indices,
        row_source_medium_shuffle_alt_indices,
        iterations,
        warmup_iterations);
    runCorrespondenceSortedCopyScalarDoubleDetailABSet(
        "256K",
        row_source_large,
        row_source_large_target,
        row_source_large_shuffle_indices,
        row_source_large_shuffle_alt_indices,
        iterations,
        warmup_iterations);
  }

  if (caseEnabled(argc, argv, "correspondence-sorted-copy-production-probe")) {
    runCorrespondenceSortedCopyProductionProbeSet(
        "4K",
        row_source_small,
        row_source_small_target,
        row_source_small_shuffle_indices,
        row_source_small_shuffle_alt_indices,
        iterations,
        warmup_iterations);
    runCorrespondenceSortedCopyProductionProbeSet(
        "64K",
        row_source_medium,
        row_source_medium_target,
        row_source_medium_shuffle_indices,
        row_source_medium_shuffle_alt_indices,
        iterations,
        warmup_iterations);
    runCorrespondenceSortedCopyProductionProbeSet(
        "256K",
        row_source_large,
        row_source_large_target,
        row_source_large_shuffle_indices,
        row_source_large_shuffle_alt_indices,
        iterations,
        warmup_iterations);
  }

  printBanner('=');
  return 0;
}
