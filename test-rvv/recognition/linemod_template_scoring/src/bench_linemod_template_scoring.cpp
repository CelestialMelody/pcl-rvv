#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "linemod_template_scoring.h"

namespace
{

std::vector<std::vector<std::uint8_t>>
makeScoreMaps (const std::size_t nr_maps, const std::size_t mem_size)
{
  std::vector<std::vector<std::uint8_t>> maps (nr_maps, std::vector<std::uint8_t> (mem_size));
  for (std::size_t map_index = 0; map_index < nr_maps; ++map_index)
  {
    for (std::size_t value_index = 0; value_index < mem_size; ++value_index)
      maps[map_index][value_index] = static_cast<std::uint8_t> ((map_index * 7 + value_index * 3 + 2) % 5);
  }
  return maps;
}

std::vector<const std::uint8_t*>
mapPointers (const std::vector<std::vector<std::uint8_t>>& maps)
{
  std::vector<const std::uint8_t*> pointers;
  pointers.reserve (maps.size ());
  for (const auto& map : maps)
    pointers.push_back (map.data ());
  return pointers;
}

std::vector<std::uint8_t>
makeQuantizedMap (const std::size_t map_size)
{
  std::vector<std::uint8_t> quantized (map_size);
  for (std::size_t index = 0; index < map_size; ++index)
  {
    const auto pattern = static_cast<std::uint8_t> ((index * 37 + (index >> 3) * 11 + 0x5a) & 0xff);
    quantized[index] = static_cast<std::uint8_t> (pattern ^ static_cast<std::uint8_t> (1u << (index & 7u)));
  }
  return quantized;
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

std::uint64_t
checksum (const std::vector<std::uint16_t>& values)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto value : values)
  {
    result ^= static_cast<std::uint64_t> (value);
    result *= 1099511628211ull;
  }
  return result;
}

std::uint64_t
checksumBytes (const std::vector<std::uint8_t>& values)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto value : values)
  {
    result ^= static_cast<std::uint64_t> (value);
    result *= 1099511628211ull;
  }
  return result;
}

std::size_t
chooseLinearizedWidth (const std::size_t mem_size)
{
  std::size_t lin_width = std::min<std::size_t> (64, mem_size == 0 ? 1 : mem_size);
  while (lin_width > 1 && mem_size % lin_width != 0)
    --lin_width;
  return lin_width;
}

std::uint64_t
checksumIndices (const std::vector<std::size_t>& values)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto value : values)
  {
    result ^= static_cast<std::uint64_t> (value);
    result *= 1099511628211ull;
  }
  return result;
}

} // namespace

