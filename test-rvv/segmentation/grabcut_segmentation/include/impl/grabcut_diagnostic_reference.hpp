#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>

#include <riscv_vector.h>
#endif

namespace grabcut_diag {

struct Color {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
};

struct LinkSet {
  int valid_count = 0;
  std::vector<int> indices;
  std::vector<float> dists;
  std::vector<float> color_distances;
  std::vector<float> weights;
};

struct OrganizedNLinksResult {
  float beta = 0.0f;
  std::vector<LinkSet> links;
};

struct Matrix3f {
  float values[9] = {};

  float
  operator()(std::size_t row, std::size_t col) const
  {
    return values[row * 3 + col];
  }
};

struct Gaussian {
  Color mu;
  Matrix3f inverse;
  float determinant = 0.0f;
  float pi = 1.0f;
};

struct MixtureGMM {
  std::vector<Gaussian> components;
};

struct TerminalWeights {
  std::vector<float> foreground_costs;
  std::vector<float> background_costs;
};

struct InitGraphNoSolveResult {
  TerminalWeights terminal;
  float sink_checksum = 0.0f;
};

struct FullLearnGMMsResult {
  std::vector<std::size_t> components;
  MixtureGMM background_gmm;
  MixtureGMM foreground_gmm;
};

struct GaussianFitAccumulator {
  float sum[3] = {};
  float products[9] = {};
  std::uint32_t count = 0;
};

// 这个 helper 是 GrabCut organized n-link 的测试专用参考链路。
// 它按 production 中 `computeBetaOrganized` 的四个方向顺序保存邻接边，
// 再按 `computeNLinksOrganized` 的公式把 color distance（颜色距离）转为权重。
inline float
squaredColorDistance(const Color& a, const Color& b)
{
  const float dr = a.r - b.r;
  const float dg = a.g - b.g;
  const float db = a.b - b.b;
  return dr * dr + dg * dg + db * db;
}

inline void
setLink(LinkSet& links, std::size_t slot, int neighbor, float dist, float color_distance)
{
  links.indices[slot] = neighbor;
  links.dists[slot] = dist;
  links.color_distances[slot] = color_distance;
  links.weights[slot] = color_distance;
  ++links.valid_count;
}

inline OrganizedNLinksResult
computeOrganizedNLinkDistancesReference(const std::vector<Color>& image,
                                        std::uint32_t width,
                                        std::uint32_t height)
{
  const std::size_t expected_size = static_cast<std::size_t>(width) * height;
  if (width == 0 || height == 0 || image.size() != expected_size) {
    throw std::invalid_argument("image size must match width * height");
  }

  OrganizedNLinksResult result;
  result.links.resize(expected_size);
  float edge_sum = 0.0f;
  std::size_t edges = 0;

  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const std::size_t point_index = static_cast<std::size_t>(y) * width + x;
      LinkSet& links = result.links[point_index];
      links.indices.assign(4, -1);
      links.dists.assign(4, 0.0f);
      links.color_distances.assign(4, 0.0f);
      links.weights.assign(4, 0.0f);

      if (x > 0 && y < height - 1) {
        const std::size_t upleft = static_cast<std::size_t>(y + 1) * width + x - 1;
        const float color_dist = squaredColorDistance(image[point_index], image[upleft]);
        setLink(links, 0, static_cast<int>(upleft), std::sqrt(2.0f), color_dist);
        edge_sum += color_dist;
        ++edges;
      }

      if (y < height - 1) {
        const std::size_t up = static_cast<std::size_t>(y + 1) * width + x;
        const float color_dist = squaredColorDistance(image[point_index], image[up]);
        setLink(links, 1, static_cast<int>(up), 1.0f, color_dist);
        edge_sum += color_dist;
        ++edges;
      }

      if (x < width - 1 && y < height - 1) {
        const std::size_t upright = static_cast<std::size_t>(y + 1) * width + x + 1;
        const float color_dist = squaredColorDistance(image[point_index], image[upright]);
        setLink(links, 2, static_cast<int>(upright), std::sqrt(2.0f), color_dist);
        edge_sum += color_dist;
        ++edges;
      }

      if (x < width - 1) {
        const std::size_t right = static_cast<std::size_t>(y) * width + x + 1;
        const float color_dist = squaredColorDistance(image[point_index], image[right]);
        setLink(links, 3, static_cast<int>(right), 1.0f, color_dist);
        edge_sum += color_dist;
        ++edges;
      }
    }
  }

  result.beta = edges == 0 ? 0.0f : 100000.0f / (2.0f * edge_sum / static_cast<float>(edges));

  return result;
}

