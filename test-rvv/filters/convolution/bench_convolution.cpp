#include <pcl/filters/convolution.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t kBenchmarkBannerWidth = 110;

void printBanner(const char ch, const std::size_t width = kBenchmarkBannerWidth)
{
  std::cout << std::string(width, ch) << '\n';
}

Eigen::ArrayXf makeKernel(const int width)
{
  Eigen::ArrayXf kernel(width);
  const float sigma = static_cast<float>(width) / 6.0f;
  float sum = 0.0f;
  const int half = width / 2;
  for (int i = 0; i < width; ++i)
  {
    const float x = static_cast<float>(i - half);
    kernel[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
    sum += kernel[i];
  }
  return kernel / sum;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr makeCloud(const std::uint32_t width,
                                               const std::uint32_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = width;
  cloud->height = height;
  cloud->is_dense = true;
  cloud->resize(width * height);
  for (std::uint32_t r = 0; r < height; ++r)
  {
    for (std::uint32_t c = 0; c < width; ++c)
    {
      auto& p = (*cloud)(c, r);
      p.x = 0.001f * static_cast<float>(c) + 0.002f * static_cast<float>(r);
      p.y = std::sin(0.01f * static_cast<float>(c));
      p.z = std::cos(0.01f * static_cast<float>(r));
      p.intensity = 100.0f + 0.25f * static_cast<float>((c + r) % 256);
    }
  }
  return cloud;
}

float checksum(const pcl::PointCloud<pcl::PointXYZI>& cloud)
{
  float sum = 0.0f;
  for (const auto& p : cloud)
    if (std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z))
      sum += p.x * 0.125f + p.y * 0.25f + p.z * 0.5f + p.intensity * 0.03125f;
  return sum;
}

void runCase(const std::string& name,
             const std::uint32_t width,
             const std::uint32_t height,
             const int kernel_width,
             const int iterations,
             const bool rows,
             const int border_policy = pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_IGNORE)
{
  const auto input = makeCloud(width, height);
  const auto kernel = makeKernel(kernel_width);
  pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI> convolution;
  convolution.setInputCloud(input);
  convolution.setKernel(kernel);
  convolution.setBordersPolicy(border_policy);
  pcl::PointCloud<pcl::PointXYZI> output;

  if (rows)
    convolution.convolveRows(output);
  else
    convolution.convolveCols(output);

  const auto begin = Clock::now();
  float guard = 0.0f;
  for (int iter = 0; iter < iterations; ++iter)
  {
    if (rows)
      convolution.convolveRows(output);
    else
      convolution.convolveCols(output);
    guard += checksum(output) * 0.000001f;
  }
  const auto end = Clock::now();
  const double total_ms = std::chrono::duration<double, std::milli>(end - begin).count();

  const double time_per_iteration = total_ms / static_cast<double>(iterations);
  std::cout << std::left << std::setw(52) << name << ": " << std::fixed
            << std::setprecision(4) << time_per_iteration << " ms/iter\n";
  std::cout << "  Direction: " << (rows ? "rows" : "cols")
            << "; Border Policy: "
            << (border_policy == pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE
                    ? "duplicate"
                    : (border_policy == pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR
                           ? "mirror"
                           : "ignore"))
            << "; Image Size: " << width << " x " << height
            << "; Kernel Width: " << kernel_width
            << "; Total Time: " << std::setprecision(4) << total_ms << " ms"
            << "; Checksum: " << std::setprecision(6) << guard << "\n";
}

} // namespace

int main(int argc, char** argv)
{
  const int iterations = argc > 1 ? std::atoi(argv[1]) : 5;
  const bool full_dataset = argc > 2 && std::strcmp(argv[2], "full") == 0;
  printBanner('=');
  std::cout << " PCL filters/convolution Benchmark (Convolution<PointXYZI, PointXYZI> dense boundary policies)\n";
  std::cout << "Dataset: "
            << (full_dataset
                    ? "organized dense PointXYZI convolution; ignore baseline plus duplicate/mirror rows/cols cases"
                    : "organized dense PointXYZI convolution qemu-smoke; ignore baseline plus duplicate/mirror rows/cols cases")
            << "\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << " Args: ITERS [full] (omit for QEMU smoke; pass `20 full` for board dataset)\n";
#if defined(__RVV10__)
  std::cout << " Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << " Build: Std (__RVV10__ disabled)\n";
#endif
  printBanner('=');
  if (full_dataset)
  {
    runCase("640x240-k7-rows", 640, 240, 7, iterations, true);
    runCase("640x240-k7-cols", 640, 240, 7, iterations, false);
    runCase("1280x480-k15-rows", 1280, 480, 15, iterations, true);
    runCase("640x240-k7-rows-duplicate", 640, 240, 7, iterations, true,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE);
    runCase("640x240-k7-cols-duplicate", 640, 240, 7, iterations, false,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE);
    runCase("640x240-k7-rows-mirror", 640, 240, 7, iterations, true,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR);
    runCase("640x240-k7-cols-mirror", 640, 240, 7, iterations, false,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR);
  }
  else
  {
    runCase("128x64-k7-rows", 128, 64, 7, iterations, true);
    runCase("128x64-k7-cols", 128, 64, 7, iterations, false);
    runCase("256x128-k15-rows", 256, 128, 15, iterations, true);
    runCase("128x64-k7-rows-duplicate", 128, 64, 7, iterations, true,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE);
    runCase("128x64-k7-cols-duplicate", 128, 64, 7, iterations, false,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE);
    runCase("128x64-k7-rows-mirror", 128, 64, 7, iterations, true,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR);
    runCase("128x64-k7-cols-mirror", 128, 64, 7, iterations, false,
            pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR);
  }
  printBanner('=');
  return EXIT_SUCCESS;
}
