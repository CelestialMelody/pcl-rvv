/*
 * 本文件做什么：
 * 这里验证 `io/src/oni_grabber.cpp` 里的 production detail helper
 *（生产内部 helper）是否和标量链路 bitwise（逐 bit）一致。测试不构造
 * ONIGrabber 对象，因为当前目标是生产内部 depth-only `PointXYZ` helper；
 * 它直接调用 test hook 暴露的生产源码 helper，作为 PI3 的 production-detail
 * evidence（生产内部边界证据）。
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include <pcl/point_types.h>

extern "C" {
void
pcl_rvv_oni_grabber_reset_test_hook ();
int
pcl_rvv_oni_grabber_last_test_hook ();
void
pcl_rvv_oni_grabber_fill_xyz_std_test_hook (const std::uint16_t* depth,
                                            unsigned width,
                                            unsigned height,
                                            float constant,
                                            int center_x,
                                            int center_y,
                                            std::uint64_t no_sample_value,
                                            std::uint64_t shadow_value,
                                            pcl::PointXYZ* cloud);
void
pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook (const std::uint16_t* depth,
                                                  unsigned width,
                                                  unsigned height,
                                                  float constant,
                                                  int center_x,
                                                  int center_y,
                                                  std::uint64_t no_sample_value,
                                                  std::uint64_t shadow_value,
                                                  pcl::PointXYZ* cloud);
}

namespace {

enum class ExpectedHookPath {
  None = 0,
  Scalar = 1,
  Rvv = 2,
};

std::uint32_t
floatBits (float value)
{
  std::uint32_t bits = 0;
  std::memcpy (&bits, &value, sizeof (bits));
  return bits;
}

void
expectSameXYZBits (const std::vector<pcl::PointXYZ>& reference,
                   const std::vector<pcl::PointXYZ>& candidate)
{
  ASSERT_EQ (reference.size (), candidate.size ());
  for (std::size_t i = 0; i < reference.size (); ++i)
  {
    EXPECT_EQ (floatBits (reference[i].x), floatBits (candidate[i].x)) << "x index=" << i;
    EXPECT_EQ (floatBits (reference[i].y), floatBits (candidate[i].y)) << "y index=" << i;
    EXPECT_EQ (floatBits (reference[i].z), floatBits (candidate[i].z)) << "z index=" << i;
  }
}

std::vector<std::uint16_t>
makeDepthFrame (unsigned width, unsigned height)
{
  std::vector<std::uint16_t> depth (static_cast<std::size_t> (width) * height);
  for (unsigned y = 0; y < height; ++y)
  {
    for (unsigned x = 0; x < width; ++x)
    {
      const auto index = static_cast<std::size_t> (y) * width + x;
      depth[index] = static_cast<std::uint16_t> (600 + ((x * 17 + y * 37) % 5000));
      if ((x + y * 3) % 19 == 0)
        depth[index] = 0;
      if ((x * 5 + y) % 23 == 0)
        depth[index] = 2047;
      if ((x + y * 11) % 29 == 0)
        depth[index] = 65535;
    }
  }
  return depth;
}

} // namespace

TEST (ONIGrabberProductionDetail, CandidateMatchesStdBitwiseOnDepthFrame)
{
  const unsigned width = 54;
  const unsigned height = 18;
  const auto depth = makeDepthFrame (width, height);
  std::vector<pcl::PointXYZ> reference (depth.size ());
  std::vector<pcl::PointXYZ> candidate (depth.size ());

  pcl_rvv_oni_grabber_fill_xyz_std_test_hook (depth.data (),
                                              width,
                                              height,
                                              1.0f / 525.0f,
                                              static_cast<int> (width >> 1),
                                              static_cast<int> (height >> 1),
                                              2047,
                                              65535,
                                              reference.data ());
  pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook (depth.data (),
                                                    width,
                                                    height,
                                                    1.0f / 525.0f,
                                                    static_cast<int> (width >> 1),
                                                    static_cast<int> (height >> 1),
                                                    2047,
                                                    65535,
                                                    candidate.data ());

  expectSameXYZBits (reference, candidate);
}

TEST (ONIGrabberProductionDetail, CandidateRecordsScalarOrRvvPath)
{
  const unsigned width = 32;
  const unsigned height = 8;
  const auto depth = makeDepthFrame (width, height);
  std::vector<pcl::PointXYZ> cloud (depth.size ());

  pcl_rvv_oni_grabber_reset_test_hook ();
  pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook (depth.data (),
                                                    width,
                                                    height,
                                                    1.0f / 525.0f,
                                                    static_cast<int> (width >> 1),
                                                    static_cast<int> (height >> 1),
                                                    2047,
                                                    65535,
                                                    cloud.data ());

#if defined(__RVV10__)
  EXPECT_EQ (pcl_rvv_oni_grabber_last_test_hook (),
             static_cast<int> (ExpectedHookPath::Rvv));
#else
  EXPECT_EQ (pcl_rvv_oni_grabber_last_test_hook (),
             static_cast<int> (ExpectedHookPath::Scalar));
#endif
}

TEST (ONIGrabberProductionDetail, CandidateFallsBackForOddDimensions)
{
  const unsigned width = 7;
  const unsigned height = 5;
  const auto depth = makeDepthFrame (width, height);
  std::vector<pcl::PointXYZ> reference (depth.size ());
  std::vector<pcl::PointXYZ> candidate (depth.size ());

  pcl_rvv_oni_grabber_fill_xyz_std_test_hook (depth.data (),
                                              width,
                                              height,
                                              1.0f / 525.0f,
                                              static_cast<int> (width >> 1),
                                              static_cast<int> (height >> 1),
                                              2047,
                                              65535,
                                              reference.data ());

  pcl_rvv_oni_grabber_reset_test_hook ();
  pcl_rvv_oni_grabber_fill_xyz_candidate_test_hook (depth.data (),
                                                    width,
                                                    height,
                                                    1.0f / 525.0f,
                                                    static_cast<int> (width >> 1),
                                                    static_cast<int> (height >> 1),
                                                    2047,
                                                    65535,
                                                    candidate.data ());

  expectSameXYZBits (reference, candidate);
  EXPECT_EQ (pcl_rvv_oni_grabber_last_test_hook (),
             static_cast<int> (ExpectedHookPath::Scalar));
}
