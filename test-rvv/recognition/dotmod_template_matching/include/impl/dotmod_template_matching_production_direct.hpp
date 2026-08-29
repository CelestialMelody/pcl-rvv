#pragma once

/*
 * 本文件做什么：
 * 这里构造 DOTMOD template matching 的 production direct（真实生产路径）测试入口。
 * 测试用 FixedDOTModality 只负责提供稳定的 QuantizedMap；模板仍通过真实
 * DOTMOD::createAndAddTemplate() 写入 DOTMOD 私有模板状态，检测仍调用真实
 * DOTMOD::detectTemplates()。因此它能证明 production dispatch（生产分流）是否
 * 被当前源码命中，而不是只证明 test-only helper。
 */

#include <pcl/recognition/dotmod.h>
#include <pcl/recognition/mask_map.h>
#include <pcl/recognition/quantized_map.h>
#include <pcl/recognition/region_xy.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pcl::test::dotmod_rvv
{

class FixedDOTModality final : public pcl::DOTModality
{
public:
  FixedDOTModality (const std::size_t width,
                    const std::size_t height,
                    const std::size_t salt)
    : map_ (width, height)
  {
    fillMap (salt);
  }

  pcl::QuantizedMap&
  getDominantQuantizedMap () override
  {
    return map_;
  }

  pcl::QuantizedMap
  computeInvariantQuantizedMap (const pcl::MaskMap&, const pcl::RegionXY& region) override
  {
    pcl::QuantizedMap result (static_cast<std::size_t> (region.width),
                              static_cast<std::size_t> (region.height));
    for (std::size_t row = 0; row < result.getHeight (); ++row)
    {
      for (std::size_t col = 0; col < result.getWidth (); ++col)
      {
        const std::size_t source_x = static_cast<std::size_t> (region.x) + col;
        const std::size_t source_y = static_cast<std::size_t> (region.y) + row;
        result (col, row) = map_ (source_x, source_y);
      }
    }
    return result;
  }

private:
  void
  fillMap (const std::size_t salt)
  {
    for (std::size_t row = 0; row < map_.getHeight (); ++row)
    {
      for (std::size_t col = 0; col < map_.getWidth (); ++col)
      {
        const auto bit0 = static_cast<unsigned char> (1u << ((row * 3 + col * 5 + salt) & 7u));
        const auto bit1 = static_cast<unsigned char> (1u << ((row * 11 + col * 7 + salt * 3 + 1) & 7u));
        map_ (col, row) = static_cast<unsigned char> (bit0 | bit1);
      }
    }
  }

  pcl::QuantizedMap map_;
};

inline std::vector<FixedDOTModality>
makeFixedModalities (const std::size_t width,
                     const std::size_t height,
                     const std::size_t nr_modalities)
{
  std::vector<FixedDOTModality> modalities;
  modalities.reserve (nr_modalities);
  for (std::size_t modality = 0; modality < nr_modalities; ++modality)
    modalities.emplace_back (width, height, modality + 1);
  return modalities;
}

inline std::vector<pcl::DOTModality*>
modalityPointers (std::vector<FixedDOTModality>& modalities)
{
  std::vector<pcl::DOTModality*> pointers;
  pointers.reserve (modalities.size ());
  for (auto& modality : modalities)
    pointers.push_back (&modality);
  return pointers;
}

inline std::vector<pcl::MaskMap>
makeFullMasks (const std::size_t width,
               const std::size_t height,
               const std::size_t nr_modalities)
{
  std::vector<pcl::MaskMap> masks;
  masks.reserve (nr_modalities);
  for (std::size_t modality = 0; modality < nr_modalities; ++modality)
  {
    pcl::MaskMap mask (width, height);
    for (std::size_t row = 0; row < height; ++row)
      for (std::size_t col = 0; col < width; ++col)
        mask.set (col, row);
    masks.push_back (std::move (mask));
  }
  return masks;
}

inline std::vector<pcl::MaskMap*>
maskPointers (std::vector<pcl::MaskMap>& masks)
{
  std::vector<pcl::MaskMap*> pointers;
  pointers.reserve (masks.size ());
  for (auto& mask : masks)
    pointers.push_back (&mask);
  return pointers;
}

inline pcl::DOTMOD
makeProductionDOTMOD (const std::size_t template_width,
                      const std::size_t template_height,
                      std::vector<pcl::DOTModality*>& modalities,
                      std::vector<pcl::MaskMap*>& masks,
                      const std::size_t nr_templates)
{
  pcl::DOTMOD dotmod (template_width, template_height);
  for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
  {
    pcl::RegionXY region;
    region.x = static_cast<int> (template_index % 3);
    region.y = static_cast<int> ((template_index * 2) % 3);
    region.width = static_cast<int> (template_width);
    region.height = static_cast<int> (template_height);
    dotmod.createAndAddTemplate (modalities, masks, 0, 0, region);
  }
  return dotmod;
}

inline std::uint64_t
checksumDetections (const std::vector<pcl::DOTMODDetection>& detections)
{
  std::uint64_t checksum = 1469598103934665603ull;
  for (const auto& detection : detections)
  {
    checksum ^= static_cast<std::uint64_t> (detection.bin_x + 17 * detection.bin_y + 131 * detection.template_id);
    checksum *= 1099511628211ull;
    checksum ^= static_cast<std::uint64_t> (detection.score * 1000000.0f);
    checksum *= 1099511628211ull;
  }
  checksum ^= static_cast<std::uint64_t> (detections.size ());
  checksum *= 1099511628211ull;
  return checksum;
}

} // namespace pcl::test::dotmod_rvv
