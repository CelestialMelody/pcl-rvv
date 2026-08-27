#pragma once

/*
 * 本文件保存 MinCutSegmentation potential loop 的 test-only reference（测试专用参考）
 * 和 RVV candidate（RVV 候选）声明。它复刻 production 中 unary / binary potential
 * 的局部公式，用于 component ablation（组件消融），不能证明 production dispatch
 * （生产分流）已经存在。
 */

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/auto.h>
#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <riscv_vector.h>
#endif

#include <boost/graph/adjacency_list.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace pcl_rvv_segmentation_min_cut {

struct UnarySummary {
  double sink_checksum = 0.0;
  double source_checksum = 0.0;
};

struct BinarySummary {
  double weight_checksum = 0.0;
};

struct BuildGraphSummary {
  std::size_t vertex_count = 0;
  std::size_t edge_count = 0;
  std::size_t unary_edge_count = 0;
  std::size_t binary_edge_count = 0;
  double capacity_checksum = 0.0;
};

namespace detail {

using DiagnosticGraph = boost::adjacency_list<
    boost::vecS,
    boost::vecS,
    boost::directedS,
    boost::property<
        boost::vertex_name_t,
        std::string,
        boost::property<
            boost::vertex_index_t,
            long,
            boost::property<
                boost::vertex_color_t,
                boost::default_color_type,
                boost::property<
                    boost::vertex_distance_t,
                    long,
                    boost::property<
                        boost::vertex_predecessor_t,
                        boost::adjacency_list_traits<boost::vecS, boost::vecS, boost::directedS>::
                            edge_descriptor>>>>>,
    boost::property<
        boost::edge_capacity_t,
        double,
        boost::property<
            boost::edge_residual_capacity_t,
            double,
            boost::property<
                boost::edge_reverse_t,
                boost::adjacency_list_traits<boost::vecS, boost::vecS, boost::directedS>::
                    edge_descriptor>>>>;

using DiagnosticTraits =
    boost::adjacency_list_traits<boost::vecS, boost::vecS, boost::directedS>;
using DiagnosticVertex = DiagnosticTraits::vertex_descriptor;
using DiagnosticEdge = DiagnosticTraits::edge_descriptor;
using DiagnosticCapacityMap =
    boost::property_map<DiagnosticGraph, boost::edge_capacity_t>::type;
using DiagnosticReverseEdgeMap =
    boost::property_map<DiagnosticGraph, boost::edge_reverse_t>::type;

struct EdgeRow {
  int source = 0;
  int target = 0;
};

inline bool
markEdge(std::vector<std::set<int>>& marker, const int source, const int target)
{
  auto& targets = marker[static_cast<std::size_t>(source)];
  if (targets.find(target) != targets.end())
    return false;
  targets.insert(target);
  return true;
}

inline bool
addDiagnosticEdge(DiagnosticGraph& graph,
                  const std::vector<DiagnosticVertex>& vertices,
                  DiagnosticCapacityMap& capacity,
                  DiagnosticReverseEdgeMap& reverse_edges,
                  const int source,
                  const int target,
                  const double weight)
{
  DiagnosticEdge edge;
  DiagnosticEdge reverse_edge;
  bool edge_was_added = false;
  bool reverse_edge_was_added = false;
  boost::tie(edge, edge_was_added) =
      boost::add_edge(vertices[static_cast<std::size_t>(source)],
                      vertices[static_cast<std::size_t>(target)],
                      graph);
  boost::tie(reverse_edge, reverse_edge_was_added) =
      boost::add_edge(vertices[static_cast<std::size_t>(target)],
                      vertices[static_cast<std::size_t>(source)],
                      graph);
  if (!edge_was_added || !reverse_edge_was_added)
    return false;

  capacity[edge] = weight;
  capacity[reverse_edge] = 0.0;
  reverse_edges[edge] = reverse_edge;
  reverse_edges[reverse_edge] = edge;
  return true;
}

inline void
collectBuildGraphEdges(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const std::vector<int>& indices,
                       const unsigned int number_of_neighbours,
                       std::vector<EdgeRow>& unary_source_edges,
                       std::vector<EdgeRow>& unary_sink_edges,
                       std::vector<EdgeRow>& binary_edges)
{
  const auto number_of_points = cloud.size();
  unary_source_edges.clear();
  unary_sink_edges.clear();
  binary_edges.clear();
  if (cloud.empty() || indices.empty())
    return;

  std::vector<std::set<int>> marker(number_of_points + 2);
  const int source = static_cast<int>(number_of_points);
  const int sink = static_cast<int>(number_of_points + 1);

  unary_source_edges.reserve(indices.size());
  unary_sink_edges.reserve(indices.size());
  for (const auto point_index : indices) {
    if (markEdge(marker, source, point_index))
      unary_source_edges.push_back({source, point_index});
    if (markEdge(marker, point_index, sink))
      unary_sink_edges.push_back({point_index, sink});
  }

  pcl::PointCloud<pcl::PointXYZ>::ConstPtr cloud_ptr(
      &cloud, [](const pcl::PointCloud<pcl::PointXYZ>*) {});
  auto indices_ptr = std::make_shared<const pcl::Indices>(indices.cbegin(), indices.cend());
  std::unique_ptr<pcl::search::Search<pcl::PointXYZ>> search(
      pcl::search::autoSelectMethod<pcl::PointXYZ>(
          cloud_ptr, indices_ptr, true, pcl::search::Purpose::many_knn_search));

  pcl::Indices neighbours;
  std::vector<float> distances;
  neighbours.reserve(number_of_neighbours);
  distances.reserve(number_of_neighbours);
  binary_edges.reserve(indices.size() * static_cast<std::size_t>(number_of_neighbours));
  for (std::size_t i_point = 0; i_point < indices.size(); ++i_point) {
    const int point_index = indices[i_point];
    search->nearestKSearch(static_cast<int>(i_point),
                           static_cast<int>(number_of_neighbours),
                           neighbours,
                           distances);
    for (std::size_t i_nghbr = 1; i_nghbr < neighbours.size(); ++i_nghbr) {
      const int neighbour = neighbours[i_nghbr];
      if (markEdge(marker, point_index, neighbour))
        binary_edges.push_back({point_index, neighbour});
      if (markEdge(marker, neighbour, point_index))
        binary_edges.push_back({neighbour, point_index});
    }
    neighbours.clear();
    distances.clear();
  }
}

template <typename UnaryFunc, typename BinaryFunc>
BuildGraphSummary
computeBuildGraphPotentialBatchImpl(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                    const std::vector<int>& indices,
                                    const std::vector<pcl::PointXYZ>& foreground,
                                    const double radius,
                                    const double source_weight,
                                    const double inverse_sigma,
                                    const unsigned int number_of_neighbours,
                                    UnaryFunc&& unary_func,
                                    BinaryFunc&& binary_func)
{
  BuildGraphSummary summary;
  if (cloud.empty() || foreground.empty() || indices.empty())
    return summary;

  std::vector<EdgeRow> unary_source_edges;
  std::vector<EdgeRow> unary_sink_edges;
  std::vector<EdgeRow> binary_edges;
  collectBuildGraphEdges(cloud,
                         indices,
                         number_of_neighbours,
                         unary_source_edges,
                         unary_sink_edges,
                         binary_edges);

  DiagnosticGraph graph;
  auto capacity = boost::get(boost::edge_capacity, graph);
  auto reverse_edges = boost::get(boost::edge_reverse, graph);
  std::vector<DiagnosticVertex> vertices(cloud.size() + 2);
  for (std::size_t i = 0; i < vertices.size(); ++i)
    vertices[i] = boost::add_vertex(graph);

  std::vector<double> sink_weights;
  unary_func(cloud, indices, foreground, radius, source_weight, &sink_weights);
  std::size_t checksum_row = 0;
  for (std::size_t row = 0; row < indices.size(); ++row) {
    addDiagnosticEdge(graph,
                      vertices,
                      capacity,
                      reverse_edges,
                      unary_source_edges[row].source,
                      unary_source_edges[row].target,
                      source_weight);
    summary.capacity_checksum +=
        source_weight * static_cast<double>((checksum_row++ % 29) + 1);
    addDiagnosticEdge(graph,
                      vertices,
                      capacity,
                      reverse_edges,
                      unary_sink_edges[row].source,
                      unary_sink_edges[row].target,
                      sink_weights[row]);
    summary.capacity_checksum +=
        sink_weights[row] * static_cast<double>((checksum_row++ % 29) + 1);
  }

  std::vector<int> sources;
  std::vector<int> targets;
  sources.reserve(binary_edges.size());
  targets.reserve(binary_edges.size());
  for (const auto& edge : binary_edges) {
    sources.push_back(edge.source);
    targets.push_back(edge.target);
  }
  std::vector<double> binary_weights;
  binary_func(cloud, sources, targets, inverse_sigma, &binary_weights);
  for (std::size_t row = 0; row < binary_edges.size(); ++row) {
    addDiagnosticEdge(graph,
                      vertices,
                      capacity,
                      reverse_edges,
                      binary_edges[row].source,
                      binary_edges[row].target,
                      binary_weights[row]);
    summary.capacity_checksum +=
        binary_weights[row] * static_cast<double>((checksum_row++ % 29) + 1);
  }

  summary.vertex_count = boost::num_vertices(graph);
  summary.edge_count = boost::num_edges(graph);
  summary.unary_edge_count = unary_source_edges.size() + unary_sink_edges.size();
  summary.binary_edge_count = binary_edges.size();
  return summary;
}

} // namespace detail

