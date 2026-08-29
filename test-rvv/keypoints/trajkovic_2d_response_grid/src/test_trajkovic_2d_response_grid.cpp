/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 TrajkovicKeypoint2D 的真实 public
 * `compute()` 入口。Std build（标量构建）和 RVV build（RVV 构建）都必须得到相同
 * keypoint indices（关键点索引）和输出 intensity（响应值）。测试通过不等于性能成立；
 * 性能结论只看板卡 repeated benchmark（重复性能测试）。
 */

#include <gtest/gtest.h>

#define private public
#include <pcl/keypoints/trajkovic_2d.h>
#undef private
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace
{
using PointT = pcl::PointXYZI;
using Detector = pcl::TrajkovicKeypoint2D<PointT, PointT>;

struct RunResult
{
  std::vector<int> indices;
  std::vector<float> intensities;
  std::vector<float> responses;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  bool is_dense = false;
};

float
syntheticIntensity(const std::size_t row, const std::size_t col)
{
  const float base = static_cast<float>((row * 17 + col * 11) % 251) * 0.015f;
  const float wave = static_cast<float>((row % 7) * (col % 5)) * 0.021f;
  const bool in_patch_a = row > 18 && row < 56 && col > 24 && col < 73;
  const bool in_patch_b = row > 70 && row < 102 && col > 93 && col < 146;
  return base + wave + (in_patch_a ? 3.25f : 0.0f) + (in_patch_b ? 4.75f : 0.0f);
}

pcl::PointCloud<PointT>::Ptr
makeCloud(const std::size_t width, const std::size_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  cloud->width = static_cast<std::uint32_t>(width);
  cloud->height = static_cast<std::uint32_t>(height);
  cloud->is_dense = true;
  cloud->points.resize(width * height);
  for (std::size_t row = 0; row < height; ++row)
  {
    for (std::size_t col = 0; col < width; ++col)
    {
      PointT& point = (*cloud)[row * width + col];
      point.x = static_cast<float>(col);
      point.y = static_cast<float>(row);
      point.z = 1.0f + 0.001f * static_cast<float>((row + col) % 13);
      point.intensity = syntheticIntensity(row, col);
    }
  }
  return cloud;
}

RunResult
runDetector(const Detector::ComputationMethod method,
            const int window_size,
            const float first_threshold,
            const float second_threshold,
            const std::size_t width,
            const std::size_t height)
{
  Detector detector(method, window_size, first_threshold, second_threshold);
  detector.setNumberOfThreads(1);
  detector.setInputCloud(makeCloud(width, height));

  pcl::PointCloud<PointT> output;
  detector.compute(output);

  RunResult result;
  result.indices = detector.getKeypointsIndices()->indices;
  result.width = output.width;
  result.height = output.height;
  result.is_dense = output.is_dense;
  result.intensities.reserve(output.size());
  for (const auto& point : output)
    result.intensities.push_back(point.intensity);
  result.responses.assign(detector.response_->points.begin(), detector.response_->points.end());
  return result;
}

// 这份 reference（参考链路）按当前 production 标量公式逐项重建 response grid（响应图）
// 和后续 NMS（非极大值抑制）。它不包含 RVV intrinsic，用来防止 Std/RVV 两个构建只验证
// “各自重复运行稳定”，却漏掉跨实现语义分叉。
RunResult
runReference(const Detector::ComputationMethod method,
             const int window_size,
             const float first_threshold,
             const float second_threshold,
             const std::size_t width,
             const std::size_t height)
{
  const auto cloud = makeCloud(width, height);
  const int half_window_size = window_size / 2;
  const int w = static_cast<int>(width) - half_window_size;
  const int h = static_cast<int>(height) - half_window_size;
  std::vector<float> response(width * height, 0.0f);

  for (int j = half_window_size; j < h; ++j)
  {
    for (int i = half_window_size; i < w; ++i)
    {
      const auto intensity_at = [&](const int x, const int y) {
        return (*cloud)[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)].intensity;
      };
      const float center = intensity_at(i, j);
      const float up = intensity_at(i, j - half_window_size);
      const float down = intensity_at(i, j + half_window_size);
      const float left = intensity_at(i - half_window_size, j);
      const float right = intensity_at(i + half_window_size, j);

      const float up_center = up - center;
      const float down_center = down - center;
      const float right_center = right - center;
      const float left_center = left - center;
      const float r0 = up_center * up_center + down_center * down_center;
      const float r2 = right_center * right_center + left_center * left_center;

      if (method == Detector::FOUR_CORNERS)
      {
        const float d = std::min(r0, r2);
        if (d < first_threshold)
          continue;

        const float b1 = (right - up) * up_center + (left - down) * down_center;
        const float b2 = (right - down) * down_center + (left - up) * up_center;
        const float b = std::min(b1, b2);
        const float a = r2 - r0 - 2.0f * b;
        response[static_cast<std::size_t>(j) * width + static_cast<std::size_t>(i)] =
            ((b < 0.0f) && ((b + a) > 0.0f)) ? r0 - ((b * b) / a) : d;
      }
      else
      {
        const float upleft = intensity_at(i - half_window_size, j - half_window_size);
        const float upright = intensity_at(i + half_window_size, j - half_window_size);
        const float downleft = intensity_at(i - half_window_size, j + half_window_size);
        const float downright = intensity_at(i + half_window_size, j + half_window_size);
        const float upright_center = upright - center;
        const float downleft_center = downleft - center;
        const float downright_center = downright - center;
        const float upleft_center = upleft - center;
        const float r1 = upright_center * upright_center + downleft_center * downleft_center;
        const float r3 = downright_center * downright_center + upleft_center * upleft_center;
        const float r_values[4] = {r0, r1, r2, r3};
        const float d = *std::min_element(std::begin(r_values), std::end(r_values));
        if (d < first_threshold)
          continue;

        const float b_values[4] = {
            (upright - up) * up_center + (downleft - down) * down_center,
            (right - upright) * upright_center + (left - downleft) * downleft_center,
            (downright - right) * downright_center + (upleft - left) * upleft_center,
            (down - downright) * downright_center + (up - upleft) * upleft_center};
        const float a_values[4] = {
            r1 - r0 - b_values[0] - b_values[0],
            r2 - r1 - b_values[1] - b_values[1],
            r3 - r2 - b_values[2] - b_values[2],
            r0 - r3 - b_values[3] - b_values[3]};
        const float sum_ab[4] = {
            a_values[0] + b_values[0],
            a_values[1] + b_values[1],
            a_values[2] + b_values[2],
            a_values[3] + b_values[3]};
        if ((*std::max_element(std::begin(b_values), std::end(b_values)) < 0.0f) &&
            (*std::min_element(std::begin(sum_ab), std::end(sum_ab)) > 0.0f))
        {
          const float d_values[4] = {
              b_values[0] * b_values[0] / a_values[0],
              b_values[1] * b_values[1] / a_values[1],
              b_values[2] * b_values[2] / a_values[2],
              b_values[3] * b_values[3] / a_values[3]};
          response[static_cast<std::size_t>(j) * width + static_cast<std::size_t>(i)] =
              *std::min_element(std::begin(d_values), std::end(d_values));
        }
        else
        {
          response[static_cast<std::size_t>(j) * width + static_cast<std::size_t>(i)] = d;
        }
      }
    }
  }

  std::vector<int> sorted_indices(response.size());
  std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
  std::sort(sorted_indices.begin(), sorted_indices.end(), [&](const int lhs, const int rhs) {
    return response[static_cast<std::size_t>(lhs)] > response[static_cast<std::size_t>(rhs)];
  });

  std::vector<bool> occupancy(response.size(), false);
  RunResult result;
  result.height = 1;
  result.is_dense = cloud->is_dense;
  result.responses = response;
  for (const int idx : sorted_indices)
  {
    if (response[static_cast<std::size_t>(idx)] < second_threshold ||
        occupancy[static_cast<std::size_t>(idx)])
      continue;
    result.indices.push_back(idx);
    result.intensities.push_back(response[static_cast<std::size_t>(idx)]);
    const int x = idx % static_cast<int>(width);
    const int y = idx / static_cast<int>(width);
    const int u_end = std::min(static_cast<int>(width), x + half_window_size);
    const int v_end = std::min(static_cast<int>(height), y + half_window_size);
    for (int v = std::max(0, y - half_window_size); v < v_end; ++v)
      for (int u = std::max(0, x - half_window_size); u < u_end; ++u)
        occupancy[static_cast<std::size_t>(v) * width + static_cast<std::size_t>(u)] = true;
  }
  result.width = static_cast<std::uint32_t>(result.indices.size());
  return result;
}

