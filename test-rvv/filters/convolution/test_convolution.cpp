#include <pcl/filters/convolution.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

Eigen::ArrayXf makeKernel7()
{
  Eigen::ArrayXf kernel(7);
  kernel << 0.00443305f, 0.0540056f, 0.242036f, 0.39905f, 0.242036f, 0.0540056f, 0.00443305f;
  return kernel;
}

pcl::PointCloud<pcl::PointXYZI>::Ptr makeCloud(const std::uint32_t width,
                                               const std::uint32_t height,
                                               const bool dense)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZI>>();
  cloud->width = width;
  cloud->height = height;
  cloud->is_dense = dense;
  cloud->resize(width * height);
  for (std::uint32_t r = 0; r < height; ++r)
  {
    for (std::uint32_t c = 0; c < width; ++c)
    {
      auto& p = (*cloud)(c, r);
      p.x = 0.25f * static_cast<float>(c) + 0.125f * static_cast<float>(r);
      p.y = -0.5f * static_cast<float>(c) + 0.25f * static_cast<float>(r);
      p.z = (c % 5 == 0 ? 2.0f : -1.0f) + 0.03125f * static_cast<float>(r);
      p.intensity = 10.0f + 0.75f * static_cast<float>(c) - 0.5f * static_cast<float>(r);
    }
  }
  if (!dense && width > 10 && height > 4)
  {
    (*cloud)(5, 3).x = std::numeric_limits<float>::quiet_NaN();
    (*cloud)(8, 4).z = std::numeric_limits<float>::quiet_NaN();
  }
  return cloud;
}

pcl::PointXYZI zeroPoint()
{
  pcl::PointXYZI p;
  p.x = p.y = p.z = p.intensity = 0.0f;
  return p;
}

pcl::PointCloud<pcl::PointXYZI> runRows(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& input,
                                        const Eigen::ArrayXf& kernel,
                                        const int border_policy = pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_IGNORE)
{
  pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI> convolution;
  convolution.setInputCloud(input);
  convolution.setKernel(kernel);
  convolution.setBordersPolicy(border_policy);
  pcl::PointCloud<pcl::PointXYZI> output;
  convolution.convolveRows(output);
  return output;
}

pcl::PointCloud<pcl::PointXYZI> runCols(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& input,
                                        const Eigen::ArrayXf& kernel,
                                        const int border_policy = pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_IGNORE)
{
  pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI> convolution;
  convolution.setInputCloud(input);
  convolution.setKernel(kernel);
  convolution.setBordersPolicy(border_policy);
  pcl::PointCloud<pcl::PointXYZI> output;
  convolution.convolveCols(output);
  return output;
}

pcl::PointCloud<pcl::PointXYZI> referenceRows(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& input,
                                             const Eigen::ArrayXf& kernel)
{
  pcl::PointCloud<pcl::PointXYZI> output;
  output.width = input->width;
  output.height = input->height;
  output.is_dense = input->is_dense;
  output.resize(input->size());
  const int half_width = static_cast<int>(kernel.size()) / 2;
  const int kernel_width = static_cast<int>(kernel.size()) - 1;
  const int last = static_cast<int>(input->width) - half_width;
  for (std::uint32_t r = 0; r < input->height; ++r)
  {
    for (int c = 0; c < static_cast<int>(input->width); ++c)
    {
      auto& out = output(static_cast<std::uint32_t>(c), r);
      if (c < half_width || c >= last)
      {
        out.x = out.y = out.z = std::numeric_limits<float>::quiet_NaN();
        out.intensity = 0.0f;
        continue;
      }
      pcl::PointXYZI result = zeroPoint();
      for (int k = kernel_width, l = c - half_width; k > -1; --k, ++l)
      {
        const auto& in = (*input)(static_cast<std::uint32_t>(l), r);
        result.x += in.x * kernel[k];
        result.y += in.y * kernel[k];
        result.z += in.z * kernel[k];
        result.intensity += in.intensity * kernel[k];
      }
      out = result;
    }
  }
  return output;
}

