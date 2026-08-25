#include "apmf.h"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/segmentation/approximate_progressive_morphological_filter.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

struct FilterParams {
  int max_window_size = 9;
  float slope = 0.35f;
  float max_distance = 0.8f;
  float initial_distance = 0.18f;
  float cell_size = 1.0f;
  float base = 2.0f;
  bool exponential = true;
};

template <typename PointT>
void
fillExtraFields(PointT&, const std::size_t)
{
}

void
fillExtraFields(pcl::PointXYZI& point, const std::size_t i)
{
  point.intensity = static_cast<float>(i % 31);
}

void
fillExtraFields(pcl::PointXYZRGB& point, const std::size_t i)
{
  point.r = static_cast<std::uint8_t>((i * 3) % 251);
  point.g = static_cast<std::uint8_t>((i * 5) % 251);
  point.b = static_cast<std::uint8_t>((i * 7) % 251);
}

void
fillExtraFields(pcl::PointXYZRGBA& point, const std::size_t i)
{
  point.r = static_cast<std::uint8_t>((i * 3) % 251);
  point.g = static_cast<std::uint8_t>((i * 5) % 251);
  point.b = static_cast<std::uint8_t>((i * 7) % 251);
  point.a = static_cast<std::uint8_t>(255 - (i % 17));
}

pcl::PointCloud<pcl::PointXYZ>
makeCloud(const std::size_t n, const bool with_invalid = false)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = !with_invalid;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) * 0.37f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 17) % 251) - 125) * 0.41f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 29) % 509) - 254) * 0.015f;
  }
  if (with_invalid && n > 130) {
    cloud[7].x = std::numeric_limits<float>::quiet_NaN();
    cloud[67].z = std::numeric_limits<float>::infinity();
    cloud[129].y = -std::numeric_limits<float>::infinity();
  }
  return cloud;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makePublicCloud(const std::size_t n, const bool with_invalid = false)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = !with_invalid;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    (*cloud)[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) * 0.37f;
    (*cloud)[i].y = static_cast<float>(static_cast<int>((i * 17) % 251) - 125) * 0.41f;
    (*cloud)[i].z = static_cast<float>(static_cast<int>((i * 29) % 509) - 254) * 0.015f;
    fillExtraFields((*cloud)[i], i);
  }
  if (with_invalid && n > 130) {
    (*cloud)[7].x = std::numeric_limits<float>::quiet_NaN();
    (*cloud)[67].z = std::numeric_limits<float>::infinity();
    (*cloud)[129].y = -std::numeric_limits<float>::infinity();
  }
  return cloud;
}

std::vector<int>
computeHalfSizes(const FilterParams& params)
{
  std::vector<int> half_sizes;
  int iteration = 0;
  float window_size = 0.0f;
  while (window_size < params.max_window_size) {
    const int half_size =
        params.exponential
            ? static_cast<int>(std::pow(static_cast<float>(params.base), iteration))
            : ((iteration + 1) * static_cast<int>(params.base));
    window_size = 2 * half_size + 1;
    half_sizes.push_back(half_size);
    ++iteration;
  }
  return half_sizes;
}

std::vector<float>
computeHeightThresholds(const FilterParams& params, const std::vector<int>& half_sizes)
{
  std::vector<float> thresholds;
  std::vector<float> window_sizes;
  thresholds.reserve(half_sizes.size());
  window_sizes.reserve(half_sizes.size());
  for (std::size_t i = 0; i < half_sizes.size(); ++i) {
    const float window_size = static_cast<float>(2 * half_sizes[i] + 1);
    float threshold =
        i == 0 ? params.initial_distance
               : (params.slope * (window_size - window_sizes[i - 1]) * params.cell_size +
                  params.initial_distance);
    threshold = std::min(threshold, params.max_distance);
    window_sizes.push_back(window_size);
    thresholds.push_back(threshold);
  }
  return thresholds;
}

template <typename PointT>
pcl::Indices
runPublicFilter(const typename pcl::PointCloud<PointT>::ConstPtr& cloud,
                const FilterParams& params)
{
  pcl::ApproximateProgressiveMorphologicalFilter<PointT> filter;
  filter.setInputCloud(cloud);
  filter.setMaxWindowSize(params.max_window_size);
  filter.setSlope(params.slope);
  filter.setMaxDistance(params.max_distance);
  filter.setInitialDistance(params.initial_distance);
  filter.setCellSize(params.cell_size);
  filter.setBase(params.base);
  filter.setExponential(params.exponential);
  filter.setNumberOfThreads(1);

  pcl::Indices ground;
  filter.extract(ground);
  return ground;
}

