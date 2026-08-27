#include <pcl/test/gtest.h>

#include <grabcut_diagnostic.h>
#include <pcl/segmentation/grabcut_segmentation.h>

#include <cmath>
#include <memory>
#include <vector>

namespace {

using grabcut_diag::Color;
using pcl::segmentation::grabcut::SegmentationBackground;
using pcl::segmentation::grabcut::SegmentationForeground;
using pcl::segmentation::grabcut::TrimapBackground;
using pcl::segmentation::grabcut::TrimapForeground;
using pcl::segmentation::grabcut::TrimapUnknown;

class GrabCutTestAccess : public pcl::GrabCut<pcl::PointXYZRGB> {
public:
  using pcl::GrabCut<pcl::PointXYZRGB>::GrabCut;

  void
  setPreparedState(const std::vector<Color>& colors,
                   const std::vector<int>& indices,
                   const std::vector<pcl::segmentation::grabcut::TrimapValue>& trimap,
                   const grabcut_diag::MixtureGMM& background_gmm,
                   const grabcut_diag::MixtureGMM& foreground_gmm)
  {
    image_.reset(new pcl::segmentation::grabcut::Image);
    image_->resize(colors.size());
    image_->width = static_cast<std::uint32_t>(colors.size());
    image_->height = 1;
    for (std::size_t i = 0; i < colors.size(); ++i) {
      (*image_)[i].r = colors[i].r;
      (*image_)[i].g = colors[i].g;
      (*image_)[i].b = colors[i].b;
    }

    indices_.reset(new pcl::Indices(indices.begin(), indices.end()));
    trimap_ = trimap;
    hard_segmentation_.assign(colors.size(), SegmentationForeground);
    n_links_.assign(colors.size(), NLinks{});
    graph_nodes_.clear();

    background_GMM_.resize(background_gmm.components.size());
    foreground_GMM_.resize(foreground_gmm.components.size());
    copyGMM(background_gmm, background_GMM_);
    copyGMM(foreground_gmm, foreground_GMM_);
    computeL();
  }

  void
  runInitGraph()
  {
    initGraph();
  }

  double
  sourceCapacity(std::size_t i) const
  {
    return graph_.getSourceEdgeCapacity(graph_nodes_[i]);
  }

  double
  targetCapacity(std::size_t i) const
  {
    return graph_.getTargetEdgeCapacity(graph_nodes_[i]);
  }

#ifdef __RVV10__
  bool
  runTerminalWeightsRVVForTest()
  {
    return initGraphTerminalWeightsRVV();
  }
#endif

private:
  static void
  copyGMM(const grabcut_diag::MixtureGMM& source, pcl::segmentation::grabcut::GMM& dest)
  {
    for (std::size_t i = 0; i < source.components.size(); ++i) {
      auto& out = dest[i];
      const auto& in = source.components[i];
      out.mu.r = in.mu.r;
      out.mu.g = in.mu.g;
      out.mu.b = in.mu.b;
      out.determinant = in.determinant;
      out.pi = in.pi;
      for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
          out.inverse(row, col) = in.inverse(row, col);
        }
      }
    }
  }
};

std::vector<Color>
makeSmallImage()
{
  return {
      {0.00f, 0.00f, 0.00f},
      {0.20f, 0.00f, 0.00f},
      {0.40f, 0.10f, 0.00f},
      {0.00f, 0.30f, 0.10f},
      {0.20f, 0.30f, 0.20f},
      {0.40f, 0.40f, 0.30f},
  };
}

void
copyDiagnosticGMMToProduction(const grabcut_diag::MixtureGMM& source,
                              pcl::segmentation::grabcut::GMM& dest)
{
  dest.resize(source.components.size());
  for (std::size_t i = 0; i < source.components.size(); ++i) {
    auto& out = dest[i];
    const auto& in = source.components[i];
    out.mu.r = in.mu.r;
    out.mu.g = in.mu.g;
    out.mu.b = in.mu.b;
    out.determinant = in.determinant;
    out.pi = in.pi;
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t col = 0; col < 3; ++col) {
        out.inverse(row, col) = in.inverse(row, col);
      }
    }
  }
}

pcl::segmentation::grabcut::Image
makeProductionImage(const std::vector<Color>& colors)
{
  pcl::segmentation::grabcut::Image image;
  image.resize(colors.size());
  image.width = static_cast<std::uint32_t>(colors.size());
  image.height = 1;
  for (std::size_t i = 0; i < colors.size(); ++i) {
    image[i].r = colors[i].r;
    image[i].g = colors[i].g;
    image[i].b = colors[i].b;
  }
  return image;
}

