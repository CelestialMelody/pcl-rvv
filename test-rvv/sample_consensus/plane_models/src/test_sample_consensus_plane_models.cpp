/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2012, Willow Garage, Inc.
 *  Copyright (c) 2014-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <chrono>
#include <iostream>
#include <iomanip> // For std::setw, std::setprecision
#include <type_traits>

#include <pcl/test/gtest.h>

#include <pcl/pcl_tests.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/register_point_struct.h>
#include <pcl/rvv_point_traits.h>
#include <pcl/common/utils.h>

#include <pcl/sample_consensus/msac.h>
#include <pcl/sample_consensus/lmeds.h>
#include <pcl/sample_consensus/rmsac.h>
#include <pcl/sample_consensus/mlesac.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/rransac.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/sample_consensus/sac_model_normal_plane.h>
#include <pcl/sample_consensus/sac_model_normal_parallel_plane.h>

using namespace pcl;
using namespace pcl::io;

using SampleConsensusModelPlanePtr = SampleConsensusModelPlane<PointXYZ>::Ptr;
using SampleConsensusModelNormalPlanePtr = SampleConsensusModelNormalPlane<PointXYZ, Normal>::Ptr;
using SampleConsensusModelNormalParallelPlanePtr = SampleConsensusModelNormalParallelPlane<PointXYZ, Normal>::Ptr;

struct NormalWithDoubleCurvature
{
  float normal_x = 0.0f;
  float normal_y = 0.0f;
  float normal_z = 1.0f;
  double curvature = 0.0;
};

POINT_CLOUD_REGISTER_POINT_STRUCT (NormalWithDoubleCurvature,
    (float, normal_x, normal_x)
    (float, normal_y, normal_y)
    (float, normal_z, normal_z)
    (double, curvature, curvature)
)

struct NonAoSRegisteredNormal
{
  float normal_x = 0.0f;
  float normal_y = 0.0f;
  float normal_z = 1.0f;
  float curvature = 0.0f;

private:
  int non_standard_layout_marker_ = 0;
};

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
POINT_CLOUD_REGISTER_POINT_STRUCT (NonAoSRegisteredNormal,
    (float, normal_x, normal_x)
    (float, normal_y, normal_y)
    (float, normal_z, normal_z)
    (float, curvature, curvature)
)
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

struct NonAoSRegisteredXYZ
{
  PCL_ADD_POINT4D;

private:
  int non_standard_layout_marker_ = 0;

public:
  PCL_MAKE_ALIGNED_OPERATOR_NEW
};

#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
POINT_CLOUD_REGISTER_POINT_STRUCT (NonAoSRegisteredXYZ,
    (float, x, x)
    (float, y, y)
    (float, z, z)
)
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

static_assert(pcl::rvv::RVVXYZFloatLayout<NonAoSRegisteredXYZ>::value);
static_assert(!std::is_standard_layout_v<NonAoSRegisteredXYZ>);
static_assert(!pcl::rvv::RVVXYZAoSFloatLayout<NonAoSRegisteredXYZ>::value);
static_assert(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZI>::value);
static_assert(pcl::rvv::RVVXYZAoSFloatLayout<pcl::PointXYZINormal>::value);
static_assert(pcl::rvv::RVVNormalFloatLayout<NonAoSRegisteredNormal>::value);
static_assert(pcl::rvv::RVVFloatFieldLayout<NonAoSRegisteredNormal, pcl::fields::curvature>::value);
static_assert(!std::is_standard_layout_v<NonAoSRegisteredNormal>);
#if defined (__RVV10__)
static_assert(pcl::detail::NormalPlaneRVVNormalAoSLayout<pcl::PointNormal>::value);
static_assert(pcl::detail::NormalPlaneRVVNormalAoSLayout<pcl::PointXYZINormal>::value);
static_assert(!pcl::detail::NormalPlaneRVVNormalAoSLayout<NonAoSRegisteredNormal>::value);
#endif

PointCloud<PointXYZ>::Ptr cloud_ (new PointCloud<PointXYZ> ());
PointCloud<Normal>::Ptr normals_ (new PointCloud<Normal> ());
pcl::Indices indices_;
float plane_coeffs_[] = {-0.8964f, -0.5868f, -1.208f};

template <typename ModelType, typename SacType>
void verifyPlaneSac (ModelType& model,
                     SacType& sac,
                     unsigned int inlier_number = 2000,
                     float tol = 1e-1f,
                     float refined_tol = 1e-1f,
                     float proj_tol = 1e-3f)
{
  // Algorithm tests
  bool result = sac.computeModel ();
  ASSERT_TRUE (result);

  pcl::Indices sample;
  sac.getModel (sample);
  EXPECT_EQ (3, sample.size ());

  pcl::Indices inliers;
  sac.getInliers (inliers);
  EXPECT_LT (inlier_number, inliers.size ());

  Eigen::VectorXf coeff;
  sac.getModelCoefficients (coeff);
  EXPECT_EQ (4, coeff.size ());
  EXPECT_NEAR (plane_coeffs_[0], coeff[0] / coeff[3], tol);
  EXPECT_NEAR (plane_coeffs_[1], coeff[1] / coeff[3], tol);
  EXPECT_NEAR (plane_coeffs_[2], coeff[2] / coeff[3], tol);

  Eigen::VectorXf coeff_refined;
  model->optimizeModelCoefficients (inliers, coeff, coeff_refined);
  EXPECT_EQ (4, coeff_refined.size ());
  EXPECT_NEAR (plane_coeffs_[0], coeff_refined[0] / coeff_refined[3], refined_tol);
  EXPECT_NEAR (plane_coeffs_[1], coeff_refined[1] / coeff_refined[3], refined_tol);

  // This test fails in Windows (VS 2010) -- not sure why yet -- relaxing the constraint from 1e-2 to 1e-1
  // This test fails in MacOS too -- not sure why yet -- disabling
  //EXPECT_NEAR (coeff_refined[2] / coeff_refined[3], plane_coeffs_[2], refined_tol);

  // Projection tests
  PointCloud<PointXYZ> proj_points;
  model->projectPoints (inliers, coeff_refined, proj_points);
  EXPECT_XYZ_NEAR (PointXYZ (1.1266,  0.0152, -0.0156), proj_points[20], proj_tol);
  EXPECT_XYZ_NEAR (PointXYZ (1.1843, -0.0635, -0.0201), proj_points[30], proj_tol);
  EXPECT_XYZ_NEAR (PointXYZ (1.0749, -0.0586,  0.0587), proj_points[50], proj_tol);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelPlane, Base)
{
  // Create a shared plane model pointer directly
  SampleConsensusModelPlanePtr model (new SampleConsensusModelPlane<PointXYZ> (cloud_));

  // Basic tests
  PointCloud<PointXYZ>::ConstPtr cloud = model->getInputCloud ();
  ASSERT_EQ (cloud_->size (), cloud->size ());

  model->setInputCloud (cloud);
  cloud = model->getInputCloud ();
  ASSERT_EQ (cloud_->size (), cloud->size ());

  auto indices = model->getIndices ();
  ASSERT_EQ (indices_.size (), indices->size ());
  model->setIndices (indices_);
  indices = model->getIndices ();
  ASSERT_EQ (indices_.size (), indices->size ());
  model->setIndices (indices);
  indices = model->getIndices ();
  ASSERT_EQ (indices_.size (), indices->size ());
}

TEST (SampleConsensusModelPlane, SampleValidationPointsCollinear)
{
  PointCloud<PointXYZ> cloud;
  cloud.resize (4);

  // The "cheat point" makes it possible to find a set of valid samples and
  // therefore avoids the log message of an unsuccessful sample validation
  // being printed a 1000 times without any chance of success.
  // The order is chosen such that with a known, fixed rng-state/-seed all
  // validation steps are actually exercised.
  constexpr pcl::index_t firstCollinearPointIndex = 0;
  constexpr pcl::index_t secondCollinearPointIndex = 1;
  constexpr pcl::index_t thirdCollinearPointIndex = 2;
  constexpr pcl::index_t cheatPointIndex = 3;

  cloud[firstCollinearPointIndex].getVector3fMap () <<  0.1f,  0.1f,  0.1f;
  cloud[secondCollinearPointIndex].getVector3fMap () <<  0.2f,  0.2f,  0.2f;
  cloud[thirdCollinearPointIndex].getVector3fMap () <<  0.3f,  0.3f,  0.3f;
  cloud[cheatPointIndex].getVector3fMap () <<  0.0f,  0.1f,  0.0f; // <-- cheat point

  // Create a shared line model pointer directly and explicitly disable the
  // random seed for the reasons mentioned above
  SampleConsensusModelPlanePtr model (
    new SampleConsensusModelPlane<PointXYZ> (cloud.makeShared (), /* random = */ false));

  // Algorithm tests
  pcl::Indices samples;
  int iterations = 0;
  model->getSamples(iterations, samples);
  EXPECT_EQ (samples.size(), 3);
  // The "cheat point" has to be part of the sample, otherwise something is wrong.
  // The best option would be to assert on stderr output here, but that doesn't
  // seem to be that simple.
  EXPECT_TRUE (std::find(samples.begin (), samples.end (), cheatPointIndex) != samples.end ());

  pcl::Indices forcedSamples = {firstCollinearPointIndex, secondCollinearPointIndex, thirdCollinearPointIndex};
  Eigen::VectorXf modelCoefficients;
  EXPECT_FALSE (model->computeModelCoefficients (forcedSamples, modelCoefficients));
}

