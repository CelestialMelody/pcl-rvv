/*
 * 本文件做什么：
 * 这些 gtest（Google Test 单元测试）验证 TrajkovicKeypoint3D 的
 * FOUR_CORNERS response map（四邻域响应图）测试专用 helper 是否复刻
 * production `detectKeypoints()` 中 precomputed normals（预计算法线）路径。
 *
 * 证据边界：
 * 本文件只覆盖 response-only diagnostic（只测响应图的诊断）边界，不覆盖
 * normal estimation（法线估计）、non-max suppression（非极大值抑制）或
 * production dispatch（生产分流）。
 */

#include "trajkovic_3d.h"

#include <pcl/keypoints/trajkovic_3d.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace t3d = pcl::keypoints::rvv_test::trajkovic_3d;

namespace
{
using Detector = pcl::TrajkovicKeypoint3D<pcl::PointXYZ, pcl::PointXYZI, pcl::Normal>;

struct ResponseCase
{
  std::size_t width = 0;
  std::size_t height = 0;
  std::vector<pcl::PointXYZ> points;
  std::vector<pcl::Normal> normals;
};

ResponseCase
makeCase(std::size_t width, std::size_t height)
{
  ResponseCase data;
  data.width = width;
  data.height = height;
  data.points.resize(width * height);
  data.normals.resize(width * height);

  for (std::size_t y = 0; y < height; ++y)
  {
    for (std::size_t x = 0; x < width; ++x)
    {
      const std::size_t index = y * width + x;
      data.points[index].x = static_cast<float>(x) * 0.01f;
      data.points[index].y = static_cast<float>(y) * 0.02f;
      data.points[index].z = 1.0f + static_cast<float>((x + 3 * y) % 17) * 0.001f;

      const float nx = 0.08f * static_cast<float>(static_cast<int>(x % 7) - 3);
      const float ny = 0.06f * static_cast<float>(static_cast<int>(y % 5) - 2);
      const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
      data.normals[index].normal_x = nx;
      data.normals[index].normal_y = ny;
      data.normals[index].normal_z = nz;
      data.normals[index].curvature = 0.0f;
    }
  }

  data.points[11 * width + 13].z = std::numeric_limits<float>::quiet_NaN();
  data.normals[7 * width + 8].normal_x = std::numeric_limits<float>::infinity();
  data.normals[9 * width + 10].normal_z = std::numeric_limits<float>::quiet_NaN();
  return data;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makePointCloud(const ResponseCase& data)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(data.width);
  cloud->height = static_cast<std::uint32_t>(data.height);
  cloud->is_dense = false;
  cloud->points.assign(data.points.begin(), data.points.end());
  return cloud;
}

pcl::PointCloud<pcl::Normal>::Ptr
makeNormalCloud(const ResponseCase& data)
{
  auto normals = pcl::make_shared<pcl::PointCloud<pcl::Normal>>();
  normals->width = static_cast<std::uint32_t>(data.width);
  normals->height = static_cast<std::uint32_t>(data.height);
  normals->is_dense = false;
  normals->points.assign(data.normals.begin(), data.normals.end());
  return normals;
}

pcl::Indices
referenceKeypointIndices(const std::vector<float>& response,
                         const std::size_t width,
                         const std::size_t height,
                         const int half_window,
                         const float second_threshold)
{
  pcl::Indices indices(response.size());
  for (std::size_t i = 0; i < indices.size(); ++i)
    indices[i] = static_cast<int>(i);

  std::sort(indices.begin(), indices.end(), [&response](int a, int b) {
    return response[static_cast<std::size_t>(a)] > response[static_cast<std::size_t>(b)];
  });

  std::vector<bool> occupency_map(indices.size(), false);
  pcl::Indices selected;
  selected.reserve(indices.size());
  const int w = static_cast<int>(width);
  const int h = static_cast<int>(height);
  for (const int idx : indices)
  {
    if (response[static_cast<std::size_t>(idx)] < second_threshold || occupency_map[static_cast<std::size_t>(idx)])
      continue;
    selected.push_back(idx);

    const int x = idx % w;
    const int y = idx / w;
    const int u_end = std::min(w, x + half_window);
    const int v_end = std::min(h, y + half_window);
    for (int v = std::max(0, y - half_window); v < v_end; ++v)
      for (int u = std::max(0, x - half_window); u < u_end; ++u)
        occupency_map[static_cast<std::size_t>(v * w + u)] = true;
  }
  return selected;
}

void
expectPublicOutputMatchesScalarNMS(const ResponseCase& data, const Detector::ComputationMethod method)
{
  std::vector<float> response(data.width * data.height, 0.0f);
  constexpr float kFirstThreshold = 0.00046f;
  constexpr float kSecondThreshold = 0.0005f;
  const t3d::ResponseConfig config{data.width, data.height, 1, kFirstThreshold};
  if (method == Detector::FOUR_CORNERS)
    t3d::computeFourCornersResponseStd(data.points.data(), data.normals.data(), config, response.data());
  else
    t3d::computeEightCornersResponseStd(data.points.data(), data.normals.data(), config, response.data());
  const pcl::Indices expected = referenceKeypointIndices(response, data.width, data.height, 1, kSecondThreshold);

  Detector detector(method, 3, kFirstThreshold, kSecondThreshold);
  detector.setNumberOfThreads(1);
  detector.setInputCloud(makePointCloud(data));
  detector.setNormals(makeNormalCloud(data));

  pcl::PointCloud<pcl::PointXYZI> output;
  detector.compute(output);
  ASSERT_EQ(output.size(), detector.getKeypointsIndices()->indices.size());
  ASSERT_EQ(expected.size(), detector.getKeypointsIndices()->indices.size());

  for (std::size_t i = 0; i < expected.size(); ++i)
  {
    const int index = detector.getKeypointsIndices()->indices[i];
    EXPECT_EQ(expected[i], index) << "output_index=" << i;
    EXPECT_NEAR(response[static_cast<std::size_t>(index)], output[i].intensity, 1e-5f)
        << "output_index=" << i << " source_index=" << index;
  }
}
} // namespace

