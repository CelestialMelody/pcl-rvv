/*
 * RangeImageBorderExtractor（距离图边界提取器）score-update 诊断 benchmark。
 *
 * 输出格式兼容 test-rvv/script/analyze_bench_compare.py。Std build 编译为
 * scalar reference（标量参考），RVV build 编译为 test-only RVV candidate（测试专用
 * RVV 候选），两侧使用相同 case 名和输入构造，便于板卡 repeated benchmark 做同边界
 * A/B 对比。这个 benchmark 只计时连续 float score image（分数图像）的 3x3 邻域传播，
 * 不覆盖 RangeImage、LocalSurface、shadow/veil 状态机或 public computeFeature 入口。
 */

#include "range_image_border_extractor.h"
#include "impl/range_image_border_extractor_range_fixture.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace ribe = pcl::features::rvv_test::range_image_border_extractor;

namespace
{
using Clock = std::chrono::steady_clock;

struct Options
{
  int repeat = 8;
  int iterations = 8;
  int warmup = 2;
  std::string case_filter = "all";
};

struct BenchCase
{
  const char* name;
  std::size_t width;
  std::size_t height;
  float minimum_border_probability;
  int pattern;
};

bool
hasArgValue(const int argc, char** argv, const char* key, int& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = std::atoi(argv[i + 1]);
      return true;
    }
  }
  return false;
}

bool
hasArgValue(const int argc, char** argv, const char* key, std::string& value)
{
  for (int i = 1; i + 1 < argc; ++i)
  {
    if (std::strcmp(argv[i], key) == 0)
    {
      value = argv[i + 1];
      return true;
    }
  }
  return false;
}

Options
parseOptions(const int argc, char** argv)
{
  Options options;
  hasArgValue(argc, argv, "--repeat", options.repeat);
  hasArgValue(argc, argv, "--iterations", options.iterations);
  hasArgValue(argc, argv, "--warmup", options.warmup);
  hasArgValue(argc, argv, "--case-filter", options.case_filter);
  options.repeat = std::max(options.repeat, 1);
  options.iterations = std::max(options.iterations, 1);
  options.warmup = std::max(options.warmup, 0);
  return options;
}

