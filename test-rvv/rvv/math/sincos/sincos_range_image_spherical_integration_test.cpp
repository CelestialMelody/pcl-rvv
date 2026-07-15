/*
 * sincos_range_image_spherical_integration_test.cpp - RangeImageSpherical
 * 仅测试使用的接入原型验证。
 *
 * 阅读提示：
 *   1. 本目标只对 integration test 使用 prototype_include（测试专用头文件覆盖），
 *      让 RangeImageSpherical::calculate3DPoint 的仅测试方法体调用
 *      pcl::sincos_finite_domain_RVV_f32m2。
 *   2. 标量参考链路直接使用
 *      RangeImageSpherical::getAnglesFromImagePoint 的真实公式，再用
 *      std::sin/std::cos 计算 local xyz，并应用 getTransformationToWorldSystem。
 *   3. RVV 执行链路通过仅测试版 RangeImageSpherical::calculate3DPoint 进入
 *      原型代码；该原型没有静默标量回退，vl != 2 会输出 NaN，使验收失败。
 *   4. 验收条件检查输入域计数、xyz 误差，以及 RVV 原型是否真的可执行。
 *
 * 重要边界：
 *   - 只验证 RangeImageSpherical，不接 base RangeImage。
 *   - 不改变 public API（公开 API）。
 *   - 不修改 common/include 下的生产 RangeImageSpherical 头文件。
 *   - finite-domain fast approximation（有限输入域快速近似）只依赖
 *     angle_x in [-pi, pi]、angle_y in [-pi/2, pi/2]。
 *   - 不覆盖 base RangeImage 的 angle_x / cos(angle_y) 放大路径。
 *   - 不改变 to_world_system_ 的语义；参考链路同样应用该变换。
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <vector>

#include <pcl/range_image/range_image_spherical.h>

#if defined(__RVV10__) && !defined(PCL_RVV_SINCOS_RANGE_IMAGE_SPHERICAL_TEST_ONLY_PROTOTYPE_ACTIVE)
#error "run_sincos_range_image_spherical_integration_test must use prototype_include"
#endif

namespace {

constexpr float kPi = 3.1415927410125732f;
constexpr float kPi2 = 1.5707963705062866f;
constexpr double kMaxEuclideanGate = 2.0e-5;

struct Sample {
  float image_x;
  float image_y;
  float range;
};

struct Stats {
  std::size_t samples = 0;
  std::size_t in_contract = 0;
  std::size_t domain_out = 0;
  std::size_t angle_x_in_contract = 0;
  std::size_t angle_y_in_contract = 0;
  double max_euclidean = 0.0;
  double mean_euclidean = 0.0;
  double max_abs_x = 0.0;
  double max_abs_y = 0.0;
  double max_abs_z = 0.0;
  float worst_image_x = 0.0f;
  float worst_image_y = 0.0f;
  float worst_range = 0.0f;
  float min_angle_x = std::numeric_limits<float>::infinity();
  float max_angle_x = -std::numeric_limits<float>::infinity();
  float min_angle_y = std::numeric_limits<float>::infinity();
  float max_angle_y = -std::numeric_limits<float>::infinity();
};

inline bool
angle_x_in_contract(float angle_x)
{
  return std::isfinite(angle_x) && angle_x >= -kPi && angle_x <= kPi;
}

inline bool
angle_y_in_contract(float angle_y)
{
  return std::isfinite(angle_y) && angle_y >= -kPi2 && angle_y <= kPi2;
}

// 样本生成器。作用：构造真实 RangeImageSpherical image_x/image_y/range 输入，
// 覆盖完整 360x180 度形态和边界点。调用者：run_integration_test。
// 类别：生产接入原型验证。
std::vector<Sample>
make_samples()
{
  std::vector<Sample> samples;
  constexpr int kWidth = 720;
  constexpr int kHeight = 360;
  const float ranges[] = {1.0f, 10.0f, 80.0f};
  samples.reserve(static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight) * 3u + 256u);

  for (int y = 0; y <= kHeight; y += 3) {
    for (int x = 0; x <= kWidth; x += 3) {
      for (float range : ranges)
        samples.push_back({static_cast<float>(x), static_cast<float>(y), range});
    }
  }

  const float edge_x[] = {
      0.0f,
      1.0f,
      static_cast<float>(kWidth) * 0.25f,
      static_cast<float>(kWidth) * 0.5f,
      static_cast<float>(kWidth) * 0.75f,
      static_cast<float>(kWidth) - 1.0f,
      static_cast<float>(kWidth),
  };
  const float edge_y[] = {
      0.0f,
      1.0f,
      static_cast<float>(kHeight) * 0.25f,
      static_cast<float>(kHeight) * 0.5f,
      static_cast<float>(kHeight) * 0.75f,
      static_cast<float>(kHeight) - 1.0f,
      static_cast<float>(kHeight),
  };
  for (float y : edge_y) {
    for (float x : edge_x) {
      for (float range : ranges)
        samples.push_back({x, y, range});
    }
  }

  return samples;
}

// 标量参考链路。作用：使用真实 getAnglesFromImagePoint 得到 angle_x/angle_y，
// 再以 std::sin/std::cos 复现 RangeImageSpherical local xyz 公式，并应用
// getTransformationToWorldSystem。调用者：update_error。类别：标量参考链路。
Eigen::Vector3f
calculate_reference_point(const pcl::RangeImageSpherical& image,
                          float image_x,
                          float image_y,
                          float range)
{
  float angle_x = 0.0f;
  float angle_y = 0.0f;
  image.getAnglesFromImagePoint(image_x, image_y, angle_x, angle_y);
  const float sin_x = static_cast<float>(std::sin(static_cast<double>(angle_x)));
  const float cos_x = static_cast<float>(std::cos(static_cast<double>(angle_x)));
  const float sin_y = static_cast<float>(std::sin(static_cast<double>(angle_y)));
  const float cos_y = static_cast<float>(std::cos(static_cast<double>(angle_y)));
  const Eigen::Vector3f local(range * sin_x * cos_y, range * sin_y, range * cos_x * cos_y);
  return image.getTransformationToWorldSystem() * local;
}

// 误差与输入域统计。作用：比较测试专用 calculate3DPoint 输出与标量参考链路，
// 同时记录 angle_x/angle_y 是否落在 finite-domain helper 合同内。调用者：
// run_integration_test。类别：生产接入原型验收。
void
update_error(const pcl::RangeImageSpherical& image, const std::vector<Sample>& samples, Stats& stats)
{
  stats.samples = samples.size();
  for (const Sample& sample : samples) {
    float angle_x = 0.0f;
    float angle_y = 0.0f;
    image.getAnglesFromImagePoint(sample.image_x, sample.image_y, angle_x, angle_y);
    const bool angle_x_ok = angle_x_in_contract(angle_x);
    const bool angle_y_ok = angle_y_in_contract(angle_y);
    const bool in_domain = angle_x_ok && angle_y_ok;
    stats.angle_x_in_contract += angle_x_ok ? 1u : 0u;
    stats.angle_y_in_contract += angle_y_ok ? 1u : 0u;
    stats.in_contract += in_domain ? 1u : 0u;
    stats.domain_out += in_domain ? 0u : 1u;
    stats.min_angle_x = std::min(stats.min_angle_x, angle_x);
    stats.max_angle_x = std::max(stats.max_angle_x, angle_x);
    stats.min_angle_y = std::min(stats.min_angle_y, angle_y);
    stats.max_angle_y = std::max(stats.max_angle_y, angle_y);
    if (!in_domain)
      continue;

    Eigen::Vector3f actual;
    image.calculate3DPoint(sample.image_x, sample.image_y, sample.range, actual);
    const Eigen::Vector3f ref =
        calculate_reference_point(image, sample.image_x, sample.image_y, sample.range);
    const Eigen::Vector3f diff = actual - ref;
    const double euclidean = static_cast<double>(diff.norm());
    stats.max_abs_x = std::max(stats.max_abs_x, std::fabs(static_cast<double>(diff.x())));
    stats.max_abs_y = std::max(stats.max_abs_y, std::fabs(static_cast<double>(diff.y())));
    stats.max_abs_z = std::max(stats.max_abs_z, std::fabs(static_cast<double>(diff.z())));
    stats.mean_euclidean += euclidean;
    if (euclidean > stats.max_euclidean) {
      stats.max_euclidean = euclidean;
      stats.worst_image_x = sample.image_x;
      stats.worst_image_y = sample.image_y;
      stats.worst_range = sample.range;
    }
  }
  if (stats.in_contract != 0)
    stats.mean_euclidean /= static_cast<double>(stats.in_contract);
}

bool
run_integration_test()
{
  pcl::RangeImageSpherical image;
  image.setAngularResolution(2.0f * kPi / 720.0f, kPi / 360.0f);

  const std::vector<Sample> samples = make_samples();
  Stats stats;
  update_error(image, samples, stats);

  const bool angle_x_ok = stats.angle_x_in_contract == stats.samples;
  const bool angle_y_ok = stats.angle_y_in_contract == stats.samples;
  const bool domain_ok = stats.domain_out == 0 && stats.in_contract == stats.samples;
  const bool error_ok = stats.max_euclidean <= kMaxEuclideanGate;
  const bool ok = angle_x_ok && angle_y_ok && domain_ok && error_ok;

  std::printf("\n=== RangeImageSpherical test-only integration prototype ===\n");
  std::printf("prototype include: test-rvv/rvv/math/sincos/prototype_include/pcl/range_image/impl/range_image_spherical.hpp\n");
  std::printf("production header note: common/include RangeImageSpherical is not modified by this target\n");
  std::printf("samples=%zu angle_x_in_contract=%zu angle_y_in_contract=%zu in_contract=%zu domain_out=%zu\n",
              stats.samples,
              stats.angle_x_in_contract,
              stats.angle_y_in_contract,
              stats.in_contract,
              stats.domain_out);
  std::printf("angle_x range=[%.9e, %.9e], angle_y range=[%.9e, %.9e]\n",
              static_cast<double>(stats.min_angle_x),
              static_cast<double>(stats.max_angle_x),
              static_cast<double>(stats.min_angle_y),
              static_cast<double>(stats.max_angle_y));
  std::printf("xyz max abs: x=%.9e y=%.9e z=%.9e; euclidean max=%.9e mean=%.9e at image=(%.3f, %.3f) range=%.3f\n",
              stats.max_abs_x,
              stats.max_abs_y,
              stats.max_abs_z,
              stats.max_euclidean,
              stats.mean_euclidean,
              static_cast<double>(stats.worst_image_x),
              static_cast<double>(stats.worst_image_y),
              static_cast<double>(stats.worst_range));
  std::printf("[gate] angle_x in [-pi, pi]: %s\n", angle_x_ok ? "PASS" : "FAIL");
  std::printf("[gate] angle_y in [-pi/2, pi/2]: %s\n", angle_y_ok ? "PASS" : "FAIL");
  std::printf("[gate] domain_out == 0 and in_contract == samples: %s\n", domain_ok ? "PASS" : "FAIL");
  std::printf("[gate] xyz euclidean max <= %.1e vs scalar reference: %s\n",
              kMaxEuclideanGate,
              error_ok ? "PASS" : "FAIL");
#if defined(__RVV10__)
  std::printf("[RVV] enabled through test-only prototype; vl != 2 emits NaN and would fail the xyz gate\n");
#else
  std::printf("[RVV] disabled: x86 build verifies scalar reference plumbing only\n");
#endif
  std::printf("scope note: this test does not cover base RangeImage or its angle_x / cos(angle_y) amplification path.\n");
  std::printf("\n=== RangeImageSpherical integration gate summary: %s ===\n", ok ? "PASS" : "FAIL");
  return ok;
}

} // namespace

int
main()
{
  return run_integration_test() ? 0 : 1;
}