TEST(Trajkovic3DFourCornersResponse, MatchesScalarForInteriorFiniteAndInvalidNormals)
{
  const ResponseCase data = makeCase(37, 29);
  std::vector<float> scalar(data.width * data.height, 0.0f);
  std::vector<float> rvv(data.width * data.height, 0.0f);

  const t3d::ResponseConfig config{data.width, data.height, 1, 0.00046f};
  t3d::computeFourCornersResponseStd(data.points.data(), data.normals.data(), config, scalar.data());
  t3d::computeFourCornersResponseRVV(data.points.data(), data.normals.data(), config, rvv.data());

  ASSERT_EQ(scalar.size(), rvv.size());
  for (std::size_t index = 0; index < scalar.size(); ++index)
    EXPECT_NEAR(scalar[index], rvv[index], 1e-6f) << "index=" << index;
}

TEST(Trajkovic3DFourCornersResponse, TreatsInvalidNeighborNormalsAsNullNormals)
{
  ResponseCase data = makeCase(23, 11);
  constexpr std::size_t kRow = 5;
  constexpr std::size_t kCol = 7;
  const std::size_t up = (kRow - 1) * data.width + kCol;
  const std::size_t down = (kRow + 1) * data.width + kCol;
  data.normals[up].normal_x = std::numeric_limits<float>::quiet_NaN();
  data.normals[down].normal_y = std::numeric_limits<float>::infinity();

  std::vector<float> scalar(data.width * data.height, 0.0f);
  std::vector<float> rvv(data.width * data.height, 0.0f);

  const t3d::ResponseConfig config{data.width, data.height, 1, 0.0f};
  t3d::computeFourCornersResponseStd(data.points.data(), data.normals.data(), config, scalar.data());
  t3d::computeFourCornersResponseRVV(data.points.data(), data.normals.data(), config, rvv.data());

  const std::size_t center = kRow * data.width + kCol;
  ASSERT_TRUE(std::isfinite(scalar[center]));
  EXPECT_NEAR(scalar[center], rvv[center], 1e-6f) << "center=" << center;
}

TEST(Trajkovic3DFourCornersResponse, LeavesBorderAndRejectedResponsesZero)
{
  const ResponseCase data = makeCase(19, 17);
  std::vector<float> scalar(data.width * data.height, 42.0f);
  std::vector<float> rvv(data.width * data.height, 42.0f);

  const t3d::ResponseConfig config{data.width, data.height, 2, 10.0f};
  t3d::computeFourCornersResponseStd(data.points.data(), data.normals.data(), config, scalar.data());
  t3d::computeFourCornersResponseRVV(data.points.data(), data.normals.data(), config, rvv.data());

  for (std::size_t row = 0; row < data.height; ++row)
  {
    for (std::size_t col = 0; col < data.width; ++col)
    {
      if (row < 2 || col < 2 || row + 2 >= data.height || col + 2 >= data.width)
      {
        const std::size_t index = row * data.width + col;
        EXPECT_FLOAT_EQ(0.0f, scalar[index]) << "scalar border index=" << index;
        EXPECT_FLOAT_EQ(0.0f, rvv[index]) << "rvv border index=" << index;
      }
    }
  }
}