bool
caseEnabled(const Options& options, const std::string& name)
{
  if (options.case_filter == "all")
    return true;
  std::size_t start = 0;
  while (start <= options.case_filter.size())
  {
    const std::size_t end = options.case_filter.find(',', start);
    const std::string token =
        options.case_filter.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (token == name)
      return true;
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return false;
}

std::vector<float>
makeScoreImage(const BenchCase& bench_case)
{
  std::vector<float> scores(bench_case.width * bench_case.height);
  for (std::size_t y = 0; y < bench_case.height; ++y)
  {
    for (std::size_t x = 0; x < bench_case.width; ++x)
    {
      const std::size_t hash =
          x * static_cast<std::size_t>(37 + bench_case.pattern) +
          y * static_cast<std::size_t>(53 + bench_case.pattern * 3) +
          (x ^ (y << 1)) * static_cast<std::size_t>(11 + bench_case.pattern);
      float value = static_cast<float>(hash % 2048) / 2047.0f;
      value = value * 1.35f - 0.18f;
      if (((x + 3 * y + static_cast<std::size_t>(bench_case.pattern)) % 17) == 0)
        value = -value * 0.55f;
      if (((x * 5 + y * 7 + static_cast<std::size_t>(bench_case.pattern)) % 41) == 0)
        value = 0.02f;
      if (((x + y + static_cast<std::size_t>(bench_case.pattern)) % 67) == 0)
        value = 0.92f;
      scores[y * bench_case.width + x] = value;
    }
  }
  return scores;
}

float
checksumScores(const std::vector<float>& values)
{
  double checksum = 0.0;
  for (std::size_t i = 0; i < values.size(); ++i)
    checksum += static_cast<double>(values[i]) * static_cast<double>((i % 31) + 1);
  return static_cast<float>(checksum);
}

void
runSelectedUpdate(const std::vector<float>& input,
                  const BenchCase& bench_case,
                  std::vector<float>& output)
{
#if defined(__RVV10__)
  ribe::updateScoresRVV(input.data(),
                        bench_case.width,
                        bench_case.height,
                        bench_case.minimum_border_probability,
                        output.data());
#else
  ribe::updateScoresStd(input.data(),
                        bench_case.width,
                        bench_case.height,
                        bench_case.minimum_border_probability,
                        output.data());
#endif
}

template <typename Fn>
double
timeCase(const int warmup, const int iterations, Fn&& fn, double& checksum)
{
  for (int i = 0; i < warmup; ++i)
    checksum += fn();

  const auto t0 = Clock::now();
  for (int i = 0; i < iterations; ++i)
    checksum += fn();
  const auto t1 = Clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / static_cast<double>(iterations);
}

void
printCase(const std::string& name, const double ms_per_iter, const double checksum)
{
  std::cout << name << ": " << ms_per_iter << " ms / iter\n";
  std::cout << name << " checksum: " << checksum << "\n";
}

double
runLocalSurfaceOnce(const pcl::RangeImage& range_image, const float minimum_border_probability)
{
  pcl::RangeImageBorderExtractor extractor(&range_image);
  ribe::configureExtractor(extractor, minimum_border_probability);
  auto** surfaces = extractor.getSurfaceStructure();
  const std::size_t count =
      static_cast<std::size_t>(range_image.width) * static_cast<std::size_t>(range_image.height);
  return ribe::checksumSurfaceStructure(surfaces, count);
}

double
runBorderScoresAfterSurfaceOnce(const pcl::RangeImage& range_image,
                                const float minimum_border_probability,
                                double& elapsed_ms)
{
  pcl::RangeImageBorderExtractor extractor(&range_image);
  ribe::configureExtractor(extractor, minimum_border_probability);
  (void)extractor.getSurfaceStructure();

  const auto t0 = Clock::now();
  const float* left = extractor.getBorderScoresLeft();
  const float* right = extractor.getBorderScoresRight();
  const float* top = extractor.getBorderScoresTop();
  const float* bottom = extractor.getBorderScoresBottom();
  const auto t1 = Clock::now();

  const std::size_t count =
      static_cast<std::size_t>(range_image.width) * static_cast<std::size_t>(range_image.height);
  double checksum = 0.0;
  for (std::size_t i = 0; i < count; ++i)
  {
    checksum += static_cast<double>(left[i]) * static_cast<double>((i % 31) + 1);
    checksum += static_cast<double>(right[i]) * static_cast<double>((i % 31) + 4);
    checksum += static_cast<double>(top[i]) * static_cast<double>((i % 31) + 8);
    checksum += static_cast<double>(bottom[i]) * static_cast<double>((i % 31) + 12);
  }
  elapsed_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
  return checksum;
}

template <typename Fn>
double
timeRangeImageComponentCase(const Options& options, Fn&& fn, double& checksum)
{
  for (int i = 0; i < options.warmup; ++i)
    checksum += fn();

  double elapsed_ms = 0.0;
  for (int i = 0; i < options.iterations; ++i)
  {
    const auto t0 = Clock::now();
    for (int repeat = 0; repeat < options.repeat; ++repeat)
      checksum += fn();
    const auto t1 = Clock::now();
    elapsed_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
  }
  return elapsed_ms / static_cast<double>(options.iterations);
}

double
timeBorderScoresAfterSurfaceCase(const Options& options,
                                 const pcl::RangeImage& range_image,
                                 const float minimum_border_probability,
                                 double& checksum)
{
  double ignored_elapsed_ms = 0.0;
  for (int i = 0; i < options.warmup; ++i)
    checksum += runBorderScoresAfterSurfaceOnce(range_image, minimum_border_probability, ignored_elapsed_ms);

  double elapsed_ms = 0.0;
  for (int i = 0; i < options.iterations; ++i)
    for (int repeat = 0; repeat < options.repeat; ++repeat)
      checksum += runBorderScoresAfterSurfaceOnce(range_image, minimum_border_probability, elapsed_ms);
  return elapsed_ms / static_cast<double>(options.iterations);
}
} // namespace

