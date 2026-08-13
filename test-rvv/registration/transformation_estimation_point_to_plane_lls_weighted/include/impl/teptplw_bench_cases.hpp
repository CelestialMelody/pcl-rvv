/*
 * 本文件做什么：
 * transformation_estimation_point_to_plane_lls_weighted bench case registry。它维护
 * case-filter、case label、trace/full estimate 计时边界和 checksum 合同；src/bench
 * 只保留 main 入口。
 */

#pragma once

#include "teptplw_bench_harness.hpp"
#include "teptplw_fixtures.hpp"

#include <pcl/common/transforms.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls_weighted.h>

#include <Eigen/Core>

#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace pcl::registration::rvv_te_pt2plane_lls_weighted_bench {

inline void
print_bench_header(const BenchOptions& options)
{
  std::cout << std::fixed << std::setprecision(6);
  std::cout
      << "Dataset: synthetic PointNormal weighted point-to-plane LLS diagnostic\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup_iterations << "\n";
  if (!options.case_filter.empty())
    std::cout << "Case filter: " << options.case_filter << "\n";
  if (options.case_filter == "generic-fused-abc-trace") {
    std::cout << "Generic abc order:";
    for (const auto& token : options.generic_abc_order)
      std::cout << " " << token;
    std::cout << "\n";
  }
#ifdef __RVV10__
  std::cout << "Build: rvv\n";
#else
  std::cout << "Build: std\n";
#endif
}

