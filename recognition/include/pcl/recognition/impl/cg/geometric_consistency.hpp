/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2012, Willow Garage, Inc.
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
 *   * Neither the name of Willow Garage, Inc. nor the names of its
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

#ifndef PCL_RECOGNITION_GEOMETRIC_CONSISTENCY_IMPL_H_
#define PCL_RECOGNITION_GEOMETRIC_CONSISTENCY_IMPL_H_

#include <pcl/recognition/cg/geometric_consistency.h>
#if defined(__RVV10__)
#include <pcl/rvv_point_load.h>
#endif
#include <pcl/registration/correspondence_types.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/common/io.h>

#include <cstdint>
#include <limits>

namespace pcl
{
namespace detail
{
#if defined(__RVV10__)
template <typename PointModelT, typename PointSceneT>
inline bool
geometricConsistencyPairwiseConsistencyRVV (const pcl::PointCloud<PointModelT>& input,
                                            const pcl::PointCloud<PointSceneT>& scene,
                                            const pcl::Correspondences& model_scene_corrs,
                                            const std::vector<int>& consensus_set,
                                            const int candidate_corr_index,
                                            const double gc_size,
                                            bool& is_a_good_candidate)
{
  using ModelLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointModelT>;
  using SceneLayout = pcl::rvv::RVVXYZAoSFloatLayout<PointSceneT>;
  if constexpr (!(ModelLayout::value && SceneLayout::value))
  {
    return (false);
  }
  else
  {
    is_a_good_candidate = false;
    if (candidate_corr_index < 0 ||
        static_cast<std::size_t> (candidate_corr_index) >= model_scene_corrs.size ())
      return (false);

    if (input.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointModelT> () ||
        scene.size () > pcl::rvv::rvvMaxU32ByteOffsetElements<PointSceneT> () ||
        model_scene_corrs.size () >
            static_cast<std::size_t> (std::numeric_limits<std::uint32_t>::max ()) ||
        consensus_set.size () >
            static_cast<std::size_t> (std::numeric_limits<std::uint32_t>::max ()))
      return (false);

    const auto& candidate_corr = model_scene_corrs[candidate_corr_index];
    if (candidate_corr.index_query < 0 || candidate_corr.index_match < 0 ||
        static_cast<std::size_t> (candidate_corr.index_query) >= input.size () ||
        static_cast<std::size_t> (candidate_corr.index_match) >= scene.size ())
      return (false);

    const auto& candidate_model = input.points[static_cast<std::size_t> (candidate_corr.index_query)].
        getVector3fMap ();
    const auto& candidate_scene = scene.points[static_cast<std::size_t> (candidate_corr.index_match)].
        getVector3fMap ();
    const float cand_model_x = candidate_model.x ();
    const float cand_model_y = candidate_model.y ();
    const float cand_model_z = candidate_model.z ();
    const float cand_scene_x = candidate_scene.x ();
    const float cand_scene_y = candidate_scene.y ();
    const float cand_scene_z = candidate_scene.z ();
    const float gc_size_f = static_cast<float> (gc_size);

    const std::size_t max_vl = __riscv_vsetvlmax_e32m2 ();
    std::vector<std::uint32_t> model_indices (max_vl);
    std::vector<std::uint32_t> scene_indices (max_vl);
    const auto* model_base = reinterpret_cast<const std::uint8_t*> (input.points.data ());
    const auto* scene_base = reinterpret_cast<const std::uint8_t*> (scene.points.data ());

    for (std::size_t offset = 0; offset < consensus_set.size ();)
    {
      const std::size_t vl = __riscv_vsetvl_e32m2 (consensus_set.size () - offset);
      for (std::size_t lane = 0; lane < vl; ++lane)
      {
        const int corr_index = consensus_set[offset + lane];
        if (corr_index < 0 ||
            static_cast<std::size_t> (corr_index) >= model_scene_corrs.size ())
          return (false);

        const auto& corr = model_scene_corrs[static_cast<std::size_t> (corr_index)];
        if (corr.index_query < 0 || corr.index_match < 0 ||
            static_cast<std::size_t> (corr.index_query) >= input.size () ||
            static_cast<std::size_t> (corr.index_match) >= scene.size ())
          return (false);

        model_indices[lane] = static_cast<std::uint32_t> (corr.index_query);
        scene_indices[lane] = static_cast<std::uint32_t> (corr.index_match);
      }

      const auto model_index_v =
          __riscv_vle32_v_u32m2 (model_indices.data (), vl);
      const auto scene_index_v =
          __riscv_vle32_v_u32m2 (scene_indices.data (), vl);
      const auto model_off =
          pcl::rvv_load::byte_offsets_u32m2<PointModelT> (model_index_v, vl);
      const auto scene_off =
          pcl::rvv_load::byte_offsets_u32m2<PointSceneT> (scene_index_v, vl);

      vfloat32m2_t model_x_k, model_y_k, model_z_k;
      vfloat32m2_t scene_x_k, scene_y_k, scene_z_k;
      pcl::rvv_load::indexed_load3_f32m2<PointModelT, ModelLayout::kX, ModelLayout::kY, ModelLayout::kZ> (
          model_base, model_off, vl, model_x_k, model_y_k, model_z_k);
      pcl::rvv_load::indexed_load3_f32m2<PointSceneT, SceneLayout::kX, SceneLayout::kY, SceneLayout::kZ> (
          scene_base, scene_off, vl, scene_x_k, scene_y_k, scene_z_k);

      const vfloat32m2_t cand_model_x_v = __riscv_vfmv_v_f_f32m2 (cand_model_x, vl);
      const vfloat32m2_t cand_model_y_v = __riscv_vfmv_v_f_f32m2 (cand_model_y, vl);
      const vfloat32m2_t cand_model_z_v = __riscv_vfmv_v_f_f32m2 (cand_model_z, vl);
      const vfloat32m2_t cand_scene_x_v = __riscv_vfmv_v_f_f32m2 (cand_scene_x, vl);
      const vfloat32m2_t cand_scene_y_v = __riscv_vfmv_v_f_f32m2 (cand_scene_y, vl);
      const vfloat32m2_t cand_scene_z_v = __riscv_vfmv_v_f_f32m2 (cand_scene_z, vl);

      const vfloat32m2_t model_dx =
          __riscv_vfsub_vv_f32m2 (model_x_k, cand_model_x_v, vl);
      const vfloat32m2_t model_dy =
          __riscv_vfsub_vv_f32m2 (model_y_k, cand_model_y_v, vl);
      const vfloat32m2_t model_dz =
          __riscv_vfsub_vv_f32m2 (model_z_k, cand_model_z_v, vl);
      const vfloat32m2_t scene_dx =
          __riscv_vfsub_vv_f32m2 (scene_x_k, cand_scene_x_v, vl);
      const vfloat32m2_t scene_dy =
          __riscv_vfsub_vv_f32m2 (scene_y_k, cand_scene_y_v, vl);
      const vfloat32m2_t scene_dz =
          __riscv_vfsub_vv_f32m2 (scene_z_k, cand_scene_z_v, vl);

      const vfloat32m2_t model_norm = __riscv_vfsqrt_v_f32m2 (
          __riscv_vfadd_vv_f32m2 (
              __riscv_vfadd_vv_f32m2 (
                  __riscv_vfmul_vv_f32m2 (model_dx, model_dx, vl),
                  __riscv_vfmul_vv_f32m2 (model_dy, model_dy, vl),
                  vl),
              __riscv_vfmul_vv_f32m2 (model_dz, model_dz, vl),
              vl),
          vl);
      const vfloat32m2_t scene_norm = __riscv_vfsqrt_v_f32m2 (
          __riscv_vfadd_vv_f32m2 (
              __riscv_vfadd_vv_f32m2 (
                  __riscv_vfmul_vv_f32m2 (scene_dx, scene_dx, vl),
                  __riscv_vfmul_vv_f32m2 (scene_dy, scene_dy, vl),
                  vl),
              __riscv_vfmul_vv_f32m2 (scene_dz, scene_dz, vl),
              vl),
          vl);

      const vfloat32m2_t diff =
          __riscv_vfabs_v_f32m2 (__riscv_vfsub_vv_f32m2 (scene_norm, model_norm, vl), vl);
      const vbool16_t keep = __riscv_vmfle_vf_f32m2_b16 (diff, gc_size_f, vl);
      if (__riscv_vcpop_m_b16 (keep, vl) != vl)
        return (true);

      offset += vl;
    }

    is_a_good_candidate = true;
    return (true);
  }
}
#endif
} // namespace detail
} // namespace pcl

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool
gcCorrespSorter (pcl::Correspondence i, pcl::Correspondence j)
{
  return (i.distance < j.distance);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointModelT, typename PointSceneT> void
pcl::GeometricConsistencyGrouping<PointModelT, PointSceneT>::clusterCorrespondences (std::vector<Correspondences> &model_instances)
{
  model_instances.clear ();
  found_transformations_.clear ();

  if (!model_scene_corrs_)
  {
    PCL_ERROR(
      "[pcl::GeometricConsistencyGrouping::clusterCorrespondences()] Error! Correspondences not set, please set them before calling again this function.\n");
    return;
  }

  CorrespondencesPtr sorted_corrs (new Correspondences (*model_scene_corrs_));

  std::sort (sorted_corrs->begin (), sorted_corrs->end (), gcCorrespSorter);

  model_scene_corrs_ = sorted_corrs;
  PCL_DEBUG_STREAM("[pcl::GeometricConsistencyGrouping::clusterCorrespondences] Five best correspondences: ");
  for(std::size_t i=0; i<std::min<std::size_t>(model_scene_corrs_->size(), 5); ++i)
    PCL_DEBUG_STREAM("[" << (*input_)[(*model_scene_corrs_)[i].index_query] << " " << (*scene_)[(*model_scene_corrs_)[i].index_match] << " " << (*model_scene_corrs_)[i].distance << "] ");
  PCL_DEBUG_STREAM(std::endl);

  std::vector<int> consensus_set;
  std::vector<bool> taken_corresps (model_scene_corrs_->size (), false);

  Eigen::Vector3f dist_ref, dist_trg;

  //temp copy of scene cloud with the type cast to ModelT in order to use Ransac
  PointCloudPtr temp_scene_cloud_ptr (new PointCloud ());
  pcl::copyPointCloud (*scene_, *temp_scene_cloud_ptr);

  pcl::registration::CorrespondenceRejectorSampleConsensus<PointModelT> corr_rejector;
  corr_rejector.setMaximumIterations (10000);
  corr_rejector.setInlierThreshold (gc_size_);
  corr_rejector.setInputSource(input_);
  corr_rejector.setInputTarget (temp_scene_cloud_ptr);

  for (std::size_t i = 0; i < model_scene_corrs_->size (); ++i)
  {
    if (taken_corresps[i])
      continue;

    consensus_set.clear ();
    consensus_set.push_back (static_cast<int> (i));
    
    for (std::size_t j = 0; j < model_scene_corrs_->size (); ++j)
    {
      if ( j != i &&  !taken_corresps[j])
      {
        //Let's check if j fits into the current consensus set
        bool is_a_good_candidate = true;
#if defined(__RVV10__)
        if (!pcl::detail::geometricConsistencyPairwiseConsistencyRVV<PointModelT, PointSceneT> (
                *input_, *scene_, *model_scene_corrs_, consensus_set, static_cast<int> (j),
                gc_size_, is_a_good_candidate))
#endif
        {
          is_a_good_candidate = true;
          for (const int &k : consensus_set)
          {
            int scene_index_k = model_scene_corrs_->at (k).index_match;
            int model_index_k = model_scene_corrs_->at (k).index_query;
            int scene_index_j = model_scene_corrs_->at (j).index_match;
            int model_index_j = model_scene_corrs_->at (j).index_query;

            const Eigen::Vector3f& scene_point_k = scene_->at (scene_index_k).getVector3fMap ();
            const Eigen::Vector3f& model_point_k = input_->at (model_index_k).getVector3fMap ();
            const Eigen::Vector3f& scene_point_j = scene_->at (scene_index_j).getVector3fMap ();
            const Eigen::Vector3f& model_point_j = input_->at (model_index_j).getVector3fMap ();

            dist_ref = scene_point_k - scene_point_j;
            dist_trg = model_point_k - model_point_j;

            double distance = std::abs (dist_ref.norm () - dist_trg.norm ());

            if (distance > gc_size_)
            {
              is_a_good_candidate = false;
              break;
            }
          }
        }

        if (is_a_good_candidate)
          consensus_set.push_back (static_cast<int> (j));
      }
    }
    
    if (static_cast<int> (consensus_set.size ()) > gc_threshold_)
    {
      Correspondences temp_corrs, filtered_corrs;
      for (const int &j : consensus_set)
      {
        temp_corrs.push_back (model_scene_corrs_->at (j));
        taken_corresps[ j ] = true;
      }
      //ransac filtering
      corr_rejector.getRemainingCorrespondences (temp_corrs, filtered_corrs);
      //save transformations for recognize
      found_transformations_.push_back (corr_rejector.getBestTransformation ());

      model_instances.push_back (filtered_corrs);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointModelT, typename PointSceneT> bool
pcl::GeometricConsistencyGrouping<PointModelT, PointSceneT>::recognize (
    std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > &transformations)
{
  std::vector<pcl::Correspondences> model_instances;
  return (this->recognize (transformations, model_instances));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename PointModelT, typename PointSceneT> bool
pcl::GeometricConsistencyGrouping<PointModelT, PointSceneT>::recognize (
    std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > &transformations, std::vector<pcl::Correspondences> &clustered_corrs)
{
  transformations.clear ();
  if (!this->initCompute ())
  {
    PCL_ERROR(
      "[pcl::GeometricConsistencyGrouping::recognize()] Error! Model cloud or Scene cloud not set, please set them before calling again this function.\n");
    return (false);
  }

  clusterCorrespondences (clustered_corrs);

  transformations = found_transformations_;

  this->deinitCompute ();
  return (true);
}

#define PCL_INSTANTIATE_GeometricConsistencyGrouping(T,ST) template class PCL_EXPORTS pcl::GeometricConsistencyGrouping<T,ST>;

#endif // PCL_RECOGNITION_GEOMETRIC_CONSISTENCY_IMPL_H_