TEST (SampleConsensusModelPlane, SampleValidationPointsValid)
{
  PointCloud<PointXYZ> cloud;
  cloud.resize (3);

  cloud[0].getVector3fMap () <<  0.1f,  0.0f,  0.0f;
  cloud[1].getVector3fMap () <<  0.0f,  0.1f,  0.0f;
  cloud[2].getVector3fMap () <<  0.0f,  0.0f,  0.1f;

  // Create a shared line model pointer directly
  SampleConsensusModelPlanePtr model (
    new SampleConsensusModelPlane<PointXYZ> (cloud.makeShared ()));

  // Algorithm tests
  pcl::Indices samples;
  int iterations = 0;
  model->getSamples(iterations, samples);
  EXPECT_EQ (samples.size(), 3);

  Eigen::VectorXf modelCoefficients;
  EXPECT_TRUE (model->computeModelCoefficients (samples, modelCoefficients));
}

TEST (SampleConsensusModelPlane, SampleValidationNotEnoughSamples)
{
  PointCloud<PointXYZ> cloud;
  cloud.resize (2);

  cloud[0].getVector3fMap () <<  0.1f,  0.0f,  0.0f;
  cloud[1].getVector3fMap () <<  0.0f,  0.1f,  0.0f;

  std::vector<pcl::Indices> testIndices = {{}, {0,}, {0, 1}};

  for( const auto& indices : testIndices) {
    PointCloud<PointXYZ> subCloud {cloud, indices};

    // Create a shared line model pointer directly
    SampleConsensusModelPlanePtr model (
      new SampleConsensusModelPlane<PointXYZ> (subCloud.makeShared ()));

    // Algorithm tests
    pcl::Indices samples;
    int iterations = 0;
    model->getSamples(iterations, samples);
    EXPECT_EQ (samples.size(), 0);

    Eigen::VectorXf modelCoefficients;
    EXPECT_FALSE (model->computeModelCoefficients (indices, modelCoefficients));
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelPlane, RANSAC)
{
  srand (0);

  // Create a shared plane model pointer directly
  SampleConsensusModelPlanePtr model (new SampleConsensusModelPlane<PointXYZ> (cloud_));

  // Create the RANSAC object
  RandomSampleConsensus<PointXYZ> sac (model, 0.03);

  verifyPlaneSac (model, sac);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelPlane, LMedS)
{
  srand (0);

  // Create a shared plane model pointer directly
  SampleConsensusModelPlanePtr model (new SampleConsensusModelPlane<PointXYZ> (cloud_));

  // Create the LMedS object
  LeastMedianSquares<PointXYZ> sac (model, 0.03);

  verifyPlaneSac (model, sac);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelPlane, MSAC)
{
  srand (0);

  // Create a shared plane model pointer directly
  SampleConsensusModelPlanePtr model (new SampleConsensusModelPlane<PointXYZ> (cloud_));

  // Create the MSAC object
  MEstimatorSampleConsensus<PointXYZ> sac (model, 0.03);

  verifyPlaneSac (model, sac);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelPlane, RRANSAC)
{
  srand (0);

  // Create a shared plane model pointer directly
  SampleConsensusModelPlanePtr model (new SampleConsensusModelPlane<PointXYZ> (cloud_));

  // Create the RRANSAC object
  RandomizedRandomSampleConsensus<PointXYZ> sac (model, 0.03);

  sac.setFractionNrPretest (0.1);
  ASSERT_EQ (0.1, sac.getFractionNrPretest ());

  verifyPlaneSac (model, sac, 600, 1.0f, 1.0f, 0.01f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelPlane, MLESAC)
{
  srand (0);

  // Create a shared plane model pointer directly
  SampleConsensusModelPlanePtr model (new SampleConsensusModelPlane<PointXYZ> (cloud_));

  // Create the MSAC object
  MaximumLikelihoodSampleConsensus<PointXYZ> sac (model, 0.03);

  verifyPlaneSac (model, sac, 1000, 0.3f, 0.2f, 0.01f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelPlane, RMSAC)
{
  srand (0);

  // Create a shared plane model pointer directly
  SampleConsensusModelPlanePtr model (new SampleConsensusModelPlane<PointXYZ> (cloud_));

  // Create the RMSAC object
  RandomizedMEstimatorSampleConsensus<PointXYZ> sac (model, 0.03);

  sac.setFractionNrPretest (10.0);
  ASSERT_EQ (10.0, sac.getFractionNrPretest ());

  verifyPlaneSac (model, sac, 600, 1.0f, 1.0f, 0.01f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelNormalPlane, RANSAC)
{
  srand (0);

  // Create a shared plane model pointer directly
  SampleConsensusModelNormalPlanePtr model (new SampleConsensusModelNormalPlane<PointXYZ, Normal> (cloud_));
  model->setInputNormals (normals_);
  model->setNormalDistanceWeight (0.01);

  // Create the RANSAC object
  RandomSampleConsensus<PointXYZ> sac (model, 0.03);

  verifyPlaneSac (model, sac);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST (SampleConsensusModelNormalParallelPlane, RANSAC)
{
  srand (0);

  // Use a custom point cloud for these tests until we need something better
  PointCloud<PointXYZ> cloud;
  PointCloud<Normal> normals;
  cloud.resize (10);
  normals.resize (10);

  for (std::size_t idx = 0; idx < cloud.size (); ++idx)
  {
    cloud[idx].x = static_cast<float> ((rand () % 200) - 100);
    cloud[idx].y = static_cast<float> ((rand () % 200) - 100);
    cloud[idx].z = 0.0f;

    normals[idx].normal_x = 0.0f;
    normals[idx].normal_y = 0.0f;
    normals[idx].normal_z = 1.0f;
  }

  // Create a shared plane model pointer directly
  SampleConsensusModelNormalParallelPlanePtr model (new SampleConsensusModelNormalParallelPlane<PointXYZ, Normal> (cloud.makeShared ()));
  model->setInputNormals (normals.makeShared ());

  constexpr float max_angle_rad = 0.01f;
  constexpr float angle_eps = 0.001f;
  model->setEpsAngle (max_angle_rad);

  // Test true axis
  {
    model->setAxis (Eigen::Vector3f (0, 0, 1));

    RandomSampleConsensus<PointXYZ> sac (model, 0.03);
    sac.computeModel();

    pcl::Indices inliers;
    sac.getInliers (inliers);
    ASSERT_EQ (cloud.size (), inliers.size ());
  }

  // test axis slightly in valid range
  {
    model->setAxis (Eigen::Vector3f (0, std::sin (max_angle_rad * (1 - angle_eps)), std::cos (max_angle_rad * (1 - angle_eps))));
    RandomSampleConsensus<PointXYZ> sac (model, 0.03);
    sac.computeModel ();

    pcl::Indices inliers;
    sac.getInliers (inliers);
    ASSERT_EQ (cloud.size (), inliers.size ());
  }

  // test axis slightly out of valid range
  {
    model->setAxis (Eigen::Vector3f (0, std::sin (max_angle_rad * (1 + angle_eps)), std::cos (max_angle_rad * (1 + angle_eps))));
    RandomSampleConsensus<PointXYZ> sac (model, 0.03);
    sac.computeModel ();

    pcl::Indices inliers;
    sac.getInliers (inliers);
    ASSERT_EQ (0, inliers.size ());
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT>
class SampleConsensusModelPlaneTest : private SampleConsensusModelPlane<PointT>
{
  public:
    using SampleConsensusModelPlane<PointT>::SampleConsensusModelPlane;
    using SampleConsensusModelPlane<PointT>::countWithinDistanceStandard;
#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
    using SampleConsensusModelPlane<PointT>::countWithinDistanceSSE;
#endif
#if defined (__AVX__) && defined (__AVX2__)
    using SampleConsensusModelPlane<PointT>::countWithinDistanceAVX;
#endif
#if defined (__RVV10__)
    using SampleConsensusModelPlane<PointT>::countWithinDistanceRVV;
#endif
};

TEST (SampleConsensusModelPlane, SIMD_countWithinDistance_plane_smoke) // Test if all countWithinDistance implementations return the same value
{
  const auto seed = static_cast<unsigned> (std::time (nullptr));
  srand (seed);
  for (size_t i=0; i<100; i++) // Run as often as you like
  {
    // Generate a cloud with 1000 random points
    PointCloud<PointXYZ> cloud;
    pcl::Indices indices;
    cloud.resize (1000);
    for (std::size_t idx = 0; idx < cloud.size (); ++idx)
    {
      cloud[idx].x = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      cloud[idx].y = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      cloud[idx].z = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      if (rand () % 2 == 0)
      {
        indices.push_back (static_cast<int> (idx));
      }
    }
    SampleConsensusModelPlaneTest<PointXYZ> model (cloud.makeShared (), indices, true);

    // Generate random model parameters
    Eigen::VectorXf model_coefficients(4);
    model_coefficients << 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0,
                          2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0,
                          2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0, 0.0;
    model_coefficients.normalize ();
    model_coefficients(3) = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0; // Last parameter

    const double threshold = 0.1 * static_cast<double> (rand ()) / RAND_MAX; // threshold in [0; 0.1]

    // The number of inliers is usually somewhere between 0 and 100
    const auto res_standard = model.countWithinDistanceStandard (model_coefficients, threshold); // Standard
    PCL_DEBUG ("seed=%lu, i=%lu, model=(%f, %f, %f, %f), threshold=%f, res_standard=%lu\n", seed, i,
               model_coefficients(0), model_coefficients(1), model_coefficients(2), model_coefficients(3), threshold, res_standard);
#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
    const auto res_sse      = model.countWithinDistanceSSE (model_coefficients, threshold); // SSE
    ASSERT_EQ (res_standard, res_sse);
#endif
#if defined (__AVX__) && defined (__AVX2__)
    const auto res_avx      = model.countWithinDistanceAVX (model_coefficients, threshold); // AVX
    ASSERT_EQ (res_standard, res_avx);
#endif
#if defined (__RVV10__)
    const auto res_rvv      = model.countWithinDistanceRVV (model_coefficients, threshold); // RVV
    ASSERT_EQ (res_standard, res_rvv);
#endif
  }
}

// ---------------------------------------------------------------------------
// Test for SampleConsensusModelPlane (Pure Geometric Distance)
// ---------------------------------------------------------------------------
TEST (SampleConsensusModelPlane, SIMD_countWithinDistance)
{
  const auto seed = static_cast<unsigned> (std::time (nullptr));
  srand (seed);

  // === Timer Variables ===
  double total_time_standard = 0.0;
  double total_time_sse = 0.0;
  double total_time_avx = 0.0;
  double total_time_rvv = 0.0;

  // Number of iterations for benchmarking
  const size_t iterations = 1000;

  for (size_t i = 0; i < iterations; i++)
  {
    // Generate a cloud with 1000 random points
    PointCloud<PointXYZ> cloud;
    pcl::Indices indices;
    cloud.resize (1000);
    for (std::size_t idx = 0; idx < cloud.size (); ++idx)
    {
      cloud[idx].x = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f;
      cloud[idx].y = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f;
      cloud[idx].z = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f;
      if (rand () % 2 == 0)
      {
        indices.push_back (static_cast<int> (idx));
      }
    }
    SampleConsensusModelPlaneTest<PointXYZ> model (cloud.makeShared (), indices, true);

    // Generate random model parameters
    Eigen::VectorXf model_coefficients(4);
    model_coefficients << 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f,
                          2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f,
                          2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f, 0.0f;
    model_coefficients.normalize ();
    model_coefficients(3) = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f; // Last parameter d

    const double threshold = 0.1 * static_cast<double> (rand ()) / RAND_MAX; // threshold in [0; 0.1]

    // === 1. Measure Standard Implementation ===
    auto start_std = std::chrono::high_resolution_clock::now();
    const auto res_standard = model.countWithinDistanceStandard (model_coefficients, threshold);
    auto end_std = std::chrono::high_resolution_clock::now();
    total_time_standard += std::chrono::duration<double, std::milli>(end_std - start_std).count();

    // === 2. Measure SSE Implementation ===
#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
    auto start_sse = std::chrono::high_resolution_clock::now();
    const auto res_sse = model.countWithinDistanceSSE (model_coefficients, threshold);
    auto end_sse = std::chrono::high_resolution_clock::now();
    total_time_sse += std::chrono::duration<double, std::milli>(end_sse - start_sse).count();

    ASSERT_EQ (res_standard, res_sse);
#endif

    // === 3. Measure AVX Implementation ===
#if defined (__AVX__) && defined (__AVX2__)
    auto start_avx = std::chrono::high_resolution_clock::now();
    const auto res_avx = model.countWithinDistanceAVX (model_coefficients, threshold);
    auto end_avx = std::chrono::high_resolution_clock::now();
    total_time_avx += std::chrono::duration<double, std::milli>(end_avx - start_avx).count();

    ASSERT_EQ (res_standard, res_avx);
#endif

    // === 4. Measure RVV Implementation ===
#if defined (__RVV10__)
    auto start_rvv = std::chrono::high_resolution_clock::now();
    const auto res_rvv = model.countWithinDistanceRVV (model_coefficients, threshold);
    auto end_rvv = std::chrono::high_resolution_clock::now();
    total_time_rvv += std::chrono::duration<double, std::milli>(end_rvv - start_rvv).count();

    ASSERT_EQ (res_standard, res_rvv);
#endif
  }

  // === Performance Report ===
  std::cout << "\n========================================================" << std::endl;
  std::cout << "[ Performance Report: Plane (Geometric Distance Only) ]" << std::endl;
  std::cout << "Iterations          : " << iterations << std::endl;
  std::cout << std::fixed << std::setprecision(4);
  std::cout << "Standard Total Time : " << total_time_standard << " ms" << std::endl;

#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
  std::cout << "SSE Total Time      : " << total_time_sse << " ms" << std::endl;
  std::cout << "Speedup (Std/SSE)   : " << total_time_standard / total_time_sse << "x" << std::endl;
#endif

#if defined (__AVX__) && defined (__AVX2__)
  std::cout << "AVX Total Time      : " << total_time_avx << " ms" << std::endl;
  std::cout << "Speedup (Std/AVX)   : " << total_time_standard / total_time_avx << "x" << std::endl;
#endif

#if defined (__RVV10__)
  std::cout << "RVV Total Time      : " << total_time_rvv << " ms" << std::endl;
  std::cout << "Speedup (Std/RVV)   : " << total_time_standard / total_time_rvv << "x" << std::endl;
#endif
  std::cout << "========================================================" << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointT, typename PointNT>
class SampleConsensusModelNormalPlaneTest : private SampleConsensusModelNormalPlane<PointT, PointNT>
{
  public:
    using SampleConsensusModelNormalPlane<PointT, PointNT>::SampleConsensusModelNormalPlane;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::setNormalDistanceWeight;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::setInputNormals;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::selectWithinDistance;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::countWithinDistance;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::getDistancesToModel;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::countWithinDistanceStandard;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::selectWithinDistanceStandard;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::getDistancesToModelStandard;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::setIndices;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::error_sqr_dists_;
#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
    using SampleConsensusModelNormalPlane<PointT, PointNT>::countWithinDistanceSSE;
#endif
#if defined (__AVX__) && defined (__AVX2__)
    using SampleConsensusModelNormalPlane<PointT, PointNT>::countWithinDistanceAVX;
#endif
#if defined (__RVV10__)
    using SampleConsensusModelNormalPlane<PointT, PointNT>::countWithinDistanceRVV;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::selectWithinDistanceRVV;
    using SampleConsensusModelNormalPlane<PointT, PointNT>::getDistancesToModelRVV;
#endif
};

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForSupportedLayout)
{
  // 公开入口（public entry）必须保持和直接 RVV helper 一致；这里把法线权重设为 0，
  // 让测试只验证 dispatch / 写回形状，不把 acos 近似误差混入这个验收条件。
  constexpr std::size_t nr_points = 6;
  PointCloud<PointXYZ>::Ptr cloud (new PointCloud<PointXYZ>);
  PointCloud<Normal>::Ptr normals (new PointCloud<Normal>);
  cloud->resize (nr_points);
  normals->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    (*cloud)[idx].x = static_cast<float> (idx);
    (*cloud)[idx].y = 0.0f;
    (*cloud)[idx].z = 0.015f * static_cast<float> (idx);
    (*normals)[idx].normal_x = 0.0f;
    (*normals)[idx].normal_y = 0.0f;
    (*normals)[idx].normal_z = 1.0f;
    (*normals)[idx].curvature = 0.0f;
  }

  pcl::Indices indices = {5, 3, 1, 0, 2, 4};
  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<PointXYZ, Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.0);

  pcl::Indices public_inliers;
  model.selectWithinDistance (model_coefficients, 0.05, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (model_coefficients, 0.05);
  std::vector<double> public_distances;
  model.getDistancesToModel (model_coefficients, public_distances);

#if defined (__RVV10__)
  pcl::Indices rvv_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t rvv_count =
      model.selectWithinDistanceRVV (model_coefficients, 0.05, rvv_inliers);
  rvv_inliers.resize (rvv_count);
  model.error_sqr_dists_.resize (rvv_count);
  const std::vector<double> rvv_errors = model.error_sqr_dists_;

  std::vector<double> rvv_distances;
  model.getDistancesToModelRVV (model_coefficients, rvv_distances);
  ASSERT_EQ (rvv_count, model.countWithinDistanceRVV (model_coefficients, 0.05));
#else
  pcl::Indices rvv_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t rvv_count =
      model.selectWithinDistanceStandard (model_coefficients, 0.05, rvv_inliers);
  rvv_inliers.resize (rvv_count);
  model.error_sqr_dists_.resize (rvv_count);
  const std::vector<double> rvv_errors = model.error_sqr_dists_;

  std::vector<double> rvv_distances;
  model.getDistancesToModelStandard (model_coefficients, rvv_distances);
  ASSERT_EQ (rvv_count, model.countWithinDistanceStandard (model_coefficients, 0.05));
#endif

  ASSERT_EQ (rvv_count, public_count);
  ASSERT_EQ (rvv_inliers, public_inliers);
  ASSERT_EQ (rvv_errors.size (), public_errors.size ());
  for (std::size_t idx = 0; idx < public_errors.size (); ++idx)
    EXPECT_NEAR (rvv_errors[idx], public_errors[idx], 1e-6);

  ASSERT_EQ (rvv_distances.size (), public_distances.size ());
  for (std::size_t idx = 0; idx < public_distances.size (); ++idx)
    EXPECT_NEAR (rvv_distances[idx], public_distances[idx], 1e-6);
}

template <typename PointT>
void runNormalPlanePublicEntriesMatchDirectRVVForAoSSource()
{
  // 本 helper 验证 source 点类型扩展边界。normal-plane 只从 source 读取
  // x/y/z，额外字段例如 intensity 或 source normal 不参与输出语义；
  // 因此满足 RVVXYZAoSFloatLayout（AoS 字节偏移布局 gate）的代表点型
  // 应该和 PointXYZ 一样命中公开入口 RVV 分流。
  constexpr std::size_t nr_points = 7;
  typename PointCloud<PointT>::Ptr cloud (new PointCloud<PointT>);
  PointCloud<Normal>::Ptr normals (new PointCloud<Normal>);
  cloud->resize (nr_points);
  normals->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    (*cloud)[idx].x = static_cast<float> (idx);
    (*cloud)[idx].y = 0.125f * static_cast<float> (idx);
    (*cloud)[idx].z = 0.01f * static_cast<float> (idx);
    (*normals)[idx].normal_x = 0.0f;
    (*normals)[idx].normal_y = 0.0f;
    (*normals)[idx].normal_z = 1.0f;
    (*normals)[idx].curvature = 0.0f;
  }

  pcl::Indices indices = {6, 0, 3, 1, 5, 2, 4};
  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<PointT, Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.0);

  pcl::Indices public_inliers;
  model.selectWithinDistance (model_coefficients, 0.04, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (model_coefficients, 0.04);
  std::vector<double> public_distances;
  model.getDistancesToModel (model_coefficients, public_distances);

#if defined (__RVV10__)
  pcl::Indices direct_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t direct_count =
      model.selectWithinDistanceRVV (model_coefficients, 0.04, direct_inliers);
  direct_inliers.resize (direct_count);
  model.error_sqr_dists_.resize (direct_count);
  const std::vector<double> direct_errors = model.error_sqr_dists_;

  std::vector<double> direct_distances;
  model.getDistancesToModelRVV (model_coefficients, direct_distances);
  ASSERT_EQ (direct_count, model.countWithinDistanceRVV (model_coefficients, 0.04));
#else
  pcl::Indices direct_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t direct_count =
      model.selectWithinDistanceStandard (model_coefficients, 0.04, direct_inliers);
  direct_inliers.resize (direct_count);
  model.error_sqr_dists_.resize (direct_count);
  const std::vector<double> direct_errors = model.error_sqr_dists_;

  std::vector<double> direct_distances;
  model.getDistancesToModelStandard (model_coefficients, direct_distances);
  ASSERT_EQ (direct_count, model.countWithinDistanceStandard (model_coefficients, 0.04));
#endif

  ASSERT_EQ (direct_count, public_count);
  ASSERT_EQ (direct_inliers, public_inliers);
  ASSERT_EQ (direct_errors.size (), public_errors.size ());
  for (std::size_t idx = 0; idx < public_errors.size (); ++idx)
    EXPECT_NEAR (direct_errors[idx], public_errors[idx], 1e-6);

  ASSERT_EQ (direct_distances.size (), public_distances.size ());
  for (std::size_t idx = 0; idx < public_distances.size (); ++idx)
    EXPECT_NEAR (direct_distances[idx], public_distances[idx], 1e-6);
}

template <typename PointNT>
typename PointCloud<PointNT>::Ptr
makeNormalPlaneNormalCloud(const std::size_t nr_points)
{
  typename PointCloud<PointNT>::Ptr normals (new PointCloud<PointNT>);
  normals->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    auto& normal = (*normals)[idx];
    if constexpr (pcl::traits::has_xyz<PointNT>::value)
    {
      normal.x = 0.01f * static_cast<float> (idx);
      normal.y = -0.02f * static_cast<float> (idx);
      normal.z = 0.5f + 0.005f * static_cast<float> (idx);
    }
    if constexpr (std::is_same_v<PointNT, pcl::PointXYZINormal>)
      normal.intensity = 0.25f * static_cast<float> (idx + 1);

    normal.normal_x = 0.0f;
    normal.normal_y = 0.1f * static_cast<float> (idx % 3);
    normal.normal_z = 1.0f;
    normal.curvature = 0.02f * static_cast<float> (idx % 5);
  }

  return normals;
}

template <typename PointT>
typename PointCloud<PointT>::Ptr
makeNormalPlaneSourceCloud(const std::size_t nr_points)
{
  typename PointCloud<PointT>::Ptr cloud (new PointCloud<PointT>);
  cloud->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    auto& point = (*cloud)[idx];
    point.x = 0.15f * static_cast<float> (idx);
    point.y = 0.025f * static_cast<float> ((idx + 1) % 4);
    point.z = 0.0125f * static_cast<float> (idx);

    if constexpr (std::is_same_v<PointT, pcl::PointXYZI>)
    {
      point.intensity = 0.125f * static_cast<float> (idx + 3);
    }
    else if constexpr (std::is_same_v<PointT, pcl::PointXYZINormal>)
    {
      point.intensity = 0.125f * static_cast<float> (idx + 3);
      point.normal_x = 0.0f;
      point.normal_y = 0.0f;
      point.normal_z = 1.0f;
      point.curvature = 0.01f * static_cast<float> (idx % 5);
    }
  }

  return cloud;
}

template <typename PointNT>
void runNormalPlanePublicEntriesMatchDirectRVVForAoSNormal()
{
  // 本 helper 验证 normal layout expansion（法线布局扩展）：PointNT 只作为
  // normal cloud 使用，production RVV 只读取 normal_x/y/z/curvature。额外
  // xyz 或 intensity 字段不参与当前距离输出语义，但会改变 sizeof(PointNT)
  // 和字段偏移，因此需要 public entry（公开入口）和 direct RVV helper 对拍。
  constexpr std::size_t nr_points = 8;
  PointCloud<PointXYZ>::Ptr cloud (new PointCloud<PointXYZ>);
  cloud->resize (nr_points);
  auto normals = makeNormalPlaneNormalCloud<PointNT> (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    (*cloud)[idx].x = 0.2f * static_cast<float> (idx);
    (*cloud)[idx].y = 0.05f * static_cast<float> (idx % 4);
    (*cloud)[idx].z = 0.01f * static_cast<float> (idx);
  }

  pcl::Indices indices = {7, 0, 4, 2, 6, 1, 5, 3};
  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<PointXYZ, PointNT> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.2);

  pcl::Indices public_inliers;
  model.selectWithinDistance (model_coefficients, 0.08, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (model_coefficients, 0.08);
  std::vector<double> public_distances;
  model.getDistancesToModel (model_coefficients, public_distances);

#if defined (__RVV10__)
  pcl::Indices direct_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t direct_count =
      model.selectWithinDistanceRVV (model_coefficients, 0.08, direct_inliers);
  direct_inliers.resize (direct_count);
  model.error_sqr_dists_.resize (direct_count);
  const std::vector<double> direct_errors = model.error_sqr_dists_;

  std::vector<double> direct_distances;
  model.getDistancesToModelRVV (model_coefficients, direct_distances);
  ASSERT_EQ (direct_count, model.countWithinDistanceRVV (model_coefficients, 0.08));
#else
  pcl::Indices direct_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t direct_count =
      model.selectWithinDistanceStandard (model_coefficients, 0.08, direct_inliers);
  direct_inliers.resize (direct_count);
  model.error_sqr_dists_.resize (direct_count);
  const std::vector<double> direct_errors = model.error_sqr_dists_;

  std::vector<double> direct_distances;
  model.getDistancesToModelStandard (model_coefficients, direct_distances);
  ASSERT_EQ (direct_count, model.countWithinDistanceStandard (model_coefficients, 0.08));
#endif

  ASSERT_EQ (direct_count, public_count);
  ASSERT_EQ (direct_inliers, public_inliers);
  ASSERT_EQ (direct_errors.size (), public_errors.size ());
  for (std::size_t idx = 0; idx < public_errors.size (); ++idx)
    EXPECT_NEAR (direct_errors[idx], public_errors[idx], 1e-6);

  ASSERT_EQ (direct_distances.size (), public_distances.size ());
  for (std::size_t idx = 0; idx < public_distances.size (); ++idx)
    EXPECT_NEAR (direct_distances[idx], public_distances[idx], 1e-6);
}

template <typename PointT, typename PointNT>
void runNormalPlanePublicEntriesMatchDirectRVVForAoSSourceAndNormal()
{
  // 本 helper 验证 source × normal cross-product（source 与 normal 点型交叉组合）。
  // production gate 必须分别使用 PointT 的 xyz byte offset 和 PointNT 的
  // normal/curvature byte offset；这个测试防止两侧代表点型各自通过后，
  // 组合实例仍隐藏模板实例化或字段偏移风险。
  constexpr std::size_t nr_points = 9;
  auto cloud = makeNormalPlaneSourceCloud<PointT> (nr_points);
  auto normals = makeNormalPlaneNormalCloud<PointNT> (nr_points);

  pcl::Indices indices = {8, 1, 6, 3, 0, 7, 2, 5, 4};
  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<PointT, PointNT> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.2);

  pcl::Indices public_inliers;
  model.selectWithinDistance (model_coefficients, 0.08, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (model_coefficients, 0.08);
  std::vector<double> public_distances;
  model.getDistancesToModel (model_coefficients, public_distances);

#if defined (__RVV10__)
  pcl::Indices direct_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t direct_count =
      model.selectWithinDistanceRVV (model_coefficients, 0.08, direct_inliers);
  direct_inliers.resize (direct_count);
  model.error_sqr_dists_.resize (direct_count);
  const std::vector<double> direct_errors = model.error_sqr_dists_;

  std::vector<double> direct_distances;
  model.getDistancesToModelRVV (model_coefficients, direct_distances);
  ASSERT_EQ (direct_count, model.countWithinDistanceRVV (model_coefficients, 0.08));
#else
  pcl::Indices direct_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t direct_count =
      model.selectWithinDistanceStandard (model_coefficients, 0.08, direct_inliers);
  direct_inliers.resize (direct_count);
  model.error_sqr_dists_.resize (direct_count);
  const std::vector<double> direct_errors = model.error_sqr_dists_;

  std::vector<double> direct_distances;
  model.getDistancesToModelStandard (model_coefficients, direct_distances);
  ASSERT_EQ (direct_count, model.countWithinDistanceStandard (model_coefficients, 0.08));
#endif

  ASSERT_EQ (direct_count, public_count);
  ASSERT_EQ (direct_inliers, public_inliers);
  ASSERT_EQ (direct_errors.size (), public_errors.size ());
  for (std::size_t idx = 0; idx < public_errors.size (); ++idx)
    EXPECT_NEAR (direct_errors[idx], public_errors[idx], 1e-6);

  ASSERT_EQ (direct_distances.size (), public_distances.size ());
  for (std::size_t idx = 0; idx < public_distances.size (); ++idx)
    EXPECT_NEAR (direct_distances[idx], public_distances[idx], 1e-6);
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointXYZISource)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSSource<PointXYZI> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointXYZINormalSource)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSSource<PointXYZINormal> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointNormalNormalLayout)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSNormal<PointNormal> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointXYZINormalNormalLayout)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSNormal<PointXYZINormal> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointXYZIAndPointNormal)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSSourceAndNormal<PointXYZI, PointNormal> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointXYZIAndPointXYZINormal)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSSourceAndNormal<PointXYZI, PointXYZINormal> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointXYZINormalAndPointNormal)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSSourceAndNormal<PointXYZINormal, PointNormal> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesMatchDirectRVVForPointXYZINormalAndPointXYZINormal)
{
  runNormalPlanePublicEntriesMatchDirectRVVForAoSSourceAndNormal<PointXYZINormal, PointXYZINormal> ();
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesFallbackForNonAoSRegisteredXYZSource)
{
  // 这个 source 类型通过 PCL traits 注册了单 float x/y/z，但不是 standard-layout。
  // RVV byte-offset gather 不能安全读取它；公开入口必须在 RVV build 下丢弃
  // RVV helper 分支并回到 Standard helper。
  constexpr std::size_t nr_points = 5;
  PointCloud<NonAoSRegisteredXYZ>::Ptr cloud (new PointCloud<NonAoSRegisteredXYZ>);
  PointCloud<Normal>::Ptr normals (new PointCloud<Normal>);
  cloud->resize (nr_points);
  normals->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    (*cloud)[idx].x = 0.1f * static_cast<float> (idx);
    (*cloud)[idx].y = 0.0f;
    (*cloud)[idx].z = 0.01f * static_cast<float> (idx);
    (*normals)[idx].normal_x = 0.0f;
    (*normals)[idx].normal_y = 0.0f;
    (*normals)[idx].normal_z = 1.0f;
    (*normals)[idx].curvature = 0.0f;
  }

  pcl::Indices indices = {4, 1, 3, 0, 2};
  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<NonAoSRegisteredXYZ, Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.0);

  pcl::Indices public_inliers;
  model.selectWithinDistance (model_coefficients, 0.03, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (model_coefficients, 0.03);
  std::vector<double> public_distances;
  model.getDistancesToModel (model_coefficients, public_distances);

  pcl::Indices standard_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t standard_count =
      model.selectWithinDistanceStandard (model_coefficients, 0.03, standard_inliers);
  standard_inliers.resize (standard_count);
  model.error_sqr_dists_.resize (standard_count);
  const std::vector<double> standard_errors = model.error_sqr_dists_;

  std::vector<double> standard_distances;
  model.getDistancesToModelStandard (model_coefficients, standard_distances);

  ASSERT_EQ (standard_count, public_count);
  ASSERT_EQ (standard_count, model.countWithinDistanceStandard (model_coefficients, 0.03));
  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors, public_errors);
  ASSERT_EQ (standard_distances, public_distances);
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesFallbackForNonAoSRegisteredNormalLayout)
{
  // 这个 normal 类型注册了单 float normal_x/y/z/curvature，但不是
  // standard-layout。它隔离验证 normal layout gate：字段语义满足还不够，
  // byte-offset gather（按字节偏移离散加载）还必须有 AoS 布局前提。
  constexpr std::size_t nr_points = 6;
  PointCloud<PointXYZ>::Ptr cloud (new PointCloud<PointXYZ>);
  auto normals = makeNormalPlaneNormalCloud<NonAoSRegisteredNormal> (nr_points);
  cloud->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    (*cloud)[idx].x = 0.125f * static_cast<float> (idx);
    (*cloud)[idx].y = 0.0f;
    (*cloud)[idx].z = 0.015f * static_cast<float> (idx);
  }

  pcl::Indices indices = {5, 1, 4, 0, 3, 2};
  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<PointXYZ, NonAoSRegisteredNormal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.2);

  pcl::Indices public_inliers;
  model.selectWithinDistance (model_coefficients, 0.08, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (model_coefficients, 0.08);
  std::vector<double> public_distances;
  model.getDistancesToModel (model_coefficients, public_distances);

  pcl::Indices standard_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t standard_count =
      model.selectWithinDistanceStandard (model_coefficients, 0.08, standard_inliers);
  standard_inliers.resize (standard_count);
  model.error_sqr_dists_.resize (standard_count);
  const std::vector<double> standard_errors = model.error_sqr_dists_;

  std::vector<double> standard_distances;
  model.getDistancesToModelStandard (model_coefficients, standard_distances);

  ASSERT_EQ (standard_count, public_count);
  ASSERT_EQ (standard_count, model.countWithinDistanceStandard (model_coefficients, 0.08));
  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors, public_errors);
  ASSERT_EQ (standard_distances, public_distances);
}

