/**
 * pcl_pipeline_dag.cpp — 单条点云 DAG：raw → 滤波 → common → 法线 → 变换 → Z 栅格 + 可分高斯 → 强度图 →
 *   pcl::2d（Convolution / Morphology / Edge，impl 中含 `__RVV10__`）→ SAC（impl 部分成员含 RVV）
 *
 * 与 pcl_pipeline_app（多支线并联）互补：强调数据依赖顺序，计时 total_pipeline_dag(one iter)。
 *
 * 单次迭代 `record(i)` 与 `pipeline_benchmark_stage_labels.hpp`、board-benchmark-report.zh.md §2.3「计时阶段」表一致。
 */

#include <pcl/common/centroid.h>
#include <pcl/common/common.h>
#include <pcl/common/distances.h>
#include <pcl/common/gaussian.h>
#include <pcl/common/norms.h>
#include <pcl/common/transforms.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <pcl/2d/convolution.h>
#include <pcl/2d/edge.h>
#include <pcl/2d/kernel.h>
#include <pcl/2d/morphology.h>

#include <pcl/search/kdtree.h>
#include <pcl/sample_consensus/sac_model_normal_plane.h>
#include <pcl/sample_consensus/sac_model_plane.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "pipeline_benchmark_stage_labels.hpp"
#include "pipeline_stage_timing.hpp"

#ifdef __RVV10__
#include <riscv_vector.h>
#endif

using CloudXYZPtr = pcl::PointCloud<pcl::PointXYZ>::Ptr;
using PointXYZI = pcl::PointXYZI;
using NormalT = pcl::Normal;

namespace {

constexpr int k2dW = 400;
constexpr int k2dH = 224;

void
printBanner (char ch, std::size_t width = 110)
{
  std::cout << std::string (width, ch) << '\n';
}

template <typename T>
void
doNotOptimize (const T& value)
{
#if defined(__GNUC__) || defined(__clang__)
  asm volatile ("" : : "r,m"(value) : "memory");
#else
  (void)value;
#endif
}

class Benchmarker
{
public:
  explicit Benchmarker (const std::string& name) : name_ (name) {}