void
expectSameRun(const RunResult& lhs, const RunResult& rhs)
{
  ASSERT_EQ(lhs.responses.size(), rhs.responses.size());
  for (std::size_t i = 0; i < lhs.responses.size(); ++i)
    EXPECT_NEAR(lhs.responses[i], rhs.responses[i], 1e-5f) << "at response " << i;
  EXPECT_EQ(lhs.indices, rhs.indices);
  ASSERT_EQ(lhs.intensities.size(), rhs.intensities.size());
  for (std::size_t i = 0; i < lhs.intensities.size(); ++i)
    EXPECT_NEAR(lhs.intensities[i], rhs.intensities[i], 1e-5f) << "at output " << i;
  EXPECT_EQ(lhs.width, rhs.width);
  EXPECT_EQ(lhs.height, rhs.height);
  EXPECT_EQ(lhs.is_dense, rhs.is_dense);
}
} // namespace

TEST(Trajkovic2DResponseGrid, FourCornersPublicComputeIsStable)
{
  const RunResult reference =
      runReference(Detector::FOUR_CORNERS, 3, 0.02f, 0.20f, 131, 97);
  const RunResult first =
      runDetector(Detector::FOUR_CORNERS, 3, 0.02f, 0.20f, 131, 97);
  const RunResult second =
      runDetector(Detector::FOUR_CORNERS, 3, 0.02f, 0.20f, 131, 97);
  expectSameRun(reference, first);
  expectSameRun(first, second);
  EXPECT_FALSE(first.indices.empty());
}

