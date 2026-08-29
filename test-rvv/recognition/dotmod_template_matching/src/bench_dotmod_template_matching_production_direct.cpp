#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#include "impl/dotmod_template_matching_production_direct.hpp"

namespace
{

template <typename Fn>
double
timeKernel (Fn&& fn, const int iterations, const int warmup_iterations)
{
  for (int i = 0; i < warmup_iterations; ++i)
    fn ();
  const auto begin = std::chrono::steady_clock::now ();
  for (int i = 0; i < iterations; ++i)
    fn ();
  const auto end = std::chrono::steady_clock::now ();
  return std::chrono::duration<double, std::micro> (end - begin).count () / static_cast<double> (iterations);
}

} // namespace

int
main (int argc, char** argv)
{
  const std::size_t image_width = argc > 1 ? std::strtoull (argv[1], nullptr, 10) : 256;
  const std::size_t image_height = argc > 2 ? std::strtoull (argv[2], nullptr, 10) : 192;
  const std::size_t template_width = argc > 3 ? std::strtoull (argv[3], nullptr, 10) : 24;
  const std::size_t template_height = argc > 4 ? std::strtoull (argv[4], nullptr, 10) : 16;
  const int iterations = argc > 5 ? std::atoi (argv[5]) : 100;
  const int warmup_iterations = argc > 6 ? std::atoi (argv[6]) : 5;
  const std::size_t nr_templates = argc > 7 ? std::strtoull (argv[7], nullptr, 10) : 8;
  const std::size_t nr_modalities = argc > 8 ? std::strtoull (argv[8], nullptr, 10) : 2;
  const float threshold = argc > 9 ? std::strtof (argv[9], nullptr) : 0.90f;
  const std::size_t bin_size = argc > 10 ? std::strtoull (argv[10], nullptr, 10) : 1;

  auto modalities_storage = pcl::test::dotmod_rvv::makeFixedModalities (image_width, image_height, nr_modalities);
  auto modalities = pcl::test::dotmod_rvv::modalityPointers (modalities_storage);
  auto masks_storage = pcl::test::dotmod_rvv::makeFullMasks (image_width, image_height, nr_modalities);
  auto masks = pcl::test::dotmod_rvv::maskPointers (masks_storage);
  auto dotmod = pcl::test::dotmod_rvv::makeProductionDOTMOD (
      template_width, template_height, modalities, masks, nr_templates);
  std::vector<pcl::DOTMODDetection> detections;

  const double detect_micros = timeKernel ([&] {
    detections.clear ();
    dotmod.detectTemplates (modalities, threshold, detections, bin_size);
  }, iterations, warmup_iterations);

  std::cout << std::fixed << std::setprecision (3);
  std::cout << "Dataset: DOTMOD production-direct image=" << image_width << "x" << image_height
            << " template=" << template_width << "x" << template_height
            << " bin_size=" << bin_size << "\n";
  std::cout << "Templates: " << nr_templates << "\n";
  std::cout << "Modalities: " << nr_modalities << "\n";
  std::cout << "Threshold: " << threshold << "\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  std::cout << "DOTMOD production detectTemplates total: " << detect_micros << " us / iter\n";
  std::cout << "checksum: " << pcl::test::dotmod_rvv::checksumDetections (detections) << "\n";
  return 0;
}
