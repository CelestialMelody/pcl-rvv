/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2011, Alexandru-Eugen Ichim
 *  Copyright (c) 2012-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#ifndef PCL_FEATURES_IMPL_PPF_H_
#define PCL_FEATURES_IMPL_PPF_H_

#include <pcl/features/ppf.h>
#include <pcl/features/pfh.h>
#include <pcl/features/pfh_tools.h> // for computePairFeatures
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h> // for KdTree

#include <cmath>
#include <limits>

#if defined(__RVV10__)
#include <pcl/common/impl/rvv_math.hpp>
#include <pcl/rvv_point_traits.h>

#include <riscv_vector.h>

#include <cstdint>
#include <type_traits>
#include <vector>
#endif

#if defined(__RVV10__) && defined(PCL_RVV_PPF_ENABLE_TEST_TRACE)
#include <cstddef>
extern "C" std::size_t pcl_rvv_ppf_alpha_m_trace_hits;
#endif

namespace pcl::detail
{
  template <typename PointOutT> inline void
  setPPFNaN (PointOutT &p)
  {
    p.f1 = p.f2 = p.f3 = p.f4 = p.alpha_m =
        std::numeric_limits<float>::quiet_NaN ();
  }

  template <typename PointInT, typename PointNT, typename PointOutT> void
  computePPFFeatureStd (const pcl::PointCloud<PointInT> &input,
                        const pcl::PointCloud<PointNT> &normals,
                        const pcl::Indices &indices,
                        pcl::PointCloud<PointOutT> &output,
                        const char* class_name)
  {
    // Initialize output container - overwrite the sizes done by Feature::initCompute ()
    output.resize (indices.size () * input.size ());
    output.height = 1;
    output.width = output.size ();
    output.is_dense = true;

    // Compute point pair features for every pair of points in the cloud
    for (std::size_t index_i = 0; index_i < indices.size (); ++index_i)
    {
      std::size_t i = indices[index_i];
      for (std::size_t j = 0 ; j < input.size (); ++j)
      {
        PointOutT p;
        if (i != j)
        {
          if (//pcl::computePPFPairFeature
              pcl::computePairFeatures (input[i].getVector4fMap (),
                                        normals[i].getNormalVector4fMap (),
                                        input[j].getVector4fMap (),
                                        normals[j].getNormalVector4fMap (),
                                        p.f1, p.f2, p.f3, p.f4))
          {
            // Calculate alpha_m angle
            Eigen::Vector3f model_reference_point = input[i].getVector3fMap (),
                            model_reference_normal = normals[i].getNormalVector3fMap (),
                            model_point = input[j].getVector3fMap ();
            float rotation_angle = std::acos (model_reference_normal.dot (Eigen::Vector3f::UnitX ()));
            bool parallel_to_x = (model_reference_normal.y() == 0.0f && model_reference_normal.z() == 0.0f);
            Eigen::Vector3f rotation_axis = (parallel_to_x)?(Eigen::Vector3f::UnitY ()):(model_reference_normal.cross (Eigen::Vector3f::UnitX ()). normalized());
            Eigen::AngleAxisf rotation_mg (rotation_angle, rotation_axis);
            Eigen::Affine3f transform_mg (Eigen::Translation3f ( rotation_mg * ((-1) * model_reference_point)) * rotation_mg);

            Eigen::Vector3f model_point_transformed = transform_mg * model_point;
            float angle = std::atan2 ( -model_point_transformed(2), model_point_transformed(1));
            if (std::sin (angle) * model_point_transformed(2) < 0.0f)
              angle *= (-1);
            p.alpha_m = -angle;
          }
          else
          {
            PCL_ERROR ("[pcl::%s::computeFeature] Computing pair feature vector between points %u and %u went wrong.\n", class_name, i, j);
            setPPFNaN (p);
            output.is_dense = false;
          }
        }
        // Do not calculate the feature for identity pairs (i, i) as they are not used
        // in the following computations
        else
        {
          setPPFNaN (p);
          output.is_dense = false;
        }

        output[index_i*input.size () + j] = p;
      }
    }
  }

#if defined(__RVV10__)
  template <typename PointT, bool HasNormal = pcl::traits::has_normal<PointT>::value>
  struct PPFNormalAoSFloatLayout : std::false_type {};

  template <typename PointT>
  struct PPFNormalAoSFloatLayout<PointT, true> {
    using Pod = typename pcl::traits::POD<PointT>::type;

    static constexpr std::size_t kNX = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalX;
    static constexpr std::size_t kNY = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalY;
    static constexpr std::size_t kNZ = pcl::rvv::RVVNormalFloatLayout<PointT>::kNormalZ;