TEST (SampleConsensusModelNormalPlane, PublicEntriesFallbackForUnregisteredNormalLayout)
{
  // 这个测试使用未注册 PCL curvature field（曲率字段）的 normal 类型。
  // 在 RVV 构建下公开入口必须走 scalar fallback（标量回退），同时仍保持
  // select/count/getDistances 三个公开 API 的输出一致。
  constexpr std::size_t nr_points = 5;
  PointCloud<PointXYZ>::Ptr cloud (new PointCloud<PointXYZ>);
  PointCloud<NormalWithDoubleCurvature>::Ptr normals (new PointCloud<NormalWithDoubleCurvature>);
  cloud->resize (nr_points);
  normals->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    (*cloud)[idx].x = 0.25f * static_cast<float> (idx);
    (*cloud)[idx].y = 0.0f;
    (*cloud)[idx].z = 0.01f * static_cast<float> (idx);
    (*normals)[idx].normal_x = 0.0f;
    (*normals)[idx].normal_y = 0.0f;
    (*normals)[idx].normal_z = 1.0f;
    (*normals)[idx].curvature = 0.0;
  }

  pcl::Indices indices = {4, 0, 3, 1, 2};
  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<PointXYZ, NormalWithDoubleCurvature> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.0);

  pcl::Indices public_inliers;
  model.selectWithinDistance (model_coefficients, 0.03, public_inliers);
  const std::vector<double> public_errors = model.error_sqr_dists_;
  const std::size_t public_count = model.countWithinDistance (model_coefficients, 0.03);
  std::vector<double> public_distances;
  model.getDistancesToModel (model_coefficients, public_distances);

  pcl::Indices standard_inliers;
  model.error_sqr_dists_.clear ();
  const std::size_t standard_count =
      model.selectWithinDistanceStandard (model_coefficients, 0.03, standard_inliers);
  standard_inliers.resize (standard_count);
  model.error_sqr_dists_.resize (standard_count);
  const std::vector<double> standard_errors = model.error_sqr_dists_;

  std::vector<double> standard_distances;
  model.getDistancesToModelStandard (model_coefficients, standard_distances);

  ASSERT_EQ (standard_count, public_count);
  ASSERT_EQ (standard_count, model.countWithinDistanceStandard (model_coefficients, 0.03));
  ASSERT_EQ (standard_inliers, public_inliers);
  ASSERT_EQ (standard_errors, public_errors);
  ASSERT_EQ (standard_distances, public_distances);
}