inline OrganizedNLinksResult
computeOrganizedNLinksReference(const std::vector<Color>& image,
                                std::uint32_t width,
                                std::uint32_t height,
                                float lambda)
{
  OrganizedNLinksResult result = computeOrganizedNLinkDistancesReference(image, width, height);

  for (auto& links : result.links) {
    for (std::size_t i = 0; i < links.weights.size(); ++i) {
      if (links.indices[i] != -1) {
        links.weights[i] = lambda * std::exp(-result.beta * links.color_distances[i]) / links.dists[i];
      }
    }
  }

  return result;
}

// 这个 helper 复刻 production `GMM::probabilityDensity(i, c)` 的单个 Gaussian（高斯分量）公式。
// 它只覆盖 determinant > 0 且输入有限的诊断样本；`pi` 加权求和、GMM fitting（高斯混合模型拟合）
// 和 max-flow（最大流）状态机不在本 helper 的证据范围内。
inline float
computeGMMProbabilityReferenceOne(const Gaussian& gaussian, const Color& color)
{
  if (gaussian.pi <= 0.0f || gaussian.determinant <= 0.0f) {
    return 0.0f;
  }

  const float r = color.r - gaussian.mu.r;
  const float g = color.g - gaussian.mu.g;
  const float b = color.b - gaussian.mu.b;
  const float d =
      r * (r * gaussian.inverse(0, 0) + g * gaussian.inverse(1, 0) +
           b * gaussian.inverse(2, 0)) +
      g * (r * gaussian.inverse(0, 1) + g * gaussian.inverse(1, 1) +
           b * gaussian.inverse(2, 1)) +
      b * (r * gaussian.inverse(0, 2) + g * gaussian.inverse(1, 2) +
           b * gaussian.inverse(2, 2));

  return static_cast<float>(
      1.0 / std::sqrt(gaussian.determinant) * std::exp(-0.5f * d));
}

inline std::vector<float>
computeGMMProbabilityReference(const Gaussian& gaussian, const std::vector<Color>& colors)
{
  std::vector<float> result;
  result.reserve(colors.size());
  for (const auto& color : colors) {
    result.push_back(computeGMMProbabilityReferenceOne(gaussian, color));
  }
  return result;
}

inline std::vector<float>
computeMixtureProbabilityReference(const MixtureGMM& gmm, const std::vector<Color>& colors)
{
  std::vector<float> result(colors.size(), 0.0f);
  for (const auto& gaussian : gmm.components) {
    for (std::size_t i = 0; i < colors.size(); ++i) {
      result[i] += gaussian.pi * computeGMMProbabilityReferenceOne(gaussian, colors[i]);
    }
  }
  return result;
}

inline TerminalWeights
computeTerminalWeightsReference(const MixtureGMM& background_gmm,
                                const MixtureGMM& foreground_gmm,
                                const std::vector<Color>& colors)
{
  TerminalWeights result;
  result.foreground_costs.resize(colors.size());
  result.background_costs.resize(colors.size());
  const auto background_probability = computeMixtureProbabilityReference(background_gmm, colors);
  const auto foreground_probability = computeMixtureProbabilityReference(foreground_gmm, colors);
  for (std::size_t i = 0; i < colors.size(); ++i) {
    result.foreground_costs[i] = -std::log(background_probability[i]);
    result.background_costs[i] = -std::log(foreground_probability[i]);
  }
  return result;
}

inline float
computeTerminalWriteSink(const TerminalWeights& terminal)
{
  float checksum = 0.0f;
  for (std::size_t i = 0; i < terminal.foreground_costs.size(); ++i) {
    const float node_weight = static_cast<float>(i + 1);
    checksum += terminal.foreground_costs[i] * node_weight * 0.125f;
    checksum += terminal.background_costs[i] * (node_weight + 2.0f) * 0.0625f;
  }
  return checksum;
}

