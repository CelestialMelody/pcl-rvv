#include <pcl/filters/conditional_removal.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

pcl::PointCloud<pcl::PointXYZ>
makeCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 4099) - 2049) / 1400.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 7) % 4093) - 2046) / 1500.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 13) % 4091) - 2045) / 1600.0f;
  }
  return cloud;
}

pcl::PointCloud<pcl::PointXYZI>
makeCloudXYZI(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZI> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    cloud[i].x = static_cast<float>(static_cast<int>(i % 257) - 128) / 95.0f;
    cloud[i].y = static_cast<float>(static_cast<int>((i * 5) % 263) - 131) / 105.0f;
    cloud[i].z = static_cast<float>(static_cast<int>((i * 11) % 269) - 134) / 115.0f;
    cloud[i].intensity = static_cast<float>(i % 17);
  }
  return cloud;
}

pcl::ConditionAnd<pcl::PointXYZ>::Ptr
singleFieldCondition(const std::string& field, pcl::ComparisonOps::CompareOp op, double value)
{
  pcl::ConditionAnd<pcl::PointXYZ>::Ptr condition(new pcl::ConditionAnd<pcl::PointXYZ>());
  condition->addComparison(pcl::FieldComparison<pcl::PointXYZ>::ConstPtr(
      new pcl::FieldComparison<pcl::PointXYZ>(field, op, value)));
  return condition;
}

pcl::Indices
expectedSingleField(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                    const pcl::Indices* subset,
                    const std::string& field,
                    pcl::ComparisonOps::CompareOp op,
                    float value)
{
  pcl::Indices expected;
  const std::size_t n = subset ? subset->size() : cloud.size();
  expected.reserve(n);
  for (std::size_t pos = 0; pos < n; ++pos) {
    const int index = subset ? (*subset)[pos] : static_cast<int>(pos);
    const auto& p = cloud[index];
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
      continue;
    const float field_value = field == "x" ? p.x : (field == "y" ? p.y : p.z);
    bool keep = false;
    switch (op) {
      case pcl::ComparisonOps::GT: keep = field_value > value; break;
      case pcl::ComparisonOps::GE: keep = field_value >= value; break;
      case pcl::ComparisonOps::LT: keep = field_value < value; break;
      case pcl::ComparisonOps::LE: keep = field_value <= value; break;
      case pcl::ComparisonOps::EQ:
        keep = !((field_value > value) || (field_value < value));
        break;
    }
    if (keep)
      expected.push_back(index);
  }
  return expected;
}

pcl::Indices
outputToInputIndices(const pcl::PointCloud<pcl::PointXYZ>& input,
                     const pcl::PointCloud<pcl::PointXYZ>& output)
{
  pcl::Indices indices;
  indices.reserve(output.size());
  std::size_t search_from = 0;
  for (const auto& point : output) {
    for (std::size_t i = search_from; i < input.size(); ++i) {
      if (input[i].x == point.x && input[i].y == point.y && input[i].z == point.z) {
        indices.push_back(static_cast<int>(i));
        search_from = i + 1;
        break;
      }
    }
  }
  return indices;
}

} // namespace

TEST(ConditionalRemovalRVV, LargeSingleFieldGTKeepsOrder)
{
  const auto cloud = makeCloud(513);
  pcl::ConditionalRemoval<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(singleFieldCondition("z", pcl::ComparisonOps::GT, 0.12));

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  const auto expected = expectedSingleField(cloud, nullptr, "z", pcl::ComparisonOps::GT, 0.12f);
  EXPECT_EQ(outputToInputIndices(cloud, output), expected);
  EXPECT_EQ(output.size() + filter.getRemovedIndices()->size(), cloud.size());
  EXPECT_TRUE(output.is_dense);
}

