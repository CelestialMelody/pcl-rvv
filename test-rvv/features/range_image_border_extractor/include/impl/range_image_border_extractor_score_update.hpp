#pragma once

/*
 * 本文件做什么：
 * 这里放 RangeImageBorderExtractor（距离图边界提取器）score-update 阶段的
 * 测试专用 reference（参考链路）和 RVV candidate（候选实现）。它复刻
 * updatedScoreAccordingToNeighborValues 的 3x3 邻域传播公式，用于判断这段
 * 连续 float 图像循环是否值得进入后续生产形态诊断。
 *
 * 证据边界：
 * 本文件只服务 test-rvv，不证明 production dispatch（生产分流）已经接入。
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::range_image_border_extractor
{

struct ScoreImageSet
{
  std::vector<float> left;
  std::vector<float> right;
  std::vector<float> top;
  std::vector<float> bottom;
};

inline float
updatedScoreAtScalar(const float* border_scores,
                     const std::size_t width,
                     const std::size_t height,
                     const std::size_t x,
                     const std::size_t y,
                     const float minimum_border_probability)
{
  const float max_score_bonus = 0.5f;
  const float border_score = border_scores[y * width + x];
  if (border_score + max_score_bonus * (1.0f - border_score) < minimum_border_probability)
    return border_score;

  float average_neighbor_score = 0.0f;
  float weight_sum = 0.0f;
  for (int y2 = static_cast<int>(y) - 1; y2 <= static_cast<int>(y) + 1; ++y2)
  {
    for (int x2 = static_cast<int>(x) - 1; x2 <= static_cast<int>(x) + 1; ++x2)
    {
      if (x2 < 0 || y2 < 0 || x2 >= static_cast<int>(width) || y2 >= static_cast<int>(height) ||
          (x2 == static_cast<int>(x) && y2 == static_cast<int>(y)))
      {
        continue;
      }
      average_neighbor_score += border_scores[static_cast<std::size_t>(y2) * width + static_cast<std::size_t>(x2)];
      weight_sum += 1.0f;
    }
  }
  average_neighbor_score /= weight_sum;

  if (average_neighbor_score * border_score < 0.0f)
    return border_score;

  return border_score + max_score_bonus * average_neighbor_score * (1.0f - std::abs(border_score));
}

inline void
updateScoresStd(const float* border_scores,
                const std::size_t width,
                const std::size_t height,
                const float minimum_border_probability,
                float* output)
{
  for (std::size_t y = 0; y < height; ++y)
    for (std::size_t x = 0; x < width; ++x)
      output[y * width + x] =
          updatedScoreAtScalar(border_scores, width, height, x, y, minimum_border_probability);
}

inline std::vector<float>
updateScoresStd(const std::vector<float>& border_scores,
                const std::size_t width,
                const std::size_t height,
                const float minimum_border_probability)
{
  std::vector<float> output(width * height);
  updateScoresStd(border_scores.data(), width, height, minimum_border_probability, output.data());
  return output;
}

inline void
updateScoresRVV(const float* border_scores,
                const std::size_t width,
                const std::size_t height,
                const float minimum_border_probability,
                float* output)
{
#if defined(__RVV10__)
  if (width < 3 || height < 3)
  {
    updateScoresStd(border_scores, width, height, minimum_border_probability, output);
    return;
  }

  for (std::size_t x = 0; x < width; ++x)
  {
    output[x] = updatedScoreAtScalar(border_scores, width, height, x, 0, minimum_border_probability);
    const std::size_t bottom_index = (height - 1) * width + x;
    output[bottom_index] =
        updatedScoreAtScalar(border_scores, width, height, x, height - 1, minimum_border_probability);
  }

  for (std::size_t y = 1; y + 1 < height; ++y)
  {
    output[y * width] = updatedScoreAtScalar(border_scores, width, height, 0, y, minimum_border_probability);
    output[y * width + width - 1] =
        updatedScoreAtScalar(border_scores, width, height, width - 1, y, minimum_border_probability);

    std::size_t x = 1;
    while (x + 1 < width)
    {
      const std::size_t vl = __riscv_vsetvl_e32m1(width - 1 - x);
      const std::size_t center_index = y * width + x;

      vfloat32m1_t sum = __riscv_vle32_v_f32m1(border_scores + center_index - width - 1, vl);
      sum = __riscv_vfadd_vv_f32m1(sum, __riscv_vle32_v_f32m1(border_scores + center_index - width, vl), vl);
      sum = __riscv_vfadd_vv_f32m1(sum, __riscv_vle32_v_f32m1(border_scores + center_index - width + 1, vl), vl);
      sum = __riscv_vfadd_vv_f32m1(sum, __riscv_vle32_v_f32m1(border_scores + center_index - 1, vl), vl);
      sum = __riscv_vfadd_vv_f32m1(sum, __riscv_vle32_v_f32m1(border_scores + center_index + 1, vl), vl);
      sum = __riscv_vfadd_vv_f32m1(sum, __riscv_vle32_v_f32m1(border_scores + center_index + width - 1, vl), vl);
      sum = __riscv_vfadd_vv_f32m1(sum, __riscv_vle32_v_f32m1(border_scores + center_index + width, vl), vl);
      sum = __riscv_vfadd_vv_f32m1(sum, __riscv_vle32_v_f32m1(border_scores + center_index + width + 1, vl), vl);

      const vfloat32m1_t border_score = __riscv_vle32_v_f32m1(border_scores + center_index, vl);
      const vfloat32m1_t one = __riscv_vfmv_v_f_f32m1(1.0f, vl);
      const vfloat32m1_t average = __riscv_vfmul_vf_f32m1(sum, 0.125f, vl);
      const vfloat32m1_t threshold_value =
          __riscv_vfadd_vv_f32m1(border_score,
                                 __riscv_vfmul_vf_f32m1(__riscv_vfsub_vf_f32m1(border_score, 1.0f, vl),
                                                        -0.5f,
                                                        vl),
                                 vl);
      const vbool32_t below_threshold =
          __riscv_vmflt_vf_f32m1_b32(threshold_value, minimum_border_probability, vl);
      const vbool32_t opposite_sign =
          __riscv_vmflt_vf_f32m1_b32(__riscv_vfmul_vv_f32m1(average, border_score, vl), 0.0f, vl);
      const vbool32_t keep_original = __riscv_vmor_mm_b32(below_threshold, opposite_sign, vl);

      const vfloat32m1_t candidate =
          __riscv_vfadd_vv_f32m1(border_score,
                                 __riscv_vfmul_vv_f32m1(__riscv_vfmul_vf_f32m1(average, 0.5f, vl),
                                                        __riscv_vfsub_vv_f32m1(one,
                                                                               __riscv_vfabs_v_f32m1(border_score, vl),
                                                                               vl),
                                                        vl),
                                 vl);
      const vfloat32m1_t selected = __riscv_vmerge_vvm_f32m1(candidate, border_score, keep_original, vl);
      __riscv_vse32_v_f32m1(output + center_index, selected, vl);

      x += vl;
    }
  }
#else
  updateScoresStd(border_scores, width, height, minimum_border_probability, output);
#endif
}

inline std::vector<float>
updateScoresRVV(const std::vector<float>& border_scores,
                const std::size_t width,
                const std::size_t height,
                const float minimum_border_probability)
{
  std::vector<float> output(width * height);
  updateScoresRVV(border_scores.data(), width, height, minimum_border_probability, output.data());
  return output;
}

inline ScoreImageSet
updateScoreImageSetStd(const ScoreImageSet& border_scores,
                       const std::size_t width,
                       const std::size_t height,
                       const float minimum_border_probability)
{
  return {updateScoresStd(border_scores.left, width, height, minimum_border_probability),
          updateScoresStd(border_scores.right, width, height, minimum_border_probability),
          updateScoresStd(border_scores.top, width, height, minimum_border_probability),
          updateScoresStd(border_scores.bottom, width, height, minimum_border_probability)};
}

inline ScoreImageSet
updateScoreImageSetRVV(const ScoreImageSet& border_scores,
                       const std::size_t width,
                       const std::size_t height,
                       const float minimum_border_probability)
{
  return {updateScoresRVV(border_scores.left, width, height, minimum_border_probability),
          updateScoresRVV(border_scores.right, width, height, minimum_border_probability),
          updateScoresRVV(border_scores.top, width, height, minimum_border_probability),
          updateScoresRVV(border_scores.bottom, width, height, minimum_border_probability)};
}

} // namespace pcl::features::rvv_test::range_image_border_extractor
