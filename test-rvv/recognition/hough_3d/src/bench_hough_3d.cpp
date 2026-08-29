#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "hough_3d.h"

#define PCL_RVV_HOUGH3D_TEST_HOOK
#include <pcl/recognition/cg/hough_3d.h>

namespace
{

using pcl::test::hough_3d_rvv::ExecutionPath;
using pcl::test::hough_3d_rvv::VoteGenerationInput;

volatile std::uint64_t g_sink = 0;

class Hough3DGroupingBenchHarness
    : public pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>
{
public:
  using Base = pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>;
  using Base::houghVoting;
};

std::vector<VoteGenerationInput>
makeInputs(const std::size_t count, const float distance_base)
{
  std::vector<VoteGenerationInput> inputs;
  inputs.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    const float s = static_cast<float>(i + 1);
    inputs.push_back({
        {s, s + 1.0f, s + 2.0f},
        {0.5f * s, -0.25f * s, 0.125f * s},
        {1.0f, 0.1f, 0.2f},
        {0.2f, 1.0f, 0.3f},
        {0.1f, 0.3f, 1.0f},
        distance_base + static_cast<float>(i) * 0.01f,
    });
  }
  return inputs;
}

pcl::ReferenceFrame
makeReferenceFrame()
{
  pcl::ReferenceFrame frame;
  frame.x_axis[0] = 1.0f;
  frame.x_axis[1] = 0.0f;
  frame.x_axis[2] = 0.0f;
  frame.y_axis[0] = 0.0f;
  frame.y_axis[1] = 1.0f;
  frame.y_axis[2] = 0.0f;
  frame.z_axis[0] = 0.0f;
  frame.z_axis[1] = 0.0f;
  frame.z_axis[2] = 1.0f;
  return frame;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeModelCloud()
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
  cloud->push_back(pcl::PointXYZ(0.0f, 0.0f, 0.0f));
  cloud->push_back(pcl::PointXYZ(1.0f, 0.0f, 0.0f));
  cloud->push_back(pcl::PointXYZ(0.0f, 1.0f, 0.0f));
  cloud->push_back(pcl::PointXYZ(0.0f, 0.0f, 1.0f));
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeSceneCloud(const std::size_t count)
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
  cloud->reserve(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    const float x = static_cast<float>(i % 256);
    const float y = static_cast<float>((i / 256) % 256);
    const float z = static_cast<float>(i % 17) * 0.25f;
    cloud->push_back(pcl::PointXYZ(x, y, z));
  }
  return cloud;
}

pcl::PointCloud<pcl::ReferenceFrame>::Ptr
makeReferenceFrames(const std::size_t count)
{
  pcl::PointCloud<pcl::ReferenceFrame>::Ptr frames(new pcl::PointCloud<pcl::ReferenceFrame>);
  frames->resize(count);
  for (std::size_t i = 0; i < count; ++i)
    (*frames)[i] = makeReferenceFrame();
  return frames;
}

pcl::CorrespondencesPtr
makeCorrespondences(const std::size_t count, const std::size_t model_size, const std::size_t scene_size)
{
  pcl::CorrespondencesPtr correspondences(new pcl::Correspondences);
  correspondences->reserve(count);
  for (std::size_t i = 0; i < count; ++i)
  {
    const int query = static_cast<int>(i % model_size);
    const int match = static_cast<int>(i % scene_size);
    correspondences->emplace_back(query, match, 0.1f + static_cast<float>(i % 13) * 0.01f);
  }
  return correspondences;
}

Hough3DGroupingBenchHarness
makeProductionHarness(
    const std::size_t vote_count,
    const float hough_bin_size,
    const bool use_interpolation,
    const bool use_distance_weight)
{
  const auto model = makeModelCloud();
  const auto scene = makeSceneCloud(vote_count);
  const auto model_rf = makeReferenceFrames(model->size());
  const auto scene_rf = makeReferenceFrames(scene->size());
  const auto correspondences = makeCorrespondences(vote_count, model->size(), scene->size());

  Hough3DGroupingBenchHarness grouping;
  grouping.setInputCloud(model);
  grouping.setInputRf(model_rf);
  grouping.setSceneCloud(scene);
  grouping.setSceneRf(scene_rf);
  grouping.setModelSceneCorrespondences(correspondences);
  grouping.setHoughBinSize(hough_bin_size);
  grouping.setUseInterpolation(use_interpolation);
  grouping.setUseDistanceWeight(use_distance_weight);
  return grouping;
}

