/*
 * 同进程内：显式标量 \c *_Norm_Std 与连续 \c float* 上的 \c *_Norm（RVV 窄分发）对拍。
 * 须 \c -D__RVV10__ 与 RVV 工具链；链 \c libpcl_common（实现见源码 \c impl/norms.hpp）。
 *
 * 与 \c test_gaussian_convolve_compare.cpp 类似：单可执行、不依赖「装两套 PCL」做双进程对比。
 *
 * ---------------------------------------------------------------------------
 * 误差与容差说明（为何不能要求逐位相等）
 * ---------------------------------------------------------------------------
 *
 * 1) 比较对象
 *    - 「标量侧」：\c *_Norm_Std，按 C++ 源码从左到右的 float 累加顺序。
 *    - 「向量侧」：\c *_Norm(const float*, ...)，走 RVV 条带加载与向量规约（如 \c vfredosum 等），
 *      规约树顺序与标量不同。
 *
 * 2) \c tolOrdered(dim)（L1 / L2² / L∞ / L2 / JM / B / Sublinear / CS / HIK / PF / K 等）
 *    - 浮点加不满足结合律：同一组数按不同顺序相加，结果可差约 \c O(dim * ε) 量级（\c ε 为 float 机器精度）。
 *    - RVV 与标量路径在数学上等价，但结合顺序不同，故使用 \c EXPECT_NEAR 而非 \c EXPECT_FLOAT_EQ。
 *    - 公式：\c max(1e-5, 2e-6 * dim) 与 \c test_norms.cpp 中 \c tolRvvVsNaive 一致，随维数略放宽。
 *    - \c L2：本文件用 \c sqrt(L2_Norm_SQR_Std) 对标 \c L2_Norm；在 L2² 已有误差时，经 \c sqrt 放大/压缩，
 *      仍落在同一 \c tolOrdered 量级内。
 *
 * 3) \c tolLog(dim)（Div / KL）
 *    - 标量侧用 \c std::log；RVV 侧用 \c pcl::logf_RVV_f32m2（Remez 逼近，见 \c common.hpp），
 *      与 \c libm 在 ulp 上可有差异。
 *    - 再累加 \c (a-b)*log(ratio) 或 \c a*log(ratio)，误差随 \c dim 累积。
 *    - 公式：\c max(1.2e-4, 5e-7 * dim)；若板卡/输入导致偶发略超，可略调系数（以不掩盖真 bug 为前提）。
 *
 * 4) 数据范围
 *    - JM / B 等含 \c sqrt：非负随机向量，与 \c test_norms 一致。
 *    - Div / KL：严格正随机向量，避免分母为 0 与对数定义域问题；对拍的是该分布下两路径一致，而非全实数域。
 *
 * 5) 失败时
 *    - 先确认 \c impl/norms.hpp 未改坏；再区分是「可接受的浮点/逼近差」还是算法错误（例如整段系统性偏斜）。
 */
#if !defined(__RVV10__)
#error "norms Std vs RVV 对拍需 -D__RVV10__（与 RVV 工具链）"
#endif

#include <pcl/common/norms.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace {

inline float
tolOrdered (int dim)
{
  return std::max (1e-5f, 2e-6f * static_cast<float> (std::max (dim, 1)));
}

inline float
tolLog (int dim)
{
  return std::max (1.2e-4f, 5e-7f * static_cast<float> (std::max (dim, 1)));
}

void
fill (std::vector<float>& a, std::vector<float>& b, std::uint32_t seed)
{
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (-3.0f, 3.0f);
  for (std::size_t i = 0; i < a.size (); ++i)
  {
    a[i] = u (rng);
    b[i] = u (rng);
  }
}

void
fillNonNegative (std::vector<float>& a, std::vector<float>& b, std::uint32_t seed)
{
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (0.f, 3.5f);
  for (std::size_t i = 0; i < a.size (); ++i)
  {
    a[i] = u (rng);
    b[i] = u (rng);
  }
}

void
fillPositive (std::vector<float>& a, std::vector<float>& b, std::uint32_t seed)
{
  std::mt19937 rng (seed);
  std::uniform_real_distribution<float> u (0.2f, 2.5f);
  for (std::size_t i = 0; i < a.size (); ++i)
  {
    a[i] = u (rng);
    b[i] = u (rng);
  }
}

void
expectNear (float std_v, float rvv_v, float tol, const char* name, int dim)
{
  EXPECT_NEAR (std_v, rvv_v, tol) << name << " dim=" << dim << " std=" << std_v << " rvv=" << rvv_v;
}

} // namespace