  void
  run (const std::function<void ()>& func, int iterations, int warmup)
  {
    for (int i = 0; i < warmup; ++i)
      func ();
    const auto start = std::chrono::high_resolution_clock::now ();
    for (int i = 0; i < iterations; ++i)
      func ();
    const auto end = std::chrono::high_resolution_clock::now ();
    const double total_ms = std::chrono::duration<double, std::milli> (end - start).count ();
    const double avg_ms = total_ms / static_cast<double> (iterations);
    std::cout << std::left << std::setw (58) << name_ << ": " << std::fixed << std::setprecision (4) << avg_ms
              << " ms/iter\n";
  }

private:
  std::string name_;
};

void
fillVolumeCloud (CloudXYZPtr cloud, std::size_t n, std::uint32_t seed)
{
  cloud->resize (n);
  cloud->width = static_cast<std::uint32_t> (n);
  cloud->height = 1;
  cloud->is_dense = true;
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (0.f, 10.f);
  for (std::size_t i = 0; i < n; ++i)
  {
    (*cloud)[i].x = u (rng);
    (*cloud)[i].y = u (rng);
    (*cloud)[i].z = u (rng) * 0.18f + 0.001f * u (rng);
  }
}

#ifdef __RVV10__
/** 短时 RVV intrinsic 条带，见 board-benchmark-report.zh.md §2.3 工作流程「optional: … strip」与「可走 RVV」表。 */
static void
pipelineRvvCommonIntrinsicStrip (float* buf, unsigned n)
{
  if (n < 32u)
    return;
  unsigned cap = (n > 512u ? 512u : (n / 8u) * 8u);
  std::size_t idx = 0;
  while (idx < cap)
  {
    const std::size_t vl = __riscv_vsetvl_e32m2 (static_cast<std::size_t> (cap) - idx);
    vfloat32m2_t x = __riscv_vle32_v_f32m2 (buf + idx, vl);
    vfloat32m2_t y = __riscv_vfmul_vf_f32m2 (x, 0.2f, vl);
    vfloat32m2_t t1 = pcl::expf_RVV_f32m2 (__riscv_vfmul_vf_f32m2 (x, 0.31f, vl), vl);      // common.hpp；§2.3 optional strip
    vfloat32m2_t t2 = pcl::logf_RVV_f32m2 (__riscv_vfmax_vf_f32m2 (x, 1e-6f, vl), vl);    // common.hpp；同上
    vfloat32m2_t t3 =
        pcl::atan2_RVV_f32m2 (
            y, __riscv_vfadd_vf_f32m2 (__riscv_vfmul_vf_f32m2 (x, 0.06f, vl), 0.43f, vl), vl); // 同上
    vfloat32m2_t t4 =
        pcl::acos_RVV_f32m2 (__riscv_vfmin_vv_f32m2 (__riscv_vfabs_v_f32m2 (x, vl), __riscv_vfmv_v_f_f32m2 (0.99f, vl), vl), vl); // 同上
    vfloat32m2_t s =
        __riscv_vfadd_vv_f32m2 (__riscv_vfadd_vv_f32m2 (t1, t2, vl), __riscv_vfadd_vv_f32m2 (t3, t4, vl), vl);
    vfloat32m1_t vz = __riscv_vfmv_s_f_f32m1 (0.f, vl);
    buf[idx] =
        __riscv_vfmv_f_s_f32m1_f32 (__riscv_vfredusum_vs_f32m2_f32m1 (s, vz, vl));
    idx += vl;
  }
}
#endif

/** 由当前点云 XY 范围将 z 映射到 [0,255] 强度，写入 PointXYZI 栅格（与后续 2d 模块衔接）。 */
void
rasterizeZToIntensityGrid (const pcl::PointCloud<pcl::PointXYZ>& cloud,
                             int W,
                             int H,
                             pcl::PointCloud<PointXYZI>& grid)
{
  grid.width = static_cast<std::uint32_t> (W);
  grid.height = static_cast<std::uint32_t> (H);
  grid.resize (static_cast<std::size_t> (W) * static_cast<std::size_t> (H));
  for (auto& p : grid.points)
  {
    p.x = p.y = p.z = 0.f;
    p.intensity = 0.f;
  }
  if (cloud.empty ())
    return;

  pcl::PointXYZ mn, mx;
  pcl::getMinMax3D (cloud, mn, mx);                                                         // §2.3「不走 RVV」表：栅格内需包围盒（本 TU rasterize）；工作流程 2d 前置
  const float rx = mx.x - mn.x + 1e-6f;
  const float ry = mx.y - mn.y + 1e-6f;
  const float rz = mx.z - mn.z + 1e-6f;

  for (const auto& q : cloud.points)
  {
    int ix =
        static_cast<int> (
            std::clamp ((q.x - mn.x) / rx * static_cast<float> (W - 1), 0.f, static_cast<float> (W - 1)));
    int iy =
        static_cast<int> (
            std::clamp ((q.y - mn.y) / ry * static_cast<float> (H - 1), 0.f, static_cast<float> (H - 1)));
    const std::size_t k = static_cast<std::size_t> (iy) * static_cast<std::size_t> (W) + static_cast<std::size_t> (ix);
    const float t = (q.z - mn.z) / rz * 255.f;
    grid.points[k].intensity = std::max (grid.points[k].intensity, t);
  }
}

/** 由 filtered 包围盒推导 XY 平面上的 N 顶点闭合多边形，供 calculatePolygonArea（与 cloud 同源）。 */
void
makePolygonFromFilteredExtent (const pcl::PointCloud<pcl::PointXYZ>& filtered, int nVert, pcl::PointCloud<pcl::PointXYZ>& poly)
{
  pcl::PointXYZ mn {}, mx {};
  pcl::getMinMax3D (filtered, mn, mx);                                                        // §2.3 工作流程 polygon 辅助依赖（文档「不走」表 makePolygon 辅助同源）
  const float cx = 0.5f * (mn.x + mx.x);
  const float cy = 0.5f * (mn.y + mx.y);
  const float rx = 0.45f * (mx.x - mn.x + 1e-6f);
  const float ry = 0.45f * (mx.y - mn.y + 1e-6f);
  const float r = std::max (0.05f, std::min (rx, ry));
  poly.resize (static_cast<std::size_t> (nVert));
  poly.width = static_cast<std::uint32_t> (nVert);
  poly.height = 1;
  const float two_pi = 6.2831853f;
  for (int i = 0; i < nVert; ++i)
  {
    const float t = two_pi * static_cast<float> (i) / static_cast<float> (nVert);
    poly[static_cast<std::size_t> (i)].x = cx + r * std::cos (t);
    poly[static_cast<std::size_t> (i)].y = cy + r * std::sin (t);
    poly[static_cast<std::size_t> (i)].z = 0.5f * (mn.z + mx.z);
  }
}

struct DagCtx
{
  CloudXYZPtr cloud_raw {};

