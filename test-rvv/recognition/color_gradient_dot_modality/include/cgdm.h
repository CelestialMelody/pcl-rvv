#pragma once

/*
 * 本文件做什么：
 * 这是 recognition/color_gradient_dot_modality RVV topic 的测试支撑聚合入口。
 * 当前覆盖 DOTMOD 输入图像预处理主链路：从 organized RGB（有宽高的二维 RGB 点云）
 * 生成 dominant quantized gradient map（主方向量化图）。
 *
 * 证据边界：
 * 这里的 helper 只属于 test-rvv（专项测试资产），不修改 production（生产源码）
 * 的 color_gradient_dot_modality.h，也不证明 production dispatch（生产分流）已经
 * 接入 RVV。
 */

#include "impl/cgdm_color_gradient.hpp"
