/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *  Copyright (c) 2012-, Open Perception, Inc
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
 *
 */

#ifndef PCL_REGISTRATION_IMPL_ICP_HPP_
#define PCL_REGISTRATION_IMPL_ICP_HPP_

#include <pcl/correspondence.h>
#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#include <pcl/rvv_point_store.h>
#endif

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace pcl {
// NOLINTBEGIN(readability-container-data-pointer)

namespace registration {
namespace detail {

template <typename PointT>
inline void
transformCloudStandard(const pcl::PointCloud<PointT>& input,
                       pcl::PointCloud<PointT>& output,
                       const Eigen::Matrix4f& transform,
                       const bool has_normals,
                       const std::size_t x_idx_offset,
                       const std::size_t y_idx_offset,
                       const std::size_t z_idx_offset,
                       const std::size_t nx_idx_offset,
                       const std::size_t ny_idx_offset,
                       const std::size_t nz_idx_offset)
{
  Eigen::Vector4f pt(0.0f, 0.0f, 0.0f, 1.0f), pt_t;

  // XYZ is ALWAYS present due to the templatization, so we only have to check for
  // normals
  if (has_normals) {
    Eigen::Vector3f nt, nt_t;
    Eigen::Matrix3f rot = transform.block<3, 3>(0, 0);

    for (std::size_t i = 0; i < input.size(); ++i) {
      const auto* data_in = reinterpret_cast<const std::uint8_t*>(&input[i]);
      auto* data_out = reinterpret_cast<std::uint8_t*>(&output[i]);
      memcpy(&pt[0], data_in + x_idx_offset, sizeof(float));
      memcpy(&pt[1], data_in + y_idx_offset, sizeof(float));
      memcpy(&pt[2], data_in + z_idx_offset, sizeof(float));

      if (!std::isfinite(pt[0]) || !std::isfinite(pt[1]) || !std::isfinite(pt[2]))
        continue;

      pt_t.noalias() = transform * pt;

      memcpy(data_out + x_idx_offset, &pt_t[0], sizeof(float));
      memcpy(data_out + y_idx_offset, &pt_t[1], sizeof(float));
      memcpy(data_out + z_idx_offset, &pt_t[2], sizeof(float));

      memcpy(&nt[0], data_in + nx_idx_offset, sizeof(float));
      memcpy(&nt[1], data_in + ny_idx_offset, sizeof(float));
      memcpy(&nt[2], data_in + nz_idx_offset, sizeof(float));

      if (!std::isfinite(nt[0]) || !std::isfinite(nt[1]) || !std::isfinite(nt[2]))
        continue;

      nt_t.noalias() = rot * nt;

      memcpy(data_out + nx_idx_offset, &nt_t[0], sizeof(float));
      memcpy(data_out + ny_idx_offset, &nt_t[1], sizeof(float));
      memcpy(data_out + nz_idx_offset, &nt_t[2], sizeof(float));
    }
  }
  else {
    for (std::size_t i = 0; i < input.size(); ++i) {
      const auto* data_in = reinterpret_cast<const std::uint8_t*>(&input[i]);
      auto* data_out = reinterpret_cast<std::uint8_t*>(&output[i]);
      memcpy(&pt[0], data_in + x_idx_offset, sizeof(float));
      memcpy(&pt[1], data_in + y_idx_offset, sizeof(float));
      memcpy(&pt[2], data_in + z_idx_offset, sizeof(float));

      if (!std::isfinite(pt[0]) || !std::isfinite(pt[1]) || !std::isfinite(pt[2]))
        continue;

      pt_t.noalias() = transform * pt;

      memcpy(data_out + x_idx_offset, &pt_t[0], sizeof(float));
      memcpy(data_out + y_idx_offset, &pt_t[1], sizeof(float));
      memcpy(data_out + z_idx_offset, &pt_t[2], sizeof(float));
    }
  }
}

#if defined(__RVV10__)
inline constexpr std::size_t kICPTransformCloudMinRVVPoints = 32;

inline vbool16_t
finiteMaskF32M2ICP(vfloat32m2_t value, const std::size_t vl)
{
  vbool16_t finite = __riscv_vmfeq_vv_f32m2_b16(value, value, vl);
  finite = __riscv_vmand_mm_b16(
      finite,
      __riscv_vmflt_vf_f32m2_b16(
          __riscv_vfabs_v_f32m2(value, vl), std::numeric_limits<float>::infinity(), vl),
      vl);
  return finite;
}

template <typename PointT,
          std::size_t kStride,
          std::size_t kX,
          std::size_t kY,
          std::size_t kZ>
inline void
transformCloudXYZRVV(const pcl::PointCloud<PointT>& input,
                     pcl::PointCloud<PointT>& output,
                     const Eigen::Matrix4f& transform)
{
  const auto* base_in = reinterpret_cast<const std::uint8_t*>(input.data());
  auto* base_out = reinterpret_cast<std::uint8_t*>(output.data());
  for (std::size_t i = 0; i < input.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(input.size() - i);
    const std::uint8_t* in = base_in + i * sizeof(PointT);
    std::uint8_t* out = base_out + i * sizeof(PointT);
    vfloat32m2_t x, y, z;
    pcl::rvv_load::strided_load3_f32m2<kStride, kX, kY, kZ>(in, vl, x, y, z);

    vbool16_t keep = finiteMaskF32M2ICP(x, vl);
    keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2ICP(y, vl), vl);
    keep = __riscv_vmand_mm_b16(keep, finiteMaskF32M2ICP(z, vl), vl);

    vfloat32m2_t tx = __riscv_vfmul_vf_f32m2(x, transform(0, 0), vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 1), y, vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 2), z, vl);
    tx = __riscv_vfadd_vf_f32m2(tx, transform(0, 3), vl);

    vfloat32m2_t ty = __riscv_vfmul_vf_f32m2(x, transform(1, 0), vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 1), y, vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 2), z, vl);
    ty = __riscv_vfadd_vf_f32m2(ty, transform(1, 3), vl);

    vfloat32m2_t tz = __riscv_vfmul_vf_f32m2(x, transform(2, 0), vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 1), y, vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 2), z, vl);
    tz = __riscv_vfadd_vf_f32m2(tz, transform(2, 3), vl);

    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep, reinterpret_cast<float*>(out + kX), tx, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep, reinterpret_cast<float*>(out + kY), ty, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep, reinterpret_cast<float*>(out + kZ), tz, vl);
    i += vl;
  }
}