inline InitGraphNoSolveResult
computeInitGraphNoSolveReference(const MixtureGMM& background_gmm,
                                 const MixtureGMM& foreground_gmm,
                                 const std::vector<Color>& colors)
{
  InitGraphNoSolveResult result;
  result.terminal = computeTerminalWeightsReference(background_gmm, foreground_gmm, colors);
  result.sink_checksum = computeTerminalWriteSink(result.terminal);
  return result;
}

#if defined(__RVV10__)
// 这个 candidate 只把 n-link 权重公式中的 expf（单精度指数函数）阶段交给
// `pcl::expf_RVV_f32m2`。邻接边枚举、固定 4 槽位和 beta reduction 仍复用
// reference，这让 Phase 010 可以单独判断 RVV exp 权重阶段是否值得继续。
inline OrganizedNLinksResult
computeOrganizedNLinksRVV(const std::vector<Color>& image,
                          std::uint32_t width,
                          std::uint32_t height,
                          float lambda)
{
  OrganizedNLinksResult result = computeOrganizedNLinkDistancesReference(image, width, height);
  constexpr std::size_t kScratchLanes = 64;
  if (__riscv_vsetvlmax_e32m2() > kScratchLanes) {
    return computeOrganizedNLinksReference(image, width, height, lambda);
  }

  std::vector<float> inputs;
  std::vector<float*> outputs;
  std::vector<float> distances;
  inputs.reserve(result.links.size() * 4);
  outputs.reserve(result.links.size() * 4);
  distances.reserve(result.links.size() * 4);

  for (auto& links : result.links) {
    for (std::size_t slot = 0; slot < links.weights.size(); ++slot) {
      if (links.indices[slot] == -1) {
        continue;
      }
      inputs.push_back(-result.beta * links.color_distances[slot]);
      outputs.push_back(&links.weights[slot]);
      distances.push_back(links.dists[slot]);
    }
  }

  std::size_t offset = 0;
  while (offset < inputs.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(inputs.size() - offset);
    const vfloat32m2_t x = __riscv_vle32_v_f32m2(inputs.data() + offset, vl);
    const vfloat32m2_t exp_x = pcl::expf_RVV_f32m2(x, vl);
    float tmp[kScratchLanes];
    __riscv_vse32_v_f32m2(tmp, exp_x, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      *outputs[offset + lane] = lambda * tmp[lane] / distances[offset + lane];
    }
    offset += vl;
  }

  return result;
}
#endif

// Std 构建下 candidate（候选实现）刻意复用同构标量链路；RVV 构建下才替换
// exp 子链路。这样同一个 gtest 能同时验证 fallback 和 RVV batch 的数值预算。
inline std::vector<float>
computeGMMProbabilityCandidate(const Gaussian& gaussian, const std::vector<Color>& colors)
{
  if (gaussian.pi <= 0.0f || gaussian.determinant <= 0.0f) {
    return std::vector<float>(colors.size(), 0.0f);
  }

#if defined(__RVV10__)
  std::vector<float> result(colors.size(), 0.0f);
  const float inv_sqrt_det = 1.0f / std::sqrt(gaussian.determinant);
  std::size_t offset = 0;
  while (offset < colors.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(colors.size() - offset);
    std::vector<float> r_buf(vl);
    std::vector<float> g_buf(vl);
    std::vector<float> b_buf(vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const Color& color = colors[offset + lane];
      r_buf[lane] = color.r - gaussian.mu.r;
      g_buf[lane] = color.g - gaussian.mu.g;
      b_buf[lane] = color.b - gaussian.mu.b;
    }

    const vfloat32m2_t r = __riscv_vle32_v_f32m2(r_buf.data(), vl);
    const vfloat32m2_t g = __riscv_vle32_v_f32m2(g_buf.data(), vl);
    const vfloat32m2_t b = __riscv_vle32_v_f32m2(b_buf.data(), vl);

    vfloat32m2_t row0 = __riscv_vfmul_vf_f32m2(r, gaussian.inverse(0, 0), vl);
    row0 = __riscv_vfmacc_vf_f32m2(row0, gaussian.inverse(1, 0), g, vl);
    row0 = __riscv_vfmacc_vf_f32m2(row0, gaussian.inverse(2, 0), b, vl);

    vfloat32m2_t row1 = __riscv_vfmul_vf_f32m2(r, gaussian.inverse(0, 1), vl);
    row1 = __riscv_vfmacc_vf_f32m2(row1, gaussian.inverse(1, 1), g, vl);
    row1 = __riscv_vfmacc_vf_f32m2(row1, gaussian.inverse(2, 1), b, vl);

    vfloat32m2_t row2 = __riscv_vfmul_vf_f32m2(r, gaussian.inverse(0, 2), vl);
    row2 = __riscv_vfmacc_vf_f32m2(row2, gaussian.inverse(1, 2), g, vl);
    row2 = __riscv_vfmacc_vf_f32m2(row2, gaussian.inverse(2, 2), b, vl);

    vfloat32m2_t d = __riscv_vfmul_vv_f32m2(r, row0, vl);
    d = __riscv_vfmacc_vv_f32m2(d, g, row1, vl);
    d = __riscv_vfmacc_vv_f32m2(d, b, row2, vl);
    const vfloat32m2_t exponent = __riscv_vfmul_vf_f32m2(d, -0.5f, vl);
    const vfloat32m2_t exp_value = pcl::expf_RVV_f32m2(exponent, vl);
    const vfloat32m2_t value = __riscv_vfmul_vf_f32m2(exp_value, inv_sqrt_det, vl);
    __riscv_vse32_v_f32m2(result.data() + offset, value, vl);
    offset += vl;
  }
  return result;
#else
  return computeGMMProbabilityReference(gaussian, colors);
#endif
}

