/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）先把 GASD（Globally Aligned Spatial
 * Distribution，全球对齐空间分布）topic 的当前生产入口跑通，再对 fixed-grid
 * histogram copy（固定网格直拷贝）candidate 做 same-chain（同构链路）对拍。
 *
 * 证据边界：
 * 本文件不修改 production 头文件。它证明的是公开入口能在合成输入上产出稳定 descriptor，
 * 以及 candidate helper 的连续写回语义与标量参考一致。
 */

#include "gasd.h"

#include <pcl/features/gasd.h>
#include <pcl/test/gtest.h>

#include <cmath>
#include <cstdint>
#include <utility>

namespace gasd = pcl::features::rvv_test::gasd;

namespace
{
template <typename PointOutT>
void
expectDescriptorFinite(const pcl::PointCloud<PointOutT>& descriptor)
{
  ASSERT_EQ(descriptor.size(), 1u);
  for (Eigen::Index i = 0; i < descriptor[0].descriptorSize(); ++i)
    EXPECT_TRUE(std::isfinite(descriptor[0].histogram[static_cast<std::size_t>(i)]));
}

void
expectFlatEqual(const std::vector<float>& actual, const std::vector<float>& expected, const float tolerance)
{
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i)
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "bin=" << i;
}
} // namespace

TEST(GASDProduction, ShapeDescriptorRunsOnSyntheticCloud)
{
  const auto cloud = gasd::makeShapeCloud(64);

  pcl::GASDEstimation<pcl::PointXYZ, pcl::GASDSignature512> estimator;
  estimator.setInputCloud(cloud);

  pcl::PointCloud<pcl::GASDSignature512> descriptor;
  estimator.compute(descriptor);

  expectDescriptorFinite(descriptor);
}

TEST(GASDProduction, ColorDescriptorRunsOnSyntheticCloud)
{
  const auto shape_cloud = gasd::makeShapeCloud(64);
  const auto color_cloud = gasd::makeColorCloud(*shape_cloud);

  pcl::GASDColorEstimation<pcl::PointXYZRGBA, pcl::GASDSignature984> estimator;
  estimator.setInputCloud(color_cloud);

  pcl::PointCloud<pcl::GASDSignature984> descriptor;
  estimator.compute(descriptor);

  expectDescriptorFinite(descriptor);
}

TEST(GASDHistogramCopy, CopiesShapeCellsLikeScalarReference)
{
  const auto hists = gasd::makeHistogramGrid(4, 8);
  const std::vector<float> expected = gasd::copyShapeHistogramsStd(hists, 4, 8);
  std::vector<float> actual;
  gasd::copyShapeHistogramsRVVToBuffer(hists, 4, 8, actual);
  expectFlatEqual(actual, expected, 0.0f);
}

TEST(GASDHistogramCopy, CopiesColorCellsLikeScalarReference)
{
  auto hists = gasd::makeHistogramGrid(3, 12);
  const std::vector<float> expected = gasd::copyColorHistogramsStd(hists, 3, 12);
  std::vector<float> actual;
  gasd::copyColorHistogramsRVVToBuffer(std::move(hists), 3, 12, actual);
  expectFlatEqual(actual, expected, 0.0f);
}

TEST(GASDShapeProjection, ProjectsSamplesLikeScalarReference)
{
  const auto cloud = gasd::makeShapeCloud(96);
  gasd::ShapeProjectionBuffers expected;
  gasd::ShapeProjectionBuffers actual;

  gasd::projectShapeSamplesStdToBuffers(*cloud, 8.0f, 16.0f, 4, 12, expected);
  gasd::projectShapeSamplesRVVToBuffers(*cloud, 8.0f, 16.0f, 4, 12, actual);

  expectFlatEqual(actual.grid_x, expected.grid_x, 1.0e-5f);
  expectFlatEqual(actual.grid_y, expected.grid_y, 1.0e-5f);
  expectFlatEqual(actual.grid_z, expected.grid_z, 1.0e-5f);
  expectFlatEqual(actual.dbin, expected.dbin, 1.0e-4f);
}

TEST(GASDShapeProjection, ProjectsBenchSizedCloudWithProductionLikeNormalization)
{
  const auto cloud = gasd::makeShapeCloud(4096);
  const auto normalization = gasd::computeShapeProjectionNormalization(*cloud);
  gasd::ShapeProjectionBuffers expected;
  gasd::ShapeProjectionBuffers actual;

  gasd::projectShapeSamplesStdToBuffers(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, expected);
  gasd::projectShapeSamplesRVVToBuffers(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, actual);

  expectFlatEqual(actual.grid_x, expected.grid_x, 1.0e-5f);
  expectFlatEqual(actual.grid_y, expected.grid_y, 1.0e-5f);
  expectFlatEqual(actual.grid_z, expected.grid_z, 1.0e-5f);
  expectFlatEqual(actual.dbin, expected.dbin, 1.0e-3f);
}

