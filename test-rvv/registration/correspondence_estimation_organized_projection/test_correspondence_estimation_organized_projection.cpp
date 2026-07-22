#include "correspondence_estimation_organized_projection_diag.hpp"

#include <pcl/common/rvv_point_traits.h>
#include <pcl/test/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>

namespace diag = pcl::registration::correspondence_estimation_organized_projection_diag;

namespace {

static_assert(pcl::rvv::RVVXYZFloatLayout<pcl::PointXYZ>::value);
static_assert(pcl::rvv::RVVXYZFloatLayout<pcl::PointXYZI>::value);
static_assert(!pcl::rvv::RVVXYZNormalFloatLayout<pcl::PointXYZI>::value);
static_assert(pcl::rvv::rvvMaxU32ByteOffsetElements<pcl::PointXYZ>() ==
              static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() /
                                       sizeof(pcl::PointXYZ)));

std::uint32_t
floatBits(float value)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

#if defined(__RVV10__)
std::vector<float>
computeDistancesRVV(const std::vector<float>& dx,
                    const std::vector<float>& dy,
                    const std::vector<float>& dz)
{
  std::vector<float> distances(dx.size());
  std::size_t i = 0;
  while (i < dx.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(dx.size() - i);
    const vfloat32m2_t vx = __riscv_vle32_v_f32m2(dx.data() + i, vl);
    const vfloat32m2_t vy = __riscv_vle32_v_f32m2(dy.data() + i, vl);
    const vfloat32m2_t vz = __riscv_vle32_v_f32m2(dz.data() + i, vl);
    vfloat32m2_t dist2 = __riscv_vfmul_vv_f32m2(vx, vx, vl);
    dist2 = __riscv_vfmacc_vv_f32m2(dist2, vy, vy, vl);
    dist2 = __riscv_vfmacc_vv_f32m2(dist2, vz, vz, vl);
    const vfloat32m2_t dist = __riscv_vfsqrt_v_f32m2(dist2, vl);
    __riscv_vse32_v_f32m2(distances.data() + i, dist, vl);
    i += vl;
  }
  return distances;
}

bool
rvvDistanceThresholdAccepts(float distance, double max_distance)
{
  const float threshold = static_cast<float>(max_distance);
  if (static_cast<double>(threshold) < max_distance)
    return distance <= threshold;
  return distance < threshold;
}
#endif

// Baseline identity-transform data used by the helper, diagnostic-class and
// production tests. The NaN source and negative-depth source exercise the
// source finite and transformed z predicates; the target edits exercise the
// scalar tail's target finite and depth rejection paths.
pcl::PointCloud<pcl::PointXYZ>::Ptr
makeSource(std::size_t n)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = false;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const int u = static_cast<int>(i % 320);
    const int v = static_cast<int>((i / 320) % 240);
    const float z = 2.0f + static_cast<float>((i * 17) % 31) * 0.01f;
    (*cloud)[i].x = (static_cast<float>(u) - 160.0f) * z / 300.0f;
    (*cloud)[i].y = (static_cast<float>(v) - 120.0f) * z / 310.0f;
    (*cloud)[i].z = z;
  }
  if (n > 137)
    (*cloud)[137].x = std::numeric_limits<float>::quiet_NaN();
  if (n > 911)
    (*cloud)[911].z = -0.5f;
  return cloud;
}

template <typename PointT>
pcl::PointCloud<PointT>
copyXYZCloud(const pcl::PointCloud<pcl::PointXYZ>& input)
{
  // Keep geometry identical while changing the point type. The tests use this
  // to prove RVV traits and scalar-tail access are driven by x/y/z fields, not
  // by an assumed PointXYZ-only layout.
  pcl::PointCloud<PointT> output;
  output.width = input.width;
  output.height = input.height;
  output.is_dense = input.is_dense;
  output.points.resize(input.points.size());
  for (std::size_t i = 0; i < input.points.size(); ++i) {
    output.points[i].x = input.points[i].x;
    output.points[i].y = input.points[i].y;
    output.points[i].z = input.points[i].z;
  }
  return output;
}

template <typename PointT>
typename pcl::PointCloud<PointT>::Ptr
makeCloudPtr(const pcl::PointCloud<PointT>& input)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<PointT>>();
  *cloud = input;
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeTarget(std::uint32_t width, std::uint32_t height)
{
  // Organized target whose pixels correspond to makeSource() under the default
  // camera parameters. Two edited pixels force the scalar tail to exercise depth
  // and target-finite rejection after RVV source staging.
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = width;
  cloud->height = height;
  cloud->is_dense = false;
  cloud->points.resize(static_cast<std::size_t>(width) * height);
  for (std::uint32_t v = 0; v < height; ++v) {
    for (std::uint32_t u = 0; u < width; ++u) {
      const float z = 2.0f + static_cast<float>(((u + 7 * v) % 31)) * 0.01f;
      auto& p = cloud->at(u, v);
      p.x = (static_cast<float>(u) - 160.0f) * z / 300.0f;
      p.y = (static_cast<float>(v) - 120.0f) * z / 310.0f;
      p.z = z;
    }
  }
  cloud->at(23, 11).z += 5.0f;
  cloud->at(41, 19).x = std::numeric_limits<float>::infinity();
  return cloud;
}

