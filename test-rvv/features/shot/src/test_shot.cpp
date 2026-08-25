/*
 * shot.hpp RVV topic correctness tests.
 *
 * 本文件验证 SHOT352 和 SHOT1344 公开入口在固定 local reference frame（局部参考系）
 * 下的 descriptor（描述子）归一化、indices（索引）子集和 NaN fallback（非法路径回退）
 * 语义。当前阶段只提供 production-shaped diagnostic（生产形态诊断）证据，不证明
 * production dispatch（生产分流）已经接入 RVV。
 */

#include <pcl/test/gtest.h>
#include <pcl/features/shot.h>
#include <pcl/search/kdtree.h>

#include "shot.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

constexpr int kSide = 11;
constexpr double kSearchRadius = 0.055;

template <typename EstimatorT, typename CloudT, typename OutputT>
void
configureShotEstimator(EstimatorT& estimator,
                       const typename CloudT::Ptr& cloud,
                       const pcl::PointCloud<pcl::Normal>::Ptr& normals,
                       const pcl::IndicesPtr& indices,
                       const pcl::PointCloud<pcl::ReferenceFrame>::Ptr& frames)
{
  estimator.setInputCloud(cloud);
  estimator.setSearchSurface(cloud);
  estimator.setInputNormals(normals);
  estimator.setIndices(indices);
  estimator.setInputReferenceFrames(frames);
  estimator.setSearchMethod(typename pcl::search::KdTree<typename CloudT::PointType>::Ptr(
      new pcl::search::KdTree<typename CloudT::PointType>));
  estimator.setRadiusSearch(kSearchRadius);
}

template <typename OutputT>
void
expectFiniteUnitDescriptors(const pcl::PointCloud<OutputT>& output)
{
  ASSERT_FALSE(output.empty());
  ASSERT_TRUE(output.is_dense);
  for (const auto& point : output) {
    for (const float value : point.descriptor)
      ASSERT_TRUE(std::isfinite(value));
    EXPECT_NEAR(pcl_rvv_shot::descriptorL2Norm(point), 1.0f, 1e-4f);
    for (const float value : point.rf)
      ASSERT_TRUE(std::isfinite(value));
  }
}

std::vector<float>
makeDescriptorSample(const int length)
{
  std::vector<float> values(static_cast<std::size_t>(length));
  for (int i = 0; i < length; ++i) {
    const float wave = std::sin(static_cast<float>(i) * 0.071f);
    values[static_cast<std::size_t>(i)] = 0.125f + 0.5f * wave + static_cast<float>((i % 11) + 1) * 0.01f;
  }
  return values;
}

void
expectNearDescriptor(const std::vector<float>& reference, const std::vector<float>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i)
    EXPECT_NEAR(reference[i], candidate[i], 2e-6f) << "descriptor index " << i;
}

void
makeShapeBinSamples(std::vector<float>& nx, std::vector<float>& ny, std::vector<float>& nz)
{
  const std::size_t count = nx.size();
  for (std::size_t i = 0; i < count; ++i) {
    nx[i] = std::sin(static_cast<float>(i) * 0.17f) * 0.35f;
    ny[i] = std::cos(static_cast<float>(i) * 0.11f) * 0.25f;
    nz[i] = 0.72f + static_cast<float>(i % 9) * 0.04f;
  }
  nx[3] = std::numeric_limits<float>::quiet_NaN();
  nz[17] = 2.0f;
  nz[31] = -2.0f;
}

void
expectNearShapeBins(const std::vector<double>& reference, const std::vector<double>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    if (std::isnan(reference[i])) {
      EXPECT_TRUE(std::isnan(candidate[i])) << "shape-bin index " << i;
    } else {
      EXPECT_NEAR(reference[i], candidate[i], 1e-9) << "shape-bin index " << i;
    }
  }
}