pcl::PointCloud<pcl::PointXYZI> referenceCols(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& input,
                                             const Eigen::ArrayXf& kernel)
{
  pcl::PointCloud<pcl::PointXYZI> output;
  output.width = input->width;
  output.height = input->height;
  output.is_dense = input->is_dense;
  output.resize(input->size());
  const int half_width = static_cast<int>(kernel.size()) / 2;
  const int kernel_width = static_cast<int>(kernel.size()) - 1;
  const int last = static_cast<int>(input->height) - half_width;
  for (int r = 0; r < static_cast<int>(input->height); ++r)
  {
    for (std::uint32_t c = 0; c < input->width; ++c)
    {
      auto& out = output(c, static_cast<std::uint32_t>(r));
      if (r < half_width || r >= last)
      {
        out.x = out.y = out.z = std::numeric_limits<float>::quiet_NaN();
        out.intensity = 0.0f;
        continue;
      }
      pcl::PointXYZI result = zeroPoint();
      for (int k = kernel_width, l = r - half_width; k > -1; --k, ++l)
      {
        const auto& in = (*input)(c, static_cast<std::uint32_t>(l));
        result.x += in.x * kernel[k];
        result.y += in.y * kernel[k];
        result.z += in.z * kernel[k];
        result.intensity += in.intensity * kernel[k];
      }
      out = result;
    }
  }
  return output;
}

void applyRowsDuplicateBorder(pcl::PointCloud<pcl::PointXYZI>& output, const int half_width)
{
  const int last = static_cast<int>(output.width) - half_width;
  const int w = last - 1;
  for (std::uint32_t r = 0; r < output.height; ++r)
  {
    for (int c = last; c < static_cast<int>(output.width); ++c)
      output(static_cast<std::uint32_t>(c), r) = output(static_cast<std::uint32_t>(w), r);
    for (int c = 0; c < half_width; ++c)
      output(static_cast<std::uint32_t>(c), r) = output(static_cast<std::uint32_t>(half_width), r);
  }
}

void applyRowsMirrorBorder(pcl::PointCloud<pcl::PointXYZI>& output, const int half_width)
{
  const int last = static_cast<int>(output.width) - half_width;
  const int w = last - 1;
  for (std::uint32_t r = 0; r < output.height; ++r)
  {
    for (int c = last, l = 0; c < static_cast<int>(output.width); ++c, ++l)
      output(static_cast<std::uint32_t>(c), r) = output(static_cast<std::uint32_t>(w - l), r);
    for (int c = 0; c < half_width; ++c)
      output(static_cast<std::uint32_t>(c), r) = output(static_cast<std::uint32_t>(half_width + 1 - c), r);
  }
}

void clearRowsBorder(pcl::PointCloud<pcl::PointXYZI>& output, const int half_width)
{
  const int last = static_cast<int>(output.width) - half_width;
  for (std::uint32_t r = 0; r < output.height; ++r)
    for (std::uint32_t c = 0; c < output.width; ++c)
      if (static_cast<int>(c) < half_width || static_cast<int>(c) >= last)
        output(c, r) = zeroPoint();
}

void applyColsDuplicateBorder(pcl::PointCloud<pcl::PointXYZI>& output, const int half_width)
{
  const int last = static_cast<int>(output.height) - half_width;
  const int h = last - 1;
  for (std::uint32_t c = 0; c < output.width; ++c)
  {
    for (int r = last; r < static_cast<int>(output.height); ++r)
      output(c, static_cast<std::uint32_t>(r)) = output(c, static_cast<std::uint32_t>(h));
    for (int r = 0; r < half_width; ++r)
      output(c, static_cast<std::uint32_t>(r)) = output(c, static_cast<std::uint32_t>(half_width));
  }
}

void applyColsMirrorBorder(pcl::PointCloud<pcl::PointXYZI>& output, const int half_width)
{
  const int last = static_cast<int>(output.height) - half_width;
  const int h = last - 1;
  for (std::uint32_t c = 0; c < output.width; ++c)
  {
    for (int r = last, l = 0; r < static_cast<int>(output.height); ++r, ++l)
      output(c, static_cast<std::uint32_t>(r)) = output(c, static_cast<std::uint32_t>(h - l));
    for (int r = 0; r < half_width; ++r)
      output(c, static_cast<std::uint32_t>(r)) = output(c, static_cast<std::uint32_t>(half_width + 1 - r));
  }
}

