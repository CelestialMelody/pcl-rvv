#pragma once

/*
 * 本文件做什么：
 * 这里放 Hough3DGrouping::houghVoting() 首阶段的 scalar reference（标量参考链路）
 * 和 RVV candidate（RVV 候选链路）。两条链路都复刻 production 里 vote
 * generation 的核心公式：用 scene RF 的三个基向量和 model vote 做 3x3 线性
 * 组合，再加上 scene point 本身，得到 scene vote。
 *
 * 证据边界：
 * 这些 helper 是 production-shaped diagnostic（生产形态诊断），不是
 * production direct（真实生产路径证据）。它们目前不接 HoughSpace3D::vote() /
 * voteInt() 的 accumulator scatter，也不接 `findMaxima()`。
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::test::hough_3d_rvv
{

enum class ExecutionPath
{
  ScalarFallback,
  RvvVoteGeneration
};

struct VoteGenerationInput
{
  std::array<float, 3> scene_point{};
  std::array<float, 3> model_vote{};
  std::array<float, 3> scene_rf_x{};
  std::array<float, 3> scene_rf_y{};
  std::array<float, 3> scene_rf_z{};
  float distance = 0.0f;
};

struct VoteGenerationOutput
{
  std::vector<std::array<float, 3>> scene_votes;
  std::array<float, 3> d_min{};
  std::array<float, 3> d_max{};
  float max_distance = 0.0f;
};

[[gnu::noinline]]
inline VoteGenerationOutput
computeSceneVotesScalarReference(const std::vector<VoteGenerationInput>& inputs,
                                 const bool use_interpolation,
                                 const bool use_distance_weight)
{
  VoteGenerationOutput out;
  out.scene_votes.resize(inputs.size());
  out.d_min = {std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max()};
  out.d_max = {-std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max()};
  out.max_distance = -std::numeric_limits<float>::max();

  for (std::size_t i = 0; i < inputs.size(); ++i)
  {
    const auto& input = inputs[i];
    const std::array<float, 3> vote{
        input.scene_rf_x[0] * input.model_vote[0] + input.scene_rf_y[0] * input.model_vote[1] +
            input.scene_rf_z[0] * input.model_vote[2] + input.scene_point[0],
        input.scene_rf_x[1] * input.model_vote[0] + input.scene_rf_y[1] * input.model_vote[1] +
            input.scene_rf_z[1] * input.model_vote[2] + input.scene_point[1],
        input.scene_rf_x[2] * input.model_vote[0] + input.scene_rf_y[2] * input.model_vote[1] +
            input.scene_rf_z[2] * input.model_vote[2] + input.scene_point[2],
    };
    out.scene_votes[i] = vote;
    for (int axis = 0; axis < 3; ++axis)
    {
      if (vote[axis] < out.d_min[axis])
        out.d_min[axis] = vote[axis];
      if (vote[axis] > out.d_max[axis])
        out.d_max[axis] = vote[axis];
    }
    if (use_interpolation && use_distance_weight && out.max_distance < input.distance)
      out.max_distance = input.distance;
  }

  return out;
}

[[gnu::noinline]]
inline VoteGenerationOutput
computeSceneVotesCandidate(const std::vector<VoteGenerationInput>& inputs,
                           const bool use_interpolation,
                           const bool use_distance_weight,
                           ExecutionPath& execution_path)
{
#if defined(__RVV10__)
  execution_path = ExecutionPath::RvvVoteGeneration;
  VoteGenerationOutput out;
  out.scene_votes.resize(inputs.size());
  out.d_min = {std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max()};
  out.d_max = {-std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max(),
               -std::numeric_limits<float>::max()};
  out.max_distance = -std::numeric_limits<float>::max();

  if (inputs.empty())
    return out;

  const auto* base = reinterpret_cast<const std::uint8_t*>(inputs.data());
  const std::ptrdiff_t stride = static_cast<std::ptrdiff_t>(sizeof(VoteGenerationInput));
  const std::size_t max_vl = __riscv_vsetvlmax_e32m2();
  std::vector<float> vote_x_values(max_vl);
  std::vector<float> vote_y_values(max_vl);
  std::vector<float> vote_z_values(max_vl);

  constexpr std::size_t scene_point_offset = offsetof(VoteGenerationInput, scene_point);
  constexpr std::size_t model_vote_offset = offsetof(VoteGenerationInput, model_vote);
  constexpr std::size_t scene_rf_x_offset = offsetof(VoteGenerationInput, scene_rf_x);
  constexpr std::size_t scene_rf_y_offset = offsetof(VoteGenerationInput, scene_rf_y);
  constexpr std::size_t scene_rf_z_offset = offsetof(VoteGenerationInput, scene_rf_z);

  for (std::size_t i = 0; i < inputs.size ();)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2(inputs.size() - i);
    const auto load_field = [&](const std::size_t field_offset, const std::size_t lane_offset) {
      return __riscv_vlse32_v_f32m2(
          reinterpret_cast<const float*>(base + field_offset + lane_offset * sizeof(float)),
          stride,
          vl);
    };

    const vfloat32m2_t scene_point_x = load_field(scene_point_offset, 0);
    const vfloat32m2_t scene_point_y = load_field(scene_point_offset, 1);
    const vfloat32m2_t scene_point_z = load_field(scene_point_offset, 2);
    const vfloat32m2_t model_vote_x = load_field(model_vote_offset, 0);
    const vfloat32m2_t model_vote_y = load_field(model_vote_offset, 1);
    const vfloat32m2_t model_vote_z = load_field(model_vote_offset, 2);
    const vfloat32m2_t scene_rf_x0 = load_field(scene_rf_x_offset, 0);
    const vfloat32m2_t scene_rf_x1 = load_field(scene_rf_x_offset, 1);
    const vfloat32m2_t scene_rf_x2 = load_field(scene_rf_x_offset, 2);
    const vfloat32m2_t scene_rf_y0 = load_field(scene_rf_y_offset, 0);
    const vfloat32m2_t scene_rf_y1 = load_field(scene_rf_y_offset, 1);
    const vfloat32m2_t scene_rf_y2 = load_field(scene_rf_y_offset, 2);
    const vfloat32m2_t scene_rf_z0 = load_field(scene_rf_z_offset, 0);
    const vfloat32m2_t scene_rf_z1 = load_field(scene_rf_z_offset, 1);
    const vfloat32m2_t scene_rf_z2 = load_field(scene_rf_z_offset, 2);

    vfloat32m2_t vote_x = __riscv_vfmul_vv_f32m2(scene_rf_x0, model_vote_x, vl);
    vote_x = __riscv_vfadd_vv_f32m2(vote_x, __riscv_vfmul_vv_f32m2(scene_rf_y0, model_vote_y, vl), vl);
    vote_x = __riscv_vfadd_vv_f32m2(vote_x, __riscv_vfmul_vv_f32m2(scene_rf_z0, model_vote_z, vl), vl);
    vote_x = __riscv_vfadd_vv_f32m2(vote_x, scene_point_x, vl);

    vfloat32m2_t vote_y = __riscv_vfmul_vv_f32m2(scene_rf_x1, model_vote_x, vl);
    vote_y = __riscv_vfadd_vv_f32m2(vote_y, __riscv_vfmul_vv_f32m2(scene_rf_y1, model_vote_y, vl), vl);
    vote_y = __riscv_vfadd_vv_f32m2(vote_y, __riscv_vfmul_vv_f32m2(scene_rf_z1, model_vote_z, vl), vl);
    vote_y = __riscv_vfadd_vv_f32m2(vote_y, scene_point_y, vl);

    vfloat32m2_t vote_z = __riscv_vfmul_vv_f32m2(scene_rf_x2, model_vote_x, vl);
    vote_z = __riscv_vfadd_vv_f32m2(vote_z, __riscv_vfmul_vv_f32m2(scene_rf_y2, model_vote_y, vl), vl);
    vote_z = __riscv_vfadd_vv_f32m2(vote_z, __riscv_vfmul_vv_f32m2(scene_rf_z2, model_vote_z, vl), vl);
    vote_z = __riscv_vfadd_vv_f32m2(vote_z, scene_point_z, vl);

    __riscv_vse32_v_f32m2(vote_x_values.data(), vote_x, vl);
    __riscv_vse32_v_f32m2(vote_y_values.data(), vote_y, vl);
    __riscv_vse32_v_f32m2(vote_z_values.data(), vote_z, vl);

    for (std::size_t lane = 0; lane < vl; ++lane)
    {
      const std::array<float, 3> vote{
          vote_x_values[lane],
          vote_y_values[lane],
          vote_z_values[lane],
      };
      out.scene_votes[i + lane] = vote;
      for (int axis = 0; axis < 3; ++axis)
      {
        if (vote[axis] < out.d_min[axis])
          out.d_min[axis] = vote[axis];
        if (vote[axis] > out.d_max[axis])
          out.d_max[axis] = vote[axis];
      }
      if (use_interpolation && use_distance_weight && out.max_distance < inputs[i + lane].distance)
        out.max_distance = inputs[i + lane].distance;
    }

    i += vl;
  }
  return out;
#else
  execution_path = ExecutionPath::ScalarFallback;
  return computeSceneVotesScalarReference(inputs, use_interpolation, use_distance_weight);
#endif
}

inline std::uint64_t
checksumSceneVotes(const std::vector<std::array<float, 3>>& votes)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto& vote : votes)
  {
    for (const float value : vote)
    {
      std::uint32_t bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      result ^= static_cast<std::uint64_t>(bits);
      result *= 1099511628211ull;
    }
  }
  return result;
}

} // namespace pcl::test::hough_3d_rvv