inline std::vector<float>
computeMixtureProbabilityCandidate(const MixtureGMM& gmm, const std::vector<Color>& colors)
{
  std::vector<float> result(colors.size(), 0.0f);
  for (const auto& gaussian : gmm.components) {
    const auto component_probability = computeGMMProbabilityCandidate(gaussian, colors);
    for (std::size_t i = 0; i < colors.size(); ++i) {
      result[i] += gaussian.pi * component_probability[i];
    }
  }
  return result;
}

inline TerminalWeights
computeTerminalWeightsCandidate(const MixtureGMM& background_gmm,
                                const MixtureGMM& foreground_gmm,
                                const std::vector<Color>& colors)
{
  TerminalWeights result;
  result.foreground_costs.resize(colors.size());
  result.background_costs.resize(colors.size());
  const auto background_probability = computeMixtureProbabilityCandidate(background_gmm, colors);
  const auto foreground_probability = computeMixtureProbabilityCandidate(foreground_gmm, colors);
  for (std::size_t i = 0; i < colors.size(); ++i) {
    result.foreground_costs[i] = -std::log(background_probability[i]);
    result.background_costs[i] = -std::log(foreground_probability[i]);
  }
  return result;
}

inline InitGraphNoSolveResult
computeInitGraphNoSolveCandidate(const MixtureGMM& background_gmm,
                                 const MixtureGMM& foreground_gmm,
                                 const std::vector<Color>& colors)
{
  InitGraphNoSolveResult result;
  result.terminal = computeTerminalWeightsCandidate(background_gmm, foreground_gmm, colors);
  result.sink_checksum = computeTerminalWriteSink(result.terminal);
  return result;
}

inline std::vector<std::size_t>
assignGMMComponentsReference(const MixtureGMM& background_gmm,
                             const MixtureGMM& foreground_gmm,
                             const std::vector<Color>& colors,
                             const std::vector<unsigned char>& foreground_mask)
{
  if (colors.size() != foreground_mask.size()) {
    throw std::invalid_argument("colors and foreground_mask size mismatch");
  }

  std::vector<std::size_t> components(colors.size(), 0);
  for (std::size_t idx = 0; idx < colors.size(); ++idx) {
    const MixtureGMM& gmm = foreground_mask[idx] ? foreground_gmm : background_gmm;
    std::size_t best = 0;
    float max_probability = 0.0f;
    for (std::size_t component = 0; component < gmm.components.size(); ++component) {
      const float probability = computeGMMProbabilityReferenceOne(gmm.components[component], colors[idx]);
      if (probability > max_probability) {
        best = component;
        max_probability = probability;
      }
    }
    components[idx] = best;
  }
  return components;
}

