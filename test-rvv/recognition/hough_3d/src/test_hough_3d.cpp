#include <pcl/test/gtest.h>

#include <cmath>
#include <vector>

#include "hough_3d.h"

#define PCL_RVV_HOUGH3D_TEST_HOOK
#include <pcl/recognition/cg/hough_3d.h>

namespace
{

using pcl::test::hough_3d_rvv::ExecutionPath;
using pcl::test::hough_3d_rvv::VoteGenerationInput;

class Hough3DGroupingTestHarness
    : public pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>
{
public:
  using Base = pcl::Hough3DGrouping<pcl::PointXYZ, pcl::PointXYZ, pcl::ReferenceFrame, pcl::ReferenceFrame>;
  using Base::houghVoting;
};

std::vector<VoteGenerationInput>
makeInputs()
{
  return {
      {{1.0f, 2.0f, 3.0f},
       {0.5f, -1.0f, 2.0f},
       {1.0f, 0.0f, 0.0f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       0.2f},
      {{-2.0f, 1.0f, 4.0f},
       {1.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 0.0f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       0.7f},
  };
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
    const float x = static_cast<float>(i % 64);
    const float y = static_cast<float>((i / 64) % 64);
    const float z = static_cast<float>(i % 11) * 0.25f;
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

} // namespace

TEST (PCL, Hough3DVoteGenerationCandidateMatchesScalarReference)
{
  const auto inputs = makeInputs();
  const auto scalar = pcl::test::hough_3d_rvv::computeSceneVotesScalarReference (inputs, true, true);
  ExecutionPath execution_path = ExecutionPath::ScalarFallback;
  const auto candidate = pcl::test::hough_3d_rvv::computeSceneVotesCandidate (inputs, true, true, execution_path);

  ASSERT_EQ (scalar.scene_votes.size (), candidate.scene_votes.size ());
  for (std::size_t i = 0; i < scalar.scene_votes.size (); ++i)
  {
    for (int axis = 0; axis < 3; ++axis)
      EXPECT_FLOAT_EQ (scalar.scene_votes[i][axis], candidate.scene_votes[i][axis]);
  }
  for (int axis = 0; axis < 3; ++axis)
  {
    EXPECT_FLOAT_EQ (scalar.d_min[axis], candidate.d_min[axis]);
    EXPECT_FLOAT_EQ (scalar.d_max[axis], candidate.d_max[axis]);
  }
  EXPECT_FLOAT_EQ (scalar.max_distance, candidate.max_distance);

#if defined(__RVV10__)
  EXPECT_EQ (execution_path, ExecutionPath::RvvVoteGeneration);
#else
  EXPECT_EQ (execution_path, ExecutionPath::ScalarFallback);
#endif

  EXPECT_NE (pcl::test::hough_3d_rvv::checksumSceneVotes (scalar.scene_votes), 0ull);
}

TEST (PCL, Hough3DProductionVotingPathRuns)
{
  const auto model = makeModelCloud();
  const auto scene = makeSceneCloud(128);
  const auto model_rf = makeReferenceFrames(model->size());
  const auto scene_rf = makeReferenceFrames(scene->size());
  const auto correspondences = makeCorrespondences(512, model->size(), scene->size());

#if defined(__RVV10__)
  pcl::detail::hough3DVoteGenerationRVVPathHits = 0;
#endif

  Hough3DGroupingTestHarness grouping;
  grouping.setInputCloud(model);
  grouping.setInputRf(model_rf);
  grouping.setSceneCloud(scene);
  grouping.setSceneRf(scene_rf);
  grouping.setModelSceneCorrespondences(correspondences);
  grouping.setHoughBinSize(1.0);
  grouping.setUseInterpolation(true);
  grouping.setUseDistanceWeight(true);

  EXPECT_TRUE(grouping.houghVoting());

#if defined(__RVV10__)
  EXPECT_GT(pcl::detail::hough3DVoteGenerationRVVPathHits, 0ull);
#endif
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