void
makeDistanceBoundaryClouds(pcl::PointCloud<pcl::PointXYZ>::Ptr& source,
                           pcl::PointCloud<pcl::PointXYZ>::Ptr& target,
                           diag::ProjectionParams& params)
{
  constexpr std::uint32_t width = 32;
  constexpr std::uint32_t height = 3;
  constexpr std::size_t n = static_cast<std::size_t>(width) * height;

  source = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  target = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  source->width = static_cast<std::uint32_t>(n);
  source->height = 1;
  source->is_dense = true;
  source->points.resize(n);
  target->width = width;
  target->height = height;
  target->is_dense = true;
  target->points.resize(n);

  params = diag::ProjectionParams{};
  params.fx = 1.0f;
  params.fy = 1.0f;
  params.cx = 0.0f;
  params.cy = 0.0f;
  params.depth_threshold = 0.0f;
  const float boundary = 0.03125f;
  const float boundary_next =
      std::nextafterf(boundary, std::numeric_limits<float>::infinity());
  params.max_distance =
      std::nextafter(static_cast<double>(boundary),
                     std::numeric_limits<double>::infinity());

  for (std::uint32_t v = 0; v < height; ++v) {
    for (std::uint32_t u = 0; u < width; ++u) {
      const std::size_t idx = static_cast<std::size_t>(v) * width + u;
      auto& src = (*source)[idx];
      auto& tgt = target->at(u, v);
      src.x = static_cast<float>(u);
      src.y = static_cast<float>(v);
      src.z = 1.0f;
      tgt = src;
      if ((idx % 3u) == 0u)
        tgt.x = src.x - boundary;
      else if ((idx % 3u) == 1u)
        tgt.x = src.x - boundary_next;
      else
        tgt.x = src.x - 0.25f;
    }
  }
}

pcl::Indices
makeIndices(std::size_t n)
{
  pcl::Indices indices(n);
  for (std::size_t i = 0; i < n; ++i)
    indices[i] = static_cast<int>(i);
  return indices;
}

pcl::Indices
makeShuffledDuplicateIndices(std::size_t n)
{
  // The production entry must preserve caller-visible order, including duplicate
  // query indices. This index set catches implementations that accidentally sort,
  // uniquify or write compressed lanes in source-index order instead of scan order.
  pcl::Indices indices;
  indices.reserve(n + n / 16);
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t mapped = (i * 37u + 11u) % n;
    indices.push_back(static_cast<int>(mapped));
    if ((i % 16u) == 0u)
      indices.push_back(static_cast<int>(mapped));
  }
  return indices;
}

diag::ProjectionParams
makeParams()
{
  // Identity transform baseline. The focal lengths and centers are chosen so
  // makeSource() projects onto makeTarget() with integer pixel coordinates.
  diag::ProjectionParams params;
  params.fx = 300.0f;
  params.fy = 310.0f;
  params.cx = 160.0f;
  params.cy = 120.0f;
  params.depth_threshold = 0.08f;
  params.max_distance = 0.15;
  return params;
}

void
expectSameCorrespondences(const pcl::Correspondences& expected,
                          const pcl::Correspondences& actual)
{
  // The RVV path may only differ by tiny distance rounding. Query index, target
  // index and correspondence count are exact behavioral outputs and must match.
  ASSERT_EQ(expected.size(), actual.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ASSERT_EQ(expected[i].index_query, actual[i].index_query) << "mismatch at " << i;
    ASSERT_EQ(expected[i].index_match, actual[i].index_match) << "mismatch at " << i;
    ASSERT_NEAR(expected[i].distance, actual[i].distance, 1e-6f) << "mismatch at " << i;
  }
}

template <typename PointSource, typename PointTarget>
diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                           PointTarget,
                                                           float,
                                                           diag::DiagMode::Candidate>
makeCandidateDiagnostic(const typename pcl::PointCloud<PointSource>::ConstPtr& source,
                        const typename pcl::PointCloud<PointTarget>::ConstPtr& target,
                        const diag::ProjectionParams& params)
{
  // Test-only class that inherits the upstream CEOP object state, then routes
  // determineCorrespondences() into the RVV source-staging diagnostic helper.
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                             PointTarget,
                                                             float,
                                                             diag::DiagMode::Candidate>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  return ce;
}

template <typename PointSource, typename PointTarget>
diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                           PointTarget,
                                                           float,
                                                           diag::DiagMode::ProjectedIdentity>
