/*
 * 本文件做什么：
 * 这些 gtest（单元测试）验证 transformation_estimation_svd_scale 的 production public
 * scale path（生产公开 scale 路径）和 scale-aware fused accumulation（带尺度估计的融合累加）
 * candidate 是否与同构标量 reference 保持一致。
 *
 * 证据边界：
 * public ordered-cloud-pair case（公开顺序点云对用例）用于 production integration
 * correctness（生产接入正确性）；test-only candidate 仍只作为 diagnostic 对照。
 */

#include "tesvd_scale.h"

#include <pcl/rvv_point_traits.h>
#include <pcl/test/gtest.h>

namespace support = pcl::registration::rvv_tesvd_scale_support;

namespace {

template <typename PointSource, typename PointTarget, typename Scalar>
class ScaleFallbackCaller
: public pcl::registration::TransformationEstimationSVDScale<
      PointSource,
      PointTarget,
      Scalar> {
public:
  using BaseScale = pcl::registration::
      TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>;
  using BaseSVD =
      pcl::registration::TransformationEstimationSVD<PointSource, PointTarget, Scalar>;
  using Matrix4 = typename BaseScale::Matrix4;

  Matrix4
  estimateOrderedViaBase(const pcl::PointCloud<PointSource>& source,
                         const pcl::PointCloud<PointTarget>& target) const
  {
    Matrix4 matrix = Matrix4::Identity();
    BaseSVD::estimateRigidTransformation(source, target, matrix);
    return matrix;
  }

  Matrix4
  estimateIndexedViaBase(const pcl::PointCloud<PointSource>& source,
                         const pcl::Indices& indices,
                         const pcl::PointCloud<PointTarget>& target) const
  {
    Matrix4 matrix = Matrix4::Identity();
    BaseSVD::estimateRigidTransformation(source, indices, target, matrix);
    return matrix;
  }

  Matrix4
  estimateDualIndexedViaBase(const pcl::PointCloud<PointSource>& source,
                             const pcl::Indices& source_indices,
                             const pcl::PointCloud<PointTarget>& target,
                             const pcl::Indices& target_indices) const
  {
    Matrix4 matrix = Matrix4::Identity();
    BaseSVD::estimateRigidTransformation(
        source, source_indices, target, target_indices, matrix);
    return matrix;
  }

  Matrix4
  estimateCorrespondenceViaBase(const pcl::PointCloud<PointSource>& source,
                                const pcl::PointCloud<PointTarget>& target,
                                const pcl::Correspondences& correspondences) const
  {
    Matrix4 matrix = Matrix4::Identity();
    BaseSVD::estimateRigidTransformation(source, target, correspondences, matrix);
    return matrix;
  }
};

template <typename Scalar>
using PointXYZScaleFallbackCaller =
    ScaleFallbackCaller<pcl::PointXYZ, pcl::PointXYZ, Scalar>;

template <typename CandidateMatrix, typename ReferenceMatrix>
void
expectMatrixNear(const CandidateMatrix& candidate,
                 const ReferenceMatrix& reference,
                 const double epsilon)
{
  ASSERT_EQ(candidate.rows(), reference.rows());
  ASSERT_EQ(candidate.cols(), reference.cols());
  for (int r = 0; r < candidate.rows(); ++r) {
    for (int c = 0; c < candidate.cols(); ++c) {
      EXPECT_NEAR(static_cast<double>(candidate(r, c)),
                  static_cast<double>(reference(r, c)),
                  epsilon)
          << "matrix mismatch at (" << r << ", " << c << ")";
    }
  }
}

template <typename PointSource, typename PointTarget>
void
expectGenericXYZPublicPathMatchesReference()
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value);
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value);

  const auto source = support::makePointCloudXYZ<PointSource>(8192);
  const auto target =
      support::transformCloudXYZTo<PointSource, PointTarget>(
          source, support::makeSimilarityTransform());

  const Eigen::Matrix4f public_matrix = support::estimateScalePublic(source, target);
  const Eigen::Matrix4f reference_matrix = support::estimateScaleStd(source, target);
  expectMatrixNear(public_matrix, reference_matrix, 5e-4f);
}

template <typename PointSource, typename PointTarget>
void
expectGenericXYZDoublePublicPathMatchesReference()
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value);
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value);

  const auto source = support::makePointCloudXYZ<PointSource>(8192);
  const auto target =
      support::transformCloudXYZTo<PointSource, PointTarget>(
          source, support::makeSimilarityTransform());

  pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, double>
      estimator;
  Eigen::Matrix4d public_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, public_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4d reference_matrix =
      support::estimateScaleStdDouble(source, target, &stats);
  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());

  const ScaleFallbackCaller<PointSource, PointTarget, double> fallback_caller;
  expectMatrixNear(public_matrix, reference_matrix, 5e-8);
  expectMatrixNear(public_matrix,
                   fallback_caller.estimateOrderedViaBase(source, target),
                   5e-8);
}

template <typename PointSource, typename PointTarget>
void
expectGenericXYZDoubleRVVDispatchHits()
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value);
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value);

  const auto source = support::makePointCloudXYZ<PointSource>(8192);
  const auto target =
      support::transformCloudXYZTo<PointSource, PointTarget>(
          source, support::makeSimilarityTransform());
  Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
#ifdef __RVV10__
  EXPECT_TRUE(pcl::registration::detail::
                  estimateRigidTransformationSVDScaleOrderedCloudPairRVV(
                      source, target, matrix));
#else
  (void)matrix;
#endif
}

template <typename PointSource, typename PointTarget>
void
expectGenericXYZDoubleFallbackBoundaries()
{
  const ScaleFallbackCaller<PointSource, PointTarget, double> fallback_caller;

  {
    const auto source = support::makePointCloudXYZ<PointSource>(8);
    const auto target =
        support::transformCloudXYZTo<PointSource, PointTarget>(
            source, support::makeSimilarityTransform());
    pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, double>
        estimator;
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(source, target, matrix);
    expectMatrixNear(matrix,
                     fallback_caller.estimateOrderedViaBase(source, target),
                     5e-8);
#ifdef __RVV10__
    Eigen::Matrix4d detail_matrix = Eigen::Matrix4d::Identity();
    EXPECT_FALSE(pcl::registration::detail::
                     estimateRigidTransformationSVDScaleOrderedCloudPairRVV(
                         source, target, detail_matrix));
#endif
  }

  {
    auto source = support::makePointCloudXYZ<PointSource>(8192);
    auto target =
        support::transformCloudXYZTo<PointSource, PointTarget>(
            source, support::makeSimilarityTransform());
    source.is_dense = false;
    target.is_dense = false;
    pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, double>
        estimator;
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(source, target, matrix);
    expectMatrixNear(matrix,
                     fallback_caller.estimateOrderedViaBase(source, target),
                     5e-8);
#ifdef __RVV10__
    Eigen::Matrix4d detail_matrix = Eigen::Matrix4d::Identity();
    EXPECT_FALSE(pcl::registration::detail::
                     estimateRigidTransformationSVDScaleOrderedCloudPairRVV(
                         source, target, detail_matrix));
#endif
  }
}