inline void
addGaussianFitSample(GaussianFitAccumulator& accumulator, const Color& c)
{
  accumulator.sum[0] += c.r;
  accumulator.sum[1] += c.g;
  accumulator.sum[2] += c.b;
  accumulator.products[0] += c.r * c.r;
  accumulator.products[1] += c.r * c.g;
  accumulator.products[2] += c.r * c.b;
  accumulator.products[3] += c.g * c.r;
  accumulator.products[4] += c.g * c.g;
  accumulator.products[5] += c.g * c.b;
  accumulator.products[6] += c.b * c.r;
  accumulator.products[7] += c.b * c.g;
  accumulator.products[8] += c.b * c.b;
  ++accumulator.count;
}

inline void
fitGaussianFromAccumulator(const GaussianFitAccumulator& accumulator,
                           std::size_t total_count,
                           Gaussian& gaussian,
                           float epsilon = 0.0001f)
{
  if (accumulator.count == 0) {
    gaussian.pi = 0.0f;
    return;
  }

  const float count_f = static_cast<float>(accumulator.count);
  const float mu_r = accumulator.sum[0] / count_f;
  const float mu_g = accumulator.sum[1] / count_f;
  const float mu_b = accumulator.sum[2] / count_f;
  gaussian.mu = {mu_r, mu_g, mu_b};

  Matrix3f covariance;
  covariance.values[0] = accumulator.products[0] / count_f - mu_r * mu_r + epsilon;
  covariance.values[1] = accumulator.products[1] / count_f - mu_r * mu_g;
  covariance.values[2] = accumulator.products[2] / count_f - mu_r * mu_b;
  covariance.values[3] = accumulator.products[3] / count_f - mu_g * mu_r;
  covariance.values[4] = accumulator.products[4] / count_f - mu_g * mu_g + epsilon;
  covariance.values[5] = accumulator.products[5] / count_f - mu_g * mu_b;
  covariance.values[6] = accumulator.products[6] / count_f - mu_b * mu_r;
  covariance.values[7] = accumulator.products[7] / count_f - mu_b * mu_g;
  covariance.values[8] = accumulator.products[8] / count_f - mu_b * mu_b + epsilon;

  const float determinant =
      covariance(0, 0) * (covariance(1, 1) * covariance(2, 2) -
                          covariance(1, 2) * covariance(2, 1)) -
      covariance(0, 1) * (covariance(1, 0) * covariance(2, 2) -
                          covariance(1, 2) * covariance(2, 0)) +
      covariance(0, 2) * (covariance(1, 0) * covariance(2, 1) -
                          covariance(1, 1) * covariance(2, 0));
  gaussian.determinant = determinant;
  gaussian.inverse.values[0] =
      (covariance(1, 1) * covariance(2, 2) -
       covariance(1, 2) * covariance(2, 1)) /
      determinant;
  gaussian.inverse.values[3] =
      -(covariance(1, 0) * covariance(2, 2) -
        covariance(1, 2) * covariance(2, 0)) /
      determinant;
  gaussian.inverse.values[6] =
      (covariance(1, 0) * covariance(2, 1) -
       covariance(1, 1) * covariance(2, 0)) /
      determinant;
  gaussian.inverse.values[1] =
      -(covariance(0, 1) * covariance(2, 2) -
        covariance(0, 2) * covariance(2, 1)) /
      determinant;
  gaussian.inverse.values[4] =
      (covariance(0, 0) * covariance(2, 2) -
       covariance(0, 2) * covariance(2, 0)) /
      determinant;
  gaussian.inverse.values[7] =
      -(covariance(0, 0) * covariance(2, 1) -
        covariance(0, 1) * covariance(2, 0)) /
      determinant;
  gaussian.inverse.values[2] =
      (covariance(0, 1) * covariance(1, 2) -
       covariance(0, 2) * covariance(1, 1)) /
      determinant;
  gaussian.inverse.values[5] =
      -(covariance(0, 0) * covariance(1, 2) -
        covariance(0, 2) * covariance(1, 0)) /
      determinant;
  gaussian.inverse.values[8] =
      (covariance(0, 0) * covariance(1, 1) -
       covariance(0, 1) * covariance(1, 0)) /
      determinant;
  gaussian.pi = count_f / static_cast<float>(total_count);
}