  float voxel_xy { 0.22f };
  float voxel_z { 0.09f };
  float normal_radius { 0.55f };
  float gauss_sigma { 3.5f };

  std::vector<float> math_scratch {};

  pcl::GaussianKernel gaussK {};
  Eigen::VectorXf gaussSep {};

  pcl::Convolution<PointXYZI>* conv { nullptr };
  pcl::PointCloud<PointXYZI>::Ptr conv_kernel {};
  pcl::Morphology<PointXYZI>* morph { nullptr };
  pcl::Edge<PointXYZI, pcl::PointXYZIEdge>* edge { nullptr };
  pcl::PointCloud<pcl::PointXYZIEdge>::Ptr edge_out {};
};

/**
 * \brief 一次计时迭代：`dag_compare` / `pipeline_dag_*` 的测量体。
 *
 * 用途：强调单链数据依赖：上一阶段输出作为下一阶段输入；Outer 计时比较 std 与 `__RVV10__` 编成下
 * 整条链耗时。与 `pipelineOnce`「多段在同一次迭代里并联覆盖」互为补充。
 *
 * 阶段（`-v` 首迭，`[pipeline_dag]`，与 `tr("...")` 字符串一致）：
 * 1. filters：体素滤波得到 `filtered` / `cloud_f`（点过少则回拷 raw）。
 * 2. common(cloud_f)：重心、协方差、minmax、`getMaxDistance`、`getMaxSegment`、按点填 `fa/fb` 后 32× L1/L2 norms、
 *    由滤波范围构造多边并 `calculatePolygonArea`。
 * 3. optional：`math_scratch` 非空且定义 `__RVV10__` 时 `pipelineRvvCommonIntrinsicStrip`。
 * 4. features：`cloud_f` 上半径法线 + KdTree。
 * 5. transform：刚体得到 `cloud_t`；Eigen 环将法线乘 `R` 并归一得到 `normals_t`。
 * 6. 2d (Z)：`rasterizeZToIntensityGrid` + 归一化为 float 栅格 `zimg`，再 `GaussianKernel` 行/列卷积。
 * 7. 2d (I)：再次栅格到 `img2d`，`Convolution`/`Morphology`/`Edge` Sobel。
 * 8. sample_consensus：在 `cloud_t` 与 `normals_t` 上构造 Plane / NormalPlane 并调用若干 distance API。
 *
 * \param verbose_trace 首次迭代向 stderr 打印阶段标签。
 * \param stages 若非空则累加各阶段耗时（warmup 传 nullptr）。`record(i)` 与 `kBenchStageDAG01`…`kBenchStageDAG08` 一致。
 */
void
pipelineDagOnce (DagCtx& c, bool verbose_trace, pcl_test_rvv_app::StageAccumulator* stages)
{
  static int s_dag_iter = 0;
  ++s_dag_iter;
  const bool trace = verbose_trace && (s_dag_iter == 1);
  auto tr = [trace] (const char* msg) {
    if (trace)
      std::cerr << "[pipeline_dag] " << msg << std::endl;
  };
  auto rec = [stages] (std::size_t idx, double ms) {
    if (stages)
      stages->record (idx, ms);
  };

  tr ("[DAG01] VoxelGrid::filter (-> filtered / cloud_f); copyPointCloud fallback if sparse");
  pcl::PointCloud<pcl::PointXYZ> filtered;
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    {
      pcl::VoxelGrid<pcl::PointXYZ> vg;
      vg.setInputCloud (c.cloud_raw);
      vg.setLeafSize (c.voxel_xy, c.voxel_xy, c.voxel_z);
      vg.filter (filtered);                                                                      // §2.3 工作流程 filters；§2.3「不走 RVV」表 VoxelGrid
    }
    if (filtered.size () < 4u)
      pcl::copyPointCloud (*c.cloud_raw, filtered);                                            // §2.3「不走 RVV」表：点过少回退
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (0, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  CloudXYZPtr cloud_f (pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> (filtered));          // DAG 下一阶段 cloud_f（PCL Ptr）
  if (trace)
    std::cerr << "[pipeline_dag]   cloud_f points: " << cloud_f->size () << std::endl;

  tr ("[DAG02] on cloud_f — compute3DCentroid, computeMeanAndCovarianceMatrix, getMinMax3D, getMaxDistance, getMaxSegment, "
      "L2_Norm+L1_Norm×32, makePolygonFromFilteredExtent + calculatePolygonArea");

  pcl::PointXYZ mn {}, mx {};
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    Eigen::Matrix3f cov = Eigen::Matrix3f::Zero ();
    Eigen::Vector4f cent = Eigen::Vector4f::Zero ();
    pcl::compute3DCentroid (*cloud_f, cent);                                                    // §2.3「可走 RVV」表；工作流程 common(cloud_f)
    (void)pcl::computeMeanAndCovarianceMatrix (*cloud_f, cov, cent);                             // §2.3「可走 RVV」表；同上
    doNotOptimize (cov.trace ());

    pcl::getMinMax3D (*cloud_f, mn, mx);                                                      // §2.3「可走 RVV」表；common(cloud_f)
    Eigen::Vector4f fmax {};
    pcl::getMaxDistance (*cloud_f, cent, fmax);                                              // §2.3「可走 RVV」表

    pcl::PointXYZ seg_a {}, seg_b {};
    (void)pcl::getMaxSegment (*cloud_f, seg_a, seg_b);                                       // §2.3「可走 RVV」表

    const int dfeat = static_cast<int> (std::min<std::size_t> (128u, cloud_f->size ()));
    std::vector<float> fa (static_cast<std::size_t> (dfeat)), fb (static_cast<std::size_t> (dfeat));
    for (int i = 0; i < dfeat; ++i)
    {
      fa[static_cast<std::size_t> (i)] = (*cloud_f)[static_cast<std::size_t> (i)].x;
      fb[static_cast<std::size_t> (i)] = (*cloud_f)[static_cast<std::size_t> (i)].y;
    }
    float nacc = 0.f;
    for (int r = 0; r < 32; ++r)
    {
      nacc += pcl::L2_Norm (fa.data (), fb.data (), dfeat);                                  // §2.3「可走 RVV」表 norms(32×)
      nacc += pcl::L1_Norm (fa.data (), fb.data (), dfeat);                                  // §2.3「可走 RVV」表 同上
    }
    doNotOptimize (nacc + mx.x + fmax[0]);

    pcl::PointCloud<pcl::PointXYZ> poly_xy;
    makePolygonFromFilteredExtent (filtered, 32, poly_xy);
    const float parea = pcl::calculatePolygonArea (poly_xy);                                    // §2.3「可走 RVV」表；工作流程 polygon（poly 由本 TU 构造）
    doNotOptimize (parea);
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (1, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
#ifdef __RVV10__
    if (!c.math_scratch.empty ())
    {
      tr ("[DAG03] expf_RVV_f32m2, logf_RVV_f32m2, atan2_RVV_f32m2, acos_RVV_f32m2 (scratch; __RVV10__ only)");
      pipelineRvvCommonIntrinsicStrip (c.math_scratch.data (),
                                   static_cast<unsigned> (c.math_scratch.size ()));          // §2.3「可走 RVV」optional strip
    }
#endif
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (2, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  tr ("[DAG04] NormalEstimation::compute + KdTree radius (on cloud_f)");
  pcl::PointCloud<NormalT> normals_f;
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    {
      pcl::NormalEstimation<pcl::PointXYZ, NormalT> ne;                                          // §2.3 工作流程 features；§2.3「不走 RVV」表
      pcl::search::KdTree<pcl::PointXYZ>::Ptr kdt (new pcl::search::KdTree<pcl::PointXYZ>);       // §2.3「不走 RVV」KdTree
      kdt->setInputCloud (cloud_f);
      ne.setInputCloud (cloud_f);
      ne.setSearchMethod (kdt);
      ne.setRadiusSearch (c.normal_radius);
      ne.compute (normals_f);                                                                   // §2.3 工作流程 features（法线）
    }
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (3, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  tr ("[DAG05] transformPointCloud (cloud_f->cloud_t); per-point normals_f *= R and normalize -> normals_t");
  pcl::PointCloud<pcl::PointXYZ> cloud_t;
  pcl::PointCloud<NormalT> normals_t;
  Eigen::Affine3f T;
  Eigen::Matrix3f R;
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    T = Eigen::Affine3f::Identity ();
    T.linear () = Eigen::AngleAxisf (0.06981f, Eigen::Vector3f::UnitZ ()).matrix ();
    T.translation () << -0.05f, 0.07f, 0.03f;
    pcl::transformPointCloud (*cloud_f, cloud_t, T);                                            // §2.3 工作流程 transform；§2.3「不走 RVV」表
    R = T.linear ();

    normals_t = normals_f;
    for (std::size_t i = 0; i < normals_t.size (); ++i)
    {
      Eigen::Vector3f n (normals_f[static_cast<std::size_t> (i)].normal_x,
                         normals_f[static_cast<std::size_t> (i)].normal_y,
                         normals_f[static_cast<std::size_t> (i)].normal_z);
      n = R * n;
      if (n.squaredNorm () > 1e-20f)
        n.normalize ();
      normals_t[static_cast<std::size_t> (i)].normal_x = n.x ();
      normals_t[static_cast<std::size_t> (i)].normal_y = n.y ();
      normals_t[static_cast<std::size_t> (i)].normal_z = n.z ();
    }
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (4, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  tr ("[DAG06] rasterizeZToIntensityGrid(cloud_t)->zimg normalize; GaussianKernel::convolveRows/convolveCols");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    pcl::PointCloud<float> zimg;
    zimg.width = static_cast<std::uint32_t> (k2dW);
    zimg.height = static_cast<std::uint32_t> (k2dH);
    zimg.resize (zimg.width * zimg.height);
    {
      pcl::PointCloud<PointXYZI> gtmp;
      rasterizeZToIntensityGrid (cloud_t, k2dW, k2dH, gtmp);                                    // §2.3 工作流程「2d(Z)」前半；文档「不走」表 bench 栅格
      for (std::size_t i = 0; i < zimg.size (); ++i)
        zimg[i] = gtmp[i].intensity / 255.f;
    }
    pcl::PointCloud<float> ztmp, zout;
    c.gaussK.convolveRows (zimg, c.gaussSep, ztmp);                                            // §2.3「可走 RVV」GaussianKernel；工作流程 2d(Z) 高斯
    c.gaussK.convolveCols (ztmp, c.gaussSep, zout);                                            // §2.3「可走 RVV」同上
    doNotOptimize (zout.empty () ? 0.f : zout.points.front ());
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (5, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  tr ("[DAG07] rasterizeZToIntensityGrid(cloud_t)->img2d; copyPointCloud; Convolution::filter, Morphology::erosionGray, "
      "Edge::detectEdgeSobel");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    pcl::PointCloud<PointXYZI> img2d;
    rasterizeZToIntensityGrid (cloud_t, k2dW, k2dH, img2d);                                    // §2.3 工作流程「2d(I)」栅格；文档「不走」表第二路栅格

    pcl::PointCloud<PointXYZI>::Ptr work = pcl::make_shared<pcl::PointCloud<PointXYZI>> ();
    pcl::copyPointCloud (img2d, *work);                                                        // §2.3「不走 RVV」表：准备 2d 输入
    c.conv->setInputCloud (work);
    pcl::PointCloud<PointXYZI> conv_out;
    c.conv->filter (conv_out);                                                                 // §2.3「可走 RVV」Convolution
    c.morph->setInputCloud (pcl::make_shared<pcl::PointCloud<PointXYZI>> (conv_out));
    pcl::PointCloud<PointXYZI> morp_out;
    c.morph->erosionGray (morp_out);                                                          // §2.3「可走 RVV」Morphology
    c.edge->setInputCloud (pcl::make_shared<pcl::PointCloud<PointXYZI>> (morp_out));
    c.edge->detectEdgeSobel (*c.edge_out);                                                    // §2.3「可走 RVV」Edge
    doNotOptimize (c.edge_out->empty () ? 0.f : c.edge_out->points.front ().magnitude);
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (6, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  tr ("[DAG08] construct ModelPlane/ModelNormalPlane per iter on (cloud_t, normals_t); countWithinDistance, "
      "selectWithinDistance, getDistancesToModel");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    CloudXYZPtr cloud_t_ptr = pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> (cloud_t);        // §2.3 SAC 输入云
    pcl::PointCloud<NormalT>::Ptr n_t_ptr = pcl::make_shared<pcl::PointCloud<NormalT>> (normals_t);

    pcl::SampleConsensusModelPlane<pcl::PointXYZ> model_plane (cloud_t_ptr);                    // §2.3「可走 RVV」SAC distance API
    pcl::SampleConsensusModelNormalPlane<pcl::PointXYZ, NormalT> model_nplane (cloud_t_ptr);  // §2.3「可走 RVV」同上
    model_nplane.setInputNormals (n_t_ptr);                                                          // §2.3 SAC：法线模型输入

    const Eigen::Vector3f center_w (0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z));
    Eigen::Vector4f c_est = T * Eigen::Vector4f (center_w.x (), center_w.y (), center_w.z (), 1.f);
    Eigen::Vector3f n_est = R * Eigen::Vector3f (0.f, 0.f, 1.f);
    n_est.normalize ();
    Eigen::VectorXf coeff (4);
    coeff[0] = n_est.x ();
    coeff[1] = n_est.y ();
    coeff[2] = n_est.z ();
    coeff[3] = -n_est.dot (c_est.head<3> ());

    const double thr = 0.12;
    pcl::Indices inliers;
    (void)model_plane.countWithinDistance (coeff, thr);                                        // §2.3 工作流程 sample_consensus
    model_nplane.selectWithinDistance (coeff, thr, inliers);                                  // §2.3「可走 RVV」表
    (void)model_nplane.countWithinDistance (coeff, thr);                                      // §2.3「可走 RVV」表
    std::vector<double> dists (normals_t.size ());
    model_nplane.getDistancesToModel (coeff, dists);                                          // §2.3「可走 RVV」表
    doNotOptimize (dists.empty () ? 0.0 : dists[0]);
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (7, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  tr ("end: iteration complete (DAG chain)");
}

/** 解析 `-v`/`--verbose`；剩余位置：`ITERS WARMUP [INPUT_PCD|'-']` */
static bool
collectArgs (int argc, char** argv, int& iterations, int& warmup, const char** pcd_in, bool* verbose_out)
{
  std::vector<const char*> pos;
  *verbose_out = false;
  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp (argv[i], "-v") == 0 || std::strcmp (argv[i], "--verbose") == 0)
    {
      *verbose_out = true;
      continue;
    }
    pos.push_back (argv[i]);
  }
  iterations = (pos.size () > 0) ? std::max (1, std::atoi (pos[0])) : 10;
  warmup = (pos.size () > 1) ? std::max (0, std::atoi (pos[1])) : 3;
  *pcd_in = nullptr;
  if (pos.size () > 2 && std::strcmp (pos[2], "-") != 0)
    *pcd_in = pos[2];
  return true;
}

} // namespace

int
main (int argc, char** argv)
{
  int iterations = 10;
  int warmup = 3;
  const char* pcd_in = nullptr;
  bool verbose = false;

  collectArgs (argc, argv, iterations, warmup, &pcd_in, &verbose);

  DagCtx c {};
  c.cloud_raw.reset (new pcl::PointCloud<pcl::PointXYZ>);                                          // §2.3 raw 输入（热身外初始化）
  c.conv_kernel.reset (new pcl::PointCloud<PointXYZI>);
  c.edge_out.reset (new pcl::PointCloud<pcl::PointXYZIEdge>);
  c.math_scratch.resize (512);
  {
    std::mt19937 rr (808u);
    std::uniform_real_distribution<float> u (0.22f, 0.93f);
    for (float& v : c.math_scratch)
      v = u (rr);
  }

  constexpr int kN = 50'000;
  if (pcd_in)
  {
    pcl::PCLPointCloud2 blob;
    if (pcl::io::loadPCDFile (pcd_in, blob) < 0)                                                      // CLI；不计 §2.3「测试工作流程」计时体
    {
      std::cerr << "[WARN] 无法读取 PCD，使用合成点云: " << pcd_in << "\n";
      fillVolumeCloud (c.cloud_raw, static_cast<std::size_t> (kN), 42u);
    }
    else
      pcl::fromPCLPointCloud2 (blob, *c.cloud_raw);                                                  // CLI；同上
  }
  else
    fillVolumeCloud (c.cloud_raw, static_cast<std::size_t> (kN), 42u);

  c.gaussK.compute (c.gauss_sigma, c.gaussSep);                                                      // 为 §2.3 工作流程「2d(Z)」Gaussian 准备可分核（计时在内层）

  c.conv = new pcl::Convolution<PointXYZI>;                                                        // §2.3 可走 RVV 2d（计时在 pipelineDagOnce）
  {
    pcl::kernel<PointXYZI> k;
    k.setKernelType (pcl::kernel<PointXYZI>::GAUSSIAN);
    k.setKernelSize (5);
    k.setKernelSigma (1.0f);
    k.fetchKernel (*c.conv_kernel);                                                                // pcl::kernel 热身；表中重点为 filter()
    c.conv->setKernel (*c.conv_kernel);
  }
  c.morph = new pcl::Morphology<PointXYZI>;                                                           // §2.3 可走 RVV 2d（计时在内层）
  {
    pcl::PointCloud<PointXYZI> elt;
    c.morph->structuringElementRectangle (elt, 3, 3);                                              // pcl::Morphology 构造结构元（热身）
    c.morph->setStructuringElement (pcl::make_shared<pcl::PointCloud<PointXYZI>> (elt));
  }
  c.edge = new pcl::Edge<PointXYZI, pcl::PointXYZIEdge>;                                            // §2.3 可走 RVV Edge（计时在内层）

  printBanner ('=');
  std::cout << " PCL DAG 单链：raw → voxel → common → normals → transform → gauss+2d → SAC\n";
  std::cout << "（与 pcl_pipeline_app 的多轨覆盖版并列；本程序强调数据依赖顺序。）\n";
  std::cout << " Args: [ -v | --verbose ] ITERS WARMUP [INPUT_PCD|'-']\n";
  std::cout << " Image Size: 400 x 224 (raster grids + 2d; 与 k2dW/k2dH 一致)\n";
  std::cout << " Dataset: pipeline_dag voxel+KdTree_radius_normals+raster_z_gauss_2d+SAC synthetic raw_pts="
            << c.cloud_raw->size () << '\n';
  std::cout << " raw cloud points: " << c.cloud_raw->size () << '\n';
  std::cout << " Iterations: " << iterations << '\n';
  std::cout << " Warmup: " << warmup << '\n';
  if (verbose)
    std::cout << " verbose: ON（首次 DAG 时在 stderr 打印阶段；不计入 warmup/计时正文）\n";
#if defined(__RVV10__)
  std::cout << " build: __RVV10__ ON\n";
#else
  std::cout << " build: __RVV10__ OFF\n";
#endif
  printBanner ('=');

  pcl_test_rvv_app::StageAccumulator stage_acc ({
      pcl_test_rvv_app::kBenchStageDAG01,
      pcl_test_rvv_app::kBenchStageDAG02,
      pcl_test_rvv_app::kBenchStageDAG03,
      pcl_test_rvv_app::kBenchStageDAG04,
      pcl_test_rvv_app::kBenchStageDAG05,
      pcl_test_rvv_app::kBenchStageDAG06,
      pcl_test_rvv_app::kBenchStageDAG07,
      pcl_test_rvv_app::kBenchStageDAG08,
  });
  pcl_test_rvv_app::runTimedBenchmarkWithStages (
      "total_pipeline_dag (one full chain)",
      [&c, verbose] (pcl_test_rvv_app::StageAccumulator* st) { pipelineDagOnce (c, verbose, st); },
      stage_acc,
      iterations,
      warmup,
      68);
  std::cout << "[pipeline_dag] 完成 " << iterations << " 次计时迭代（不含 warmup）。\n";

  delete c.edge;
  delete c.morph;
  delete c.conv;
  printBanner ('=');
  return 0;
}
