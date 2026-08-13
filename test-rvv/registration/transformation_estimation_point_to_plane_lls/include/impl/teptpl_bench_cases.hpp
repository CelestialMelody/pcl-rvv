/*
 * 本文件做什么：
 * TEPTPL bench case registry（性能测试用例登记）和运行入口。case label、checksum
 * 和输出格式保持与拆分前一致，供 QEMU log-shape 和 board summary 继续复用。
 */

#pragma once

#include "teptpl_bench_components.hpp"

#include <pcl/registration/transformation_estimation_point_to_plane_lls.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_bench {

template <typename Fn>
inline BenchResult
runCase(const std::string& name, const int iterations, Fn&& fn)
{
  // runCase 统一输出合同需要的 avg、Total Time 和 checksum，供 QEMU 与板卡日志共用。
  double checksum = 0.0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto stop = std::chrono::steady_clock::now();
  const double total_ms =
      std::chrono::duration<double, std::milli>(stop - start).count();
  return BenchResult{name, total_ms / iterations, total_ms, checksum};
}

template <typename Fn>
inline void
appendCase(std::vector<BenchResult>& results,
           const BenchOptions& options,
           const std::string& name,
           Fn&& fn)
{
  if (!options.case_filter.empty() &&
      name.find(options.case_filter) == std::string::npos)
    return;
  results.push_back(runCase(name, options.iterations, std::forward<Fn>(fn)));
}