std::vector<pcl::segmentation::grabcut::SegmentationValue>
makeHardSegmentation(const std::vector<unsigned char>& foreground_mask)
{
  std::vector<pcl::segmentation::grabcut::SegmentationValue> hard_segmentation;
  hard_segmentation.reserve(foreground_mask.size());
  for (const auto value : foreground_mask) {
    hard_segmentation.push_back(value ? SegmentationForeground : SegmentationBackground);
  }
  return hard_segmentation;
}

double
expectedGraphSourceCapacity(float source_capacity, float target_capacity)
{
  double result = 0.0;
  if (source_capacity >= 0.0f) {
    result += source_capacity;
  }
  if (target_capacity < 0.0f) {
    result -= target_capacity;
  }
  return result;
}

double
expectedGraphTargetCapacity(float source_capacity, float target_capacity)
{
  double result = 0.0;
  if (source_capacity < 0.0f) {
    result -= source_capacity;
  }
  if (target_capacity >= 0.0f) {
    result += target_capacity;
  }
  return result;
}

} // namespace

// 这个测试先锁定 organized n-link（规则图像邻接边）的标量语义：
// 小图包含 interior、右边界和下边界像素，能同时覆盖四个方向的存在/缺失条件。
// 失败时说明后续 RVV candidate（RVV 候选实现）没有可靠 reference（参考链路）。
TEST(GrabCutDiagnosticReference, ComputesOrganizedNLinksForSmallImage)
{
  const auto image = makeSmallImage();
  const float lambda = 50.0f;

  const auto result = grabcut_diag::computeOrganizedNLinksReference(image, 3, 2, lambda);

  ASSERT_EQ(result.links.size(), image.size());
  EXPECT_EQ(result.links[0].indices, (std::vector<int>{-1, 3, 4, 1}));
  EXPECT_EQ(result.links[0].valid_count, 3);
  EXPECT_EQ(result.links[2].indices, (std::vector<int>{4, 5, -1, -1}));
  EXPECT_EQ(result.links[2].valid_count, 2);
  EXPECT_EQ(result.links[3].indices, (std::vector<int>{-1, -1, -1, 4}));
  EXPECT_EQ(result.links[3].valid_count, 1);

  const float expected_edge_sum =
      grabcut_diag::squaredColorDistance(image[0], image[3]) +
      grabcut_diag::squaredColorDistance(image[0], image[4]) +
      grabcut_diag::squaredColorDistance(image[0], image[1]) +
      grabcut_diag::squaredColorDistance(image[1], image[3]) +
      grabcut_diag::squaredColorDistance(image[1], image[4]) +
      grabcut_diag::squaredColorDistance(image[1], image[5]) +
      grabcut_diag::squaredColorDistance(image[1], image[2]) +
      grabcut_diag::squaredColorDistance(image[2], image[4]) +
      grabcut_diag::squaredColorDistance(image[2], image[5]) +
      grabcut_diag::squaredColorDistance(image[3], image[4]) +
      grabcut_diag::squaredColorDistance(image[4], image[5]);
  const float expected_beta = 100000.0f / (2.0f * expected_edge_sum / 11.0f);
  EXPECT_NEAR(result.beta, expected_beta, expected_beta * 1e-6f);

  const float right_color_distance = grabcut_diag::squaredColorDistance(image[0], image[1]);
  const float expected_weight =
      lambda * std::exp(-expected_beta * right_color_distance) / 1.0f;
  ASSERT_EQ(result.links[0].weights.size(), 4);
  EXPECT_NEAR(result.links[0].weights[3], expected_weight, expected_weight * 1e-5f + 1e-6f);
}

// 这个测试覆盖 `segmentation/src/grabcut_segmentation.cpp` 中
// `GMM::probabilityDensity(i, c)` 的 per-Gaussian（单个高斯分量）公式。
// Std 构建使用标量 same-chain（同构链路），RVV 构建会命中测试专用 RVV batch
// candidate；它只证明公式 helper 的数值边界，不证明真实 production dispatch。
TEST(GrabCutDiagnosticReference, GMMProbabilityBatchMatchesScalarReference)
{
  grabcut_diag::Gaussian gaussian;
  gaussian.mu = {0.35f, 0.45f, 0.25f};
  gaussian.determinant = 0.21875f;
  gaussian.inverse = {
      2.10f, 0.12f, 0.05f,
      0.12f, 1.80f, 0.08f,
      0.05f, 0.08f, 2.40f,
  };

  const std::vector<Color> samples = {
      {0.35f, 0.45f, 0.25f},
      {0.10f, 0.20f, 0.30f},
      {0.85f, 0.70f, 0.45f},
      {0.25f, 0.95f, 0.15f},
      {0.50f, 0.30f, 0.90f},
      {0.02f, 0.80f, 0.75f},
      {0.65f, 0.55f, 0.05f},
  };

  const auto expected = grabcut_diag::computeGMMProbabilityReference(gaussian, samples);
  const auto actual = grabcut_diag::computeGMMProbabilityCandidate(gaussian, samples);

  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const float tolerance = std::max(2e-5f, std::abs(expected[i]) * 2e-4f);
    EXPECT_NEAR(actual[i], expected[i], tolerance) << "sample " << i;
  }
}