int
main(int argc, char** argv)
{
  const Options options = parseOptions(argc, argv);
  const std::vector<BenchCase> cases = {
      {"score_update_320x240", 320, 240, 0.80f, 1},
      {"score_update_641x481_tail", 641, 481, 0.80f, 2},
      {"score_update_threshold_sign_mix", 257, 193, 0.62f, 3},
      {"score_pipeline_641x481_four_images", 641, 481, 0.80f, 4},
      {"range_image_score_generation_160x120", 160, 120, 0.80f, 8},
      {"range_image_generation_plus_update_160x120", 160, 120, 0.80f, 9},
      {"range_image_local_surface_160x120", 160, 120, 0.80f, 10},
      {"range_image_border_scores_after_surface_160x120", 160, 120, 0.80f, 11},
  };

  std::cout << "Dataset: synthetic range_image_border_extractor score images; repeat="
            << options.repeat
            << "; cases=320x240,641x481_tail,257x193_threshold_sign_mix,"
               "score_pipeline_641x481_four_images,range_image_score_generation_160x120,"
               "range_image_generation_plus_update_160x120,range_image_local_surface_160x120,"
               "range_image_border_scores_after_surface_160x120\n";
  std::cout << "Iterations: " << options.iterations << "\n";
  std::cout << "Warmup Iterations: " << options.warmup << "\n";

  for (const BenchCase& bench_case : cases)
  {
    if (!caseEnabled(options, bench_case.name))
      continue;

    const bool local_surface_case =
        std::strcmp(bench_case.name, "range_image_local_surface_160x120") == 0;
    const bool border_scores_after_surface_case =
        std::strcmp(bench_case.name, "range_image_border_scores_after_surface_160x120") == 0;
    if (local_surface_case || border_scores_after_surface_case)
    {
      const pcl::RangeImage range_image =
          ribe::makeRangeImageFixture(static_cast<std::uint32_t>(bench_case.width),
                                      static_cast<std::uint32_t>(bench_case.height));
      double checksum = 0.0;
      const double ms = local_surface_case ?
          timeRangeImageComponentCase(options,
                                      [&]() {
                                        return runLocalSurfaceOnce(range_image,
                                                                   bench_case.minimum_border_probability);
                                      },
                                      checksum) :
          timeBorderScoresAfterSurfaceCase(options,
                                           range_image,
                                           bench_case.minimum_border_probability,
                                           checksum);
      printCase(bench_case.name, ms, checksum);
      continue;
    }

    const bool range_generation_case =
        std::strcmp(bench_case.name, "range_image_score_generation_160x120") == 0;
    const bool range_generation_plus_update_case =
        std::strcmp(bench_case.name, "range_image_generation_plus_update_160x120") == 0;
    const std::vector<float> input =
        (range_generation_case || range_generation_plus_update_case) ? std::vector<float>() :
                                                                       makeScoreImage(bench_case);
    std::vector<float> output(bench_case.width * bench_case.height);
    ribe::ScoreImageSet input_set;
    if (std::strcmp(bench_case.name, "score_pipeline_641x481_four_images") == 0)
    {
      input_set.left = input;
      input_set.right = makeScoreImage({"right", bench_case.width, bench_case.height, 0.77f, 5});
      input_set.top = makeScoreImage({"top", bench_case.width, bench_case.height, 0.73f, 6});
      input_set.bottom = makeScoreImage({"bottom", bench_case.width, bench_case.height, 0.69f, 7});
    }
    double checksum = 0.0;
    const double ms = timeCase(options.warmup, options.iterations, [&]() {
      double local_checksum = 0.0;
      for (int repeat = 0; repeat < options.repeat; ++repeat)
      {
        if (range_generation_case || range_generation_plus_update_case)
        {
          const pcl::RangeImage range_image =
              ribe::makeRangeImageFixture(static_cast<std::uint32_t>(bench_case.width),
                                          static_cast<std::uint32_t>(bench_case.height));
          const ribe::ScoreImageSet generated_scores =
              ribe::extractProductionScoreImages(range_image, bench_case.minimum_border_probability);
          if (range_generation_plus_update_case)
          {
#if defined(__RVV10__)
            const ribe::ScoreImageSet output_set =
                ribe::updateScoreImageSetRVV(generated_scores,
                                             bench_case.width,
                                             bench_case.height,
                                             bench_case.minimum_border_probability);
#else
            const ribe::ScoreImageSet output_set =
                ribe::updateScoreImageSetStd(generated_scores,
                                             bench_case.width,
                                             bench_case.height,
                                             bench_case.minimum_border_probability);
#endif
            local_checksum += ribe::checksumScoreImageSet(output_set);
          }
          else
          {
            local_checksum += ribe::checksumScoreImageSet(generated_scores);
          }
        }
        else if (std::strcmp(bench_case.name, "score_pipeline_641x481_four_images") == 0)
        {
#if defined(__RVV10__)
          const ribe::ScoreImageSet output_set =
              ribe::updateScoreImageSetRVV(input_set,
                                           bench_case.width,
                                           bench_case.height,
                                           bench_case.minimum_border_probability);
#else
          const ribe::ScoreImageSet output_set =
              ribe::updateScoreImageSetStd(input_set,
                                           bench_case.width,
                                           bench_case.height,
                                           bench_case.minimum_border_probability);
#endif
          local_checksum += static_cast<double>(checksumScores(output_set.left));
          local_checksum += static_cast<double>(checksumScores(output_set.right));
          local_checksum += static_cast<double>(checksumScores(output_set.top));
          local_checksum += static_cast<double>(checksumScores(output_set.bottom));
        }
        else
        {
          runSelectedUpdate(input, bench_case, output);
          local_checksum += static_cast<double>(checksumScores(output));
        }
      }
      return local_checksum;
    }, checksum);
    printCase(bench_case.name, ms, checksum);
  }

  return 0;
}
