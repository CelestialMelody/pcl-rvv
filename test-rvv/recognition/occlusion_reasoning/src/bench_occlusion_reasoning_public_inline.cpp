/*
 * 本文件做什么：
 * 这是 public inline `pcl::occlusion_reasoning::filter()` 的 bench（性能测试）入口。
 * Std build 运行标量路径；RVV build 运行 shared RVV dispatch。它度量的是公开 inline
 * wrapper 的完整 wall time，因此包含 `copyPointCloud`，用来回答“把 RVV 接进公开头之后，
 * 端到端是否仍值得接入”。
 *
 * 证据边界：
 * 这里不计 ZBuffering 内部 depth map 构建；bench 只在调用前准备 organized scene 和 model，
 * 然后计时 `pcl::occlusion_reasoning::filter(scene, model, f, threshold)` 的完整 wrapper。
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/recognition/hv/occlusion_reasoning.h>
#include <vector>

#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
namespace pcl::detail
{
extern "C" void pcl_rvv_occlusion_reasoning_reset_test_hook();
extern "C" int pcl_rvv_occlusion_reasoning_last_test_hook();
} // namespace pcl::detail
#endif

namespace
{

template <typename Fn>
double
timeKernel(Fn&& fn, const int iterations, const int warmup_iterations)
{
  for (int i = 0; i < warmup_iterations; ++i)
    fn();
  const auto begin = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    fn();
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(end - begin).count() /
         static_cast<double>(iterations);
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeOrganizedScene(const int width, const int height)
{
  auto scene = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  scene->resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  scene->width = static_cast<std::uint32_t>(width);
  scene->height = static_cast<std::uint32_t>(height);
  scene->is_dense = true;
  const float cx = static_cast<float>(width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(height) / 2.0f - 0.5f;
  for (int u = 0; u < width; ++u)
  {
    for (int v = 0; v < height; ++v)
    {
      const float z = 1.0f + static_cast<float>((u * 19 + v * 23) % 300) * 0.01f;
      scene->at(u, v) = pcl::PointXYZ{(static_cast<float>(u) - cx + 0.25f) * z,
                                      (static_cast<float>(v) - cy + 0.25f) * z,
                                      z};
    }
  }
  return scene;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeModelPoints(const std::size_t count, const int width, const int height)
{
  auto model = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  model->resize(count);
  model->width = static_cast<std::uint32_t>(count);
  model->height = 1;
  model->is_dense = true;
  const float cx = static_cast<float>(width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(height) / 2.0f - 0.5f;
  for (std::size_t i = 0; i < count; ++i)
  {
    const int u = static_cast<int>((i * 37u + 11u) % static_cast<std::size_t>(width));
    const int v = static_cast<int>((i * 53u + 7u) % static_cast<std::size_t>(height));
    const float z = 1.0f + static_cast<float>((i * 17u) % 300u) * 0.01f;
    (*model)[i].z = z;
    (*model)[i].x = (static_cast<float>(u) - cx + 0.25f) * z;
    (*model)[i].y = (static_cast<float>(v) - cy + 0.25f) * z;
  }
  return model;
}

std::uint64_t
checksumCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto& point : cloud)
  {
    const auto* words = reinterpret_cast<const std::uint32_t*>(&point);
    for (int i = 0; i < 3; ++i)
    {
      result ^= static_cast<std::uint64_t>(words[i]);
      result *= 1099511628211ull;
    }
  }
  result ^= static_cast<std::uint64_t>(cloud.size());
  result *= 1099511628211ull;
  return result;
}

} // namespace

int
main(int argc, char** argv)
{
  const std::size_t point_count = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 65536;
  const int scene_width = argc > 2 ? std::atoi(argv[2]) : 150;
  const int scene_height = argc > 3 ? std::atoi(argv[3]) : 150;
  const int iterations = argc > 4 ? std::atoi(argv[4]) : 200;
  const int warmup_iterations = argc > 5 ? std::atoi(argv[5]) : 5;
  const float focal = 1.0f;
  const float threshold = 0.01f;

  const auto scene = makeOrganizedScene(scene_width, scene_height);
  const auto model = makeModelPoints(point_count, scene_width, scene_height);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr scene_const(scene);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr model_const(model);

  pcl::PointCloud<pcl::PointXYZ>::Ptr filtered;

  const double micros = timeKernel(
      [&] {
#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
        pcl::detail::pcl_rvv_occlusion_reasoning_reset_test_hook();
#endif
        filtered = pcl::occlusion_reasoning::filter<pcl::PointXYZ, pcl::PointXYZ>(
            scene_const, model_const, focal, threshold);
      },
      iterations,
      warmup_iterations);

  const auto checksum = checksumCloud(*filtered);
  std::cout << "Dataset: public inline occlusion filter; scene=" << scene_width << "x"
            << scene_height << "; model_points=" << point_count << "; PointXYZ/float/AoS\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  std::cout << "public_inline_filter_visible_points: " << micros << " ms/iter\n";
  std::cout << "  Total Time: " << (micros * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", kept: " << filtered->size() << "\n";
  return checksum == 0 ? 1 : 0;
}