pcl::Indices
runReferenceFilter(const pcl::PointCloud<pcl::PointXYZ>& cloud, const FilterParams& params)
{
  const auto shape = pcl_rvv_segmentation_apmf::computeGridShape(cloud, params.cell_size);
  const auto half_sizes = computeHalfSizes(params);
  const auto thresholds = computeHeightThresholds(params, half_sizes);
  return pcl_rvv_segmentation_apmf::progressiveFilterStd(
             cloud, shape, half_sizes, thresholds)
      .ground;
}

template <typename PointT>
pcl::PointCloud<pcl::PointXYZ>
toXYZCloud(const pcl::PointCloud<PointT>& cloud)
{
  pcl::PointCloud<pcl::PointXYZ> xyz;
  xyz.width = cloud.width;
  xyz.height = cloud.height;
  xyz.is_dense = cloud.is_dense;
  xyz.points.resize(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    xyz[i].x = cloud[i].x;
    xyz[i].y = cloud[i].y;
    xyz[i].z = cloud[i].z;
  }
  return xyz;
}

void
expectMatrixEqualWithNan(const Eigen::MatrixXf& actual, const Eigen::MatrixXf& expected)
{
  ASSERT_EQ(actual.rows(), expected.rows());
  ASSERT_EQ(actual.cols(), expected.cols());
  for (int r = 0; r < actual.rows(); ++r) {
    for (int c = 0; c < actual.cols(); ++c) {
      if (std::isnan(expected(r, c)))
        EXPECT_TRUE(std::isnan(actual(r, c))) << "cell=(" << r << "," << c << ")";
      else
        EXPECT_FLOAT_EQ(actual(r, c), expected(r, c)) << "cell=(" << r << "," << c << ")";
    }
  }
}

} // namespace

TEST(ApproximateProgressiveMorphologicalFilterDiag, GridZMinMatchesScalarDense)
{
  const auto cloud = makeCloud(4096);
  const auto shape = pcl_rvv_segmentation_apmf::computeGridShape(cloud, 1.0f);
  const auto expected = pcl_rvv_segmentation_apmf::computeGridZMinStd(cloud, shape);

#if defined(__RVV10__)
  Eigen::MatrixXf actual;
  ASSERT_TRUE(pcl_rvv_segmentation_apmf::computeGridZMinRVV(cloud, shape, actual));
  expectMatrixEqualWithNan(actual, expected);
#else
  EXPECT_GT(expected.size(), 0);
#endif
}

TEST(ApproximateProgressiveMorphologicalFilterDiag, GridZMinSkipsInvalidWhenNonDense)
{
  const auto cloud = makeCloud(4096, true);
  const auto shape = pcl_rvv_segmentation_apmf::computeGridShape(cloud, 1.0f);
  const auto expected = pcl_rvv_segmentation_apmf::computeGridZMinStd(cloud, shape);

#if defined(__RVV10__)
  Eigen::MatrixXf actual;
  ASSERT_TRUE(pcl_rvv_segmentation_apmf::computeGridZMinRVV(cloud, shape, actual));
  expectMatrixEqualWithNan(actual, expected);
#else
  EXPECT_GT(expected.size(), 0);
#endif
}

TEST(ApproximateProgressiveMorphologicalFilterDiag, WindowOpenMatchesScalarWithNanHoles)
{
  Eigen::MatrixXf grid(9, 7);
  grid.setConstant(std::numeric_limits<float>::quiet_NaN());
  for (int c = 0; c < grid.cols(); ++c)
    for (int r = 0; r < grid.rows(); ++r)
      if ((r + c) % 5 != 0)
        grid(r, c) = static_cast<float>((r * 13 + c * 7) % 31) * 0.25f;

  const auto expected = pcl_rvv_segmentation_apmf::morphologicalOpenStd(grid, 2);

#if defined(__RVV10__)
  Eigen::MatrixXf actual;
  ASSERT_TRUE(pcl_rvv_segmentation_apmf::morphologicalOpenRVV(grid, 2, actual));
  expectMatrixEqualWithNan(actual, expected);
#else
  EXPECT_GT(expected.size(), 0);
#endif
}