inline void
relearnGMMsFromComponents(const std::vector<Color>& colors,
                          const std::vector<unsigned char>& foreground_mask,
                          const std::vector<std::size_t>& components,
                          MixtureGMM& background_gmm,
                          MixtureGMM& foreground_gmm)
{
  if (colors.size() != foreground_mask.size() || colors.size() != components.size()) {
    throw std::invalid_argument("learnGMMs inputs size mismatch");
  }

  std::vector<GaussianFitAccumulator> back_fitters(background_gmm.components.size());
  std::vector<GaussianFitAccumulator> fore_fitters(foreground_gmm.components.size());
  std::size_t foreground_count = 0;
  std::size_t background_count = 0;
  for (std::size_t idx = 0; idx < colors.size(); ++idx) {
    if (foreground_mask[idx]) {
      addGaussianFitSample(fore_fitters[components[idx]], colors[idx]);
      ++foreground_count;
    }
    else {
      addGaussianFitSample(back_fitters[components[idx]], colors[idx]);
      ++background_count;
    }
  }

  for (std::size_t component = 0; component < background_gmm.components.size(); ++component) {
    fitGaussianFromAccumulator(
        back_fitters[component], background_count, background_gmm.components[component]);
  }
  for (std::size_t component = 0; component < foreground_gmm.components.size(); ++component) {
    fitGaussianFromAccumulator(
        fore_fitters[component], foreground_count, foreground_gmm.components[component]);
  }
}

inline FullLearnGMMsResult
learnGMMsReference(const MixtureGMM& background_gmm,
                   const MixtureGMM& foreground_gmm,
                   const std::vector<Color>& colors,
                   const std::vector<unsigned char>& foreground_mask)
{
  FullLearnGMMsResult result;
  result.background_gmm = background_gmm;
  result.foreground_gmm = foreground_gmm;
  result.components = assignGMMComponentsReference(
      background_gmm, foreground_gmm, colors, foreground_mask);
  relearnGMMsFromComponents(
      colors, foreground_mask, result.components, result.background_gmm, result.foreground_gmm);
  return result;
}

inline void
assignGMMComponentsForGroupCandidate(const MixtureGMM& gmm,
                                     const std::vector<Color>& colors,
                                     const std::vector<std::size_t>& positions,
                                     std::vector<std::size_t>& components)
{
  if (positions.empty()) {
    return;
  }

#if defined(__RVV10__)
  std::vector<float> max_probability(positions.size(), 0.0f);
  std::vector<std::size_t> group_components(positions.size(), 0);
  const std::size_t scratch_lanes = __riscv_vsetvlmax_e32m2();
  std::vector<float> r_buffer(scratch_lanes);
  std::vector<float> g_buffer(scratch_lanes);
  std::vector<float> b_buffer(scratch_lanes);
  std::vector<float> probability_buffer(scratch_lanes);

  for (std::size_t component = 0; component < gmm.components.size(); ++component) {
    const Gaussian& gaussian = gmm.components[component];
    if (gaussian.pi <= 0.0f || gaussian.determinant <= 0.0f) {
      continue;
    }

    const float inv_sqrt_det = 1.0f / std::sqrt(gaussian.determinant);
    std::size_t offset = 0;
    while (offset < positions.size()) {
      const std::size_t vl = __riscv_vsetvl_e32m2(positions.size() - offset);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        const Color& color = colors[positions[offset + lane]];
        r_buffer[lane] = color.r - gaussian.mu.r;
        g_buffer[lane] = color.g - gaussian.mu.g;
        b_buffer[lane] = color.b - gaussian.mu.b;
      }

      const vfloat32m2_t r = __riscv_vle32_v_f32m2(r_buffer.data(), vl);
      const vfloat32m2_t g = __riscv_vle32_v_f32m2(g_buffer.data(), vl);
      const vfloat32m2_t b = __riscv_vle32_v_f32m2(b_buffer.data(), vl);

      vfloat32m2_t row0 = __riscv_vfmul_vf_f32m2(r, gaussian.inverse(0, 0), vl);
      row0 = __riscv_vfmacc_vf_f32m2(row0, gaussian.inverse(1, 0), g, vl);
      row0 = __riscv_vfmacc_vf_f32m2(row0, gaussian.inverse(2, 0), b, vl);

      vfloat32m2_t row1 = __riscv_vfmul_vf_f32m2(r, gaussian.inverse(0, 1), vl);
      row1 = __riscv_vfmacc_vf_f32m2(row1, gaussian.inverse(1, 1), g, vl);
      row1 = __riscv_vfmacc_vf_f32m2(row1, gaussian.inverse(2, 1), b, vl);

      vfloat32m2_t row2 = __riscv_vfmul_vf_f32m2(r, gaussian.inverse(0, 2), vl);
      row2 = __riscv_vfmacc_vf_f32m2(row2, gaussian.inverse(1, 2), g, vl);
      row2 = __riscv_vfmacc_vf_f32m2(row2, gaussian.inverse(2, 2), b, vl);

      vfloat32m2_t d = __riscv_vfmul_vv_f32m2(r, row0, vl);
      d = __riscv_vfmacc_vv_f32m2(d, g, row1, vl);
      d = __riscv_vfmacc_vv_f32m2(d, b, row2, vl);
      const vfloat32m2_t exponent = __riscv_vfmul_vf_f32m2(d, -0.5f, vl);
      const vfloat32m2_t exp_value = pcl::expf_RVV_f32m2(exponent, vl);
      const vfloat32m2_t value = __riscv_vfmul_vf_f32m2(exp_value, inv_sqrt_det, vl);
      __riscv_vse32_v_f32m2(probability_buffer.data(), value, vl);

      for (std::size_t lane = 0; lane < vl; ++lane) {
        const std::size_t group_index = offset + lane;
        if (probability_buffer[lane] > max_probability[group_index]) {
          group_components[group_index] = component;
          max_probability[group_index] = probability_buffer[lane];
        }
      }
      offset += vl;
    }
  }

  for (std::size_t i = 0; i < positions.size(); ++i) {
    components[positions[i]] = group_components[i];
  }