int
main (int argc, char** argv)
{
  const std::size_t mem_size = argc > 1 ? std::strtoull (argv[1], nullptr, 10) : 4096;
  const std::size_t nr_maps = argc > 2 ? std::strtoull (argv[2], nullptr, 10) : 96;
  const int iterations = argc > 3 ? std::atoi (argv[3]) : 200;
  const int warmup_iterations = argc > 4 ? std::atoi (argv[4]) : 5;

  const auto maps = makeScoreMaps (nr_maps, mem_size);
  const auto pointers = mapPointers (maps);
  const auto quantized = makeQuantizedMap (mem_size);
  const std::size_t step_size = 8;
  const std::size_t lin_width = chooseLinearizedWidth (mem_size);
  const std::size_t lin_height = mem_size / lin_width;
  const std::size_t source_width = lin_width * step_size;
  const std::size_t source_height = lin_height * step_size;
  const auto linearized_source = makeQuantizedMap (source_width * source_height);
  std::vector<std::uint16_t> scores (mem_size, 0);
  std::vector<std::uint8_t> energy_maps (mem_size * 8, 0);
  std::vector<std::uint8_t> linearized_maps (mem_size * step_size * step_size, 0);
  std::vector<std::size_t> candidate_indices;
  const std::uint16_t raw_threshold = static_cast<std::uint16_t> (nr_maps * 2);

#if defined(LINEMOD_SCORE_BENCH_FULL_CHAIN_LINEARIZED_COPY_ONLY)
  const std::size_t nr_bins = 8;
  const std::size_t source_map_size = source_width * source_height;
  const std::size_t linearized_map_count = nr_bins * step_size * step_size;
  const std::size_t scoring_map_count = std::min (nr_maps, linearized_map_count);
  const auto full_chain_quantized = makeQuantizedMap (source_map_size);
  std::vector<std::uint8_t> full_chain_energy_maps (nr_bins * source_map_size, 0);
  std::vector<std::uint8_t> full_chain_linearized_maps (linearized_map_count * mem_size, 0);
  std::vector<std::uint16_t> full_chain_scores (mem_size, 0);
  std::vector<const std::uint8_t*> full_chain_pointers;
  full_chain_pointers.reserve (scoring_map_count);
  for (std::size_t map_index = 0; map_index < scoring_map_count; ++map_index)
    full_chain_pointers.push_back (full_chain_linearized_maps.data () + map_index * mem_size);
  const std::uint16_t full_chain_threshold = static_cast<std::uint16_t> (scoring_map_count * 2);

  const double full_chain_energy_micros = timeKernel ([&] {
    pcl::test::linemod_rvv::buildEnergyMapsStd (
        full_chain_quantized.data (), source_map_size, nr_bins, full_chain_energy_maps.data ());
  }, iterations, warmup_iterations);

  pcl::test::linemod_rvv::buildEnergyMapsStd (
      full_chain_quantized.data (), source_map_size, nr_bins, full_chain_energy_maps.data ());
  const double full_chain_linearized_micros = timeKernel ([&] {
#ifdef __RVV10__
    pcl::test::linemod_rvv::linearizeEnergyMapsRVV (full_chain_energy_maps.data (),
                                                    source_width,
                                                    source_height,
                                                    nr_bins,
                                                    step_size,
                                                    full_chain_linearized_maps.data ());
#else
    pcl::test::linemod_rvv::linearizeEnergyMapsStd (full_chain_energy_maps.data (),
                                                    source_width,
                                                    source_height,
                                                    nr_bins,
                                                    step_size,
                                                    full_chain_linearized_maps.data ());
#endif
  }, iterations, warmup_iterations);

#ifdef __RVV10__
  pcl::test::linemod_rvv::linearizeEnergyMapsRVV (full_chain_energy_maps.data (),
                                                  source_width,
                                                  source_height,
                                                  nr_bins,
                                                  step_size,
                                                  full_chain_linearized_maps.data ());
#else
  pcl::test::linemod_rvv::linearizeEnergyMapsStd (full_chain_energy_maps.data (),
                                                  source_width,
                                                  source_height,
                                                  nr_bins,
                                                  step_size,
                                                  full_chain_linearized_maps.data ());
#endif
  const double full_chain_accumulate_micros = timeKernel ([&] {
    pcl::test::linemod_rvv::accumulateScoreMapsStd (full_chain_pointers.data (),
                                                    full_chain_pointers.size (),
                                                    mem_size,
                                                    full_chain_scores.data ());
  }, iterations, warmup_iterations);

  pcl::test::linemod_rvv::accumulateScoreMapsStd (full_chain_pointers.data (),
                                                  full_chain_pointers.size (),
                                                  mem_size,
                                                  full_chain_scores.data ());
  const double full_chain_scan_micros = timeKernel ([&] {
    pcl::test::linemod_rvv::scanScoresStd (
        full_chain_scores.data (), full_chain_scores.size (), full_chain_threshold, candidate_indices);
  }, iterations, warmup_iterations);

  const double full_chain_total_micros = timeKernel ([&] {
    pcl::test::linemod_rvv::buildEnergyMapsStd (
        full_chain_quantized.data (), source_map_size, nr_bins, full_chain_energy_maps.data ());
#ifdef __RVV10__
    pcl::test::linemod_rvv::linearizeEnergyMapsRVV (full_chain_energy_maps.data (),
                                                    source_width,
                                                    source_height,
                                                    nr_bins,
                                                    step_size,
                                                    full_chain_linearized_maps.data ());
#else
    pcl::test::linemod_rvv::linearizeEnergyMapsStd (full_chain_energy_maps.data (),
                                                    source_width,
                                                    source_height,
                                                    nr_bins,
                                                    step_size,
                                                    full_chain_linearized_maps.data ());
#endif
    pcl::test::linemod_rvv::accumulateScoreMapsStd (full_chain_pointers.data (),
                                                    full_chain_pointers.size (),
                                                    mem_size,
                                                    full_chain_scores.data ());
    pcl::test::linemod_rvv::scanScoresStd (
        full_chain_scores.data (), full_chain_scores.size (), full_chain_threshold, candidate_indices);
  }, iterations, warmup_iterations);

  const auto full_chain_summary = pcl::test::linemod_rvv::scanScoresStd (
      full_chain_scores.data (), full_chain_scores.size (), full_chain_threshold, candidate_indices);

  std::cout << "Dataset: synthetic full-chain split LINEMOD byte maps"
            << " (mem_size=" << mem_size
            << ", nr_maps=" << scoring_map_count << ")\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
  std::cout << "LINEMOD full-chain energy map generation: " << full_chain_energy_micros << " us / iter\n";
  std::cout << "LINEMOD full-chain linearized map copy: " << full_chain_linearized_micros << " us / iter\n";
  std::cout << "LINEMOD full-chain score accumulation: " << full_chain_accumulate_micros << " us / iter\n";
  std::cout << "LINEMOD full-chain score scan: " << full_chain_scan_micros << " us / iter\n";
  std::cout << "LINEMOD full-chain total: " << full_chain_total_micros << " us / iter\n";
  std::cout << "checksum=" << checksum (full_chain_scores)
            << " mode=" <<
#ifdef __RVV10__
      "rvv"
#else
      "std"
#endif
            << " max_value=" << full_chain_summary.max_value
            << " max_index=" << full_chain_summary.max_index
            << " count_above_threshold=" << full_chain_summary.count_above_threshold
            << " index_checksum=" << checksumIndices (candidate_indices)
            << " energy_checksum=" << checksumBytes (full_chain_energy_maps)
            << " linearized_checksum=" << checksumBytes (full_chain_linearized_maps)
            << '\n';
  return 0;
#endif

#if !defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
  const double energy_micros = timeKernel ([&] {
#ifdef __RVV10__
    pcl::test::linemod_rvv::buildEnergyMapsRVV (quantized.data (), quantized.size (), 8, energy_maps.data ());
#else
    pcl::test::linemod_rvv::buildEnergyMapsStd (quantized.data (), quantized.size (), 8, energy_maps.data ());
#endif
  }, iterations, warmup_iterations);
#endif

#if defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
  const double linearized_micros = timeKernel ([&] {
#ifdef __RVV10__
    pcl::test::linemod_rvv::linearizeEnergyMapRVV (
        linearized_source.data (), source_width, source_height, step_size, linearized_maps.data ());
#else
    pcl::test::linemod_rvv::linearizeEnergyMapStd (
        linearized_source.data (), source_width, source_height, step_size, linearized_maps.data ());
#endif
  }, iterations, warmup_iterations);
#endif

#if !defined(LINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY) && !defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
  const double accumulate_micros = timeKernel ([&] {
#ifdef __RVV10__
    pcl::test::linemod_rvv::accumulateScoreMapsRVV (pointers.data (), pointers.size (), mem_size, scores.data ());
#else
    pcl::test::linemod_rvv::accumulateScoreMapsStd (pointers.data (), pointers.size (), mem_size, scores.data ());
#endif
  }, iterations, warmup_iterations);
#endif

#if !defined(LINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY) && !defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
#ifdef __RVV10__
  pcl::test::linemod_rvv::accumulateScoreMapsRVV (pointers.data (), pointers.size (), mem_size, scores.data ());
#else
    pcl::test::linemod_rvv::accumulateScoreMapsStd (pointers.data (), pointers.size (), mem_size, scores.data ());
#endif

#if defined(LINEMOD_SCORE_BENCH_ACCUMULATION_ONLY)
  const auto summary = pcl::test::linemod_rvv::summarizeScores (scores.data (), scores.size (), raw_threshold);
#else
  const double scan_micros = timeKernel ([&] {
#ifdef __RVV10__
    pcl::test::linemod_rvv::scanScoresRVV (scores.data (), scores.size (), raw_threshold, candidate_indices);
#else
    pcl::test::linemod_rvv::scanScoresStd (scores.data (), scores.size (), raw_threshold, candidate_indices);
#endif
  }, iterations, warmup_iterations);

  const double combined_micros = timeKernel ([&] {
#ifdef __RVV10__
    pcl::test::linemod_rvv::accumulateScoreMapsRVV (pointers.data (), pointers.size (), mem_size, scores.data ());
    pcl::test::linemod_rvv::scanScoresRVV (scores.data (), scores.size (), raw_threshold, candidate_indices);
#else
    pcl::test::linemod_rvv::accumulateScoreMapsStd (pointers.data (), pointers.size (), mem_size, scores.data ());
    pcl::test::linemod_rvv::scanScoresStd (scores.data (), scores.size (), raw_threshold, candidate_indices);
#endif
  }, iterations, warmup_iterations);

  const auto summary =
#ifdef __RVV10__
      pcl::test::linemod_rvv::scanScoresRVV (scores.data (), scores.size (), raw_threshold, candidate_indices);
#else
      pcl::test::linemod_rvv::scanScoresStd (scores.data (), scores.size (), raw_threshold, candidate_indices);
#endif
#endif
#else
  const pcl::test::linemod_rvv::ScoreSummary summary;
#endif
  std::cout << "Dataset: synthetic linearized LINEMOD score maps"
            << " (mem_size=" << mem_size
            << ", nr_maps=" << nr_maps << ")\n";
  std::cout << "Iterations: " << iterations << '\n';
  std::cout << "Warmup Iterations: " << warmup_iterations << '\n';
#if !defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
  std::cout << "LINEMOD energy map generation: " << energy_micros << " us / iter\n";
#endif
#if defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
  std::cout << "LINEMOD linearized map copy: " << linearized_micros << " us / iter\n";
#endif
#if !defined(LINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY) && !defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
  std::cout << "LINEMOD score accumulation: " << accumulate_micros << " us / iter\n";
#if !defined(LINEMOD_SCORE_BENCH_ACCUMULATION_ONLY)
  std::cout << "LINEMOD score scan: " << scan_micros << " us / iter\n";
  std::cout << "LINEMOD accumulate+scan: " << combined_micros << " us / iter\n";
#endif
#endif
  std::cout << "checksum=" << checksum (scores)
            << " mode=" <<
#ifdef __RVV10__
      "rvv"
#else
      "std"
#endif
            << " max_value=" << summary.max_value
            << " max_index=" << summary.max_index
#if !defined(LINEMOD_SCORE_BENCH_ACCUMULATION_ONLY) && !defined(LINEMOD_SCORE_BENCH_ENERGY_MAP_ONLY) && !defined(LINEMOD_SCORE_BENCH_LINEARIZED_COPY_ONLY)
            << " count_above_threshold=" << summary.count_above_threshold
            << " index_checksum=" << checksumIndices (candidate_indices)
#endif
            << " energy_checksum=" << checksumBytes (energy_maps)
            << " linearized_checksum=" << checksumBytes (linearized_maps)
            << '\n';

  return 0;
}