TEST(Trajkovic3DEightCornersResponse, MatchesScalarForInteriorFiniteAndInvalidNormals)
{
  const ResponseCase data = makeCase(37, 29);
  std::vector<float> scalar(data.width * data.height, 0.0f);
  std::vector<float> rvv(data.width * data.height, 0.0f);

  const t3d::ResponseConfig config{data.width, data.height, 1, 0.00046f};
  t3d::computeEightCornersResponseStd(data.points.data(), data.normals.data(), config, scalar.data());
  t3d::computeEightCornersResponseRVV(data.points.data(), data.normals.data(), config, rvv.data());

  ASSERT_EQ(scalar.size(), rvv.size());
  for (std::size_t index = 0; index < scalar.size(); ++index)
    EXPECT_NEAR(scalar[index], rvv[index], 1e-6f) << "index=" << index;
}

TEST(Trajkovic3DEightCornersResponse, LeavesBorderAndRejectedResponsesZero)
{
  const ResponseCase data = makeCase(21, 19);
  std::vector<float> scalar(data.width * data.height, 42.0f);
  std::vector<float> rvv(data.width * data.height, 42.0f);

  const t3d::ResponseConfig config{data.width, data.height, 1, 10.0f};
  t3d::computeEightCornersResponseStd(data.points.data(), data.normals.data(), config, scalar.data());
  t3d::computeEightCornersResponseRVV(data.points.data(), data.normals.data(), config, rvv.data());

  for (std::size_t row = 0; row < data.height; ++row)
  {
    for (std::size_t col = 0; col < data.width; ++col)
    {
      if (row < 1 || col < 1 || row + 1 >= data.height || col + 1 >= data.width)
      {
        const std::size_t index = row * data.width + col;
        EXPECT_FLOAT_EQ(0.0f, scalar[index]) << "scalar border index=" << index;
        EXPECT_FLOAT_EQ(0.0f, rvv[index]) << "rvv border index=" << index;
      }
    }
  }
}

TEST(Trajkovic3DProductionRVV, FourCornersGateCoversPointXYZAndNormal)
{
  EXPECT_TRUE((pcl::detail::trajkovic3DResponseRVVSupported<pcl::PointXYZ, pcl::Normal>));
  EXPECT_TRUE((pcl::detail::trajkovic3DFourCornersRVVSupported<pcl::PointXYZ, pcl::Normal>));
  EXPECT_TRUE((pcl::detail::trajkovic3DEightCornersRVVSupported<pcl::PointXYZ, pcl::Normal>));
}

TEST(Trajkovic3DProductionRVV, PublicComputeFourCornersMatchesResponseReference)
{
  const ResponseCase data = makeCase(41, 31);
  std::vector<float> response(data.width * data.height, 0.0f);

  const t3d::ResponseConfig config{data.width, data.height, 1, 0.00046f};
  t3d::computeFourCornersResponseStd(data.points.data(), data.normals.data(), config, response.data());

  Detector detector(Detector::FOUR_CORNERS, 3, 0.00046f, 0.0005f);
  detector.setNumberOfThreads(1);
  detector.setInputCloud(makePointCloud(data));
  detector.setNormals(makeNormalCloud(data));

  pcl::PointCloud<pcl::PointXYZI> output;
  detector.compute(output);

  ASSERT_EQ(output.size(), detector.getKeypointsIndices()->indices.size());
  for (std::size_t i = 0; i < output.size(); ++i)
  {
    const int index = detector.getKeypointsIndices()->indices[i];
    ASSERT_GE(index, 0);
    ASSERT_LT(static_cast<std::size_t>(index), response.size());
    EXPECT_NEAR(response[static_cast<std::size_t>(index)], output[i].intensity, 1e-5f)
        << "output_index=" << i << " source_index=" << index;
  }
}

TEST(Trajkovic3DProductionRVV, PublicComputeInvalidAndTailMatchesResponseReference)
{
  const ResponseCase data = makeCase(641, 481);
  expectPublicOutputMatchesScalarNMS(data, Detector::FOUR_CORNERS);
}

TEST(Trajkovic3DProductionRVV, PublicComputeEightCornersInvalidAndTailMatchesResponseReference)
{
  const ResponseCase data = makeCase(641, 481);
  expectPublicOutputMatchesScalarNMS(data, Detector::EIGHT_CORNERS);
}

TEST(Trajkovic3DProductionRVV, PublicComputeTinyCloudFallsBackWithoutResponses)
{
  const ResponseCase data = makeCase(2, 2);

  for (const auto method : {Detector::FOUR_CORNERS, Detector::EIGHT_CORNERS})
  {
    Detector detector(method, 3, 0.00046f, 0.0005f);
    detector.setNumberOfThreads(1);
    detector.setInputCloud(makePointCloud(data));
    detector.setNormals(makeNormalCloud(data));

    pcl::PointCloud<pcl::PointXYZI> output;
    detector.compute(output);

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(detector.getKeypointsIndices()->indices.empty());
  }
}
