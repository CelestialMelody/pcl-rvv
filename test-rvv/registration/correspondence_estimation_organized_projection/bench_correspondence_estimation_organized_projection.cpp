#include "correspondence_estimation_organized_projection_diag.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace diag = pcl::registration::correspondence_estimation_organized_projection_diag;

namespace {

constexpr int kIterations = 30;
constexpr std::size_t kBannerWidth = 110;

void
printBanner(char ch)
{
  std::cout << std::string(kBannerWidth, ch) << '\n';
}

template <typename T>
inline void
doNotOptimize(const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

class Benchmarker {
public:
  explicit Benchmarker(std::string name) : name_(std::move(name)) {}

  void run(const std::function<void()>& func, int iterations = kIterations, int warmup = 3) const
  {
    for (int i = 0; i < warmup; ++i)
      func();

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i)
      func();
    const auto stop = std::chrono::steady_clock::now();
    const double total_ms = std::chrono::duration<double, std::milli>(stop - start).count();
    std::cout << std::left << std::setw(78) << name_ << ": " << std::fixed
              << std::setprecision(4) << (total_ms / iterations) << " ms/iter\n";
    std::cout << "  Total Time: " << std::fixed << std::setprecision(4) << total_ms
              << " ms, checksum: " << checksum_ << '\n';
  }

  void setChecksum(std::uint64_t checksum) const { checksum_ = checksum; }

private:
  std::string name_;
  mutable std::uint64_t checksum_{0};
};

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeSource(std::size_t n, std::uint32_t width, std::uint32_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = static_cast<std::uint32_t>(n);
  cloud->height = 1;
  cloud->is_dense = false;
  cloud->points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const int u = static_cast<int>((i * 13) % width);
    const int v = static_cast<int>((i * 7) % height);
    const float z = 2.0f + static_cast<float>((i * 17) % 67) * 0.006f;
    (*cloud)[i].x = (static_cast<float>(u) - static_cast<float>(width) * 0.5f) * z / 420.0f;
    (*cloud)[i].y = (static_cast<float>(v) - static_cast<float>(height) * 0.5f) * z / 430.0f;
    (*cloud)[i].z = z;
  }
  for (std::size_t i = 257; i < n; i += 4099)
    (*cloud)[i].x = std::numeric_limits<float>::quiet_NaN();
  for (std::size_t i = 1021; i < n; i += 8191)
    (*cloud)[i].z = -1.0f;
  return cloud;
}

pcl::PointCloud<pcl::PointXYZ>::Ptr
makeTarget(std::uint32_t width, std::uint32_t height)
{
  auto cloud = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>>();
  cloud->width = width;
  cloud->height = height;
  cloud->is_dense = false;
  cloud->points.resize(static_cast<std::size_t>(width) * height);
  for (std::uint32_t v = 0; v < height; ++v) {
    for (std::uint32_t u = 0; u < width; ++u) {
      const float z = 2.0f + static_cast<float>(((u * 5 + v * 11) % 67)) * 0.006f;
      auto& p = cloud->at(u, v);
      p.x = (static_cast<float>(u) - static_cast<float>(width) * 0.5f) * z / 420.0f;
      p.y = (static_cast<float>(v) - static_cast<float>(height) * 0.5f) * z / 430.0f;
      p.z = z;
    }
  }
  for (std::size_t i = 127; i < cloud->size(); i += 32771)
    (*cloud)[i].z += 3.0f;
  return cloud;
}

pcl::Indices
makeIndices(std::size_t n)
{
  pcl::Indices indices(n);
  for (std::size_t i = 0; i < n; ++i)
    indices[i] = static_cast<int>(i);
  return indices;
}

diag::ProjectionParams
makeParams(std::uint32_t width, std::uint32_t height)
{
  diag::ProjectionParams params;
  params.fx = 420.0f;
  params.fy = 430.0f;
  params.cx = static_cast<float>(width) * 0.5f;
  params.cy = static_cast<float>(height) * 0.5f;
  params.depth_threshold = 0.12f;
  params.max_distance = 0.20;
  return params;
}