UnarySummary
computeUnaryPotentialsStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                          const std::vector<int>& indices,
                          const std::vector<pcl::PointXYZ>& foreground,
                          double radius,
                          double source_weight,
                          std::vector<double>* sink_weights)
{
  UnarySummary summary;
  if (sink_weights)
    sink_weights->resize(indices.size());
  for (std::size_t row = 0; row < indices.size(); ++row) {
    const auto& point = cloud[indices[row]];
    double min_dist = std::numeric_limits<double>::max();
    for (const auto& fg : foreground) {
      const double dx = static_cast<double>(fg.x) - static_cast<double>(point.x);
      const double dy = static_cast<double>(fg.y) - static_cast<double>(point.y);
      const double dist = dx * dx + dy * dy;
      min_dist = std::min(min_dist, dist);
    }
    const double sink = std::sqrt(min_dist / radius);
    if (sink_weights)
      (*sink_weights)[row] = sink;
    summary.sink_checksum += sink * static_cast<double>((row % 17) + 1);
    summary.source_checksum += source_weight;
  }
  return summary;
}

BinarySummary
computeBinaryPotentialsStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                           const std::vector<int>& sources,
                           const std::vector<int>& targets,
                           double inverse_sigma,
                           std::vector<double>* weights)
{
  BinarySummary summary;
  if (weights)
    weights->resize(sources.size());
  for (std::size_t row = 0; row < sources.size(); ++row) {
    const auto& source = cloud[sources[row]];
    const auto& target = cloud[targets[row]];
    const double dx = static_cast<double>(source.x) - static_cast<double>(target.x);
    const double dy = static_cast<double>(source.y) - static_cast<double>(target.y);
    const double dz = static_cast<double>(source.z) - static_cast<double>(target.z);
    const double distance = (dx * dx + dy * dy + dz * dz) * inverse_sigma;
    const double weight = std::exp(-distance);
    if (weights)
      (*weights)[row] = weight;
    summary.weight_checksum += weight * static_cast<double>((row % 23) + 1);
  }
  return summary;
}