makeProjectedIdentityDiagnostic(
    const typename pcl::PointCloud<PointSource>::ConstPtr& source,
    const typename pcl::PointCloud<PointTarget>::ConstPtr& target,
    const diag::ProjectionParams& params)
{
  // Projection-pixel diagnostic class. It intentionally remains identity-only
  // because production has not moved u/v calculation ahead of the scalar tail.
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                             PointTarget,
                                                             float,
                                                             diag::DiagMode::ProjectedIdentity>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  return ce;
}

template <typename PointSource, typename PointTarget>
diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                           PointTarget,
                                                           float,
                                                           diag::DiagMode::Projected>
makeProjectedDiagnostic(const typename pcl::PointCloud<PointSource>::ConstPtr& source,
                        const typename pcl::PointCloud<PointTarget>::ConstPtr& target,
                        const diag::ProjectionParams& params)
{
  // Projection-pixel diagnostic without the identity gate. It is used to prove
  // non-identity transform staging and RVV u/v staging compose correctly before
  // any production gate is expanded.
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                             PointTarget,
                                                             float,
                                                             diag::DiagMode::Projected>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  return ce;
}

template <typename PointSource, typename PointTarget>
diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                           PointTarget,
                                                           float,
                                                           diag::DiagMode::Accepted>
makeAcceptedDiagnostic(const typename pcl::PointCloud<PointSource>::ConstPtr& source,
                       const typename pcl::PointCloud<PointTarget>::ConstPtr& target,
                       const diag::ProjectionParams& params)
{
  // Target-predicate diagnostic class. It starts after projected pixel staging,
  // gathers target xyz in RVV, compresses accepted matches, and leaves only the
  // final pcl::Correspondence append in scalar code.
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                             PointTarget,
                                                             float,
                                                             diag::DiagMode::Accepted>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  return ce;
}

template <typename PointSource, typename PointTarget>
diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                           PointTarget,
                                                           float,
                                                           diag::DiagMode::Std>
makeStdDiagnostic(const typename pcl::PointCloud<PointSource>::ConstPtr& source,
                  const typename pcl::PointCloud<PointTarget>::ConstPtr& target,
                  const diag::ProjectionParams& params)
{
  // Same inherited object state as the diagnostic classes, but routed to the
  // scalar reference path. This catches mistakes in initCompute(), fake indices
  // or setIndices() handling separately from RVV math.
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource,
                                                             PointTarget,
                                                             float,
                                                             diag::DiagMode::Std>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  return ce;
}

template <typename PointSource, typename PointTarget>
pcl::registration::CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, float>
makeProductionCe(const typename pcl::PointCloud<PointSource>::ConstPtr& source,
                 const typename pcl::PointCloud<PointTarget>::ConstPtr& target,
                 const diag::ProjectionParams& params)
{
  // Real upstream class under test. Production direct tests use this instead of
  // the diagnostic subclass so the public implementation and fallback gates are
  // what gets exercised.
  pcl::registration::CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, float>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  return ce;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeFarTarget(std::uint32_t width, std::uint32_t height)
{
  // Non-identity boundary tests overwrite only the pixels that the scalar path
  // selects. All other pixels stay far away so a one-pixel projection mismatch
  // becomes an obvious missing or different correspondence.
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = width;
  cloud->height = height;
  cloud->is_dense = false;
  cloud->points.resize(static_cast<std::size_t>(width) * height);
  for (auto& point : cloud->points) {
    point.x = 0.0f;
    point.y = 0.0f;
    point.z = 1000.0f;
  }
  return cloud;
}

diag::ProjectionParams
makeNonIdentityBoundaryParams()
{
  // Non-identity transform used for Eigen/RVV FMA-order validation. The matrix
  // mixes x/y/z plus translation but stays close to identity so projected pixels
  // remain inside the organized target.
  auto params = makeParams();
  params.depth_threshold = 0.02f;
  params.max_distance = 0.05;
  params.transform = Eigen::Matrix4f::Identity();
  params.transform(0, 0) = 1.0013f;
  params.transform(0, 1) = -0.017f;
  params.transform(0, 2) = 0.009f;
  params.transform(0, 3) = 0.031f;
  params.transform(1, 0) = 0.013f;
  params.transform(1, 1) = 0.9987f;
  params.transform(1, 2) = -0.011f;
  params.transform(1, 3) = -0.027f;
  params.transform(2, 0) = -0.007f;
  params.transform(2, 1) = 0.005f;
  params.transform(2, 2) = 1.0021f;
  params.transform(2, 3) = 0.019f;
  return params;
}

void
fillNonIdentityBoundaryData(pcl::PointCloud<pcl::PointXYZ>& source,
                            pcl::PointCloud<pcl::PointXYZ>& target,
                            const diag::ProjectionParams& params,
                            bool small_z = false)
{
  // Build adversarial source points by first choosing transformed coordinates
  // whose projected u value is close to an integer boundary, then applying the
  // inverse transform. This makes 1-ulp transform differences visible after
  // static_cast<int>(uv / z), while target pixels are filled from the scalar
  // Eigen result so the scalar reference remains authoritative.
  const Eigen::Matrix4f inverse = params.transform.inverse();
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float z = (small_z ? 0.075f : 2.0f) + static_cast<float>(i % 17) * 0.01f;
    const float u_projected =
        20.0f + static_cast<float>(i % 220u) +
        ((i % 2) == 0 ? 0.00003f : 0.99997f);
    const float v_projected =
        40.0f + static_cast<float>((i * 37u) % 160u) + 0.25f;

    Eigen::Vector4f transformed;
    transformed[0] = (u_projected - params.cx) * z / params.fx;
    transformed[1] = (v_projected - params.cy) * z / params.fy;
    transformed[2] = z;
    transformed[3] = 1.0f;

    const Eigen::Vector4f source_point = inverse * transformed;
    source[i].x = source_point[0];
    source[i].y = source_point[1];
    source[i].z = source_point[2];

    const Eigen::Vector4f scalar_transformed =
        params.transform * source[i].getVector4fMap();
    const Eigen::Vector3f p_src3(
        scalar_transformed[0], scalar_transformed[1], scalar_transformed[2]);
    const float uv0 = params.fx * p_src3[0] + params.cx * p_src3[2];
    const float uv1 = params.fy * p_src3[1] + params.cy * p_src3[2];
    const int u = static_cast<int>(uv0 / p_src3[2]);
    const int v = static_cast<int>(uv1 / p_src3[2]);
    if (u >= 0 && u < static_cast<int>(target.width) && v >= 0 &&
        v < static_cast<int>(target.height)) {
      auto& target_point = target.at(static_cast<std::uint32_t>(u),
                                      static_cast<std::uint32_t>(v));
      target_point.x = p_src3[0];
      target_point.y = p_src3[1];
      target_point.z = p_src3[2];
    }
  }
}

