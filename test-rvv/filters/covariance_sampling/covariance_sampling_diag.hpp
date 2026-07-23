#pragma once

#include <pcl/rvv_point_load.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <list>
#include <numeric>
#include <vector>

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

namespace pcl_rvv_filters_covariance_sampling {

using Vector6d = Eigen::Matrix<double, 6, 1>;
using Matrix6d = Eigen::Matrix<double, 6, 6>;

struct SegmentTimes {
  double init_ms{0.0};
  double covariance_build_ms{0.0};
  double eigen_ms{0.0};
  double v_build_ms{0.0};
  double list_sort_ms{0.0};
  double sampling_ms{0.0};
};

struct SamplingResult {
  std::uint64_t checksum{0};
  double condition_number{0.0};
  std::size_t sampled_count{0};
  SegmentTimes segments;
};

inline void
mixU64(std::uint64_t& checksum, std::uint64_t value)
{
  checksum ^= value;
  checksum *= 1099511628211ull;
}

inline void
mixDouble(std::uint64_t& checksum, double value)
{
  std::uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  mixU64(checksum, bits);
}

inline std::uint64_t
checksumVector3f(const std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>& values)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto& v : values) {
    mixDouble(checksum, static_cast<double>(v.x()));
    mixDouble(checksum, static_cast<double>(v.y()));
    mixDouble(checksum, static_cast<double>(v.z()));
  }
  return checksum;
}

inline std::uint64_t
checksumVector6d(const std::vector<Vector6d, Eigen::aligned_allocator<Vector6d>>& values)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto& v : values) {
    for (int i = 0; i < 6; ++i)
      mixDouble(checksum, v[i]);
  }
  return checksum;
}

inline std::uint64_t
checksumIndices(const pcl::Indices& indices)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const int id : indices)
    mixU64(checksum, static_cast<std::uint64_t>(static_cast<std::uint32_t>(id)));
  return checksum;
}

inline pcl::PointCloud<pcl::PointXYZ>
makePointCloud(std::size_t n)
{
  pcl::PointCloud<pcl::PointXYZ> cloud;
  cloud.width = static_cast<std::uint32_t>(n);
  cloud.height = 1;
  cloud.is_dense = true;
  cloud.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float t = static_cast<float>(i);
    cloud[i].x = std::sin(t * 0.013f) * 4.0f + static_cast<float>(i % 17) * 0.031f;
    cloud[i].y = std::cos(t * 0.017f) * 3.0f + static_cast<float>(i % 23) * 0.027f;
    cloud[i].z = std::sin(t * 0.007f) * std::cos(t * 0.011f) * 2.0f + static_cast<float>(i % 31) * 0.019f;
  }
  return cloud;
}

inline pcl::PointCloud<pcl::Normal>
makeNormalCloud(std::size_t n)
{
  pcl::PointCloud<pcl::Normal> normals;
  normals.width = static_cast<std::uint32_t>(n);
  normals.height = 1;
  normals.is_dense = true;
  normals.points.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float t = static_cast<float>(i);
    Eigen::Vector3f v(std::cos(t * 0.019f) + 0.25f,
                      std::sin(t * 0.023f) + 0.15f,
                      std::cos(t * 0.029f) * 0.7f + 0.2f);
    const float norm = std::max(v.norm(), 1e-6f);
    v /= norm;
    normals[i].normal_x = v.x();
    normals[i].normal_y = v.y();
    normals[i].normal_z = v.z();
    normals[i].curvature = static_cast<float>(i % 101) * 0.0001f;
  }
  return normals;
}

inline pcl::Indices
makeIndices(std::size_t n, bool shuffled)
{
  pcl::Indices indices(n);
  std::iota(indices.begin(), indices.end(), 0);
  if (shuffled) {
    for (std::size_t i = 0; i < n; ++i) {
      const std::size_t j = (i * 1103515245u + 12345u) % n;
      std::swap(indices[i], indices[j]);
    }
  }
  return indices;
}