BuildGraphSummary
computeBuildGraphPotentialBatchStd(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                   const std::vector<int>& indices,
                                   const std::vector<pcl::PointXYZ>& foreground,
                                   double radius,
                                   double source_weight,
                                   double inverse_sigma,
                                   unsigned int number_of_neighbours)
{
  return detail::computeBuildGraphPotentialBatchImpl(
      cloud,
      indices,
      foreground,
      radius,
      source_weight,
      inverse_sigma,
      number_of_neighbours,
      computeUnaryPotentialsStd,
      computeBinaryPotentialsStd);
}

#if defined(__RVV10__)
UnarySummary
computeUnaryPotentialsRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                          const std::vector<int>& indices,
                          const std::vector<pcl::PointXYZ>& foreground,
                          double radius,
                          double source_weight,
                          std::vector<double>* sink_weights)
{
  UnarySummary summary;
  if (sink_weights)
    sink_weights->resize(indices.size());

  const float inv_radius = static_cast<float>(1.0 / radius);
  std::vector<float> scratch;
  scratch.resize(__riscv_vsetvlmax_e32m2());

  std::size_t row = 0;
  while (row < indices.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(indices.size() - row);
    vfloat32m2_t min_dist =
        __riscv_vfmv_v_f_f32m2(std::numeric_limits<float>::max(), vl);

    for (const auto& fg : foreground) {
      for (std::size_t lane = 0; lane < vl; ++lane) {
        const auto& point = cloud[indices[row + lane]];
        scratch[lane] = point.x;
      }
      const vfloat32m2_t x = __riscv_vle32_v_f32m2(scratch.data(), vl);
      for (std::size_t lane = 0; lane < vl; ++lane) {
        const auto& point = cloud[indices[row + lane]];
        scratch[lane] = point.y;
      }
      const vfloat32m2_t y = __riscv_vle32_v_f32m2(scratch.data(), vl);
      const vfloat32m2_t dx = __riscv_vfsub_vf_f32m2(x, fg.x, vl);
      const vfloat32m2_t dy = __riscv_vfsub_vf_f32m2(y, fg.y, vl);
      vfloat32m2_t dist = __riscv_vfmul_vv_f32m2(dx, dx, vl);
      dist = __riscv_vfmacc_vv_f32m2(dist, dy, dy, vl);
      min_dist = __riscv_vfmin_vv_f32m2(min_dist, dist, vl);
    }

    vfloat32m2_t sink = __riscv_vfmul_vf_f32m2(min_dist, inv_radius, vl);
    sink = __riscv_vfsqrt_v_f32m2(sink, vl);
    __riscv_vse32_v_f32m2(scratch.data(), sink, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const double value = static_cast<double>(scratch[lane]);
      if (sink_weights)
        (*sink_weights)[row + lane] = value;
      summary.sink_checksum += value * static_cast<double>(((row + lane) % 17) + 1);
      summary.source_checksum += source_weight;
    }
    row += vl;
  }

  return summary;
}