    static constexpr bool value =
        pcl::rvv::RVVNormalFloatLayout<PointT>::value &&
        std::is_standard_layout_v<Pod> && sizeof (PointT) == sizeof (Pod) &&
        sizeof (PointT) % alignof (float) == 0 && kNX % alignof (float) == 0 &&
        kNY % alignof (float) == 0 && kNZ % alignof (float) == 0;
  };

  template <typename PointT, std::size_t kFieldOffsetBytes> inline float
  ppfRVVReadFloatField (const PointT &point)
  {
    const auto* base = reinterpret_cast<const std::uint8_t*> (&point);
    return *reinterpret_cast<const float*> (base + kFieldOffsetBytes);
  }

  template <typename PointInT, typename PointNT, typename PointOutT> bool
  computePPFFeatureAlphaMRVV (const pcl::PointCloud<PointInT> &input,
                              const pcl::PointCloud<PointNT> &normals,
                              const pcl::Indices &indices,
                              pcl::PointCloud<PointOutT> &output,
                              const char* class_name)
  {
    if constexpr (!pcl::rvv::RVVXYZAoSFloatLayout<PointInT>::value ||
                  !PPFNormalAoSFloatLayout<PointNT>::value ||
                  !std::is_same_v<PointOutT, pcl::PPFSignature>)
      return false;
    else
    {
      output.resize (indices.size () * input.size ());
      output.height = 1;
      output.width = output.size ();
      output.is_dense = true;

      std::vector<float> dx;
      std::vector<float> dy;
      std::vector<float> dz;
      std::vector<float> nx;
      std::vector<float> ny;
      std::vector<float> nz;
      std::vector<std::size_t> output_rows;
      const std::size_t pair_capacity = indices.size () * input.size ();
      dx.reserve (pair_capacity);
      dy.reserve (pair_capacity);
      dz.reserve (pair_capacity);
      nx.reserve (pair_capacity);
      ny.reserve (pair_capacity);
      nz.reserve (pair_capacity);
      output_rows.reserve (pair_capacity);

      using SourceLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointInT>;
      using NormalLayout = PPFNormalAoSFloatLayout<PointNT>;
      for (std::size_t index_i = 0; index_i < indices.size (); ++index_i)
      {
        const std::size_t i = indices[index_i];
        for (std::size_t j = 0 ; j < input.size (); ++j)
        {
          auto &p = output[index_i*input.size () + j];
          if (i != j)
          {
            if (pcl::computePairFeatures (input[i].getVector4fMap (),
                                          normals[i].getNormalVector4fMap (),
                                          input[j].getVector4fMap (),
                                          normals[j].getNormalVector4fMap (),
                                          p.f1, p.f2, p.f3, p.f4))
            {
              dx.push_back (
                  ppfRVVReadFloatField<PointInT, SourceLayout::kX> (input[j]) -
                  ppfRVVReadFloatField<PointInT, SourceLayout::kX> (input[i]));
              dy.push_back (
                  ppfRVVReadFloatField<PointInT, SourceLayout::kY> (input[j]) -
                  ppfRVVReadFloatField<PointInT, SourceLayout::kY> (input[i]));
              dz.push_back (
                  ppfRVVReadFloatField<PointInT, SourceLayout::kZ> (input[j]) -
                  ppfRVVReadFloatField<PointInT, SourceLayout::kZ> (input[i]));
              nx.push_back (ppfRVVReadFloatField<PointNT, NormalLayout::kNX> (normals[i]));
              ny.push_back (ppfRVVReadFloatField<PointNT, NormalLayout::kNY> (normals[i]));
              nz.push_back (ppfRVVReadFloatField<PointNT, NormalLayout::kNZ> (normals[i]));
              output_rows.push_back (index_i*input.size () + j);
            }
            else
            {
              PCL_ERROR ("[pcl::%s::computeFeature] Computing pair feature vector between points %u and %u went wrong.\n", class_name, i, j);
              setPPFNaN (p);
              output.is_dense = false;
            }
          }
          else
          {
            setPPFNaN (p);
            output.is_dense = false;
          }
        }
      }

      std::vector<float> alpha_m (dx.size ());
      for (std::size_t offset = 0; offset < dx.size ();)
      {
        const std::size_t vl = __riscv_vsetvl_e32m2 (dx.size () - offset);
        const vfloat32m2_t vdx = __riscv_vle32_v_f32m2 (dx.data () + offset, vl);
        const vfloat32m2_t vdy = __riscv_vle32_v_f32m2 (dy.data () + offset, vl);
        const vfloat32m2_t vdz = __riscv_vle32_v_f32m2 (dz.data () + offset, vl);
        const vfloat32m2_t vnx = __riscv_vle32_v_f32m2 (nx.data () + offset, vl);
        const vfloat32m2_t vny = __riscv_vle32_v_f32m2 (ny.data () + offset, vl);
        const vfloat32m2_t vnz = __riscv_vle32_v_f32m2 (nz.data () + offset, vl);

        const vfloat32m2_t one = __riscv_vfmv_v_f_f32m2 (1.0f, vl);
        const vfloat32m2_t yz_norm2 =
            __riscv_vfmacc_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vny, vny, vl), vnz, vnz, vl);
        const vbool16_t parallel_to_x = __riscv_vmfeq_vf_f32m2_b16 (yz_norm2, 0.0f, vl);
        const vfloat32m2_t yz_norm2_safe =
            __riscv_vmerge_vvm_f32m2 (yz_norm2, one, parallel_to_x, vl);
        const vfloat32m2_t scale =
            __riscv_vfdiv_vv_f32m2 (__riscv_vfsub_vv_f32m2 (one, vnx, vl), yz_norm2_safe, vl);
        const vfloat32m2_t axis_dot_delta =
            __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vnz, vdy, vl),
                                    __riscv_vfmul_vv_f32m2 (vny, vdz, vl),
                                    vl);
        const vfloat32m2_t scaled_axis_dot =
            __riscv_vfmul_vv_f32m2 (axis_dot_delta, scale, vl);

        vfloat32m2_t general_y =
            __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vnx, vdy, vl),
                                    __riscv_vfmul_vv_f32m2 (vny, vdx, vl),
                                    vl);
        general_y = __riscv_vfmacc_vv_f32m2 (general_y, vnz, scaled_axis_dot, vl);
        vfloat32m2_t general_z =
            __riscv_vfsub_vv_f32m2 (__riscv_vfmul_vv_f32m2 (vnx, vdz, vl),
                                    __riscv_vfmul_vv_f32m2 (vnz, vdx, vl),
                                    vl);
        general_z = __riscv_vfnmsac_vv_f32m2 (general_z, vny, scaled_axis_dot, vl);

        const vfloat32m2_t vnx2 = __riscv_vfmul_vv_f32m2 (vnx, vnx, vl);
        const vfloat32m2_t sin_angle =
            __riscv_vfsqrt_v_f32m2 (__riscv_vfmax_vf_f32m2 (__riscv_vfsub_vv_f32m2 (one, vnx2, vl), 0.0f, vl), vl);
        const vfloat32m2_t parallel_z =
            __riscv_vfadd_vv_f32m2 (__riscv_vfneg_v_f32m2 (__riscv_vfmul_vv_f32m2 (sin_angle, vdx, vl), vl),
                                    __riscv_vfmul_vv_f32m2 (vnx, vdz, vl),
                                    vl);
        const vfloat32m2_t transformed_y =
            __riscv_vmerge_vvm_f32m2 (general_y, vdy, parallel_to_x, vl);
        const vfloat32m2_t transformed_z =
            __riscv_vmerge_vvm_f32m2 (general_z, parallel_z, parallel_to_x, vl);
        const vfloat32m2_t alpha =
            pcl::atan2_RVV_f32m2 (__riscv_vfneg_v_f32m2 (transformed_z, vl), transformed_y, vl);

        __riscv_vse32_v_f32m2 (alpha_m.data () + offset, alpha, vl);
        for (std::size_t lane = 0; lane < vl; ++lane)
          output[output_rows[offset + lane]].alpha_m = alpha_m[offset + lane];

        offset += vl;
      }

