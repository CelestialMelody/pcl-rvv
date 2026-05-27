/**
 * pcl_pipeline_hot_rvv.cpp — 从 pcl_pipeline_app 摘掉「KdTree / 滤波 / 变换 / 强度图整块 2d / SAC / getAngle3D」等大头，
 *   专测 common、norms、可分高斯、面积、`#ifdef __RVV10__` 数学短条（见 common/impl）。
 *
 * =============================================================================
 * 本基准保留（随 libpcl / `__RVV10__` 编成可走 RVV TU）：
 *   - pcl::compute3DCentroid、pcl::computeMeanAndCovarianceMatrix（common/impl/centroid.hpp）
 *   - pcl::getMinMax3D、pcl::getMaxDistance（common）
 *   - pcl::L2_Norm / pcl::L1_Norm（循环；common/impl/norms.hpp）
 *   - pcl::getMaxSegment（common/distances）
 *   - pcl::GaussianKernel convolveRows/Cols（common；dense float）
 *   - pcl::calculatePolygonArea（common/impl/common.hpp）
 *   - `#ifdef __RVV10__`：pipelineRvvCommonIntrinsicStrip（common/impl/common.hpp 内 expf/logf/atan2/acos 等 intrinsic）
 *
 * （本基准摘除）— 与 pcl_pipeline_app 行旁 `[未 RVV]` 一致，或未纳入本节：
 *   - 【非 RVV】pcl::getAngle3D 占位（见 app 源码注释）
 *   - 【未 RVV】pcl::VoxelGrid
 *   - 【未 RVV】pcl::NormalEstimation + pcl::search::KdTree
 *   - 【未 RVV】pcl::transformPointCloud
 *   - pcl::2d Convolution / Morphology / Edge 整段（实现位于 pcl/2d/impl 含 RVV；本热点 baseline 不测）
 *   - pcl::SampleConsensus*（若在 pipeline_app / DAG 中对比）
 *
 * 解读：若 full pipeline std/rvv 差距小而本 baseline 差距大，
 *           则更支持「全管线被邻域检索、滤波、RANSAC 分支等大头稀释」（Amdahl），非必然实现错误。
 * =============================================================================
 *
 * Args 与 pcl_pipeline_app 一致：`[ -v | --verbose ] ITERS WARMUP [INPUT_PCD|'-'] [OUT_PCD_BINARY|'-']`
 *
 * 单次迭代 `record(i)` 与 `pipeline_benchmark_stage_labels.hpp`、board-benchmark-report.zh.md §2.2「计时阶段」表一致。
 */

#include <pcl/common/centroid.h>
#include <pcl/common/common.h>
#include <pcl/common/distances.h>
#include <pcl/common/gaussian.h>
#include <pcl/common/norms.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <Eigen/Core>

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
/** 短时 RVV intrinsic 条带，见 board-benchmark-report.zh.md §2.2「可走 RVV」表 optional 与工作流程 common(hot) 段末尾。 */
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
    vfloat32m2_t t1 = pcl::expf_RVV_f32m2 (__riscv_vfmul_vf_f32m2 (x, 0.31f, vl), vl);     // common.hpp；§2.2 optional strip
    vfloat32m2_t t2 = pcl::logf_RVV_f32m2 (__riscv_vfmax_vf_f32m2 (x, 1e-6f, vl), vl);   // common.hpp；同上
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

/** 精简资产：无滤波/法线/2d/SAC/img2d。 */
struct HotAssets
{
  CloudXYZPtr cloud_raw {};
  pcl::PointCloud<float> gauss_gray {};
  pcl::GaussianKernel gaussKernel {};
  Eigen::VectorXf gaussKernelSep {};
  std::vector<float> feat_a, feat_b, math_scratch {};
  CloudXYZM cloud_segment {};
  CloudXYZM polygon_xy {};
  int norm_dim { 128 };
  int norm_repeat { 400 };
  float gauss_sigma { 4.f };
};