diag::ProjectionParams
makeNonIdentityParams(std::uint32_t width, std::uint32_t height)
{
  auto params = makeParams(width, height);
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
fillNonIdentityData(pcl::PointCloud<pcl::PointXYZ>& source,
                    pcl::PointCloud<pcl::PointXYZ>& target,
                    const diag::ProjectionParams& params)
{
  const Eigen::Matrix4f inverse = params.transform.inverse();
  for (auto& point : target.points) {
    point.x = 0.0f;
    point.y = 0.0f;
    point.z = 1000.0f;
  }

  for (std::size_t i = 0; i < source.size(); ++i) {
    const float z = 2.0f + static_cast<float>(i % 67) * 0.006f;
    const float u_projected =
        20.0f + static_cast<float>((i * 13u) % (target.width - 40u)) +
        ((i % 2u) == 0u ? 0.00003f : 0.99997f);
    const float v_projected =
        30.0f + static_cast<float>((i * 7u) % (target.height - 60u)) + 0.25f;

    Eigen::Vector4f transformed;
    transformed[0] = (u_projected - params.cx) * z / params.fx;
    transformed[1] = (v_projected - params.cy) * z / params.fy;
    transformed[2] = z;
    transformed[3] = 1.0f;

    const Eigen::Vector4f source_point = inverse * transformed;
    source[i].x = source_point[0];
    source[i].y = source_point[1];
    source[i].z = source_point[2];

    const Eigen::Vector4f scalar_transformed = params.transform * source[i].getVector4fMap();
    const Eigen::Vector3f p_src3(
        scalar_transformed[0], scalar_transformed[1], scalar_transformed[2]);
    const int u =
        static_cast<int>((params.fx * p_src3[0] + params.cx * p_src3[2]) / p_src3[2]);
    const int v =
        static_cast<int>((params.fy * p_src3[1] + params.cy * p_src3[2]) / p_src3[2]);
    if (u >= 0 && u < static_cast<int>(target.width) && v >= 0 &&
        v < static_cast<int>(target.height)) {
      auto& target_point =
          target.at(static_cast<std::uint32_t>(u), static_cast<std::uint32_t>(v));
      target_point.x = p_src3[0];
      target_point.y = p_src3[1];
      target_point.z = p_src3[2];
    }
  }
}

std::uint64_t
checksumCorrespondences(const pcl::Correspondences& correspondences)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto& c : correspondences) {
    hash ^= static_cast<std::uint64_t>(c.index_query + 0x9e3779b9);
    hash *= 1099511628211ull;
    hash ^= static_cast<std::uint64_t>(c.index_match + 0x85ebca6b);
    hash *= 1099511628211ull;
    hash ^= static_cast<std::uint64_t>(std::llround(c.distance * 1000000.0f));
    hash *= 1099511628211ull;
  }
  return hash ^ correspondences.size();
}

std::uint64_t
checksumCandidates(const std::vector<diag::ProjectionCandidate>& candidates)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (std::size_t i = 0; i < candidates.size(); i += 7) {
    const auto& c = candidates[i];
    hash ^= c.source_index + 0x27d4eb2d;
    hash *= 1099511628211ull;
    hash ^= static_cast<std::uint64_t>(std::llround((c.x * 11.0f + c.y * 13.0f) * 100000.0f));
    hash *= 1099511628211ull;
    hash ^= static_cast<std::uint64_t>(std::llround((c.x + c.y + c.z) * 100000.0f));
    hash *= 1099511628211ull;
  }
  return hash ^ candidates.size();
}

std::uint64_t
checksumAcceptedCandidates(const std::vector<diag::AcceptedCandidate>& accepted)
{
  std::uint64_t hash = 1469598103934665603ull;
  for (std::size_t i = 0; i < accepted.size(); i += 7) {
    const auto& c = accepted[i];
    hash ^= c.source_index + 0x9e3779b9;
    hash *= 1099511628211ull;
    hash ^= c.target_index + 0x85ebca6b;
    hash *= 1099511628211ull;
    hash ^= static_cast<std::uint64_t>(std::llround(c.distance * 1000000.0f));
    hash *= 1099511628211ull;
  }
  return hash ^ accepted.size();
}

void
benchProjectionStage(const std::string& name,
                     const pcl::PointCloud<pcl::PointXYZ>& source,
                     const pcl::PointCloud<pcl::PointXYZ>& target,
                     const pcl::Indices& indices,
                     const diag::ProjectionParams& params)
{
  std::vector<diag::ProjectionCandidate> candidates;
  Benchmarker bench(name);
  bench.run([&]() {
    if (!diag::projectCandidatesCandidate(source, target, indices, params, candidates))
      candidates.clear();
    bench.setChecksum(checksumCandidates(candidates));
    doNotOptimize(candidates);
  });
}

void
benchFull(const std::string& name,
          const pcl::PointCloud<pcl::PointXYZ>& source,
          const pcl::PointCloud<pcl::PointXYZ>& target,
          const pcl::Indices& indices,
          const diag::ProjectionParams& params)
{
  pcl::Correspondences correspondences;
  Benchmarker bench(name);
  bench.run([&]() {
    diag::determineCorrespondencesCandidate(source, target, indices, params, correspondences);
    bench.setChecksum(checksumCorrespondences(correspondences));
    doNotOptimize(correspondences);
  });
}

