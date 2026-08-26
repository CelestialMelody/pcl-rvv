#ifndef TEST_RVV_SEGMENTATION_IMPL_EPPD_REFERENCE_HPP_
#define TEST_RVV_SEGMENTATION_IMPL_EPPD_REFERENCE_HPP_

#include <Eigen/Core>

#include <pcl/point_types.h>

#include <vector>

namespace eppd {

enum class CandidatePath {
  ReferenceFallback,
  RvvDense,
  RvvIndexed
};

inline float
coordinateByAxis(const pcl::PointXYZ& point, int axis)
{
  switch (axis) {
  case 0:
    return point.x;
  case 1:
    return point.y;
  default:
    return point.z;
  }
}

inline bool
isXYPointInPolygonReference(const pcl::PointXYZ& point,
                            const std::vector<pcl::PointXYZ>& polygon)
{
  bool in_poly = false;
  double x1 = 0.0;
  double x2 = 0.0;
  double y1 = 0.0;
  double y2 = 0.0;

  const auto nr_poly_points = polygon.size();
  double xold = polygon[nr_poly_points - 1].x;
  double yold = polygon[nr_poly_points - 1].y;
  for (std::size_t i = 0; i < nr_poly_points; ++i) {
    const double xnew = polygon[i].x;
    const double ynew = polygon[i].y;
    if (xnew > xold) {
      x1 = xold;
      x2 = xnew;
      y1 = yold;
      y2 = ynew;
    }
    else {
      x1 = xnew;
      x2 = xold;
      y1 = ynew;
      y2 = yold;
    }

    if ((xnew < point.x) == (point.x <= xold) &&
        (point.y - y1) * (x2 - x1) < (y2 - y1) * (point.x - x1)) {
      in_poly = !in_poly;
    }
    xold = xnew;
    yold = ynew;
  }

  return in_poly;
}

// 本 helper 复刻 production 中投影完成后的逐点扫描段：高度判断使用原始点，
// polygon（多边形）判断使用投影点 x/y。本阶段 fixture 的投影平面为 z=0，
// 因此 projected point 与输入点共享 x/y。
template <typename PointContainer>
std::vector<int>
segmentPolygonalPrismReference(const PointContainer& points,
                               const std::vector<int>& indices,
                               const std::vector<std::vector<pcl::PointXYZ>>& polygons,
                               float height_min,
                               float height_max)
{
  std::vector<int> output;
  output.reserve(indices.size());
  pcl::PointXYZ pt_xy;
  pt_xy.z = 0.0f;

  for (const int source_index : indices) {
    const auto& point = points[static_cast<std::size_t>(source_index)];
    if (point.z < height_min || point.z > height_max) {
      continue;
    }

    pt_xy.x = point.x;
    pt_xy.y = point.y;

    bool in_poly = false;
    for (const auto& polygon : polygons) {
      in_poly ^= isXYPointInPolygonReference(pt_xy, polygon);
    }

    if (in_poly) {
      output.push_back(source_index);
    }
  }

  return output;
}

// 本 helper 对齐 production `segment` 中投影完成后的真实扫描语义：高度使用
// 平面系数计算 signed distance（有符号距离），polygon 判断使用 projected point
// 按运行期 `k1/k2` 选出的二维坐标。它仍是测试资产里的 reference path。
template <typename PointContainer>
std::vector<int>
segmentPolygonalPrismFullScanReference(
    const PointContainer& points,
    const std::vector<int>& indices,
    const std::vector<std::vector<pcl::PointXYZ>>& polygons,
    const std::vector<pcl::PointXYZ>& projected_points,
    const Eigen::Vector4f& model_coefficients,
    float height_min,
    float height_max,
    int k1,
    int k2)
{
  std::vector<int> output;
  output.reserve(indices.size());
  pcl::PointXYZ pt_xy;
  pt_xy.z = 0.0f;

  for (std::size_t i = 0; i < indices.size(); ++i) {
    const int source_index = indices[i];
    const auto& point = points[static_cast<std::size_t>(source_index)];
    const float distance = model_coefficients[0] * point.x +
                           model_coefficients[1] * point.y +
                           model_coefficients[2] * point.z +
                           model_coefficients[3];
    if (distance < height_min || distance > height_max) {
      continue;
    }

    const auto& projected = projected_points[i];
    pt_xy.x = coordinateByAxis(projected, k1);
    pt_xy.y = coordinateByAxis(projected, k2);

    bool in_poly = false;
    for (const auto& polygon : polygons) {
      in_poly ^= isXYPointInPolygonReference(pt_xy, polygon);
    }

    if (in_poly) {
      output.push_back(source_index);
    }
  }

  return output;
}

} // namespace eppd

#endif // TEST_RVV_SEGMENTATION_IMPL_EPPD_REFERENCE_HPP_