void
expectNearInterpolationGeometry(const std::vector<double>& ref_x,
                                const std::vector<double>& ref_y,
                                const std::vector<double>& ref_z,
                                const std::vector<double>& ref_distance,
                                const std::vector<std::uint8_t>& ref_valid,
                                const std::vector<double>& cand_x,
                                const std::vector<double>& cand_y,
                                const std::vector<double>& cand_z,
                                const std::vector<double>& cand_distance,
                                const std::vector<std::uint8_t>& cand_valid)
{
  ASSERT_EQ(ref_x.size(), cand_x.size());
  ASSERT_EQ(ref_y.size(), cand_y.size());
  ASSERT_EQ(ref_z.size(), cand_z.size());
  ASSERT_EQ(ref_distance.size(), cand_distance.size());
  ASSERT_EQ(ref_valid.size(), cand_valid.size());
  for (std::size_t i = 0; i < ref_x.size(); ++i) {
    EXPECT_EQ(ref_valid[i], cand_valid[i]) << "valid lane " << i;
    EXPECT_NEAR(ref_x[i], cand_x[i], 1e-9) << "x projection " << i;
    EXPECT_NEAR(ref_y[i], cand_y[i], 1e-9) << "y projection " << i;
    EXPECT_NEAR(ref_z[i], cand_z[i], 1e-9) << "z projection " << i;
    EXPECT_NEAR(ref_distance[i], cand_distance[i], 1e-9) << "distance " << i;
  }
}

void
makeColorLabSamples(std::vector<float>& l, std::vector<float>& a, std::vector<float>& b)
{
  const std::size_t count = l.size();
  for (std::size_t i = 0; i < count; ++i) {
    l[i] = 0.12f + static_cast<float>(i % 19) * 0.037f;
    a[i] = -0.85f + static_cast<float>(i % 31) * 0.055f;
    b[i] = 0.75f - static_cast<float>(i % 29) * 0.051f;
  }
  l[7] = 4.0f;
  a[11] = -7.0f;
  b[23] = 7.0f;
}

void
expectNearColorBins(const std::vector<double>& reference, const std::vector<double>& candidate)
{
  ASSERT_EQ(reference.size(), candidate.size());
  for (std::size_t i = 0; i < reference.size(); ++i)
    EXPECT_NEAR(reference[i], candidate[i], 1e-9) << "color-bin index " << i;
}

void
expectNearBinSelection(const std::vector<std::int32_t>& ref_desc_index,
                       const std::vector<std::int32_t>& ref_step_index,
                       const std::vector<std::int32_t>& ref_adjacent_index,
                       const std::vector<float>& ref_adjacent_delta,
                       const std::vector<float>& ref_center_weight,
                       const std::vector<std::uint8_t>& ref_valid,
                       const std::vector<std::int32_t>& cand_desc_index,
                       const std::vector<std::int32_t>& cand_step_index,
                       const std::vector<std::int32_t>& cand_adjacent_index,
                       const std::vector<float>& cand_adjacent_delta,
                       const std::vector<float>& cand_center_weight,
                       const std::vector<std::uint8_t>& cand_valid)
{
  ASSERT_EQ(ref_desc_index.size(), cand_desc_index.size());
  ASSERT_EQ(ref_step_index.size(), cand_step_index.size());
  ASSERT_EQ(ref_adjacent_index.size(), cand_adjacent_index.size());
  ASSERT_EQ(ref_adjacent_delta.size(), cand_adjacent_delta.size());
  ASSERT_EQ(ref_center_weight.size(), cand_center_weight.size());
  ASSERT_EQ(ref_valid.size(), cand_valid.size());
  for (std::size_t i = 0; i < ref_desc_index.size(); ++i) {
    EXPECT_EQ(ref_valid[i], cand_valid[i]) << "valid lane " << i;
    EXPECT_EQ(ref_desc_index[i], cand_desc_index[i]) << "desc index " << i;
    EXPECT_EQ(ref_step_index[i], cand_step_index[i]) << "step index " << i;
    EXPECT_EQ(ref_adjacent_index[i], cand_adjacent_index[i]) << "adjacent index " << i;
    EXPECT_NEAR(ref_adjacent_delta[i], cand_adjacent_delta[i], 1e-7f) << "adjacent delta " << i;
    EXPECT_NEAR(ref_center_weight[i], cand_center_weight[i], 1e-7f) << "center weight " << i;
  }
}

