/*
 * 本文件做什么：
 * 这是 Harris 2D 首阶段 production-shaped diagnostic（生产形态诊断）的
 * bench（性能测试）入口。Std build 运行 scalar reference；RVV build 运行
 * candidate。它度量 organized response map（有组织响应图）子链路，不包含
 * production NMS sort、occupancy map 或 public dispatch。
 */

#include "harris_2d.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace h2d = pcl::keypoints::rvv_test::harris_2d;

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

void
setDetectorMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>& detector,
                  const h2d::ResponseMethod method)
{
  switch (method)
  {
    case h2d::ResponseMethod::Harris:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::HARRIS);
      break;
    case h2d::ResponseMethod::Noble:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::NOBLE);
      break;
    case h2d::ResponseMethod::Lowe:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::LOWE);
      break;
    case h2d::ResponseMethod::Tomasi:
      detector.setMethod(pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI>::TOMASI);
      break;
  }
}

float
maxAbsIntensityError(const h2d::ResponseImage& actual, const h2d::ResponseImage& expected)
{
  if (actual.intensity.size() != expected.intensity.size())
    return std::numeric_limits<float>::infinity();
  float max_error = 0.0f;
  for (std::size_t index = 0; index < actual.intensity.size(); ++index)
    max_error = std::max(max_error, std::fabs(actual.intensity[index] - expected.intensity[index]));
  return max_error;
}

void
runCase(const std::string& label,
        const std::size_t width,
        const std::size_t height,
        const h2d::ResponseMethod method,
        const int window_width,
        const int window_height,
        const int iterations,
        const int warmup_iterations,
        const bool public_entry)
{
  const auto input = h2d::makeSyntheticImage(width, height);
  h2d::ResponseImage output;
  std::size_t vector_chunks = 0;
  auto public_input = h2d::makePublicInputCloud(input);
  auto public_input_ptr = public_input.makeShared();
  pcl::HarrisKeypoint2D<pcl::PointXYZI, pcl::PointXYZI> detector;
  pcl::PointCloud<pcl::PointXYZI> public_output;
  if (public_entry)
  {
    detector.setInputCloud(public_input_ptr);
    detector.setWindowWidth(window_width);
    detector.setWindowHeight(window_height);
    detector.setNonMaxSupression(false);
    detector.setKSearch(1);
    setDetectorMethod(detector, method);
  }
  const double ms = timeKernel(
      [&] {
        std::size_t local_chunks = 0;
        if (public_entry)
        {
          detector.compute(public_output);
          vector_chunks = 0;
        }
        else
        {
        (void)h2d::computeResponsesCandidate(
              input, method, window_width, window_height, output, &local_chunks);
          vector_chunks = local_chunks;
        }
      },
      iterations,
      warmup_iterations);
  if (public_entry)
    output = h2d::makeResponseImage(public_output);
  h2d::ResponseImage expected;
  h2d::computeResponsesScalar(input, method, window_width, window_height, expected);
  const float max_abs_error = maxAbsIntensityError(output, expected);
  const float tolerance = public_entry ? 1e-3f : 1e-4f;

  std::cout << "Dataset: Harris 2D organized response diagnostic; case=" << label
            << "; width=" << width << "; height=" << height
            << "; method=" << h2d::methodName(method)
            << "; window=" << window_width << "x" << window_height
            << "; entry=" << (public_entry ? "public" : "candidate") << "\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  std::cout << label << ": " << std::fixed << std::setprecision(6) << ms << " ms/iter\n";
  std::cout << "  Total Time: " << (ms * static_cast<double>(iterations))
            << " ms, checksum: " << h2d::checksumResponses(output)
            << ", vector_chunks: " << vector_chunks << "\n";
  std::cout << "  Correctness: max_abs_error: " << std::setprecision(9) << max_abs_error
            << ", tolerance: " << tolerance
            << ", tolerance_pass: " << (max_abs_error <= tolerance ? "yes" : "no")
            << "\n";
}

bool
caseEnabled(const std::string& filter, const std::string& label)
{
  return filter == "all" || filter == label || filter.find(label) != std::string::npos;
}

} // namespace

int
main(int argc, char** argv)
{
  int iterations = 100;
  int warmup_iterations = 5;
  std::string case_filter = "all";
  bool public_entry = false;

  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--iterations" && i + 1 < argc)
      iterations = std::atoi(argv[++i]);
    else if (arg == "--warmup-iterations" && i + 1 < argc)
      warmup_iterations = std::atoi(argv[++i]);
    else if (arg == "--case-filter" && i + 1 < argc)
      case_filter = argv[++i];
    else if (arg == "--public-entry")
      public_entry = true;
  }

  if (caseEnabled(case_filter, "harris2d_harris_320x240"))
    runCase("harris2d_harris_320x240", 320, 240, h2d::ResponseMethod::Harris, 3, 3, iterations, warmup_iterations, public_entry);
  if (caseEnabled(case_filter, "harris2d_tomasi_320x240"))
    runCase("harris2d_tomasi_320x240", 320, 240, h2d::ResponseMethod::Tomasi, 3, 3, iterations, warmup_iterations, public_entry);
  if (caseEnabled(case_filter, "harris2d_noble_tail_641x481"))
    runCase("harris2d_noble_tail_641x481", 641, 481, h2d::ResponseMethod::Noble, 5, 5, iterations, warmup_iterations, public_entry);
  return 0;
}