BinarySummary
computeBinaryPotentialsRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                           const std::vector<int>& sources,
                           const std::vector<int>& targets,
                           double inverse_sigma,
                           std::vector<double>* weights)
{
  BinarySummary summary;
  if (weights)
    weights->resize(sources.size());

  const float sigma = static_cast<float>(inverse_sigma);
  std::vector<float> sx;
  std::vector<float> sy;
  std::vector<float> sz;
  std::vector<float> tx;
  std::vector<float> ty;
  std::vector<float> tz;
  const auto max_vl = __riscv_vsetvlmax_e32m2();
  sx.resize(max_vl);
  sy.resize(max_vl);
  sz.resize(max_vl);
  tx.resize(max_vl);
  ty.resize(max_vl);
  tz.resize(max_vl);

  std::size_t row = 0;
  while (row < sources.size()) {
    const std::size_t vl = __riscv_vsetvl_e32m2(sources.size() - row);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const auto& source = cloud[sources[row + lane]];
      const auto& target = cloud[targets[row + lane]];
      sx[lane] = source.x;
      sy[lane] = source.y;
      sz[lane] = source.z;
      tx[lane] = target.x;
      ty[lane] = target.y;
      tz[lane] = target.z;
    }
    const vfloat32m2_t vx0 = __riscv_vle32_v_f32m2(sx.data(), vl);
    const vfloat32m2_t vy0 = __riscv_vle32_v_f32m2(sy.data(), vl);
    const vfloat32m2_t vz0 = __riscv_vle32_v_f32m2(sz.data(), vl);
    const vfloat32m2_t vx1 = __riscv_vle32_v_f32m2(tx.data(), vl);
    const vfloat32m2_t vy1 = __riscv_vle32_v_f32m2(ty.data(), vl);
    const vfloat32m2_t vz1 = __riscv_vle32_v_f32m2(tz.data(), vl);
    const vfloat32m2_t dx = __riscv_vfsub_vv_f32m2(vx0, vx1, vl);
    const vfloat32m2_t dy = __riscv_vfsub_vv_f32m2(vy0, vy1, vl);
    const vfloat32m2_t dz = __riscv_vfsub_vv_f32m2(vz0, vz1, vl);
    vfloat32m2_t distance = __riscv_vfmul_vv_f32m2(dx, dx, vl);
    distance = __riscv_vfmacc_vv_f32m2(distance, dy, dy, vl);
    distance = __riscv_vfmacc_vv_f32m2(distance, dz, dz, vl);
    distance = __riscv_vfmul_vf_f32m2(distance, -sigma, vl);
    const vfloat32m2_t weight = pcl::expf_RVV_f32m2(distance, vl);
    __riscv_vse32_v_f32m2(sx.data(), weight, vl);
    for (std::size_t lane = 0; lane < vl; ++lane) {
      const double value = static_cast<double>(sx[lane]);
      if (weights)
        (*weights)[row + lane] = value;
      summary.weight_checksum += value * static_cast<double>(((row + lane) % 23) + 1);
    }
    row += vl;
  }

  return summary;
}

BuildGraphSummary
computeBuildGraphPotentialBatchRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                                   const std::vector<int>& indices,
                                   const std::vector<pcl::PointXYZ>& foreground,
                                   double radius,
                                   double source_weight,
                                   double inverse_sigma,
                                   unsigned int number_of_neighbours)
{
  return detail::computeBuildGraphPotentialBatchImpl(
      cloud,
      indices,
      foreground,
      radius,
      source_weight,
      inverse_sigma,
      number_of_neighbours,
      computeUnaryPotentialsRVV,
      computeBinaryPotentialsRVV);
}
#endif

} // namespace pcl_rvv_segmentation_min_cut