pcl::PointCloud<pcl::Normal>
makeAosShapeBinNormals()
{
  constexpr std::size_t kCount = 257;
  std::vector<float> nx(kCount), ny(kCount), nz(kCount);
  makeShapeBinSamples(nx, ny, nz);

  pcl::PointCloud<pcl::Normal> normals;
  normals.resize(kCount);
  for (std::size_t i = 0; i < kCount; ++i) {
    normals[i].normal_x = nx[i];
    normals[i].normal_y = ny[i];
    normals[i].normal_z = nz[i];
  }
  return normals;
}

template <typename NormalT>
pcl::PointCloud<NormalT>
makeNormalLikeShapeBinCloud()
{
  const auto normal_samples = makeAosShapeBinNormals();
  pcl::PointCloud<NormalT> normals;
  normals.resize(normal_samples.size());
  for (std::size_t i = 0; i < normal_samples.size(); ++i) {
    normals[i].normal_x = normal_samples[i].normal_x;
    normals[i].normal_y = normal_samples[i].normal_y;
    normals[i].normal_z = normal_samples[i].normal_z;
  }
  return normals;
}

template <typename NormalT>
unsigned
computeShapeBinDistanceIndexedScalarGeneric(const pcl::PointCloud<NormalT>& normals,
                                            const pcl::Indices& indices,
                                            const float frame_z[3],
                                            const int nr_shape_bins,
                                            double* out)
{
  const double scale = static_cast<double>(nr_shape_bins) * 0.5;
  Eigen::Vector4f current_frame_z(frame_z[0], frame_z[1], frame_z[2], 0.0f);
  unsigned nan_counter = 0;
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const Eigen::Vector4f& normal_vec =
        normals[static_cast<std::size_t>(indices[i])].getNormalVector4fMap();
    if (!std::isfinite(normal_vec[0]) || !std::isfinite(normal_vec[1]) ||
        !std::isfinite(normal_vec[2])) {
      out[i] = std::numeric_limits<double>::quiet_NaN();
      ++nan_counter;
      continue;
    }

    double cosine_desc = normal_vec.dot(current_frame_z);
    if (cosine_desc > 1.0)
      cosine_desc = 1.0;
    if (cosine_desc < -1.0)
      cosine_desc = -1.0;
    out[i] = (1.0 + cosine_desc) * scale;
  }
  return nan_counter;
}

template <typename PointInT, typename NormalT, typename OutputT>
class ShotShapeBinDirectProbe
: public pcl::SHOTEstimation<PointInT, NormalT, OutputT> {
 public:
  using Base = pcl::SHOTEstimation<PointInT, NormalT, OutputT>;
  using Base::createBinDistanceShape;
};

} // namespace

TEST(Shot352, PublicEntryWithProvidedReferenceFramesProducesUnitDescriptors)
{
  const auto cloud = pcl_rvv_shot::makeShotShapeCloud(kSide);
  const auto normals = pcl_rvv_shot::makeShotNormals(cloud->size());
  const auto indices = pcl_rvv_shot::makeCenterIndices(kSide);
  const auto frames = pcl_rvv_shot::makeIdentityFrames(indices->size());

  pcl::SHOTEstimation<pcl::PointXYZ, pcl::Normal, pcl::SHOT352> shot;
  configureShotEstimator<decltype(shot), pcl::PointCloud<pcl::PointXYZ>, pcl::SHOT352>(
      shot, cloud, normals, indices, frames);

  pcl::PointCloud<pcl::SHOT352> output;
  shot.compute(output);

  ASSERT_EQ(output.size(), indices->size());
  expectFiniteUnitDescriptors(output);
}

TEST(Shot352, InvalidReferenceFrameMarksDescriptorAsNonDenseNaN)
{
  const auto cloud = pcl_rvv_shot::makeShotShapeCloud(kSide);
  const auto normals = pcl_rvv_shot::makeShotNormals(cloud->size());
  const auto indices = pcl_rvv_shot::makeCenterIndices(kSide);
  const auto frames = pcl_rvv_shot::makeIdentityFrames(indices->size());
  (*frames)[1].x_axis[0] = std::numeric_limits<float>::quiet_NaN();

  pcl::SHOTEstimation<pcl::PointXYZ, pcl::Normal, pcl::SHOT352> shot;
  configureShotEstimator<decltype(shot), pcl::PointCloud<pcl::PointXYZ>, pcl::SHOT352>(
      shot, cloud, normals, indices, frames);

  pcl::PointCloud<pcl::SHOT352> output;
  shot.compute(output);

  ASSERT_EQ(output.size(), indices->size());
  EXPECT_FALSE(output.is_dense);
  EXPECT_TRUE(pcl_rvv_shot::descriptorAllNaN(output[1]));
  for (const float value : output[1].rf)
    EXPECT_TRUE(std::isnan(value));
}