TEST (SampleConsensusModelNormalPlane, SelectHelperResizesEmptyOutputBuffers)
{
  // 本测试覆盖 protected helper 的缓冲区合同：直接调用 helper 时，即使
  // inliers / error_sqr_dists_ 为空，也不能依赖调用方先按 indices 数量 resize。
  // 这能防止 public entry（公开入口）之外的尾段复用或测试用 wrapper 写越界。
  constexpr std::size_t nr_points = 4;

  PointCloud<PointXYZ>::Ptr cloud (new PointCloud<PointXYZ>);
  PointCloud<Normal>::Ptr normals (new PointCloud<Normal>);
  cloud->resize (nr_points);
  normals->resize (nr_points);

  for (std::size_t idx = 0; idx < nr_points; ++idx)
  {
    (*cloud)[idx].x = static_cast<float> (idx);
    (*cloud)[idx].y = 0.0f;
    (*cloud)[idx].z = 0.01f * static_cast<float> (idx);

    (*normals)[idx].normal_x = 0.0f;
    (*normals)[idx].normal_y = 0.0f;
    (*normals)[idx].normal_z = 1.0f;
    (*normals)[idx].curvature = 0.0f;
  }

  pcl::Indices indices;
  for (std::size_t idx = 0; idx < nr_points; ++idx)
    indices.push_back (static_cast<int> (idx));

  Eigen::VectorXf model_coefficients (4);
  model_coefficients << 0.0f, 0.0f, 1.0f, 0.0f;

  SampleConsensusModelNormalPlaneTest<PointXYZ, Normal> model (cloud);
  model.setInputNormals (normals);
  model.setIndices (std::make_shared<std::vector<int>> (indices));
  model.setNormalDistanceWeight (0.0);

  pcl::Indices inliers_standard;
  model.error_sqr_dists_.clear ();
  const std::size_t count_standard =
      model.selectWithinDistanceStandard (model_coefficients, 0.2, inliers_standard);
  ASSERT_EQ (nr_points, count_standard);
  ASSERT_EQ (nr_points, inliers_standard.size ());
  ASSERT_EQ (nr_points, model.error_sqr_dists_.size ());

#if defined (__RVV10__)
  pcl::Indices inliers_rvv;
  model.error_sqr_dists_.clear ();
  const std::size_t count_rvv =
      model.selectWithinDistanceRVV (model_coefficients, 0.2, inliers_rvv);
  ASSERT_EQ (nr_points, count_rvv);
  ASSERT_EQ (nr_points, inliers_rvv.size ());
  ASSERT_EQ (nr_points, model.error_sqr_dists_.size ());
#endif
}