template <typename PointSource, typename PointTarget>
void
expectRowSourceGenericXYZDoublePublicPathMatchesReference()
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value);
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value);

  const Eigen::Matrix4f transform = support::makeSimilarityTransform();
  const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
  const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
  const auto source = support::makePointCloudXYZ<PointSource>(source_indices.size() * 2);
  const auto target =
      support::transformCloudXYZTo<PointSource, PointTarget>(source, transform);
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const auto selected_source_target =
      support::transformCloudXYZTo<PointSource, PointTarget>(selected_source, transform);

  pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, double>
      estimator;

  Eigen::Matrix4d source_indexed_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, selected_source_target, source_indexed_matrix);
  expectMatrixNear(source_indexed_matrix,
                   support::estimateScaleStdDouble(selected_source,
                                                   selected_source_target),
                   5e-8);

  Eigen::Matrix4d dual_indexed_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, dual_indexed_matrix);
  expectMatrixNear(dual_indexed_matrix,
                   support::estimateScaleStdDouble(selected_source, selected_target),
                   5e-8);

  Eigen::Matrix4d correspondence_matrix = Eigen::Matrix4d::Identity();
  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  estimator.estimateRigidTransformation(source, target, correspondences, correspondence_matrix);
  expectMatrixNear(correspondence_matrix,
                   support::estimateScaleStdDouble(selected_source, selected_target),
                   5e-8);
}

template <typename PointSource, typename PointTarget>
void
expectRowSourceGenericXYZDoubleRVVDispatchHits()
{
#ifdef __RVV10__
  const Eigen::Matrix4f transform = support::makeSimilarityTransform();
  const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
  const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
  const auto source = support::makePointCloudXYZ<PointSource>(source_indices.size() * 2);
  const auto target =
      support::transformCloudXYZTo<PointSource, PointTarget>(source, transform);
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_source_target =
      support::transformCloudXYZTo<PointSource, PointTarget>(selected_source, transform);
  Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();

  EXPECT_TRUE(pcl::registration::detail::
                  estimateRigidTransformationSVDScaleSourceIndexedCloudPairRVV(
                      source, source_indices, selected_source_target, matrix));
  EXPECT_TRUE(pcl::registration::detail::
                  estimateRigidTransformationSVDScaleDualIndicesCloudPairRVV(
                      source, source_indices, target, target_indices, matrix));
  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  EXPECT_TRUE(pcl::registration::detail::
                  estimateRigidTransformationSVDScaleCorrespondencePairRVV(
                      source, target, correspondences, matrix));
#endif
}

template <typename PointSource, typename PointTarget>
void
expectRowSourceGenericXYZDoubleFallbackBoundaries()
{
  const ScaleFallbackCaller<PointSource, PointTarget, double> fallback_caller;

  {
    const pcl::Indices source_indices = support::makeStrideIndices(8, 1, 0);
    const pcl::Indices target_indices = support::makeStrideIndices(8, 1, 0);
    const auto source = support::makePointCloudXYZ<PointSource>(8);
    const auto target =
        support::transformCloudXYZTo<PointSource, PointTarget>(
            source, support::makeSimilarityTransform());
    pcl::registration::TransformationEstimationSVDScale<PointSource,
                                                        PointTarget,
                                                        double>
        estimator;
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(source, source_indices, target, matrix);
    expectMatrixNear(matrix,
                     fallback_caller.estimateIndexedViaBase(
                         source, source_indices, target),
                     5e-8);
#ifdef __RVV10__
    Eigen::Matrix4d detail_matrix = Eigen::Matrix4d::Identity();
    EXPECT_FALSE(pcl::registration::detail::
                     estimateRigidTransformationSVDScaleSourceIndexedCloudPairRVV(
                         source, source_indices, target, detail_matrix));
#endif
  }

  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    auto source = support::makePointCloudXYZ<PointSource>(source_indices.size() * 2);
    auto target =
        support::transformCloudXYZTo<PointSource, PointTarget>(
            source, support::makeSimilarityTransform());
    source.is_dense = false;
    target.is_dense = false;
    pcl::registration::TransformationEstimationSVDScale<PointSource,
                                                        PointTarget,
                                                        double>
        estimator;
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(
        source, source_indices, target, target_indices, matrix);
    expectMatrixNear(matrix,
                     fallback_caller.estimateDualIndexedViaBase(
                         source, source_indices, target, target_indices),
                     5e-8);
#ifdef __RVV10__
    Eigen::Matrix4d detail_matrix = Eigen::Matrix4d::Identity();
    EXPECT_FALSE(pcl::registration::detail::
                     estimateRigidTransformationSVDScaleDualIndicesCloudPairRVV(
                         source, source_indices, target, target_indices, detail_matrix));
#endif
  }
}

template <typename PointSource, typename PointTarget>
void
expectRowSourceScaleMatchesReference(const pcl::Indices& source_indices,
                                     const pcl::Indices& target_indices,
                                     const bool use_correspondences)
{
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value);
  static_assert(pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value);

  const auto source = support::makePointCloudXYZ<PointSource>(source_indices.size() * 2);
  const auto target =
      support::transformCloudXYZTo<PointSource, PointTarget>(
          source, support::makeSimilarityTransform());
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);

  pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, float>
      estimator;
  Eigen::Matrix4f row_source_matrix = Eigen::Matrix4f::Identity();
  if (use_correspondences) {
    const pcl::Correspondences correspondences =
        support::makeCorrespondences(source_indices, target_indices);
    estimator.estimateRigidTransformation(source,
                                          target,
                                          correspondences,
                                          row_source_matrix);
  }
  else if (source_indices == target_indices) {
    estimator.estimateRigidTransformation(source,
                                          source_indices,
                                          selected_target,
                                          row_source_matrix);
  }
  else {
    estimator.estimateRigidTransformation(source,
                                          source_indices,
                                          target,
                                          target_indices,
                                          row_source_matrix);
  }

  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(selected_source, selected_target);
  expectMatrixNear(row_source_matrix, reference_matrix, 5e-4f);
}

template <typename PointSource, typename PointTarget>
void
expectAllRowSourcesMoreGenericComboMatchesReference()
{
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = source_indices;
    expectRowSourceScaleMatchesReference<PointSource, PointTarget>(
        source_indices, target_indices, false);
  }
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    expectRowSourceScaleMatchesReference<PointSource, PointTarget>(
        source_indices, target_indices, false);
  }
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    expectRowSourceScaleMatchesReference<PointSource, PointTarget>(
        source_indices, target_indices, true);
  }
}

void
expectCustomLayoutOffsetsAreSampled()
{
  using SrcLayout = pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScalePaddedXYZSource>;
  using TgtLayout = pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScaleWideXYZTarget>;

  static_assert(SrcLayout::value);
  static_assert(TgtLayout::value);
  static_assert(SrcLayout::kX == 4u);
  static_assert(SrcLayout::kY == 12u);
  static_assert(SrcLayout::kZ == 24u);
  static_assert(TgtLayout::kX == 8u);
  static_assert(TgtLayout::kY == 20u);
  static_assert(TgtLayout::kZ == 28u);
  static_assert(sizeof(LocalSVDScalePaddedXYZSource) != sizeof(pcl::PointXYZ));
  static_assert(sizeof(LocalSVDScaleWideXYZTarget) != sizeof(pcl::PointXYZ));
}

