#pragma once

/*
 * 本文件保存 OrganizedMultiPlaneSegmentation 的 test-only component helpers
 * （测试专用组件 helper）。这些函数只复刻 production 中三个局部循环：
 * plane_d 点积、boundary cloud gather（边界点云离散复制）和 viewpoint projection
 * （视点投影）。它们用于 component ablation（组件消融），不能证明 production
 * dispatch（生产分流）已经接入。
 */

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

#if defined(__GNUC__)
#define PCL_RVV_OMPS_NOINLINE __attribute__((noinline))
#else
#define PCL_RVV_OMPS_NOINLINE
#endif

namespace pcl_rvv_segmentation_omps {

struct ComponentSummary {
  double checksum = 0.0;
  std::size_t count = 0;
};

struct RegionBoundaryInput {
  std::vector<int> boundary_indices;
  Eigen::Vector4f model = Eigen::Vector4f::Zero();
  Eigen::Vector3f centroid = Eigen::Vector3f::Zero();
  unsigned int inlier_count = 0;
};

inline ComponentSummary
summarizeFloats(const std::vector<float>& values)
{
  ComponentSummary summary;
  summary.count = values.size();
  for (std::size_t i = 0; i < values.size(); ++i)
    summary.checksum += static_cast<double>(values[i]) *
                        static_cast<double>((i % 17) + 1);
  return summary;
}

inline ComponentSummary
summarizeCloud(const pcl::PointCloud<pcl::PointXYZ>& cloud)
{
  ComponentSummary summary;
  summary.count = cloud.size();
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const double lane = static_cast<double>((i % 19) + 1);
    summary.checksum += lane * static_cast<double>(cloud[i].x) +
                        (lane + 1.0) * static_cast<double>(cloud[i].y) +
                        (lane + 2.0) * static_cast<double>(cloud[i].z);
  }
  return summary;
}

inline ComponentSummary
computePlaneDValuesStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const pcl::PointCloud<pcl::Normal>& normals,
                       std::vector<float>* plane_d)
{
  if (plane_d)
    plane_d->resize(cloud.size());
  std::vector<float> local;
  auto& out = plane_d ? *plane_d : local;
  out.resize(cloud.size());

  for (std::size_t i = 0; i < cloud.size(); ++i)
    out[i] = cloud[i].x * normals[i].normal_x +
             cloud[i].y * normals[i].normal_y +
             cloud[i].z * normals[i].normal_z;

  return summarizeFloats(out);
}

inline ComponentSummary
gatherBoundaryCloudStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const std::vector<int>& boundary_indices,
                       pcl::PointCloud<pcl::PointXYZ>* boundary_cloud)
{
  pcl::PointCloud<pcl::PointXYZ> local;
  auto& out = boundary_cloud ? *boundary_cloud : local;
  out.clear();
  out.reserve(boundary_indices.size());
  for (const int index : boundary_indices)
    out.push_back(cloud[static_cast<std::size_t>(index)]);

  return summarizeCloud(out);
}

inline ComponentSummary
projectBoundaryFromViewpointStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                const Eigen::Vector4f& normal,
                                const Eigen::Vector3f& centroid,
                                const Eigen::Vector3f& viewpoint,
                                pcl::PointCloud<pcl::PointXYZ>* projected_cloud)
{
  if (projected_cloud)
    projected_cloud->resize(cloud.size());

  pcl::PointCloud<pcl::PointXYZ> local;
  auto& out = projected_cloud ? *projected_cloud : local;
  out.resize(cloud.size());

  const Eigen::Vector3f norm(normal[0], normal[1], normal[2]);
  const float numerator = norm.dot(centroid - viewpoint);
  for (std::size_t i = 0; i < cloud.size(); ++i) {
    const Eigen::Vector3f pt(cloud[i].x, cloud[i].y, cloud[i].z);
    const float u = numerator / norm.dot(pt - viewpoint);
    const Eigen::Vector3f intersection(viewpoint + u * (pt - viewpoint));
    out[i].x = intersection[0];
    out[i].y = intersection[1];
    out[i].z = intersection[2];
  }

  return summarizeCloud(out);
}

inline ComponentSummary
assembleRegionBoundariesStd(
    const pcl::PointCloud<pcl::PointXYZ>& cloud,
    const std::vector<RegionBoundaryInput>& regions,
    const bool project_points,
    std::vector<pcl::PointCloud<pcl::PointXYZ>>* output_boundaries)
{
  if (output_boundaries) {
    output_boundaries->clear();
    output_boundaries->reserve(regions.size());
  }

  ComponentSummary total;
  const Eigen::Vector3f viewpoint(0.0f, 0.0f, 0.0f);
  for (std::size_t i = 0; i < regions.size(); ++i) {
    pcl::PointCloud<pcl::PointXYZ> boundary_cloud;
    gatherBoundaryCloudStd(cloud, regions[i].boundary_indices, &boundary_cloud);
    if (project_points && !boundary_cloud.empty()) {
      pcl::PointCloud<pcl::PointXYZ> projected_cloud;
      projectBoundaryFromViewpointStd(
          boundary_cloud, regions[i].model, regions[i].centroid, viewpoint, &projected_cloud);
      boundary_cloud.swap(projected_cloud);
    }

    const auto summary = summarizeCloud(boundary_cloud);
    total.checksum += summary.checksum * static_cast<double>((i % 13) + 1) +
                      static_cast<double>(regions[i].inlier_count);
    total.count += summary.count;
    if (output_boundaries)
      output_boundaries->push_back(boundary_cloud);
  }
  return total;
}