void
benchDiagnosticFull(const std::string& name,
                    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source,
                    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                    const pcl::Indices* indices,
                    const diag::ProjectionParams& params)
{
  pcl::Correspondences correspondences;
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<pcl::PointXYZ,
                                                              pcl::PointXYZ,
                                                              float,
                                                              diag::DiagMode::Candidate>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  if (indices)
    ce.setIndices(pcl::make_shared<pcl::Indices>(*indices));

  Benchmarker bench(name);
  bench.run([&]() {
    ce.determineCorrespondences(correspondences, params.max_distance);
    bench.setChecksum(checksumCorrespondences(correspondences));
    doNotOptimize(correspondences);
  });
}

void
benchAcceptedDiagnosticFull(const std::string& name,
                            const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source,
                            const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                            const pcl::Indices* indices,
                            const diag::ProjectionParams& params)
{
  pcl::Correspondences correspondences;
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<pcl::PointXYZ,
                                                              pcl::PointXYZ,
                                                              float,
                                                              diag::DiagMode::Accepted>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  if (indices)
    ce.setIndices(pcl::make_shared<pcl::Indices>(*indices));

  Benchmarker bench(name);
  bench.run([&]() {
    ce.determineCorrespondences(correspondences, params.max_distance);
    bench.setChecksum(checksumCorrespondences(correspondences));
    doNotOptimize(correspondences);
  });
}

void
benchDiagnosticStdFull(const std::string& name,
                       const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source,
                       const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                       const pcl::Indices* indices,
                       const diag::ProjectionParams& params)
{
  pcl::Correspondences correspondences;
  diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<pcl::PointXYZ,
                                                              pcl::PointXYZ,
                                                              float,
                                                              diag::DiagMode::Std>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  if (indices)
    ce.setIndices(pcl::make_shared<pcl::Indices>(*indices));

  Benchmarker bench(name);
  bench.run([&]() {
    ce.determineCorrespondences(correspondences, params.max_distance);
    bench.setChecksum(checksumCorrespondences(correspondences));
    doNotOptimize(correspondences);
  });
}

void
benchProductionFull(const std::string& name,
                    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& source,
                    const pcl::PointCloud<pcl::PointXYZ>::ConstPtr& target,
                    const pcl::Indices* indices,
                    const diag::ProjectionParams& params)
{
  pcl::Correspondences correspondences;
  pcl::registration::CorrespondenceEstimationOrganizedProjection<pcl::PointXYZ,
                                                                 pcl::PointXYZ,
                                                                 float>
      ce;
  ce.setInputSource(source);
  ce.setInputTarget(target);
  ce.setFocalLengths(params.fx, params.fy);
  ce.setCameraCenters(params.cx, params.cy);
  ce.setSourceTransformation(params.transform);
  ce.setDepthThreshold(params.depth_threshold);
  if (indices)
    ce.setIndices(pcl::make_shared<pcl::Indices>(*indices));

  Benchmarker bench(name);
  bench.run([&]() {
    ce.determineCorrespondences(correspondences, params.max_distance);
    bench.setChecksum(checksumCorrespondences(correspondences));
    doNotOptimize(correspondences);
  });
}

void
benchProjectedDiagnostic(const std::string& name,
                         const pcl::PointCloud<pcl::PointXYZ>& source,
                         const pcl::PointCloud<pcl::PointXYZ>& target,
                         const pcl::Indices& indices,
                         const diag::ProjectionParams& params)
{
  pcl::Correspondences correspondences;
  Benchmarker bench(name);
  bench.run([&]() {
    diag::determineCorrespondencesProjectedIdentityCandidate(
        source, target, indices, params, correspondences);
    bench.setChecksum(checksumCorrespondences(correspondences));
    doNotOptimize(correspondences);
  });
}

void
benchAcceptedStage(const std::string& name,
                   const pcl::PointCloud<pcl::PointXYZ>& source,
                   const pcl::PointCloud<pcl::PointXYZ>& target,
                   const pcl::Indices& indices,
                   const diag::ProjectionParams& params)
{
  std::vector<diag::ProjectionCandidate> candidates;
  std::vector<diag::ProjectedCandidate> projected;
  std::vector<diag::AcceptedCandidate> accepted;
  Benchmarker bench(name);
  bench.run([&]() {
    if (diag::projectCandidatesCandidate(source, target, indices, params, candidates) &&
        diag::projectPixelsCandidate(target, params, candidates, projected) &&
        diag::acceptProjectedCandidatesCandidate(target, params, projected, accepted)) {
      bench.setChecksum(checksumAcceptedCandidates(accepted));
    } else {
      accepted.clear();
      bench.setChecksum(0);
    }
    doNotOptimize(accepted);
  });
}

void
benchAcceptedDiagnostic(const std::string& name,
                        const pcl::PointCloud<pcl::PointXYZ>& source,
                        const pcl::PointCloud<pcl::PointXYZ>& target,
                        const pcl::Indices& indices,
                        const diag::ProjectionParams& params)
{
  pcl::Correspondences correspondences;
  Benchmarker bench(name);
  bench.run([&]() {
    diag::determineCorrespondencesAcceptedCandidate(
        source, target, indices, params, correspondences);
    bench.setChecksum(checksumCorrespondences(correspondences));
    doNotOptimize(correspondences);
  });
}

} // namespace