void
expectCustomPaddingLayoutOffsetsAreSampled()
{
  using CompactSrcLayout =
      pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScaleCompactXYZSource>;
  using CompactTgtLayout =
      pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScaleCompactXYZTarget>;
  using HugeSrcLayout =
      pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScaleHugePaddingXYZSource>;
  using HugeTgtLayout =
      pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScaleHugePaddingXYZTarget>;

  static_assert(CompactSrcLayout::value);
  static_assert(CompactTgtLayout::value);
  static_assert(HugeSrcLayout::value);
  static_assert(HugeTgtLayout::value);
  static_assert(CompactSrcLayout::kX == 0u);
  static_assert(CompactSrcLayout::kY == 4u);
  static_assert(CompactSrcLayout::kZ == 8u);
  static_assert(CompactTgtLayout::kX == 4u);
  static_assert(CompactTgtLayout::kY == 8u);
  static_assert(CompactTgtLayout::kZ == 12u);
  static_assert(HugeSrcLayout::kX == 8u);
  static_assert(HugeSrcLayout::kY == 24u);
  static_assert(HugeSrcLayout::kZ == 48u);
  static_assert(HugeTgtLayout::kX == 16u);
  static_assert(HugeTgtLayout::kY == 48u);
  static_assert(HugeTgtLayout::kZ == 60u);
  static_assert(sizeof(LocalSVDScaleCompactXYZSource) !=
                sizeof(LocalSVDScalePaddedXYZSource));
  static_assert(sizeof(LocalSVDScaleHugePaddingXYZSource) >
                sizeof(LocalSVDScalePaddedXYZSource));
  static_assert(sizeof(LocalSVDScaleHugePaddingXYZTarget) >
                sizeof(LocalSVDScaleWideXYZTarget));
}

void
expectCustomAlignmentLayoutOffsetsAreSampled()
{
  using AlignedSrcLayout =
      pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScaleAligned64XYZSource>;
  using AlignedTgtLayout =
      pcl::rvv::RVVXYZAoSFloatLayout<LocalSVDScaleAligned32XYZTarget>;

  static_assert(AlignedSrcLayout::value);
  static_assert(AlignedTgtLayout::value);
  static_assert(AlignedSrcLayout::kX == 16u);
  static_assert(AlignedSrcLayout::kY == 32u);
  static_assert(AlignedSrcLayout::kZ == 60u);
  static_assert(AlignedTgtLayout::kX == 28u);
  static_assert(AlignedTgtLayout::kY == 60u);
  static_assert(AlignedTgtLayout::kZ == 92u);
  static_assert(alignof(LocalSVDScaleAligned64XYZSource) == 64u);
  static_assert(alignof(LocalSVDScaleAligned32XYZTarget) == 32u);
  static_assert(sizeof(LocalSVDScaleAligned64XYZSource) >
                sizeof(LocalSVDScaleHugePaddingXYZSource));
  static_assert(sizeof(LocalSVDScaleAligned32XYZTarget) >=
                sizeof(LocalSVDScaleHugePaddingXYZTarget));
}

} // namespace

// public scale path（公开 scale 路径）是当前 production truth（生产事实）。
// 同构标量 reference 直接从原始点对累加 H、source variance 和 scale 项；这条测试保护
// 代数改写没有偏离 `TransformationEstimationSVDScale` 的输出语义。
TEST(TransformationEstimationSVDScale, ReferenceMatchesPublicScaleOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());

  const Eigen::Matrix4f public_matrix = support::estimateScalePublic(source, target);
  const Eigen::Matrix4f reference_matrix = support::estimateScaleStd(source, target);

  expectMatrixNear(reference_matrix, public_matrix, 5e-4f);
}

// production ordered public overload（生产顺序公开重载）的小规模和非 dense 输入必须保持
// fallback 语义；这里不要求运行时统计，只要求 public output 不偏离同构 reference。
TEST(TransformationEstimationSVDScale, PublicScaleFallbackBoundariesMatchReference)
{
  PointXYZScaleFallbackCaller<float> fallback_caller;

  {
    const auto source = support::makePointXYZCloud(8);
    const auto target =
        support::transformCloudXYZ(source, support::makeSimilarityTransform());
    expectMatrixNear(support::estimateScalePublic(source, target),
                     fallback_caller.estimateOrderedViaBase(source, target),
                     5e-4);
  }

  {
    auto source = support::makePointXYZCloud(8192);
    auto target = support::transformCloudXYZ(source, support::makeSimilarityTransform());
    source.is_dense = false;
    target.is_dense = false;
    expectMatrixNear(support::estimateScalePublic(source, target),
                     fallback_caller.estimateOrderedViaBase(source, target),
                     5e-4);
  }
}

// Indexed overload（索引重载）仍不是 ordered production RVV 覆盖范围。
// 它应保持既有 public scale 语义，并在 RVV 构建中自然回到父类路径。
TEST(TransformationEstimationSVDScale, UncoveredPublicEntriesStayCorrect)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  PointXYZScaleFallbackCaller<float> float_fallback_caller;

  pcl::Indices indices(source.size());
  for (std::size_t i = 0; i < source.size(); ++i)
    indices[i] = static_cast<pcl::index_t>(i);

  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, float>
      indexed_estimator;
  Eigen::Matrix4f indexed_matrix = Eigen::Matrix4f::Identity();
  indexed_estimator.estimateRigidTransformation(source, indices, target, indexed_matrix);
  expectMatrixNear(indexed_matrix,
                   float_fallback_caller.estimateIndexedViaBase(source, indices, target),
                   5e-4);
}

// Phase 065 的 ordered `Scalar=double` production probe（生产探针）只覆盖
// `PointXYZ -> PointXYZ` dense 公开入口。正确性预算沿用 Phase 063 建立的
// double fused accumulation（双精度融合累加）与父类 double fallback 对齐边界。
TEST(TransformationEstimationSVDScale, ScalarDoubleProductionProbeMatchesReference)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  PointXYZScaleFallbackCaller<double> double_fallback_caller;
  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
      double_estimator;
  Eigen::Matrix4d double_matrix = Eigen::Matrix4d::Identity();
  double_estimator.estimateRigidTransformation(source, target, double_matrix);

  support::CandidateStats stats;
  const Eigen::Matrix4d scalar_double_reference =
      support::estimateScaleStdDouble(source, target, &stats);
  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
  expectMatrixNear(double_matrix, scalar_double_reference, 5e-8);
  expectMatrixNear(double_matrix,
                   double_fallback_caller.estimateOrderedViaBase(source, target),
                   5e-8);
}

// Phase 065 只让 dense ordered double 进入 RVV probe；小规模和 non-dense 仍走
// 父类 fallback，防止 double probe 扩大到未验证输入边界。
TEST(TransformationEstimationSVDScale, ScalarDoubleProductionProbeFallbackBoundaries)
{
  PointXYZScaleFallbackCaller<double> double_fallback_caller;

  {
    const auto source = support::makePointXYZCloud(8);
    const auto target =
        support::transformCloudXYZ(source, support::makeSimilarityTransform());
    pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
        estimator;
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(source, target, matrix);
    expectMatrixNear(matrix,
                     double_fallback_caller.estimateOrderedViaBase(source, target),
                     5e-8);
  }

  {
    auto source = support::makePointXYZCloud(8192);
    auto target = support::transformCloudXYZ(source, support::makeSimilarityTransform());
    source.is_dense = false;
    target.is_dense = false;
    pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
        estimator;
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(source, target, matrix);
    expectMatrixNear(matrix,
                     double_fallback_caller.estimateOrderedViaBase(source, target),
                     5e-8);
  }
}