TEST(GASDColorHue, ProjectsHueBinsLikeScalarReference)
{
  auto cloud = gasd::ColorCloudT::Ptr(new gasd::ColorCloudT);
  cloud->points.resize(8);
  cloud->width = 8;
  cloud->height = 1;
  cloud->is_dense = true;

  cloud->points[0].r = 80;
  cloud->points[0].g = 80;
  cloud->points[0].b = 80;
  cloud->points[1].r = 255;
  cloud->points[1].g = 0;
  cloud->points[1].b = 0;
  cloud->points[2].r = 0;
  cloud->points[2].g = 255;
  cloud->points[2].b = 0;
  cloud->points[3].r = 0;
  cloud->points[3].g = 0;
  cloud->points[3].b = 255;
  cloud->points[4].r = 200;
  cloud->points[4].g = 40;
  cloud->points[4].b = 120;
  cloud->points[5].r = 50;
  cloud->points[5].g = 180;
  cloud->points[5].b = 90;
  cloud->points[6].r = 30;
  cloud->points[6].g = 60;
  cloud->points[6].b = 210;
  cloud->points[7].r = 120;
  cloud->points[7].g = 15;
  cloud->points[7].b = 200;

  gasd::ColorHueBuffers expected;
  gasd::ColorHueBuffers actual;

  gasd::projectColorHueStdToBuffers(*cloud, 12, expected);
  gasd::projectColorHueRVVToBuffers(*cloud, 12, actual);

  expectFlatEqual(actual.hue, expected.hue, 1.0e-4f);
  expectFlatEqual(actual.hbin, expected.hbin, 1.0e-4f);
}

TEST(GASDInterpolation, ComputesTrilinearWeightsLikeScalarReference)
{
  const auto cloud = gasd::makeShapeCloud(257);
  const auto normalization = gasd::computeShapeProjectionNormalization(*cloud);
  gasd::ShapeProjectionBuffers projection;
  gasd::projectShapeSamplesStdToBuffers(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, projection);

  gasd::TrilinearInterpolationBuffers expected;
  gasd::TrilinearInterpolationBuffers actual;

  gasd::computeTrilinearInterpolationStdToBuffers(projection, 6, expected);
  gasd::computeTrilinearInterpolationRVVToBuffers(projection, 6, actual);

  EXPECT_EQ(actual.grid_idx, expected.grid_idx);
  EXPECT_EQ(actual.h_idx, expected.h_idx);
  expectFlatEqual(actual.w000, expected.w000, 1.0e-5f);
  expectFlatEqual(actual.w001, expected.w001, 1.0e-5f);
  expectFlatEqual(actual.w010, expected.w010, 1.0e-5f);
  expectFlatEqual(actual.w011, expected.w011, 1.0e-5f);
  expectFlatEqual(actual.w100, expected.w100, 1.0e-5f);
  expectFlatEqual(actual.w101, expected.w101, 1.0e-5f);
  expectFlatEqual(actual.w110, expected.w110, 1.0e-5f);
  expectFlatEqual(actual.w111, expected.w111, 1.0e-5f);
}

TEST(GASDHistogramWrite, AccumulatesTrilinearFlatHistogramLikeScalarReference)
{
  const auto cloud = gasd::makeShapeCloud(257);
  const auto normalization = gasd::computeShapeProjectionNormalization(*cloud);
  gasd::ShapeProjectionBuffers projection;
  gasd::projectShapeSamplesStdToBuffers(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, projection);

  std::vector<float> expected;
  std::vector<float> actual;
  const float hist_incr = 100.0f / static_cast<float>(cloud->size() - 1);

  gasd::accumulateTrilinearHistogramStd(projection, 6, 16, hist_incr, expected);
  gasd::accumulateTrilinearHistogramRVVStaged(projection, 6, 16, hist_incr, actual);

  expectFlatEqual(actual, expected, 1.0e-5f);
}

TEST(GASDEigenHistogramWrite, AccumulatesTrilinearEigenHistogramLikeScalarReference)
{
  const auto cloud = gasd::makeShapeCloud(257);
  const auto normalization = gasd::computeShapeProjectionNormalization(*cloud);
  gasd::ShapeProjectionBuffers projection;
  gasd::projectShapeSamplesStdToBuffers(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, projection);

  gasd::HistogramGrid expected;
  gasd::HistogramGrid actual;
  const float hist_incr = 100.0f / static_cast<float>(cloud->size() - 1);

  gasd::accumulateTrilinearHistogramEigenStd(projection, 6, 16, hist_incr, expected);
  gasd::accumulateTrilinearHistogramEigenRVVStaged(projection, 6, 16, hist_incr, actual);

  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t cell = 0; cell < actual.size(); ++cell)
    expectFlatEqual(std::vector<float>(actual[cell].data(), actual[cell].data() + actual[cell].size()),
                    std::vector<float>(expected[cell].data(), expected[cell].data() + expected[cell].size()),
                    1.0e-5f);
}

TEST(GASDShapeCombined, ComputesTrilinearShapeDescriptorLikeScalarReference)
{
  const auto cloud = gasd::makeShapeCloud(257);
  const auto normalization = gasd::computeShapeProjectionNormalization(*cloud);
  std::vector<float> expected;
  std::vector<float> actual;

  gasd::computeShapeDescriptorTrilinearStd(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, expected);
  gasd::computeShapeDescriptorTrilinearRVVStaged(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, actual);

  expectFlatEqual(actual, expected, 1.0e-5f);
  EXPECT_EQ(gasd::checksumFlatScaled(actual, 1000.0f), gasd::checksumFlatScaled(expected, 1000.0f));
}

TEST(GASDShapeCombined, BenchSizedCloudMatchesScalarReference)
{
  const auto cloud = gasd::makeShapeCloud(4096);
  const auto normalization = gasd::computeShapeProjectionNormalization(*cloud);
  std::vector<float> expected;
  std::vector<float> actual;

  gasd::computeShapeDescriptorTrilinearStd(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, expected);
  gasd::computeShapeDescriptorTrilinearRVVStaged(*cloud, normalization.max_coord, normalization.distance_normalization_factor, 6, 16, actual);

  expectFlatEqual(actual, expected, 1.0e-5f);
  EXPECT_EQ(gasd::checksumFlatScaled(actual, 1000.0f), gasd::checksumFlatScaled(expected, 1000.0f));
}
