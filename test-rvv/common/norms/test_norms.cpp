/**
 * test_norms.cpp — pcl/common/norms.h 正确性（朴素 float 参考，容差比较）
 */
#include <pcl/common/norms.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

namespace {

/** RVV 有序规约 vs 从左到右标量累加；含 sqrt/log 的链略放宽。 */
inline float
tolRvvVsNaive (int dim)
{
  return std::max (1e-5f, 2e-6f * static_cast<float> (std::max (dim, 1)));
}

/** Div / KL：向量块内与标量同序、同 \c std::log，容差可更紧。 */
inline float
tolRvvLogNorms (int dim)
{
  (void)dim;
  return 1e-5f;
}

float
ref_L1_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
    s += std::abs (a[static_cast<std::size_t> (i)] - b[static_cast<std::size_t> (i)]);
  return s;
}

float
ref_L2_sqr_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float d = a[static_cast<std::size_t> (i)] - b[static_cast<std::size_t> (i)];
    s += d * d;
  }
  return s;
}

float
ref_Linf_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float m = 0.f;
  for (int i = 0; i < dim; ++i)
    m = (std::max) (m, std::abs (a[static_cast<std::size_t> (i)] - b[static_cast<std::size_t> (i)]));
  return m;
}

float
ref_JM_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float d =
        std::sqrt (a[static_cast<std::size_t> (i)]) - std::sqrt (b[static_cast<std::size_t> (i)]);
    s += d * d;
  }
  return std::sqrt (s);
}

float
ref_B_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
    s += std::sqrt (a[static_cast<std::size_t> (i)] * b[static_cast<std::size_t> (i)]);
  if (s > 0.f)
    return -std::log (s);
  return 0.f;
}

float
ref_Sublinear_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
    s += std::sqrt (
        std::abs (a[static_cast<std::size_t> (i)] - b[static_cast<std::size_t> (i)]));
  return s;
}

float
ref_CS_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float ai = a[static_cast<std::size_t> (i)];
    const float bi = b[static_cast<std::size_t> (i)];
    if ((ai + bi) != 0.f)
      s += (ai - bi) * (ai - bi) / (ai + bi);
  }
  return s;
}

float
ref_Div_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float ai = a[static_cast<std::size_t> (i)];
    const float bi = b[static_cast<std::size_t> (i)];
    const float r = ai / bi;
    if (r > 0.f)
      s += (ai - bi) * std::log (r);
  }
  return s;
}

float
ref_KL_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float ai = a[static_cast<std::size_t> (i)];
    const float bi = b[static_cast<std::size_t> (i)];
    const float r = ai / bi;
    if ((bi != 0.f) && (r > 0.f))
      s += ai * std::log (r);
  }
  return s;
}

float
ref_HIK_naive (const std::vector<float>& a, const std::vector<float>& b, int dim)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
    s += (std::min) (a[static_cast<std::size_t> (i)], b[static_cast<std::size_t> (i)]);
  return s;
}

float
ref_PF_naive (const std::vector<float>& a, const std::vector<float>& b, int dim, float P1, float P2)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
  {
    const float d =
        P1 * a[static_cast<std::size_t> (i)] - P2 * b[static_cast<std::size_t> (i)];
    s += d * d;
  }
  return std::sqrt (s);
}

float
ref_K_naive (const std::vector<float>& a, const std::vector<float>& b, int dim, float P1, float P2)
{
  float s = 0.f;
  for (int i = 0; i < dim; ++i)
    s += std::abs (P1 * a[static_cast<std::size_t> (i)] - P2 * b[static_cast<std::size_t> (i)]);
  return s;
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

/** a、b 严格为正，便于 Div / KL 与朴素参考对齐。 */
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

/** 非负，满足 JM / B 等对 \c sqrt(a)、\c sqrt(a*b) 的常见定义域。 */
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

} // namespace

TEST (NormsTest, L1_L2sqr_Linf_PointerAndVector)
{
  for (int dim : {0, 1, 8, 15, 16, 17, 36, 200, 1024})
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fill (a, b, 99u + static_cast<std::uint32_t> (dim));

    const float r1 = ref_L1_naive (a, b, dim);
    const float r2 = ref_L2_sqr_naive (a, b, dim);
    const float ri = ref_Linf_naive (a, b, dim);
#if defined(__RVV10__)
    const float te = tolRvvVsNaive (dim);
#endif

    const float p_l1 = pcl::L1_Norm (a.data (), b.data (), dim);
    const float p_l2s = pcl::L2_Norm_SQR (a.data (), b.data (), dim);
    const float p_li = pcl::Linf_Norm (a.data (), b.data (), dim);
    const float p_l2 = pcl::L2_Norm (a.data (), b.data (), dim);

#if defined(__RVV10__)
    EXPECT_NEAR (p_l1, r1, te) << "dim=" << dim << " L1 ptr";
    EXPECT_NEAR (p_l2s, r2, te) << "dim=" << dim << " L2^2 ptr";
    EXPECT_NEAR (p_li, ri, te) << "dim=" << dim << " Linf ptr";
    EXPECT_NEAR (p_l2, std::sqrt (r2), te) << "dim=" << dim << " L2 ptr";
#else
    EXPECT_FLOAT_EQ (p_l1, r1) << "dim=" << dim << " L1 ptr";
    EXPECT_FLOAT_EQ (p_l2s, r2) << "dim=" << dim << " L2^2 ptr";
    EXPECT_FLOAT_EQ (p_li, ri) << "dim=" << dim << " Linf ptr";
    EXPECT_NEAR (p_l2, std::sqrt (r2), 1e-6f) << "dim=" << dim << " L2 ptr";
#endif

    const float v_l1 = pcl::L1_Norm (a, b, dim);
    const float v_l2s = pcl::L2_Norm_SQR (a, b, dim);
    const float v_li = pcl::Linf_Norm (a, b, dim);
#if defined(__RVV10__)
    EXPECT_NEAR (v_l1, r1, te) << "dim=" << dim << " L1 vector";
    EXPECT_NEAR (v_l2s, r2, te) << "dim=" << dim << " L2^2 vector";
    EXPECT_NEAR (v_li, ri, te) << "dim=" << dim << " Linf vector";
#else
    EXPECT_FLOAT_EQ (v_l1, r1) << "dim=" << dim << " L1 vector";
    EXPECT_FLOAT_EQ (v_l2s, r2) << "dim=" << dim << " L2^2 vector";
    EXPECT_FLOAT_EQ (v_li, ri) << "dim=" << dim << " Linf vector";
#endif

#if defined(__RVV10__)
    EXPECT_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::L1), r1, te);
    EXPECT_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::L2_SQR), r2, te);
    EXPECT_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::LINF), ri, te);