// 这个测试把 Phase 010 的单 Gaussian（高斯分量）公式推进到
// `initGraph` 的 unknown trimap（未知三分图标记）端点权重形态：
// background GMM 生成 source/foreground cost，foreground GMM 生成 sink/background cost。
// 它仍是 public-shaped diagnostic（公开入口形态诊断），不证明 graph mutation（图边写入）。
TEST(GrabCutDiagnosticReference, TerminalWeightsMatchScalarReference)
{
  grabcut_diag::MixtureGMM background_gmm;
  background_gmm.components = {
      {{0.15f, 0.25f, 0.20f}, {2.00f, 0.10f, 0.04f, 0.10f, 1.70f, 0.06f, 0.04f, 0.06f, 2.30f}, 0.310f, 0.38f},
      {{0.35f, 0.45f, 0.30f}, {1.80f, 0.08f, 0.03f, 0.08f, 1.55f, 0.05f, 0.03f, 0.05f, 2.05f}, 0.420f, 0.27f},
      {{0.55f, 0.20f, 0.55f}, {2.20f, 0.07f, 0.02f, 0.07f, 1.90f, 0.04f, 0.02f, 0.04f, 1.85f}, 0.360f, 0.18f},
      {{0.70f, 0.65f, 0.40f}, {1.65f, 0.06f, 0.01f, 0.06f, 1.80f, 0.03f, 0.01f, 0.03f, 2.10f}, 0.390f, 0.11f},
      {{0.25f, 0.75f, 0.70f}, {2.35f, 0.05f, 0.02f, 0.05f, 1.60f, 0.02f, 0.02f, 0.02f, 1.95f}, 0.280f, 0.06f},
  };
  grabcut_diag::MixtureGMM foreground_gmm;
  foreground_gmm.components = {
      {{0.75f, 0.25f, 0.18f}, {1.75f, 0.09f, 0.03f, 0.09f, 2.10f, 0.05f, 0.03f, 0.05f, 1.80f}, 0.330f, 0.34f},
      {{0.60f, 0.50f, 0.32f}, {2.05f, 0.11f, 0.04f, 0.11f, 1.85f, 0.07f, 0.04f, 0.07f, 2.20f}, 0.450f, 0.26f},
      {{0.40f, 0.70f, 0.52f}, {1.95f, 0.04f, 0.02f, 0.04f, 2.25f, 0.06f, 0.02f, 0.06f, 1.90f}, 0.370f, 0.19f},
      {{0.20f, 0.55f, 0.78f}, {2.40f, 0.03f, 0.01f, 0.03f, 1.70f, 0.04f, 0.01f, 0.04f, 2.05f}, 0.290f, 0.13f},
      {{0.85f, 0.80f, 0.68f}, {1.60f, 0.05f, 0.02f, 0.05f, 1.95f, 0.03f, 0.02f, 0.03f, 2.30f}, 0.410f, 0.08f},
  };

  const std::vector<Color> samples = {
      {0.12f, 0.20f, 0.25f},
      {0.35f, 0.42f, 0.38f},
      {0.50f, 0.30f, 0.70f},
      {0.72f, 0.60f, 0.45f},
      {0.88f, 0.75f, 0.66f},
      {0.22f, 0.82f, 0.72f},
  };

  const auto expected =
      grabcut_diag::computeTerminalWeightsReference(background_gmm, foreground_gmm, samples);
  const auto actual =
      grabcut_diag::computeTerminalWeightsCandidate(background_gmm, foreground_gmm, samples);

  ASSERT_EQ(actual.foreground_costs.size(), expected.foreground_costs.size());
  ASSERT_EQ(actual.background_costs.size(), expected.background_costs.size());
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const float fore_tolerance =
        std::max(5e-5f, std::abs(expected.foreground_costs[i]) * 3e-4f);
    const float back_tolerance =
        std::max(5e-5f, std::abs(expected.background_costs[i]) * 3e-4f);
    EXPECT_NEAR(actual.foreground_costs[i], expected.foreground_costs[i], fore_tolerance)
        << "foreground sample " << i;
    EXPECT_NEAR(actual.background_costs[i], expected.background_costs[i], back_tolerance)
        << "background sample " << i;
  }
}