bool
parseBoolArg(const char* text, const bool default_value)
{
  if (!text)
    return default_value;
  const std::string value(text);
  if (value == "1" || value == "true" || value == "TRUE" || value == "on")
    return true;
  if (value == "0" || value == "false" || value == "FALSE" || value == "off")
    return false;
  return default_value;
}

template <typename Fn>
double
timeLoop(const int iterations, const Fn& fn)
{
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i)
    fn();
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::micro>(end - start).count() / iterations;
}

} // namespace

int
main (int argc, char** argv)
{
  if (argc < 6)
  {
    std::cerr << "Usage: " << argv[0] << " <vote_count> <iterations> <warmup> <distance_base> <threshold> <repeat>\n";
    return 1;
  }

  const std::size_t vote_count = static_cast<std::size_t>(std::stoul (argv[1]));
  const int iterations = std::stoi (argv[2]);
  const int warmup = std::stoi (argv[3]);
  const float distance_base = std::stof (argv[4]);
  const float threshold = std::stof (argv[5]);
  const int repeat = argc > 6 ? std::stoi (argv[6]) : 1;
  const bool use_interpolation = argc > 7 ? parseBoolArg (argv[7], true) : true;
  const bool use_distance_weight = argc > 8 ? parseBoolArg (argv[8], true) : true;

  std::vector<VoteGenerationInput> inputs = makeInputs (vote_count, distance_base);
  Hough3DGroupingBenchHarness production_harness =
      makeProductionHarness (vote_count, 1.0f, use_interpolation, use_distance_weight);

  for (int i = 0; i < warmup; ++i)
  {
    ExecutionPath path = ExecutionPath::ScalarFallback;
    (void)pcl::test::hough_3d_rvv::computeSceneVotesCandidate (inputs, true, true, path);
    (void)production_harness.houghVoting();
  }

  const auto scalar_us = timeLoop(iterations, [&] {
    const auto out = pcl::test::hough_3d_rvv::computeSceneVotesScalarReference (inputs, true, true);
    g_sink ^= pcl::test::hough_3d_rvv::checksumSceneVotes(out.scene_votes);
  });
  const auto candidate_us = timeLoop(iterations, [&] {
    ExecutionPath path = ExecutionPath::ScalarFallback;
    const auto out = pcl::test::hough_3d_rvv::computeSceneVotesCandidate (inputs, true, true, path);
    g_sink ^= pcl::test::hough_3d_rvv::checksumSceneVotes(out.scene_votes);
    g_sink ^= static_cast<std::uint64_t>(path == ExecutionPath::RvvVoteGeneration ? 1u : 0u);
  });
  const auto production_us = timeLoop(iterations, [&] {
    g_sink ^= static_cast<std::uint64_t>(production_harness.houghVoting());
#if defined(__RVV10__)
    g_sink ^= pcl::detail::hough3DVoteGenerationRVVPathHits;
#endif
  });

  std::cout << "Dataset: synthetic Hough3D production direct\n";
  std::cout << "Iterations: " << iterations << "\n";
  std::cout << "Warmup Iterations: " << warmup << "\n";
  std::cout << "Vote Count: " << vote_count << "\n";
  std::cout << "Threshold: " << threshold << "\n";
  std::cout << "Use Interpolation: " << (use_interpolation ? 1 : 0) << "\n";
  std::cout << "Use Distance Weight: " << (use_distance_weight ? 1 : 0) << "\n";
  std::cout << std::fixed << std::setprecision(3);
  std::cout << "Hough3D vote generation scalar reference: " << scalar_us << " us / iter\n";
  std::cout << "Hough3D vote generation candidate: " << candidate_us << " us / iter\n";
  std::cout << "Hough3D production houghVoting: " << production_us << " us / iter\n";
  std::cout << "checksum: " << pcl::test::hough_3d_rvv::checksumSceneVotes(
                                pcl::test::hough_3d_rvv::computeSceneVotesScalarReference(
                                    inputs, true, true)
                                    .scene_votes)
            << "\n";
  std::cout << "Repeat: " << repeat << "\n";
  std::cout << "sink: " << g_sink << "\n";
  return 0;
}
