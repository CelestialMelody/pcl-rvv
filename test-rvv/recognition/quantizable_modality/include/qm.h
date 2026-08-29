#pragma once

/*
 * 本文件做什么：
 * 这是 recognition/quantizable_modality RVV topic 的测试支撑聚合入口。
 * 当前短标识 qm 对应完整 topic `quantizable_modality`。首阶段只覆盖
 * `QuantizedMap::spreadQuantizedMap()` 这个公共 byte map spread helper
 *（字节图扩散辅助函数），不把任一具体 modality 的 `processInputData()`
 * 计时边界写成已接入 production（生产源码）。
 *
 * 证据边界：
 * 这里的 helper 只属于 test-rvv（专项测试资产）。它负责构造输入、
 * 校验标量和 RVV 链路的输出一致性，并为 bench（性能测试）生成稳定
 * checksum（校验和）。真实 production dispatch（生产分流）必须由
 * `recognition/src/quantizable_modality.cpp` 的当前源码和 production direct
 * 证据单独证明。
 */

#include <pcl/recognition/quantized_map.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pcl::recognition::rvv_test::quantizable_modality
{
  enum class SpreadPath
  {
    None = 0,
    Scalar = 1,
    Rvv = 2,
  };

  inline pcl::QuantizedMap
  makePatternMap (const std::size_t width, const std::size_t height)
  {
    pcl::QuantizedMap map (width, height);
    for (std::size_t y = 0; y < height; ++y)
    {
      for (std::size_t x = 0; x < width; ++x)
      {
        const auto bit = static_cast<unsigned char> (1u << ((x * 3 + y * 5) & 7u));
        const auto sparse_bit = ((x + 2 * y) % 11 == 0)
            ? static_cast<unsigned char> (1u << ((x + y) & 7u))
            : static_cast<unsigned char> (0u);
        map (x, y) = static_cast<unsigned char> (bit | sparse_bit);
      }
    }
    return map;
  }

  inline std::vector<std::uint8_t>
  copyMap (const pcl::QuantizedMap& map)
  {
    const auto size = map.getWidth () * map.getHeight ();
    const auto* data = map.getData ();
    return std::vector<std::uint8_t> (data, data + size);
  }

  inline std::uint64_t
  checksumMap (const pcl::QuantizedMap& map)
  {
    const auto* data = map.getData ();
    const auto size = map.getWidth () * map.getHeight ();
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i)
    {
      hash ^= data[i];
      hash *= 1099511628211ull;
    }
    return hash;
  }

  inline std::string
  pathName ()
  {
#if defined(__RVV10__)
    return "rvv-build";
#else
    return "std-build";
#endif
  }
} // namespace pcl::recognition::rvv_test::quantizable_modality
