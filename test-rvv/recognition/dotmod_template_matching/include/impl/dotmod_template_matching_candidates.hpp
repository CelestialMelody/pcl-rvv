#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(__RVV10__) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

namespace pcl::test::dotmod_rvv
{

struct Detection
{
  std::size_t bin_x = 0;
  std::size_t bin_y = 0;
  std::size_t template_id = 0;
  float score = 0.0f;
};

// 这个 baseline（基线）复刻 `DOTMOD::detectTemplates()` 当前的数据形态：
// 每个窗口先形成 row-major submap（按行连续子图），再按 `data_index`
// 与 template byte 做 bitwise AND（按位与）计数。它刻意保留临时 vector，
// 用来把 submap 分配 / 拷贝成本纳入 Phase 000 的 A/B 边界。
inline std::uint32_t
scoreWindowViaSubMapStd (const std::uint8_t* image,
                         const std::size_t image_width,
                         const std::size_t window_x,
                         const std::size_t window_y,
                         const std::size_t window_width,
                         const std::size_t window_height,
                         const std::uint8_t* templ)
{
  std::vector<std::uint8_t> submap (window_width * window_height);
  for (std::size_t row = 0; row < window_height; ++row)
  {
    const std::uint8_t* source_row = image + (window_y + row) * image_width + window_x;
    std::copy_n (source_row, window_width, submap.data () + row * window_width);
  }

  std::uint32_t score = 0;
  for (std::size_t index = 0; index < submap.size (); ++index)
    score += ((submap[index] & templ[index]) != 0);
  return score;
}

// 标量 direct-window reference（直接窗口参考链路）不构造 submap，而是逐行从
// 原图窗口读取。它和 RVV helper 对齐同一语义，便于后续把“去掉 submap 拷贝”
// 与“使用 RVV 计数”拆开解释。
inline std::uint32_t
scoreWindowDirectStd (const std::uint8_t* image,
                      const std::size_t image_width,
                      const std::size_t window_x,
                      const std::size_t window_y,
                      const std::size_t window_width,
                      const std::size_t window_height,
                      const std::uint8_t* templ)
{
  std::uint32_t score = 0;
  for (std::size_t row = 0; row < window_height; ++row)
  {
    const std::uint8_t* image_row = image + (window_y + row) * image_width + window_x;
    const std::uint8_t* template_row = templ + row * window_width;
    for (std::size_t col = 0; col < window_width; ++col)
      score += ((image_row[col] & template_row[col]) != 0);
  }
  return score;
}

// Phase 000 的 RVV 候选只替代窗口内 byte AND + count。每一行作为一个或多个
// VL chunk（可变向量长度分块）处理：连续加载 image/template byte，构造非零
// mask（掩码），再用 vcpop 统计命中通道数。外层滑窗、responses 分配和
// detection 输出顺序仍不在本 helper 的证据边界内。
inline std::uint32_t
scoreWindowDirectRVV (const std::uint8_t* image,
                      const std::size_t image_width,
                      const std::size_t window_x,
                      const std::size_t window_y,
                      const std::size_t window_width,
                      const std::size_t window_height,
                      const std::uint8_t* templ)
{
#if defined(__RVV10__) && defined(__riscv_vector)
  std::uint32_t score = 0;
  for (std::size_t row = 0; row < window_height; ++row)
  {
    const std::uint8_t* image_row = image + (window_y + row) * image_width + window_x;
    const std::uint8_t* template_row = templ + row * window_width;
    for (std::size_t col = 0; col < window_width;)
    {
      const std::size_t vl = __riscv_vsetvl_e8m1 (window_width - col);
      const vuint8m1_t image_values = __riscv_vle8_v_u8m1 (image_row + col, vl);
      const vuint8m1_t template_values = __riscv_vle8_v_u8m1 (template_row + col, vl);
      const vuint8m1_t masked = __riscv_vand_vv_u8m1 (image_values, template_values, vl);
      const vbool8_t hit = __riscv_vmsne_vx_u8m1_b8 (masked, 0, vl);
      score += static_cast<std::uint32_t> (__riscv_vcpop_m_b8 (hit, vl));
      col += vl;
    }
  }
  return score;
#else
  return scoreWindowDirectStd (image, image_width, window_x, window_y, window_width, window_height, templ);
#endif
}

// Phase 010 的 full-shaped baseline（完整形态基线）复刻 production 的三层顺序：
// row/col 滑窗、modality 累加、template 输出。它仍使用 Phase 000 的 submap
// baseline 来保留当前 production 每个窗口构造临时 QuantizedMap 的成本形态。
inline std::vector<Detection>
detectTemplatesViaSubMapStd (const std::vector<std::vector<std::uint8_t>>& modality_maps,
                             const std::size_t image_width,
                             const std::size_t window_width,
                             const std::size_t window_height,
                             const std::vector<std::vector<std::vector<std::uint8_t>>>& templates,
                             const float template_response_threshold)
{
  std::vector<Detection> detections;
  if (modality_maps.empty () || templates.empty () || image_width == 0 || window_width == 0 || window_height == 0)
    return detections;

  const std::size_t image_height = modality_maps[0].size () / image_width;
  const std::size_t max_x = image_width > window_width ? image_width - window_width : 0;
  const std::size_t max_y = image_height > window_height ? image_height - window_height : 0;
  const std::size_t nr_templates = templates.size ();
  const float scaling_factor = 1.0f / static_cast<float> (window_width * window_height);

  for (std::size_t row_index = 0; row_index < max_y; ++row_index)
  {
    for (std::size_t col_index = 0; col_index < max_x; ++col_index)
    {
      std::vector<float> responses (nr_templates, 0.0f);
      for (std::size_t modality_index = 0; modality_index < modality_maps.size (); ++modality_index)
      {
        for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
        {
          responses[template_index] += static_cast<float> (scoreWindowViaSubMapStd (
              modality_maps[modality_index].data (),
              image_width,
              col_index,
              row_index,
              window_width,
              window_height,
              templates[template_index][modality_index].data ()));
        }
      }

      for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
      {
        const float response = responses[template_index] * scaling_factor;
        if (response > template_response_threshold)
          detections.push_back ({col_index, row_index, template_index, response});
      }
    }
  }
  return detections;
}

inline std::vector<Detection>
detectTemplatesDirectStd (const std::vector<std::vector<std::uint8_t>>& modality_maps,
                          const std::size_t image_width,
                          const std::size_t window_width,
                          const std::size_t window_height,
                          const std::vector<std::vector<std::vector<std::uint8_t>>>& templates,
                          const float template_response_threshold)
{
  std::vector<Detection> detections;
  if (modality_maps.empty () || templates.empty () || image_width == 0 || window_width == 0 || window_height == 0)
    return detections;

  const std::size_t image_height = modality_maps[0].size () / image_width;
  const std::size_t max_x = image_width > window_width ? image_width - window_width : 0;
  const std::size_t max_y = image_height > window_height ? image_height - window_height : 0;
  const std::size_t nr_templates = templates.size ();
  const float scaling_factor = 1.0f / static_cast<float> (window_width * window_height);

  for (std::size_t row_index = 0; row_index < max_y; ++row_index)
  {
    for (std::size_t col_index = 0; col_index < max_x; ++col_index)
    {
      std::vector<float> responses (nr_templates, 0.0f);
      for (std::size_t modality_index = 0; modality_index < modality_maps.size (); ++modality_index)
      {
        for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
        {
          responses[template_index] += static_cast<float> (scoreWindowDirectStd (
              modality_maps[modality_index].data (),
              image_width,
              col_index,
              row_index,
              window_width,
              window_height,
              templates[template_index][modality_index].data ()));
        }
      }

      for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
      {
        const float response = responses[template_index] * scaling_factor;
        if (response > template_response_threshold)
          detections.push_back ({col_index, row_index, template_index, response});
      }
    }
  }
  return detections;
}

// 这个 helper 只把窗口内 score 替换为 RVV，外层 full detectTemplates-shaped
// 顺序保持标量写法。这样 Phase 010 可以单独判断“计分核收益”放回完整链路后
// 是否仍成立，而不会把 response buffer 复用或 fused loop 混进同一个候选。
inline std::vector<Detection>
detectTemplatesDirectRVV (const std::vector<std::vector<std::uint8_t>>& modality_maps,
                          const std::size_t image_width,
                          const std::size_t window_width,
                          const std::size_t window_height,
                          const std::vector<std::vector<std::vector<std::uint8_t>>>& templates,
                          const float template_response_threshold)
{
  std::vector<Detection> detections;
  if (modality_maps.empty () || templates.empty () || image_width == 0 || window_width == 0 || window_height == 0)
    return detections;

  const std::size_t image_height = modality_maps[0].size () / image_width;
  const std::size_t max_x = image_width > window_width ? image_width - window_width : 0;
  const std::size_t max_y = image_height > window_height ? image_height - window_height : 0;
  const std::size_t nr_templates = templates.size ();
  const float scaling_factor = 1.0f / static_cast<float> (window_width * window_height);

  for (std::size_t row_index = 0; row_index < max_y; ++row_index)
  {
    for (std::size_t col_index = 0; col_index < max_x; ++col_index)
    {
      std::vector<float> responses (nr_templates, 0.0f);
      for (std::size_t modality_index = 0; modality_index < modality_maps.size (); ++modality_index)
      {
        for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
        {
          responses[template_index] += static_cast<float> (scoreWindowDirectRVV (
              modality_maps[modality_index].data (),
              image_width,
              col_index,
              row_index,
              window_width,
              window_height,
              templates[template_index][modality_index].data ()));
        }
      }

      for (std::size_t template_index = 0; template_index < nr_templates; ++template_index)
      {
        const float response = responses[template_index] * scaling_factor;
        if (response > template_response_threshold)
          detections.push_back ({col_index, row_index, template_index, response});
      }
    }
  }
  return detections;
}

} // namespace pcl::test::dotmod_rvv
