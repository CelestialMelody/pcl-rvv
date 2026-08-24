#pragma once

/*
 * Phase 001 test-only candidate（仅测试候选）。
 *
 * 该 helper 只覆盖 dense sequential row indices（连续 SPFH 行索引）和
 * 11+11+11 bin 的 weighted SPFH（加权 SPFH）组件。其它形态回退到
 * scalar reference（标量参考），避免把诊断范围扩大成 production 泛型结论。
 */

#include "impl/fpfh_reference.hpp"

#include <cstddef>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::features::rvv_test::fpfh
{

inline bool
isDenseSequentialWeightedSPFHShape(const Eigen::MatrixXf& hist_f1,
                                   const Eigen::MatrixXf& hist_f2,
                                   const Eigen::MatrixXf& hist_f3,
                                   const pcl::Indices& indices,
                                   const std::vector<float>& dists)
{
  if (indices.size() != dists.size())
    return false;
  if (hist_f1.cols() != 11 || hist_f2.cols() != 11 || hist_f3.cols() != 11)
    return false;
  if (hist_f1.rows() != hist_f2.rows() || hist_f1.rows() != hist_f3.rows())
    return false;
  if (static_cast<Eigen::Index>(indices.size()) > hist_f1.rows())
    return false;

  for (std::size_t i = 0; i < indices.size(); ++i)
  {
    if (indices[i] != static_cast<pcl::index_t>(i))
      return false;
  }
  return true;
}

#if defined(__RVV10__)
inline void
weightPointSPFHDenseRowsRvvKernel(const Eigen::MatrixXf& hist_f1,
                                  const Eigen::MatrixXf& hist_f2,
                                  const Eigen::MatrixXf& hist_f3,
                                  const pcl::Indices& indices,
                                  const std::vector<float>& dists,
                                  Eigen::VectorXf& fpfh_histogram)
{
  constexpr Eigen::Index kBins = 11;
  constexpr Eigen::Index kTotalBins = 33;
  const Eigen::Index rows = hist_f1.rows();
  const ptrdiff_t bin_stride_bytes = static_cast<ptrdiff_t>(rows * static_cast<Eigen::Index>(sizeof(float)));

  fpfh_histogram.setZero(kTotalBins);

  for (std::size_t idx = 0; idx < indices.size(); ++idx)
  {
    if (dists[idx] == 0.0f)
      continue;

    const Eigen::Index row = static_cast<Eigen::Index>(indices[idx]);
    const float weight = 1.0f / dists[idx];
    const float* bases[3] = {hist_f1.data() + row, hist_f2.data() + row, hist_f3.data() + row};

    for (Eigen::Index segment = 0; segment < 3; ++segment)
    {
      for (Eigen::Index bin = 0; bin < kBins;)
      {
        const std::size_t vl = __riscv_vsetvl_e32m2(static_cast<std::size_t>(kBins - bin));
        const vfloat32m2_t src = __riscv_vlse32_v_f32m2(bases[segment] + bin * rows, bin_stride_bytes, vl);
        const vfloat32m2_t acc = __riscv_vle32_v_f32m2(fpfh_histogram.data() + segment * kBins + bin, vl);
        __riscv_vse32_v_f32m2(fpfh_histogram.data() + segment * kBins + bin,
                              __riscv_vfmacc_vf_f32m2(acc, weight, src, vl),
                              vl);
        bin += static_cast<Eigen::Index>(vl);
      }
    }
  }

  for (Eigen::Index segment = 0; segment < 3; ++segment)
  {
    double sum = 0.0;
    for (Eigen::Index bin = 0; bin < kBins; ++bin)
      sum += fpfh_histogram[segment * kBins + bin];
    const float scale = sum == 0.0 ? 0.0f : static_cast<float>(100.0 / sum);

    for (Eigen::Index bin = 0; bin < kBins;)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2(static_cast<std::size_t>(kBins - bin));
      const vfloat32m2_t values = __riscv_vle32_v_f32m2(fpfh_histogram.data() + segment * kBins + bin, vl);
      __riscv_vse32_v_f32m2(fpfh_histogram.data() + segment * kBins + bin,
                            __riscv_vfmul_vf_f32m2(values, scale, vl),
                            vl);
      bin += static_cast<Eigen::Index>(vl);
    }
  }
}
#endif

inline void
weightPointSPFHDenseRowsRVV(const Eigen::MatrixXf& hist_f1,
                            const Eigen::MatrixXf& hist_f2,
                            const Eigen::MatrixXf& hist_f3,
                            const pcl::Indices& indices,
                            const std::vector<float>& dists,
                            Eigen::VectorXf& fpfh_histogram)
{
  if (!isDenseSequentialWeightedSPFHShape(hist_f1, hist_f2, hist_f3, indices, dists))
  {
    weightPointSPFHReference(hist_f1, hist_f2, hist_f3, indices, dists, fpfh_histogram);
    return;
  }

#if defined(__RVV10__)
  weightPointSPFHDenseRowsRvvKernel(hist_f1, hist_f2, hist_f3, indices, dists, fpfh_histogram);
#else
  weightPointSPFHReference(hist_f1, hist_f2, hist_f3, indices, dists, fpfh_histogram);
#endif
}

} // namespace pcl::features::rvv_test::fpfh