#if defined(__RVV10__)
PCL_RVV_OMPS_NOINLINE inline ComponentSummary
computePlaneDValuesRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const pcl::PointCloud<pcl::Normal>& normals,
                       std::vector<float>* plane_d)
{
  if (plane_d)
    plane_d->resize(cloud.size());
  std::vector<float> local;
  auto& out = plane_d ? *plane_d : local;
  out.resize(cloud.size());

  const auto* points = cloud.points.data();
  const auto* normal_points = normals.points.data();
  constexpr ptrdiff_t point_stride = static_cast<ptrdiff_t>(sizeof(pcl::PointXYZ));
  constexpr ptrdiff_t normal_stride = static_cast<ptrdiff_t>(sizeof(pcl::Normal));

  for (std::size_t i = 0; i < cloud.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    const auto* point = points + i;
    const auto* normal = normal_points + i;

    const vfloat32m2_t px = __riscv_vlse32_v_f32m2(&point->x, point_stride, vl);
    const vfloat32m2_t py = __riscv_vlse32_v_f32m2(&point->y, point_stride, vl);
    const vfloat32m2_t pz = __riscv_vlse32_v_f32m2(&point->z, point_stride, vl);
    const vfloat32m2_t nx = __riscv_vlse32_v_f32m2(&normal->normal_x, normal_stride, vl);
    const vfloat32m2_t ny = __riscv_vlse32_v_f32m2(&normal->normal_y, normal_stride, vl);
    const vfloat32m2_t nz = __riscv_vlse32_v_f32m2(&normal->normal_z, normal_stride, vl);

    vfloat32m2_t dot = __riscv_vfmul_vv_f32m2(px, nx, vl);
    dot = __riscv_vfmacc_vv_f32m2(dot, py, ny, vl);
    dot = __riscv_vfmacc_vv_f32m2(dot, pz, nz, vl);
    __riscv_vse32_v_f32m2(out.data() + i, dot, vl);
    i += vl;
  }

  return summarizeFloats(out);
}

PCL_RVV_OMPS_NOINLINE inline ComponentSummary
gatherBoundaryCloudRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const std::vector<int>& boundary_indices,
                       pcl::PointCloud<pcl::PointXYZ>* boundary_cloud)
{
  if (cloud.empty() || boundary_indices.empty()) {
    if (boundary_cloud)
      boundary_cloud->clear();
    return ComponentSummary{};
  }

  pcl::PointCloud<pcl::PointXYZ> local;
  auto& out = boundary_cloud ? *boundary_cloud : local;
  out.points.resize(boundary_indices.size());
  out.width = static_cast<std::uint32_t>(boundary_indices.size());
  out.height = 1;
  out.is_dense = cloud.is_dense;

  const auto* input = cloud.points.data();
  auto* output = out.points.data();
  constexpr ptrdiff_t point_stride = static_cast<ptrdiff_t>(sizeof(pcl::PointXYZ));

  for (std::size_t i = 0; i < boundary_indices.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(boundary_indices.size() - i);
    const vuint32m2_t indices = __riscv_vle32_v_u32m2(
        reinterpret_cast<const std::uint32_t*>(boundary_indices.data() + i), vl);
    const vuint32m2_t offsets =
        __riscv_vmul_vx_u32m2(indices, static_cast<std::uint32_t>(sizeof(pcl::PointXYZ)), vl);

    const vfloat32m2_t x = __riscv_vluxei32_v_f32m2(&input->x, offsets, vl);
    const vfloat32m2_t y = __riscv_vluxei32_v_f32m2(&input->y, offsets, vl);
    const vfloat32m2_t z = __riscv_vluxei32_v_f32m2(&input->z, offsets, vl);

    auto* point = output + i;
    __riscv_vsse32_v_f32m2(&point->x, point_stride, x, vl);
    __riscv_vsse32_v_f32m2(&point->y, point_stride, y, vl);
    __riscv_vsse32_v_f32m2(&point->z, point_stride, z, vl);
    i += vl;
  }

  return summarizeCloud(out);
}