template <typename PointT,
          std::size_t kStride,
          std::size_t kX,
          std::size_t kY,
          std::size_t kZ,
          std::size_t kNX,
          std::size_t kNY,
          std::size_t kNZ>
inline void
transformCloudXYZNormalRVV(const pcl::PointCloud<PointT>& input,
                           pcl::PointCloud<PointT>& output,
                           const Eigen::Matrix4f& transform)
{
  const auto* base_in = reinterpret_cast<const std::uint8_t*>(input.data());
  auto* base_out = reinterpret_cast<std::uint8_t*>(output.data());
  for (std::size_t i = 0; i < input.size();) {
    const std::size_t vl = __riscv_vsetvl_e32m2(input.size() - i);
    const std::uint8_t* in = base_in + i * sizeof(PointT);
    std::uint8_t* out = base_out + i * sizeof(PointT);
    vfloat32m2_t x, y, z;
    pcl::rvv_load::strided_load3_f32m2<kStride, kX, kY, kZ>(in, vl, x, y, z);

    vbool16_t keep_xyz = finiteMaskF32M2ICP(x, vl);
    keep_xyz = __riscv_vmand_mm_b16(keep_xyz, finiteMaskF32M2ICP(y, vl), vl);
    keep_xyz = __riscv_vmand_mm_b16(keep_xyz, finiteMaskF32M2ICP(z, vl), vl);

    vfloat32m2_t tx = __riscv_vfmul_vf_f32m2(x, transform(0, 0), vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 1), y, vl);
    tx = __riscv_vfmacc_vf_f32m2(tx, transform(0, 2), z, vl);
    tx = __riscv_vfadd_vf_f32m2(tx, transform(0, 3), vl);

    vfloat32m2_t ty = __riscv_vfmul_vf_f32m2(x, transform(1, 0), vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 1), y, vl);
    ty = __riscv_vfmacc_vf_f32m2(ty, transform(1, 2), z, vl);
    ty = __riscv_vfadd_vf_f32m2(ty, transform(1, 3), vl);

    vfloat32m2_t tz = __riscv_vfmul_vf_f32m2(x, transform(2, 0), vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 1), y, vl);
    tz = __riscv_vfmacc_vf_f32m2(tz, transform(2, 2), z, vl);
    tz = __riscv_vfadd_vf_f32m2(tz, transform(2, 3), vl);

    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_xyz, reinterpret_cast<float*>(out + kX), tx, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_xyz, reinterpret_cast<float*>(out + kY), ty, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_xyz, reinterpret_cast<float*>(out + kZ), tz, vl);

    vfloat32m2_t nx, ny, nz;
    pcl::rvv_load::strided_load3_f32m2<kStride, kNX, kNY, kNZ>(in, vl, nx, ny, nz);
    vbool16_t keep_normal = finiteMaskF32M2ICP(nx, vl);
    keep_normal = __riscv_vmand_mm_b16(keep_normal, finiteMaskF32M2ICP(ny, vl), vl);
    keep_normal = __riscv_vmand_mm_b16(keep_normal, finiteMaskF32M2ICP(nz, vl), vl);
    keep_normal = __riscv_vmand_mm_b16(keep_normal, keep_xyz, vl);

    vfloat32m2_t tnx = __riscv_vfmul_vf_f32m2(nx, transform(0, 0), vl);
    tnx = __riscv_vfmacc_vf_f32m2(tnx, transform(0, 1), ny, vl);
    tnx = __riscv_vfmacc_vf_f32m2(tnx, transform(0, 2), nz, vl);

    vfloat32m2_t tny = __riscv_vfmul_vf_f32m2(nx, transform(1, 0), vl);
    tny = __riscv_vfmacc_vf_f32m2(tny, transform(1, 1), ny, vl);
    tny = __riscv_vfmacc_vf_f32m2(tny, transform(1, 2), nz, vl);

    vfloat32m2_t tnz = __riscv_vfmul_vf_f32m2(nx, transform(2, 0), vl);
    tnz = __riscv_vfmacc_vf_f32m2(tnz, transform(2, 1), ny, vl);
    tnz = __riscv_vfmacc_vf_f32m2(tnz, transform(2, 2), nz, vl);

    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_normal, reinterpret_cast<float*>(out + kNX), tnx, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_normal, reinterpret_cast<float*>(out + kNY), tny, vl);
    pcl::rvv_store::masked_strided_store_f32m2<kStride>(
        keep_normal, reinterpret_cast<float*>(out + kNZ), tnz, vl);
    i += vl;
  }
}