inline int
run_teptpl_bench(int argc, char** argv)
{
  const BenchOptions options = parseOptions(argc, argv);
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "Dataset: synthetic point-to-plane LLS normal-equation diagnostic\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  if (!options.case_filter.empty())
    std::cout << "Case filter: " << options.case_filter << "\n";
  if (options.component_only)
    std::cout << "Mode: component-only\n";
#ifdef __RVV10__
  std::cout << "Build: rvv\n";
#else
  std::cout << "Build: std\n";
#endif

  std::vector<BenchResult> results;
  for (const auto n : options.sizes) {
    pcl::PointCloud<pcl::PointNormal> source = makeCloudWithAtLeast(n);
    pcl::PointCloud<pcl::PointNormal> target;
    pcl::transformPointCloudWithNormals(source, target, makeTransform());
    const pcl::PointCloud<pcl::PointXYZ> source_xyz = copySourceAsXYZ(source);
    const pcl::PointCloud<pcl::PointXYZINormal> target_xyzinormal =
        copyTargetAsXYZINormal(target);

    const pcl::Indices indexed_rows = makeIndexedRows(source.size());
    const pcl::PointCloud<pcl::PointNormal> compact_target =
        copyIndexedCloud(target, indexed_rows);
    const pcl::Indices target_indexed_rows =
        makeIndependentTargetIndexedRows(target.size());

    if (options.component_only) {
      const ComponentSoA full_soa = makeFullSoA(source, target);
      const FormulaSoA full_formula = makeFormulaSoA(full_soa);
      appendCase(
          results,
          options,
          "lls component full-cloud load-store-only pointnormal " +
              std::to_string(n),
          [&]() { return componentFullLoadStoreOnly(source, target); });
      appendCase(
          results,
          options,
          "lls component dual-indices independent-stream gather-load-store-only pointnormal " +
              std::to_string(n),
          [&]() {
            return componentDualGatherLoadStoreOnly(
                source, indexed_rows, target, target_indexed_rows);
          });
      appendCase(
          results,
          options,
          "lls component full-cloud formula-store-only pointnormal " +
              std::to_string(n),
          [&]() { return componentFormulaStoreOnly(full_soa); });
      appendCase(
          results,
          options,
          "lls component full-cloud mask-compress-only pointnormal " +
              std::to_string(n),
          [&]() { return componentMaskCompressOnly(full_soa, full_formula); });
      appendCase(
          results,
          options,
          "lls component full-cloud tail-only pointnormal " + std::to_string(n),
          [&]() { return componentTailOnly(full_formula); });
      appendCase(
          results,
          options,
          "lls component full-cloud no-solve pointnormal " + std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      appendCase(
          results,
          options,
          "lls component full-cloud fused-reduction no-solve pointnormal " +
              std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full_fused_reduction(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      appendCase(
          results,
          options,
          "lls component full-cloud grouped-reduction no-solve pointnormal " +
              std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full_grouped_reduction(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      appendCase(
          results,
          options,
          "lls component full-cloud block-reduction no-solve pointnormal " +
              std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full_block_reduction(source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      appendCase(
          results,
          options,
          "lls component full-cloud block-fused-formula no-solve pointnormal " +
              std::to_string(n),
          [&]() {
            support::AccumulationStats stats;
            const support::NormalEquation eq =
                support::accumulate_candidate_full_block_fused_formula_reduction(
                    source, target, &stats);
            return normalEquationChecksum(eq) +
                   static_cast<double>(stats.accepted_points) * 1e-9;
          });
      continue;
    }

    appendCase(
        results,
        options,
        "lls normal-equation full-cloud pointnormal " + std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    // Historical diagnostic: trusted-dense/fused/grouped/block rows separate local
    // implementation choices from production dispatch. Only block currently maps to
    // production, and only through the explicit production-dispatch case below.
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_trusted_dense(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud fused-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_fused_reduction(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud grouped-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_grouped_reduction(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud block-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_block_reduction(source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation full-cloud block-fused-formula pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_full_block_fused_formula_reduction(
                  source, target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    // Production-dispatch evidence: both std and RVV builds enter the same public
    // overload. Under __RVV10__ the production gate may select the f32 AoS
    // layout-gated full-cloud block path; otherwise it is the scalar path.
    appendCase(
        results,
        options,
        "lls production-dispatch full-cloud pointnormal " + std::to_string(n),
        [&]() {
          pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointNormal,
                                                                     pcl::PointNormal>
              estimator;
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimator.estimateRigidTransformation(source, target, matrix);
          return support::matrix_checksum(matrix);
        });
    appendCase(
        results,
        options,
        "lls production-dispatch full-cloud pointxyz-to-pointnormal " +
            std::to_string(n),
        [&]() {
          pcl::registration::TransformationEstimationPointToPlaneLLS<pcl::PointXYZ,
                                                                     pcl::PointNormal>
              estimator;
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimator.estimateRigidTransformation(source_xyz, target, matrix);
          return support::matrix_checksum(matrix);
        });
    appendCase(
        results,
        options,
        "lls production-dispatch full-cloud pointxyz-to-pointxyzinormal " +
            std::to_string(n),
        [&]() {
          pcl::registration::TransformationEstimationPointToPlaneLLS<
              pcl::PointXYZ,
              pcl::PointXYZINormal>
              estimator;
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimator.estimateRigidTransformation(source_xyz, target_xyzinormal, matrix);
          return support::matrix_checksum(matrix);
        });
    // Bench-only shape check: useful when comparing public-overload overhead to the
    // block helper, but it does not prove real dispatch or fallback behavior.
    appendCase(
        results,
        options,
        "lls public-entry-shaped full-cloud block-reduction pointnormal " +
            std::to_string(n),
        [&]() {
          Eigen::Matrix4f matrix = Eigen::Matrix4f::Identity();
          estimatePublicEntryShapedFullCloudBlock(source, target, matrix);
          return support::matrix_checksum(matrix);
        });

    // Historical indexed diagnostics: retained to explain why this closeout does not
    // expand beyond full-cloud. Negative results here must not be collapsed into a
    // single gather-only cause without a separate profile or ablation.
    appendCase(
        results,
        options,
        "lls normal-equation source-indices pointnormal " + std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_source_indices(
              source, indexed_rows, compact_target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation source-indices trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_source_indices_trusted_dense(
                  source, indexed_rows, compact_target, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    appendCase(
        results,
        options,
        "lls normal-equation dual-indices same-stream pointnormal " + std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_dual_indices(
              source, indexed_rows, target, indexed_rows, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    appendCase(
        results,
        options,
        "lls normal-equation dual-indices independent-stream pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_dual_indices(
              source, indexed_rows, target, target_indexed_rows, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation dual-indices independent-stream trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_dual_indices_trusted_dense(
                  source, indexed_rows, target, target_indexed_rows, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    // Historical correspondence diagnostics: query/match expansion is included in
    // timing, so these rows are not interchangeable with dual-indices gather rows.
    const pcl::Correspondences correspondences = makeCorrespondences(source.size());
    appendCase(
        results,
        options,
        "lls normal-equation correspondences same-index pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_correspondences(
              source, target, correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    const pcl::Correspondences local_offset_correspondences =
        makeLocalOffsetCorrespondences(source.size());
    appendCase(
        results,
        options,
        "lls normal-equation correspondences local-offset pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_correspondences(
              source, target, local_offset_correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });

    const pcl::Correspondences independent_stream_correspondences =
        makeCorrespondencesFromIndices(indexed_rows, target_indexed_rows);
    appendCase(
        results,
        options,
        "lls normal-equation correspondences independent-stream pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix = support::estimate_candidate_correspondences(
              source, target, independent_stream_correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
    appendCase(
        results,
        options,
        "lls normal-equation correspondences independent-stream trusted-dense pointnormal " +
            std::to_string(n),
        [&]() {
          support::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              support::estimate_candidate_correspondences_trusted_dense(
                  source, target, independent_stream_correspondences, &stats);
          return support::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        });
  }

  double checksum = 0.0;
  double total = 0.0;
  for (const auto& result : results) {
    checksum += result.checksum;
    total += result.total_ms;
    std::cout << result.name << ": " << result.average_ms << " ms/iter\n";
    std::cout << "  Total Time: " << result.total_ms << " ms\n";
    std::cout << "  Checksum: " << result.checksum << "\n";
  }
  std::cout << "Total Time: " << total << " ms\n";
  std::cout << "Checksum: " << checksum << "\n";
  return 0;
}


} // namespace pcl::registration::rvv_te_pt2plane_lls_bench