TEST(Shot1344, ColorPublicEntryWithProvidedReferenceFramesProducesUnitDescriptors)
{
  const auto cloud = pcl_rvv_shot::makeShotColorCloud(kSide);
  const auto normals = pcl_rvv_shot::makeShotNormals(cloud->size());
  const auto indices = pcl_rvv_shot::makeCenterIndices(kSide);
  const auto frames = pcl_rvv_shot::makeIdentityFrames(indices->size());

  pcl::SHOTColorEstimation<pcl::PointXYZRGBA, pcl::Normal, pcl::SHOT1344> shot(true, true);
  configureShotEstimator<decltype(shot), pcl::PointCloud<pcl::PointXYZRGBA>, pcl::SHOT1344>(
      shot, cloud, normals, indices, frames);

  pcl::PointCloud<pcl::SHOT1344> output;
  shot.compute(output);

  ASSERT_EQ(output.size(), indices->size());
  expectFiniteUnitDescriptors(output);
}

// 这个测试先约束 Phase 010 的 same-chain（同构链路）合同：RVV 候选必须和
// production `normalizeHistogram` 相同，先用 double 累加平方和，再按 float 写回。
TEST(ShotNormalizeComponent, RvvMatchesScalarReferenceForShot352)
{
  auto reference = makeDescriptorSample(352);
  auto candidate = reference;

  pcl_rvv_shot::normalizeDescriptorScalar(reference.data(), reference.size());
  pcl_rvv_shot::normalizeDescriptorRVV(candidate.data(), candidate.size());

  expectNearDescriptor(reference, candidate);
  EXPECT_NEAR(pcl_rvv_shot::descriptorL2Norm(reference.data(), reference.size()), 1.0f, 2e-6f);
  EXPECT_NEAR(pcl_rvv_shot::descriptorL2Norm(candidate.data(), candidate.size()), 1.0f, 2e-6f);
}

// 1344 维覆盖 SHOTColor 的长 descriptor。它是后续板卡 component bench 的主候选，
// 因为更长的连续数组更可能摊薄 RVV setup（向量初始化）开销。
TEST(ShotNormalizeComponent, RvvMatchesScalarReferenceForShot1344)
{
  auto reference = makeDescriptorSample(1344);
  auto candidate = reference;

  pcl_rvv_shot::normalizeDescriptorScalar(reference.data(), reference.size());
  pcl_rvv_shot::normalizeDescriptorRVV(candidate.data(), candidate.size());

  expectNearDescriptor(reference, candidate);
  EXPECT_NEAR(pcl_rvv_shot::descriptorL2Norm(reference.data(), reference.size()), 1.0f, 2e-6f);
  EXPECT_NEAR(pcl_rvv_shot::descriptorL2Norm(candidate.data(), candidate.size()), 1.0f, 2e-6f);
}

// Shape-bin 组件先覆盖 production `createBinDistanceShape` 的核心数学：
// finite normal 检查、normal dot frame_z、clamp 到 [-1, 1] 和 double 输出。
TEST(ShotShapeBinComponent, RvvMatchesScalarReferenceForFiniteClampAndNaN)
{
  constexpr std::size_t kCount = 257;
  std::vector<float> nx(kCount), ny(kCount), nz(kCount);
  makeShapeBinSamples(nx, ny, nz);
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};

  std::vector<double> reference(kCount);
  std::vector<double> candidate(kCount);

  pcl_rvv_shot::computeShapeBinDistanceScalar(
      nx.data(), ny.data(), nz.data(), kCount, frame_z, 10, reference.data());
  pcl_rvv_shot::computeShapeBinDistanceRVV(
      nx.data(), ny.data(), nz.data(), kCount, frame_z, 10, candidate.data());

  expectNearShapeBins(reference, candidate);
}