#if defined(PCL_RVV_PPF_ENABLE_TEST_TRACE)
      if (!dx.empty ())
        pcl_rvv_ppf_alpha_m_trace_hits += dx.size ();
#endif
      return true;
    }
  }
#endif
} // namespace pcl::detail

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT>
pcl::PPFEstimation<PointInT, PointNT, PointOutT>::PPFEstimation ()
    : FeatureFromNormals <PointInT, PointNT, PointOutT> ()
{
  feature_name_ = "PPFEstimation";
  // Slight hack in order to pass the check for the presence of a search method in Feature::initCompute ()
  Feature<PointInT, PointOutT>::tree_.reset (new pcl::search::KdTree <PointInT> ());
  Feature<PointInT, PointOutT>::search_radius_ = 1.0f;
}


//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointNT, typename PointOutT> void
pcl::PPFEstimation<PointInT, PointNT, PointOutT>::computeFeature (PointCloudOut &output)
{
#if defined(__RVV10__)
  if (pcl::detail::computePPFFeatureAlphaMRVV (*input_, *normals_, *indices_, output, getClassName ().c_str ()))
    return;
#endif
  pcl::detail::computePPFFeatureStd (*input_, *normals_, *indices_, output, getClassName ().c_str ());
}

#define PCL_INSTANTIATE_PPFEstimation(T,NT,OutT) template class PCL_EXPORTS pcl::PPFEstimation<T,NT,OutT>;


#endif // PCL_FEATURES_IMPL_PPF_H_
