#include "radius_outlier_removal_diag.hpp"

#include <pcl/filters/radius_outlier_removal.h>

#include <gtest/gtest.h>

namespace {

using namespace pcl_rvv_filters_radius_outlier_removal;

void
expectSameTail(const ApplyFilterIndicesTailResult& a, const ApplyFilterIndicesTailResult& b)
{
  EXPECT_EQ(a.kept, b.kept);
  EXPECT_EQ(a.removed, b.removed);
  EXPECT_EQ(checksumApplyFilterIndicesTailResult(a), checksumApplyFilterIndicesTailResult(b));
}

template <typename FilterT>
EntryDiagnosticResult
runEntry(FilterT& filter)
{
  EntryDiagnosticResult result;
  filter.filter(result.kept);
  if (filter.getRemovedIndices())
    result.removed = *filter.getRemovedIndices();
  result.checksum =
      RadiusOutlierRemovalEntryDiagnostic<pcl::PointXYZ>::checksumEntryDiagnosticResult(result);
  return result;
}

void
expectSameEntry(const EntryDiagnosticResult& a, const EntryDiagnosticResult& b)
{
  EXPECT_EQ(a.kept, b.kept);
  EXPECT_EQ(a.removed, b.removed);
  EXPECT_EQ(a.checksum, b.checksum);
}

} // namespace

TEST(RadiusOutlierRemovalDiag, TailCompressMostlyKeepMatchesScalar)
{
  const auto indices = makeIndices(1024, false);
  const auto to_keep = makeKeepMask(1024, 0);
  const ApplyFilterIndicesTailReplayContext ctx{&indices, &to_keep, true};
  const auto std_result = applyFilterIndicesTailStdReplay(ctx);
  ApplyFilterIndicesTailResult rvv_result;
  const bool rvv_used = applyFilterIndicesTailRVVReplay(ctx, rvv_result);

#if defined(__RVV10__) && defined(PCL_RADIUS_OUTLIER_REMOVAL_RVV_DIAGNOSTIC)
  EXPECT_TRUE(rvv_used);
#else
  EXPECT_FALSE(rvv_used);
  rvv_result = applyFilterIndicesTailStdReplay(ctx);
#endif
  expectSameTail(std_result, rvv_result);
}

TEST(RadiusOutlierRemovalDiag, TailCompressShuffledHalfKeepMatchesScalar)
{
  const auto indices = makeIndices(2048, true);
  const auto to_keep = makeKeepMask(2048, 1);
  const ApplyFilterIndicesTailReplayContext ctx{&indices, &to_keep, true};
  const auto std_result = applyFilterIndicesTailStdReplay(ctx);
  ApplyFilterIndicesTailResult rvv_result;
  const bool rvv_used = applyFilterIndicesTailRVVReplay(ctx, rvv_result);

#if defined(__RVV10__) && defined(PCL_RADIUS_OUTLIER_REMOVAL_RVV_DIAGNOSTIC)
  EXPECT_TRUE(rvv_used);
#else
  EXPECT_FALSE(rvv_used);
  rvv_result = applyFilterIndicesTailStdReplay(ctx);
#endif
  expectSameTail(std_result, rvv_result);
}

TEST(RadiusOutlierRemovalDiag, TailCompressWithoutRemovedMatchesScalar)
{
  const auto indices = makeIndices(2048, true);
  const auto to_keep = makeKeepMask(2048, 2);
  const ApplyFilterIndicesTailReplayContext ctx{&indices, &to_keep, false};
  const auto std_result = applyFilterIndicesTailStdReplay(ctx);
  ApplyFilterIndicesTailResult rvv_result;
  const bool rvv_used = applyFilterIndicesTailRVVReplay(ctx, rvv_result);

#if defined(__RVV10__) && defined(PCL_RADIUS_OUTLIER_REMOVAL_RVV_DIAGNOSTIC)
  EXPECT_TRUE(rvv_used);
#else
  EXPECT_FALSE(rvv_used);
  rvv_result = applyFilterIndicesTailStdReplay(ctx);
#endif
  expectSameTail(std_result, rvv_result);
}