// Phase 050 只验证 interpolation 前置 geometry staging（几何暂存）：
// indexed surface gather、中心点差值、三轴 dot、distance 和 valid lane mask。
// 它不实现 histogram scatter，也不覆盖 `acos` / `atan2` 后续语义。
TEST(ShotInterpolationGeometryComponent, RvvMatchesScalarReferenceForIndexedSurfaceCloud)
{
  const auto cloud = pcl_rvv_shot::makeShotShapeCloud(kSide);
  const pcl::Indices indices = {12, 13, 24, 36, 48, 60, 72, 84, 96, 108, 120};
  std::vector<float> sqr_dists(indices.size());
  std::vector<double> bin_distance(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i) {
    sqr_dists[i] = 0.0004f + static_cast<float>(i % 7) * 0.00013f;
    bin_distance[i] = 2.25 + static_cast<double>(i % 5) * 0.25;
  }
  sqr_dists[3] = 0.0f;
  bin_distance[7] = std::numeric_limits<double>::quiet_NaN();

  const float central[3] = {(*cloud)[60].x, (*cloud)[60].y, (*cloud)[60].z};
  const float frame_x[3] = {0.96f, 0.08f, 0.02f};
  const float frame_y[3] = {-0.05f, 0.98f, 0.04f};
  const float frame_z[3] = {0.03f, -0.02f, 1.0f};

  std::vector<double> ref_x(indices.size()), ref_y(indices.size()), ref_z(indices.size()),
      ref_distance(indices.size());
  std::vector<double> cand_x(indices.size()), cand_y(indices.size()), cand_z(indices.size()),
      cand_distance(indices.size());
  std::vector<std::uint8_t> ref_valid(indices.size()), cand_valid(indices.size());

  pcl_rvv_shot::computeInterpolationGeometryIndexedScalar(*cloud,
                                                           indices,
                                                           sqr_dists.data(),
                                                           bin_distance.data(),
                                                           central,
                                                           frame_x,
                                                           frame_y,
                                                           frame_z,
                                                           ref_x.data(),
                                                           ref_y.data(),
                                                           ref_z.data(),
                                                           ref_distance.data(),
                                                           ref_valid.data());
  pcl_rvv_shot::computeInterpolationGeometryIndexedRVV(*cloud,
                                                       indices,
                                                       sqr_dists.data(),
                                                       bin_distance.data(),
                                                       central,
                                                       frame_x,
                                                       frame_y,
                                                       frame_z,
                                                       cand_x.data(),
                                                       cand_y.data(),
                                                       cand_z.data(),
                                                       cand_distance.data(),
                                                       cand_valid.data());

  EXPECT_EQ(ref_valid[3], 0u);
  EXPECT_EQ(ref_valid[7], 0u);
  expectNearInterpolationGeometry(ref_x,
                                  ref_y,
                                  ref_z,
                                  ref_distance,
                                  ref_valid,
                                  cand_x,
                                  cand_y,
                                  cand_z,
                                  cand_distance,
                                  cand_valid);
}

// Phase 060 只覆盖已经归一化的 LAB（颜色空间）距离算术，不覆盖 RGB2CIELAB
// LUT（查找表）或 production `std::vector::push_back` 成本。失败说明颜色 bin
// distance 的 same-chain（同构链路）证据断了。
TEST(ShotColorLabDistanceComponent, RvvMatchesScalarReferenceForNormalizedLabArrays)
{
  constexpr std::size_t kCount = 257;
  std::vector<float> l(kCount), a(kCount), b(kCount);
  makeColorLabSamples(l, a, b);

  const float l_ref = 0.42f;
  const float a_ref = -0.35f;
  const float b_ref = 0.61f;
  constexpr int kColorBins = 30;

  std::vector<double> reference(kCount);
  std::vector<double> candidate(kCount);

  pcl_rvv_shot::computeColorBinDistanceScalar(
      l.data(), a.data(), b.data(), kCount, l_ref, a_ref, b_ref, kColorBins, reference.data());
  pcl_rvv_shot::computeColorBinDistanceRVV(
      l.data(), a.data(), b.data(), kCount, l_ref, a_ref, b_ref, kColorBins, candidate.data());

  expectNearColorBins(reference, candidate);
  EXPECT_DOUBLE_EQ(reference[7], static_cast<double>(kColorBins));
  EXPECT_DOUBLE_EQ(reference[11], static_cast<double>(kColorBins));
  EXPECT_DOUBLE_EQ(reference[23], static_cast<double>(kColorBins));
}