// Phase 067 把 `Scalar=double` 的 production probe（生产探针）扩到三类 row-source
// public overload（公开重载）。范围仍严格收窄到 dense `PointXYZ -> PointXYZ`，
// 不证明泛型点型、custom layout 或 `float` row-source family selection。
TEST(TransformationEstimationSVDScale, ScalarDoubleRowSourceProductionProbeMatchesReference)
{
  const Eigen::Matrix4f transform = support::makeSimilarityTransform();
  const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
  const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
  const auto source = support::makePointXYZCloud(source_indices.size() * 2);
  const auto target = support::transformCloudXYZ(source, transform);
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const auto selected_source_target = support::transformCloudXYZ(selected_source, transform);

  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
      estimator;

  Eigen::Matrix4d source_indexed_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, selected_source_target, source_indexed_matrix);
  expectMatrixNear(source_indexed_matrix,
                   support::estimateScaleStdDouble(selected_source,
                                                   selected_source_target),
                   5e-8);

  Eigen::Matrix4d dual_indexed_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(
      source, source_indices, target, target_indices, dual_indexed_matrix);
  expectMatrixNear(dual_indexed_matrix,
                   support::estimateScaleStdDouble(selected_source, selected_target),
                   5e-8);

  Eigen::Matrix4d correspondence_matrix = Eigen::Matrix4d::Identity();
  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  estimator.estimateRigidTransformation(source, target, correspondences, correspondence_matrix);
  expectMatrixNear(correspondence_matrix,
                   support::estimateScaleStdDouble(selected_source, selected_target),
                   5e-8);
}

// row-source double probe 的小规模和 non-dense 输入必须回到父类 double fallback。
// 这条测试隔离 scale 阈值和 dense gate，防止 Phase 067 的 exact-type branch 误扩范围。
TEST(TransformationEstimationSVDScale, ScalarDoubleRowSourceProductionProbeFallbackBoundaries)
{
  PointXYZScaleFallbackCaller<double> double_fallback_caller;

  {
    const pcl::Indices source_indices = support::makeStrideIndices(8, 1, 0);
    const pcl::Indices target_indices = support::makeStrideIndices(8, 1, 0);
    const auto source = support::makePointXYZCloud(8);
    const auto target = support::transformCloudXYZ(source, support::makeSimilarityTransform());
    pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ,
                                                        pcl::PointXYZ,
                                                        double>
        estimator;
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(source, source_indices, target, matrix);
    expectMatrixNear(matrix,
                     double_fallback_caller.estimateIndexedViaBase(
                         source, source_indices, target),
                     5e-8);
  }

  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    auto source = support::makePointXYZCloud(source_indices.size() * 2);
    auto target =
        support::transformCloudXYZ(source, support::makeSimilarityTransform());
    source.is_dense = false;
    target.is_dense = false;
    pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ,
                                                        pcl::PointXYZ,
                                                        double>
        estimator;

    Eigen::Matrix4d dual_indexed_matrix = Eigen::Matrix4d::Identity();
    estimator.estimateRigidTransformation(
        source, source_indices, target, target_indices, dual_indexed_matrix);
    expectMatrixNear(dual_indexed_matrix,
                     double_fallback_caller.estimateDualIndexedViaBase(
                         source, source_indices, target, target_indices),
                     5e-8);

    Eigen::Matrix4d correspondence_matrix = Eigen::Matrix4d::Identity();
    const pcl::Correspondences correspondences =
        support::makeCorrespondences(source_indices, target_indices);
    estimator.estimateRigidTransformation(
        source, target, correspondences, correspondence_matrix);
    expectMatrixNear(correspondence_matrix,
                     double_fallback_caller.estimateCorrespondenceViaBase(
                         source, target, correspondences),
                     5e-8);
  }
}

// Phase 068 先只把 ordered `Scalar=double` 从 exact `PointXYZ` 扩到 common
// PCL xyz AoS 点型。测试直接检查 production detail dispatch（生产细节分流）
// 是否命中 RVV，避免“public 结果正确但实际 fallback”的假阳性。
TEST(TransformationEstimationSVDScale, GenericScalarDoubleOrderedProductionProbeMatchesReference)
{
  expectGenericXYZDoublePublicPathMatchesReference<pcl::PointXYZI, pcl::PointXYZI>();
  expectGenericXYZDoubleRVVDispatchHits<pcl::PointXYZI, pcl::PointXYZI>();
  expectGenericXYZDoublePublicPathMatchesReference<pcl::PointXYZRGB, pcl::PointXYZRGB>();
  expectGenericXYZDoubleRVVDispatchHits<pcl::PointXYZRGB, pcl::PointXYZRGB>();
  expectGenericXYZDoublePublicPathMatchesReference<pcl::PointXYZI, pcl::PointXYZRGB>();
  expectGenericXYZDoubleRVVDispatchHits<pcl::PointXYZI, pcl::PointXYZRGB>();
  expectGenericXYZDoublePublicPathMatchesReference<pcl::PointXYZRGBA, pcl::PointXYZRGBA>();
  expectGenericXYZDoubleRVVDispatchHits<pcl::PointXYZRGBA, pcl::PointXYZRGBA>();
  expectGenericXYZDoublePublicPathMatchesReference<pcl::PointNormal, pcl::PointXYZRGB>();
  expectGenericXYZDoubleRVVDispatchHits<pcl::PointNormal, pcl::PointXYZRGB>();
}

// generic double probe 仍必须保留 small / non-dense fallback（回退）边界。
// 这些 case 证明 traits-gated 点型不会绕过 Phase 065 的 size / dense gate。
TEST(TransformationEstimationSVDScale,
     GenericScalarDoubleOrderedProductionProbeFallbackBoundaries)
{
  expectGenericXYZDoubleFallbackBoundaries<pcl::PointXYZI, pcl::PointXYZI>();
  expectGenericXYZDoubleFallbackBoundaries<pcl::PointXYZRGB, pcl::PointXYZRGB>();
}

// Phase 069 继续把 `Scalar=double` 扩到 row-source generic common PCL xyz AoS。
// 这里覆盖三类公开 row-source overload，并用 detail-hit 检查确认 RVV 分流实际命中。
TEST(TransformationEstimationSVDScale,
     RowSourceGenericScalarDoubleProductionProbeMatchesReference)
{
  expectRowSourceGenericXYZDoublePublicPathMatchesReference<pcl::PointXYZI,
                                                           pcl::PointXYZI>();
  expectRowSourceGenericXYZDoubleRVVDispatchHits<pcl::PointXYZI,
                                                 pcl::PointXYZI>();
  expectRowSourceGenericXYZDoublePublicPathMatchesReference<pcl::PointXYZRGB,
                                                           pcl::PointXYZRGB>();
  expectRowSourceGenericXYZDoubleRVVDispatchHits<pcl::PointXYZRGB,
                                                 pcl::PointXYZRGB>();
  expectRowSourceGenericXYZDoublePublicPathMatchesReference<pcl::PointXYZI,
                                                           pcl::PointXYZRGB>();
  expectRowSourceGenericXYZDoubleRVVDispatchHits<pcl::PointXYZI,
                                                 pcl::PointXYZRGB>();
}

// Phase 069 的 generic row-source double 分支仍保留 small / non-dense fallback。
// 这些 case 防止 common PCL 点型因为通过 traits gate 而绕过运行期边界。
TEST(TransformationEstimationSVDScale,
     RowSourceGenericScalarDoubleProductionProbeFallbackBoundaries)
{
  expectRowSourceGenericXYZDoubleFallbackBoundaries<pcl::PointXYZI,
                                                   pcl::PointXYZI>();
  expectRowSourceGenericXYZDoubleFallbackBoundaries<pcl::PointXYZRGB,
                                                   pcl::PointXYZRGB>();
}