// 这个测试给 Phase 030 的 no-solve diagnostic（不含求解诊断）定边界：
// 在 terminal weight 公式后追加一个轻量 graph node / terminal write sink。
// 它不模拟真实 Boykov-Kolmogorov 图结构，只检查 RVV candidate 穿过写入外壳后仍保持数值语义。
TEST(GrabCutDiagnosticReference, InitGraphNoSolveMatchesScalarReference)
{
  grabcut_diag::MixtureGMM background_gmm;
  background_gmm.components = {
      {{0.15f, 0.25f, 0.20f}, {2.00f, 0.10f, 0.04f, 0.10f, 1.70f, 0.06f, 0.04f, 0.06f, 2.30f}, 0.310f, 0.38f},
      {{0.35f, 0.45f, 0.30f}, {1.80f, 0.08f, 0.03f, 0.08f, 1.55f, 0.05f, 0.03f, 0.05f, 2.05f}, 0.420f, 0.27f},
      {{0.55f, 0.20f, 0.55f}, {2.20f, 0.07f, 0.02f, 0.07f, 1.90f, 0.04f, 0.02f, 0.04f, 1.85f}, 0.360f, 0.18f},
      {{0.70f, 0.65f, 0.40f}, {1.65f, 0.06f, 0.01f, 0.06f, 1.80f, 0.03f, 0.01f, 0.03f, 2.10f}, 0.390f, 0.11f},
      {{0.25f, 0.75f, 0.70f}, {2.35f, 0.05f, 0.02f, 0.05f, 1.60f, 0.02f, 0.02f, 0.02f, 1.95f}, 0.280f, 0.06f},
  };
  grabcut_diag::MixtureGMM foreground_gmm;
  foreground_gmm.components = {
      {{0.75f, 0.25f, 0.18f}, {1.75f, 0.09f, 0.03f, 0.09f, 2.10f, 0.05f, 0.03f, 0.05f, 1.80f}, 0.330f, 0.34f},
      {{0.60f, 0.50f, 0.32f}, {2.05f, 0.11f, 0.04f, 0.11f, 1.85f, 0.07f, 0.04f, 0.07f, 2.20f}, 0.450f, 0.26f},
      {{0.40f, 0.70f, 0.52f}, {1.95f, 0.04f, 0.02f, 0.04f, 2.25f, 0.06f, 0.02f, 0.06f, 1.90f}, 0.370f, 0.19f},
      {{0.20f, 0.55f, 0.78f}, {2.40f, 0.03f, 0.01f, 0.03f, 1.70f, 0.04f, 0.01f, 0.04f, 2.05f}, 0.290f, 0.13f},
      {{0.85f, 0.80f, 0.68f}, {1.60f, 0.05f, 0.02f, 0.05f, 1.95f, 0.03f, 0.02f, 0.03f, 2.30f}, 0.410f, 0.08f},
  };
  const std::vector<Color> samples = makeSmallImage();

  const auto expected =
      grabcut_diag::computeInitGraphNoSolveReference(background_gmm, foreground_gmm, samples);
  const auto actual =
      grabcut_diag::computeInitGraphNoSolveCandidate(background_gmm, foreground_gmm, samples);

  ASSERT_EQ(actual.terminal.foreground_costs.size(), expected.terminal.foreground_costs.size());
  ASSERT_EQ(actual.terminal.background_costs.size(), expected.terminal.background_costs.size());
  EXPECT_NEAR(actual.sink_checksum, expected.sink_checksum, 1.0e-4f);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const float fore_tolerance =
        std::max(5e-5f, std::abs(expected.terminal.foreground_costs[i]) * 3e-4f);
    const float back_tolerance =
        std::max(5e-5f, std::abs(expected.terminal.background_costs[i]) * 3e-4f);
    EXPECT_NEAR(actual.terminal.foreground_costs[i],
                expected.terminal.foreground_costs[i],
                fore_tolerance)
        << "foreground sample " << i;
    EXPECT_NEAR(actual.terminal.background_costs[i],
                expected.terminal.background_costs[i],
                back_tolerance)
        << "background sample " << i;
  }
}