// Phase 070 把颜色路径推进到 indexed `PointXYZRGBA` + RGB2CIELAB LUT（查找表）。
// RVV 候选允许先做 RGB byte staging（标量暂存），但 staging 成本必须计入 bench。
TEST(ShotColorRgbLutIndexedComponent, RvvMatchesScalarReferenceForIndexedColorCloud)
{
  const auto cloud = pcl_rvv_shot::makeShotColorCloud(kSide);
  const pcl::Indices indices = {12, 13, 24, 36, 48, 60, 72, 84, 96, 108, 120, 13, 60};
  constexpr int kColorBins = 30;

  std::vector<double> reference(indices.size());
  std::vector<double> candidate(indices.size());

  pcl_rvv_shot::computeColorBinDistanceIndexedRGBScalar(
      *cloud, indices, 60, kColorBins, reference.data());
  pcl_rvv_shot::computeColorBinDistanceIndexedRGBRVV(
      *cloud, indices, 60, kColorBins, candidate.data());

  expectNearColorBins(reference, candidate);
  EXPECT_DOUBLE_EQ(reference[5], 0.0);
  EXPECT_DOUBLE_EQ(reference[11], reference[1]);
  EXPECT_DOUBLE_EQ(reference[12], reference[5]);
}

// Phase 080 只覆盖 interpolation（插值）中 scatter 前的 bin-selection
// staging（bin 选择暂存）：desc_index、step_index、相邻 bin 增量和中心权重。
// 手算断言覆盖象限、内外半径、`step_index == nr_bins` 和 invalid lane。
TEST(ShotInterpolationBinSelectionComponent, RvvMatchesScalarReferenceForScalarTailStaging)
{
  const std::vector<double> x = {1.0, -0.2, -0.9, 0.0, 0.2, 0.3, 0.0, -0.5};
  const std::vector<double> y = {0.2, 0.7, -0.1, -0.4, 0.1, 0.2, 0.5, 0.0};
  const std::vector<double> z = {0.3, -0.1, 0.2, -0.2, 0.3, 0.4, 0.0, 0.1};
  const std::vector<double> distance = {0.4, 0.8, 0.6, 0.2, 0.3, 0.0, 0.5, 0.51};
  const std::vector<double> bin_distance = {
      2.25, 4.75, 0.4, 9.6, std::numeric_limits<double>::quiet_NaN(), 1.2, 3.5, 7.49};

  const std::vector<std::int32_t> expected_desc = {17, 26, 3, 4, -1, -1, 20, 31};
  const std::vector<std::int32_t> expected_step = {2, 5, 0, 10, -1, -1, 4, 7};
  const std::vector<std::int32_t> expected_adjacent = {190, 290, 34, 53, -1, -1, 223, 349};
  const std::vector<float> expected_delta = {0.25f, 0.25f, 0.4f, 0.4f, 0.0f, 0.0f, 0.5f, 0.49f};
  const std::vector<float> expected_center = {0.75f, 0.75f, 0.6f, 0.6f, 0.0f, 0.0f, 0.5f, 0.51f};
  const std::vector<std::uint8_t> expected_valid = {1u, 1u, 1u, 1u, 0u, 0u, 1u, 1u};

  std::vector<std::int32_t> ref_desc(x.size()), ref_step(x.size()), ref_adjacent(x.size());
  std::vector<std::int32_t> cand_desc(x.size()), cand_step(x.size()), cand_adjacent(x.size());
  std::vector<float> ref_delta(x.size()), ref_center(x.size());
  std::vector<float> cand_delta(x.size()), cand_center(x.size());
  std::vector<std::uint8_t> ref_valid(x.size()), cand_valid(x.size());

  pcl_rvv_shot::computeInterpolationBinSelectionScalar(x.data(),
                                                       y.data(),
                                                       z.data(),
                                                       distance.data(),
                                                       bin_distance.data(),
                                                       x.size(),
                                                       10,
                                                       0.5,
                                                       ref_desc.data(),
                                                       ref_step.data(),
                                                       ref_adjacent.data(),
                                                       ref_delta.data(),
                                                       ref_center.data(),
                                                       ref_valid.data());
  pcl_rvv_shot::computeInterpolationBinSelectionRVV(x.data(),
                                                    y.data(),
                                                    z.data(),
                                                    distance.data(),
                                                    bin_distance.data(),
                                                    x.size(),
                                                    10,
                                                    0.5,
                                                    cand_desc.data(),
                                                    cand_step.data(),
                                                    cand_adjacent.data(),
                                                    cand_delta.data(),
                                                    cand_center.data(),
                                                    cand_valid.data());

  EXPECT_EQ(ref_desc, expected_desc);
  EXPECT_EQ(ref_step, expected_step);
  EXPECT_EQ(ref_adjacent, expected_adjacent);
  expectNearBinSelection(expected_desc,
                         expected_step,
                         expected_adjacent,
                         expected_delta,
                         expected_center,
                         expected_valid,
                         ref_desc,
                         ref_step,
                         ref_adjacent,
                         ref_delta,
                         ref_center,
                         ref_valid);
  expectNearBinSelection(ref_desc,
                         ref_step,
                         ref_adjacent,
                         ref_delta,
                         ref_center,
                         ref_valid,
                         cand_desc,
                         cand_step,
                         cand_adjacent,
                         cand_delta,
                         cand_center,
                         cand_valid);
}