/**
 * \brief 一次计时迭代：`hot_compare` / `pipeline_hot_*` 的测量体。
 *
 * 用途：从 `pipelineOnce` 中去掉体素、法线+KdTree、刚体变换、整条强度图 2d、SAC、以及 `getAngle3D` 占位，
 * 仅在 common + norms + 可分高斯 + 面积 (+ 可选 intrinsic 短条) 上对比 std/rvv，用于估算全链路中非本段大头对倍率的稀释。
 *
 * 阶段（`-v` 首迭，`[pipeline_hot]`）：
 * 1. common(hot subset, raw)：与 `pipelineOnce` 开头 common 段落算子对齐（ centroid / cov / minmax /
 *    maxdist / norms 循环 / `getMaxSegment` / `gauss_gray` / 面积）。
 * 2. optional：若定义 `__RVV10__`，`pipelineRvvCommonIntrinsicStrip`。
 * 3. end：本迭代结束（无 voxel / normals / tf / 2d / SAC）。
 *
 * \param verbose_trace 首次迭代 stderr 打印阶段字符串。
 * \param stages 若非空则累加各阶段耗时（warmup 传 nullptr）。`record(i)` 与 `kBenchStageHOT01`/`HOT02` 一致。
 */
void
pipelineHotOnce (HotAssets& a, bool verbose_trace, pcl_test_rvv_app::StageAccumulator* stages)
{
  static int s_inv = 0;
  ++s_inv;
  const bool trace = verbose_trace && (s_inv == 1);
  auto tr = [trace] (const char* msg) {
    if (trace)
      std::cerr << "[pipeline_hot] " << msg << std::endl;
  };
  auto rec = [stages] (std::size_t idx, double ms) {
    if (stages)
      stages->record (idx, ms);
  };

  tr ("[HOT01] on raw — compute3DCentroid, computeMeanAndCovarianceMatrix, getMinMax3D, getMaxDistance, "
      "L2_Norm+L1_Norm×norm_repeat, getMaxSegment, GaussianKernel conv, calculatePolygonArea (no voxel/normals/tf/2d/SAC)");
  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
    Eigen::Matrix3f cov = Eigen::Matrix3f::Zero ();
    Eigen::Vector4f cent = Eigen::Vector4f::Zero ();
    pcl::compute3DCentroid (*a.cloud_raw, cent);                                             // §2.2「可走 RVV」表；工作流程 common(hot subset)
    (void)pcl::computeMeanAndCovarianceMatrix (*a.cloud_raw, cov, cent);                      // §2.2「可走 RVV」表；同上
    doNotOptimize (cov.trace ());

    pcl::PointXYZ mn {}, mx {};
    pcl::getMinMax3D (*a.cloud_raw, mn, mx);                                                  // §2.2「可走 RVV」表
    Eigen::Vector4f fmax {};
    pcl::getMaxDistance (*a.cloud_raw, cent, fmax);                                          // §2.2「可走 RVV」表
    doNotOptimize (mx.x + fmax[0]);

    float acc = 0.f;
    for (int r = 0; r < a.norm_repeat; ++r)
    {
      acc += pcl::L2_Norm (a.feat_a.data (), a.feat_b.data (), a.norm_dim);                   // §2.2「可走 RVV」表；norms 循环
      acc += pcl::L1_Norm (a.feat_a.data (), a.feat_b.data (), a.norm_dim);                   // §2.2「可走 RVV」表；同上
    }
    doNotOptimize (acc);

    pcl::PointXYZ gsmin {}, gsmax {};
    (void)pcl::getMaxSegment (a.cloud_segment, gsmin, gsmax);                                 // §2.2「可走 RVV」表

    pcl::PointCloud<float> tmpf, outs;
    a.gaussKernel.convolveRows (a.gauss_gray, a.gaussKernelSep, tmpf);                        // §2.2「可走 RVV」GaussianKernel
    a.gaussKernel.convolveCols (tmpf, a.gaussKernelSep, outs);                               // §2.2「可走 RVV」同上
    doNotOptimize (outs.empty () ? 0.f : outs.points.front ());

    const float pa = pcl::calculatePolygonArea (a.polygon_xy);                              // §2.2「可走 RVV」表；polygon
    doNotOptimize (pa);
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (0, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }

  {
    const auto t0 = std::chrono::high_resolution_clock::now ();
#ifdef __RVV10__
    tr ("[HOT02] expf_RVV_f32m2, logf_RVV_f32m2, atan2_RVV_f32m2, acos_RVV_f32m2 (intrinsic strip only if __RVV10__)");
    pipelineRvvCommonIntrinsicStrip (a.math_scratch.data (), static_cast<unsigned> (a.math_scratch.size ())); // §2.2「可走 RVV」optional strip
#endif
    const auto t1 = std::chrono::high_resolution_clock::now ();
    rec (1, pcl_test_rvv_app::chronoDurationMs (t0, t1));
  }
  tr ("end: iteration complete (hot; no voxel/normals/transform/2d/SAC/getAngle3D)");
}

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

} // namespace