#else
    EXPECT_FLOAT_EQ (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::L1), r1);
    EXPECT_FLOAT_EQ (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::L2_SQR), r2);
    EXPECT_FLOAT_EQ (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::LINF), ri);
#endif
  }
}

TEST (NormsTest, JM_B_Sublinear_CS_HIK_PF_K)
{
  constexpr float P1 = 0.65f;
  constexpr float P2 = 1.15f;
  for (int dim : {0, 1, 8, 16, 17, 64, 256})
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fillNonNegative (a, b, 200u + static_cast<std::uint32_t> (dim));

    const float r_jm = ref_JM_naive (a, b, dim);
    const float r_b = ref_B_naive (a, b, dim);
    const float r_sub = ref_Sublinear_naive (a, b, dim);
    const float r_cs = ref_CS_naive (a, b, dim);
    const float r_hik = ref_HIK_naive (a, b, dim);
    const float r_pf = ref_PF_naive (a, b, dim, P1, P2);
    const float r_k = ref_K_naive (a, b, dim, P1, P2);

#if defined(__RVV10__)
    const float te = tolRvvVsNaive (dim);
#define PCL_NORMS_EXPECT_RVV_NEAR(e, r, msg) EXPECT_NEAR ((e), (r), te) << (msg)
#else
#define PCL_NORMS_EXPECT_RVV_NEAR(e, r, msg) EXPECT_FLOAT_EQ ((e), (r)) << (msg)
#endif

    PCL_NORMS_EXPECT_RVV_NEAR (pcl::JM_Norm (a.data (), b.data (), dim), r_jm, "JM ptr");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::B_Norm (a.data (), b.data (), dim), r_b, "B ptr");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::Sublinear_Norm (a.data (), b.data (), dim), r_sub, "Sub ptr");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::CS_Norm (a.data (), b.data (), dim), r_cs, "CS ptr");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::HIK_Norm (a.data (), b.data (), dim), r_hik, "HIK ptr");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::PF_Norm (a.data (), b.data (), dim, P1, P2), r_pf, "PF ptr");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::K_Norm (a.data (), b.data (), dim, P1, P2), r_k, "K ptr");

    PCL_NORMS_EXPECT_RVV_NEAR (pcl::JM_Norm (a, b, dim), r_jm, "JM vec");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::B_Norm (a, b, dim), r_b, "B vec");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::Sublinear_Norm (a, b, dim), r_sub, "Sub vec");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::CS_Norm (a, b, dim), r_cs, "CS vec");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::HIK_Norm (a, b, dim), r_hik, "HIK vec");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::PF_Norm (a, b, dim, P1, P2), r_pf, "PF vec");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::K_Norm (a, b, dim, P1, P2), r_k, "K vec");

    PCL_NORMS_EXPECT_RVV_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::JM), r_jm, "sel JM");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::B), r_b, "sel B");
    PCL_NORMS_EXPECT_RVV_NEAR (
        pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::SUBLINEAR), r_sub, "sel SUB");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::CS), r_cs, "sel CS");
    PCL_NORMS_EXPECT_RVV_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::HIK), r_hik, "sel HIK");

#undef PCL_NORMS_EXPECT_RVV_NEAR
  }
}

TEST (NormsTest, Div_KL_PositiveData)
{
  for (int dim : {0, 1, 8, 16, 17, 100, 512})
  {
    std::vector<float> a (static_cast<std::size_t> (dim));
    std::vector<float> b (static_cast<std::size_t> (dim));
    fillPositive (a, b, 300u + static_cast<std::uint32_t> (dim));

    const float r_div = ref_Div_naive (a, b, dim);
    const float r_kl = ref_KL_naive (a, b, dim);

#if defined(__RVV10__)
    const float tl = tolRvvLogNorms (dim);
    EXPECT_NEAR (pcl::Div_Norm (a.data (), b.data (), dim), r_div, tl) << "Div ptr dim=" << dim;
    EXPECT_NEAR (pcl::KL_Norm (a.data (), b.data (), dim), r_kl, tl) << "KL ptr dim=" << dim;
    EXPECT_NEAR (pcl::Div_Norm (a, b, dim), r_div, tl) << "Div vec";
    EXPECT_NEAR (pcl::KL_Norm (a, b, dim), r_kl, tl) << "KL vec";
    EXPECT_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::DIV), r_div, tl);
    EXPECT_NEAR (pcl::selectNorm<std::vector<float>> (a, b, dim, pcl::KL), r_kl, tl);
#else
    EXPECT_FLOAT_EQ (pcl::Div_Norm (a.data (), b.data (), dim), r_div);
    EXPECT_FLOAT_EQ (pcl::KL_Norm (a.data (), b.data (), dim), r_kl);
#endif
  }
}