// Phase 030 先把 shape-bin 组件推进到 `pcl::Normal` AoS（结构数组）布局。
// 这个测试仍然只验证连续 normal cloud，不覆盖 production 的任意 indices gather。
TEST(ShotShapeBinAosComponent, RvvMatchesScalarReferenceForContiguousNormalCloud)
{
  const auto normals = makeAosShapeBinNormals();
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};

  std::vector<double> reference(normals.size());
  std::vector<double> candidate(normals.size());

  pcl_rvv_shot::computeShapeBinDistanceAoSScalar(normals, 0, normals.size(), frame_z, 10, reference.data());
  pcl_rvv_shot::computeShapeBinDistanceAoSRVV(normals, 0, normals.size(), frame_z, 10, candidate.data());

  expectNearShapeBins(reference, candidate);
}

// Phase 040 覆盖更接近 production 的 indexed gather（按索引离散加载）形态。
// 这里要求 helper 保留输出顺序、重复 index 和 NaN normal count，但仍不触发真实 PCL_WARN。
TEST(ShotShapeBinIndexedComponent, RvvMatchesScalarReferenceForIndexedNormalCloud)
{
  const auto normals = makeAosShapeBinNormals();
  const pcl::Indices indices = {5, 3, 17, 17, 0, 31, 8, 42, 3, 128, 256, 64};
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};

  std::vector<double> reference(indices.size());
  std::vector<double> candidate(indices.size());

  const unsigned reference_nan_count = pcl_rvv_shot::computeShapeBinDistanceIndexedScalar(
      normals, indices, frame_z, 10, reference.data());
  const unsigned candidate_nan_count = pcl_rvv_shot::computeShapeBinDistanceIndexedRVV(
      normals, indices, frame_z, 10, candidate.data());

  EXPECT_EQ(reference_nan_count, 2u);
  EXPECT_EQ(candidate_nan_count, reference_nan_count);
  expectNearShapeBins(reference, candidate);
}