TEST (SampleConsensusModelNormalPlane, SIMD_selectWithinDistance)
{
  const auto seed = static_cast<unsigned> (std::time (nullptr));
  std::srand (seed);

  // === Timer Variables ===
  double total_time_standard = 0.0;
  double total_time_rvv = 0.0;

  // Configuration
  const size_t iterations = 1000;
  const size_t nr_points = 2000;

  for (size_t i = 0; i < iterations; i++)
  {
    // --- A. Data Generation ---
    PointCloud<PointXYZ>::Ptr cloud (new PointCloud<PointXYZ>);
    PointCloud<Normal>::Ptr normals (new PointCloud<Normal>);
    pcl::Indices indices;

    cloud->resize (nr_points);
    normals->resize (nr_points);
    indices.resize (nr_points);

    for (std::size_t idx = 0; idx < nr_points; ++idx)
    {
      // 1. Random Point XYZ, value range is [-5, 5]
      (*cloud)[idx].x = 10.0f * static_cast<float> (rand ()) / RAND_MAX - 5.0f;
      (*cloud)[idx].y = 10.0f * static_cast<float> (rand ()) / RAND_MAX - 5.0f;
      (*cloud)[idx].z = 10.0f * static_cast<float> (rand ()) / RAND_MAX - 5.0f;

      // 2. Random Normal (Must be Normalized), value range is [-0.5, 0.5]
      Eigen::Vector3f n;
      n[0] = static_cast<float> (rand ()) / RAND_MAX - 0.5f;
      n[1] = static_cast<float> (rand ()) / RAND_MAX - 0.5f;
      n[2] = static_cast<float> (rand ()) / RAND_MAX - 0.5f;
      n.normalize();
      (*normals)[idx].normal_x = n[0];
      (*normals)[idx].normal_y = n[1];
      (*normals)[idx].normal_z = n[2];

      // 3. Random Curvature [0, 1]
      (*normals)[idx].curvature = static_cast<float> (rand ()) / RAND_MAX;

      indices[idx] = static_cast<int>(idx);
    }

    // Instantiate Proxy
    SampleConsensusModelNormalPlaneTest<PointXYZ, Normal> model (cloud);
    model.setInputNormals(normals);
    model.setIndices(std::make_shared<std::vector<int>>(indices));

    double w = 0.5 * static_cast<double>(rand()) / RAND_MAX;
    model.setNormalDistanceWeight(w);

    // Generate random model
    Eigen::VectorXf model_coefficients(4);
    model_coefficients << static_cast<float> (rand ()) / RAND_MAX - 0.5f,
                          static_cast<float> (rand ()) / RAND_MAX - 0.5f,
                          static_cast<float> (rand ()) / RAND_MAX - 0.5f, 0.0f;
    model_coefficients.normalize ();
    model_coefficients(3) = static_cast<float> (rand ()) / RAND_MAX * 5.0f;

    const double threshold = 0.2 * static_cast<double> (rand ()) / RAND_MAX;

    // Output containers
    std::vector<int> inliers_standard, inliers_rvv;

    // === 1. Measure Standard Implementation ===
    {
        model.error_sqr_dists_.clear();
        model.error_sqr_dists_.resize(nr_points);
        inliers_standard.resize(nr_points);

        auto start_std = std::chrono::high_resolution_clock::now();
        size_t count_std = model.selectWithinDistanceStandard (model_coefficients, threshold, inliers_standard, 0);
        auto end_std = std::chrono::high_resolution_clock::now();
        total_time_standard += std::chrono::duration<double, std::milli>(end_std - start_std).count();

        inliers_standard.resize(count_std);
        model.error_sqr_dists_.resize(count_std);
    }

    // 拷贝 Standard 的距离结果
    std::vector<double> dists_standard = model.error_sqr_dists_;

    // === 2. Measure RVV Implementation ===
#if defined (__RVV10__)
    {
        model.error_sqr_dists_.clear();
        model.error_sqr_dists_.resize(nr_points);
        inliers_rvv.resize(nr_points);

        auto start_rvv = std::chrono::high_resolution_clock::now();
        size_t count_rvv = model.selectWithinDistanceRVV (model_coefficients, threshold, inliers_rvv);
        auto end_rvv = std::chrono::high_resolution_clock::now();
        total_time_rvv += std::chrono::duration<double, std::milli>(end_rvv - start_rvv).count();

        inliers_rvv.resize(count_rvv);
        model.error_sqr_dists_.resize(count_rvv);

        // --- Correctness Check (Robust with Tolerance Counter) ---
        const std::vector<double>& dists_rvv = model.error_sqr_dists_;

        size_t idx_s = 0;
        size_t idx_r = 0;

        // NormalPlane 包含 acos，在 float(RVV) 和 double(Standard) 之间会产生较大的精度差异。
        // value_tolerance: 允许距离计算值的绝对误差 (2e-3 足够覆盖大多数情况)
        const double value_tolerance = 2e-3;
        // boundary_tolerance: 判定是否在阈值边界的宽容度
        const double boundary_tolerance = 2e-3;

        // 允许的Mismatch数量 (模仿 countWithDistance 的 EXPECT_LE(diff, 2u))
        size_t mismatch_count = 0;
        const size_t max_allowed_mismatches = 2;

        while (idx_s < inliers_standard.size() && idx_r < inliers_rvv.size()) {
            int p_s = inliers_standard[idx_s];
            int p_r = inliers_rvv[idx_r];

            if (p_s == p_r) {
                // Case 1: 共同选中的点
                double d_s = dists_standard[idx_s];
                double d_r = dists_rvv[idx_r];

                if (std::abs(d_s - d_r) > value_tolerance) {
                    mismatch_count++;
                    if (mismatch_count > max_allowed_mismatches) {
                         FAIL() << "Distance mismatch at point " << p_s
                                << " (Common Inlier). Std=" << d_s << ", RVV=" << d_r
                                << ", Diff=" << std::abs(d_s - d_r)
                                << ". Too many mismatches!";
                    }
                }
                idx_s++;
                idx_r++;
            } else if (p_s < p_r) {
                // Case 2: Standard 选中 (漏检)
                double d_s = dists_standard[idx_s];
                // 必须在边界附近
                if (std::abs(d_s - threshold) > boundary_tolerance) {
                    mismatch_count++;
                    if (mismatch_count > max_allowed_mismatches) {
                        FAIL() << "Standard selected point " << p_s << " (dist=" << d_s
                               << ", th=" << threshold << ") but RVV rejected it. Not a boundary case!";
                    }
                }
                idx_s++;
            } else { // p_s > p_r
                // Case 3: RVV 选中 (误检)
                double d_r = dists_rvv[idx_r];
                // 必须在边界附近
                if (std::abs(d_r - threshold) > boundary_tolerance) {
                    mismatch_count++;
                    if (mismatch_count > max_allowed_mismatches) {
                        FAIL() << "RVV selected point " << p_r << " (dist=" << d_r
                               << ", th=" << threshold << ") but Standard rejected it. Not a boundary case!";
                    }
                }
                idx_r++;
            }
        }

        // 处理尾部剩余 (Tail Check)
        while (idx_s < inliers_standard.size()) {
            double d_s = dists_standard[idx_s];
            if (std::abs(d_s - threshold) > boundary_tolerance) mismatch_count++;
            idx_s++;
        }
        while (idx_r < inliers_rvv.size()) {
            double d_r = dists_rvv[idx_r];
            if (std::abs(d_r - threshold) > boundary_tolerance) mismatch_count++;
            idx_r++;
        }

        // 最终断言：错误总数不超过2
        ASSERT_LE(mismatch_count, max_allowed_mismatches)
            << "Too many mismatches (count=" << mismatch_count << ") at iter " << i << ", Seed: " << seed;
    }
#endif
  }

  // === Performance Report ===
  std::cout << "\n========================================================" << std::endl;
  std::cout << "[ Performance Report: NormalPlane (Select Within Distance) ]" << std::endl;
  std::cout << "Points per cloud    : " << nr_points << std::endl;
  std::cout << "Iterations          : " << iterations << std::endl;
  std::cout << std::fixed << std::setprecision(4);
  std::cout << "Standard Total Time : " << total_time_standard << " ms" << std::endl;

#if defined (__RVV10__)
  std::cout << "RVV Total Time      : " << total_time_rvv << " ms" << std::endl;
  if (total_time_rvv > 0) {
      std::cout << "Speedup (Std/RVV)   : " << total_time_standard / total_time_rvv << "x" << std::endl;
  }
#else
  std::cout << "RVV Total Time      : N/A (RVV not enabled)" << std::endl;
#endif
  std::cout << "========================================================" << std::endl;
}