TEST(Trajkovic2DResponseGrid, EightCornersPublicComputeIsStable)
{
  const RunResult reference =
      runReference(Detector::EIGHT_CORNERS, 3, 0.02f, 0.20f, 133, 99);
  const RunResult first =
      runDetector(Detector::EIGHT_CORNERS, 3, 0.02f, 0.20f, 133, 99);
  const RunResult second =
      runDetector(Detector::EIGHT_CORNERS, 3, 0.02f, 0.20f, 133, 99);
  expectSameRun(reference, first);
  expectSameRun(first, second);
  EXPECT_FALSE(first.indices.empty());
}

TEST(Trajkovic2DResponseGrid, TailWidthPublicComputeIsStable)
{
  const RunResult reference =
      runReference(Detector::FOUR_CORNERS, 3, 0.02f, 0.20f, 641, 37);
  const RunResult first =
      runDetector(Detector::FOUR_CORNERS, 3, 0.02f, 0.20f, 641, 37);
  const RunResult second =
      runDetector(Detector::FOUR_CORNERS, 3, 0.02f, 0.20f, 641, 37);
  expectSameRun(reference, first);
  expectSameRun(first, second);
}

TEST(Trajkovic2DResponseGrid, NonThreeByThreeWindowFallsBackWithoutChangingResult)
{
  const RunResult reference =
      runReference(Detector::EIGHT_CORNERS, 5, 0.02f, 0.20f, 69, 51);
  const RunResult first =
      runDetector(Detector::EIGHT_CORNERS, 5, 0.02f, 0.20f, 69, 51);
  const RunResult second =
      runDetector(Detector::EIGHT_CORNERS, 5, 0.02f, 0.20f, 69, 51);
  expectSameRun(reference, first);
  expectSameRun(first, second);
}

int
main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