void
fillIdentityProjectionBoundaryData(pcl::PointCloud<pcl::PointXYZ>& source,
                                   pcl::PointCloud<pcl::PointXYZ>& target,
                                   const diag::ProjectionParams& params)
{
  // Identity projection-pixel production now moves uv/z truncation into RVV.
  // These points sit close to integer pixel boundaries, and the target is
  // populated from the scalar expression so any contraction mismatch changes
  // the correspondence count or target index.
  for (std::size_t i = 0; i < source.size(); ++i) {
    const float z = 2.0f + static_cast<float>(i % 19) * 0.015f;
    const float u_projected =
        18.0f + static_cast<float>((i * 31u) % 240u) +
        ((i % 2u) == 0u ? 0.00003f : 0.99997f);
    const float v_projected =
        33.0f + static_cast<float>((i * 43u) % 170u) +
        ((i % 3u) == 0u ? 0.00004f : 0.35f);

    source[i].x = (u_projected - params.cx) * z / params.fx;
    source[i].y = (v_projected - params.cy) * z / params.fy;
    source[i].z = z;

    const float uv0 = params.fx * source[i].x + params.cx * source[i].z;
    const float uv1 = params.fy * source[i].y + params.cy * source[i].z;
    const int u = static_cast<int>(uv0 / source[i].z);
    const int v = static_cast<int>(uv1 / source[i].z);
    if (u >= 0 && u < static_cast<int>(target.width) && v >= 0 &&
        v < static_cast<int>(target.height)) {
      auto& target_point = target.at(static_cast<std::uint32_t>(u),
                                      static_cast<std::uint32_t>(v));
      target_point.x = source[i].x;
      target_point.y = source[i].y;
      target_point.z = source[i].z;
    }
  }
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeNonIdentityBoundarySource(std::size_t n)
{
  // The fill routine writes adversarial points after it knows the transform and
  // target. This helper only establishes a correctly shaped unorganized source.
  auto source = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  source->width = static_cast<std::uint32_t>(n);
  source->height = 1;
  source->is_dense = false;
  source->points.resize(n);
  return source;
}

} // namespace

// Low-level free-function candidate: verifies the core RVV staging helper and
// scalar tail against the scalar formula with explicit [0,n) indices.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, CandidateMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesCandidate(*source, *target, indices, params, candidate);
  expectSameCorrespondences(scalar, candidate);
}

// Traits guard for target-only PointXYZI. Extra fields must not force fallback
// or change x/y/z offset handling when the target carries intensity.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, CandidateSupportsPointXYZITarget)
{
  const auto source = makeSource(8192);
  const auto target_xyz = makeTarget(320, 240);
  const pcl::PointCloud<pcl::PointXYZI> target = copyXYZCloud<pcl::PointXYZI>(*target_xyz);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  diag::determineCorrespondencesStd(*source, target, indices, params, scalar);
  diag::determineCorrespondencesCandidate(*source, target, indices, params, candidate);
  expectSameCorrespondences(scalar, candidate);
}