// Phase 070 先用测试本地 registered custom xyz AoS layout（自定义 xyz 结构数组布局）
// 检查 `Scalar=double` 的真实 public path。它和 Phase 069 的 common PCL whitelist
// 分开：如果 RVV 构建下 detail dispatch 没有命中，这个 scout 不能写成 custom layout
// double 已覆盖。
TEST(TransformationEstimationSVDScale,
     CustomLayoutScalarDoubleProductionScoutMatchesReference)
{
  expectCustomLayoutOffsetsAreSampled();
  expectGenericXYZDoublePublicPathMatchesReference<LocalSVDScalePaddedXYZSource,
                                                   LocalSVDScaleWideXYZTarget>();
  expectGenericXYZDoubleRVVDispatchHits<LocalSVDScalePaddedXYZSource,
                                        LocalSVDScaleWideXYZTarget>();
  expectRowSourceGenericXYZDoublePublicPathMatchesReference<
      LocalSVDScalePaddedXYZSource,
      LocalSVDScaleWideXYZTarget>();
  expectRowSourceGenericXYZDoubleRVVDispatchHits<LocalSVDScalePaddedXYZSource,
                                                 LocalSVDScaleWideXYZTarget>();
}

// custom layout double 仍必须保留运行期 fallback（回退）边界。这个测试隔离 small
// 和 non-dense gate，避免 Phase 070 的 scout 因为 layout gate 放宽而吞掉未验证输入。
TEST(TransformationEstimationSVDScale,
     CustomLayoutScalarDoubleProductionScoutFallbackBoundaries)
{
  expectGenericXYZDoubleFallbackBoundaries<LocalSVDScalePaddedXYZSource,
                                          LocalSVDScaleWideXYZTarget>();
  expectRowSourceGenericXYZDoubleFallbackBoundaries<LocalSVDScalePaddedXYZSource,
                                                   LocalSVDScaleWideXYZTarget>();
}

// Phase 071 扩展 custom layout double（自定义布局双精度）取样，只复用 Phase 070
// 已存在的 production dispatch，不新增生产行为。这里覆盖 compact、huge-padding 和
// aligned 三组 layout，避免把单个 padded/wide 样本误读成全部 custom layout 已证明。
TEST(TransformationEstimationSVDScale,
     MoreCustomLayoutScalarDoubleSamplingMatchesReference)
{
  expectGenericXYZDoublePublicPathMatchesReference<LocalSVDScaleCompactXYZSource,
                                                   LocalSVDScaleCompactXYZTarget>();
  expectGenericXYZDoubleRVVDispatchHits<LocalSVDScaleCompactXYZSource,
                                        LocalSVDScaleCompactXYZTarget>();
  expectRowSourceGenericXYZDoublePublicPathMatchesReference<
      LocalSVDScaleCompactXYZSource,
      LocalSVDScaleCompactXYZTarget>();
  expectRowSourceGenericXYZDoubleRVVDispatchHits<LocalSVDScaleCompactXYZSource,
                                                 LocalSVDScaleCompactXYZTarget>();

  expectGenericXYZDoublePublicPathMatchesReference<LocalSVDScaleHugePaddingXYZSource,
                                                   LocalSVDScaleHugePaddingXYZTarget>();
  expectGenericXYZDoubleRVVDispatchHits<LocalSVDScaleHugePaddingXYZSource,
                                        LocalSVDScaleHugePaddingXYZTarget>();
  expectRowSourceGenericXYZDoublePublicPathMatchesReference<
      LocalSVDScaleHugePaddingXYZSource,
      LocalSVDScaleHugePaddingXYZTarget>();
  expectRowSourceGenericXYZDoubleRVVDispatchHits<LocalSVDScaleHugePaddingXYZSource,
                                                 LocalSVDScaleHugePaddingXYZTarget>();

  expectGenericXYZDoublePublicPathMatchesReference<LocalSVDScaleAligned64XYZSource,
                                                   LocalSVDScaleAligned32XYZTarget>();
  expectGenericXYZDoubleRVVDispatchHits<LocalSVDScaleAligned64XYZSource,
                                        LocalSVDScaleAligned32XYZTarget>();
  expectRowSourceGenericXYZDoublePublicPathMatchesReference<
      LocalSVDScaleAligned64XYZSource,
      LocalSVDScaleAligned32XYZTarget>();
  expectRowSourceGenericXYZDoubleRVVDispatchHits<LocalSVDScaleAligned64XYZSource,
                                                 LocalSVDScaleAligned32XYZTarget>();
}

TEST(TransformationEstimationSVDScale,
     MoreCustomLayoutScalarDoubleSamplingFallbackBoundaries)
{
  expectGenericXYZDoubleFallbackBoundaries<LocalSVDScaleCompactXYZSource,
                                           LocalSVDScaleCompactXYZTarget>();
  expectRowSourceGenericXYZDoubleFallbackBoundaries<LocalSVDScaleCompactXYZSource,
                                                    LocalSVDScaleCompactXYZTarget>();
  expectGenericXYZDoubleFallbackBoundaries<LocalSVDScaleHugePaddingXYZSource,
                                           LocalSVDScaleHugePaddingXYZTarget>();
  expectRowSourceGenericXYZDoubleFallbackBoundaries<
      LocalSVDScaleHugePaddingXYZSource,
      LocalSVDScaleHugePaddingXYZTarget>();
  expectGenericXYZDoubleFallbackBoundaries<LocalSVDScaleAligned64XYZSource,
                                           LocalSVDScaleAligned32XYZTarget>();
  expectRowSourceGenericXYZDoubleFallbackBoundaries<LocalSVDScaleAligned64XYZSource,
                                                    LocalSVDScaleAligned32XYZTarget>();
}

// Phase 063 的 Scalar=double diagnostic scout（双精度诊断侦察）不接 production。
// 这里先把测试专用 double accumulation（双精度累加）对到公开 double fallback；
// 失败说明后续 double 数值预算不能建立。
TEST(TransformationEstimationSVDScale, ScalarDoubleAccumulationScoutMatchesPublicFallback)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  PointXYZScaleFallbackCaller<double> double_fallback_caller;

  support::CandidateStats stats;
  const Eigen::Matrix4d scout_matrix =
      support::estimateScaleStdDouble(source, target, &stats);
  const Eigen::Matrix4d fallback_matrix =
      double_fallback_caller.estimateOrderedViaBase(source, target);

  EXPECT_EQ(stats.input_points, source.size());
  EXPECT_EQ(stats.accepted_points, source.size());
  EXPECT_FALSE(stats.used_rvv);
  EXPECT_FALSE(stats.used_fallback);
  // double scout 走 fused accumulation（融合累加），公开 fallback 走父类矩阵路径；
  // 两者数学等价但累加树不同，因此本阶段把预算固定在 5e-8。
  expectMatrixNear(scout_matrix, fallback_matrix, 5e-8);
}

