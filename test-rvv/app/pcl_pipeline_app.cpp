/**
 * pcl_pipeline_app.cpp — PCL RVV 综合管线（覆盖 common / 2d / sample_consensus；与各模块 RVV TU 对齐）
 *
 * 对比方式：exe 是否在编译时启用 -D__RVV10__；须与所用 libpcl_common 等对 `__RVV10__` 的编成一致，见 Makefile 注解。
 *
 * 另见同目录 pcl_pipeline_dag.cpp：单条点云 DAG（滤波 → common → 法线 → 变换 → 栅格与高斯 → 2d → SAC）。
 *
 * 说明：`doc-rvv/` 等为推导与笔记目录；本条所列「未 RVV」表示该 API 或 filters/features 等相关 TU
 * 在本仓库未维护 `__RVV10__` 分支（或与行旁占位注释一致），不得解读为断言 pcl::2d / sample_consensus 无 RVV。
 *
 * 【行旁 `[未 RVV]` / `[非 RVV]` 段落】：
 *   - 【非 RVV】pcl::getAngle3D（仅标量占位）
 *   - 【未 RVV】pcl::VoxelGrid
 *   - 【未 RVV】pcl::NormalEstimation + pcl::search::KdTree（半径邻域）
 *   - 【未 RVV】pcl::transformPointCloud
 *
 * 【强度图：pcl::Convolution / Morphology / Edge——pcl/2d/impl 中含 `__RVV10__`】
 *
 * 【SAC：平面 / NormalPlane——impl 中部分成员有 RVV；非全部 SAC API；仍为几何+RANSAC 语境】
 *
 * 【对比】pcl_pipeline_hot_rvv.cpp：摘掉体素/KdTree/变换/整块 2d/SAC/getAngle3D，仅保留 common + norms +
 *           可分高斯 + `#ifdef __RVV10__` common.hpp 数学短条，用以观察全链路被「非向量化大头」稀释的程度。
 *
 * 单次迭代分段计时、`record(i)`、`pipeline_benchmark_stage_labels.hpp`、board-benchmark-report.zh.md §2.1「计时阶段」表一致。
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
#include <cstdint>
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
using CloudXYZM = pcl::PointCloud<pcl::PointXYZ>;
using PointXYZI = pcl::PointXYZI;
using NormalT = pcl::Normal;

namespace {

constexpr std::size_t kBannerWidth = 118;

void
printBanner (char ch, std::size_t width = kBannerWidth)
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
    std::cout << std::left << std::setw (62) << name_ << ": " << std::fixed << std::setprecision (4)
              << avg_ms << " ms/iter\n";
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
    (*cloud)[i].z = u (rng) * 0.12f + 0.001f * u (rng);
  }
}

void
fillVolumeCloudStack (CloudXYZM& cloud, std::size_t n, std::uint32_t seed)
{
  cloud.resize (n);
  cloud.width = static_cast<std::uint32_t> (n);
  cloud.height = 1;
  cloud.is_dense = true;
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (0.f, 10.f);
  for (std::size_t i = 0; i < n; ++i)
  {
    cloud[i].x = u (rng);
    cloud[i].y = u (rng);
    cloud[i].z = u (rng) * 0.12f + 0.001f * u (rng);
  }
}

void
fillNearlyPlanarScene (pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_out,
                       pcl::PointCloud<NormalT>::Ptr normals_out,
                       std::size_t n,
                       std::uint32_t seed)
{
  cloud_out->resize (n);
  normals_out->resize (n);
  cloud_out->width = static_cast<std::uint32_t> (n);
  cloud_out->height = 1;
  cloud_out->is_dense = true;
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (-6.f, 6.f);
  std::uniform_real_distribution<float> zn (-0.02f, 0.02f);
  std::uniform_real_distribution<float> j (-0.035f, 0.035f);
  for (std::size_t i = 0; i < n; ++i)
  {
    (*cloud_out)[i].x = u (rng);
    (*cloud_out)[i].y = u (rng);
    (*cloud_out)[i].z = zn (rng);
    float nx = j (rng);
    float ny = j (rng);
    float nz = std::sqrt (std::max (0.008f, 1.f - nx * nx - ny * ny));
    (*normals_out)[i].normal_x = nx;
    (*normals_out)[i].normal_y = ny;
    (*normals_out)[i].normal_z = nz;
  }
}

void
fillRandomImageFloat (pcl::PointCloud<float>& img, std::uint32_t w, std::uint32_t h, std::uint32_t seed)
{
  img.width = w;
  img.height = h;
  img.resize (w * h);
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (0.f, 1.f);
  for (std::size_t i = 0; i < img.size (); ++i)
    img[i] = u (rng);
}

void
fillIntensityImage (pcl::PointCloud<PointXYZI>::Ptr img, int w, int h, std::uint32_t seed)
{
  img->width = static_cast<std::uint32_t> (w);
  img->height = static_cast<std::uint32_t> (h);
  img->resize (static_cast<std::size_t> (w * h));
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (0.f, 255.f);
  for (auto& p : img->points)
  {
    p.x = p.y = p.z = 0.f;
    p.intensity = u (rng);
  }
}

void
fillFeaturePair (std::vector<float>& a, std::vector<float>& b, std::uint32_t seed)
{
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (-1.f, 1.f);
  for (std::size_t i = 0; i < a.size (); ++i)
  {
    a[i] = u (rng);
    b[i] = u (rng);
  }
}

void
makeCirclePolygon (CloudXYZM& poly, int nVert, float r)
{
  poly.resize (static_cast<std::size_t> (nVert));
  poly.width = static_cast<std::uint32_t> (nVert);
  poly.height = 1;
  const float two_pi = 6.2831853f;
  for (int i = 0; i < nVert; ++i)
  {
    const float t = two_pi * static_cast<float> (i) / static_cast<float> (nVert);
    poly[static_cast<std::size_t> (i)].x = r * std::cos (t);
    poly[static_cast<std::size_t> (i)].y = r * std::sin (t);
    poly[static_cast<std::size_t> (i)].z = 0.f;
  }
}

#ifdef __RVV10__
/** 短时 RVV intrinsic 热身条带：遍历 `buf` 并调用 common.hpp 中 `pcl::*_RVV_f32m2`。对应 board-benchmark-report.zh.md
 * §2.1／§2.2／§2.3「可走 RVV」表与工作流程里的 optional／`strip(common.hpp intrinsics)` 段（仅 `#ifdef __RVV10__`）。 */
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
    vfloat32m2_t t1 = pcl::expf_RVV_f32m2 (__riscv_vfmul_vf_f32m2 (x, 0.31f, vl), vl);     // common.hpp；§2.1～2.3 optional strip 表
    vfloat32m2_t t2 = pcl::logf_RVV_f32m2 (__riscv_vfmax_vf_f32m2 (x, 1e-6f, vl), vl);   // common.hpp；同上
    vfloat32m2_t t3 =
        pcl::atan2_RVV_f32m2 (                                                                   // common.hpp；同上
            y, __riscv_vfadd_vf_f32m2 (__riscv_vfmul_vf_f32m2 (x, 0.06f, vl), 0.43f, vl), vl);
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

