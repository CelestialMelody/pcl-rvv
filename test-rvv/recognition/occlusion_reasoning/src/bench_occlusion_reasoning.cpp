/*
 * 本文件做什么：
 * 这是 ZBuffering::filter() production direct（真实生产入口直连）
 * 的 bench（性能测试）入口。Std build 运行 production 标量路径；
 * RVV build 运行 production RVV dispatch（生产 RVV 分流）。输出采用共享 bench analyzer 能解析的 `Dataset`、
 * `Iterations`、计时行和 checksum（校验和）格式。
 *
 * 证据边界：
 * bench 在计时前调用真实 computeDepthMap() 构建成员 depth buffer，计时段只覆盖
 * `ZBuffering::filter(model, indices)` 真实公开成员入口，不包含 copyPointCloud。
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
makeModelPoints(const std::size_t count, const int depth_width, const int depth_height)
{
  auto points = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  points->resize(count);
  points->width = static_cast<std::uint32_t>(count);
  points->height = 1;
  points->is_dense = true;
  const float cx = static_cast<float>(depth_width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(depth_height) / 2.0f - 0.5f;
  for (std::size_t i = 0; i < count; ++i)
  {
    const int u = static_cast<int>((i * 37u + 11u) % static_cast<std::size_t>(depth_width));
    const int v = static_cast<int>((i * 53u + 7u) % static_cast<std::size_t>(depth_height));
    const float z = 1.0f + static_cast<float>((i * 17u) % 300u) * 0.01f;
    (*points)[i].z = z;
    (*points)[i].x = (static_cast<float>(u) - cx + 0.25f) * z;
    (*points)[i].y = (static_cast<float>(v) - cy + 0.25f) * z;
  }
  return points;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeSceneForDepth(const int width, const int height)
{
  auto scene = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  scene->reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
  scene->width = static_cast<std::uint32_t>(width * height);
  scene->height = 1;
  scene->is_dense = true;
  const float cx = static_cast<float>(width) / 2.0f - 0.5f;
  const float cy = static_cast<float>(height) / 2.0f - 0.5f;
  for (int u = 0; u < width; ++u)
  {
    for (int v = 0; v < height; ++v)
    {
      const float z = 1.0f + static_cast<float>((u * 19 + v * 23) % 300) * 0.01f;
      pcl::PointXYZ point;
      point.z = z;
      point.x = (static_cast<float>(u) - cx + 0.25f) * z;
      point.y = (static_cast<float>(v) - cy + 0.25f) * z;
      scene->push_back(point);
    }
  }
  return scene;
}

std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto index : indices)
  {
    result ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(index));
    result *= 1099511628211ull;
  }
  return result;
}

} // namespace

int
main(int argc, char** argv)
{
  const std::size_t point_count = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 65536;
  const int depth_width = argc > 2 ? std::atoi(argv[2]) : 150;
  const int depth_height = argc > 3 ? std::atoi(argv[3]) : 150;
  const int iterations = argc > 4 ? std::atoi(argv[4]) : 200;
  const int warmup_iterations = argc > 5 ? std::atoi(argv[5]) : 5;
  const float focal = 1.0f;
  const float threshold = 0.01f;

  const auto scene = makeSceneForDepth(depth_width, depth_height);
  const auto model = makeModelPoints(point_count, depth_width, depth_height);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr scene_const(scene);
  pcl::PointCloud<pcl::PointXYZ>::ConstPtr model_const(model);
  pcl::occlusion_reasoning::ZBuffering<pcl::PointXYZ, pcl::PointXYZ> zbuffer(
      depth_width, depth_height, focal);
  zbuffer.computeDepthMap(scene_const, false, false);

  pcl::Indices indices;
  std::size_t vector_chunks = 0;

  const double micros = timeKernel(
      [&] {
#if defined(__RVV10__)
#if defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
        pcl::detail::pcl_rvv_occlusion_reasoning_reset_test_hook();
#endif
#else
        vector_chunks = 0;
#endif
        zbuffer.filter(model_const, indices, threshold);
#if defined(__RVV10__) && defined(PCL_RVV_OCCLUSION_REASONING_TEST_HOOK)
        if (pcl::detail::pcl_rvv_occlusion_reasoning_last_test_hook() == 2)
          vector_chunks = point_count;
#endif
      },
      iterations,
      warmup_iterations);

  const auto checksum = checksumIndices(indices);
  std::cout << "Dataset: production direct ZBuffering filter indices; scene=" << depth_width << "x"
            << depth_height << "; model_points=" << point_count << "; PointXYZ/float/AoS\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  std::cout << "production_filter_indices_projection_mask_compress: " << micros
            << " ms/iter\n";
  std::cout << "  Total Time: " << (micros * static_cast<double>(iterations))
            << " ms, checksum: " << checksum << ", kept: " << indices.size()
            << ", vector_chunks: " << vector_chunks << "\n";
  return checksum == 0 ? 1 : 0;
}