int
main()
{
  printBanner('=');
  std::cout << "PCL registration/correspondence_estimation_organized_projection diagnostic benchmark\n";
#if defined(__RVV10__)
  std::cout << "Build: RVV (__RVV10__ enabled)\n";
#else
  std::cout << "Build: Std (__RVV10__ disabled)\n";
#endif
  std::cout << "Dataset: synthetic PointXYZ source and organized PointXYZ target; projection staging plus full correspondence diagnostic\n";
  std::cout << "Iterations: " << kIterations << '\n';
  printBanner('-');

  constexpr std::uint32_t width = 640;
  constexpr std::uint32_t height = 480;
  const auto target = makeTarget(width, height);
  const auto params = makeParams(width, height);
  const auto source_64k = makeSource(64 * 1024, width, height);
  const auto source_256k = makeSource(256 * 1024, width, height);
  const auto indices_64k = makeIndices(source_64k->size());
  const auto indices_256k = makeIndices(source_256k->size());
  const auto non_identity_params = makeNonIdentityParams(width, height);
  const auto non_identity_source_64k = makeSource(64 * 1024, width, height);
  const auto non_identity_target = makeTarget(width, height);
  fillNonIdentityData(*non_identity_source_64k, *non_identity_target, non_identity_params);
  const auto non_identity_indices_64k = makeIndices(non_identity_source_64k->size());

  benchProjectionStage("ceop projection-staging pointxyz 64K", *source_64k, *target, indices_64k, params);
  benchProjectionStage("ceop projection-staging pointxyz 256K", *source_256k, *target, indices_256k, params);
  benchProjectedDiagnostic("ceop identity projection-pixel diagnostic pointxyz 64K", *source_64k, *target, indices_64k, params);
  benchProjectedDiagnostic("ceop identity projection-pixel diagnostic pointxyz 256K", *source_256k, *target, indices_256k, params);
  benchAcceptedStage("ceop target-predicate staging pointxyz 64K", *source_64k, *target, indices_64k, params);
  benchAcceptedStage("ceop target-predicate staging pointxyz 256K", *source_256k, *target, indices_256k, params);
  benchAcceptedDiagnostic("ceop identity target-predicate diagnostic pointxyz 64K", *source_64k, *target, indices_64k, params);
  benchAcceptedDiagnostic("ceop identity target-predicate diagnostic pointxyz 256K", *source_256k, *target, indices_256k, params);
  benchFull("ceop full-correspondence pointxyz 64K", *source_64k, *target, indices_64k, params);
  benchFull("ceop full-correspondence pointxyz 256K", *source_256k, *target, indices_256k, params);
  benchDiagnosticStdFull("ceop upstream-like std fake-indices pointxyz 64K", source_64k, target, nullptr, params);
  benchDiagnosticFull("ceop upstream-like candidate fake-indices pointxyz 64K", source_64k, target, nullptr, params);
  benchAcceptedDiagnosticFull("ceop upstream-like accepted fake-indices pointxyz 64K", source_64k, target, nullptr, params);
  benchDiagnosticStdFull("ceop upstream-like std explicit-indices pointxyz 64K", source_64k, target, &indices_64k, params);
  benchDiagnosticFull("ceop upstream-like candidate explicit-indices pointxyz 64K", source_64k, target, &indices_64k, params);
  benchAcceptedDiagnosticFull("ceop upstream-like accepted explicit-indices pointxyz 64K", source_64k, target, &indices_64k, params);
  benchProductionFull("ceop production fake-indices pointxyz 64K", source_64k, target, nullptr, params);
  benchProductionFull("ceop production explicit-indices pointxyz 64K", source_64k, target, &indices_64k, params);
  benchAcceptedDiagnosticFull("ceop upstream-like accepted non-identity fake-indices pointxyz 64K",
                              non_identity_source_64k,
                              non_identity_target,
                              nullptr,
                              non_identity_params);
  benchAcceptedDiagnosticFull("ceop upstream-like accepted non-identity explicit-indices pointxyz 64K",
                              non_identity_source_64k,
                              non_identity_target,
                              &non_identity_indices_64k,
                              non_identity_params);
  benchProductionFull("ceop production non-identity fake-indices pointxyz 64K",
                      non_identity_source_64k,
                      non_identity_target,
                      nullptr,
                      non_identity_params);
  benchProductionFull("ceop production non-identity explicit-indices pointxyz 64K",
                      non_identity_source_64k,
                      non_identity_target,
                      &non_identity_indices_64k,
                      non_identity_params);

  printBanner('=');
  return 0;
}