template <typename PointT>
inline bool
tryTransformCloudRVV(const pcl::PointCloud<PointT>& input,
                     pcl::PointCloud<PointT>& output,
                     const Eigen::Matrix4f& transform,
                     const bool has_normals,
                     const std::size_t x_idx_offset,
                     const std::size_t y_idx_offset,
                     const std::size_t z_idx_offset,
                     const std::size_t nx_idx_offset,
                     const std::size_t ny_idx_offset,
                     const std::size_t nz_idx_offset)
{
  if (input.size() < kICPTransformCloudMinRVVPoints)
    return false;

  if (!has_normals) {
    if constexpr (pcl::rvv::kRVVXYZAoSPointCompatible<PointT>) {
      using Layout = pcl::rvv::RVVXYZAoSFloatLayout<PointT>;
      if (x_idx_offset == Layout::kX && y_idx_offset == Layout::kY &&
          z_idx_offset == Layout::kZ) {
        transformCloudXYZRVV<PointT, sizeof(PointT), Layout::kX, Layout::kY, Layout::kZ>(
            input, output, transform);
        return true;
      }
    }
  }
  else {
    if constexpr (pcl::rvv::kRVVXYZNormalPointCompatible<PointT>) {
      using Layout = pcl::rvv::RVVXYZNormalFloatLayout<PointT>;
      if (x_idx_offset == Layout::kX && y_idx_offset == Layout::kY &&
          z_idx_offset == Layout::kZ && nx_idx_offset == Layout::kNX &&
          ny_idx_offset == Layout::kNY && nz_idx_offset == Layout::kNZ) {
        transformCloudXYZNormalRVV<PointT,
                                   sizeof(PointT),
                                   Layout::kX,
                                   Layout::kY,
                                   Layout::kZ,
                                   Layout::kNX,
                                   Layout::kNY,
                                   Layout::kNZ>(input, output, transform);
        return true;
      }
    }
  }
  return false;
}

#endif // defined(__RVV10__)

} // namespace detail
} // namespace registration

