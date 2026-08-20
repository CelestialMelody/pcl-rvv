/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010, Willow Garage, Inc.
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
 */

#ifndef PCL_SURFACE_IMPL_MARCHING_CUBES_RBF_H_
#define PCL_SURFACE_IMPL_MARCHING_CUBES_RBF_H_

#include <pcl/surface/marching_cubes_rbf.h>
#include <pcl/point_types.h>
#include <pcl/rvv_point_traits.h>

#include <cmath>
#include <type_traits>
#include <vector>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace pcl::detail
{
  struct MarchingCubesRBFCenters
  {
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> z;
  };

  inline double
  marchingCubesRBFCubicKernel (const double cx, const double cy, const double cz,
                               const double px, const double py, const double pz)
  {
    const double dx = px - cx;
    const double dy = py - cy;
    const double dz = pz - cz;
    const double r2 = dx * dx + dy * dy + dz * dz;
    return r2 * std::sqrt (r2);
  }

  template <typename PointNT> void
  marchingCubesRBFBuildCenters (const pcl::PointCloud<PointNT> &input,
                                const double off_surface_epsilon,
                                MarchingCubesRBFCenters &centers)
  {
    const auto n = input.size ();
    centers.x.resize (2 * n);
    centers.y.resize (2 * n);
    centers.z.resize (2 * n);

    for (std::size_t i = 0; i < n; ++i)
    {
      const Eigen::Vector3d point =
          Eigen::Vector3f (input[i].getVector3fMap ()).cast<double> ();
      const Eigen::Vector3d normal =
          Eigen::Vector3f (input[i].getNormalVector3fMap ()).cast<double> ();
      centers.x[i] = point.x ();
      centers.y[i] = point.y ();
      centers.z[i] = point.z ();
      centers.x[i + n] = point.x () + normal.x () * off_surface_epsilon;
      centers.y[i + n] = point.y () + normal.y () * off_surface_epsilon;
      centers.z[i + n] = point.z () + normal.z () * off_surface_epsilon;
    }
  }

  inline void
  marchingCubesRBFFillMatrixStandard (const MarchingCubesRBFCenters &centers,
                                      Eigen::MatrixXd &matrix)
  {
    const auto count = static_cast<Eigen::Index> (centers.x.size ());
    matrix.resize (count, count);
    for (Eigen::Index col = 0; col < count; ++col)
    {
      const double cx = centers.x[static_cast<std::size_t> (col)];
      const double cy = centers.y[static_cast<std::size_t> (col)];
      const double cz = centers.z[static_cast<std::size_t> (col)];
      for (Eigen::Index row = 0; row < count; ++row)
        matrix (row, col) =
            marchingCubesRBFCubicKernel (cx, cy, cz,
                                         centers.x[static_cast<std::size_t> (row)],
                                         centers.y[static_cast<std::size_t> (row)],
                                         centers.z[static_cast<std::size_t> (row)]);
    }
  }

  inline Eigen::MatrixXd
  marchingCubesRBFMakeRhs (const std::size_t point_count,
                           const double off_surface_epsilon)
  {
    Eigen::MatrixXd rhs (static_cast<Eigen::Index> (2 * point_count), 1);
    for (std::size_t row = 0; row < 2 * point_count; ++row)
      rhs (static_cast<Eigen::Index> (row), 0) =
          row >= point_count ? off_surface_epsilon : 0.0;
    return rhs;
  }

  inline std::vector<double>
  marchingCubesRBFMakeWeights (const Eigen::MatrixXd &solution)
  {
    std::vector<double> weights (static_cast<std::size_t> (solution.rows ()));
    for (Eigen::Index i = 0; i < solution.rows (); ++i)
      weights[static_cast<std::size_t> (i)] = solution (i, 0);
    return weights;
  }

  inline void
  marchingCubesRBFEvaluateGridStandard (const MarchingCubesRBFCenters &centers,
                                        const std::vector<double> &weights,
                                        std::vector<float> &grid,
                                        const int res_x,
                                        const int res_y,
                                        const int res_z,
                                        const Eigen::Array3f &size_voxel,
                                        const Eigen::Array3f &lower_boundary)
  {
    for (int x = 0; x < res_x; ++x)
      for (int y = 0; y < res_y; ++y)
        for (int z = 0; z < res_z; ++z)
        {
          const Eigen::Vector3d point =
              (size_voxel * Eigen::Array3f (x, y, z) + lower_boundary).matrix ().cast<double> ();

          double f = 0.0;
          for (std::size_t i = 0; i < centers.x.size (); ++i)
            f += weights[i] *
                 marchingCubesRBFCubicKernel (centers.x[i], centers.y[i], centers.z[i],
                                              point.x (), point.y (), point.z ());

          grid[x * res_y * res_z + y * res_z + z] = static_cast<float> (f);
        }
  }

  template <typename PointNT> void
  marchingCubesRBFVoxelizeDataStandard (const pcl::PointCloud<PointNT> &input,
                                        const double off_surface_epsilon,
                                        std::vector<float> &grid,
                                        const int res_x,
                                        const int res_y,
                                        const int res_z,
                                        const Eigen::Array3f &size_voxel,
                                        const Eigen::Array3f &lower_boundary)
  {
    MarchingCubesRBFCenters centers;
    marchingCubesRBFBuildCenters (input, off_surface_epsilon, centers);

    Eigen::MatrixXd matrix;
    marchingCubesRBFFillMatrixStandard (centers, matrix);
    const Eigen::MatrixXd rhs =
        marchingCubesRBFMakeRhs (input.size (), off_surface_epsilon);
    const std::vector<double> weights =
        marchingCubesRBFMakeWeights (matrix.fullPivLu ().solve (rhs));

    marchingCubesRBFEvaluateGridStandard (centers, weights, grid, res_x, res_y, res_z,
                                          size_voxel, lower_boundary);
  }

#if defined(__RVV10__)
  inline void
  marchingCubesRBFFillMatrixRVV (const MarchingCubesRBFCenters &centers,
                                 Eigen::MatrixXd &matrix)
  {
    const auto count = centers.x.size ();
    matrix.resize (static_cast<Eigen::Index> (count), static_cast<Eigen::Index> (count));
    for (std::size_t col = 0; col < count; ++col)
    {
      const double cx = centers.x[col];
      const double cy = centers.y[col];
      const double cz = centers.z[col];
      std::size_t row = 0;
      while (row < count)
      {
        const std::size_t vl = __riscv_vsetvl_e64m1 (count - row);
        const vfloat64m1_t px = __riscv_vle64_v_f64m1 (centers.x.data () + row, vl);
        const vfloat64m1_t py = __riscv_vle64_v_f64m1 (centers.y.data () + row, vl);
        const vfloat64m1_t pz = __riscv_vle64_v_f64m1 (centers.z.data () + row, vl);
        const vfloat64m1_t dx = __riscv_vfsub_vf_f64m1 (px, cx, vl);
        const vfloat64m1_t dy = __riscv_vfsub_vf_f64m1 (py, cy, vl);
        const vfloat64m1_t dz = __riscv_vfsub_vf_f64m1 (pz, cz, vl);
        vfloat64m1_t r2 = __riscv_vfmul_vv_f64m1 (dx, dx, vl);
        r2 = __riscv_vfmacc_vv_f64m1 (r2, dy, dy, vl);
        r2 = __riscv_vfmacc_vv_f64m1 (r2, dz, dz, vl);
        const vfloat64m1_t out = __riscv_vfmul_vv_f64m1 (r2, __riscv_vfsqrt_v_f64m1 (r2, vl), vl);
        __riscv_vse64_v_f64m1 (&matrix (static_cast<Eigen::Index> (row),
                                        static_cast<Eigen::Index> (col)),
                               out, vl);
        row += vl;
      }
    }
  }

  inline void
  marchingCubesRBFEvaluateGridRVV (const MarchingCubesRBFCenters &centers,
                                   const std::vector<double> &weights,
                                   std::vector<float> &grid,
                                   const int res_x,
                                   const int res_y,
                                   const int res_z,
                                   const Eigen::Array3f &size_voxel,
                                   const Eigen::Array3f &lower_boundary)
  {
    for (int x = 0; x < res_x; ++x)
      for (int y = 0; y < res_y; ++y)
        for (int z = 0; z < res_z; ++z)
        {
          const Eigen::Vector3d point =
              (size_voxel * Eigen::Array3f (x, y, z) + lower_boundary).matrix ().cast<double> ();

          double f = 0.0;
          std::size_t i = 0;
          while (i < centers.x.size ())
          {
            const std::size_t vl = __riscv_vsetvl_e64m1 (centers.x.size () - i);
            const vfloat64m1_t cx = __riscv_vle64_v_f64m1 (centers.x.data () + i, vl);
            const vfloat64m1_t cy = __riscv_vle64_v_f64m1 (centers.y.data () + i, vl);
            const vfloat64m1_t cz = __riscv_vle64_v_f64m1 (centers.z.data () + i, vl);
            const vfloat64m1_t weight = __riscv_vle64_v_f64m1 (weights.data () + i, vl);
            const vfloat64m1_t dx = __riscv_vfsub_vf_f64m1 (cx, point.x (), vl);
            const vfloat64m1_t dy = __riscv_vfsub_vf_f64m1 (cy, point.y (), vl);
            const vfloat64m1_t dz = __riscv_vfsub_vf_f64m1 (cz, point.z (), vl);
            vfloat64m1_t r2 = __riscv_vfmul_vv_f64m1 (dx, dx, vl);
            r2 = __riscv_vfmacc_vv_f64m1 (r2, dy, dy, vl);
            r2 = __riscv_vfmacc_vv_f64m1 (r2, dz, dz, vl);
            const vfloat64m1_t weighted =
                __riscv_vfmul_vv_f64m1 (weight,
                                        __riscv_vfmul_vv_f64m1 (r2,
                                                               __riscv_vfsqrt_v_f64m1 (r2, vl),
                                                               vl),
                                        vl);
            const vfloat64m1_t zero = __riscv_vfmv_v_f_f64m1 (0.0, vl);
            const vfloat64m1_t reduced =
                __riscv_vfredusum_vs_f64m1_f64m1 (weighted, zero, vl);
            f += __riscv_vfmv_f_s_f64m1_f64 (reduced);
            i += vl;
          }

          grid[x * res_y * res_z + y * res_z + z] = static_cast<float> (f);
        }
  }

  template <typename PointNT> bool
  marchingCubesRBFVoxelizeDataRVV (const pcl::PointCloud<PointNT> &input,
                                   const double off_surface_epsilon,
                                   std::vector<float> &grid,
                                   const int res_x,
                                   const int res_y,
                                   const int res_z,
                                   const Eigen::Array3f &size_voxel,
                                    const Eigen::Array3f &lower_boundary)
  {
    if constexpr (!pcl::rvv::RVVXYZNormalFloatLayout<PointNT>::value)
      return false;

    if (input.size () < 16)
      return false;

    MarchingCubesRBFCenters centers;
    marchingCubesRBFBuildCenters (input, off_surface_epsilon, centers);

    Eigen::MatrixXd matrix;
    marchingCubesRBFFillMatrixRVV (centers, matrix);
    const Eigen::MatrixXd rhs =
        marchingCubesRBFMakeRhs (input.size (), off_surface_epsilon);
    const std::vector<double> weights =
        marchingCubesRBFMakeWeights (matrix.fullPivLu ().solve (rhs));

    marchingCubesRBFEvaluateGridRVV (centers, weights, grid, res_x, res_y, res_z,
                                     size_voxel, lower_boundary);
    return true;
  }
#endif
} // namespace pcl::detail

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointNT>
pcl::MarchingCubesRBF<PointNT>::~MarchingCubesRBF () = default;

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointNT> void
pcl::MarchingCubesRBF<PointNT>::voxelizeData ()
{
#if defined(__RVV10__)
  // Only traits-proved xyz+normal AoS point types enter the RVV path.
  if (pcl::detail::marchingCubesRBFVoxelizeDataRVV<PointNT> (
          *input_, off_surface_epsilon_, grid_, res_x_, res_y_, res_z_,
          size_voxel_, lower_boundary_))
    return;
#endif

  pcl::detail::marchingCubesRBFVoxelizeDataStandard (
      *input_, off_surface_epsilon_, grid_, res_x_, res_y_, res_z_,
      size_voxel_, lower_boundary_);
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointNT> double
pcl::MarchingCubesRBF<PointNT>::kernel (Eigen::Vector3d c, Eigen::Vector3d x)
{
  double r = (x - c).norm ();
  return (r * r * r);
}

#define PCL_INSTANTIATE_MarchingCubesRBF(T) template class PCL_EXPORTS pcl::MarchingCubesRBF<T>;

#endif    // PCL_SURFACE_IMPL_MARCHING_CUBES_HOPPE_H_