inline void
computeScaledPointsStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const pcl::Indices& indices,
                       std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>& scaled_points,
                       Eigen::Vector3f& centroid,
                       double& average_norm)
{
  centroid = Eigen::Vector3f::Zero();
  for (const int id : indices)
    centroid += cloud[static_cast<std::size_t>(id)].getVector3fMap();
  centroid /= static_cast<float>(indices.size());

  scaled_points.resize(indices.size());
  average_norm = 0.0;
  for (std::size_t i = 0; i < indices.size(); ++i) {
    scaled_points[i] = cloud[static_cast<std::size_t>(indices[i])].getVector3fMap() - centroid;
    average_norm += scaled_points[i].norm();
  }
  average_norm /= static_cast<double>(scaled_points.size());
  const float inv = 1.0f / static_cast<float>(average_norm);
  for (auto& point : scaled_points)
    point *= inv;
}

inline bool
computeScaledPointsRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const pcl::Indices& indices,
                       std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>& scaled_points,
                       Eigen::Vector3f& centroid,
                       double& average_norm)
{
#if defined(__RVV10__) && defined(PCL_COVARIANCE_SAMPLING_RVV_DIAGNOSTIC)
  if (indices.size() < 32)
    return false;

  const auto* base_u8 = reinterpret_cast<const std::uint8_t*>(cloud.points.data());
  const auto* raw_indices = reinterpret_cast<const std::uint32_t*>(indices.data());
  vfloat32m2_t acc_x = __riscv_vfmv_v_f_f32m2(0.0f, __riscv_vsetvlmax_e32m2());
  vfloat32m2_t acc_y = __riscv_vfmv_v_f_f32m2(0.0f, __riscv_vsetvlmax_e32m2());
  vfloat32m2_t acc_z = __riscv_vfmv_v_f_f32m2(0.0f, __riscv_vsetvlmax_e32m2());

  std::size_t offset = 0;
  while (offset < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_ids, vl);
    vfloat32m2_t vx, vy, vz;
    pcl::rvv_load::indexed_load3_f32m2<pcl::PointXYZ,
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(base_u8, v_off, vl, vx, vy, vz);
    acc_x = __riscv_vfadd_vv_f32m2_tu(acc_x, acc_x, vx, vl);
    acc_y = __riscv_vfadd_vv_f32m2_tu(acc_y, acc_y, vy, vl);
    acc_z = __riscv_vfadd_vv_f32m2_tu(acc_z, acc_z, vz, vl);
    offset += vl;
  }

  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  alignas(64) float sx[64];
  alignas(64) float sy[64];
  alignas(64) float sz[64];
  __riscv_vse32_v_f32m2(sx, acc_x, vlmax);
  __riscv_vse32_v_f32m2(sy, acc_y, vlmax);
  __riscv_vse32_v_f32m2(sz, acc_z, vlmax);
  float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
  for (std::size_t i = 0; i < vlmax; ++i) {
    sum_x += sx[i];
    sum_y += sy[i];
    sum_z += sz[i];
  }
  centroid = Eigen::Vector3f(sum_x, sum_y, sum_z) / static_cast<float>(indices.size());

  scaled_points.resize(indices.size());
  double norm_sum = 0.0;
  offset = 0;
  while (offset < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<pcl::PointXYZ>(v_ids, vl);
    vfloat32m2_t vx, vy, vz;
    pcl::rvv_load::indexed_load3_f32m2<pcl::PointXYZ,
                                       offsetof(pcl::PointXYZ, x),
                                       offsetof(pcl::PointXYZ, y),
                                       offsetof(pcl::PointXYZ, z)>(base_u8, v_off, vl, vx, vy, vz);
    vx = __riscv_vfsub_vf_f32m2(vx, centroid.x(), vl);
    vy = __riscv_vfsub_vf_f32m2(vy, centroid.y(), vl);
    vz = __riscv_vfsub_vf_f32m2(vz, centroid.z(), vl);
    const vfloat32m2_t sq = __riscv_vfmacc_vv_f32m2(
        __riscv_vfmacc_vv_f32m2(__riscv_vfmul_vv_f32m2(vx, vx, vl), vy, vy, vl), vz, vz, vl);
    alignas(64) float tx[64], ty[64], tz[64], tn[64];
    __riscv_vse32_v_f32m2(tx, vx, vl);
    __riscv_vse32_v_f32m2(ty, vy, vl);
    __riscv_vse32_v_f32m2(tz, vz, vl);
    __riscv_vse32_v_f32m2(tn, sq, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      scaled_points[offset + lane] = Eigen::Vector3f(tx[lane], ty[lane], tz[lane]);
      norm_sum += std::sqrt(static_cast<double>(tn[lane]));
    }
    offset += vl;
  }
  average_norm = norm_sum / static_cast<double>(scaled_points.size());
  const float inv = 1.0f / static_cast<float>(average_norm);
  for (auto& point : scaled_points)
    point *= inv;
  return true;
