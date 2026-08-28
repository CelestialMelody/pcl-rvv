#pragma once

/*
 * 本文件做什么：
 * 这是 recognition/color_gradient_modality RVV topic 的测试支撑聚合入口。
 * 当前覆盖 ColorGradientModality 的 Sobel gradient（Sobel 梯度）、
 * quantize（角度量化）和 3x3 dominant filter（主方向过滤）子链路：
 * 从 organized RGB（有宽高的二维 RGB 点云）得到 8 方向量化 map，或从
 * quantized map 得到 one-hot filtered map。Gaussian、spread 和
 * feature extraction 保持在后续 phase。
 *
 * 证据边界：
 * 这里的 helper 只属于 test-rvv（专项测试资产），不修改 production（生产源码）
 * 的 color_gradient_modality.h，也不证明 production dispatch（生产分流）已经
 * 接入 RVV。
 */

#include "impl/cgm_sobel_quantize.hpp"
