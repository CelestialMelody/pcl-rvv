#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

#include "dotmod_template_matching.h"

namespace
{

std::vector<std::uint8_t>
makeImageMap (const std::size_t width, const std::size_t height)
{
  std::vector<std::uint8_t> values (width * height);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
      values[row * width + col] =
          static_cast<std::uint8_t> ((row * 13 + col * 31 + (row << 1) + (col >> 1)) & 0xff);
  }
  return values;
}

std::vector<std::uint8_t>
makeTemplateMap (const std::size_t width, const std::size_t height)
{
  std::vector<std::uint8_t> values (width * height);
  for (std::size_t index = 0; index < values.size (); ++index)
    values[index] = static_cast<std::uint8_t> (1u << (index & 7u));
  return values;
}

std::vector<std::vector<std::uint8_t>>
makeModalityMaps (const std::size_t width, const std::size_t height, const std::size_t nr_modalities)
{
  std::vector<std::vector<std::uint8_t>> maps;
  maps.reserve (nr_modalities);
  for (std::size_t modality = 0; modality < nr_modalities; ++modality)
  {
    auto map = makeImageMap (width, height);
    for (std::size_t index = 0; index < map.size (); ++index)
      map[index] = static_cast<std::uint8_t> ((map[index] << (modality % 3)) | (1u << (index % 8)));
    maps.push_back (std::move (map));
  }
  return maps;
}

std::vector<std::vector<std::vector<std::uint8_t>>>
makeTemplateBank (const std::size_t window_width,
                  const std::size_t window_height,
                  const std::size_t nr_templates,
                  const std::size_t nr_modalities)
{
  std::vector<std::vector<std::vector<std::uint8_t>>> templates;
  templates.reserve (nr_templates);
  for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
  {
    std::vector<std::vector<std::uint8_t>> modalities;
    modalities.reserve (nr_modalities);
    for (std::size_t modality = 0; modality < nr_modalities; ++modality)
    {
      auto templ = makeTemplateMap (window_width, window_height);
      for (std::size_t index = 0; index < templ.size (); ++index)
      {
        const auto shift = static_cast<unsigned> ((index + template_index + modality) % 8);
        templ[index] = static_cast<std::uint8_t> (1u << shift);
      }
      modalities.push_back (std::move (templ));
    }
    templates.push_back (std::move (modalities));
  }
  return templates;
}

void
mixDetections (std::uint64_t& checksum, const std::vector<pcl::test::dotmod_rvv::Detection>& detections)
{
  for (const auto& detection : detections)
  {
    checksum ^= static_cast<std::uint64_t> (detection.bin_x + 17 * detection.bin_y + 131 * detection.template_id);
    checksum *= 1099511628211ull;
    checksum ^= static_cast<std::uint64_t> (detection.score * 1000000.0f);
    checksum *= 1099511628211ull;
  }
  checksum ^= static_cast<std::uint64_t> (detections.size ());
  checksum *= 1099511628211ull;
}

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
  const std::size_t window_width = argc > 3 ? std::strtoull (argv[3], nullptr, 10) : 24;
  const std::size_t window_height = argc > 4 ? std::strtoull (argv[4], nullptr, 10) : 16;
  const int iterations = argc > 5 ? std::atoi (argv[5]) : 200;
  const int warmup_iterations = argc > 6 ? std::atoi (argv[6]) : 5;
  const std::size_t nr_templates = argc > 7 ? std::strtoull (argv[7], nullptr, 10) : 8;
  const std::size_t nr_modalities = argc > 8 ? std::strtoull (argv[8], nullptr, 10) : 2;
  const float threshold = argc > 9 ? std::strtof (argv[9], nullptr) : 0.90f;

  const auto image = makeImageMap (image_width, image_height);
  const auto templ = makeTemplateMap (window_width, window_height);
  const auto modality_maps = makeModalityMaps (image_width, image_height, nr_modalities);
  const auto templates = makeTemplateBank (window_width, window_height, nr_templates, nr_modalities);
  const std::size_t max_x = image_width > window_width ? image_width - window_width : 0;
  const std::size_t max_y = image_height > window_height ? image_height - window_height : 0;

  std::uint64_t checksum = 1469598103934665603ull;
  const auto mix_score = [&] (const std::uint32_t score) {
    checksum ^= static_cast<std::uint64_t> (score);
    checksum *= 1099511628211ull;
  };

  auto run_baseline = [&] {
    std::uint64_t local = 0;
    for (std::size_t y = 0; y < max_y; ++y)
    {
      for (std::size_t x = 0; x < max_x; ++x)
        local += pcl::test::dotmod_rvv::scoreWindowViaSubMapStd (
            image.data (), image_width, x, y, window_width, window_height, templ.data ());
    }
    mix_score (static_cast<std::uint32_t> (local));
  };

  auto run_direct = [&] {
    std::uint64_t local = 0;
    for (std::size_t y = 0; y < max_y; ++y)
    {
      for (std::size_t x = 0; x < max_x; ++x)
        local += pcl::test::dotmod_rvv::scoreWindowDirectRVV (
            image.data (), image_width, x, y, window_width, window_height, templ.data ());
    }
    mix_score (static_cast<std::uint32_t> (local));
  };

  auto run_full_chain = [&] {
#if defined(__RVV10__) && defined(__riscv_vector)
    const auto detections = pcl::test::dotmod_rvv::detectTemplatesDirectRVV (
        modality_maps, image_width, window_width, window_height, templates, threshold);
#else
    const auto detections = pcl::test::dotmod_rvv::detectTemplatesViaSubMapStd (
        modality_maps, image_width, window_width, window_height, templates, threshold);
#endif
    mixDetections (checksum, detections);
  };

  std::cout << "Dataset: DOTMOD synthetic direct-window image=" << image_width << "x" << image_height
            << " window=" << window_width << "x" << window_height << "\n";
  std::cout << "Templates: " << nr_templates << "\n";
  std::cout << "Modalities: " << nr_modalities << "\n";
  std::cout << "Threshold: " << threshold << "\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup_iterations << "\n";
  const double baseline_micros = timeKernel (run_baseline, iterations, warmup_iterations);
  const double direct_micros = timeKernel (run_direct, iterations, warmup_iterations);
  const double full_chain_micros = timeKernel (run_full_chain, iterations, warmup_iterations);
  std::cout << "DOTMOD submap baseline window score: " << baseline_micros << " us / iter\n";
  std::cout << "DOTMOD direct window score: " << direct_micros << " us / iter\n";
  std::cout << "DOTMOD full detectTemplates-shaped total: " << full_chain_micros << " us / iter\n";
  std::cout << "checksum: " << checksum << "\n";
  return 0;
}