#else
  (void)cloud;
  (void)indices;
  (void)scaled_points;
  (void)centroid;
  (void)average_norm;
  return false;
#endif
}

inline void
buildVectorsStd(const std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>& scaled_points,
                const pcl::PointCloud<pcl::Normal>& normals,
                const pcl::Indices& indices,
                std::vector<Vector6d, Eigen::aligned_allocator<Vector6d>>& vectors)
{
  vectors.resize(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const auto& normal = normals[static_cast<std::size_t>(indices[i])];
    const Eigen::Vector3f n = normal.getNormalVector3fMap();
    vectors[i].block<3, 1>(0, 0) = scaled_points[i].cross(n).template cast<double>();
    vectors[i].block<3, 1>(3, 0) = n.template cast<double>();
  }
}

inline bool
buildVectorsRVV(const std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>& scaled_points,
                const pcl::PointCloud<pcl::Normal>& normals,
                const pcl::Indices& indices,
                std::vector<Vector6d, Eigen::aligned_allocator<Vector6d>>& vectors)
{
#if defined(__RVV10__) && defined(PCL_COVARIANCE_SAMPLING_RVV_DIAGNOSTIC)
  if (indices.size() < 32)
    return false;
  vectors.resize(indices.size());
  const auto* normal_base = reinterpret_cast<const std::uint8_t*>(normals.points.data());
  const auto* raw_indices = reinterpret_cast<const std::uint32_t*>(indices.data());
  std::size_t offset = 0;
  while (offset < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - offset);
    const vuint32m2_t v_ids = __riscv_vle32_v_u32m2(raw_indices + offset, vl);
    const vuint32m2_t v_off = pcl::rvv_load::byte_offsets_u32m2<pcl::Normal>(v_ids, vl);
    const vfloat32m2_t nx = pcl::rvv_load::gather_load_f32m2<pcl::Normal, offsetof(pcl::Normal, normal_x)>(normal_base, v_off, vl);
    const vfloat32m2_t ny = pcl::rvv_load::gather_load_f32m2<pcl::Normal, offsetof(pcl::Normal, normal_y)>(normal_base, v_off, vl);
    const vfloat32m2_t nz = pcl::rvv_load::gather_load_f32m2<pcl::Normal, offsetof(pcl::Normal, normal_z)>(normal_base, v_off, vl);

    alignas(64) float nnx[64], nny[64], nnz[64];
    __riscv_vse32_v_f32m2(nnx, nx, vl);
    __riscv_vse32_v_f32m2(nny, ny, vl);
    __riscv_vse32_v_f32m2(nnz, nz, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const std::size_t i = offset + lane;
      const Eigen::Vector3f n(nnx[lane], nny[lane], nnz[lane]);
      vectors[i].block<3, 1>(0, 0) = scaled_points[i].cross(n).template cast<double>();
      vectors[i].block<3, 1>(3, 0) = n.template cast<double>();
    }
    offset += vl;
  }
  return true;
#else
  (void)scaled_points;
  (void)normals;
  (void)indices;
  (void)vectors;
  return false;
#endif
}

inline Matrix6d
computeCovarianceFromVectors(const std::vector<Vector6d, Eigen::aligned_allocator<Vector6d>>& vectors)
{
  Matrix6d covariance = Matrix6d::Zero();
  for (const auto& v : vectors)
    covariance.noalias() += v * v.transpose();
  return covariance;
}

