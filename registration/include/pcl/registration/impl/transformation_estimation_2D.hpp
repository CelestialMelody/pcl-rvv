/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2012-, Open Perception Inc.
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
 */

#ifndef PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_2D_HPP_
#define PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_2D_HPP_

#include <pcl/common/point_tests.h>
#include <pcl/point_types.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef __RVV10__
#include <pcl/rvv_point_load.h>
#include <riscv_vector.h>
#endif

namespace pcl {

namespace registration {

#ifdef __RVV10__
namespace detail {

struct TransformationEstimation2DAccumulation {
  float source_centroid[2]{0.0f, 0.0f};
  float target_centroid[2]{0.0f, 0.0f};
  float correlation[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

inline float
reduceTransformationEstimation2DF32M2(const vfloat32m2_t value,
                                       const std::size_t vlmax)
{
  const vfloat32m1_t zero = __riscv_vfmv_s_f_f32m1(0.0f, 1);
  return __riscv_vfmv_f_s_f32m1_f32(
      __riscv_vfredosum_vs_f32m2_f32m1(value, zero, vlmax));
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline bool
tryTransformationEstimation2DOrderedCloudPairRVV(
    const pcl::PointCloud<PointSource>& cloud_src,
    const pcl::PointCloud<PointTarget>& cloud_tgt,
    Eigen::Matrix<Scalar, 4, 4>& transformation_matrix)
{
  if constexpr (!std::is_same_v<PointSource, pcl::PointXYZ> ||
                !std::is_same_v<PointTarget, pcl::PointXYZ> ||
                !std::is_same_v<Scalar, float>) {
    return false;
  }

  const std::size_t nr_points = cloud_src.size();
  if (nr_points < 16 || !cloud_src.is_dense || !cloud_tgt.is_dense)
    return false;

  for (std::size_t i = 0; i < nr_points; ++i) {
    if (!pcl::isFinite(cloud_src.points[i]) || !pcl::isFinite(cloud_tgt.points[i]))
      return false;
  }

  TransformationEstimation2DAccumulation acc;
  const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
  const vfloat32m2_t zero = __riscv_vfmv_v_f_f32m2(0.0f, vlmax);
  vfloat32m2_t source_sum_x = zero, source_sum_y = zero;
  vfloat32m2_t target_sum_x = zero, target_sum_y = zero;
  const auto* source_base =
      reinterpret_cast<const std::uint8_t*>(cloud_src.points.data());
  const auto* target_base =
      reinterpret_cast<const std::uint8_t*>(cloud_tgt.points.data());

  std::size_t i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       offsetof(PointSource, x),
                                       offsetof(PointSource, y),
                                       offsetof(PointSource, z)>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       offsetof(PointTarget, x),
                                       offsetof(PointTarget, y),
                                       offsetof(PointTarget, z)>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    source_sum_x = __riscv_vfadd_vv_f32m2_tu(source_sum_x, source_sum_x, sx, vl);
    source_sum_y = __riscv_vfadd_vv_f32m2_tu(source_sum_y, source_sum_y, sy, vl);
    target_sum_x = __riscv_vfadd_vv_f32m2_tu(target_sum_x, target_sum_x, tx, vl);
    target_sum_y = __riscv_vfadd_vv_f32m2_tu(target_sum_y, target_sum_y, ty, vl);
    i += vl;
  }

  const float inv_n = 1.0f / static_cast<float>(nr_points);
  acc.source_centroid[0] =
      reduceTransformationEstimation2DF32M2(source_sum_x, vlmax) * inv_n;
  acc.source_centroid[1] =
      reduceTransformationEstimation2DF32M2(source_sum_y, vlmax) * inv_n;
  acc.target_centroid[0] =
      reduceTransformationEstimation2DF32M2(target_sum_x, vlmax) * inv_n;
  acc.target_centroid[1] =
      reduceTransformationEstimation2DF32M2(target_sum_y, vlmax) * inv_n;

  vfloat32m2_t source_x_target_x = zero, source_x_target_y = zero;
  vfloat32m2_t source_y_target_x = zero, source_y_target_y = zero;
  i = 0;
  while (i < nr_points) {
    const std::size_t vl = __riscv_vsetvl_e32m2(nr_points - i);
    vfloat32m2_t sx, sy, sz, tx, ty, tz;
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointSource),
                                       offsetof(PointSource, x),
                                       offsetof(PointSource, y),
                                       offsetof(PointSource, z)>(
        source_base + i * sizeof(PointSource), vl, sx, sy, sz);
    pcl::rvv_load::strided_load3_f32m2<sizeof(PointTarget),
                                       offsetof(PointTarget, x),
                                       offsetof(PointTarget, y),
                                       offsetof(PointTarget, z)>(
        target_base + i * sizeof(PointTarget), vl, tx, ty, tz);
    const vfloat32m2_t centered_source_x =
        __riscv_vfsub_vf_f32m2(sx, acc.source_centroid[0], vl);
    const vfloat32m2_t centered_source_y =
        __riscv_vfsub_vf_f32m2(sy, acc.source_centroid[1], vl);
    const vfloat32m2_t centered_target_x =
        __riscv_vfsub_vf_f32m2(tx, acc.target_centroid[0], vl);
    const vfloat32m2_t centered_target_y =
        __riscv_vfsub_vf_f32m2(ty, acc.target_centroid[1], vl);
    source_x_target_x = __riscv_vfmacc_vv_f32m2_tu(
        source_x_target_x, centered_source_x, centered_target_x, vl);
    source_x_target_y = __riscv_vfmacc_vv_f32m2_tu(
        source_x_target_y, centered_source_x, centered_target_y, vl);
    source_y_target_x = __riscv_vfmacc_vv_f32m2_tu(
        source_y_target_x, centered_source_y, centered_target_x, vl);
    source_y_target_y = __riscv_vfmacc_vv_f32m2_tu(
        source_y_target_y, centered_source_y, centered_target_y, vl);
    i += vl;
  }

  acc.correlation[0] =
      reduceTransformationEstimation2DF32M2(source_x_target_x, vlmax);
  acc.correlation[1] =
      reduceTransformationEstimation2DF32M2(source_x_target_y, vlmax);
  acc.correlation[2] =
      reduceTransformationEstimation2DF32M2(source_y_target_x, vlmax);
  acc.correlation[3] =
      reduceTransformationEstimation2DF32M2(source_y_target_y, vlmax);

  const float angle =
      std::atan2(acc.correlation[1] - acc.correlation[2],
                 acc.correlation[0] + acc.correlation[3]);
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  transformation_matrix.setIdentity();
  transformation_matrix(0, 0) = c;
  transformation_matrix(0, 1) = -s;
  transformation_matrix(1, 0) = s;
  transformation_matrix(1, 1) = c;
  transformation_matrix(0, 3) =
      acc.target_centroid[0] -
      (c * acc.source_centroid[0] - s * acc.source_centroid[1]);
  transformation_matrix(1, 3) =
      acc.target_centroid[1] -
      (s * acc.source_centroid[0] + c * acc.source_centroid[1]);
  transformation_matrix(2, 3) = 0.0f;
  return true;
}

} // namespace detail
#endif

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimation2D<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  const auto nr_points = cloud_src.size();
  if (cloud_tgt.size() != nr_points) {
    PCL_ERROR("[pcl::TransformationEstimation2D::estimateRigidTransformation] Number "
              "or points in source (%zu) differs than target (%zu)!\n",
              static_cast<std::size_t>(nr_points),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

#ifdef __RVV10__
  if (detail::tryTransformationEstimation2DOrderedCloudPairRVV(
          cloud_src, cloud_tgt, transformation_matrix))
    return;
#endif

  ConstCloudIterator<PointSource> source_it(cloud_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimation2D<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                Matrix4& transformation_matrix) const
{
  if (indices_src.size() != cloud_tgt.size()) {
    PCL_ERROR("[pcl::Transformation2D::estimateRigidTransformation] Number or points "
              "in source (%zu) differs than target (%zu)!\n",
              indices_src.size(),
              static_cast<std::size_t>(cloud_tgt.size()));
    return;
  }

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimation2D<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::Indices& indices_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Indices& indices_tgt,
                                Matrix4& transformation_matrix) const
{
  if (indices_src.size() != indices_tgt.size()) {
    PCL_ERROR("[pcl::TransformationEstimation2D::estimateRigidTransformation] Number "
              "or points in source (%lu) differs than target (%lu)!\n",
              indices_src.size(),
              indices_tgt.size());
    return;
  }

  ConstCloudIterator<PointSource> source_it(cloud_src, indices_src);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, indices_tgt);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimation2D<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                                const pcl::PointCloud<PointTarget>& cloud_tgt,
                                const pcl::Correspondences& correspondences,
                                Matrix4& transformation_matrix) const
{
  ConstCloudIterator<PointSource> source_it(cloud_src, correspondences, true);
  ConstCloudIterator<PointTarget> target_it(cloud_tgt, correspondences, false);
  estimateRigidTransformation(source_it, target_it, transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
inline void
TransformationEstimation2D<PointSource, PointTarget, Scalar>::
    estimateRigidTransformation(ConstCloudIterator<PointSource>& source_it,
                                ConstCloudIterator<PointTarget>& target_it,
                                Matrix4& transformation_matrix) const
{
  source_it.reset();
  target_it.reset();

  Eigen::Matrix<Scalar, 4, 1> centroid_src, centroid_tgt;
  // Estimate the centroids of source, target
  compute3DCentroid(source_it, centroid_src);
  compute3DCentroid(target_it, centroid_tgt);
  source_it.reset();
  target_it.reset();

  // ignore z component
  centroid_src[2] = 0.0f;
  centroid_tgt[2] = 0.0f;
  // Subtract the centroids from source, target
  Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> cloud_src_demean,
      cloud_tgt_demean;
  demeanPointCloud(source_it, centroid_src, cloud_src_demean);
  demeanPointCloud(target_it, centroid_tgt, cloud_tgt_demean);

  getTransformationFromCorrelation(cloud_src_demean,
                                   centroid_src,
                                   cloud_tgt_demean,
                                   centroid_tgt,
                                   transformation_matrix);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
TransformationEstimation2D<PointSource, PointTarget, Scalar>::
    getTransformationFromCorrelation(
        const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>& cloud_src_demean,
        const Eigen::Matrix<Scalar, 4, 1>& centroid_src,
        const Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>& cloud_tgt_demean,
        const Eigen::Matrix<Scalar, 4, 1>& centroid_tgt,
        Matrix4& transformation_matrix) const
{
  transformation_matrix.setIdentity();

  // Assemble the correlation matrix H = source * target'
  Eigen::Matrix<Scalar, 3, 3> H =
      (cloud_src_demean * cloud_tgt_demean.transpose()).template topLeftCorner<3, 3>();

  float angle = std::atan2((H(0, 1) - H(1, 0)), (H(0, 0) + H(1, 1)));

  Eigen::Matrix<Scalar, 3, 3> R(Eigen::Matrix<Scalar, 3, 3>::Identity());
  R(0, 0) = R(1, 1) = std::cos(angle);
  R(0, 1) = -std::sin(angle);
  R(1, 0) = std::sin(angle);

  // Return the correct transformation
  transformation_matrix.template topLeftCorner<3, 3>().matrix() = R;
  const Eigen::Matrix<Scalar, 3, 1> Rc(R * centroid_src.template head<3>().matrix());
  transformation_matrix.template block<3, 1>(0, 3).matrix() =
      centroid_tgt.template head<3>() - Rc;
}

} // namespace registration
} // namespace pcl

#endif // PCL_REGISTRATION_TRANSFORMATION_ESTIMATION_2D_HPP_
