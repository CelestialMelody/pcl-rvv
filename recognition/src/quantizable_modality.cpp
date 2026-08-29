/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
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
 */

#include <pcl/recognition/quantizable_modality.h>
#include <cstddef>

#if defined(__RVV10__)
#include <riscv_vector.h>
#endif

namespace
{
#if defined(__RVV10__) && defined(PCL_RVV_QM_TEST_HOOK)
enum class QuantizedMapSpreadPathHook
{
  None = 0,
  Scalar = 1,
  Rvv = 2,
};

int&
quantizedMapSpreadLastTestHook ()
{
  static int last_hook = static_cast<int> (QuantizedMapSpreadPathHook::None);
  return (last_hook);
}

bool&
quantizedMapSpreadForceScalarTestHook ()
{
  static bool force_scalar = false;
  return (force_scalar);
}

extern "C" void
pcl_rvv_qm_reset_test_hook ()
{
  quantizedMapSpreadLastTestHook () =
      static_cast<int> (QuantizedMapSpreadPathHook::None);
  quantizedMapSpreadForceScalarTestHook () = false;
}

extern "C" int
pcl_rvv_qm_last_test_hook ()
{
  return (quantizedMapSpreadLastTestHook ());
}

extern "C" void
pcl_rvv_qm_set_force_scalar_test_hook (const int enabled)
{
  quantizedMapSpreadForceScalarTestHook () = enabled != 0;
}
#endif

void
recordQuantizedMapSpreadScalarPath ()
{
#if defined(__RVV10__) && defined(PCL_RVV_QM_TEST_HOOK)
  quantizedMapSpreadLastTestHook () =
      static_cast<int> (QuantizedMapSpreadPathHook::Scalar);
#endif
}

void
recordQuantizedMapSpreadRvvPath ()
{
#if defined(__RVV10__) && defined(PCL_RVV_QM_TEST_HOOK)
  quantizedMapSpreadLastTestHook () =
      static_cast<int> (QuantizedMapSpreadPathHook::Rvv);
#endif
}

bool
forceQuantizedMapSpreadScalarPath ()
{
#if defined(__RVV10__) && defined(PCL_RVV_QM_TEST_HOOK)
  return (quantizedMapSpreadForceScalarTestHook ());
#else
  return (false);
#endif
}

void
spreadQuantizedMapStd (const pcl::QuantizedMap & input_map,
                       pcl::QuantizedMap & output_map,
                       const std::size_t spreading_size)
{
  const std::size_t width = input_map.getWidth ();
  const std::size_t height = input_map.getHeight ();
  const std::size_t half_spreading_size = spreading_size / 2;

  pcl::QuantizedMap tmp_map (width, height);
  output_map.resize (width, height);

  for (std::size_t row_index = 0; row_index < height-spreading_size-1; ++row_index)
  {
    for (std::size_t col_index = 0; col_index < width-spreading_size-1; ++col_index)
    {
      unsigned char value = 0;
      const unsigned char * data_ptr = &(input_map (col_index, row_index));
      for (std::size_t spreading_index = 0; spreading_index < spreading_size; ++spreading_index, ++data_ptr)
      {
        value |= *data_ptr;
      }

      tmp_map (col_index + half_spreading_size, row_index) = value;
    }
  }

  for (std::size_t row_index = 0; row_index < height-spreading_size-1; ++row_index)
  {
    for (std::size_t col_index = 0; col_index < width-spreading_size-1; ++col_index)
    {
      unsigned char value = 0;
      const unsigned char * data_ptr = &(tmp_map (col_index, row_index));
      for (std::size_t spreading_index = 0; spreading_index < spreading_size; ++spreading_index, data_ptr += width)
      {
        value |= *data_ptr;
      }

      output_map (col_index, row_index + half_spreading_size) = value;
    }
  }
}

#if defined(__RVV10__)
bool
spreadQuantizedMapRVV (const pcl::QuantizedMap & input_map,
                       pcl::QuantizedMap & output_map,
                       const std::size_t spreading_size)
{
  if (spreading_size != 8 || forceQuantizedMapSpreadScalarPath ())
    return (false);

  const std::size_t width = input_map.getWidth ();
  const std::size_t height = input_map.getHeight ();
  if (width <= spreading_size + 1 || height <= spreading_size + 1)
    return (false);

  constexpr std::size_t spread = 8;
  constexpr std::size_t half_spread = spread / 2;
  pcl::QuantizedMap tmp_map (width, height);
  output_map.resize (width, height);

  const auto* input_data = input_map.getData ();
  auto* tmp_data = tmp_map.getData ();
  auto* output_data = output_map.getData ();
  const std::size_t active_width = width - spread - 1;
  const std::size_t active_height = height - spread - 1;

  for (std::size_t row_index = 0; row_index < active_height; ++row_index)
  {
    for (std::size_t col_index = 0; col_index < active_width;)
    {
      const std::size_t vl = __riscv_vsetvl_e8m2 (active_width - col_index);
      const auto* input_row = input_data + row_index * width + col_index;
      vuint8m2_t value = __riscv_vle8_v_u8m2 (input_row, vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (input_row + 1, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (input_row + 2, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (input_row + 3, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (input_row + 4, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (input_row + 5, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (input_row + 6, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (input_row + 7, vl), vl);
      __riscv_vse8_v_u8m2 (tmp_data + (col_index + half_spread) + row_index * width,
                            value,
                            vl);
      col_index += vl;
    }
  }

  for (std::size_t row_index = 0; row_index < active_height; ++row_index)
  {
    for (std::size_t col_index = 0; col_index < active_width;)
    {
      const std::size_t vl = __riscv_vsetvl_e8m2 (active_width - col_index);
      const auto* tmp_row = tmp_data + row_index * width + col_index;
      vuint8m2_t value = __riscv_vle8_v_u8m2 (tmp_row, vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (tmp_row + width, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (tmp_row + 2 * width, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (tmp_row + 3 * width, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (tmp_row + 4 * width, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (tmp_row + 5 * width, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (tmp_row + 6 * width, vl), vl);
      value = __riscv_vor_vv_u8m2 (value, __riscv_vle8_v_u8m2 (tmp_row + 7 * width, vl), vl);
      __riscv_vse8_v_u8m2 (output_data + col_index + (row_index + half_spread) * width,
                            value,
                            vl);
      col_index += vl;
    }
  }

  recordQuantizedMapSpreadRvvPath ();
  return (true);
}
#endif
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////
pcl::QuantizableModality::QuantizableModality () = default;

//////////////////////////////////////////////////////////////////////////////////////////////
pcl::QuantizableModality::~QuantizableModality () = default;

//////////////////////////////////////////////////////////////////////////////////////////////
pcl::QuantizedMap::QuantizedMap ()
  : data_ (0), width_ (0), height_ (0)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////
pcl::QuantizedMap::QuantizedMap (const QuantizedMap & copy_me)
  : data_ (0), width_ (copy_me.width_), height_ (copy_me.height_)
{
  data_.insert (data_.begin (), copy_me.data_.begin (), copy_me.data_.end ());
}

//////////////////////////////////////////////////////////////////////////////////////////////
pcl::QuantizedMap::QuantizedMap (const std::size_t width, const std::size_t height)
  : data_ (width*height), width_ (width), height_ (height)
{
}

//////////////////////////////////////////////////////////////////////////////////////////////
pcl::QuantizedMap::~QuantizedMap () = default;

//////////////////////////////////////////////////////////////////////////////////////////////
void
pcl::QuantizedMap::
resize (const std::size_t width, const std::size_t height)
{
  data_.resize (width*height);
  width_ = width;
  height_ = height;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void
pcl::QuantizedMap::
spreadQuantizedMap (const QuantizedMap & input_map, QuantizedMap & output_map, const std::size_t spreading_size)
{
#if defined(__RVV10__)
  if (spreadQuantizedMapRVV (input_map, output_map, spreading_size))
    return;
#endif

  spreadQuantizedMapStd (input_map, output_map, spreading_size);
  recordQuantizedMapSpreadScalarPath ();
}