inline double
conditionNumber(const Matrix6d& covariance)
{
  const Eigen::SelfAdjointEigenSolver<Matrix6d> solver(covariance, Eigen::EigenvaluesOnly);
  const double max_ev = solver.eigenvalues().maxCoeff();
  const double min_ev = solver.eigenvalues().minCoeff();
  return max_ev / min_ev;
}

inline pcl::Indices
sampleFromVectors(const std::vector<Vector6d, Eigen::aligned_allocator<Vector6d>>& vectors,
                  const pcl::Indices& indices,
                  std::size_t num_samples,
                  const Matrix6d& eigenvectors)
{
  std::vector<std::size_t> candidate_indices(indices.size());
  std::iota(candidate_indices.begin(), candidate_indices.end(), 0);
  std::vector<std::list<std::pair<int, double>>> lists(6);
  for (std::size_t axis = 0; axis < 6; ++axis) {
    for (std::size_t i = 0; i < candidate_indices.size(); ++i)
      lists[axis].emplace_back(static_cast<int>(i), std::abs(vectors[i].dot(eigenvectors.block<6, 1>(0, axis))));
    lists[axis].sort([](const auto& a, const auto& b) { return a.second > b.second; });
  }

  std::vector<double> totals(6, 0.0);
  std::vector<bool> point_sampled(candidate_indices.size(), false);
  pcl::Indices sampled(num_samples);
  for (std::size_t sample_i = 0; sample_i < num_samples; ++sample_i) {
    std::size_t min_axis = 0;
    for (std::size_t axis = 1; axis < 6; ++axis) {
      if (totals[min_axis] > totals[axis])
        min_axis = axis;
    }
    while (point_sampled[static_cast<std::size_t>(lists[min_axis].front().first)])
      lists[min_axis].pop_front();
    const int local_id = lists[min_axis].front().first;
    point_sampled[static_cast<std::size_t>(local_id)] = true;
    lists[min_axis].pop_front();
    sampled[sample_i] = indices[static_cast<std::size_t>(local_id)];
    for (std::size_t axis = 0; axis < 6; ++axis) {
      const double value = vectors[static_cast<std::size_t>(local_id)].dot(eigenvectors.block<6, 1>(0, axis));
      totals[axis] += value * value;
    }
  }
  return sampled;
}

inline SamplingResult
runDiagnostic(const pcl::PointCloud<pcl::PointXYZ>& cloud,
              const pcl::PointCloud<pcl::Normal>& normals,
              const pcl::Indices& indices,
              std::size_t num_samples,
              bool use_rvv_fragments)
{
  SamplingResult result;
  std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>> scaled_points;
  Eigen::Vector3f centroid;
  double average_norm = 0.0;
  if (use_rvv_fragments && !computeScaledPointsRVV(cloud, indices, scaled_points, centroid, average_norm))
    computeScaledPointsStd(cloud, indices, scaled_points, centroid, average_norm);
  else if (!use_rvv_fragments)
    computeScaledPointsStd(cloud, indices, scaled_points, centroid, average_norm);

  std::vector<Vector6d, Eigen::aligned_allocator<Vector6d>> vectors;
  if (use_rvv_fragments && !buildVectorsRVV(scaled_points, normals, indices, vectors))
    buildVectorsStd(scaled_points, normals, indices, vectors);
  else if (!use_rvv_fragments)
    buildVectorsStd(scaled_points, normals, indices, vectors);

  const Matrix6d covariance = computeCovarianceFromVectors(vectors);
  const Eigen::SelfAdjointEigenSolver<Matrix6d> solver(covariance);
  const pcl::Indices sampled = sampleFromVectors(vectors, indices, num_samples, solver.eigenvectors());
  result.condition_number = conditionNumber(covariance);
  result.sampled_count = sampled.size();
  result.checksum = checksumVector3f(scaled_points);
  result.checksum ^= checksumVector6d(vectors);
  result.checksum ^= checksumIndices(sampled);
  mixDouble(result.checksum, result.condition_number);
  return result;
}

} // namespace pcl_rvv_filters_covariance_sampling
