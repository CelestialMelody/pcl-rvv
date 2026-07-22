/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *
 *  Copyright (c) 2026
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
 */

#pragma once

#include <pcl/field_traits.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace pcl {
namespace rvv {

template <typename T>
using RVVFieldScalar = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename PointT,
          typename Field,
          bool HasField = pcl::traits::has_field<PointT, Field>::value>
struct RVVFloatFieldLayout : std::false_type {};

template <typename PointT, typename Field>
struct RVVFloatFieldLayout<PointT, Field, true>
: std::bool_constant<
      std::is_same_v<typename pcl::traits::datatype<PointT, Field>::decomposed::type,
                     float> &&
      pcl::traits::datatype<PointT, Field>::decomposed::value == 1> {};

template <typename PointT, bool HasXYZ = pcl::traits::has_xyz<PointT>::value>
struct RVVXYZFloatLayout : std::false_type {};

template <typename PointT>
struct RVVXYZFloatLayout<PointT, true>
: std::bool_constant<RVVFloatFieldLayout<PointT, pcl::fields::x>::value &&
                     RVVFloatFieldLayout<PointT, pcl::fields::y>::value &&
                     RVVFloatFieldLayout<PointT, pcl::fields::z>::value> {
  static constexpr std::size_t kX = pcl::traits::offset<PointT, pcl::fields::x>::value;
  static constexpr std::size_t kY = pcl::traits::offset<PointT, pcl::fields::y>::value;
  static constexpr std::size_t kZ = pcl::traits::offset<PointT, pcl::fields::z>::value;
};

template <typename PointT,
          bool HasXYZ = pcl::traits::has_xyz<PointT>::value,
          bool HasNormal = pcl::traits::has_normal<PointT>::value>
struct RVVXYZNormalFloatLayout : std::false_type {};

template <typename PointT>
struct RVVXYZNormalFloatLayout<PointT, true, true> {
  using Pod = typename pcl::traits::POD<PointT>::type;

  static constexpr std::size_t kX = pcl::traits::offset<PointT, pcl::fields::x>::value;
  static constexpr std::size_t kY = pcl::traits::offset<PointT, pcl::fields::y>::value;
  static constexpr std::size_t kZ = pcl::traits::offset<PointT, pcl::fields::z>::value;
  static constexpr std::size_t kNX =
      pcl::traits::offset<PointT, pcl::fields::normal_x>::value;
  static constexpr std::size_t kNY =
      pcl::traits::offset<PointT, pcl::fields::normal_y>::value;
  static constexpr std::size_t kNZ =
      pcl::traits::offset<PointT, pcl::fields::normal_z>::value;

  static constexpr bool value =
      RVVXYZFloatLayout<PointT>::value &&
      RVVFloatFieldLayout<PointT, pcl::fields::normal_x>::value &&
      RVVFloatFieldLayout<PointT, pcl::fields::normal_y>::value &&
      RVVFloatFieldLayout<PointT, pcl::fields::normal_z>::value &&
      std::is_standard_layout_v<Pod> && sizeof(PointT) == sizeof(Pod) &&
      sizeof(PointT) % alignof(float) == 0 && kX % alignof(float) == 0 &&
      kY % alignof(float) == 0 && kZ % alignof(float) == 0 &&
      kNX % alignof(float) == 0 && kNY % alignof(float) == 0 &&
      kNZ % alignof(float) == 0;
};

namespace detail {

template <typename PointT, typename = void>
struct RVVXYZMemberFloatLayout : std::false_type {};

template <typename PointT>
struct RVVXYZMemberFloatLayout<
    PointT,
    std::void_t<decltype(std::declval<PointT>().x),
                decltype(std::declval<PointT>().y),
                decltype(std::declval<PointT>().z)>>
: std::bool_constant<
      std::is_standard_layout_v<PointT> &&
      std::is_same_v<RVVFieldScalar<decltype(std::declval<PointT>().x)>, float> &&
      std::is_same_v<RVVFieldScalar<decltype(std::declval<PointT>().y)>, float> &&
      std::is_same_v<RVVFieldScalar<decltype(std::declval<PointT>().z)>, float>> {};

} // namespace detail

template <typename PointT>
inline constexpr bool kRVVXYZPointCompatible =
    detail::RVVXYZMemberFloatLayout<PointT>::value;

template <typename PointT>
inline constexpr bool kRVVXYZNormalPointCompatible =
    RVVXYZNormalFloatLayout<PointT>::value;

template <typename PointT>
constexpr std::size_t
rvvMaxU32ByteOffsetElements()
{
  return static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() /
                                  sizeof(PointT));
}

} // namespace rvv
} // namespace pcl