TEST (SampleConsensusModelNormalPlane, SIMD_countWithinDistance_normal_plane_smoke) // Test if all countWithinDistance implementations return the same value
{
  const auto seed = static_cast<unsigned> (std::time (nullptr));
  srand (seed);
  for (size_t i=0; i<1000; i++) // Run as often as you like
  {
    // Generate a cloud with 10000 random points
    PointCloud<PointXYZ> cloud;
    PointCloud<Normal> normal_cloud;
    pcl::Indices indices;
    cloud.resize (10000);
    normal_cloud.resize (10000);
    for (std::size_t idx = 0; idx < cloud.size (); ++idx)
    {
      cloud[idx].x = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      cloud[idx].y = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      cloud[idx].z = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      const double a = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      const double b = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      const double c = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      const double factor = 1.0 / sqrt(a * a + b * b + c * c);
      normal_cloud[idx].normal[0] = a * factor;
      normal_cloud[idx].normal[1] = b * factor;
      normal_cloud[idx].normal[2] = c * factor;
      if (rand () % 4 != 0)
      {
        indices.push_back (static_cast<int> (idx));
      }
    }
    SampleConsensusModelNormalPlaneTest<PointXYZ, Normal> model (cloud.makeShared (), indices, true);

    const double normal_distance_weight = 0.3 * static_cast<double> (rand ()) / RAND_MAX; // in [0; 0.3]
    model.setNormalDistanceWeight (normal_distance_weight);
    model.setInputNormals (normal_cloud.makeShared ());

    // Generate random model parameters
    Eigen::VectorXf model_coefficients(4);
    model_coefficients << 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0,
                          2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0,
                          2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0, 0.0;
    model_coefficients.normalize ();
    model_coefficients(3) = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0; // Last parameter

    const double threshold = 0.1 * static_cast<double> (rand ()) / RAND_MAX; // threshold in [0; 0.1]

    // The number of inliers is usually somewhere between 0 and 100
    const auto res_standard = model.countWithinDistanceStandard (model_coefficients, threshold); // Standard
    pcl::utils::ignore(res_standard);
#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
    const auto res_sse      = model.countWithinDistanceSSE (model_coefficients, threshold); // SSE
    EXPECT_LE ((res_standard > res_sse ? res_standard - res_sse : res_sse - res_standard), 2u) << "seed=" << seed << ", i=" << i
        << ", model=(" << model_coefficients(0) << ", " << model_coefficients(1) << ", " << model_coefficients(2) << ", " << model_coefficients(3)
        << "), threshold=" << threshold << ", normal_distance_weight=" << normal_distance_weight << ", res_standard=" << res_standard << std::endl;
#endif
#if defined (__AVX__) && defined (__AVX2__)
    const auto res_avx      = model.countWithinDistanceAVX (model_coefficients, threshold); // AVX
    EXPECT_LE ((res_standard > res_avx ? res_standard - res_avx : res_avx - res_standard), 2u) << "seed=" << seed << ", i=" << i
        << ", model=(" << model_coefficients(0) << ", " << model_coefficients(1) << ", " << model_coefficients(2) << ", " << model_coefficients(3)
        << "), threshold=" << threshold << ", normal_distance_weight=" << normal_distance_weight << ", res_standard=" << res_standard << std::endl;
#endif
#if defined (__RVV10__)
    const auto res_rvv      = model.countWithinDistanceRVV (model_coefficients, threshold); // RVV
    EXPECT_LE ((res_standard > res_rvv ? res_standard - res_rvv : res_rvv - res_standard), 2u) << "seed=" << seed << ", i=" << i
        << ", model=(" << model_coefficients(0) << ", " << model_coefficients(1) << ", " << model_coefficients(2) << ", " << model_coefficients(3)
        << "), threshold=" << threshold << ", normal_distance_weight=" << normal_distance_weight << ", res_standard=" << res_standard << std::endl;
#endif
  }
}