// 这个测试覆盖 `learnGMMs()` 第一段 component assignment（分量归属选择）语义：
// 每个样本只在 foreground 或 background GMM（前景/背景高斯混合模型）中选最大概率分量。
// RVV 构建下 candidate 会批量计算概率；Std 构建下它退回同构标量链路。
TEST(GrabCutDiagnosticReference, LearnGMMComponentAssignmentMatchesScalarReference)
{
  const grabcut_diag::Matrix3f identity = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f,
  };
  grabcut_diag::MixtureGMM background_gmm;
  background_gmm.components = {
      {{0.05f, 0.05f, 0.05f}, identity, 1.0f, 1.0f},
      {{0.90f, 0.90f, 0.90f}, identity, 1.0f, 1.0f},
      {{0.20f, 0.70f, 0.25f}, identity, 1.0f, 1.0f},
  };
  grabcut_diag::MixtureGMM foreground_gmm;
  foreground_gmm.components = {
      {{0.85f, 0.10f, 0.10f}, identity, 1.0f, 1.0f},
      {{0.10f, 0.85f, 0.10f}, identity, 1.0f, 1.0f},
      {{0.10f, 0.10f, 0.85f}, identity, 1.0f, 1.0f},
  };
  const std::vector<Color> samples = {
      {0.04f, 0.06f, 0.07f},
      {0.88f, 0.87f, 0.93f},
      {0.22f, 0.68f, 0.20f},
      {0.82f, 0.12f, 0.08f},
      {0.12f, 0.82f, 0.12f},
      {0.12f, 0.08f, 0.83f},
  };
  const std::vector<unsigned char> foreground_mask = {0, 0, 0, 1, 1, 1};

  const auto expected = grabcut_diag::assignGMMComponentsReference(
      background_gmm, foreground_gmm, samples, foreground_mask);
  const auto actual = grabcut_diag::assignGMMComponentsCandidate(
      background_gmm, foreground_gmm, samples, foreground_mask);

  EXPECT_EQ(expected, (std::vector<std::size_t>{0, 1, 2, 0, 1, 2}));
  EXPECT_EQ(actual, expected);
}

// 这个测试把 Phase 090 的 assignment 子阶段放回完整 `learnGMMs()` 局部流程中：
// candidate 先用 RVV 选择分量，再按 production 同形态的标量 fitter 重新训练 GMM。
// 若 components 或更新后的 GMM 参数偏离 reference，说明 Phase 100 的 full diagnostic 不能成立。
TEST(GrabCutDiagnosticReference, LearnGMMsFullMatchesScalarReference)
{
  const grabcut_diag::Matrix3f identity = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f,
  };
  grabcut_diag::MixtureGMM background_gmm;
  background_gmm.components = {
      {{0.05f, 0.05f, 0.05f}, identity, 1.0f, 1.0f},
      {{0.90f, 0.90f, 0.90f}, identity, 1.0f, 1.0f},
      {{0.20f, 0.70f, 0.25f}, identity, 1.0f, 1.0f},
  };
  grabcut_diag::MixtureGMM foreground_gmm;
  foreground_gmm.components = {
      {{0.85f, 0.10f, 0.10f}, identity, 1.0f, 1.0f},
      {{0.10f, 0.85f, 0.10f}, identity, 1.0f, 1.0f},
      {{0.10f, 0.10f, 0.85f}, identity, 1.0f, 1.0f},
  };
  const std::vector<Color> samples = {
      {0.04f, 0.06f, 0.07f}, {0.06f, 0.05f, 0.03f},
      {0.88f, 0.87f, 0.93f}, {0.92f, 0.91f, 0.88f},
      {0.22f, 0.68f, 0.20f}, {0.19f, 0.73f, 0.26f},
      {0.82f, 0.12f, 0.08f}, {0.86f, 0.09f, 0.12f},
      {0.12f, 0.82f, 0.12f}, {0.09f, 0.87f, 0.08f},
      {0.12f, 0.08f, 0.83f}, {0.08f, 0.12f, 0.88f},
  };
  const std::vector<unsigned char> foreground_mask = {
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1,
  };

  const auto expected =
      grabcut_diag::learnGMMsReference(background_gmm, foreground_gmm, samples, foreground_mask);
  const auto actual =
      grabcut_diag::learnGMMsCandidate(background_gmm, foreground_gmm, samples, foreground_mask);

  EXPECT_EQ(actual.components, expected.components);
  const auto expect_close_gaussian = [](const grabcut_diag::Gaussian& actual_gaussian,
                                        const grabcut_diag::Gaussian& expected_gaussian) {
    EXPECT_NEAR(actual_gaussian.mu.r, expected_gaussian.mu.r, 1.0e-6f);
    EXPECT_NEAR(actual_gaussian.mu.g, expected_gaussian.mu.g, 1.0e-6f);
    EXPECT_NEAR(actual_gaussian.mu.b, expected_gaussian.mu.b, 1.0e-6f);
    EXPECT_NEAR(actual_gaussian.determinant, expected_gaussian.determinant, 1.0e-8f);
    EXPECT_NEAR(actual_gaussian.pi, expected_gaussian.pi, 1.0e-6f);
    for (std::size_t i = 0; i < 9; ++i) {
      EXPECT_NEAR(actual_gaussian.inverse.values[i], expected_gaussian.inverse.values[i], 1.0e-3f)
          << "inverse element " << i;
    }
  };
  for (std::size_t component = 0; component < expected.background_gmm.components.size(); ++component) {
    expect_close_gaussian(
        actual.background_gmm.components[component], expected.background_gmm.components[component]);
  }
  for (std::size_t component = 0; component < expected.foreground_gmm.components.size(); ++component) {
    expect_close_gaussian(
        actual.foreground_gmm.components[component], expected.foreground_gmm.components[component]);
  }
}

