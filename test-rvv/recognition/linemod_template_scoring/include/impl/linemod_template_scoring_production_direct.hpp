#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <pcl/recognition/linemod.h>

namespace pcl::test::linemod_rvv
{

class FixedQuantizableModality final : public pcl::QuantizableModality
{
public:
  FixedQuantizableModality (const std::size_t width, const std::size_t height)
    : quantized_map_ (width, height)
    , spreaded_quantized_map_ (width, height)
  {
    fillMap (quantized_map_);
    fillMap (spreaded_quantized_map_);
  }

  pcl::QuantizedMap&
  getQuantizedMap () override
  {
    return quantized_map_;
  }

  pcl::QuantizedMap&
  getSpreadedQuantizedMap () override
  {
    return spreaded_quantized_map_;
  }

  void
  extractFeatures (const pcl::MaskMap&,
                   std::size_t,
                   std::size_t,
                   std::vector<pcl::QuantizedMultiModFeature>&) const override
  {
  }

  void
  extractAllFeatures (const pcl::MaskMap&,
                      std::size_t,
                      std::size_t,
                      std::vector<pcl::QuantizedMultiModFeature>&) const override
  {
  }

private:
  static void
  fillMap (pcl::QuantizedMap& map)
  {
    for (std::size_t row = 0; row < map.getHeight (); ++row)
    {
      for (std::size_t col = 0; col < map.getWidth (); ++col)
      {
        const auto bit = static_cast<unsigned char> (1u << ((row * 3 + col * 5 + (row ^ col)) & 7u));
        const auto neighbor = static_cast<unsigned char> (1u << ((row + col * 7 + 3) & 7u));
        map (col, row) = static_cast<unsigned char> (bit | neighbor);
      }
    }
  }

  pcl::QuantizedMap quantized_map_;
  pcl::QuantizedMap spreaded_quantized_map_;
};

inline std::size_t
chooseProductionLinWidth (const std::size_t mem_size)
{
  std::size_t lin_width = std::min<std::size_t> (64, mem_size == 0 ? 1 : mem_size);
  while (lin_width > 1 && mem_size % lin_width != 0)
    --lin_width;
  return lin_width;
}

inline pcl::SparseQuantizedMultiModTemplate
makeProductionTemplate (const std::size_t nr_features)
{
  pcl::SparseQuantizedMultiModTemplate linemod_template;
  linemod_template.region.x = 0;
  linemod_template.region.y = 0;
  linemod_template.region.width = 64;
  linemod_template.region.height = 64;
  linemod_template.features.reserve (nr_features);
  for (std::size_t feature_index = 0; feature_index < nr_features; ++feature_index)
  {
    pcl::QuantizedMultiModFeature feature;
    feature.x = static_cast<int> ((feature_index * 5) & 7u);
    feature.y = static_cast<int> ((feature_index * 3) & 7u);
    feature.modality_index = 0;
    feature.quantized_value = static_cast<unsigned char> (1u << (feature_index & 7u));
    linemod_template.features.push_back (feature);
  }
  return linemod_template;
}

inline std::uint64_t
checksumDetections (const std::vector<pcl::LINEMODDetection>& detections)
{
  std::uint64_t result = 1469598103934665603ull;
  for (const auto& detection : detections)
  {
    const auto mix = static_cast<std::uint64_t> (static_cast<std::uint32_t> (detection.x))
        ^ (static_cast<std::uint64_t> (static_cast<std::uint32_t> (detection.y)) << 11)
        ^ (static_cast<std::uint64_t> (static_cast<std::uint32_t> (detection.template_id)) << 22)
        ^ static_cast<std::uint64_t> (static_cast<int> (detection.score * 1000000.0f));
    result ^= mix;
    result *= 1099511628211ull;
  }
  return result;
}

inline pcl::LINEMOD
makeProductionLinemod (const std::size_t nr_features)
{
  pcl::LINEMOD linemod;
  linemod.setDetectionThreshold (0.0f);
  linemod.setNonMaxSuppression (false);
  linemod.setDetectionAveraging (false);
  linemod.addTemplate (makeProductionTemplate (nr_features));
  return linemod;
}

} // namespace pcl::test::linemod_rvv