struct PipelineAssets
{
  CloudXYZPtr cloud_raw {}; // XYZ 管线主云
  pcl::PointCloud<PointXYZI>::Ptr img2d_master {};
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_sac {};
  pcl::PointCloud<NormalT>::Ptr normals_sac {};

  CloudXYZM cloud_segment {};
  pcl::PointCloud<float> gauss_gray {};
  pcl::GaussianKernel gaussKernel {};
  Eigen::VectorXf gaussKernelSep {};
  std::vector<float> feat_a, feat_b, math_scratch {};
  CloudXYZM polygon_xy {};

  int norm_dim { 128 };
  int norm_repeat { 400 };
  float gauss_sigma { 4.f };
  float voxel_leaf_xy { 0.21f };
  float voxel_leaf_z { 0.09f };
  float normal_radius { 0.55f };

  pcl::Convolution<PointXYZI>* conv { nullptr };
  pcl::PointCloud<PointXYZI>::Ptr conv_kernel {};

  pcl::Morphology<PointXYZI>* morph { nullptr };
  pcl::PointCloud<PointXYZI>::Ptr morph_struct {};

  pcl::Edge<PointXYZI, pcl::PointXYZIEdge>* edge { nullptr };
  pcl::PointCloud<pcl::PointXYZIEdge>::Ptr edge_out {};