// 这个 production direct（真实生产路径）测试直接调用
// `pcl::segmentation::grabcut::learnGMMs()`。Std 构建证明抽出的标量 fallback
// 保持原语义；RVV 构建在反汇编归属配合下证明真实 production helper 被接入。
TEST(GrabCutProductionDirect, LearnGMMsMatchesScalarReference)
{
  const grabcut_diag::Matrix3f identity = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f,
  };
  grabcut_diag::MixtureGMM background_gmm;
  background_gmm.components = {
      {{0.05f, 0.05f, 0.05f}, identity, 1.0f, 1.0f},
      {{0.90f, 0.90f, 0.90f}, identity, 1.0f, 1.0f},
      {{0.20f, 0.70f, 0.25f}, identity, 1.0f, 1.0f},
  };
  grabcut_diag::MixtureGMM foreground_gmm;
  foreground_gmm.components = {
      {{0.85f, 0.10f, 0.10f}, identity, 1.0f, 1.0f},
      {{0.10f, 0.85f, 0.10f}, identity, 1.0f, 1.0f},
      {{0.10f, 0.10f, 0.85f}, identity, 1.0f, 1.0f},
  };
  const std::vector<Color> samples = {
      {0.04f, 0.06f, 0.07f}, {0.06f, 0.05f, 0.03f},
      {0.88f, 0.87f, 0.93f}, {0.92f, 0.91f, 0.88f},
      {0.22f, 0.68f, 0.20f}, {0.19f, 0.73f, 0.26f},
      {0.82f, 0.12f, 0.08f}, {0.86f, 0.09f, 0.12f},
      {0.12f, 0.82f, 0.12f}, {0.09f, 0.87f, 0.08f},
      {0.12f, 0.08f, 0.83f}, {0.08f, 0.12f, 0.88f},
  };
  const std::vector<unsigned char> foreground_mask = {
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1,
  };

  auto image = makeProductionImage(samples);
  pcl::Indices indices(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) {
    indices[i] = static_cast<int>(i);
  }
  auto hard_segmentation = makeHardSegmentation(foreground_mask);
  std::vector<std::size_t> components(samples.size(), 0);
  pcl::segmentation::grabcut::GMM production_background;
  pcl::segmentation::grabcut::GMM production_foreground;
  copyDiagnosticGMMToProduction(background_gmm, production_background);
  copyDiagnosticGMMToProduction(foreground_gmm, production_foreground);

  pcl::segmentation::grabcut::learnGMMs(image,
                                        indices,
                                        hard_segmentation,
                                        components,
                                        production_background,
                                        production_foreground);

  const auto expected =
      grabcut_diag::learnGMMsReference(background_gmm, foreground_gmm, samples, foreground_mask);

  EXPECT_EQ(components, expected.components);
  const auto expect_close_gaussian = [](const pcl::segmentation::grabcut::Gaussian& actual,
                                        const grabcut_diag::Gaussian& expected_gaussian) {
    EXPECT_NEAR(actual.mu.r, expected_gaussian.mu.r, 1.0e-6f);
    EXPECT_NEAR(actual.mu.g, expected_gaussian.mu.g, 1.0e-6f);
    EXPECT_NEAR(actual.mu.b, expected_gaussian.mu.b, 1.0e-6f);
    EXPECT_NEAR(actual.determinant, expected_gaussian.determinant, 1.0e-8f);
    EXPECT_NEAR(actual.pi, expected_gaussian.pi, 1.0e-6f);
    for (std::size_t i = 0; i < 9; ++i) {
      const float inverse_tolerance =
          std::max(1.0e-3f, std::abs(expected_gaussian.inverse.values[i]) * 2.0e-3f);
      EXPECT_NEAR(actual.inverse(i / 3, i % 3),
                  expected_gaussian.inverse.values[i],
                  inverse_tolerance)
          << "inverse element " << i;
    }
  };
  for (std::size_t component = 0; component < expected.background_gmm.components.size(); ++component) {
    expect_close_gaussian(production_background[component],
                          expected.background_gmm.components[component]);
  }
  for (std::size_t component = 0; component < expected.foreground_gmm.components.size(); ++component) {
    expect_close_gaussian(production_foreground[component],
                          expected.foreground_gmm.components[component]);
  }
}