// ---------------------------------------------------------------------------
// Test for SampleConsensusModelNormalPlane (Geometric + Angular Distance)
// ---------------------------------------------------------------------------
TEST (SampleConsensusModelNormalPlane, SIMD_countWithinDistance)
{
  const auto seed = static_cast<unsigned> (std::time (nullptr));
  srand (seed);

  // === Timer Variables ===
  double total_time_standard = 0.0;
  double total_time_sse = 0.0;
  double total_time_avx = 0.0;
  double total_time_rvv = 0.0;

  // Increase iterations for more accurate measurement
  const size_t iterations = 1000;

  for (size_t i = 0; i < iterations; i++)
  {
    // Generate a cloud with 10000 random points and normals
    PointCloud<PointXYZ> cloud;
    PointCloud<Normal> normal_cloud;
    pcl::Indices indices;
    cloud.resize (10000);
    normal_cloud.resize (10000);

    for (std::size_t idx = 0; idx < cloud.size (); ++idx)
    {
      cloud[idx].x = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f;
      cloud[idx].y = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f;
      cloud[idx].z = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f;

      const double a = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      const double b = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      const double c = 2.0 * static_cast<float> (rand ()) / RAND_MAX - 1.0;
      const double factor = 1.0 / sqrt(a * a + b * b + c * c);

      normal_cloud[idx].normal[0] = static_cast<float>(a * factor);
      normal_cloud[idx].normal[1] = static_cast<float>(b * factor);
      normal_cloud[idx].normal[2] = static_cast<float>(c * factor);

      if (rand () % 4 != 0)
      {
        indices.push_back (static_cast<int> (idx));
      }
    }

    SampleConsensusModelNormalPlaneTest<PointXYZ, Normal> model (cloud.makeShared (), indices, true);

    const double normal_distance_weight = 0.3 * static_cast<double> (rand ()) / RAND_MAX; // in [0; 0.3]
    model.setNormalDistanceWeight (normal_distance_weight);
    model.setInputNormals (normal_cloud.makeShared ());

    // Generate random model parameters
    Eigen::VectorXf model_coefficients(4);
    model_coefficients << 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f,
                          2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f,
                          2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f, 0.0f;
    model_coefficients.normalize ();
    model_coefficients(3) = 2.0f * static_cast<float> (rand ()) / RAND_MAX - 1.0f; // Last parameter

    const double threshold = 0.1 * static_cast<double> (rand ()) / RAND_MAX; // threshold in [0; 0.1]

    // === 1. Measure Standard Implementation ===
    auto start_std = std::chrono::high_resolution_clock::now();
    const auto res_standard = model.countWithinDistanceStandard (model_coefficients, threshold);
    auto end_std = std::chrono::high_resolution_clock::now();
    total_time_standard += std::chrono::duration<double, std::milli>(end_std - start_std).count();

    // Use pcl::utils::ignore or simple void cast to avoid unused variable warning in Release builds
    (void)res_standard;

    // === 2. Measure SSE Implementation ===
#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
    auto start_sse = std::chrono::high_resolution_clock::now();
    const auto res_sse = model.countWithinDistanceSSE (model_coefficients, threshold);
    auto end_sse = std::chrono::high_resolution_clock::now();
    total_time_sse += std::chrono::duration<double, std::milli>(end_sse - start_sse).count();

    EXPECT_LE ((res_standard > res_sse ? res_standard - res_sse : res_sse - res_standard), 2u)
        << "SSE mismatch! seed=" << seed << ", i=" << i << std::endl;
#endif

    // === 3. Measure AVX Implementation ===
#if defined (__AVX__) && defined (__AVX2__)
    auto start_avx = std::chrono::high_resolution_clock::now();
    const auto res_avx = model.countWithinDistanceAVX (model_coefficients, threshold);
    auto end_avx = std::chrono::high_resolution_clock::now();
    total_time_avx += std::chrono::duration<double, std::milli>(end_avx - start_avx).count();

    EXPECT_LE ((res_standard > res_avx ? res_standard - res_avx : res_avx - res_standard), 2u)
        << "AVX mismatch! seed=" << seed << ", i=" << i << std::endl;
#endif

    // === 4. Measure RVV Implementation ===
#if defined (__RVV10__)
    auto start_rvv = std::chrono::high_resolution_clock::now();
    const auto res_rvv = model.countWithinDistanceRVV (model_coefficients, threshold);
    auto end_rvv = std::chrono::high_resolution_clock::now();
    total_time_rvv += std::chrono::duration<double, std::milli>(end_rvv - start_rvv).count();

    // Verify correctness
    EXPECT_LE ((res_standard > res_rvv ? res_standard - res_rvv : res_rvv - res_standard), 2u)
        << "RVV mismatch! seed=" << seed << ", i=" << i
        << ", threshold=" << threshold
        << ", weight=" << normal_distance_weight << std::endl;
#endif
  }

  // === Performance Report ===
  std::cout << "\n========================================================" << std::endl;
  std::cout << "[ Performance Report: NormalPlane (Weighted Distance) ]" << std::endl;
  std::cout << "Points per cloud    : 10000" << std::endl;
  std::cout << "Iterations          : " << iterations << std::endl;
  std::cout << std::fixed << std::setprecision(4);
  std::cout << "Standard Total Time : " << total_time_standard << " ms" << std::endl;

#if defined (__SSE__) && defined (__SSE2__) && defined (__SSE4_1__)
  std::cout << "SSE Total Time      : " << total_time_sse << " ms" << std::endl;
  std::cout << "Speedup (Std/SSE)   : " << total_time_standard / total_time_sse << "x" << std::endl;
#endif

#if defined (__AVX__) && defined (__AVX2__)
  std::cout << "AVX Total Time      : " << total_time_avx << " ms" << std::endl;
  std::cout << "Speedup (Std/AVX)   : " << total_time_standard / total_time_avx << "x" << std::endl;
#endif

#if defined (__RVV10__)
  std::cout << "RVV Total Time      : " << total_time_rvv << " ms" << std::endl;
  std::cout << "Speedup (Std/RVV)   : " << total_time_standard / total_time_rvv << "x" << std::endl;
#endif
  std::cout << "========================================================" << std::endl;
}

