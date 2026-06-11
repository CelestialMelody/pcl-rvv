#include "convolution_3d_diag.hpp"

#include <cstdlib>
#include <iostream>

namespace {

bool
check(bool condition, const char* message)
{
  if (!condition)
    std::cerr << "FAIL: " << message << '\n';
  return condition;
}

} // namespace

int
main()
{
  const auto cloud = pcl_rvv_filters_convolution_3d::makeCloud(12, 12, 12, true);
  pcl::Indices neighbors;
  std::vector<float> distances;
  for (int i = 0; i < 192; ++i) {
    neighbors.push_back(i);
    distances.push_back(static_cast<float>((i % 31) + 1) * 0.0001f);
  }

  const float sigma = 0.05f;
  const float sigma_sqr = sigma * sigma;
  const float threshold = 6.0f * 6.0f * sigma_sqr;
  const auto expected =
      pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZStd(cloud, neighbors, distances, sigma_sqr, threshold);
  auto actual = expected;
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
  const bool rvv_used = pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZRVV(
      cloud, neighbors, distances, sigma_sqr, threshold, actual);
#else
  const bool rvv_used = false;
#endif

  bool ok = true;
#if defined(__RVV10__) && defined(PCL_CONVOLUTION_3D_RVV_DIAGNOSTIC)
  ok &= check(rvv_used, "large kernel case should use RVV diagnostic path");
#else
  ok &= check(!rvv_used, "non-RVV build should not report RVV diagnostic path");
#endif
  ok &= check(expected.accepted == actual.accepted, "accepted neighbor count mismatch");
  ok &= check(std::abs(expected.x - actual.x) < 8e-4f, "x error exceeds budget");
  ok &= check(std::abs(expected.y - actual.y) < 8e-4f, "y error exceeds budget");
  ok &= check(std::abs(expected.z - actual.z) < 8e-4f, "z error exceeds budget");

  pcl::Indices small_neighbors(neighbors.begin(), neighbors.begin() + 8);
  std::vector<float> small_distances(distances.begin(), distances.begin() + 8);
  pcl_rvv_filters_convolution_3d::KernelResult small_actual;
  const bool small_rvv = pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZRVV(
      cloud, small_neighbors, small_distances, sigma_sqr, threshold, small_actual);
  ok &= check(!small_rvv, "small-neighbor fallback should stay scalar");
  const auto small_expected = pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZStd(
      cloud, small_neighbors, small_distances, sigma_sqr, threshold);
  small_actual = pcl_rvv_filters_convolution_3d::gaussianKernelPointXYZDispatch(
      cloud, small_neighbors, small_distances, sigma_sqr, threshold);
  ok &= check(small_expected.accepted == small_actual.accepted,
              "fallback after RVV path changed accepted count");
  ok &= check(std::abs(small_expected.x - small_actual.x) < 1e-7f,
              "fallback after RVV path changed scalar result");

  const auto query = pcl_rvv_filters_convolution_3d::makeIndices(cloud.size(), true);
  const auto std_full =
      pcl_rvv_filters_convolution_3d::convolveDiagnosticStd(cloud, query, 0.08, sigma_sqr, threshold);
  const auto rvv_full =
      pcl_rvv_filters_convolution_3d::convolveDiagnosticRVV(cloud, query, 0.08, sigma_sqr, threshold);
  const auto stats = pcl_rvv_filters_convolution_3d::compareCloud(std_full, rvv_full);
  ok &= check(stats.max_abs < 8e-4f, "full diagnostic max_abs exceeds budget");
  ok &= check(stats.max_rel < 5e-5f, "full diagnostic max_rel exceeds budget");

  std::cout << "convolution_3d diagnostic test passed"
            << " max_abs=" << stats.max_abs
            << " max_rel=" << stats.max_rel
            << " rvv_used=" << (rvv_used ? 1 : 0) << '\n';
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