// Traits guard for PointXYZI on both source and target. This catches mistakes in
// source gather offsets as well as target scalar-tail field access.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, CandidateSupportsPointXYZISourceAndTarget)
{
  const auto source_xyz = makeSource(8192);
  const auto target_xyz = makeTarget(320, 240);
  const pcl::PointCloud<pcl::PointXYZI> source = copyXYZCloud<pcl::PointXYZI>(*source_xyz);
  const pcl::PointCloud<pcl::PointXYZI> target = copyXYZCloud<pcl::PointXYZI>(*target_xyz);
  const auto indices = makeIndices(source.size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  diag::determineCorrespondencesStd(source, target, indices, params, scalar);
  diag::determineCorrespondencesCandidate(source, target, indices, params, candidate);
  expectSameCorrespondences(scalar, candidate);
}

// Helper-level fallback: n < 64 should bypass RVV staging and still produce the
// same result as the scalar formula.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, SmallInputFallbackMatchesScalar)
{
  const auto source = makeSource(17);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesCandidate(*source, *target, indices, params, candidate);
  expectSameCorrespondences(scalar, candidate);
}

// Production-shaped diagnostic without explicit setIndices(). This verifies the
// test-only derived class goes through initCompute() and PCLBase fake indices,
// not just the low-level helper with caller-provided indices.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, DiagnosticFakeIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto candidate_ce = makeCandidateDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences diagnostic_std;
  pcl::Correspondences diagnostic_candidate;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  std_ce.determineCorrespondences(diagnostic_std, params.max_distance);
  candidate_ce.determineCorrespondences(diagnostic_candidate, params.max_distance);

  expectSameCorrespondences(scalar, diagnostic_std);
  expectSameCorrespondences(scalar, diagnostic_candidate);
}

// Production-shaped diagnostic with shuffled duplicate indices. It verifies the
// derived diagnostic preserves subset lifetime, duplicate queries and output order.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, DiagnosticSubsetIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeShuffledDuplicateIndices(source->size());
  const auto params = makeParams();

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto candidate_ce = makeCandidateDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  std_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));
  candidate_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences diagnostic_std;
  pcl::Correspondences diagnostic_candidate;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  std_ce.determineCorrespondences(diagnostic_std, params.max_distance);
  candidate_ce.determineCorrespondences(diagnostic_candidate, params.max_distance);

  expectSameCorrespondences(scalar, diagnostic_std);
  expectSameCorrespondences(scalar, diagnostic_candidate);
}

// Sanity check for the hand-written scalar reference. It ensures the reference
// formula used throughout this file matches the upstream PCL class behavior for
// the identity baseline data.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, PclClassMatchesScalarFormula)
{
  const auto source = makeSource(2048);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences pcl_class;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesPclClass(source, target, params, pcl_class);
  expectSameCorrespondences(scalar, pcl_class);
}

// Direct production entry with fake indices. This is the main identity-transform
// production evidence for the public determineCorrespondences() call shape.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, ProductionFakeIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production entry with user-provided shuffled duplicate indices. This
// proves vcompress staging and the scalar tail preserve caller-visible order.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, ProductionSubsetIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeShuffledDuplicateIndices(source->size());
  const auto params = makeParams();

  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production traits case for a PointXYZ source and PointXYZI target.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, ProductionSupportsPointXYZITarget)
{
  const auto source = makeSource(8192);
  const auto target_xyz = makeTarget(320, 240);
  const auto target = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*target_xyz));
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZI>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production traits case where both source and target have extra fields.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, ProductionSupportsPointXYZISourceAndTarget)
{
  const auto source_xyz = makeSource(8192);
  const auto target_xyz = makeTarget(320, 240);
  const auto source = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*source_xyz));
  const auto target = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*target_xyz));
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  auto ce = makeProductionCe<pcl::PointXYZI, pcl::PointXYZI>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production fallback for small inputs. This protects the n < 64 guard.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, ProductionSmallInputFallbackMatchesScalar)
{
  const auto source = makeSource(17);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production distance-threshold boundary case. Some lanes have scalar
// Eigen float norm exactly equal to float(max_distance) while the double
// threshold is one double-ulp above it, so scalar accepts them. The RVV distance
// predicate must mirror double(float_dist) < max_distance rather than use a
// naive strict compare against float(max_distance).
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     ProductionDistanceBoundaryPredicateMatchesScalar)
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr source;
  pcl::PointCloud<pcl::PointXYZ>::Ptr target;
  diag::ProjectionParams params;
  makeDistanceBoundaryClouds(source, target, params);
  const auto indices = makeIndices(source->size());

  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);

  std::size_t boundary_accepts = 0;
  for (const auto& correspondence : scalar) {
    if ((correspondence.index_query % 3) == 0)
      ++boundary_accepts;
  }
  ASSERT_GT(boundary_accepts, 0u);
  ASSERT_LT(scalar.size(), source->size());
}

TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     DistanceRvvMatchesEigenNormBits)
{
#if defined(__RVV10__)
  constexpr std::size_t n = 512;
  std::vector<float> dx(n);
  std::vector<float> dy(n);
  std::vector<float> dz(n);
  for (std::size_t i = 0; i < n; ++i) {
    dx[i] = (static_cast<float>((i * 17u) % 101u) - 50.0f) * 0.00390625f;
    dy[i] = (static_cast<float>((i * 29u) % 113u) - 56.0f) * 0.001953125f;
    dz[i] = (static_cast<float>((i * 37u) % 127u) - 63.0f) * 0.0078125f;
  }

  const auto rvv = computeDistancesRVV(dx, dy, dz);
  for (std::size_t i = 0; i < n; ++i) {
    const Eigen::Vector3f delta(dx[i], dy[i], dz[i]);
    const float scalar = delta.norm();
    ASSERT_EQ(floatBits(scalar), floatBits(rvv[i])) << "mismatch at " << i;
  }
#else
  GTEST_SKIP() << "Requires RVV distance diagnostics.";
#endif
}

TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate)
{
#if defined(__RVV10__)
  const float boundary = 0.03125f;
  const float above_boundary =
      std::nextafterf(boundary, std::numeric_limits<float>::infinity());
  const double thresholds[] = {
      static_cast<double>(boundary),
      std::nextafter(static_cast<double>(boundary),
                     std::numeric_limits<double>::infinity()),
      (static_cast<double>(boundary) + static_cast<double>(above_boundary)) * 0.5,
      static_cast<double>(above_boundary)};
  const float distances[] = {
      std::nextafterf(boundary, 0.0f), boundary, above_boundary};

  for (const double threshold : thresholds) {
    for (const float distance : distances) {
      const bool scalar = static_cast<double>(distance) < threshold;
      const bool rvv_equivalent = rvvDistanceThresholdAccepts(distance, threshold);
      ASSERT_EQ(scalar, rvv_equivalent)
          << "distance=" << distance << " threshold=" << threshold;
    }
  }
#else
  GTEST_SKIP() << "Requires RVV distance diagnostics.";
#endif
}

// Direct production identity projection-pixel boundary case. In RVV builds this
// exercises the production ProjectedOrganizedProjectionCandidate staging path.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     ProductionIdentityProjectionPixelBoundaryMatchesScalar)
{
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeParams();
  params.depth_threshold = 0.002f;
  params.max_distance = 0.01;
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillIdentityProjectionBoundaryData(*source, *target, params);

  const auto indices = makeIndices(source->size());
  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production non-identity gate with fake indices. The data places scalar
// projection results near pixel boundaries, so transform FMA ordering differences
// would change correspondence count or target indices.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     ProductionNonIdentityTransformFakeIndicesMatchesScalar)
{
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  const auto indices = makeIndices(source->size());
  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production non-identity gate with shuffled duplicate indices. This is
// the public-entry counterpart to the diagnostic subset test.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     ProductionNonIdentityTransformSubsetIndicesMatchesScalar)
{
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  const auto indices = makeShuffledDuplicateIndices(source->size());
  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production non-identity projection-pixel boundary case. This covers
// the expanded production gate that composes Eigen-aligned transform staging
// with RVV u/v staging before target access returns to scalar.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     ProductionNonIdentityProjectionPixelBoundaryMatchesScalar)
{
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  const auto indices = makeIndices(source->size());
  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production non-identity gate with small positive z and PointXYZI layout.
// This combines a numerically sensitive depth scale with the production traits
// path used for extra-field point types.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     ProductionNonIdentityTransformSmallZPointXYZIMatchesScalar)
{
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  params.depth_threshold = 0.004f;
  params.max_distance = 0.01;
  auto source_xyz = makeNonIdentityBoundarySource(n);
  auto target_xyz = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source_xyz, *target_xyz, params, true);
  const auto source = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*source_xyz));
  const auto target = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*target_xyz));
  const auto indices = makeIndices(source->size());

  auto ce = makeProductionCe<pcl::PointXYZI, pcl::PointXYZI>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// Direct production fallback for small non-identity input. It keeps the n < 64
// fallback covered after expanding the transform gate.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     ProductionNonIdentitySmallInputFallbackMatchesScalar)
{
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 17;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  const auto indices = makeIndices(source->size());
  auto ce = makeProductionCe<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences production;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  ce.determineCorrespondences(production, params.max_distance);
  expectSameCorrespondences(scalar, production);
}