// PI2 production-detail（生产细节直连）测试：通过派生类调用真实
// `createBinDistanceShape`，而不是 test-only helper。它覆盖 indexed gather、
// clamp、重复 index 和 NaN normal count 触发的 warning 侧效应边界。
TEST(ShotShapeBinProductionDetail, CreateBinDistanceShapeMatchesReferenceForIndexedNormalCloud)
{
  const auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>>(makeAosShapeBinNormals());
  const pcl::Indices indices = {5, 3, 17, 17, 0, 31, 8, 42, 3, 128, 256, 64,
                                5, 6, 7,  8,  9, 10, 3, 17, 31, 42, 64, 128};
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};
  const auto frames = pcl::make_shared<pcl::PointCloud<pcl::ReferenceFrame>>();
  frames->resize(1);
  (*frames)[0].z_axis[0] = frame_z[0];
  (*frames)[0].z_axis[1] = frame_z[1];
  (*frames)[0].z_axis[2] = frame_z[2];

  ShotShapeBinDirectProbe<pcl::PointXYZ, pcl::Normal, pcl::SHOT352> shot;
  shot.setInputNormals(normals);
  shot.setInputReferenceFrames(frames);

  std::vector<double> reference(indices.size());
  std::vector<double> candidate;
  const unsigned reference_nan_count = computeShapeBinDistanceIndexedScalarGeneric(
      *normals, indices, frame_z, 10, reference.data());
  shot.createBinDistanceShape(0, indices, candidate);

  EXPECT_EQ(reference_nan_count, 3u);
  ASSERT_EQ(candidate.size(), reference.size());
  expectNearShapeBins(reference, candidate);
}

// 这个 case 证明 PI2 不是 exact `pcl::Normal` 门控：`PointNormal` 也通过
// normal traits（字段特征）和 AoS layout（结构数组布局）进入同一真实 helper。
TEST(ShotShapeBinProductionDetail, CreateBinDistanceShapeMatchesReferenceForPointNormalLayout)
{
  const auto normals =
      pcl::make_shared<pcl::PointCloud<pcl::PointNormal>>(makeNormalLikeShapeBinCloud<pcl::PointNormal>());
  const pcl::Indices indices = {5, 6, 7, 8, 9, 10, 3, 17, 31, 42, 64, 128,
                                256, 128, 64, 42, 31, 17, 3, 10, 9, 8, 7, 6};
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};
  const auto frames = pcl::make_shared<pcl::PointCloud<pcl::ReferenceFrame>>();
  frames->resize(1);
  (*frames)[0].z_axis[0] = frame_z[0];
  (*frames)[0].z_axis[1] = frame_z[1];
  (*frames)[0].z_axis[2] = frame_z[2];

  ShotShapeBinDirectProbe<pcl::PointXYZ, pcl::PointNormal, pcl::SHOT352> shot;
  shot.setInputNormals(normals);
  shot.setInputReferenceFrames(frames);

  std::vector<double> reference(indices.size());
  std::vector<double> candidate;
  const unsigned reference_nan_count = computeShapeBinDistanceIndexedScalarGeneric(
      *normals, indices, frame_z, 10, reference.data());
  shot.createBinDistanceShape(0, indices, candidate);

  EXPECT_EQ(reference_nan_count, 2u);
  ASSERT_EQ(candidate.size(), reference.size());
  expectNearShapeBins(reference, candidate);
}

// 小邻域走标量 fallback（回退路径），用于约束 RVV setup 成本较高时的生产 gate。
TEST(ShotShapeBinProductionDetail, SmallNeighborhoodKeepsScalarResult)
{
  const auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>>(makeAosShapeBinNormals());
  const pcl::Indices indices = {5, 0, 31};
  const float frame_z[3] = {0.11f, -0.07f, 1.0f};
  const auto frames = pcl::make_shared<pcl::PointCloud<pcl::ReferenceFrame>>();
  frames->resize(1);
  (*frames)[0].z_axis[0] = frame_z[0];
  (*frames)[0].z_axis[1] = frame_z[1];
  (*frames)[0].z_axis[2] = frame_z[2];

  ShotShapeBinDirectProbe<pcl::PointXYZ, pcl::Normal, pcl::SHOT352> shot;
  shot.setInputNormals(normals);
  shot.setInputReferenceFrames(frames);

  std::vector<double> reference(indices.size());
  std::vector<double> candidate;
  computeShapeBinDistanceIndexedScalarGeneric(*normals, indices, frame_z, 10, reference.data());
  shot.createBinDistanceShape(0, indices, candidate);

  expectNearShapeBins(reference, candidate);
}