#else
  (void)gmm;
  (void)colors;
  (void)positions;
  (void)components;
#endif
}

// 这个 helper 只覆盖 `learnGMMs()` 第一段 component assignment（分量归属选择）。
// RVV 构建下，概率公式复用已验证的批量 helper；按 foreground/background mask（前景/背景掩码）
// 分组是测试专用 staging（分阶段暂存），不等同于完整 production `learnGMMs()`。
inline std::vector<std::size_t>
assignGMMComponentsCandidate(const MixtureGMM& background_gmm,
                             const MixtureGMM& foreground_gmm,
                             const std::vector<Color>& colors,
                             const std::vector<unsigned char>& foreground_mask)
{
  if (colors.size() != foreground_mask.size()) {
    throw std::invalid_argument("colors and foreground_mask size mismatch");
  }

#if defined(__RVV10__)
  std::vector<std::size_t> foreground_positions;
  std::vector<std::size_t> background_positions;
  foreground_positions.reserve(colors.size());
  background_positions.reserve(colors.size());
  for (std::size_t i = 0; i < foreground_mask.size(); ++i) {
    if (foreground_mask[i]) {
      foreground_positions.push_back(i);
    }
    else {
      background_positions.push_back(i);
    }
  }

  std::vector<std::size_t> components(colors.size(), 0);
  assignGMMComponentsForGroupCandidate(
      foreground_gmm, colors, foreground_positions, components);
  assignGMMComponentsForGroupCandidate(
      background_gmm, colors, background_positions, components);
  return components;
#else
  return assignGMMComponentsReference(background_gmm, foreground_gmm, colors, foreground_mask);
#endif
}

// 这个 helper 对齐完整 `learnGMMs()` 局部函数形态：candidate 只替换第一段
// component assignment（分量归属选择），第二段 GMM relearn（重新训练）复用同一标量 fitter 语义。
// 因此本 helper 能判断 assignment 的局部收益在完整 GMM 学习阶段是否被后续累加稀释。
inline FullLearnGMMsResult
learnGMMsCandidate(const MixtureGMM& background_gmm,
                   const MixtureGMM& foreground_gmm,
                   const std::vector<Color>& colors,
                   const std::vector<unsigned char>& foreground_mask)
{
  FullLearnGMMsResult result;
  result.background_gmm = background_gmm;
  result.foreground_gmm = foreground_gmm;
  result.components = assignGMMComponentsCandidate(
      background_gmm, foreground_gmm, colors, foreground_mask);
  relearnGMMsFromComponents(
      colors, foreground_mask, result.components, result.background_gmm, result.foreground_gmm);
  return result;
}

} // namespace grabcut_diag