void clearColsBorder(pcl::PointCloud<pcl::PointXYZI>& output, const int half_width)
{
  const int last = static_cast<int>(output.height) - half_width;
  for (std::uint32_t r = 0; r < output.height; ++r)
    for (std::uint32_t c = 0; c < output.width; ++c)
      if (static_cast<int>(r) < half_width || static_cast<int>(r) >= last)
        output(c, r) = zeroPoint();
}

pcl::PointCloud<pcl::PointXYZI> referenceRowsWithPolicy(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& input,
                                                        const Eigen::ArrayXf& kernel,
                                                        const int border_policy)
{
  auto output = referenceRows(input, kernel);
  const int half_width = static_cast<int>(kernel.size()) / 2;
  if (border_policy == pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE)
    applyRowsDuplicateBorder(output, half_width);
  else if (border_policy == pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR)
  {
    clearRowsBorder(output, half_width);
    applyRowsMirrorBorder(output, half_width);
  }
  return output;
}

pcl::PointCloud<pcl::PointXYZI> referenceColsWithPolicy(const pcl::PointCloud<pcl::PointXYZI>::ConstPtr& input,
                                                        const Eigen::ArrayXf& kernel,
                                                        const int border_policy)
{
  auto output = referenceCols(input, kernel);
  const int half_width = static_cast<int>(kernel.size()) / 2;
  if (border_policy == pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE)
    applyColsDuplicateBorder(output, half_width);
  else if (border_policy == pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR)
  {
    clearColsBorder(output, half_width);
    applyColsMirrorBorder(output, half_width);
  }
  return output;
}

bool nearlyEqual(const float a, const float b, const float tol)
{
  if (std::isnan(a) || std::isnan(b))
    return std::isnan(a) && std::isnan(b);
  return std::fabs(a - b) <= tol;
}

bool compareClouds(const pcl::PointCloud<pcl::PointXYZI>& actual,
                   const pcl::PointCloud<pcl::PointXYZI>& expected,
                   const float tol,
                   const std::string& label)
{
  if (actual.width != expected.width || actual.height != expected.height || actual.size() != expected.size())
  {
    std::cerr << label << " size mismatch\n";
    return false;
  }
  for (std::size_t i = 0; i < actual.size(); ++i)
  {
    const auto& a = actual[i];
    const auto& e = expected[i];
    if (!nearlyEqual(a.x, e.x, tol) || !nearlyEqual(a.y, e.y, tol) ||
        !nearlyEqual(a.z, e.z, tol) || !nearlyEqual(a.intensity, e.intensity, tol))
    {
      std::cerr << label << " mismatch at " << i
                << " actual=(" << a.x << "," << a.y << "," << a.z << "," << a.intensity << ")"
                << " expected=(" << e.x << "," << e.y << "," << e.z << "," << e.intensity << ")\n";
      return false;
    }
  }
  return true;
}

bool expectBorderNaN(const pcl::PointCloud<pcl::PointXYZI>& cloud,
                     const int half_width,
                     const bool rows,
                     const std::string& label)
{
  if (rows)
  {
    const int last = static_cast<int>(cloud.width) - half_width;
    for (std::uint32_t r = 0; r < cloud.height; ++r)
      for (std::uint32_t c = 0; c < cloud.width; ++c)
        if (static_cast<int>(c) < half_width || static_cast<int>(c) >= last)
        {
          const auto& p = cloud(c, r);
          if (!std::isnan(p.x) || !std::isnan(p.y) || !std::isnan(p.z))
          {
            std::cerr << label << " row border not NaN at (" << c << "," << r << ")\n";
            return false;
          }
        }
  }
  else
  {
    const int last = static_cast<int>(cloud.height) - half_width;
    for (std::uint32_t r = 0; r < cloud.height; ++r)
      for (std::uint32_t c = 0; c < cloud.width; ++c)
        if (static_cast<int>(r) < half_width || static_cast<int>(r) >= last)
        {
          const auto& p = cloud(c, r);
          if (!std::isnan(p.x) || !std::isnan(p.y) || !std::isnan(p.z))
          {
            std::cerr << label << " col border not NaN at (" << c << "," << r << ")\n";
            return false;
          }
        }
  }
  return true;
}