  pcl::SampleConsensusModelPlane<pcl::PointXYZ>* model_plane { nullptr };
  pcl::SampleConsensusModelNormalPlane<pcl::PointXYZ, NormalT>* model_nplane { nullptr };
  Eigen::VectorXf plane_hessian {};
  double sac_thresh { 0.1 };
};

/**
 * \brief 一次计时迭代：`pipeline_compare`/`pipeline_app_*` 的测量体。
 *
 * 用途：在同一份合成（或载入）资产上串联多类 PCL/Eigen 工作，Outer 计时比较 `std` 与 `-D__RVV10__`
 * 编成下整链耗时；用于观察 RVV 在「多阶段并行覆盖」负载下的端到端收益。
 *
 * 数据与阶段（与 `-v` 首迭 stderr 前缀 `[pipeline]` 一致）：
 * 1. common(raw)：重心、协方差、包围盒、`getMaxDistance`、反复 L1/L2 范数、`getMaxSegment`、灰度图上可分高斯、
 *    XY 多边面积；若定义 `__RVV10__` 则再跑 `pipelineRvvCommonIntrinsicStrip`（common.hpp intrinsics）。
 * 2. 占位：`getAngle3D` 标量调用（本仓库未给该 API 做 RVV 批量路径）。
 * 3. filters：体素滤波（点过少则回拷 raw）。
 * 4. features：半径法线 + KdTree 邻域。
 * 5. transform：固定刚体变换滤波后点云。
 * 6. 2d：在预制 `img2d_master` 上卷积 / 形态学 / Sobel（与上一阶段点云链数据独立，但同进程计时）。
 * 7. sample_consensus：在专用 `cloud_sac` 上对平面 / 带法线平面模型调用若干 distance API（impl 内需宏时可有 RVV 分支）。
 * 8. 屏障读回副作用，便于不被优化删掉。
 *
 * \param verbose_trace 首次迭代向 stderr 打印阶段标签。
 * \param stages 若非空，则累加各阶段 wall-clock（与外层迭代次数一致；warmup 传入 nullptr）。
 *        `record(i)` 与同目录 `pipeline_benchmark_stage_labels.hpp` 内 `kBenchStagePIP01`…`kBenchStagePIP07` 顺序一致。
 */