// RVV-only low-level adversarial transform staging. This isolates the helper
// math from PCL object state and proves the Eigen-aligned FMA structure fixes the
// original pixel-boundary mismatch.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityTransformRvvStagingBoundaryMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  const auto indices = makeIndices(source->size());
  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesCandidate(*source, *target, indices, params, candidate);

  expectSameCorrespondences(scalar, candidate);
#else
  GTEST_SKIP() << "Requires RVV transform staging diagnostics.";
#endif
}

// RVV-only production-shaped diagnostic for non-identity fake indices. It covers
// initCompute() and fake-index state while still exercising the test-only helper.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityTransformDiagnosticFakeIndicesMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto candidate_ce = makeCandidateDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  candidate_ce.determineCorrespondences(candidate, params.max_distance);
  expectSameCorrespondences(scalar, candidate);
#else
  GTEST_SKIP() << "Requires RVV transform staging diagnostics.";
#endif
}

// RVV-only production-shaped diagnostic for non-identity shuffled duplicates.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityTransformDiagnosticSubsetIndicesMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);
  const auto indices = makeShuffledDuplicateIndices(source->size());

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto candidate_ce = makeCandidateDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  std_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));
  candidate_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  candidate_ce.determineCorrespondences(candidate, params.max_distance);
  expectSameCorrespondences(scalar, candidate);
#else
  GTEST_SKIP() << "Requires RVV transform staging diagnostics.";
#endif
}

// RVV-only production-shaped diagnostic that keeps the non-identity
// transform + projection-pixel combination independently covered under fake
// indices. Production direct tests now cover this path; this remains as a
// helper-level semantic probe and regression guard for the earlier FMA mismatch.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto projected_ce = makeProjectedDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(
      source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences projected;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  projected_ce.determineCorrespondences(projected, params.max_distance);
  expectSameCorrespondences(scalar, projected);
#else
  GTEST_SKIP() << "Requires RVV projection-pixel diagnostics.";
#endif
}

// RVV-only production-shaped diagnostic for the same non-identity projected
// path with shuffled duplicate indices. It independently checks projected
// staging order after the production gate was expanded beyond identity.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);
  const auto indices = makeShuffledDuplicateIndices(source->size());

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto projected_ce = makeProjectedDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(
      source, target, params);
  std_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));
  projected_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences projected;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  projected_ce.determineCorrespondences(projected, params.max_distance);
  expectSameCorrespondences(scalar, projected);
#else
  GTEST_SKIP() << "Requires RVV projection-pixel diagnostics.";
#endif
}

// RVV-only diagnostic for small positive z and PointXYZI layout. This mirrors
// the production coverage while keeping the test-only helper as an independent
// semantic probe.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityTransformSmallZPointXYZIMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  params.depth_threshold = 0.004f;
  params.max_distance = 0.01;
  auto source_xyz = makeNonIdentityBoundarySource(n);
  auto target_xyz = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source_xyz, *target_xyz, params, true);
  const auto source = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*source_xyz));
  const auto target = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*target_xyz));
  const auto indices = makeIndices(source->size());

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesCandidate(*source, *target, indices, params, candidate);
  expectSameCorrespondences(scalar, candidate);
#else
  GTEST_SKIP() << "Requires RVV transform staging diagnostics.";
#endif
}

// Threshold-tail guard: the scalar tail must still apply depth rejection after
// RVV staging. With a zero depth threshold, all tiny depth differences matter.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, TightDepthThresholdRejectsMatches)
{
  const auto source = makeSource(4096);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  auto params = makeParams();
  params.depth_threshold = 0.0f;

  pcl::Correspondences scalar;
  pcl::Correspondences candidate;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesCandidate(*source, *target, indices, params, candidate);
  expectSameCorrespondences(scalar, candidate);
}

// Low-level projection-pixel diagnostic. It keeps the vfmacc-based u/v staging
// checked independently from production so pixel-boundary regressions can be
// attributed before target gather and distance predicates are involved.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, ProjectionPixelRvvDiagnosticMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences projected;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesProjectedCandidate(*source, *target, indices, params, projected);

  expectSameCorrespondences(scalar, projected);
}

// Production-shaped projection-pixel diagnostic for fake indices. The identity
// gate mirrors the production boundary for projection-pixel staging.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     IdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto params = makeParams();

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto projected_ce =
      makeProjectedIdentityDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences projected;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  projected_ce.determineCorrespondences(projected, params.max_distance);

  expectSameCorrespondences(scalar, projected);
}

// Production-shaped projection-pixel diagnostic for shuffled duplicate indices.
// It verifies that projected staging keeps source order independently from the
// later target-predicate stage, which production now also RVV-optimizes.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     IdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeShuffledDuplicateIndices(source->size());
  const auto params = makeParams();

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto projected_ce =
      makeProjectedIdentityDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  std_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));
  projected_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences projected;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  projected_ce.determineCorrespondences(projected, params.max_distance);

  expectSameCorrespondences(scalar, projected);
}