// RVV f64 scout（双精度 RVV 侦察）仍是 test-only candidate：它从 float xyz
// 加载后扩宽到 double 规约，只判断这条 code shape 能否满足上面的 double 数值预算。
TEST(TransformationEstimationSVDScale, ScalarDoubleRVVWidenedScoutMatchesScalarDoubleScout)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());

  support::CandidateStats scalar_stats;
  support::CandidateStats rvv_stats;
  const Eigen::Matrix4d scalar_matrix =
      support::estimateScaleStdDouble(source, target, &scalar_stats);
  const Eigen::Matrix4d rvv_matrix =
      support::estimateScaleRVVDouble(source, target, &rvv_stats);

  EXPECT_EQ(rvv_stats.input_points, scalar_stats.input_points);
  EXPECT_EQ(rvv_stats.accepted_points, scalar_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(rvv_stats.used_rvv);
  EXPECT_FALSE(rvv_stats.used_fallback);
#else
  EXPECT_FALSE(rvv_stats.used_rvv);
  EXPECT_TRUE(rvv_stats.used_fallback);
#endif
  expectMatrixNear(rvv_matrix, scalar_matrix, 5e-8);
}

// 泛型点型扩展先只证明 production public path（生产公开路径）在代表 xyz AoS
// 点型上与同构标量 reference 一致；性能和 asm 仍留给独立 phase 证据。
TEST(TransformationEstimationSVDScale, GenericXYZPointTypesMatchReference)
{
  expectGenericXYZPublicPathMatchesReference<pcl::PointXYZI, pcl::PointXYZI>();
  expectGenericXYZPublicPathMatchesReference<pcl::PointXYZRGB, pcl::PointXYZRGB>();
  expectGenericXYZPublicPathMatchesReference<pcl::PointXYZI, pcl::PointXYZRGB>();
  expectGenericXYZPublicPathMatchesReference<pcl::PointXYZRGB, pcl::PointXYZ>();
}

// Phase 051 继续扩大 traits-gated xyz AoS（字段布局门控的 xyz 结构数组）
// 证据边界：这些点型仍只证明 ordered public path 的 correctness，不证明全部自定义点型
// 或 row-source / `Scalar=double`。
TEST(TransformationEstimationSVDScale, MoreGenericXYZAoSPointTypesMatchReference)
{
  expectGenericXYZPublicPathMatchesReference<pcl::PointXYZRGBA, pcl::PointXYZRGBA>();
  expectGenericXYZPublicPathMatchesReference<pcl::PointXYZL, pcl::PointXYZ>();
  expectGenericXYZPublicPathMatchesReference<pcl::PointNormal, pcl::PointXYZRGB>();
  expectGenericXYZPublicPathMatchesReference<pcl::PointWithRange, pcl::PointWithRange>();
  expectGenericXYZPublicPathMatchesReference<pcl::PointWithViewpoint, pcl::PointXYZ>();
}

// RVV candidate（RVV 候选）只替换 ordered-cloud-pair 的 scale-aware accumulation
// 前段，3x3 SVD 后段仍走 Eigen。Std 构建自然 fallback；RVV 构建必须命中 RVV path。
TEST(TransformationEstimationSVDScale, CandidateMatchesReferenceOrderedCloudPair)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());

  support::CandidateStats std_stats;
  support::CandidateStats candidate_stats;
  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(source, target, &std_stats);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateScaleCandidate(source, target, &candidate_stats);

  EXPECT_EQ(candidate_stats.input_points, std_stats.input_points);
  EXPECT_EQ(candidate_stats.accepted_points, std_stats.accepted_points);
#ifdef __RVV10__
  EXPECT_TRUE(candidate_stats.used_rvv);
  EXPECT_FALSE(candidate_stats.used_fallback);
#else
  EXPECT_FALSE(candidate_stats.used_rvv);
  EXPECT_TRUE(candidate_stats.used_fallback);
#endif
  expectMatrixNear(candidate_matrix, reference_matrix, 2e-3f);
}

// matrix-local-scale-simplification（矩阵局部 scale 简化）只验证后段公式：
// `trace(R * H)` 应等价于旧路径先计算 `R4 * source_demean` 再逐列点积。
// 它不证明 production dispatch，也不改变当前 PI5 待确认状态。
TEST(TransformationEstimationSVDScale, MatrixLocalScaleSimplificationMatchesLegacyPath)
{
  const auto source = support::makePointXYZCloud(8192);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());

  const Eigen::Matrix4f legacy_matrix =
      support::estimateScaleMatrixLocalLegacy(source, target);
  const Eigen::Matrix4f trace_matrix =
      support::estimateScaleMatrixLocalTrace(source, target);
  const Eigen::Matrix4f public_matrix = support::estimateScalePublic(source, target);

  expectMatrixNear(trace_matrix, legacy_matrix, 5e-4f);
  expectMatrixNear(trace_matrix, public_matrix, 5e-4f);
}

TEST(TransformationEstimationSVDScale, SourceIndexedScaleMatchesReference)
{
  const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
  const pcl::Indices target_indices = source_indices;
  expectRowSourceScaleMatchesReference<pcl::PointXYZ, pcl::PointXYZ>(source_indices,
                                                                      target_indices,
                                                                      false);
}

TEST(TransformationEstimationSVDScale, DualIndexedScaleMatchesReference)
{
  const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
  const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
  expectRowSourceScaleMatchesReference<pcl::PointXYZ, pcl::PointXYZ>(source_indices,
                                                                      target_indices,
                                                                      false);
}

TEST(TransformationEstimationSVDScale, CorrespondenceScaleMatchesReference)
{
  const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
  const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
  expectRowSourceScaleMatchesReference<pcl::PointXYZ, pcl::PointXYZ>(source_indices,
                                                                      target_indices,
                                                                      true);
}

// correspondence sorted-copy production probe（对应关系排序副本生产探针）只在足够大且
// 非规则 shuffle-like disorder（类似洗牌的非规则乱序）时接管 public path。
// 这里用 64K shuffled correspondences 证明新分支仍与 selected-cloud reference 对齐。
TEST(TransformationEstimationSVDScale, CorrespondenceSortedCopyProductionProbeMatchesReference)
{
  const pcl::Indices source_indices = support::makeDeterministicShuffledIndices(
      support::makeStrideIndices(65536, 2, 1));
  const pcl::Indices target_indices = support::makeDeterministicShuffledIndices(
      support::makeStrideIndices(65536, 2, 0));
  const auto source = support::makePointXYZCloud(source_indices.size() * 2);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);

  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  Eigen::Matrix4f probe_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, probe_matrix);

  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(selected_source, selected_target);
  expectMatrixNear(probe_matrix, reference_matrix, 5e-4f);
}

// Phase 075 回滚 sorted-copy（排序副本）double 分流后，shuffled correspondence
// 的 `Scalar=double` 公开入口仍应由 D64 gather RVV path 接管，并与 selected-cloud
// double reference 对齐。
TEST(TransformationEstimationSVDScale,
     CorrespondenceScalarDoubleGatherProductionPathMatchesReference)
{
  const pcl::Indices source_indices = support::makeDeterministicShuffledIndices(
      support::makeStrideIndices(65536, 2, 1));
  const pcl::Indices target_indices = support::makeDeterministicShuffledIndices(
      support::makeStrideIndices(65536, 2, 0));
  const auto source = support::makePointXYZCloud(source_indices.size() * 2);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);

  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, double>
      estimator;
  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  Eigen::Matrix4d probe_matrix = Eigen::Matrix4d::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, probe_matrix);

  const Eigen::Matrix4d reference_matrix =
      support::estimateScaleStdDouble(selected_source, selected_target);
  expectMatrixNear(probe_matrix, reference_matrix, 5e-8);