TEST(ApproximateProgressiveMorphologicalFilterDiag, TailCompressPreservesIndexOrder)
{
  const auto cloud = makeCloud(512);
  const auto shape = pcl_rvv_segmentation_apmf::computeGridShape(cloud, 1.0f);
  const auto filtered = pcl_rvv_segmentation_apmf::computeGridZMinStd(cloud, shape);
  pcl::Indices ground;
  ground.reserve(cloud.size());
  for (std::size_t i = 0; i < cloud.size(); ++i)
    ground.push_back(static_cast<int>(i));
  const auto expected =
      pcl_rvv_segmentation_apmf::thresholdGroundStd(cloud, ground, shape, filtered, 0.35f);

#if defined(__RVV10__)
  pcl::Indices actual;
  ASSERT_TRUE(pcl_rvv_segmentation_apmf::thresholdGroundRVV(
      cloud, ground, shape, filtered, 0.35f, actual));
  EXPECT_EQ(actual, expected);
#else
  EXPECT_FALSE(expected.empty());
#endif
}

TEST(ApproximateProgressiveMorphologicalFilterDiag, FullPipelineMatchesScalarMultiIteration)
{
  const auto cloud = makeCloud(4096, true);
  const auto shape = pcl_rvv_segmentation_apmf::computeGridShape(cloud, 1.0f);
  const std::vector<int> half_sizes{1, 2, 4};
  const std::vector<float> height_thresholds{0.20f, 0.35f, 0.50f};
  const auto expected = pcl_rvv_segmentation_apmf::progressiveFilterStd(
      cloud, shape, half_sizes, height_thresholds);

#if defined(__RVV10__)
  pcl_rvv_segmentation_apmf::ProgressiveFilterResult actual;
  ASSERT_TRUE(pcl_rvv_segmentation_apmf::progressiveFilterRVV(
      cloud, shape, half_sizes, height_thresholds, actual));
  expectMatrixEqualWithNan(actual.grid, expected.grid);
  EXPECT_EQ(actual.ground, expected.ground);
#else
  EXPECT_FALSE(expected.ground.empty());
  EXPECT_EQ(expected.grid.rows(), shape.rows);
  EXPECT_EQ(expected.grid.cols(), shape.cols);
#endif
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractMatchesReferenceDense)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZ>(4096);
  const auto expected = runReferenceFilter(*cloud, params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZ>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractMatchesReferenceNonDense)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZ>(4096, true);
  const auto expected = runReferenceFilter(*cloud, params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZ>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractSmallInputFallsBack)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZ>(32);
  const auto expected = runReferenceFilter(*cloud, params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZ>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractPointXYZIMatchesXYZReference)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZI>(4096);
  const auto expected = runReferenceFilter(toXYZCloud(*cloud), params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZI>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractPointXYZINonDenseMatchesXYZReference)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZI>(4096, true);
  const auto expected = runReferenceFilter(toXYZCloud(*cloud), params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZI>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractPointXYZRGBMatchesXYZReference)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZRGB>(4096);
  const auto expected = runReferenceFilter(toXYZCloud(*cloud), params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZRGB>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractPointXYZRGBNonDenseMatchesXYZReference)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZRGB>(4096, true);
  const auto expected = runReferenceFilter(toXYZCloud(*cloud), params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZRGB>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractPointXYZRGBAMatchesXYZReference)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZRGBA>(4096);
  const auto expected = runReferenceFilter(toXYZCloud(*cloud), params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZRGBA>(cloud, params), expected);
}

TEST(ApproximateProgressiveMorphologicalFilterProduction, PublicExtractPointXYZRGBANonDenseMatchesXYZReference)
{
  const FilterParams params;
  const auto cloud = makePublicCloud<pcl::PointXYZRGBA>(4096, true);
  const auto expected = runReferenceFilter(toXYZCloud(*cloud), params);

  EXPECT_EQ(runPublicFilter<pcl::PointXYZRGBA>(cloud, params), expected);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