// 单样本路径覆盖小规模 fallback（回退路径）。RVV 构建下 production helper 会拒绝
// `indices.size() < 2` 并回到标量链路；Std 构建则自然只执行标量 helper。
TEST(GrabCutProductionDirect, LearnGMMsSmallInputFallbackMatchesReference)
{
  const grabcut_diag::Matrix3f identity = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f,
  };
  grabcut_diag::MixtureGMM background_gmm;
  background_gmm.components = {{{0.05f, 0.05f, 0.05f}, identity, 1.0f, 1.0f}};
  grabcut_diag::MixtureGMM foreground_gmm;
  foreground_gmm.components = {{{0.85f, 0.10f, 0.10f}, identity, 1.0f, 1.0f}};
  const std::vector<Color> samples = {{0.04f, 0.06f, 0.07f}};
  const std::vector<unsigned char> foreground_mask = {0};

  auto image = makeProductionImage(samples);
  pcl::Indices indices = {0};
  auto hard_segmentation = makeHardSegmentation(foreground_mask);
  std::vector<std::size_t> components(samples.size(), 0);
  pcl::segmentation::grabcut::GMM production_background;
  pcl::segmentation::grabcut::GMM production_foreground;
  copyDiagnosticGMMToProduction(background_gmm, production_background);
  copyDiagnosticGMMToProduction(foreground_gmm, production_foreground);

  pcl::segmentation::grabcut::learnGMMs(image,
                                        indices,
                                        hard_segmentation,
                                        components,
                                        production_background,
                                        production_foreground);

  const auto expected =
      grabcut_diag::learnGMMsReference(background_gmm, foreground_gmm, samples, foreground_mask);
  EXPECT_EQ(components, expected.components);
  EXPECT_NEAR(production_background[0].pi, expected.background_gmm.components[0].pi, 1.0e-6f);
  EXPECT_NEAR(production_background[0].mu.r, expected.background_gmm.components[0].mu.r, 1.0e-6f);
}