inline std::vector<BenchResult>
collect_bench_results(const BenchOptions& options)
{
  std::vector<BenchResult> results;
  for (const auto n : options.sizes) {
    pcl::PointCloud<pcl::PointNormal> source = diag::make_bench_cloud_with_at_least(n);
    pcl::PointCloud<pcl::PointNormal> target;
    pcl::transformPointCloudWithNormals(source, target, diag::make_transform());
    const std::vector<float> weights = diag::make_weights(source.size());

    if (options.case_filter == "production-dispatch") {
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointNormal,
          pcl::PointNormal>
          pointnormal_estimator;
      results.push_back(run_case(
          "weighted lls production-dispatch full-cloud pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_full_cloud_weighted(
                pointnormal_estimator, source, target, weights);
          }));

      const auto source_xyz = diag::copy_source_as_xyz(source);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointNormal>
          pointxyz_pointnormal_estimator;
      results.push_back(run_case(
          "weighted lls production-dispatch full-cloud pointxyz-to-pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_full_cloud_weighted(
                pointxyz_pointnormal_estimator, source_xyz, target, weights);
          }));

      const auto target_xyzinormal = diag::copy_target_as_xyzinormal(target);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointXYZINormal>
          pointxyz_xyzinormal_estimator;
      results.push_back(run_case(
          "weighted lls production-dispatch full-cloud pointxyz-to-pointxyzinormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_full_cloud_weighted(
                pointxyz_xyzinormal_estimator, source_xyz, target_xyzinormal, weights);
          }));
      continue;
    }

    if (options.case_filter == "production-source-indices") {
      const pcl::Indices source_indices = diag::make_source_indices(source.size());

      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointNormal,
          pcl::PointNormal>
          pointnormal_estimator;
      results.push_back(run_case(
          "weighted lls production-dispatch source-indices pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_source_indices_weighted(
                pointnormal_estimator, source, source_indices, target, weights);
          }));

      const auto source_xyz = diag::copy_source_as_xyz(source);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointNormal>
          pointxyz_pointnormal_estimator;
      results.push_back(run_case(
          "weighted lls production-dispatch source-indices pointxyz-to-pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_source_indices_weighted(
                pointxyz_pointnormal_estimator, source_xyz, source_indices, target, weights);
          }));

      const auto target_xyzinormal = diag::copy_target_as_xyzinormal(target);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointXYZINormal>
          pointxyz_xyzinormal_estimator;
      results.push_back(run_case(
          "weighted lls production-dispatch source-indices pointxyz-to-pointxyzinormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_source_indices_weighted(pointxyz_xyzinormal_estimator,
                                                     source_xyz,
                                                     source_indices,
                                                     target_xyzinormal,
                                                     weights);
          }));

      continue;
    }

    if (options.case_filter == "production-source-indices-detail-ba") {
      const pcl::Indices source_indices = diag::make_source_indices(source.size());
      const auto source_xyz = diag::copy_source_as_xyz(source);
      const auto target_xyzinormal = diag::copy_target_as_xyzinormal(target);

      auto add_detail_pair = [&](const std::string& case_label,
                                 const auto& source_cloud,
                                 const auto& target_cloud) {
        results.push_back(run_case_trace(
            "weighted lls production-source-indices-detail component staged-gather " +
                case_label + " no-solve " + std::to_string(n),
            options.iterations,
            [&]() {
              return run_production_source_indices_staged_no_solve(
                  source_cloud, source_indices, target_cloud, weights);
            }));
        results.push_back(run_case_trace(
            "weighted lls production-source-indices-detail component block-fused-abcd-ilp " +
                case_label + " no-solve " + std::to_string(n),
            options.iterations,
            [&]() {
              return run_production_source_indices_block_fused_no_solve(
                  source_cloud, source_indices, target_cloud, weights);
            }));

        results.push_back(run_case_trace(
            "weighted lls production-source-indices-detail staged-gather " +
                case_label + " " + std::to_string(n),
            options.iterations,
            [&]() {
              return run_production_source_indices_staged_full(
                  source_cloud, source_indices, target_cloud, weights);
            }));
        results.push_back(run_case_trace(
            "weighted lls production-source-indices-detail block-fused-abcd-ilp " +
                case_label + " " + std::to_string(n),
            options.iterations,
            [&]() {
              return run_production_source_indices_block_fused_full(
                  source_cloud, source_indices, target_cloud, weights);
            }));
      };

      add_detail_pair("pointnormal", source, target);
      add_detail_pair("pointxyz-to-pointnormal", source_xyz, target);
      add_detail_pair("pointxyz-to-pointxyzinormal", source_xyz, target_xyzinormal);
      continue;
    }

    if (options.case_filter == "production-shaped-fused-formula") {
      results.push_back(run_case(
          "weighted lls production-shaped full-cloud block-baseline pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const Eigen::Matrix4f matrix =
                diag::estimate_candidate_full_block_reduction(
                    source, target, weights, &stats);
            return diag::matrix_checksum(matrix) +
                   static_cast<double>(stats.accepted_points) * 1e-6;
          }));

      auto add_production_shaped_fused_case = [&](const std::string& case_name,
                                                 auto estimate) {
        results.push_back(run_case(
            "weighted lls production-shaped full-cloud " + case_name +
                " pointnormal " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const Eigen::Matrix4f matrix = estimate(source, target, weights, &stats);
              return diag::matrix_checksum(matrix) +
                     static_cast<double>(stats.accepted_points) * 1e-6;
            }));
      };

      add_production_shaped_fused_case("block-fused-abc",
                                       diag::estimate_candidate_full_block_fused_abc);
      add_production_shaped_fused_case(
          "block-fused-abc-ilp",
          diag::estimate_candidate_full_block_fused_abc_ilp);
      add_production_shaped_fused_case(
          "block-fused-d-six-term",
          diag::estimate_candidate_full_block_fused_d_six_term);
      add_production_shaped_fused_case(
          "block-fused-d-six-term-ilp",
          diag::estimate_candidate_full_block_fused_d_six_term_ilp);
      add_production_shaped_fused_case(
          "block-fused-d-displacement",
          diag::estimate_candidate_full_block_fused_d_displacement);
      add_production_shaped_fused_case(
          "block-fused-d-displacement-ilp",
          diag::estimate_candidate_full_block_fused_d_displacement_ilp);
      add_production_shaped_fused_case("block-fused-abcd",
                                       diag::estimate_candidate_full_block_fused_abcd);
      add_production_shaped_fused_case("block-fused-abcd-ilp",
                                       diag::estimate_candidate_full_block_fused_abcd_ilp);
      continue;
    }

    if (options.case_filter == "production-default-fused-abcd-ilp") {
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointNormal,
          pcl::PointNormal>
          pointnormal_estimator;
      results.push_back(run_case_trace(
          "weighted lls production-default full-cloud fused-abcd-ilp pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_full_cloud_weighted(
                pointnormal_estimator, source, target, weights);
          }));

      const auto source_xyz = diag::copy_source_as_xyz(source);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointNormal>
          pointxyz_pointnormal_estimator;
      results.push_back(run_case_trace(
          "weighted lls production-default full-cloud fused-abcd-ilp pointxyz-to-pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_full_cloud_weighted(
                pointxyz_pointnormal_estimator, source_xyz, target, weights);
          }));

      const auto target_xyzinormal = diag::copy_target_as_xyzinormal(target);
      pcl::registration::TransformationEstimationPointToPlaneLLSWeighted<
          pcl::PointXYZ,
          pcl::PointXYZINormal>
          pointxyz_xyzinormal_estimator;
      results.push_back(run_case_trace(
          "weighted lls production-default full-cloud fused-abcd-ilp pointxyz-to-pointxyzinormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            return run_public_full_cloud_weighted(
                pointxyz_xyzinormal_estimator, source_xyz, target_xyzinormal, weights);
          }));
      continue;
    }

    if (options.case_filter == "row-sources") {
      const pcl::Indices source_indices = diag::make_source_indices(source.size());
      const pcl::Indices target_indices = diag::make_target_indices(source.size());
      const pcl::Correspondences correspondences =
          diag::make_bench_correspondences(source.size());

      results.push_back(run_case(
          "weighted lls row-sources full-cloud pointnormal " + std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const Eigen::Matrix4f matrix =
                diag::estimate_candidate_full(source, target, weights, &stats);
            return diag::matrix_checksum(matrix) +
                   static_cast<double>(stats.accepted_points) * 1e-6;
          }));

      results.push_back(run_case(
          "weighted lls row-sources source-indices pointnormal " + std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const Eigen::Matrix4f matrix = diag::estimate_candidate_source_indices(
                source, source_indices, target, weights, &stats);
            return diag::matrix_checksum(matrix) +
                   static_cast<double>(stats.accepted_points) * 1e-6;
          }));

      results.push_back(run_case(
          "weighted lls row-sources dual-indices pointnormal " + std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const Eigen::Matrix4f matrix = diag::estimate_candidate_dual_indices(
                source, source_indices, target, target_indices, weights, &stats);
            return diag::matrix_checksum(matrix) +
                   static_cast<double>(stats.accepted_points) * 1e-6;
          }));

      results.push_back(run_case(
          "weighted lls row-sources correspondences pointnormal " + std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const Eigen::Matrix4f matrix = diag::estimate_candidate_correspondences(
                source, target, correspondences, &stats);
            return diag::matrix_checksum(matrix) +
                   static_cast<double>(stats.accepted_points) * 1e-6;
          }));
      continue;
    }

    if (options.case_filter == "source-indexed-family") {
      const pcl::Indices source_indices = diag::make_source_indices(source.size());

      results.push_back(run_case(
          "weighted lls source-indexed-family staged-gather pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const diag::NormalEquation eq = diag::accumulate_candidate_source_indices(
                source, source_indices, target, weights, &stats);
            return solved_normal_equation_checksum(eq);
          }));

      results.push_back(run_case(
          "weighted lls source-indexed-family block-baseline pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const diag::NormalEquation eq =
                diag::accumulate_candidate_source_indices_block_reduction(
                    source, source_indices, target, weights, &stats);
            return solved_normal_equation_checksum(eq);
          }));

      results.push_back(run_case(
          "weighted lls source-indexed-family block-fused-abcd-ilp pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const diag::NormalEquation eq =
                diag::accumulate_candidate_source_indices_block_fused_abcd_ilp(
                    source, source_indices, target, weights, &stats);
            return solved_normal_equation_checksum(eq);
          }));

      results.push_back(run_case(
          "weighted lls component source-indexed-family block-baseline pointnormal no-solve " +
              std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const diag::NormalEquation eq =
                diag::accumulate_candidate_source_indices_block_reduction(
                    source, source_indices, target, weights, &stats);
            return normal_equation_checksum(eq) +
                   static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
          }));

      results.push_back(run_case(
          "weighted lls component source-indexed-family block-fused-abcd-ilp pointnormal no-solve " +
              std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const diag::NormalEquation eq =
                diag::accumulate_candidate_source_indices_block_fused_abcd_ilp(
                    source, source_indices, target, weights, &stats);
            return normal_equation_checksum(eq) +
                   static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
          }));
      continue;
    }

    if (options.case_filter == "dual-correspondence-family") {
      const pcl::Indices source_indices = diag::make_source_indices(source.size());
      const pcl::Indices target_indices = diag::make_target_indices(source.size());
      const pcl::Correspondences correspondences =
          diag::make_bench_correspondences(source.size());

      auto add_full_estimate_case = [&](const std::string& row_source,
                                        const std::string& variant,
                                        auto estimate) {
        results.push_back(run_case(
            "weighted lls dual-correspondence-family " + row_source + " " +
                variant + " pointnormal " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const Eigen::Matrix4f matrix = estimate(&stats);
              return diag::matrix_checksum(matrix) +
                     static_cast<double>(stats.accepted_points) * 1e-6;
            }));
      };
      auto add_component_case = [&](const std::string& row_source,
                                    const std::string& variant,
                                    auto accumulate) {
        results.push_back(run_case(
            "weighted lls component dual-correspondence-family " + row_source +
                " " + variant + " pointnormal no-solve " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const diag::NormalEquation eq = accumulate(&stats);
              return normal_equation_checksum(eq) +
                     static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
            }));
      };

      add_full_estimate_case(
          "dual-indices",
          "staged-gather",
          [&](diag::AccumulationStats* stats) {
            return diag::estimate_candidate_dual_indices(
                source, source_indices, target, target_indices, weights, stats);
          });
      add_full_estimate_case(
          "dual-indices",
          "block-baseline",
          [&](diag::AccumulationStats* stats) {
            return diag::estimate_candidate_dual_indices_block_reduction(
                source, source_indices, target, target_indices, weights, stats);
          });
      add_full_estimate_case(
          "dual-indices",
          "block-fused-abcd-ilp",
          [&](diag::AccumulationStats* stats) {
            return diag::estimate_candidate_dual_indices_block_fused_abcd_ilp(
                source, source_indices, target, target_indices, weights, stats);
          });
      add_component_case(
          "dual-indices",
          "block-baseline",
          [&](diag::AccumulationStats* stats) {
            return diag::accumulate_candidate_dual_indices_block_reduction(
                source, source_indices, target, target_indices, weights, stats);
          });
      add_component_case(
          "dual-indices",
          "block-fused-abcd-ilp",
          [&](diag::AccumulationStats* stats) {
            return diag::accumulate_candidate_dual_indices_block_fused_abcd_ilp(
                source, source_indices, target, target_indices, weights, stats);
          });

      add_full_estimate_case(
          "correspondences",
          "staged-gather",
          [&](diag::AccumulationStats* stats) {
            return diag::estimate_candidate_correspondences(
                source, target, correspondences, stats);
          });
      add_full_estimate_case(
          "correspondences",
          "block-baseline",
          [&](diag::AccumulationStats* stats) {
            return diag::estimate_candidate_correspondences_block_reduction(
                source, target, correspondences, stats);
          });
      add_full_estimate_case(
          "correspondences",
          "block-fused-abcd-ilp",
          [&](diag::AccumulationStats* stats) {
            return diag::estimate_candidate_correspondences_block_fused_abcd_ilp(
                source, target, correspondences, stats);
          });
      add_component_case(
          "correspondences",
          "block-baseline",
          [&](diag::AccumulationStats* stats) {
            return diag::accumulate_candidate_correspondences_block_reduction(
                source, target, correspondences, stats);
          });
      add_component_case(
          "correspondences",
          "block-fused-abcd-ilp",
          [&](diag::AccumulationStats* stats) {
            return diag::accumulate_candidate_correspondences_block_fused_abcd_ilp(
                source, target, correspondences, stats);
          });
      continue;
    }

    if (options.case_filter == "generic-fused-abc" ||
        options.case_filter == "generic-fused-formula") {
      const bool include_d_candidates = options.case_filter == "generic-fused-formula";
      const auto source_xyz = diag::copy_source_as_xyz(source);
      const auto target_xyzinormal = diag::copy_target_as_xyzinormal(target);
      auto add_generic_fused_cases = [&](const std::string& case_label,
                                         const auto& source_cloud,
                                         const auto& target_cloud) {
        using SourceCloud = std::decay_t<decltype(source_cloud)>;
        using TargetCloud = std::decay_t<decltype(target_cloud)>;
        using PointSource = typename SourceCloud::PointType;
        using PointTarget = typename TargetCloud::PointType;

        results.push_back(run_case(
            "weighted lls component full-cloud block-baseline " + case_label +
                " no-solve " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const diag::NormalEquation eq =
                  diag::accumulate_candidate_full_block_reduction_layout_gated(
                      source_cloud, target_cloud, weights, &stats);
              return normal_equation_checksum(eq) +
                     static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
            }));
        results.push_back(run_case(
            "weighted lls component full-cloud block-fused-abc " + case_label +
                " no-solve " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const diag::NormalEquation eq =
                  diag::accumulate_candidate_full_block_fused_abc_layout_gated(
                      source_cloud, target_cloud, weights, &stats);
              return normal_equation_checksum(eq) +
                     static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
            }));
        results.push_back(run_case(
            "weighted lls component full-cloud block-fused-abc-ilp " + case_label +
                " no-solve " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const diag::NormalEquation eq =
                  diag::accumulate_candidate_full_block_fused_abc_ilp_layout_gated(
                      source_cloud, target_cloud, weights, &stats);
              return normal_equation_checksum(eq) +
	                     static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
            }));
        if (include_d_candidates) {
          results.push_back(run_case(
              "weighted lls component full-cloud block-fused-d-six-term " +
                  case_label + " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_fused_d_six_term_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
          results.push_back(run_case(
              "weighted lls component full-cloud block-fused-d-six-term-ilp " +
                  case_label + " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_fused_d_six_term_ilp_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
          results.push_back(run_case(
              "weighted lls component full-cloud block-fused-d-displacement " +
                  case_label + " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_fused_d_displacement_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
          results.push_back(run_case(
              "weighted lls component full-cloud block-fused-d-displacement-ilp " +
                  case_label + " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_fused_d_displacement_ilp_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
          results.push_back(run_case(
              "weighted lls component full-cloud block-fused-abcd " + case_label +
                  " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_fused_abcd_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
          results.push_back(run_case(
              "weighted lls component full-cloud block-fused-abcd-ilp " +
                  case_label + " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_fused_abcd_ilp_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
        }

        results.push_back(run_case(
            "weighted lls production-shaped full-cloud block-baseline " + case_label +
                " " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const Eigen::Matrix4f matrix =
                  diag::estimate_candidate_full_block_reduction_layout_gated(
                      source_cloud, target_cloud, weights, &stats);
              return diag::matrix_checksum(matrix) +
                     static_cast<double>(stats.accepted_points) * 1e-6;
            }));
        results.push_back(run_case(
            "weighted lls production-shaped full-cloud block-fused-abc " + case_label +
                " " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const Eigen::Matrix4f matrix =
                  diag::estimate_candidate_full_block_fused_abc_layout_gated(
                      source_cloud, target_cloud, weights, &stats);
              return diag::matrix_checksum(matrix) +
                     static_cast<double>(stats.accepted_points) * 1e-6;
            }));
        results.push_back(run_case(
            "weighted lls production-shaped full-cloud block-fused-abc-ilp " +
                case_label + " " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const Eigen::Matrix4f matrix =
                  diag::estimate_candidate_full_block_fused_abc_ilp_layout_gated(
                      source_cloud, target_cloud, weights, &stats);
              return diag::matrix_checksum(matrix) +
	                     static_cast<double>(stats.accepted_points) * 1e-6;
            }));
        if (include_d_candidates) {
          results.push_back(run_case(
              "weighted lls production-shaped full-cloud block-fused-d-six-term " +
                  case_label + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_fused_d_six_term_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
          results.push_back(run_case(
              "weighted lls production-shaped full-cloud block-fused-d-six-term-ilp " +
                  case_label + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_fused_d_six_term_ilp_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
          results.push_back(run_case(
              "weighted lls production-shaped full-cloud block-fused-d-displacement " +
                  case_label + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_fused_d_displacement_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
          results.push_back(run_case(
              "weighted lls production-shaped full-cloud block-fused-d-displacement-ilp " +
                  case_label + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_fused_d_displacement_ilp_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
          results.push_back(run_case(
              "weighted lls production-shaped full-cloud block-fused-abcd " +
                  case_label + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_fused_abcd_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
          results.push_back(run_case(
              "weighted lls production-shaped full-cloud block-fused-abcd-ilp " +
                  case_label + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_fused_abcd_ilp_layout_gated<
                        PointSource,
                        PointTarget>(source_cloud, target_cloud, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
        }
      };

      add_generic_fused_cases("pointnormal", source, target);
      add_generic_fused_cases("pointxyz-to-pointnormal", source_xyz, target);
      add_generic_fused_cases(
          "pointxyz-to-pointxyzinormal", source_xyz, target_xyzinormal);
      continue;
    }

    if (options.case_filter == "generic-fused-abc-trace") {
      const auto source_xyz = diag::copy_source_as_xyz(source);
      const auto target_xyzinormal = diag::copy_target_as_xyzinormal(target);
      constexpr const char* kCaseLabel = "pointxyz-to-pointxyzinormal";

      auto add_component_trace = [&](const std::string& token) {
        if (token == "baseline") {
          results.push_back(run_case_trace(
              "weighted lls component full-cloud block-baseline " +
                  std::string(kCaseLabel) + " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_reduction_layout_gated(
                        source_xyz, target_xyzinormal, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
          return;
        }
        if (token == "abc") {
          results.push_back(run_case_trace(
              "weighted lls component full-cloud block-fused-abc " +
                  std::string(kCaseLabel) + " no-solve " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const diag::NormalEquation eq =
                    diag::accumulate_candidate_full_block_fused_abc_layout_gated(
                        source_xyz, target_xyzinormal, weights, &stats);
                return normal_equation_checksum(eq) +
                       static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
              }));
          return;
        }
        results.push_back(run_case_trace(
            "weighted lls component full-cloud block-fused-abc-ilp " +
                std::string(kCaseLabel) + " no-solve " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const diag::NormalEquation eq =
                  diag::accumulate_candidate_full_block_fused_abc_ilp_layout_gated(
                      source_xyz, target_xyzinormal, weights, &stats);
              return normal_equation_checksum(eq) +
                     static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
            }));
      };

      auto add_production_trace = [&](const std::string& token) {
        if (token == "baseline") {
          results.push_back(run_case_trace(
              "weighted lls production-shaped full-cloud block-baseline " +
                  std::string(kCaseLabel) + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_reduction_layout_gated(
                        source_xyz, target_xyzinormal, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
          return;
        }
        if (token == "abc") {
          results.push_back(run_case_trace(
              "weighted lls production-shaped full-cloud block-fused-abc " +
                  std::string(kCaseLabel) + " " + std::to_string(n),
              options.iterations,
              [&]() {
                diag::AccumulationStats stats;
                const Eigen::Matrix4f matrix =
                    diag::estimate_candidate_full_block_fused_abc_layout_gated(
                        source_xyz, target_xyzinormal, weights, &stats);
                return diag::matrix_checksum(matrix) +
                       static_cast<double>(stats.accepted_points) * 1e-6;
              }));
          return;
        }
        results.push_back(run_case_trace(
            "weighted lls production-shaped full-cloud block-fused-abc-ilp " +
                std::string(kCaseLabel) + " " + std::to_string(n),
            options.iterations,
            [&]() {
              diag::AccumulationStats stats;
              const Eigen::Matrix4f matrix =
                  diag::estimate_candidate_full_block_fused_abc_ilp_layout_gated(
                      source_xyz, target_xyzinormal, weights, &stats);
              return diag::matrix_checksum(matrix) +
                     static_cast<double>(stats.accepted_points) * 1e-6;
            }));
      };

      for (const auto& token : options.generic_abc_order)
        add_component_trace(token);
      for (const auto& token : options.generic_abc_order)
        add_production_trace(token);
      continue;
    }

    const bool fused_formula_only = options.case_filter == "fused-formula";

    results.push_back(run_case(
        "weighted lls full-cloud pointnormal " + std::to_string(n),
        options.iterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix =
              diag::estimate_candidate_full(source, target, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    results.push_back(run_case(
        "weighted lls full-cloud block-reduction pointnormal " + std::to_string(n),
        options.iterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_full_block_reduction(
              source, target, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    auto add_fused_formula_cases = [&](const std::string& case_name,
                                       auto accumulate,
                                       auto estimate) {
      results.push_back(run_case(
          "weighted lls component full-cloud " + case_name +
              " no-solve pointnormal " + std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const diag::NormalEquation eq = accumulate(source, target, weights, &stats);
            return normal_equation_checksum(eq) +
                   static_cast<double>(stats.used_rvv ? 1 : 0) * 1e-3;
          }));
      results.push_back(run_case(
          "weighted lls full-cloud " + case_name + " pointnormal " +
              std::to_string(n),
          options.iterations,
          [&]() {
            diag::AccumulationStats stats;
            const Eigen::Matrix4f matrix = estimate(source, target, weights, &stats);
            return diag::matrix_checksum(matrix) +
                   static_cast<double>(stats.accepted_points) * 1e-6;
          }));
    };

    add_fused_formula_cases("block-fused-abc",
                            diag::accumulate_candidate_full_block_fused_abc,
                            diag::estimate_candidate_full_block_fused_abc);
    add_fused_formula_cases("block-fused-abc-ilp",
                            diag::accumulate_candidate_full_block_fused_abc_ilp,
                            diag::estimate_candidate_full_block_fused_abc_ilp);
    add_fused_formula_cases("block-fused-d-six-term",
                            diag::accumulate_candidate_full_block_fused_d_six_term,
                            diag::estimate_candidate_full_block_fused_d_six_term);
    add_fused_formula_cases(
        "block-fused-d-six-term-ilp",
        diag::accumulate_candidate_full_block_fused_d_six_term_ilp,
        diag::estimate_candidate_full_block_fused_d_six_term_ilp);
    add_fused_formula_cases(
        "block-fused-d-displacement",
        diag::accumulate_candidate_full_block_fused_d_displacement,
        diag::estimate_candidate_full_block_fused_d_displacement);
    add_fused_formula_cases(
        "block-fused-d-displacement-ilp",
        diag::accumulate_candidate_full_block_fused_d_displacement_ilp,
        diag::estimate_candidate_full_block_fused_d_displacement_ilp);
    add_fused_formula_cases("block-fused-abcd",
                            diag::accumulate_candidate_full_block_fused_abcd,
                            diag::estimate_candidate_full_block_fused_abcd);
    add_fused_formula_cases("block-fused-abcd-ilp",
                            diag::accumulate_candidate_full_block_fused_abcd_ilp,
                            diag::estimate_candidate_full_block_fused_abcd_ilp);

    if (fused_formula_only)
      continue;

    const pcl::Indices source_indices = diag::make_source_indices(source.size());
    results.push_back(run_case(
        "weighted lls source-indices pointnormal " + std::to_string(n),
        options.iterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_source_indices(
              source, source_indices, target, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    const pcl::Indices target_indices = diag::make_target_indices(source.size());
    results.push_back(run_case(
        "weighted lls dual-indices pointnormal " + std::to_string(n),
        options.iterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_dual_indices(
              source, source_indices, target, target_indices, weights, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));

    const pcl::Correspondences correspondences = diag::make_bench_correspondences(source.size());
    results.push_back(run_case(
        "weighted lls correspondences pointnormal " + std::to_string(n),
        options.iterations,
        [&]() {
          diag::AccumulationStats stats;
          const Eigen::Matrix4f matrix = diag::estimate_candidate_correspondences(
              source, target, correspondences, &stats);
          return diag::matrix_checksum(matrix) +
                 static_cast<double>(stats.accepted_points) * 1e-6;
        }));
  }

  return results;
}

} // namespace pcl::registration::rvv_te_pt2plane_lls_weighted_bench