template <typename PointSource, typename PointTarget, typename Scalar>
void
IterativeClosestPoint<PointSource, PointTarget, Scalar>::transformCloud(
    const PointCloudSource& input, PointCloudSource& output, const Matrix4& transform)
{
  Eigen::Matrix4f tr = transform.template cast<float>();

#if defined(__RVV10__)
  if constexpr (std::is_same_v<Scalar, float>) {
    if (registration::detail::tryTransformCloudRVV<PointSource>(input,
                                                                output,
                                                                tr,
                                                                source_has_normals_,
                                                                x_idx_offset_,
                                                                y_idx_offset_,
                                                                z_idx_offset_,
                                                                nx_idx_offset_,
                                                                ny_idx_offset_,
                                                                nz_idx_offset_))
      return;
  }
#endif

  registration::detail::transformCloudStandard<PointSource>(input,
                                                            output,
                                                            tr,
                                                            source_has_normals_,
                                                            x_idx_offset_,
                                                            y_idx_offset_,
                                                            z_idx_offset_,
                                                            nx_idx_offset_,
                                                            ny_idx_offset_,
                                                            nz_idx_offset_);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
IterativeClosestPoint<PointSource, PointTarget, Scalar>::computeTransformation(
    PointCloudSource& output, const Matrix4& guess)
{
  // Point cloud containing the correspondences of each point in <input, indices>
  PointCloudSourcePtr input_transformed(new PointCloudSource);

  nr_iterations_ = 0;
  converged_ = false;

  // Initialise final transformation to the guessed one
  final_transformation_ = guess;

  // If the guessed transformation is non identity
  if (guess != Matrix4::Identity()) {
    input_transformed->resize(input_->size());
    // Apply guessed transformation prior to search for neighbours
    transformCloud(*input_, *input_transformed, guess);
  }
  else
    *input_transformed = *input_;

  transformation_ = Matrix4::Identity();

  // Make blobs if necessary
  determineRequiredBlobData();
  PCLPointCloud2::Ptr target_blob(new PCLPointCloud2);
  if (need_target_blob_)
    pcl::toPCLPointCloud2(*target_, *target_blob);

  // Pass in the default target for the Correspondence Estimation/Rejection code
  correspondence_estimation_->setInputTarget(target_);
  if (correspondence_estimation_->requiresTargetNormals())
    correspondence_estimation_->setTargetNormals(target_blob);
  // Correspondence Rejectors need a binary blob
  for (std::size_t i = 0; i < correspondence_rejectors_.size(); ++i) {
    registration::CorrespondenceRejector::Ptr& rej = correspondence_rejectors_[i];
    if (rej->requiresTargetPoints())
      rej->setTargetPoints(target_blob);
    if (rej->requiresTargetNormals() && target_has_normals_)
      rej->setTargetNormals(target_blob);
  }

  convergence_criteria_->setMaximumIterations(max_iterations_);
  convergence_criteria_->setRelativeMSE(euclidean_fitness_epsilon_);
  convergence_criteria_->setTranslationThreshold(transformation_epsilon_);
  if (transformation_rotation_epsilon_ > 0)
    convergence_criteria_->setRotationThreshold(transformation_rotation_epsilon_);

  // Repeat until convergence
  do {
    // Get blob data if needed
    PCLPointCloud2::Ptr input_transformed_blob;
    if (need_source_blob_) {
      input_transformed_blob.reset(new PCLPointCloud2);
      toPCLPointCloud2(*input_transformed, *input_transformed_blob);
    }
    // Save the previously estimated transformation
    previous_transformation_ = transformation_;

    // Set the source each iteration, to ensure the dirty flag is updated
    correspondence_estimation_->setInputSource(input_transformed);
    if (correspondence_estimation_->requiresSourceNormals())
      correspondence_estimation_->setSourceNormals(input_transformed_blob);
    // Estimate correspondences
    if (use_reciprocal_correspondence_)
      correspondence_estimation_->determineReciprocalCorrespondences(
          *correspondences_, corr_dist_threshold_);
    else
      correspondence_estimation_->determineCorrespondences(*correspondences_,
                                                           corr_dist_threshold_);

    // if (correspondence_rejectors_.empty ())
    CorrespondencesPtr temp_correspondences(new Correspondences(*correspondences_));
    for (std::size_t i = 0; i < correspondence_rejectors_.size(); ++i) {
      registration::CorrespondenceRejector::Ptr& rej = correspondence_rejectors_[i];
      PCL_DEBUG("Applying a correspondence rejector method: %s.\n",
                rej->getClassName().c_str());
      if (rej->requiresSourcePoints())
        rej->setSourcePoints(input_transformed_blob);
      if (rej->requiresSourceNormals() && source_has_normals_)
        rej->setSourceNormals(input_transformed_blob);
      rej->setInputCorrespondences(temp_correspondences);
      rej->getCorrespondences(*correspondences_);
      // Modify input for the next iteration
      if (i < correspondence_rejectors_.size() - 1)
        *temp_correspondences = *correspondences_;
    }

    // Check whether we have enough correspondences
    if (correspondences_->size() < min_number_correspondences_) {
      PCL_ERROR("[pcl::%s::computeTransformation] Not enough correspondences found. "
                "Relax your threshold parameters.\n",
                getClassName().c_str());
      convergence_criteria_->setConvergenceState(
          pcl::registration::DefaultConvergenceCriteria<
              Scalar>::CONVERGENCE_CRITERIA_NO_CORRESPONDENCES);
      converged_ = false;
      break;
    }

    // Estimate the transform
    transformation_estimation_->estimateRigidTransformation(
        *input_transformed, *target_, *correspondences_, transformation_);

    // Transform the data
    transformCloud(*input_transformed, *input_transformed, transformation_);

    // Obtain the final transformation
    final_transformation_ = transformation_ * final_transformation_;

    ++nr_iterations_;

    // Update the visualization of icp convergence
    if (update_visualizer_ != nullptr) {
      pcl::Indices source_indices_good, target_indices_good;
      for (const Correspondence& corr : *correspondences_) {
        source_indices_good.emplace_back(corr.index_query);
        target_indices_good.emplace_back(corr.index_match);
      }
      update_visualizer_(
          *input_transformed, source_indices_good, *target_, target_indices_good);
    }

    converged_ = static_cast<bool>((*convergence_criteria_));
  } while (convergence_criteria_->getConvergenceState() ==
           pcl::registration::DefaultConvergenceCriteria<
               Scalar>::CONVERGENCE_CRITERIA_NOT_CONVERGED);

  // Transform the input cloud using the final transformation
  PCL_DEBUG("Transformation "
            "is:\n\t%5f\t%5f\t%5f\t%5f\n\t%5f\t%5f\t%5f\t%5f\n\t%5f\t%5f\t%5f\t%5f\n\t%"
            "5f\t%5f\t%5f\t%5f\n",
            final_transformation_(0, 0),
            final_transformation_(0, 1),
            final_transformation_(0, 2),
            final_transformation_(0, 3),
            final_transformation_(1, 0),
            final_transformation_(1, 1),
            final_transformation_(1, 2),
            final_transformation_(1, 3),
            final_transformation_(2, 0),
            final_transformation_(2, 1),
            final_transformation_(2, 2),
            final_transformation_(2, 3),
            final_transformation_(3, 0),
            final_transformation_(3, 1),
            final_transformation_(3, 2),
            final_transformation_(3, 3));

  // Copy all the values
  output = *input_;
  // Transform the XYZ + normals
  transformCloud(*input_, output, final_transformation_);
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
IterativeClosestPoint<PointSource, PointTarget, Scalar>::determineRequiredBlobData()
{
  need_source_blob_ = false;
  need_target_blob_ = false;
  // Check estimator
  need_source_blob_ |= correspondence_estimation_->requiresSourceNormals();
  need_target_blob_ |= correspondence_estimation_->requiresTargetNormals();
  // Add warnings if necessary
  if (correspondence_estimation_->requiresSourceNormals() && !source_has_normals_) {
    PCL_WARN("[pcl::%s::determineRequiredBlobData] Estimator expects source normals, "
             "but we can't provide them.\n",
             getClassName().c_str());
  }
  if (correspondence_estimation_->requiresTargetNormals() && !target_has_normals_) {
    PCL_WARN("[pcl::%s::determineRequiredBlobData] Estimator expects target normals, "
             "but we can't provide them.\n",
             getClassName().c_str());
  }
  // Check rejectors
  for (std::size_t i = 0; i < correspondence_rejectors_.size(); i++) {
    registration::CorrespondenceRejector::Ptr& rej = correspondence_rejectors_[i];
    need_source_blob_ |= rej->requiresSourcePoints();
    need_source_blob_ |= rej->requiresSourceNormals();
    need_target_blob_ |= rej->requiresTargetPoints();
    need_target_blob_ |= rej->requiresTargetNormals();
    if (rej->requiresSourceNormals() && !source_has_normals_) {
      PCL_WARN("[pcl::%s::determineRequiredBlobData] Rejector %s expects source "
               "normals, but we can't provide them.\n",
               getClassName().c_str(),
               rej->getClassName().c_str());
    }
    if (rej->requiresTargetNormals() && !target_has_normals_) {
      PCL_WARN("[pcl::%s::determineRequiredBlobData] Rejector %s expects target "
               "normals, but we can't provide them.\n",
               getClassName().c_str(),
               rej->getClassName().c_str());
    }
  }
}

template <typename PointSource, typename PointTarget, typename Scalar>
void
IterativeClosestPointWithNormals<PointSource, PointTarget, Scalar>::transformCloud(
    const PointCloudSource& input, PointCloudSource& output, const Matrix4& transform)
{
  pcl::transformPointCloudWithNormals(input, output, transform);
}
// NOLINTEND(readability-container-data-pointer)

} // namespace pcl

#endif /* PCL_REGISTRATION_IMPL_ICP_HPP_ */