constexpr float kTolerance = 1e-4f;

} // namespace

TEST(ConvolutionPointXYZI, DenseRowsIgnoreBoundary)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(96, 48, true);
  const auto output = runRows(input, kernel);
  const auto reference = referenceRows(input, kernel);
  EXPECT_TRUE(compareClouds(output, reference, kTolerance, "dense rows"));
  EXPECT_TRUE(expectBorderNaN(output, static_cast<int>(kernel.size()) / 2, true, "dense rows"));
}

TEST(ConvolutionPointXYZI, DenseColsIgnoreBoundary)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(96, 64, true);
  const auto output = runCols(input, kernel);
  const auto reference = referenceCols(input, kernel);
  EXPECT_TRUE(compareClouds(output, reference, kTolerance, "dense cols"));
  EXPECT_TRUE(expectBorderNaN(output, static_cast<int>(kernel.size()) / 2, false, "dense cols"));
}

TEST(ConvolutionPointXYZI, DenseRowsDuplicateBoundary)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(96, 48, true);
  const auto policy = pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE;
  const auto output = runRows(input, kernel, policy);
  const auto reference = referenceRowsWithPolicy(input, kernel, policy);
  EXPECT_TRUE(compareClouds(output, reference, kTolerance, "dense rows duplicate"));
}

TEST(ConvolutionPointXYZI, DenseRowsMirrorBoundary)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(96, 48, true);
  const auto policy = pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR;
  const auto output = runRows(input, kernel, policy);
  const auto reference = referenceRowsWithPolicy(input, kernel, policy);
  EXPECT_TRUE(compareClouds(output, reference, kTolerance, "dense rows mirror"));
}

TEST(ConvolutionPointXYZI, DenseColsDuplicateBoundary)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(96, 64, true);
  const auto policy = pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_DUPLICATE;
  const auto output = runCols(input, kernel, policy);
  const auto reference = referenceColsWithPolicy(input, kernel, policy);
  EXPECT_TRUE(compareClouds(output, reference, kTolerance, "dense cols duplicate"));
}

TEST(ConvolutionPointXYZI, DenseColsMirrorBoundary)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(96, 64, true);
  const auto policy = pcl::filters::Convolution<pcl::PointXYZI, pcl::PointXYZI>::BORDERS_POLICY_MIRROR;
  const auto output = runCols(input, kernel, policy);
  const auto reference = referenceColsWithPolicy(input, kernel, policy);
  EXPECT_TRUE(compareClouds(output, reference, kTolerance, "dense cols mirror"));
}

TEST(ConvolutionPointXYZI, SmallRowsFallbackKeepsBorders)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(12, 8, true);
  const auto output = runRows(input, kernel);
  EXPECT_TRUE(expectBorderNaN(output, static_cast<int>(kernel.size()) / 2, true, "small rows fallback"));
}

TEST(ConvolutionPointXYZI, NonDenseRowsFallbackRunsScalarSemantics)
{
  const auto kernel = makeKernel7();
  const auto input = makeCloud(64, 32, false);
  const auto output = runRows(input, kernel);

  EXPECT_EQ(output.width, input->width);
  EXPECT_EQ(output.height, input->height);
  EXPECT_FALSE(output.is_dense);
  EXPECT_TRUE(expectBorderNaN(output, static_cast<int>(kernel.size()) / 2, true, "non-dense rows fallback"));
  EXPECT_TRUE(std::isfinite(output(20, 3).x));
  EXPECT_TRUE(std::isfinite(output(20, 3).y));
  EXPECT_TRUE(std::isfinite(output(20, 3).z));
}

int main(int argc, char** argv)
{

#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