void
pipelineOnce (PipelineAssets& a, bool verbose_trace, pcl_test_rvv_app::StageAccumulator* stages)
{
  static int s_pipe_invocation = 0;
  ++s_pipe_invocation;
  const bool trace = verbose_trace && (s_pipe_invocation == 1);
  auto tr = [trace] (const char* msg) {
    if (trace)
      std::cerr << "[pipeline] " << msg << std::endl;
  };
  auto rec = [stages] (std::size_t idx, double ms) {
    if (stages)
      stages->record (idx, ms);
  };

  tr ("[PIP01] on raw cloud — compute3DCentroid, computeMeanAndCovarianceMatrix, getMinMax3D, getMaxDistance, "
      "L2_Norm+L1_Norm×norm_repeat, getMaxSegment, GaussianKernel conv on gauss_gray, calculatePolygonArea; "
      "#ifdef __RVV10__: expf_RVV_f32m2, logf_RVV_f32m2, atan2_RVV_f32m2, acos_RVV_f32m2");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    Eigen::Matrix3f cov = Eigen::Matrix3f::Zero ();
    Eigen::Vector4f cent = Eigen::Vector4f::Zero ();
    pcl::compute3DCentroid (*a.cloud_raw, cent);                                        // §2.1「可走 RVV」表；工作流程 common(raw)
    (void)pcl::computeMeanAndCovarianceMatrix (*a.cloud_raw, cov, cent);                  // §2.1「可走 RVV」表；同上
    doNotOptimize (cov.trace ());

    pcl::PointXYZ mn {}, mx {};
    pcl::getMinMax3D (*a.cloud_raw, mn, mx);                                               // §2.1「可走 RVV」表；工作流程 common(raw)
    Eigen::Vector4f fmax {};
    pcl::getMaxDistance (*a.cloud_raw, cent, fmax);                                        // §2.1「可走 RVV」表；同上
    doNotOptimize (mx.x + fmax[0]);

    float acc = 0.f;
    for (int r = 0; r < a.norm_repeat; ++r)
    {
      acc += pcl::L2_Norm (a.feat_a.data (), a.feat_b.data (), a.norm_dim);               // §2.1「可走 RVV」表；工作流程 norms(repeat)
      acc += pcl::L1_Norm (a.feat_a.data (), a.feat_b.data (), a.norm_dim);               // §2.1「可走 RVV」表；同上
    }
    doNotOptimize (acc);

    pcl::PointXYZ gsmin {}, gsmax {};
    (void)pcl::getMaxSegment (a.cloud_segment, gsmin, gsmax);                             // §2.1「可走 RVV」表；maxSegment

    pcl::PointCloud<float> tmpf, outs;
    a.gaussKernel.convolveRows (a.gauss_gray, a.gaussKernelSep, tmpf);                     // §2.1「可走 RVV」表；工作流程 gauss_gray
    a.gaussKernel.convolveCols (tmpf, a.gaussKernelSep, outs);                             // §2.1「可走 RVV」表；同上
    doNotOptimize (outs.empty () ? 0.f : outs.points.front ());

    const float pa = pcl::calculatePolygonArea (a.polygon_xy);                             // §2.1「可走 RVV」表；工作流程 polygon
    doNotOptimize (pa);

#ifdef __RVV10__
    pipelineRvvCommonIntrinsicStrip (a.math_scratch.data (),
                                 static_cast<unsigned> (a.math_scratch.size ()));          // §2.1「可走 RVV」表；工作流程 optional RVV strip
#endif
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (0, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  // [非 RVV：getAngle3D 未在本仓库维护 RVV 批量路径；仅此标量占位]
  tr ("[PIP02] getAngle3D only (scalar; not RVV-tuned in this repo)");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    doNotOptimize (pcl::getAngle3D (Eigen::Vector3f (1.f, 0.f, 0.f), Eigen::Vector3f (0.f, 1.f, 0.f), false)); // §2.1 工作流程占位；§2.1「不走 RVV」表
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (1, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  // [未 RVV：VoxelGrid]
  tr ("[PIP03] VoxelGrid::filter (-> filtered); copyPointCloud if too few points");
  pcl::PointCloud<pcl::PointXYZ> filtered;
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    {
      pcl::VoxelGrid<pcl::PointXYZ> vg;
      vg.setInputCloud (a.cloud_raw);
      vg.setLeafSize (a.voxel_leaf_xy, a.voxel_leaf_xy, a.voxel_leaf_z);
      vg.filter (filtered);                                                                  // §2.1 工作流程 filters；§2.1「不走 RVV」表
    }
    if (filtered.size () < 8u)
      pcl::copyPointCloud (*a.cloud_raw, filtered);                                          // §2.1「不走 RVV」表（点过少回退）
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (2, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }
  if (trace)
    std::cerr << "[pipeline]   filtered points: " << filtered.size () << std::endl;

  // [未 RVV：NormalEstimation + KdTree]
  tr ("[PIP04] NormalEstimation::compute + KdTree radius search (normals on filtered)");
  pcl::PointCloud<NormalT> nf;
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    {
      pcl::NormalEstimation<pcl::PointXYZ, NormalT> ne;                                  // §2.1 工作流程 features；§2.1「不走 RVV」表
      pcl::search::KdTree<pcl::PointXYZ>::Ptr kdt (new pcl::search::KdTree<pcl::PointXYZ>); // §2.1「不走 RVV」表 KdTree 邻域检索
      pcl::PointCloud<pcl::PointXYZ>::Ptr fp (pcl::make_shared<pcl::PointCloud<pcl::PointXYZ>> (filtered));
      kdt->setInputCloud (fp);
      ne.setInputCloud (fp);
      ne.setSearchMethod (kdt);
      ne.setRadiusSearch (a.normal_radius);
      ne.compute (nf);                                                                     // §2.1 工作流程 features；§2.1「不走 RVV」表（含 KdTree）
    }
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (3, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  // [未 RVV：transformPointCloud]
  tr ("[PIP05] transformPointCloud (rigid T: filtered -> xformed)");
  pcl::PointCloud<pcl::PointXYZ> xformed;
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    {
      Eigen::Affine3f T = Eigen::Affine3f::Identity ();
      T.linear () = Eigen::AngleAxisf (0.06981f, Eigen::Vector3f::UnitZ ()).matrix ();
      T.translation () << -0.05f, 0.07f, 0.03f;
      pcl::transformPointCloud (filtered, xformed, T);                                     // §2.1 工作流程 transform；§2.1「不走 RVV」表
    }
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (4, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  // 2d：convolution / morphology / edge（pcl/2d/impl 含 __RVV10__）
  tr ("[PIP06] copyPointCloud(img2d_master) -> Convolution::filter, Morphology::erosionGray, Edge::detectEdgeSobel "
      "(independent of filtered cloud)");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    pcl::PointCloud<PointXYZI>::Ptr work = pcl::make_shared<pcl::PointCloud<PointXYZI>> ();
    pcl::copyPointCloud (*a.img2d_master, *work);                                          // §2.1 工作流程 2d 强度图链路；§2.1「不走 RVV」表：copy
    a.conv->setInputCloud (work);
    pcl::PointCloud<PointXYZI> conv_out;
    a.conv->filter (conv_out);                                                             // §2.1「可走 RVV」表 Convolution；工作流程 2d
    a.morph->setInputCloud (pcl::make_shared<pcl::PointCloud<PointXYZI>> (conv_out));
    pcl::PointCloud<PointXYZI> morp_out;
    a.morph->erosionGray (morp_out);                                                       // §2.1「可走 RVV」表 Morphology
    a.edge->setInputCloud (pcl::make_shared<pcl::PointCloud<PointXYZI>> (morp_out));
    a.edge->detectEdgeSobel (*a.edge_out);                                                // §2.1「可走 RVV」表 Edge
    doNotOptimize (a.edge_out->empty () ? 0.f : a.edge_out->points.front ().magnitude);
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (5, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  // sample_consensus：平面 / NormalPlane；impl 中若干 distance 接口在 __RVV10__ 下有分支
  tr ("[PIP07] on cloud_sac — SampleConsensusModelPlane / NormalPlane: countWithinDistance, selectWithinDistance, "
      "getDistancesToModel");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    pcl::Indices inliers;
    (void)a.model_plane->countWithinDistance (a.plane_hessian, a.sac_thresh);                // §2.1「可走 RVV」表 SAC（cloud_sac）；工作流程 sample_consensus
    a.model_nplane->selectWithinDistance (a.plane_hessian, a.sac_thresh, inliers);         // §2.1「可走 RVV」表；同上
    (void)a.model_nplane->countWithinDistance (a.plane_hessian, a.sac_thresh);               // §2.1「可走 RVV」表；同上
    std::vector<double> dbuf (a.normals_sac->size ());
    a.model_nplane->getDistancesToModel (a.plane_hessian, dbuf);                             // §2.1「可走 RVV」表；同上
    doNotOptimize (static_cast<double> (nf.size () + xformed.size ()));
    doNotOptimize (dbuf.empty () ? -1.0 : dbuf[0]);
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (6, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }
  tr ("end: iteration complete");
}

} // namespace

/** 解析 `-v` / `--verbose`；剩余位置参数顺序： ITERS WARMUP [PCD|-] [OUT|-] */
static bool
collectArgs (int argc, char** argv, int& iterations, int& warmup, const char** pcd_in, const char** pcd_out, bool* verbose_out)
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
  iterations = (pos.size () > 0) ? std::max (1, std::atoi (pos[0])) : 12;
  warmup = (pos.size () > 1) ? std::max (0, std::atoi (pos[1])) : 3;
  *pcd_in = nullptr;
  *pcd_out = nullptr;
  if (pos.size () > 2 && std::strcmp (pos[2], "-") != 0)
    *pcd_in = pos[2];
  if (pos.size () > 3 && std::strcmp (pos[3], "-") != 0)
    *pcd_out = pos[3];
  return true;
}

int
main (int argc, char** argv)
{
  int iterations = 12;
  int warmup = 3;
  const char* pcd_in = nullptr;
  const char* pcd_out = nullptr;
  bool verbose = false;

  collectArgs (argc, argv, iterations, warmup, &pcd_in, &pcd_out, &verbose);

  PipelineAssets a {};
  a.cloud_raw.reset (new pcl::PointCloud<pcl::PointXYZ>);                                 // §2「数据与负载」承载 raw/cloud（计时外初始化）
  a.img2d_master.reset (new pcl::PointCloud<PointXYZI>);                                  // §2.1 工作流程 2d 强度图源（计时外）
  a.cloud_sac.reset (new pcl::PointCloud<pcl::PointXYZ>);
  a.normals_sac.reset (new pcl::PointCloud<NormalT>);
  a.conv_kernel.reset (new pcl::PointCloud<PointXYZI>);
  a.morph_struct.reset (new pcl::PointCloud<PointXYZI>);
  a.edge_out.reset (new pcl::PointCloud<pcl::PointXYZIEdge>);

  constexpr int kN = 50'000;
  constexpr int kSac = 10'000;
  constexpr int kSeg = 512;
  constexpr int k2dw = 400;
  constexpr int k2dh = 224;

  if (pcd_in)
  {
    pcl::PCLPointCloud2 blob;
    if (pcl::io::loadPCDFile (pcd_in, blob) < 0)                                             // CLI；不在 §2.1 工作流程计时体内
    {
      std::cerr << "[WARN] 无法读取 PCD，使用合成点云: " << pcd_in << "\n";
      fillVolumeCloud (a.cloud_raw, static_cast<std::size_t> (kN), 42u);
    }
    else
      pcl::fromPCLPointCloud2 (blob, *a.cloud_raw);                                          // CLI；同上
  }
  else
    fillVolumeCloud (a.cloud_raw, static_cast<std::size_t> (kN), 42u);

  fillNearlyPlanarScene (a.cloud_sac, a.normals_sac, static_cast<std::size_t> (kSac), 202603u);
  fillVolumeCloudStack (a.cloud_segment, static_cast<std::size_t> (kSeg), 202602u);
  fillRandomImageFloat (a.gauss_gray, 480, 272, 77u);
  a.gaussKernel.compute (a.gauss_sigma, a.gaussKernelSep);                                 // pcl::GaussianKernel；为 §2.1 common(raw) gauss_gray 计时准备核（热身外）
  fillIntensityImage (a.img2d_master, k2dw, k2dh, 314u);

  a.feat_a.assign (static_cast<std::size_t> (a.norm_dim), 0.f);
  a.feat_b.assign (static_cast<std::size_t> (a.norm_dim), 0.f);
  fillFeaturePair (a.feat_a, a.feat_b, 901u);
  makeCirclePolygon (a.polygon_xy, 32, 5.5f);

  a.math_scratch.resize (512);
  {
    std::mt19937 rr (929u);
    std::uniform_real_distribution<float> dist (0.2f, 0.94f);
    for (float& v : a.math_scratch)
      v = dist (rr);
  }

  a.plane_hessian.resize (4);
  a.plane_hessian << 0.f, 0.f, 1.f, 0.f;

  a.model_plane = new pcl::SampleConsensusModelPlane<pcl::PointXYZ> (a.cloud_sac);          // SAC 专用云；耗时在 pipelineOnce §2.1 sample_consensus
  a.model_nplane = new pcl::SampleConsensusModelNormalPlane<pcl::PointXYZ, NormalT> (a.cloud_sac);
  a.model_nplane->setInputNormals (a.normals_sac);                                           // §2.1 计时体所用 SAC 平面场景法线（热身外绑定）

  a.conv = new pcl::Convolution<PointXYZI>;                                                  // §2.1 可走 RVV 2d；计时在 pipelineOnce
  {
    pcl::kernel<PointXYZI> k;                                                                // pcl::kernel「不走」计时重点在 filter()；此为初始化
    k.setKernelType (pcl::kernel<PointXYZI>::GAUSSIAN);
    k.setKernelSize (5);
    k.setKernelSigma (1.0f);
    k.fetchKernel (*a.conv_kernel);
    a.conv->setKernel (*a.conv_kernel);
  }

  a.morph = new pcl::Morphology<PointXYZI>;                                                   // §2.1 可走 RVV 2d（计时在内层）
  {
    pcl::PointCloud<PointXYZI> elt;
    a.morph->structuringElementRectangle (elt, 3, 3);
    a.morph->setStructuringElement (pcl::make_shared<pcl::PointCloud<PointXYZI>> (elt));
  }

  a.edge = new pcl::Edge<PointXYZI, pcl::PointXYZIEdge>;                                       // §2.1 可走 RVV 2d（计时在内层）

  printBanner ('=');
  std::cout << " PCL RVV 综合流水线（生成/滤波/法线/变换/2d/SAC/common）\n";
  std::cout << " Workload: common(centroid..polygon,gaussian) + filters + features(normals)"
               " + transform + 2d + SAC + RVV intrinsic strip (common.hpp)\n";
  std::cout << " Args: [ -v | --verbose ] ITERS WARMUP [INPUT_PCD|'-'] [OUT_PCD_BINARY|'-']\n";
  std::cout << " Image Size: " << k2dw << " x " << k2dh << " (img2d master; gaussian float image 480 x 272)\n";
  std::cout << " Dataset: pipeline_app synth raw_pts=" << a.cloud_raw->size () << " SAC_pts=" << a.cloud_sac->size ()
            << " norm_dim=" << a.norm_dim << " norm_repeat=" << a.norm_repeat << "\n";
  std::cout << " raw cloud: " << a.cloud_raw->size () << "  SAC planar: " << a.cloud_sac->size () << '\n';
  std::cout << " Iterations: " << iterations << '\n';
  std::cout << " Warmup: " << warmup << '\n';
  if (verbose)
    std::cout << " verbose: ON（首次进入管线时在 stderr 打印阶段；不影响计时循环外的说明）\n";
#if defined(__RVV10__)
  std::cout << " build: __RVV10__ ON\n";
#else
  std::cout << " build: __RVV10__ OFF\n";
#endif
  printBanner ('=');

  pcl_test_rvv_app::StageAccumulator stage_acc ({
      pcl_test_rvv_app::kBenchStagePIP01,
      pcl_test_rvv_app::kBenchStagePIP02,
      pcl_test_rvv_app::kBenchStagePIP03,
      pcl_test_rvv_app::kBenchStagePIP04,
      pcl_test_rvv_app::kBenchStagePIP05,
      pcl_test_rvv_app::kBenchStagePIP06,
      pcl_test_rvv_app::kBenchStagePIP07,
  });
  pcl_test_rvv_app::runTimedBenchmarkWithStages (
      "total_pipeline (full chain)",
      [&a, verbose] (pcl_test_rvv_app::StageAccumulator* st) { pipelineOnce (a, verbose, st); },
      stage_acc,
      iterations,
      warmup,
      68);
  std::cout << "[pipeline] 完成 " << iterations << " 次计时迭代（不含 warmup）。\n";

  delete a.edge;
  delete a.morph;
  delete a.conv;
  delete a.model_nplane;
  delete a.model_plane;

  if (pcd_out && !a.cloud_raw->empty ())
  {
    // [未 RVV：二进制 PCD I/O — 仅存 raw 合成/读入的云，不按管线写回滤波结果以免误解]
    pcl::io::savePCDFileBinary (pcd_out, *a.cloud_raw);                                      // CLI 写出；不计入 §2.1 工作流程计时
    std::cout << "[IO] 已保存 RAW 点云 -> " << pcd_out << "\n";
  }

  printBanner ('=');
  return 0;
}
