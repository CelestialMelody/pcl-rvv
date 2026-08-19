/*
 * 本文件做什么：
 * 这里是 gicp topic 的 correctness（正确性）入口。测试把 test-only RVV
 * diagnostic candidate（测试专用 RVV 诊断候选）和标量 reference（参考链路）
 * 放在同一局部数学链路中对拍。
 *
 * 证据边界：
 * 这些 TEST 不修改 production，也不证明 GICP public entry（公开入口）已经
 * 接入 RVV。它们只回答 residual / Mahalanobis dense-row 累加和 covariance
 * post-KNN 局部组件是否保持同构结果。
 */

#include "gicp.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/gicp.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>

namespace {

using pcl::registration::rvv_gicp_support::CovarianceResult;
using pcl::registration::rvv_gicp_support::HessianLoopResult;
using pcl::registration::rvv_gicp_support::ResidualResult;

#ifdef __RVV10__
constexpr bool kExpectRvvForNonEmptyInput = true;
#else
constexpr bool kExpectRvvForNonEmptyInput = false;
#endif

void
expect_residual_near(const ResidualResult& actual,
                     const ResidualResult& expected,
                     const double tolerance)
{
  EXPECT_NEAR(actual.f, expected.f, tolerance);
  for (std::size_t i = 0; i < actual.translation_gradient.size(); ++i)
    EXPECT_NEAR(actual.translation_gradient[i], expected.translation_gradient[i], tolerance)
        << "translation gradient index " << i;
  for (std::size_t i = 0; i < actual.dcost_drt.size(); ++i)
    EXPECT_NEAR(actual.dcost_drt[i], expected.dcost_drt[i], tolerance)
        << "dCost_dR_T index " << i;
}

void
expect_hessian_loop_near(const HessianLoopResult& actual,
                         const HessianLoopResult& expected,
                         const double tolerance)
{
  for (std::size_t i = 0; i < actual.translation_gradient.size(); ++i)
    EXPECT_NEAR(actual.translation_gradient[i], expected.translation_gradient[i], tolerance)
        << "translation gradient index " << i;
  for (std::size_t i = 0; i < actual.translation_hessian.size(); ++i)
    EXPECT_NEAR(actual.translation_hessian[i], expected.translation_hessian[i], tolerance)
        << "translation hessian index " << i;
  for (std::size_t i = 0; i < actual.dcost_drt.size(); ++i)
    EXPECT_NEAR(actual.dcost_drt[i], expected.dcost_drt[i], tolerance)
        << "dCost_dR_T index " << i;
  for (std::size_t i = 0; i < actual.dcost_drt_b.size(); ++i)
    EXPECT_NEAR(actual.dcost_drt_b[i], expected.dcost_drt_b[i], tolerance)
        << "dCost_dR_T*b index " << i;
  for (std::size_t i = 0; i < actual.hessian_rot_tmp.size(); ++i)
    EXPECT_NEAR(actual.hessian_rot_tmp[i], expected.hessian_rot_tmp[i], tolerance)
        << "hessian_rot_tmp index " << i;
}

void
expect_covariance_near(const CovarianceResult& actual,
                       const CovarianceResult& expected,
                       const double tolerance)
{
  ASSERT_EQ(actual.cov00.size(), expected.cov00.size());
  for (std::size_t i = 0; i < actual.cov00.size(); ++i) {
    EXPECT_NEAR(actual.cov00[i], expected.cov00[i], tolerance) << "cov00 index " << i;
    EXPECT_NEAR(actual.cov10[i], expected.cov10[i], tolerance) << "cov10 index " << i;
    EXPECT_NEAR(actual.cov11[i], expected.cov11[i], tolerance) << "cov11 index " << i;
    EXPECT_NEAR(actual.cov20[i], expected.cov20[i], tolerance) << "cov20 index " << i;
    EXPECT_NEAR(actual.cov21[i], expected.cov21[i], tolerance) << "cov21 index " << i;
    EXPECT_NEAR(actual.cov22[i], expected.cov22[i], tolerance) << "cov22 index " << i;
  }
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
make_public_target_cloud()
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->reserve(160);
  for (int z = 0; z < 4; ++z) {
    for (int y = 0; y < 5; ++y) {
      for (int x = 0; x < 8; ++x) {
        pcl::PointXYZ point;
        point.x = 0.05f * static_cast<float>(x) +
                  0.003f * std::sin(static_cast<float>(y + z));
        point.y = 0.04f * static_cast<float>(y) +
                  0.002f * std::cos(static_cast<float>(x + z));
        point.z = 0.06f * static_cast<float>(z) +
                  0.001f * std::sin(static_cast<float>(x + y));
        cloud->push_back(point);
      }
    }
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
make_public_source_cloud(const pcl::PointCloud<pcl::PointXYZ>& target)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->reserve(target.size());
  for (const auto& point : target) {
    pcl::PointXYZ shifted;
    shifted.x = point.x - 0.02f;
    shifted.y = point.y + 0.015f;
    shifted.z = point.z - 0.01f;
    cloud->push_back(shifted);
  }
  return cloud;
}

} // namespace

// residual functor 是 Newton/BFGS 每次 functor 调用都会扫描 correspondence 的
// 局部热点。这个测试覆盖 64K dense-row 形态，验证 RVV 规约误差保持在预算内。
TEST(GICPResidualDiagnostic, DenseRowsMatchScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const ResidualInput input = make_residual_input(65536);
  const ResidualResult expected = residual_accumulate_std(input);
  const ResidualResult actual = residual_accumulate_candidate(input);

  expect_residual_near(actual, expected, 1e-8);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// 非 2 的幂规模覆盖 RVV tail（尾段）处理。失败通常说明 vl 分块或最后一个
// chunk（分块）没有和标量参考链路一致。
TEST(GICPResidualDiagnostic, TailRowsMatchScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const ResidualInput input = make_residual_input(4099);
  const ResidualResult expected = residual_accumulate_std(input);
  const ResidualResult actual = residual_accumulate_candidate(input);

  expect_residual_near(actual, expected, 1e-9);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// 空输入是 defensive boundary（防御性边界）。真实 GICP 在进入 functor 前应有
// correspondence 数量门禁；这里保护 helper 不会在无 lane 时声称命中 RVV。
TEST(GICPResidualDiagnostic, EmptyInputKeepsFallbackShape)
{
  using namespace pcl::registration::rvv_gicp_support;

  const ResidualInput input = make_residual_input(0);
  const ResidualResult expected = residual_accumulate_std(input);
  const ResidualResult actual = residual_accumulate_candidate(input);

  expect_residual_near(actual, expected, 0.0);
  EXPECT_FALSE(actual.used_rvv);
}

// indexed gather 诊断模拟 production functor 的 tmp_idx_src_ / tmp_idx_tgt_
// 访问形态。它不证明 public entry，但能检查 dense-row 收益是否会被索引访存吞掉。
TEST(GICPResidualDiagnostic, IndexedGatherMatchesScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const IndexedResidualInput input = make_indexed_residual_input(32768);
  const ResidualResult expected = indexed_residual_accumulate_std(input);
  const ResidualResult actual = indexed_residual_accumulate_candidate(input);

  expect_residual_near(actual, expected, 1e-8);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

TEST(GICPResidualDiagnostic, IndexedGatherTailMatchesScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const IndexedResidualInput input = make_indexed_residual_input(4103);
  const ResidualResult expected = indexed_residual_accumulate_std(input);
  const ResidualResult actual = indexed_residual_accumulate_candidate(input);

  expect_residual_near(actual, expected, 1e-9);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// dfddf loop diagnostic（默认 Newton 的 Hessian 主循环诊断）覆盖
// `OptimizationFunctorWithIndices::dfddf()` 中逐 correspondence 的批量累加项。
// 后续 6x6 eigensolver 和旋转二阶导后处理仍留在 production 标量路径外。
TEST(GICPHessianDiagnostic, DenseRowsMatchScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const ResidualInput input = make_residual_input(32768);
  const HessianLoopResult expected = hessian_loop_accumulate_std(input);
  const HessianLoopResult actual = hessian_loop_accumulate_candidate(input);

  expect_hessian_loop_near(actual, expected, 1e-8);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

TEST(GICPHessianDiagnostic, TailRowsMatchScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const ResidualInput input = make_residual_input(4101);
  const HessianLoopResult expected = hessian_loop_accumulate_std(input);
  const HessianLoopResult actual = hessian_loop_accumulate_candidate(input);

  expect_hessian_loop_near(actual, expected, 1e-9);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// covariance post-KNN 候选只覆盖 nearestKSearch 返回邻居之后的 mean/cov 累加，
// 不覆盖 KdTree search 和后续 3x3 SVD。这个测试锁住 k=20 的默认 GICP 形态。
TEST(GICPCovarianceDiagnostic, DefaultKMatchesScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const CovarianceInput input = make_covariance_input(2048, 20);
  const CovarianceResult expected = covariance_post_knn_std(input);
  const CovarianceResult actual = covariance_post_knn_candidate(input);

  expect_covariance_near(actual, expected, 1e-12);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// k=7 覆盖小邻域和尾段组合。当前候选在每个点内部做短规约，因此小 k 是
// 预期高风险路径，必须先用 correctness 锁住再谈性能。
TEST(GICPCovarianceDiagnostic, SmallKMatchesScalarReference)
{
  using namespace pcl::registration::rvv_gicp_support;

  const CovarianceInput input = make_covariance_input(257, 7);
  const CovarianceResult expected = covariance_post_knn_std(input);
  const CovarianceResult actual = covariance_post_knn_candidate(input);

  expect_covariance_near(actual, expected, 1e-12);
  EXPECT_EQ(actual.used_rvv, kExpectRvvForNonEmptyInput);
}

// production direct smoke（生产公开入口冒烟）实例化真正的 gicp.hpp。
// 它不单独证明性能，只验证 gated RVV probe 不破坏 public align 行为。
TEST(GICPProductionDirect, PublicAlignPointXYZSmoke)
{
  using PointT = pcl::PointXYZ;

  const auto target = make_public_target_cloud();
  const auto source = make_public_source_cloud(*target);
  pcl::PointCloud<PointT> output;

  pcl::GeneralizedIterativeClosestPoint<PointT, PointT> reg;
  reg.setInputSource(source);
  reg.setInputTarget(target);
  reg.setMaximumIterations(20);
  reg.setMaximumOptimizerIterations(8);
  reg.setTransformationEpsilon(1e-7);
  reg.setCorrespondenceRandomness(12);
  reg.setNumberOfThreads(1);

  reg.align(output);

  ASSERT_TRUE(reg.hasConverged());
  EXPECT_EQ(output.size(), source->size());
  EXPECT_NEAR(reg.getFitnessScore(), 0.0, 1e-8);
  const auto transform = reg.getFinalTransformation();
  const double expected_transform[4][4] = {
      {1.0, 0.0, 0.0, 0.020000003278255463},
      {0.0, 1.0, 0.0, -0.015000001527369022},
      {0.0, 0.0, 1.0, 0.010000000707805157},
      {0.0, 0.0, 0.0, 1.0},
  };
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      EXPECT_NEAR(static_cast<double>(transform(row, col)),
                  expected_transform[row][col],
                  1e-6)
          << "transform(" << row << ", " << col << ")";
}