#ifdef __RVV10__
// 这个 production direct（真实生产路径）测试要求 `initGraph()` 的 unknown terminal
// branch（未知端点分支）拥有可命中的 RVV helper。若生产代码删掉 RVV 分流、规模 gate
// 错误或 helper 没有写入真实 graph terminal capacities（图端点容量），本测试会失败。
TEST(GrabCutProductionDirect, RvvTerminalWeightsMatchScalarAndFixedLabelsFallback)
{
  grabcut_diag::MixtureGMM background_gmm;
  background_gmm.components = {
      {{0.15f, 0.25f, 0.20f}, {2.00f, 0.10f, 0.04f, 0.10f, 1.70f, 0.06f, 0.04f, 0.06f, 2.30f}, 0.310f, 0.38f},
      {{0.35f, 0.45f, 0.30f}, {1.80f, 0.08f, 0.03f, 0.08f, 1.55f, 0.05f, 0.03f, 0.05f, 2.05f}, 0.420f, 0.27f},
      {{0.55f, 0.20f, 0.55f}, {2.20f, 0.07f, 0.02f, 0.07f, 1.90f, 0.04f, 0.02f, 0.04f, 1.85f}, 0.360f, 0.18f},
      {{0.70f, 0.65f, 0.40f}, {1.65f, 0.06f, 0.01f, 0.06f, 1.80f, 0.03f, 0.01f, 0.03f, 2.10f}, 0.390f, 0.11f},
      {{0.25f, 0.75f, 0.70f}, {2.35f, 0.05f, 0.02f, 0.05f, 1.60f, 0.02f, 0.02f, 0.02f, 1.95f}, 0.280f, 0.06f},
  };
  grabcut_diag::MixtureGMM foreground_gmm;
  foreground_gmm.components = {
      {{0.75f, 0.25f, 0.18f}, {1.75f, 0.09f, 0.03f, 0.09f, 2.10f, 0.05f, 0.03f, 0.05f, 1.80f}, 0.330f, 0.34f},
      {{0.60f, 0.50f, 0.32f}, {2.05f, 0.11f, 0.04f, 0.11f, 1.85f, 0.07f, 0.04f, 0.07f, 2.20f}, 0.450f, 0.26f},
      {{0.40f, 0.70f, 0.52f}, {1.95f, 0.04f, 0.02f, 0.04f, 2.25f, 0.06f, 0.02f, 0.06f, 1.90f}, 0.370f, 0.19f},
      {{0.20f, 0.55f, 0.78f}, {2.40f, 0.03f, 0.01f, 0.03f, 1.70f, 0.04f, 0.01f, 0.04f, 2.05f}, 0.290f, 0.13f},
      {{0.85f, 0.80f, 0.68f}, {1.60f, 0.05f, 0.02f, 0.05f, 1.95f, 0.03f, 0.02f, 0.03f, 2.30f}, 0.410f, 0.08f},
  };
  const auto colors = makeSmallImage();
  const std::vector<int> indices = {0, 1, 2, 3, 4, 5};
  const std::vector<pcl::segmentation::grabcut::TrimapValue> trimap = {
      TrimapUnknown, TrimapBackground, TrimapUnknown, TrimapForeground, TrimapUnknown, TrimapUnknown};

  GrabCutTestAccess grabcut(5, 50.0f);
  grabcut.setPreparedState(colors, indices, trimap, background_gmm, foreground_gmm);
  grabcut.runInitGraph();

  std::vector<Color> unknown_colors = {colors[0], colors[2], colors[4], colors[5]};
  const auto expected_unknown =
      grabcut_diag::computeTerminalWeightsReference(background_gmm, foreground_gmm, unknown_colors);
  const std::vector<std::size_t> unknown_points = {0, 2, 4, 5};
  for (std::size_t i = 0; i < unknown_points.size(); ++i) {
    const std::size_t point = unknown_points[i];
    const double expected_source =
        expectedGraphSourceCapacity(expected_unknown.foreground_costs[i],
                                    expected_unknown.background_costs[i]);
    const double expected_target =
        expectedGraphTargetCapacity(expected_unknown.foreground_costs[i],
                                    expected_unknown.background_costs[i]);
    EXPECT_NEAR(grabcut.sourceCapacity(point),
                expected_source,
                std::max(5e-5, std::abs(expected_source) * 3e-4))
        << "source capacity for point " << point;
    EXPECT_NEAR(grabcut.targetCapacity(point),
                expected_target,
                std::max(5e-5, std::abs(expected_target) * 3e-4))
        << "target capacity for point " << point;
  }
  EXPECT_DOUBLE_EQ(grabcut.sourceCapacity(1), 0.0);
  EXPECT_DOUBLE_EQ(grabcut.targetCapacity(1), grabcut.getLambda() * 8.0f + 1.0f);
  EXPECT_DOUBLE_EQ(grabcut.sourceCapacity(3), grabcut.getLambda() * 8.0f + 1.0f);
  EXPECT_DOUBLE_EQ(grabcut.targetCapacity(3), 0.0);

  GrabCutTestAccess small_grabcut(5, 50.0f);
  small_grabcut.setPreparedState({colors[0]}, {0}, {TrimapUnknown}, background_gmm, foreground_gmm);
  EXPECT_FALSE(small_grabcut.runTerminalWeightsRVVForTest());
}
#endif

#ifdef __RVV10__
// 这个测试要求 RVV candidate（RVV 候选实现）在相同固定槽位语义下匹配标量 reference。
// 它只证明测试资产里的 pre-production diagnostic（接入生产前诊断）候选正确，
// 不证明 production dispatch（生产分流）已经接入。
TEST(GrabCutDiagnosticCandidate, RvvOrganizedNLinksMatchesReference)
{
  const auto image = makeSmallImage();
  const float lambda = 50.0f;

  const auto expected = grabcut_diag::computeOrganizedNLinksReference(image, 3, 2, lambda);
  const auto actual = grabcut_diag::computeOrganizedNLinksRVV(image, 3, 2, lambda);

  ASSERT_EQ(actual.links.size(), expected.links.size());
  EXPECT_NEAR(actual.beta, expected.beta, expected.beta * 1e-5f);
  for (std::size_t i = 0; i < expected.links.size(); ++i) {
    EXPECT_EQ(actual.links[i].indices, expected.links[i].indices) << "pixel=" << i;
    EXPECT_EQ(actual.links[i].valid_count, expected.links[i].valid_count) << "pixel=" << i;
    ASSERT_EQ(actual.links[i].weights.size(), expected.links[i].weights.size()) << "pixel=" << i;
    for (std::size_t slot = 0; slot < expected.links[i].weights.size(); ++slot) {
      EXPECT_NEAR(actual.links[i].weights[slot],
                  expected.links[i].weights[slot],
                  std::abs(expected.links[i].weights[slot]) * 2e-4f + 1e-5f)
          << "pixel=" << i << " slot=" << slot;
    }
  }
}
#endif
