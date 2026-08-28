#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "impl/linemod_template_scoring_production_direct.hpp"

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
  const std::size_t mem_size = argc > 1 ? std::strtoull (argv[1], nullptr, 10) : 4096;
  const std::size_t nr_features = argc > 2 ? std::strtoull (argv[2], nullptr, 10) : 96;
  const int iterations = argc > 3 ? std::atoi (argv[3]) : 200;
  const int warmup_iterations = argc > 4 ? std::atoi (argv[4]) : 5;
  const std::size_t step_size = 8;
  const std::size_t lin_width = pcl::test::linemod_rvv::chooseProductionLinWidth (mem_size);
  const std::size_t lin_height = mem_size / lin_width;
  pcl::test::linemod_rvv::FixedQuantizableModality modality (lin_width * step_size, lin_height * step_size);
  std::vector<pcl::QuantizableModality*> modalities{&modality};
  auto linemod = pcl::test::linemod_rvv::makeProductionLinemod (nr_features);
  std::vector<pcl::LINEMODDetection> match_detections;
  std::vector<pcl::LINEMODDetection> detect_detections;
  std::vector<pcl::LINEMODDetection> semiscale_detections;

  const double match_micros = timeKernel ([&] {
    match_detections.clear ();
    linemod.matchTemplates (modalities, match_detections);
  }, iterations, warmup_iterations);

  const double detect_micros = timeKernel ([&] {
    detect_detections.clear ();
    linemod.detectTemplates (modalities, detect_detections);
  }, iterations, warmup_iterations);

  const double semiscale_micros = timeKernel ([&] {
    semiscale_detections.clear ();
    linemod.detectTemplatesSemiScaleInvariant (modalities, semiscale_detections, 1.0f, 2.0f, 2.0f);
  }, iterations, warmup_iterations);

  std::cout << "Dataset: production-direct LINEMOD public entries"
            << " (mem_size=" << mem_size
            << ", nr_maps=" << nr_features << ")\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  std::cout << "LINEMOD production matchTemplates total: " << match_micros << " us / iter\n";
  std::cout << "LINEMOD production detectTemplates total: " << detect_micros << " us / iter\n";
  std::cout << "LINEMOD production semi-scale detectTemplates total: " << semiscale_micros << " us / iter\n";
  std::cout << "checksum=" << pcl::test::linemod_rvv::checksumDetections (match_detections)
            << " mode=" <<
#ifdef __RVV10__
      "rvv"
#else
      "std"
#endif
            << " max_value=" << match_detections.size ()
            << " max_index=" << semiscale_detections.size ()
            << " count_above_threshold=" << detect_detections.size ()
            << " index_checksum=" << pcl::test::linemod_rvv::checksumDetections (semiscale_detections)
            << " energy_checksum=not_applicable"
            << " linearized_checksum=not_exposed"
            << '\n';

  return 0;
}