TEST (NormsStdVsRvv, L1_L2sqr_Linf_L2_on_float_pointer)
{
  for (int dim : {0, 1, 8, 16, 17, 64, 200, 1024})
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fill (a, b, 501u + static_cast<std::uint32_t> (dim));
    float* pa = a.data ();
    float* pb = b.data ();
    const float te = tolOrdered (dim);

    expectNear (pcl::L1_Norm_Std (pa, pb, dim), pcl::L1_Norm (pa, pb, dim), te, "L1", dim);
    expectNear (pcl::L2_Norm_SQR_Std (pa, pb, dim), pcl::L2_Norm_SQR (pa, pb, dim), te, "L2_SQR", dim);
    expectNear (pcl::Linf_Norm_Std (pa, pb, dim), pcl::Linf_Norm (pa, pb, dim), te, "Linf", dim);
    const float l2_std =
        std::sqrt (pcl::L2_Norm_SQR_Std (pa, pb, dim));
    expectNear (l2_std, pcl::L2_Norm (pa, pb, dim), te, "L2", dim);
  }
}

TEST (NormsStdVsRvv, JM_B_Sublinear_CS_HIK_PF_K_nonnegative)
{
  constexpr float P1 = 0.65f;
  constexpr float P2 = 1.15f;
  for (int dim : {0, 1, 8, 16, 17, 256})
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fillNonNegative (a, b, 601u + static_cast<std::uint32_t> (dim));
    float* pa = a.data ();
    float* pb = b.data ();
    const float te = tolOrdered (dim);

    expectNear (pcl::JM_Norm_Std (pa, pb, dim), pcl::JM_Norm (pa, pb, dim), te, "JM", dim);
    expectNear (pcl::B_Norm_Std (pa, pb, dim), pcl::B_Norm (pa, pb, dim), te, "B", dim);
    expectNear (
        pcl::Sublinear_Norm_Std (pa, pb, dim), pcl::Sublinear_Norm (pa, pb, dim), te, "Sublinear", dim);
    expectNear (pcl::CS_Norm_Std (pa, pb, dim), pcl::CS_Norm (pa, pb, dim), te, "CS", dim);
    expectNear (pcl::HIK_Norm_Std (pa, pb, dim), pcl::HIK_Norm (pa, pb, dim), te, "HIK", dim);
    expectNear (
        pcl::PF_Norm_Std (pa, pb, dim, P1, P2), pcl::PF_Norm (pa, pb, dim, P1, P2), te, "PF", dim);
    expectNear (
        pcl::K_Norm_Std (pa, pb, dim, P1, P2), pcl::K_Norm (pa, pb, dim, P1, P2), te, "K", dim);
  }
}

TEST (NormsStdVsRvv, Div_KL_positive)
{
  for (int dim : {0, 1, 8, 17, 100, 512})
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fillPositive (a, b, 701u + static_cast<std::uint32_t> (dim));
    float* pa = a.data ();
    float* pb = b.data ();
    const float tl = tolLog (dim);

    expectNear (pcl::Div_Norm_Std (pa, pb, dim), pcl::Div_Norm (pa, pb, dim), tl, "Div", dim);
    expectNear (pcl::KL_Norm_Std (pa, pb, dim), pcl::KL_Norm (pa, pb, dim), tl, "KL", dim);
  }
}

TEST (NormsStdVsRvv, selectNorm_vector_matches_std_dispatch)
{
  constexpr float P1 = 0.65f;
  constexpr float P2 = 1.15f;
  for (int dim : {0, 1, 16, 128})
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fill (a, b, 801u + static_cast<std::uint32_t> (dim));
    const float te = tolOrdered (dim);

    expectNear (
        pcl::L1_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::L1),
        te,
        "sel L1",
        dim);
    expectNear (
        pcl::L2_Norm_SQR_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::L2_SQR),
        te,
        "sel L2_SQR",
        dim);
    expectNear (
        std::sqrt (pcl::L2_Norm_SQR_Std (a, b, dim)),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::L2),
        te,
        "sel L2",
        dim);
    expectNear (
        pcl::Linf_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::LINF),
        te,
        "sel LINF",
        dim);

    fillNonNegative (a, b, 802u + static_cast<std::uint32_t> (dim));
    expectNear (
        pcl::JM_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::JM),
        te,
        "sel JM",
        dim);
    expectNear (
        pcl::B_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::B),
        te,
        "sel B",
        dim);
    expectNear (
        pcl::Sublinear_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::SUBLINEAR),
        te,
        "sel SUBLINEAR",
        dim);
    expectNear (
        pcl::CS_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::CS),
        te,
        "sel CS",
        dim);
    expectNear (
        pcl::HIK_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::HIK),
        te,
        "sel HIK",
        dim);

    fillPositive (a, b, 803u + static_cast<std::uint32_t> (dim));
    const float tl = tolLog (dim);
    expectNear (
        pcl::Div_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::DIV),
        tl,
        "sel DIV",
        dim);
    expectNear (
        pcl::KL_Norm_Std (a, b, dim),
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::KL),
        tl,
        "sel KL",
        dim);

    fillNonNegative (a, b, 804u + static_cast<std::uint32_t> (dim));
    expectNear (
        pcl::PF_Norm_Std (a, b, dim, P1, P2), pcl::PF_Norm (a, b, dim, P1, P2), te, "PF vec", dim);
    expectNear (
        pcl::K_Norm_Std (a, b, dim, P1, P2), pcl::K_Norm (a, b, dim, P1, P2), te, "K vec", dim);
  }
}

int
main (int argc, char** argv)
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