PCL_RVV_OMPS_NOINLINE inline ComponentSummary
projectBoundaryFromViewpointRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                const Eigen::Vector4f& normal,
                                const Eigen::Vector3f& centroid,
                                const Eigen::Vector3f& viewpoint,
                                pcl::PointCloud<pcl::PointXYZ>* projected_cloud)
{
  pcl::PointCloud<pcl::PointXYZ> local;
  auto& out = projected_cloud ? *projected_cloud : local;
  out.points.resize(cloud.size());
  out.width = cloud.width;
  out.height = cloud.height;
  out.is_dense = cloud.is_dense;

  const float nx = normal[0];
  const float ny = normal[1];
  const float nz = normal[2];
  const float vpx = viewpoint[0];
  const float vpy = viewpoint[1];
  const float vpz = viewpoint[2];
  const float numerator =
      nx * (centroid[0] - vpx) + ny * (centroid[1] - vpy) + nz * (centroid[2] - vpz);

  const auto* input = cloud.points.data();
  auto* output = out.points.data();
  constexpr ptrdiff_t point_stride = static_cast<ptrdiff_t>(sizeof(pcl::PointXYZ));

  for (std::size_t i = 0; i < cloud.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(cloud.size() - i);
    const auto* point = input + i;
    const vfloat32m2_t x = __riscv_vlse32_v_f32m2(&point->x, point_stride, vl);
    const vfloat32m2_t y = __riscv_vlse32_v_f32m2(&point->y, point_stride, vl);
    const vfloat32m2_t z = __riscv_vlse32_v_f32m2(&point->z, point_stride, vl);

    const vfloat32m2_t dx = __riscv_vfsub_vf_f32m2(x, vpx, vl);
    const vfloat32m2_t dy = __riscv_vfsub_vf_f32m2(y, vpy, vl);
    const vfloat32m2_t dz = __riscv_vfsub_vf_f32m2(z, vpz, vl);
    vfloat32m2_t denominator = __riscv_vfmul_vf_f32m2(dx, nx, vl);
    denominator = __riscv_vfmacc_vf_f32m2(denominator, ny, dy, vl);
    denominator = __riscv_vfmacc_vf_f32m2(denominator, nz, dz, vl);
    const vfloat32m2_t u =
        __riscv_vfdiv_vv_f32m2(__riscv_vfmv_v_f_f32m2(numerator, vl), denominator, vl);

    vfloat32m2_t projected_x = __riscv_vfmv_v_f_f32m2(vpx, vl);
    projected_x = __riscv_vfmacc_vv_f32m2(projected_x, u, dx, vl);
    vfloat32m2_t projected_y = __riscv_vfmv_v_f_f32m2(vpy, vl);
    projected_y = __riscv_vfmacc_vv_f32m2(projected_y, u, dy, vl);
    vfloat32m2_t projected_z = __riscv_vfmv_v_f_f32m2(vpz, vl);
    projected_z = __riscv_vfmacc_vv_f32m2(projected_z, u, dz, vl);

    auto* out_point = output + i;
    __riscv_vsse32_v_f32m2(&out_point->x, point_stride, projected_x, vl);
    __riscv_vsse32_v_f32m2(&out_point->y, point_stride, projected_y, vl);
    __riscv_vsse32_v_f32m2(&out_point->z, point_stride, projected_z, vl);
    i += vl;
  }

  return summarizeCloud(out);
}

PCL_RVV_OMPS_NOINLINE inline ComponentSummary
assembleRegionBoundariesRVV(
    const pcl::PointCloud<pcl::PointXYZ>& cloud,
    const std::vector<RegionBoundaryInput>& regions,
    const bool project_points,
    std::vector<pcl::PointCloud<pcl::PointXYZ>>* output_boundaries)
{
  if (output_boundaries) {
    output_boundaries->clear();
    output_boundaries->reserve(regions.size());
  }

  ComponentSummary total;
  const Eigen::Vector3f viewpoint(0.0f, 0.0f, 0.0f);
  for (std::size_t i = 0; i < regions.size(); ++i) {
    pcl::PointCloud<pcl::PointXYZ> boundary_cloud;
    gatherBoundaryCloudRVV(cloud, regions[i].boundary_indices, &boundary_cloud);
    if (project_points && !boundary_cloud.empty()) {
      pcl::PointCloud<pcl::PointXYZ> projected_cloud;
      projectBoundaryFromViewpointRVV(
          boundary_cloud, regions[i].model, regions[i].centroid, viewpoint, &projected_cloud);
      boundary_cloud.swap(projected_cloud);
    }

    const auto summary = summarizeCloud(boundary_cloud);
    total.checksum += summary.checksum * static_cast<double>((i % 13) + 1) +
                      static_cast<double>(regions[i].inlier_count);
    total.count += summary.count;
    if (output_boundaries)
      output_boundaries->push_back(boundary_cloud);
  }
  return total;
}
#endif

} // namespace pcl_rvv_segmentation_omps

#undef PCL_RVV_OMPS_NOINLINE