TEST(RadiusOutlierRemovalDiag, SmallInputFallsBackToScalar)
{
  const auto indices = makeIndices(16, false);
  const auto to_keep = makeKeepMask(16, 0);
  ApplyFilterIndicesTailResult rvv_result;
  const ApplyFilterIndicesTailReplayContext ctx{&indices, &to_keep, true};
  EXPECT_FALSE(applyFilterIndicesTailRVVReplay(ctx, rvv_result));
}

TEST(RadiusOutlierRemovalDiag, FullDiagnosticKeepsCompressSemantics)
{
  const auto cloud = makePointCloud(1024);
  const auto indices = makeIndices(1024, true);
  const auto to_keep = makeKeepMask(1024, 0);
  const auto std_result = runApplyFilterIndicesDiagnostic(cloud, indices, to_keep, true, 4, false);
  const auto rvv_result = runApplyFilterIndicesDiagnostic(cloud, indices, to_keep, true, 4, true);

  expectSameTail(std_result.tail, rvv_result.tail);
  EXPECT_EQ(std_result.search_checksum, rvv_result.search_checksum);
  EXPECT_EQ(std_result.checksum, rvv_result.checksum);
}

TEST(RadiusOutlierRemovalDiag, ProductionRadiusOutlierRemovalStillRuns)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>(makePointCloud(128));
  pcl::RadiusOutlierRemoval<pcl::PointXYZ> filter(true);
  filter.setInputCloud(cloud);
  filter.setRadiusSearch(0.20);
  filter.setMinNeighborsInRadius(2);
  pcl::Indices output;
  filter.filter(output);

  EXPECT_LE(output.size(), cloud->size());
  EXPECT_LE(filter.getRemovedIndices()->size(), cloud->size());
}

TEST(RadiusOutlierRemovalDiag, EntryDiagnosticMatchesUpstreamFakeIndices)
{
  auto cloud =
      pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>(makeEntryDiagnosticPointCloud(256));

  pcl::RadiusOutlierRemoval<pcl::PointXYZ> upstream(true);
  upstream.setInputCloud(cloud);
  upstream.setRadiusSearch(0.035);
  upstream.setMinNeighborsInRadius(2);

  RadiusOutlierRemovalEntryDiagnostic<pcl::PointXYZ> diagnostic(true);
  diagnostic.setInputCloud(cloud);
  diagnostic.setRadiusSearch(0.035);
  diagnostic.setMinNeighborsInRadius(2);

  expectSameEntry(runEntry(upstream), diagnostic.runApplyFilterIndicesEntry());
}

TEST(RadiusOutlierRemovalDiag, EntryDiagnosticMatchesUpstreamExplicitIndicesNegative)
{
  auto cloud =
      pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>(makeEntryDiagnosticPointCloud(256));
  auto indices = pcl::make_shared<pcl::Indices>();
  indices->reserve(70);
  for (int i = 63; i >= 0; --i)
    indices->push_back(i);
  indices->push_back(7);
  indices->push_back(7);
  indices->push_back(128);
  indices->push_back(129);

  pcl::RadiusOutlierRemoval<pcl::PointXYZ> upstream(true);
  upstream.setInputCloud(cloud);
  upstream.setIndices(indices);
  upstream.setRadiusSearch(0.035);
  upstream.setMinNeighborsInRadius(2);
  upstream.setNegative(true);

  RadiusOutlierRemovalEntryDiagnostic<pcl::PointXYZ> diagnostic(true);
  diagnostic.setInputCloud(cloud);
  diagnostic.setIndices(indices);
  diagnostic.setRadiusSearch(0.035);
  diagnostic.setMinNeighborsInRadius(2);
  diagnostic.setNegative(true);

  expectSameEntry(runEntry(upstream), diagnostic.runApplyFilterIndicesEntry());
}