#ifdef __RVV10__
  Eigen::Matrix4d detail_matrix = Eigen::Matrix4d::Identity();
  EXPECT_TRUE(pcl::registration::detail::
                  estimateRigidTransformationSVDScaleCorrespondencePairRVV(
                      source, target, correspondences, detail_matrix));
  expectMatrixNear(detail_matrix, reference_matrix, 5e-8);
#endif
}

// row-source generic xyz AoS（按索引 / 对应关系取行的泛型 xyz 数组结构）证据
// 只覆盖代表点型和 `Scalar=float`，不外推到所有自定义点型或 `double`。
TEST(TransformationEstimationSVDScale, RowSourceGenericXYZPointTypesMatchReference)
{
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = source_indices;
    expectRowSourceScaleMatchesReference<pcl::PointXYZI, pcl::PointXYZI>(
        source_indices, target_indices, false);
  }
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    expectRowSourceScaleMatchesReference<pcl::PointXYZRGB, pcl::PointXYZRGB>(
        source_indices, target_indices, false);
  }
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    expectRowSourceScaleMatchesReference<pcl::PointXYZI, pcl::PointXYZRGB>(
        source_indices, target_indices, true);
  }
}

// Phase 053 继续把 more-generic xyz AoS（更多常见 xyz 结构数组）点型扩到
// row-source public overload。这里仍只覆盖具体代表组合，不外推到全部自定义点型。
TEST(TransformationEstimationSVDScale, RowSourceMoreGenericXYZAoSPointTypesMatchReference)
{
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = source_indices;
    expectRowSourceScaleMatchesReference<pcl::PointXYZRGBA, pcl::PointXYZRGBA>(
        source_indices, target_indices, false);
  }
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    expectRowSourceScaleMatchesReference<pcl::PointNormal, pcl::PointXYZRGB>(
        source_indices, target_indices, false);
  }
  {
    const pcl::Indices source_indices = support::makeStrideIndices(4096, 2, 1);
    const pcl::Indices target_indices = support::makeStrideIndices(4096, 2, 0);
    expectRowSourceScaleMatchesReference<pcl::PointWithViewpoint, pcl::PointXYZ>(
        source_indices, target_indices, true);
  }
}

// Phase 054 补齐 Phase 051 的五个 more-generic xyz AoS 组合在三类 row-source
// 公开入口下的全交叉代表矩阵。它只扩证据边界，不改变 production gate。
TEST(TransformationEstimationSVDScale, RowSourceAllMoreGenericXYZAoSMatrixMatchesReference)
{
  expectAllRowSourcesMoreGenericComboMatchesReference<pcl::PointXYZRGBA,
                                                      pcl::PointXYZRGBA>();
  expectAllRowSourcesMoreGenericComboMatchesReference<pcl::PointXYZL,
                                                      pcl::PointXYZ>();
  expectAllRowSourcesMoreGenericComboMatchesReference<pcl::PointNormal,
                                                      pcl::PointXYZRGB>();
  expectAllRowSourcesMoreGenericComboMatchesReference<pcl::PointWithRange,
                                                      pcl::PointWithRange>();
  expectAllRowSourcesMoreGenericComboMatchesReference<pcl::PointWithViewpoint,
                                                      pcl::PointXYZ>();
}

// Phase 055 取样自定义 xyz AoS layout（结构数组布局）：source / target 都是
// 测试本地注册点型，x/y/z offset 和 stride 不同于常见 PCL 点型。这个测试只证明
// 当前 traits-gated public path 能处理这些具体 layout，不外推到任意自定义点型全集。
TEST(TransformationEstimationSVDScale, CustomXYZAoSLayoutSamplingMatchesReference)
{
  expectCustomLayoutOffsetsAreSampled();
  expectGenericXYZPublicPathMatchesReference<LocalSVDScalePaddedXYZSource,
                                             LocalSVDScaleWideXYZTarget>();
  expectAllRowSourcesMoreGenericComboMatchesReference<LocalSVDScalePaddedXYZSource,
                                                      LocalSVDScaleWideXYZTarget>();
}

// Phase 057 继续取样 custom layout padding / stride。新增的 compact-ish 与
// huge-padding 点型只证明当前这些 registered xyz AoS 组合能通过 adopted public path，
// 不把证据扩大到全部自定义点型。
TEST(TransformationEstimationSVDScale, CustomLayoutPaddingSensitivityMatchesReference)
{
  expectCustomPaddingLayoutOffsetsAreSampled();
  expectGenericXYZPublicPathMatchesReference<LocalSVDScaleCompactXYZSource,
                                             LocalSVDScaleCompactXYZTarget>();
  expectAllRowSourcesMoreGenericComboMatchesReference<LocalSVDScaleCompactXYZSource,
                                                      LocalSVDScaleCompactXYZTarget>();
  expectGenericXYZPublicPathMatchesReference<LocalSVDScaleHugePaddingXYZSource,
                                             LocalSVDScaleHugePaddingXYZTarget>();
  expectAllRowSourcesMoreGenericComboMatchesReference<LocalSVDScaleHugePaddingXYZSource,
                                                      LocalSVDScaleHugePaddingXYZTarget>();
}

// Phase 059 取样 alignment sensitivity（对齐敏感性）：这些点型显式提高
// `alignof(PointT)` 并扩大 stride，只证明这个采样组合能通过当前 traits-gated public path。
TEST(TransformationEstimationSVDScale, CustomLayoutAlignmentSensitivityMatchesReference)
{
  expectCustomAlignmentLayoutOffsetsAreSampled();
  expectGenericXYZPublicPathMatchesReference<LocalSVDScaleAligned64XYZSource,
                                             LocalSVDScaleAligned32XYZTarget>();
  expectAllRowSourcesMoreGenericComboMatchesReference<LocalSVDScaleAligned64XYZSource,
                                                      LocalSVDScaleAligned32XYZTarget>();
}

// Phase 060 的 affine index fast path（等差索引快速路径）候选只覆盖 step=1 的
// contiguous offset slice（连续偏移片段）。这里让 source-indexed、dual-indexed 和
// correspondence 共享同一个 selected-cloud reference，先证明诊断 candidate 没有改变点对集合。
TEST(TransformationEstimationSVDScale, AffineIndexFastPathCandidateMatchesReference)
{
  constexpr std::size_t kCount = 4096;
  constexpr std::size_t kSourceOffset = 17;
  constexpr std::size_t kTargetOffset = 23;
  const pcl::Indices source_indices =
      support::makeStrideIndices(kCount, 1, kSourceOffset);
  const pcl::Indices target_indices =
      support::makeStrideIndices(kCount, 1, kTargetOffset);
  const auto source = support::makePointXYZCloud(kCount + kSourceOffset + 8);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(selected_source, selected_target);

  support::CandidateStats stats;
  const Eigen::Matrix4f source_indexed_candidate =
      support::estimateScaleContiguousOffsetCandidate(
          source, kSourceOffset, selected_target, 0, kCount, &stats);
  EXPECT_EQ(stats.accepted_points, kCount);
  expectMatrixNear(source_indexed_candidate, reference_matrix, 5e-4f);

  const Eigen::Matrix4f dual_indexed_candidate =
      support::estimateScaleContiguousOffsetCandidate(
          source, kSourceOffset, target, kTargetOffset, kCount, &stats);
  EXPECT_EQ(stats.accepted_points, kCount);
  expectMatrixNear(dual_indexed_candidate, reference_matrix, 5e-4f);

  const Eigen::Matrix4f correspondence_candidate =
      support::estimateScaleContiguousOffsetCandidate(
          source, kSourceOffset, target, kTargetOffset, kCount, &stats);
  EXPECT_EQ(stats.accepted_points, kCount);
  expectMatrixNear(correspondence_candidate, reference_matrix, 5e-4f);
}

