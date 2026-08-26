/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）为 PFHRGB（带颜色的点特征直方图）
 * 建立 same-chain correctness（同构链路正确性）验收。后续 RVV candidate
 * （候选实现）必须先和当前 production helper（生产源码 helper）对拍一致，不能只靠
 * bench（性能测试）数字。
 *
 * 证据边界：
 * 本文件不修改 production 头文件。Std/RVV 两个构建都运行同一套测试，用来验证
 * PFHRGB topic scaffold（脚手架）和后续候选的 correctness gate（正确性验收条件）。
 */

#include "pfhrgb.h"

#include <pcl/features/pfh_tools.h>
#include <pcl/features/pfhrgb.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>
#include <pcl/test/gtest.h>

#include <cmath>

namespace
{
namespace pfhrgb_test = pcl::features::rvv_test::pfhrgb;

using PointT = pfhrgb_test::PointT;
using CloudT = pfhrgb_test::CloudT;
using Estimator = pcl::PFHRGBEstimation<PointT, PointT, pcl::PFHRGBSignature250>;

using RGBCloudT = pcl::PointCloud<pcl::PointXYZRGB>;
using MixedEstimator =
    pcl::PFHRGBEstimation<pcl::PointXYZRGB, pcl::PointXYZRGBNormal, pcl::PFHRGBSignature250>;

void
expectVectorNear(const Eigen::VectorXf& actual, const Eigen::VectorXf& expected, const float tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (Eigen::Index i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "bin=" << i;
}

RGBCloudT::Ptr
makeRgbOnlyCloud(const CloudT& source)
{
  auto cloud = pcl::make_shared<RGBCloudT>();
  cloud->reserve(source.size());
  for (const auto& src : source)
  {
    pcl::PointXYZRGB point;
    point.x = src.x;
    point.y = src.y;
    point.z = src.z;
    point.r = src.r;
    point.g = src.g;
    point.b = src.b;
    cloud->push_back(point);
  }
  cloud->width = source.width;
  cloud->height = source.height;
  cloud->is_dense = source.is_dense;
  return cloud;
}

void
computeMixedPointPFHRGBReference(const RGBCloudT& cloud,
                                 const CloudT& normals,
                                 const pcl::Indices& indices,
                                 const int nr_split,
                                 Eigen::VectorXf& histogram)
{
  histogram.setZero(2 * nr_split * nr_split * nr_split);
  if (indices.size() < 2)
    return;

  const float hist_incr =
      100.0f / static_cast<float>(indices.size() * (indices.size() - 1) / 2);
  for (const auto index_i : indices)
  {
    for (const auto index_j : indices)
    {
      if (index_i == index_j)
        continue;

      const auto& point_i = cloud[static_cast<std::size_t>(index_i)];
      const auto& point_j = cloud[static_cast<std::size_t>(index_j)];
      const auto& normal_i = normals[static_cast<std::size_t>(index_i)];
      const auto& normal_j = normals[static_cast<std::size_t>(index_j)];
      const Eigen::Vector4i colors_i(point_i.r, point_i.g, point_i.b, 0);
      const Eigen::Vector4i colors_j(point_j.r, point_j.g, point_j.b, 0);

      float f1 = 0.0f;
      float f2 = 0.0f;
      float f3 = 0.0f;
      float f4 = 0.0f;
      float f5 = 0.0f;
      float f6 = 0.0f;
      float f7 = 0.0f;
      pcl::computeRGBPairFeatures(point_i.getVector4fMap(),
                                  normal_i.getNormalVector4fMap(),
                                  colors_i,
                                  point_j.getVector4fMap(),
                                  normal_j.getNormalVector4fMap(),
                                  colors_j,
                                  f1,
                                  f2,
                                  f3,
                                  f4,
                                  f5,
                                  f6,
                                  f7);
      if (f4 == 0.0f)
        continue;
      pfhrgb_test::accumulatePFHRGBHistogramBins(
          f1, f2, f3, f5, f6, f7, nr_split, hist_incr, histogram);
    }
  }
}
} // namespace

TEST(PFHRGBReference, ComputesPublicDescriptorLikeProductionHelper)
{
  const auto cloud = pfhrgb_test::makeFeatureCloud(9);
  Eigen::VectorXf expected(250);
  pcl::PointCloud<pcl::PFHRGBSignature250> output;
  pcl::Indices neighborhood;
  std::vector<float> distances;
  auto tree = pcl::make_shared<pcl::search::KdTree<PointT>>();
  Estimator pfhrgb;
  tree->setInputCloud(cloud);
  pfhrgb.setInputCloud(cloud);
  pfhrgb.setInputNormals(cloud);
  pfhrgb.setSearchMethod(tree);
  pfhrgb.setKSearch(16);
  pfhrgb.compute(output);

  ASSERT_GT(tree->nearestKSearch(0, 16, neighborhood, distances), 0);
  pfhrgb_test::computePointPFHRGBReference(*cloud, neighborhood, 5, expected);

  ASSERT_FALSE(output.empty());
  const Eigen::Map<const Eigen::VectorXf> actual(output[0].histogram, 250);

  expectVectorNear(actual, expected, 1e-5f);
}

TEST(PFHRGBProduction, ExactPointTypePublicEntryMatchesScalarReference)
{
  const auto cloud = pfhrgb_test::makeFeatureCloud(9);
  constexpr int k = 16;

  pcl::PointCloud<pcl::PFHRGBSignature250> output;
  auto tree = pcl::make_shared<pcl::search::KdTree<PointT>>();
  Estimator pfhrgb;
  tree->setInputCloud(cloud);
  pfhrgb.setInputCloud(cloud);
  pfhrgb.setInputNormals(cloud);
  pfhrgb.setSearchMethod(tree);
  pfhrgb.setKSearch(k);
  pfhrgb.compute(output);

  ASSERT_EQ(output.size(), cloud->size());
  for (std::size_t point_index = 0; point_index < cloud->size(); ++point_index)
  {
    pcl::Indices neighborhood;
    std::vector<float> distances;
    ASSERT_GT(tree->nearestKSearch(static_cast<int>(point_index), k, neighborhood, distances), 0);

    Eigen::VectorXf expected(250);
    pfhrgb_test::computePointPFHRGBReference(*cloud, neighborhood, 5, expected);
    const Eigen::Map<const Eigen::VectorXf> actual(output[point_index].histogram, 250);
    expectVectorNear(actual, expected, 2e-3f);
  }
}

TEST(PFHRGBProduction, NonExactSourcePointTypeKeepsScalarFallbackSemantics)
{
  const auto normal_cloud = pfhrgb_test::makeFeatureCloud(8);
  const auto rgb_cloud = makeRgbOnlyCloud(*normal_cloud);
  constexpr int k = 12;

  pcl::PointCloud<pcl::PFHRGBSignature250> output;
  auto tree = pcl::make_shared<pcl::search::KdTree<pcl::PointXYZRGB>>();
  MixedEstimator pfhrgb;
  tree->setInputCloud(rgb_cloud);
  pfhrgb.setInputCloud(rgb_cloud);
  pfhrgb.setInputNormals(normal_cloud);
  pfhrgb.setSearchMethod(tree);
  pfhrgb.setKSearch(k);
  pfhrgb.compute(output);

  ASSERT_EQ(output.size(), rgb_cloud->size());
  for (std::size_t point_index = 0; point_index < rgb_cloud->size(); ++point_index)
  {
    pcl::Indices neighborhood;
    std::vector<float> distances;
    ASSERT_GT(tree->nearestKSearch(static_cast<int>(point_index), k, neighborhood, distances), 0);

    Eigen::VectorXf expected(250);
    computeMixedPointPFHRGBReference(*rgb_cloud, *normal_cloud, neighborhood, 5, expected);
    const Eigen::Map<const Eigen::VectorXf> actual(output[point_index].histogram, 250);
    expectVectorNear(actual, expected, 1e-5f);
  }
}

TEST(PFHRGBCandidate, PairBatchRVVComputesHistogramCloseToReference)
{
  const auto cloud = pfhrgb_test::makeFeatureCloud(10);
  const pcl::Indices neighborhood = pfhrgb_test::makeWrappedNeighborhood(*cloud, 48, 23);

  Eigen::VectorXf expected;
  Eigen::VectorXf actual;
  pfhrgb_test::computePointPFHRGBReference(*cloud, neighborhood, 5, expected);
  pfhrgb_test::computePointPFHRGBSignaturePairBatchRVV(*cloud, neighborhood, 5, actual);

  expectVectorNear(actual, expected, 2e-3f);
}

TEST(PFHRGBCandidate, PublicShapedCandidateComputesDescriptorsCloseToEstimator)
{
  const auto cloud = pfhrgb_test::makeFeatureCloud(12);
  constexpr int k = 20;

  pcl::PointCloud<pcl::PFHRGBSignature250> expected;
  pcl::PointCloud<pcl::PFHRGBSignature250> actual;

  Estimator pfhrgb;
  pfhrgb.setInputCloud(cloud);
  pfhrgb.setInputNormals(cloud);
  pfhrgb.setSearchMethod(pcl::search::KdTree<PointT>::Ptr(new pcl::search::KdTree<PointT>));
  pfhrgb.setKSearch(k);
  pfhrgb.compute(expected);

  pfhrgb_test::computePublicPFHRGBWithPairBatchCandidate(*cloud, k, actual);

  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t point_index = 0; point_index < actual.size(); ++point_index)
  {
    const Eigen::Map<const Eigen::VectorXf> actual_histogram(actual[point_index].histogram, 250);
    const Eigen::Map<const Eigen::VectorXf> expected_histogram(expected[point_index].histogram, 250);
    expectVectorNear(actual_histogram, expected_histogram, 2e-3f);
  }
}

TEST(PFHRGBCandidate, ReusablePublicShapedCandidateComputesDescriptorsCloseToEstimator)
{
  const auto cloud = pfhrgb_test::makeFeatureCloud(12);
  constexpr int k = 20;

  pcl::PointCloud<pcl::PFHRGBSignature250> expected;
  pcl::PointCloud<pcl::PFHRGBSignature250> actual;

  Estimator pfhrgb;
  pfhrgb.setInputCloud(cloud);
  pfhrgb.setInputNormals(cloud);
  pfhrgb.setSearchMethod(pcl::search::KdTree<PointT>::Ptr(new pcl::search::KdTree<PointT>));
  pfhrgb.setKSearch(k);
  pfhrgb.compute(expected);

  pfhrgb_test::computePublicPFHRGBWithReusablePairBatchCandidate(*cloud, k, actual);

  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t point_index = 0; point_index < actual.size(); ++point_index)
  {
    const Eigen::Map<const Eigen::VectorXf> actual_histogram(actual[point_index].histogram, 250);
    const Eigen::Map<const Eigen::VectorXf> expected_histogram(expected[point_index].histogram, 250);
    expectVectorNear(actual_histogram, expected_histogram, 2e-3f);
  }
}