TEST (SampleConsensusModelNormalPlane, SIMD_getDistancesToModel)
{
  const auto seed = static_cast<unsigned> (std::time (nullptr));
  std::srand (seed);

  // === Timer Variables ===
  double total_time_standard = 0.0;
  double total_time_rvv = 0.0;

  // Configuration
  const size_t iterations = 1000;
  const size_t nr_points = 2000;

  for (size_t i = 0; i < iterations; i++)
  {
    // --- A. Data Generation (与 SIMD_selectWithinDistance 完全一致) ---
    PointCloud<PointXYZ>::Ptr cloud (new PointCloud<PointXYZ>);
    PointCloud<Normal>::Ptr normals (new PointCloud<Normal>);
    pcl::Indices indices;

    cloud->resize (nr_points);
    normals->resize (nr_points);
    indices.resize (nr_points);

    for (std::size_t idx = 0; idx < nr_points; ++idx)
    {
      // 1. Random Point XYZ, value range is [-5, 5]
      (*cloud)[idx].x = 10.0f * static_cast<float> (rand ()) / RAND_MAX - 5.0f;
      (*cloud)[idx].y = 10.0f * static_cast<float> (rand ()) / RAND_MAX - 5.0f;
      (*cloud)[idx].z = 10.0f * static_cast<float> (rand ()) / RAND_MAX - 5.0f;

      // 2. Random Normal (Must be Normalized), value range is [-0.5, 0.5]
      Eigen::Vector3f n;
      n[0] = static_cast<float> (rand ()) / RAND_MAX - 0.5f;
      n[1] = static_cast<float> (rand ()) / RAND_MAX - 0.5f;
      n[2] = static_cast<float> (rand ()) / RAND_MAX - 0.5f;
      n.normalize();
      (*normals)[idx].normal_x = n[0];
      (*normals)[idx].normal_y = n[1];
      (*normals)[idx].normal_z = n[2];

      // 3. Random Curvature [0, 1]
      (*normals)[idx].curvature = static_cast<float> (rand ()) / RAND_MAX;

      indices[idx] = static_cast<int>(idx);
    }

    // Instantiate Proxy
    SampleConsensusModelNormalPlaneTest<PointXYZ, Normal> model (cloud);
    model.setInputNormals(normals);
    model.setIndices(std::make_shared<std::vector<int>>(indices));

    // Random weight
    double w = 0.5 * static_cast<double>(rand()) / RAND_MAX;
    model.setNormalDistanceWeight(w);

    // Generate random model coefficients
    Eigen::VectorXf model_coefficients(4);
    model_coefficients << static_cast<float> (rand ()) / RAND_MAX - 0.5f,
                          static_cast<float> (rand ()) / RAND_MAX - 0.5f,
                          static_cast<float> (rand ()) / RAND_MAX - 0.5f,
                          0.0f;
    model_coefficients.normalize ();
    model_coefficients(3) = static_cast<float> (rand ()) / RAND_MAX * 5.0f;

    // Output containers
    std::vector<double> distances_standard, distances_rvv;

    distances_rvv.resize(nr_points);
    distances_standard.resize(nr_points);

    // === 1. Measure Standard Implementation ===
    {
      auto start = std::chrono::high_resolution_clock::now ();
      model.getDistancesToModelStandard (model_coefficients, distances_standard);
      auto end = std::chrono::high_resolution_clock::now ();
      total_time_standard += std::chrono::duration<double, std::milli> (end - start).count ();
    }

    // === 2. Measure RVV Implementation ===
#if defined (__RVV10__)
    {
      auto start = std::chrono::high_resolution_clock::now ();
      model.getDistancesToModelRVV (model_coefficients, distances_rvv);
      auto end = std::chrono::high_resolution_clock::now ();
      total_time_rvv += std::chrono::duration<double, std::milli> (end - start).count ();
    }

    // === 3. Verification ===
    ASSERT_EQ (distances_standard.size (), distances_rvv.size ());

    for (std::size_t idx = 0; idx < distances_standard.size (); ++idx)
    {
      EXPECT_NEAR (distances_standard[idx], distances_rvv[idx], 5e-3)
        << "Mismatch at index " << idx
        << ", weight=" << w
        << ", coeffs=[" << model_coefficients.transpose() << "]";
    }
#endif
  }

  // === Performance Report ===
  std::cout << "\n========================================================" << std::endl;
  std::cout << "[ Performance Report: NormalPlane (getDistancesToModel) ]" << std::endl;
  std::cout << "Points per cloud    : " << nr_points << std::endl;
  std::cout << "Iterations          : " << iterations << std::endl;
  std::cout << std::fixed << std::setprecision (4);
  std::cout << "Standard Total Time : " << total_time_standard << " ms" << std::endl;

#if defined (__RVV10__)
  std::cout << "RVV Total Time      : " << total_time_rvv << " ms" << std::endl;
  if (total_time_rvv > 0.0)
    std::cout << "Speedup (Std/RVV)   : " << total_time_standard / total_time_rvv << "x" << std::endl;
#endif
  std::cout << "========================================================\n" << std::endl;
}

TEST (SampleConsensusModelPlane, OptimizeFarFromOrigin)
{ // Test if the model can successfully optimize a plane that is far from the origin
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
  Eigen::Vector3d x(-0.435197968, 0.598251061, -0.672828654);
  Eigen::Vector3d y(-0.547340139, 0.417556627,  0.725303548);
  Eigen::Vector3d z( 0.714857680, 0.683916759,  0.145727023); // This is the normal of the plane
  Eigen::Vector3d center(7380.86467, -8350.60056617, 4324.22814107);
  for(double i=-0.5; i<0.5; i+=0.01)
    for(double j=-0.5; j<0.5; j+=0.01) {
      Eigen::Vector3d p = center + i*x + j*y;
      cloud->emplace_back(p[0], p[1], p[2]);
    }
  pcl::SampleConsensusModelPlane<pcl::PointXYZ> model(cloud, true);
  pcl::Indices inliers;
  for(std::size_t i=0; i<cloud->size(); ++i) inliers.push_back(i);
  Eigen::VectorXf coeffs(4); // Doesn't have to be initialized, the function doesn't use them
  Eigen::VectorXf optimized_coeffs(4);
  model.optimizeModelCoefficients(inliers, coeffs, optimized_coeffs);
  EXPECT_NEAR(optimized_coeffs[0], z[0], 6e-6);
  EXPECT_NEAR(optimized_coeffs[1], z[1], 6e-6);
  EXPECT_NEAR(optimized_coeffs[2], z[2], 6e-6);
#ifndef __i386__
  EXPECT_NEAR(optimized_coeffs[3], -z.dot(center), 5e-2);
#else
  EXPECT_NEAR(optimized_coeffs[3], -z.dot(center), 1e-1);
#endif
}

int
main (int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "No test file given. Please download `sac_plane_test.pcd` and pass its path to the test." << std::endl;
    return (-1);
  }

  // Load a standard PCD file from disk
  pcl::PCLPointCloud2 cloud_blob;
  if (loadPCDFile (argv[1], cloud_blob) < 0)
  {
    std::cerr << "Failed to read test file. Please download `sac_plane_test.pcd` and pass its path to the test." << std::endl;
    return (-1);
  }

  fromPCLPointCloud2 (cloud_blob, *cloud_);
  fromPCLPointCloud2 (cloud_blob, *normals_);

  indices_.resize (cloud_->size ());
  for (std::size_t i = 0; i < indices_.size (); ++i) { indices_[i] = static_cast<int>(i); }

  testing::InitGoogleTest (&argc, argv);
  return (RUN_ALL_TESTS ());
}