// Phase 061 把 affine fast path（等差快速路径）接入 production public overload。
// 这里不读取内部统计，只通过三类公开入口的输出保护 dispatch 命中后的语义。
TEST(TransformationEstimationSVDScale, AffineIndexFastPathPublicProbeMatchesReference)
{
  constexpr std::size_t kCount = 4096;
  constexpr std::size_t kSourceOffset = 17;
  constexpr std::size_t kTargetOffset = 23;
  const pcl::Indices source_indices =
      support::makeStrideIndices(kCount, 1, kSourceOffset);
  const pcl::Indices target_indices =
      support::makeStrideIndices(kCount, 1, kTargetOffset);
  const auto source = support::makePointXYZCloud(kCount + kTargetOffset + 8);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  const auto selected_source = support::selectPointCloudByIndices(source, source_indices);
  const auto selected_target = support::selectPointCloudByIndices(target, target_indices);
  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(selected_source, selected_target);

  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ, pcl::PointXYZ, float>
      estimator;
  Eigen::Matrix4f source_indexed_matrix = Eigen::Matrix4f::Identity();
  const auto source_indexed_target =
      support::selectPointCloudByIndices(target, source_indices);
  estimator.estimateRigidTransformation(source,
                                        source_indices,
                                        source_indexed_target,
                                        source_indexed_matrix);
  expectMatrixNear(source_indexed_matrix,
                   support::estimateScaleStd(selected_source, source_indexed_target),
                   5e-4f);

  Eigen::Matrix4f dual_indexed_matrix = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source,
                                        source_indices,
                                        target,
                                        target_indices,
                                        dual_indexed_matrix);
  expectMatrixNear(dual_indexed_matrix, reference_matrix, 5e-4f);

  Eigen::Matrix4f correspondence_matrix = Eigen::Matrix4f::Identity();
  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  estimator.estimateRigidTransformation(source, target, correspondences, correspondence_matrix);
  expectMatrixNear(correspondence_matrix, reference_matrix, 5e-4f);
}

// Phase 046 的 sorted-copy（排序副本）候选会改变 reduction order（规约顺序），
// 但不应改变 shuffled row-source 输入代表的点对集合。这里把 current public path
// 和 sorted-copy path 都对到同一个 selected-cloud reference，给 detail A/B 提供
// correctness guard（正确性保护）。
TEST(TransformationEstimationSVDScale, RowSourceSortedCopyMatchesShuffledPublicPath)
{
  const pcl::Indices source_indices =
      support::makeDeterministicShuffledIndices(support::makeStrideIndices(4096, 2, 1));
  const pcl::Indices target_indices =
      support::makeDeterministicShuffledIndices(support::makeStrideIndices(4096, 2, 0));
  const auto source = support::makePointXYZCloud(source_indices.size() * 2);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(support::selectPointCloudByIndices(source, source_indices),
                                support::selectPointCloudByIndices(target, target_indices));

  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ,
                                                      pcl::PointXYZ,
                                                      float>
      estimator;
  Eigen::Matrix4f current_dual_indexed = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source,
                                        source_indices,
                                        target,
                                        target_indices,
                                        current_dual_indexed);
  const auto sorted_pairs =
      support::makeSortedIndexPairsBySource(source_indices, target_indices);
  Eigen::Matrix4f sorted_dual_indexed = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source,
                                        sorted_pairs.first,
                                        target,
                                        sorted_pairs.second,
                                        sorted_dual_indexed);

  const pcl::Correspondences correspondences =
      support::makeCorrespondences(source_indices, target_indices);
  Eigen::Matrix4f current_correspondence = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source, target, correspondences, current_correspondence);
  const pcl::Correspondences sorted_correspondences =
      support::makeSortedCorrespondencesByQueryIndex(correspondences);
  Eigen::Matrix4f sorted_correspondence = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source,
                                        target,
                                        sorted_correspondences,
                                        sorted_correspondence);

  expectMatrixNear(current_dual_indexed, reference_matrix, 5e-4f);
  expectMatrixNear(sorted_dual_indexed, reference_matrix, 5e-4f);
  expectMatrixNear(current_correspondence, reference_matrix, 5e-4f);
  expectMatrixNear(sorted_correspondence, reference_matrix, 5e-4f);
}

// target-sorted（按 target index 排序）只改变双索引输入的规约顺序，不应改变
// selected-cloud reference（选中子云参考）的数学结果。这里把 current path 和
// target-sorted path 都对到同一个 reference。
TEST(TransformationEstimationSVDScale, RowSourceTargetSortedMatchesShuffledPublicPath)
{
  const pcl::Indices source_indices =
      support::makeDeterministicShuffledIndices(support::makeStrideIndices(4096, 2, 1));
  const pcl::Indices target_indices =
      support::makeDeterministicShuffledIndices(support::makeStrideIndices(4096, 2, 0));
  const auto source = support::makePointXYZCloud(source_indices.size() * 2);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());
  const Eigen::Matrix4f reference_matrix =
      support::estimateScaleStd(support::selectPointCloudByIndices(source, source_indices),
                                support::selectPointCloudByIndices(target, target_indices));

  pcl::registration::TransformationEstimationSVDScale<pcl::PointXYZ,
                                                      pcl::PointXYZ,
                                                      float>
      estimator;
  Eigen::Matrix4f current_dual_indexed = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source,
                                        source_indices,
                                        target,
                                        target_indices,
                                        current_dual_indexed);

  const auto sorted_pairs =
      support::makeSortedIndexPairsByTarget(source_indices, target_indices);
  Eigen::Matrix4f target_sorted_dual_indexed = Eigen::Matrix4f::Identity();
  estimator.estimateRigidTransformation(source,
                                        sorted_pairs.first,
                                        target,
                                        sorted_pairs.second,
                                        target_sorted_dual_indexed);

  expectMatrixNear(current_dual_indexed, reference_matrix, 5e-4f);
  expectMatrixNear(target_sorted_dual_indexed, reference_matrix, 5e-4f);
}

// 小规模输入走 fallback（回退路径），避免把一次 3x3 SVD 周围的短数组送入 RVV setup
// cost（向量设置成本）更高的路径。
TEST(TransformationEstimationSVDScale, SmallInputFallsBack)
{
  const auto source = support::makePointXYZCloud(8);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f reference_matrix = support::estimateScaleStd(source, target);
  const Eigen::Matrix4f candidate_matrix =
      support::estimateScaleCandidate(source, target, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  expectMatrixNear(candidate_matrix, reference_matrix, 5e-4f);
}

// source variance（source 方差）为 0 时 scale 分母为 0。candidate 不能把这种退化样本
// 写成 RVV positive；本阶段只确认它不会误报 `used_rvv`。
TEST(TransformationEstimationSVDScale, DegenerateSourceFallsBack)
{
  const auto source = support::makeDegeneratePointXYZCloud(64);
  const auto target =
      support::transformCloudXYZ(source, support::makeSimilarityTransform());

  support::CandidateStats stats;
  const Eigen::Matrix4f candidate_matrix =
      support::estimateScaleCandidate(source, target, &stats);

  EXPECT_FALSE(stats.used_rvv);
  EXPECT_TRUE(stats.used_fallback);
  (void)candidate_matrix;
}