TEST(ConditionalRemovalRVV, NonFiniteXYZIsRemovedBeforeCondition)
{
  auto cloud = makeCloud(257);
  cloud[3].x = std::numeric_limits<float>::quiet_NaN();
  cloud[67].y = std::numeric_limits<float>::infinity();
  cloud[129].z = -std::numeric_limits<float>::infinity();
  pcl::ConditionalRemoval<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(singleFieldCondition("x", pcl::ComparisonOps::LE, 0.25));

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  EXPECT_EQ(outputToInputIndices(cloud, output), expectedSingleField(cloud, nullptr, "x", pcl::ComparisonOps::LE, 0.25f));
  EXPECT_EQ(output.size() + filter.getRemovedIndices()->size(), cloud.size());
}

TEST(ConditionalRemovalRVV, KeepOrganizedFallbackPreservesShape)
{
  const auto cloud = makeCloud(257);
  pcl::ConditionalRemoval<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(singleFieldCondition("y", pcl::ComparisonOps::GE, -0.2));
  filter.setKeepOrganized(true);

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  EXPECT_EQ(output.width, cloud.width);
  EXPECT_EQ(output.height, cloud.height);
  EXPECT_EQ(output.size(), cloud.size());
}

TEST(ConditionalRemovalRVV, ExplicitIndicesFallbackPreservesSubset)
{
  const auto cloud = makeCloud(257);
  pcl::IndicesPtr subset(new pcl::Indices{3, 5, 7, 97, 111, 131, 173, 199, 201});
  pcl::ConditionalRemoval<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setIndices(subset);
  filter.setCondition(singleFieldCondition("z", pcl::ComparisonOps::LT, 0.1));

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  EXPECT_EQ(outputToInputIndices(cloud, output), expectedSingleField(cloud, subset.get(), "z", pcl::ComparisonOps::LT, 0.1f));
}

TEST(ConditionalRemovalRVV, CompoundConditionFallbackPreservesScalarLogic)
{
  const auto cloud = makeCloud(257);
  pcl::ConditionAnd<pcl::PointXYZ>::Ptr condition(new pcl::ConditionAnd<pcl::PointXYZ>());
  condition->addComparison(pcl::FieldComparison<pcl::PointXYZ>::ConstPtr(
      new pcl::FieldComparison<pcl::PointXYZ>("z", pcl::ComparisonOps::GT, 0.05)));
  condition->addComparison(pcl::FieldComparison<pcl::PointXYZ>::ConstPtr(
      new pcl::FieldComparison<pcl::PointXYZ>("y", pcl::ComparisonOps::LT, 0.2)));
  pcl::ConditionalRemoval<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(condition);

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  for (const auto& point : output) {
    EXPECT_GT(point.z, 0.05f);
    EXPECT_LT(point.y, 0.2f);
  }
}

TEST(ConditionalRemovalRVV, EQFallbackKeepsScalarNaNFieldSemantics)
{
  auto cloud = makeCloud(257);
  cloud[42].z = std::numeric_limits<float>::quiet_NaN();
  pcl::ConditionalRemoval<pcl::PointXYZ> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(singleFieldCondition("z", pcl::ComparisonOps::EQ, 0.0));

  pcl::PointCloud<pcl::PointXYZ> output;
  filter.filter(output);

  EXPECT_EQ(outputToInputIndices(cloud, output), expectedSingleField(cloud, nullptr, "z", pcl::ComparisonOps::EQ, 0.0f));
}

TEST(ConditionalRemovalRVV, PointXYZISingleFloatFieldKeepsGenericFields)
{
  const auto cloud = makeCloudXYZI(257);
  pcl::ConditionAnd<pcl::PointXYZI>::Ptr condition(new pcl::ConditionAnd<pcl::PointXYZI>());
  condition->addComparison(pcl::FieldComparison<pcl::PointXYZI>::ConstPtr(
      new pcl::FieldComparison<pcl::PointXYZI>("intensity", pcl::ComparisonOps::GT, 4.0)));
  pcl::ConditionalRemoval<pcl::PointXYZI> filter;
  filter.setInputCloud(cloud.makeShared());
  filter.setCondition(condition);

  pcl::PointCloud<pcl::PointXYZI> output;
  filter.filter(output);

  for (const auto& point : output)
    EXPECT_GT(point.intensity, 4.0f);
}

int
main(int argc, char** argv)
{
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
