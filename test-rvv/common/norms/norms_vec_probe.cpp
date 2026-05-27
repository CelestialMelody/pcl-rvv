/**
 * norms_vec_probe.cpp
 * 仅编译为 .o，用于对 norms.hpp 内标量循环做 fopt-info-vec-missed 诊断（见 Makefile compile_norms_probe）。
 */
#include <pcl/common/norms.h>

#include <vector>

void
pcl_norms_vec_probe (float* a, float* b, int n, std::vector<float>& v)
{
  constexpr float P1 = 1.f;
  constexpr float P2 = 1.f;
  (void)pcl::L1_Norm (a, b, n);
  (void)pcl::L2_Norm_SQR (a, b, n);
  (void)pcl::Linf_Norm (a, b, n);
  (void)pcl::L2_Norm (a, b, n);
  (void)pcl::JM_Norm (a, b, n);
  (void)pcl::B_Norm (a, b, n);
  (void)pcl::Sublinear_Norm (a, b, n);
  (void)pcl::CS_Norm (a, b, n);
  (void)pcl::Div_Norm (a, b, n);
  (void)pcl::KL_Norm (a, b, n);
  (void)pcl::HIK_Norm (a, b, n);
  (void)pcl::PF_Norm (a, b, n, P1, P2);
  (void)pcl::K_Norm (a, b, n, P1, P2);
  (void)pcl::selectNorm<std::vector<float>> (v, v, static_cast<int> (v.size ()), pcl::L1);
}