// Low-level target-predicate diagnostic. makeTarget() includes a non-finite
// target lane and a depth-rejected target lane, so the RVV target gather mask is
// checked before the append-only scalar tail runs.
TEST(CorrespondenceEstimationOrganizedProjectionDiag, TargetPredicateRvvDiagnosticMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesAcceptedCandidate(*source, *target, indices, params, accepted);

  expectSameCorrespondences(scalar, accepted);
}

// Production-shaped target-predicate diagnostic without explicit setIndices().
// This keeps PCLBase fake-index expansion in front of the new RVV target gather.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     TargetPredicateDiagnosticFakeIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto params = makeParams();

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto accepted_ce = makeAcceptedDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  accepted_ce.determineCorrespondences(accepted, params.max_distance);

  expectSameCorrespondences(scalar, accepted);
}

// Production-shaped target-predicate diagnostic with shuffled duplicate indices.
// It proves target gather and the second vcompress preserve projected scan order.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     TargetPredicateDiagnosticSubsetIndicesMatchesScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeShuffledDuplicateIndices(source->size());
  const auto params = makeParams();

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto accepted_ce = makeAcceptedDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  std_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));
  accepted_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  accepted_ce.determineCorrespondences(accepted, params.max_distance);

  expectSameCorrespondences(scalar, accepted);
}

// Traits guard for the target-gather diagnostic. Source and target use PointXYZI
// so both staging helpers must honor x/y/z field offsets across the full chain.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     TargetPredicateSupportsPointXYZISourceAndTarget)
{
  const auto source_xyz = makeSource(8192);
  const auto target_xyz = makeTarget(320, 240);
  const auto source = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*source_xyz));
  const auto target = makeCloudPtr(copyXYZCloud<pcl::PointXYZI>(*target_xyz));
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesAcceptedCandidate(*source, *target, indices, params, accepted);

  expectSameCorrespondences(scalar, accepted);
}

// Target-predicate fallback guard. The accepted-stage helper refuses tiny input,
// so the wrapper must return through the scalar reference path without changing
// public output.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     TargetPredicateSmallInputFallbackMatchesScalar)
{
  const auto source = makeSource(17);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  const auto params = makeParams();

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesAcceptedCandidate(*source, *target, indices, params, accepted);

  expectSameCorrespondences(scalar, accepted);
}

// Tight distance/depth predicate case for the target-gather stage. It increases
// rejection pressure after projection so accepted staging has to match scalar
// predicate order, not just gather valid target lanes.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     TargetPredicateTightThresholdsMatchScalar)
{
  const auto source = makeSource(8192);
  const auto target = makeTarget(320, 240);
  const auto indices = makeIndices(source->size());
  auto params = makeParams();
  params.depth_threshold = 0.01f;
  params.max_distance = 0.03;

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
  diag::determineCorrespondencesAcceptedCandidate(*source, *target, indices, params, accepted);

  expectSameCorrespondences(scalar, accepted);
}

// Non-identity target-predicate diagnostic. This composes Eigen-aligned source
// transform staging, RVV pixel staging and RVV target predicates while keeping
// the final append scalar.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityTargetPredicateDiagnosticFakeIndicesMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto accepted_ce = makeAcceptedDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  accepted_ce.determineCorrespondences(accepted, params.max_distance);
  expectSameCorrespondences(scalar, accepted);
#else
  GTEST_SKIP() << "Requires RVV target-predicate diagnostics.";
#endif
}

// Non-identity target-predicate diagnostic with shuffled duplicates. This is the
// order-preservation guard for the accepted staging vcompress path.
TEST(CorrespondenceEstimationOrganizedProjectionDiag,
     NonIdentityTargetPredicateDiagnosticSubsetIndicesMatchesScalar)
{
#if defined(__RVV10__)
  constexpr std::uint32_t width = 320;
  constexpr std::uint32_t height = 240;
  constexpr std::size_t n = 192;

  auto params = makeNonIdentityBoundaryParams();
  auto source = makeNonIdentityBoundarySource(n);
  auto target = makeFarTarget(width, height);
  fillNonIdentityBoundaryData(*source, *target, params);
  const auto indices = makeShuffledDuplicateIndices(source->size());

  auto std_ce = makeStdDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  auto accepted_ce = makeAcceptedDiagnostic<pcl::PointXYZ, pcl::PointXYZ>(source, target, params);
  std_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));
  accepted_ce.setIndices(pcl::make_shared<pcl::Indices>(indices));

  pcl::Correspondences scalar;
  pcl::Correspondences accepted;
  std_ce.determineCorrespondences(scalar, params.max_distance);
  accepted_ce.determineCorrespondences(accepted, params.max_distance);
  expectSameCorrespondences(scalar, accepted);
#else
  GTEST_SKIP() << "Requires RVV target-predicate diagnostics.";
#endif
}