int
main (int argc, char** argv)
{
  int iterations = 12;
  int warmup = 3;
  const char* pcd_in = nullptr;
  const char* pcd_out = nullptr;
  bool verbose = false;

  collectArgs (argc, argv, iterations, warmup, &pcd_in, &pcd_out, &verbose);

  HotAssets a {};
  a.cloud_raw.reset (new pcl::PointCloud<pcl::PointXYZ>);                                      // §2.2 raw 载荷（热身外）

  constexpr int kN = 50'000;
  constexpr int kSeg = 512;

  if (pcd_in)
  {
    pcl::PCLPointCloud2 blob;
    if (pcl::io::loadPCDFile (pcd_in, blob) < 0)                                                // CLI；不计入 §2.2「测试工作流程」计时体
    {
      std::cerr << "[WARN] 无法读取 PCD，使用合成点云: " << pcd_in << "\n";
      fillVolumeCloud (a.cloud_raw, static_cast<std::size_t> (kN), 42u);
    }
    else
      pcl::fromPCLPointCloud2 (blob, *a.cloud_raw);                                             // CLI；同上
  }
  else
    fillVolumeCloud (a.cloud_raw, static_cast<std::size_t> (kN), 42u);

  fillVolumeCloudStack (a.cloud_segment, static_cast<std::size_t> (kSeg), 202602u);
  fillRandomImageFloat (a.gauss_gray, 480, 272, 77u);
  a.gaussKernel.compute (a.gauss_sigma, a.gaussKernelSep);                                      // 为 §2.2 Gaussian 段准备可分核（计时在 pipelineHotOnce）

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

  printBanner ('=');
  std::cout << " PCL pipeline_hot_rvv — common/norms/gaussConv/polygon[+__RVV10__ strip]，已去掉 voxel/法线KdTree/tf/2d/SAC。\n";
  std::cout << " Args: [ -v | --verbose ] ITERS WARMUP [INPUT_PCD|'-'] [OUT_PCD_BINARY|'-']\n";
  std::cout << " Image Size: " << static_cast<int> (a.gauss_gray.width) << " x " << static_cast<int> (a.gauss_gray.height)
            << "\n";
  std::cout << " Dataset: pipeline_hot synth raw_pts=" << a.cloud_raw->size ()
            << " seg_pts=" << a.cloud_segment.size () << " norm_dim=" << a.norm_dim << " norm_repeat="
            << a.norm_repeat << " gaussian_float_image\n";
  std::cout << " raw cloud points: " << a.cloud_raw->size () << '\n';
  std::cout << " Iterations: " << iterations << '\n';
  std::cout << " Warmup: " << warmup << '\n';
  if (verbose)
    std::cout << " verbose: ON（首次迭代 stderr 简述阶段）。\n";
#if defined(__RVV10__)
  std::cout << " build: __RVV10__ ON\n";
#else
  std::cout << " build: __RVV10__ OFF\n";
#endif
  printBanner ('=');

  pcl_test_rvv_app::StageAccumulator stage_acc ({
      pcl_test_rvv_app::kBenchStageHOT01,
      pcl_test_rvv_app::kBenchStageHOT02,
  });
  pcl_test_rvv_app::runTimedBenchmarkWithStages (
      "total_pipeline_hot_rvv_only (common+norms+gauss+poly[+strip])",
      [&a, verbose] (pcl_test_rvv_app::StageAccumulator* st) { pipelineHotOnce (a, verbose, st); },
      stage_acc,
      iterations,
      warmup,
      68);
  std::cout << "[pipeline_hot] 完成 " << iterations << " 次计时迭代。\n";

  if (pcd_out && !a.cloud_raw->empty ())
  {
    pcl::io::savePCDFileBinary (pcd_out, *a.cloud_raw);                                         // CLI 写出；不计入 §2.2 计时体
    std::cout << "[IO] 已保存 RAW 点云 -> " << pcd_out << '\n';
  }

  printBanner ('=');
  return 0;
}
